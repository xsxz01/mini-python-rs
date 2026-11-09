#include "../cffi.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 归一化大小写 (Windows 下不区分大小写)
// Normalize case (case-insensitive on Windows)
// ᠲᠣᠮᠤ ᠪᠤᠯᠭᠠᠬᠤ
std::string normalizeCase(const std::string& text){
#ifdef _WIN32
    std::string out = text;
    for (char& c : out){
        if (c == '/') c = '\\';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
#else
    return text;
#endif
}

// 判断是否是集合中的字符
// Whether a character belongs to a glob set
// ᠰᠢᠳᠤᠷ ᠳᠤ ᠪᠠᠶᠢᠭ᠎ᠠ ᠡᠰᠡᠬᠦ
bool matchSet(const std::string& pat, size_t& p, char ch, bool& closed){
    bool negate = false;
    bool matched = false;
    size_t i = p;
    if (i < pat.size() && (pat[i] == '!' || pat[i] == '^')){
        negate = true;
        i++;
    }
    if (i < pat.size() && pat[i] == ']'){
        if (ch == ']') matched = true;
        i++;
    }
    closed = false;
    while (i < pat.size()){
        if (pat[i] == ']'){
            closed = true;
            i++;
            break;
        }
        char first = pat[i];
        if (first == '\\' && i + 1 < pat.size()){
            i++;
            first = pat[i];
        }
        if (i + 2 < pat.size() && pat[i + 1] == '-' && pat[i + 2] != ']'){
            char last = pat[i + 2];
            if (last == '\\' && i + 3 < pat.size()) last = pat[i + 3];
            if (first <= ch && ch <= last) matched = true;
            i += (pat[i + 1] == '-' ? 3 : 1);
            continue;
        }
        if (first == ch) matched = true;
        i++;
    }
    p = i;
    return negate ? !matched : matched;
}

// 递归匹配模式
// Recursive glob matcher
// ᠳᠠᠪᠬᠠᠷᠢᠶ᠎ᠠ ᠬᠡᠯᠪᠡᠷᠢ
bool matchFrom(const std::string& name, size_t ni,
               const std::string& pat, size_t pi){
    while (pi < pat.size()){
        char pc = pat[pi];
        if (pc == '*'){
            while (pi < pat.size() && pat[pi] == '*') pi++;
            if (pi >= pat.size()) return true;
            for (size_t k = ni; k <= name.size(); ++k){
                if (matchFrom(name, k, pat, pi)) return true;
            }
            return false;
        }
        if (pc == '?'){
            if (ni >= name.size()) return false;
            ni++;
            pi++;
            continue;
        }
        if (pc == '['){
            if (ni >= name.size()) return false;
            size_t p = pi + 1;
            bool closed = false;
            bool ok = matchSet(pat, p, name[ni], closed);
            if (!closed) return false;
            if (!ok) return false;
            ni++;
            pi = p;
            continue;
        }
        if (pc == '\\' && pi + 1 < pat.size()){
            pi++;
            pc = pat[pi];
        }
        if (ni >= name.size()) return false;
        if (name[ni] != pc) return false;
        ni++;
        pi++;
    }
    return ni == name.size();
}

// 转义正则字符
// Escape a character for a regular expression
// ᠬᠡᠯᠪᠡᠷᠢ ᠦᠰᠦᠭ ᠢ ᠪᠤᠰᠠᠭᠠᠬᠤ
std::string regexEscape(char c){
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') return std::string(1, c);
    return std::string("\\") + c;
}

// 读取列表参数为字符串列表
// Read a list argument as a string list
// ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠢ ᠦᠰᠦᠭ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
bool readStringList(cffi& a, const value& source, std::vector<std::string>& out){
    std::vector<value> items;
    a.vm->materializeIterable(source, items);
    if (a.vm->hasError()) return false;
    for (const auto& item : items) out.push_back(a.vm->toString(item));
    return true;
}

// 构建字符串列表值
// Build a list of strings
// ᠦᠰᠦᠭ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeStringList(cffi& a, const std::vector<std::string>& items){
    std::vector<value> values;
    for (const auto& item : items) values.push_back(a.mkstr(item));
    return a.mklist(values);
}
}

// fn Fnmatchcase
// fn fnmatchcase
// fn Fnmatchcase
static value fnFnmatchCase(cffi& a){
    std::string name = a.arg_str(0);
    std::string pat = a.arg_str(1);
    return a.mkbool(matchFrom(name, 0, pat, 0));
}

// fn Fnmatch
// fn fnmatch
// fn Fnmatch
static value fnFnmatch(cffi& a){
    std::string name = normalizeCase(a.arg_str(0));
    std::string pat = normalizeCase(a.arg_str(1));
    return a.mkbool(matchFrom(name, 0, pat, 0));
}

// fn Filter
// fn filter
// fn Filter
static value fnFilter(cffi& a){
    std::vector<std::string> names;
    if (!readStringList(a, a.arg(0), names)) return a.none();
    std::string pat = normalizeCase(a.arg_str(1));
    std::vector<std::string> result;
    for (const auto& name : names){
        if (matchFrom(normalizeCase(name), 0, pat, 0)) result.push_back(name);
    }
    return makeStringList(a, result);
}

// fn Translate
// fn translate
// fn Translate
static value fnTranslate(cffi& a){
    std::string pat = a.arg_str(0);
    std::string result;
    size_t i = 0;
    while (i < pat.size()){
        char c = pat[i];
        i++;
        if (c == '*'){
            bool star = false;
            if (!result.empty() && result.back() == '*') star = true;
            (void)star;
            result += ".*";
        } else if (c == '?'){
            result += '.';
        } else if (c == '['){
            size_t j = i;
            if (j < pat.size() && pat[j] == '!') j++;
            if (j < pat.size() && pat[j] == ']') j++;
            while (j < pat.size() && pat[j] != ']') j++;
            if (j >= pat.size()){
                result += "\\[";
            } else {
                std::string stuff = pat.substr(i, j - i);
                std::string escaped;
                for (char sc : stuff){
                    if (sc == '\\') escaped += "\\\\";
                    else escaped.push_back(sc);
                }
                i = j + 1;
                if (!escaped.empty() && escaped[0] == '!') escaped = "^" + escaped.substr(1);
                else if (!escaped.empty() && escaped[0] == '^') escaped = "\\" + escaped;
                result += "[" + escaped + "]";
            }
        } else if (c == '\\' && i < pat.size()){
            result += regexEscape(pat[i]);
            i++;
        } else {
            result += regexEscape(c);
        }
    }
    return a.mkstr("(?s:" + result + ")\\Z");
}

// cffi fnmatch lib
// cffi fnmatch lib
// cffi fnmatch lib
ffilib cffi_fnmatch_lib(){
    return cffi_lib(
        "fnmatch",
        {
            {"fnmatch", (void*)fnFnmatch},
            {"fnmatchcase", (void*)fnFnmatchCase},
            {"filter", (void*)fnFilter},
            {"translate", (void*)fnTranslate}
        });
}

}
