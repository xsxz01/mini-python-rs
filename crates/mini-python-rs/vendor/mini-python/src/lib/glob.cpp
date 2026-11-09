#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace minipy {

namespace {

namespace fs = std::filesystem;

std::string u8Text(const fs::path& p){
    std::u8string u8 = p.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

fs::path fsPathFromUtf8(const std::string& text){
    std::u8string u8(text.begin(), text.end());
    return fs::path(u8);
}

bool wildMatchImpl(const std::string& name, size_t ni, const std::string& pat, size_t pi){
    while (pi < pat.size()){
        char pc = pat[pi];
        if (pc == '*'){
            if (pi + 1 == pat.size()) return true;
            for (size_t k = ni; k <= name.size(); ++k){
                if (wildMatchImpl(name, k, pat, pi + 1)) return true;
            }
            return false;
        }
        if (ni >= name.size()) return false;
        if (pc == '?'){
            ++ni; ++pi; continue;
        }
        if (pc == '['){
            size_t end = pat.find(']', pi + 1);
            if (end == std::string::npos) end = pat.size();
            bool negate = false;
            size_t k = pi + 1;
            if (k < end && (pat[k] == '!' || pat[k] == '^')){ negate = true; ++k; }
            bool matched = false;
            while (k < end){
                char lo = pat[k];
                if (k + 2 < end && pat[k + 1] == '-'){
                    char hi = pat[k + 2];
                    if (name[ni] >= lo && name[ni] <= hi) matched = true;
                    k += 3;
                } else {
                    if (name[ni] == lo) matched = true;
                    ++k;
                }
            }
            if (matched == negate) return false;
            ++ni;
            pi = end < pat.size() ? end + 1 : end;
            continue;
        }
        if (name[ni] != pc) return false;
        ++ni; ++pi;
    }
    return ni == name.size();
}

bool wildMatch(const std::string& name, const std::string& pattern){
    return wildMatchImpl(name, 0, pattern, 0);
}

bool hasMagicText(const std::string& text){
    return text.find('*') != std::string::npos || text.find('?') != std::string::npos ||
           text.find('[') != std::string::npos;
}

std::vector<std::string> splitPattern(const std::string& pattern){
    std::vector<std::string> parts;
    std::string norm = pattern;
    for (auto& ch : norm) if (ch == '\\') ch = '/';
    size_t start = 0;
    while (start <= norm.size()){
        size_t slash = norm.find('/', start);
        std::string part = slash == std::string::npos ? norm.substr(start)
                                                      : norm.substr(start, slash - start);
        if (!part.empty()) parts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return parts;
}

// 目录遍历辅助
void globWalk(const fs::path& base, const std::string& prefix,
              const std::vector<std::string>& parts, size_t index,
              bool includeHidden, std::vector<std::string>& out){
    if (index >= parts.size()){
        if (!prefix.empty()) out.push_back(prefix);
        return;
    }
    std::string part = parts[index];
    if (part == "**"){
        std::error_code ec;
        fs::recursive_directory_iterator it(base, ec);
        if (!ec){
            for (auto& entry : it){
                if (!entry.is_directory(ec)) continue;
                std::string fname = u8Text(entry.path().filename());
                if (!includeHidden && fname[0] == '.') continue;
                std::string rel = prefix.empty() ? fname : prefix + "/" + fname;
                globWalk(entry.path(), rel, parts, index + 1, includeHidden, out);
            }
        }
        globWalk(base, prefix, parts, index + 1, includeHidden, out);
        return;
    }
    if (!hasMagicText(part)){
        std::error_code ec;
        fs::path next = base / fsPathFromUtf8(part);
        if (!fs::exists(next, ec)) return;
        std::string rel = prefix.empty() ? part : prefix + "/" + part;
        if (index + 1 >= parts.size()) out.push_back(rel);
        else globWalk(next, rel, parts, index + 1, includeHidden, out);
        return;
    }
    std::error_code ec;
    fs::directory_iterator it(base, ec);
    if (ec) return;
    for (auto& entry : it){
        std::string fname = u8Text(entry.path().filename());
        if (!includeHidden && fname[0] == '.') continue;
        if (!wildMatch(fname, part)) continue;
        std::string rel = prefix.empty() ? fname : prefix + "/" + fname;
        if (index + 1 >= parts.size()) out.push_back(rel);
        else globWalk(entry.path(), rel, parts, index + 1, includeHidden, out);
    }
}

static value fnGlob(cffi& a){
    std::string pattern = a.vm->toString(a.arg(0));
    bool recursive = a.has_kwarg("recursive") ? a.kwarg_bool("recursive", false) : false;
    bool includeHidden = a.has_kwarg("include_hidden")
                             ? a.kwarg_bool("include_hidden", false) : false;
    std::string rootDir;
    if (a.has_kwarg("root_dir") && a.kwarg("root_dir").type != "none")
        rootDir = a.vm->toString(a.kwarg("root_dir"));
    std::vector<std::string> parts = splitPattern(pattern);
    std::vector<std::string> out;
    if (recursive){
        // 将 "**/" 插入到模式里使递归生效
        std::vector<std::string> expanded;
        for (auto& part : parts){
            if (part == "**"){ expanded.push_back("**"); continue; }
            expanded.push_back(part);
        }
        if (std::find(expanded.begin(), expanded.end(), "**") == expanded.end()){
            expanded.insert(expanded.begin(), "**");
        }
        parts = expanded;
    }
    fs::path base = rootDir.empty() ? fs::current_path() : fsPathFromUtf8(rootDir);
    globWalk(base, "", parts, 0, includeHidden, out);
    std::sort(out.begin(), out.end());
    std::vector<value> result;
    for (auto& item : out) result.push_back(a.mkstr(item));
    return a.mklist(result);
}

static value fnHasMagic(cffi& a){
    std::string text = a.vm->toString(a.arg(0));
    return a.mkbool(hasMagicText(text));
}

static value fnEscape(cffi& a){
    std::string text = a.vm->toString(a.arg(0));
    std::string out;
    for (char ch : text){
        if (ch == '*' || ch == '?' || ch == '['){
            out += '[';
            out += ch;
            out += ']';
        } else out += ch;
    }
    return a.mkstr(out);
}

}

ffilib cffi_glob_lib(){
    std::map<std::string, void*> functions = {
        {"glob", (void*)fnGlob},
        {"iglob", (void*)fnGlob},
        {"has_magic", (void*)fnHasMagic},
        {"escape", (void*)fnEscape}
    };
    return cffi_lib("glob", functions, {}, {});
}

}
