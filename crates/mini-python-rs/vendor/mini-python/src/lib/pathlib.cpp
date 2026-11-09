#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

namespace fs = std::filesystem;

// 路径状态
// Path state
// ᠮᠥᠷ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct PathState{
    std::string prefix;                 // "/"、"//"、"C:/"、"//server/share/" 或空
    std::vector<std::string> parts;     // 路径组件
    std::string flavor = "posix";       // posix / windows
    std::string className = "Path";     // repr 用的类名
    bool pure = false;
};

std::string lowerCopy(const std::string& text){
    std::string out = text;
    for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

// 通配符匹配 (支持 * ? [seq] [!seq])
// Wildcard match (supports * ? [seq] [!seq])
// ᠲᠥᠮᠥᠷᠬᠡᠭᠴᠢ ᠬᠠᠮᠲᠤᠷᠠᠯ
bool wildMatchImpl(const std::string& name, size_t ni, const std::string& pat, size_t pi){
    while (pi < pat.size()){
        char pc = pat[pi];
        if (pc == '*'){
            while (pi + 1 < pat.size() && pat[pi + 1] == '*') ++pi;
            if (pi + 1 == pat.size()) return true;
            for (size_t k = ni; k <= name.size(); ++k){
                if (wildMatchImpl(name, k, pat, pi + 1)) return true;
            }
            return false;
        }
        if (ni >= name.size()) return false;
        if (pc == '?'){
            ++ni;
            ++pi;
            continue;
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
        ++ni;
        ++pi;
    }
    return ni == name.size();
}

bool wildMatch(const std::string& name, const std::string& pattern, bool ci){
    if (ci) return wildMatchImpl(lowerCopy(name), 0, lowerCopy(pattern), 0);
    return wildMatchImpl(name, 0, pattern, 0);
}

bool hasMagic(const std::string& text){
    return text.find('*') != std::string::npos || text.find('?') != std::string::npos ||
           text.find('[') != std::string::npos;
}

PathState* pathOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass) return nullptr;
    const std::string& name = inst->klass->name;
    if (name != "PurePath" && name != "PurePosixPath" && name != "PureWindowsPath" &&
        name != "Path" && name != "PosixPath" && name != "WindowsPath") return nullptr;
    return static_cast<PathState*>(inst->handle);
}

std::string flavorOf(const char* className){
    std::string name = className;
    if (name == "PureWindowsPath" || name == "WindowsPath") return "windows";
    if (name == "PurePosixPath" || name == "PosixPath") return "posix";
#if defined(_WIN32)
    return "windows";
#else
    return "posix";
#endif
}

bool pureOf(const char* className){
    std::string name = className;
    return name.rfind("Pure", 0) == 0;
}

void parsePath(const std::string& raw, const std::string& flavor, bool pure,
               const std::string& className, PathState& st){
    st.prefix.clear();
    st.parts.clear();
    st.flavor = flavor;
    st.pure = pure;
    st.className = className;
    std::string s = raw;
    if (flavor == "windows")
        for (auto& ch : s) if (ch == '\\') ch = '/';
    if (flavor == "windows"){
        // UNC 路径 //server/share
        if (s.size() >= 2 && s[0] == '/' && s[1] == '/'){
            size_t i = 2;
            while (i < s.size() && s[i] == '/') ++i;
            size_t serverEnd = s.find('/', i);
            if (serverEnd != std::string::npos){
                size_t j = serverEnd + 1;
                while (j < s.size() && s[j] == '/') ++j;
                size_t shareEnd = s.find('/', j);
                std::string server = s.substr(i, serverEnd - i);
                std::string share = shareEnd == std::string::npos ? s.substr(j)
                                                                 : s.substr(j, shareEnd - j);
                st.prefix = "//" + server + "/" + share + "/";
                s = shareEnd == std::string::npos ? "" : s.substr(shareEnd + 1);
            } else {
                st.prefix = "//";
                s = s.substr(i);
            }
        } else if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':'){
            if (s.size() >= 3 && s[2] == '/'){
                st.prefix = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(s[0]))) ) + ":/";
                s = s.substr(3);
            } else {
                st.prefix = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(s[0]))) ) + ":";
                s = "";
            }
        } else if (!s.empty() && s[0] == '/'){
            st.prefix = "/";
            s = s.substr(1);
        }
    } else {
        if (!s.empty() && s[0] == '/'){
            if (s.size() >= 2 && s[1] == '/'){
                st.prefix = "//";
                size_t i = 0;
                while (i < s.size() && s[i] == '/') ++i;
                s = s.substr(i);
            } else {
                st.prefix = "/";
                s = s.substr(1);
            }
        }
    }
    size_t start = 0;
    while (start <= s.size()){
        size_t slash = s.find('/', start);
        std::string part = slash == std::string::npos ? s.substr(start)
                                                      : s.substr(start, slash - start);
        if (!part.empty() && part != ".") st.parts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
}

std::string joinParts(const PathState& st){
    std::string out = st.prefix;
    for (size_t i = 0; i < st.parts.size(); ++i){
        if (i > 0) out += "/";
        out += st.parts[i];
    }
    return out;
}

// 显示用路径: Windows 风格使用反斜杠
// Display form: backslashes on Windows flavor
// ᠬᠠᠷᠠᠭᠳᠠᠬᠤ ᠮᠥᠷ
std::string joinDisplay(const PathState& st){
    std::string out = joinParts(st);
    if (st.flavor == "windows"){
        for (auto& ch : out) if (ch == '/') ch = '\\';
    }
    return out;
}

std::string pathText(const fs::path& p){
    std::u8string u8 = p.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

std::string concreteClassName(const PathState& st){
    if (st.className == "Path")
        return st.flavor == "windows" ? "WindowsPath" : "PosixPath";
    if (st.className == "PurePath")
        return st.flavor == "windows" ? "PureWindowsPath" : "PurePosixPath";
    return st.className;
}

std::string rootPart(const PathState& st){
    if (st.prefix.empty()) return "";
    if (st.flavor == "windows"){
        if (st.prefix.size() >= 3 && st.prefix[0] == '/' && st.prefix[1] == '/'){
            std::string root = st.prefix;
            for (auto& ch : root) if (ch == '/') ch = '\\';
            return root;
        }
        if (st.prefix == "/") return "\\";
        if (st.prefix.size() >= 2 && st.prefix[1] == ':'){
            if (st.prefix.size() > 2) return st.prefix.substr(0, 2) + "\\";
            return st.prefix;
        }
        return st.prefix;
    }
    return st.prefix;
}

std::string driveOf(const PathState& st){
    if (st.flavor != "windows") return "";
    if (st.prefix.size() >= 3 && st.prefix[0] == '/' && st.prefix[1] == '/'){
        std::string p = st.prefix.substr(0, st.prefix.size() - 1);
        for (auto& ch : p) if (ch == '/') ch = '\\';
        return p;
    }
    if (st.prefix.size() >= 2 && st.prefix[1] == ':') return st.prefix.substr(0, 2);
    return "";
}

std::string rootOf(const PathState& st){
    if (st.flavor == "windows"){
        if (st.prefix.empty()) return "";
        if (st.prefix.size() >= 3 && st.prefix[0] == '/' && st.prefix[1] == '/') return "\\";
        if (st.prefix.size() >= 2 && st.prefix[1] == ':')
            return st.prefix.size() > 2 ? "\\" : "";
        return st.prefix;
    }
    return st.prefix.empty() ? "" : "/";
}

std::string nameOf(const PathState& st);
std::string suffixOf(const std::string& name);
std::string stemOf(const std::string& name);
std::vector<std::string> suffixesOf(const std::string& name);
PathState parentState(const PathState& st);

value buildPathObjCore(cffi& a, PathState st, bool withRelations);

value buildPathObj(cffi& a, PathState st){
    return buildPathObjCore(a, st, true);
}

value buildPathObjCore(cffi& a, PathState st, bool withRelations){
    auto* heap = new PathState(st);
    value obj = a.mkobject(heap);
    if (!obj.ptr) return obj;
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    std::string nm = nameOf(st);
    inst->attributes["name"] = a.mkstr(nm);
    inst->attributes["stem"] = a.mkstr(stemOf(nm));
    inst->attributes["suffix"] = a.mkstr(suffixOf(nm));
    std::vector<value> sfx;
    for (auto& s : suffixesOf(nm)) sfx.push_back(a.mkstr(s));
    inst->attributes["suffixes"] = a.mklist(sfx);
    std::vector<value> parts;
    std::string root = rootPart(st);
    if (!root.empty()) parts.push_back(a.mkstr(root));
    for (auto& p : st.parts) parts.push_back(a.mkstr(p));
    inst->attributes["parts"] = a.vm->makeTuple(parts);
    inst->attributes["drive"] = a.mkstr(driveOf(st));
    inst->attributes["root"] = a.mkstr(rootOf(st));
    inst->attributes["anchor"] = a.mkstr(driveOf(st) + rootOf(st));
    if (withRelations){
        if (!st.parts.empty()){
            PathState ps = parentState(st);
            inst->attributes["parent"] = buildPathObjCore(a, ps, true);
        } else {
            inst->attributes["parent"] = obj;
        }
        std::vector<value> pars;
        PathState cur = st;
        while (!cur.parts.empty()){
            cur.parts.pop_back();
            pars.push_back(buildPathObjCore(a, cur, false));
        }
        inst->attributes["parents"] = a.vm->makeTuple(pars);
    }
    return obj;
}

std::string nameOf(const PathState& st){
    if (st.parts.empty()) return "";
    return st.parts.back();
}

std::string suffixOf(const std::string& name){
    size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) return "";
    return name.substr(dot);
}

std::string stemOf(const std::string& name){
    std::string suffix = suffixOf(name);
    if (suffix.empty()) return name;
    return name.substr(0, name.size() - suffix.size());
}

std::vector<std::string> suffixesOf(const std::string& name){
    std::vector<std::string> out;
    if (name.empty()) return out;
    size_t start = name[0] == '.' ? 1 : 0;
    size_t dot = name.find('.', start);
    while (dot != std::string::npos && dot + 1 < name.size()){
        size_t next = name.find('.', dot + 1);
        std::string part = next == std::string::npos ? name.substr(dot)
                                                     : name.substr(dot, next - dot);
        if (part.size() > 1) out.push_back(part);
        if (next == std::string::npos) break;
        dot = next;
    }
    return out;
}

// 深拷贝 + 变换
// Copy and transform
// ᠬᠤᠪᠢᠯᠠᠬᠤ
PathState parentState(const PathState& st){
    PathState out = st;
    if (!out.parts.empty()) out.parts.pop_back();
    return out;
}

bool sameParts(const PathState& x, const PathState& y){
    bool ci = (x.flavor == "windows");
    if (x.prefix.size() != y.prefix.size()) return false;
    for (size_t i = 0; i < x.prefix.size(); ++i){
        char cx = x.prefix[i];
        char cy = y.prefix[i];
        if (ci){
            cx = static_cast<char>(std::tolower(static_cast<unsigned char>(cx)));
            cy = static_cast<char>(std::tolower(static_cast<unsigned char>(cy)));
        }
        if (cx != cy) return false;
    }
    if (x.parts.size() != y.parts.size()) return false;
    for (size_t i = 0; i < x.parts.size(); ++i){
        if (ci){
            if (lowerCopy(x.parts[i]) != lowerCopy(y.parts[i])) return false;
        } else if (x.parts[i] != y.parts[i]) return false;
    }
    return true;
}

fs::path fsPathFromUtf8(const std::string& text){
    std::u8string u8(text.begin(), text.end());
    return fs::path(u8);
}

fs::path toFsPath(const PathState& st){
    return fsPathFromUtf8(joinParts(st));
}

// ---------- 构造 ----------
bool chunkIsAbsolute(const std::string& chunk, const std::string& flavor){
    if (chunk.empty()) return false;
    if (chunk[0] == '/' || chunk[0] == '\\') return true;
    if (flavor == "windows" && chunk.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(chunk[0])) && chunk[1] == ':')
        return true;
    return false;
}

value makePath(cffi& a, const char* className){
    std::vector<std::string> chunks;
    for (size_t i = 0; i < a.argc(); ++i){
        value v = a.arg(i);
        auto* other = pathOf(v);
        if (other) chunks.push_back(joinParts(*other));
        else if (v.type == "str" && v.ptr) chunks.push_back(*static_cast<std::string*>(v.ptr));
        else if (v.type == "none") continue;
        else {
            a.vm->raiseError("TypeError",
                "expected str or PathLike object, got '" + a.vm->pythonTypeName(v) + "'");
            return a.none();
        }
    }
    std::string flavor = flavorOf(className);
    std::string raw;
    for (size_t i = 0; i < chunks.size(); ++i){
        if (i == 0){ raw = chunks[i]; continue; }
        if (chunkIsAbsolute(chunks[i], flavor)){ raw = chunks[i]; continue; }
        if (raw.empty()){ raw = chunks[i]; continue; }
        char last = raw.back();
        if (last != '/' && last != '\\') raw += '/';
        raw += chunks[i];
    }
    PathState st;
    parsePath(raw, flavor, pureOf(className), className, st);
    return buildPathObj(a, st);
}

static value fnPurePathCtor(cffi& a){ return makePath(a, "PurePath"); }
static value fnPurePosixCtor(cffi& a){ return makePath(a, "PurePosixPath"); }
static value fnPureWindowsCtor(cffi& a){ return makePath(a, "PureWindowsPath"); }
static value fnPathCtor(cffi& a){ return makePath(a, "Path"); }
static value fnPosixCtor(cffi& a){ return makePath(a, "PosixPath"); }
static value fnWindowsCtor(cffi& a){ return makePath(a, "WindowsPath"); }

// ---------- 基础属性 ----------
static value fnPathStr(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkstr("");
    return a.mkstr(joinDisplay(*st));
}

static value fnPathRepr(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkstr("Path('.')");
    return a.mkstr(concreteClassName(*st) + "(" +
                   pythonRepr(*a.vm, a.mkstr(joinDisplay(*st))) + ")");
}

static value fnPathName(cffi& a){
    auto* st = pathOf(a.self());
    return a.mkstr(st ? nameOf(*st) : "");
}

static value fnPathStem(cffi& a){
    auto* st = pathOf(a.self());
    return a.mkstr(st ? stemOf(nameOf(*st)) : "");
}

static value fnPathSuffix(cffi& a){
    auto* st = pathOf(a.self());
    return a.mkstr(st ? suffixOf(nameOf(*st)) : "");
}

static value fnPathSuffixes(cffi& a){
    auto* st = pathOf(a.self());
    std::vector<value> out;
    if (st)
        for (auto& s : suffixesOf(nameOf(*st))) out.push_back(a.mkstr(s));
    return a.mklist(out);
}

static value fnPathParts(cffi& a){
    auto* st = pathOf(a.self());
    std::vector<value> out;
    if (st){
        std::string root = rootPart(*st);
        if (!root.empty()) out.push_back(a.mkstr(root));
        for (auto& p : st->parts) out.push_back(a.mkstr(p));
    }
    return a.vm->makeTuple(out);
}

static value fnPathParent(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    return buildPathObj(a, parentState(*st));
}

static value fnPathParents(cffi& a){
    auto* st = pathOf(a.self());
    std::vector<value> out;
    if (st){
        PathState cur = *st;
        while (!cur.parts.empty()){
            cur.parts.pop_back();
            out.push_back(buildPathObj(a, cur));
        }
    }
    return a.vm->makeTuple(out);
}

static value fnPathAsPosix(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkstr("");
    return a.mkstr(joinParts(*st));
}

static value fnPathFspath(cffi& a){
    return fnPathStr(a);
}

static value fnPathIsAbsolute(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkbool(false);
    if (st->flavor == "windows"){
        if (st->prefix.size() >= 3 && st->prefix[0] == '/' && st->prefix[1] == '/')
            return a.mkbool(true);
        return a.mkbool(st->prefix.size() >= 2 && st->prefix[1] == ':');
    }
    return a.mkbool(!st->prefix.empty());
}

static value fnPathEq(cffi& a){
    auto* ls = pathOf(a.self());
    auto* rs = pathOf(a.arg(0));
    if (!ls || !rs) return a.mkbool(false);
    return a.mkbool(sameParts(*ls, *rs));
}

static value fnPathHash(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkint(0);
    std::string key = joinParts(*st);
    if (st->flavor == "windows") key = lowerCopy(key);
    return a.mkint(a.vm->hashKey(a.mkstr(key)));
}

static value fnPathTruediv(cffi& a){
    auto* st = pathOf(a.self());
    value other = a.arg(0);
    if (!st) return a.none();
    std::string chunk;
    auto* os = pathOf(other);
    if (os) chunk = joinParts(*os);
    else if (other.type == "str" && other.ptr) chunk = *static_cast<std::string*>(other.ptr);
    else {
        a.vm->raiseError("TypeError",
            "unsupported operand type(s) for /: '" + a.vm->pythonTypeName(a.self()) +
            "' and '" + a.vm->pythonTypeName(other) + "'");
        return a.none();
    }
    PathState joined;
    std::string raw = chunkIsAbsolute(chunk, st->flavor) || joinParts(*st).empty()
                          ? chunk
                          : joinParts(*st) + "/" + chunk;
    parsePath(raw, st->flavor, st->pure, st->className, joined);
    return buildPathObj(a, joined);
}

static value fnPathJoinpath(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    PathState cur = *st;
    for (size_t i = 0; i < a.argc(); ++i){
        value v = a.arg(i);
        std::string chunk;
        auto* os = pathOf(v);
        if (os) chunk = joinParts(*os);
        else if (v.type == "str" && v.ptr) chunk = *static_cast<std::string*>(v.ptr);
        else {
            a.vm->raiseError("TypeError",
                "expected str or PathLike object, got '" + a.vm->pythonTypeName(v) + "'");
            return a.none();
        }
        PathState joined;
        std::string raw = chunkIsAbsolute(chunk, cur.flavor) || joinParts(cur).empty()
                              ? chunk
                              : joinParts(cur) + "/" + chunk;
        parsePath(raw, cur.flavor, cur.pure, cur.className, joined);
        cur = joined;
    }
    return buildPathObj(a, cur);
}

static value fnPathWithName(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    if (st->parts.empty()){
        a.vm->raiseError("ValueError", "Path has an empty name");
        return a.none();
    }
    std::string name = a.arg_str(0);
    if (name.empty()){
        a.vm->raiseError("ValueError", "Invalid name ''");
        return a.none();
    }
    PathState out = *st;
    out.parts.back() = name;
    return buildPathObj(a, out);
}

static value fnPathWithSuffix(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    std::string suffix = a.argc() > 0 ? a.arg_str(0) : "";
    if (!suffix.empty() && suffix[0] != '.'){
        a.vm->raiseError("ValueError", "Invalid suffix '" + suffix + "'");
        return a.none();
    }
    std::string name = nameOf(*st);
    if (name.empty()){
        a.vm->raiseError("ValueError", "Path has an empty name");
        return a.none();
    }
    PathState out = *st;
    std::string stem = stemOf(name);
    out.parts.back() = stem + suffix;
    return buildPathObj(a, out);
}

static value fnPathMatch(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkbool(false);
    std::string pattern = a.arg_str(0);
    bool ci = (st->flavor == "windows");
    // 从右侧逐段匹配
    std::vector<std::string> patParts;
    std::string norm = pattern;
    std::size_t start = 0;
    while (start <= norm.size()){
        size_t slash = norm.find('/', start);
        std::string part = slash == std::string::npos ? norm.substr(start)
                                                      : norm.substr(start, slash - start);
        if (!part.empty() && part != ".") patParts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    size_t pi = patParts.size();
    size_t si = st->parts.size();
    while (pi > 0){
        --pi;
        std::string part = patParts[pi];
        if (part == "**") return a.mkbool(true);
        if (si == 0) return a.mkbool(false);
        --si;
        if (!wildMatch(st->parts[si], part, ci)) return a.mkbool(false);
    }
    return a.mkbool(true);
}

static value fnPathRelativeTo(cffi& a){
    auto* st = pathOf(a.self());
    auto* other = pathOf(a.arg(0));
    if (!st || !other) return a.none();
    if (st->prefix != other->prefix || st->parts.size() < other->parts.size()){
        a.vm->raiseError("ValueError",
            a.vm->toString(a.self()) + " is not in the subpath of " +
            a.vm->toString(a.arg(0)));
        return a.none();
    }
    bool ci = (st->flavor == "windows");
    for (size_t i = 0; i < other->parts.size(); ++i){
        bool same = ci ? lowerCopy(st->parts[i]) == lowerCopy(other->parts[i])
                       : st->parts[i] == other->parts[i];
        if (!same){
            a.vm->raiseError("ValueError",
                a.vm->toString(a.self()) + " is not in the subpath of " +
                a.vm->toString(a.arg(0)));
            return a.none();
        }
    }
    PathState out;
    parsePath("", st->flavor, st->pure, st->className, out);
    for (size_t i = other->parts.size(); i < st->parts.size(); ++i)
        out.parts.push_back(st->parts[i]);
    return buildPathObj(a, out);
}

// ---------- Path 文件系统操作 ----------
static value fnPathExists(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkbool(false);
    std::error_code ec;
    return a.mkbool(fs::exists(toFsPath(*st), ec));
}

static value fnPathIsFile(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkbool(false);
    std::error_code ec;
    return a.mkbool(fs::is_regular_file(toFsPath(*st), ec));
}

static value fnPathIsDir(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.mkbool(false);
    std::error_code ec;
    return a.mkbool(fs::is_directory(toFsPath(*st), ec));
}

static value fnPathIterdir(cffi& a){
    auto* st = pathOf(a.self());
    std::vector<value> out;
    if (!st) return a.mklist(out);
    std::error_code ec;
    fs::directory_iterator it(toFsPath(*st), ec);
    if (ec){
        a.vm->raiseError("FileNotFoundError",
            "[WinError 3] The system cannot find the path specified: '" + joinParts(*st) + "'");
        return a.none();
    }
    for (auto& entry : it){
        PathState child = *st;
        child.parts.push_back(pathText(entry.path().filename()));
        out.push_back(buildPathObj(a, child));
    }
    return a.mklist(out);
}

// 目录遍历 + 通配符
void collectGlob(cffi& a, const fs::path& base, const std::string& relBase,
                 const std::vector<std::string>& patParts, size_t index,
                 bool recursive, bool ci, std::vector<value>& out,
                 const std::string& className, const std::string& flavor){
    if (index >= patParts.size()){
        if (!relBase.empty()){
            PathState st;
            parsePath(relBase, flavor, false, className, st);
            out.push_back(buildPathObj(a, st));
        }
        return;
    }
    std::string part = patParts[index];
    if (part == "**"){
        // 匹配任意深度
        std::error_code ec;
        fs::recursive_directory_iterator it(base, ec);
        if (!ec){
            for (auto& entry : it){
                std::string fname = pathText(entry.path().filename());
                std::string rel = relBase.empty() ? fname : relBase + "/" + fname;
                if (!entry.is_directory(ec)) continue;
                collectGlob(a, entry.path(), rel, patParts, index + 1, recursive, ci, out,
                            className, flavor);
            }
        }
        collectGlob(a, base, relBase, patParts, index + 1, recursive, ci, out,
                    className, flavor);
        return;
    }
    bool magic = hasMagic(part);
    if (!magic){
        fs::path next = base / fsPathFromUtf8(part);
        std::error_code ec;
        if (fs::exists(next, ec)){
            std::string rel = relBase.empty() ? part : relBase + "/" + part;
            if (index + 1 >= patParts.size()){
                PathState st;
                parsePath(rel, flavor, false, className, st);
                out.push_back(buildPathObj(a, st));
            } else {
                collectGlob(a, next, rel, patParts, index + 1, recursive, ci, out,
                            className, flavor);
            }
        }
        return;
    }
    std::error_code ec;
    fs::directory_iterator it(base, ec);
    if (ec) return;
    for (auto& entry : it){
        std::string fname = pathText(entry.path().filename());
        if (!wildMatch(fname, part, ci)) continue;
        std::string rel = relBase.empty() ? fname : relBase + "/" + fname;
        if (index + 1 >= patParts.size()){
            PathState st;
            parsePath(rel, flavor, false, className, st);
            out.push_back(buildPathObj(a, st));
        } else {
            collectGlob(a, entry.path(), rel, patParts, index + 1, recursive, ci, out,
                        className, flavor);
        }
    }
}

static value doGlob(cffi& a, bool recursive){
    auto* st = pathOf(a.self());
    std::vector<value> out;
    if (!st) return a.mklist(out);
    std::string pattern = a.arg_str(0);
    if (recursive && pattern.rfind("**/", 0) != 0) pattern = "**/" + pattern;
    std::vector<std::string> patParts;
    size_t start = 0;
    std::string norm = pattern;
    for (auto& ch : norm) if (ch == '\\') ch = '/';
    while (start <= norm.size()){
        size_t slash = norm.find('/', start);
        std::string part = slash == std::string::npos ? norm.substr(start)
                                                      : norm.substr(start, slash - start);
        if (!part.empty() && part != ".") patParts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    bool ci = (st->flavor == "windows");
    PathState named = *st;
    named.className = concreteClassName(*st);
    std::string className = named.className;
    fs::path base = toFsPath(*st);
    collectGlob(a, base, joinParts(*st), patParts, 0, recursive, ci, out, className, st->flavor);
    std::sort(out.begin(), out.end(), [](const value& x, const value& y){
        auto* xs = pathOf(x);
        auto* ys = pathOf(y);
        if (!xs || !ys) return false;
        return joinParts(*xs) < joinParts(*ys);
    });
    return a.mklist(out);
}

static value fnPathGlob(cffi& a){ return doGlob(a, false); }
static value fnPathRglob(cffi& a){ return doGlob(a, true); }

static value fnPathMkdir(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    bool parents = a.has_kwarg("parents") ? a.kwarg_bool("parents", false) : false;
    bool existOk = a.has_kwarg("exist_ok") ? a.kwarg_bool("exist_ok", false) : false;
    std::error_code ec;
    fs::path p = toFsPath(*st);
    bool exists = fs::exists(p, ec);
    if (exists){
        if (!existOk){
            a.vm->raiseError("FileExistsError",
                "[WinError 183] Cannot create a file when that file already exists: '" +
                joinParts(*st) + "'");
            return a.none();
        }
        if (!fs::is_directory(p, ec)){
            a.vm->raiseError("FileExistsError", "File exists: '" + joinParts(*st) + "'");
        }
        return a.none();
    }
    if (parents) fs::create_directories(p, ec);
    else fs::create_directory(p, ec);
    if (ec){
        a.vm->raiseError("FileNotFoundError",
            "[WinError 3] The system cannot find the path specified: '" + joinParts(*st) + "'");
    }
    return a.none();
}

static std::string translateNewlinesOut(const std::string& text){
#if defined(_WIN32)
    std::string out;
    out.reserve(text.size() * 2);
    for (size_t i = 0; i < text.size(); ++i){
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) out += "\r\n";
        else out += text[i];
    }
    return out;
#else
    return text;
#endif
}

static std::string translateNewlinesIn(const std::string& text){
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i){
        if (text[i] == '\r'){
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            out += text[i];
        }
    }
    return out;
}

static value fnPathReadText(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    std::ifstream in(toFsPath(*st), std::ios::binary);
    if (!in.is_open()){
        a.vm->raiseError("FileNotFoundError",
            "[Errno 2] No such file or directory: '" + joinParts(*st) + "'");
        return a.none();
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return a.mkstr(translateNewlinesIn(text));
}

static value fnPathWriteText(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    std::string text = translateNewlinesOut(a.vm->toString(a.arg(0)));
    std::ofstream out(toFsPath(*st), std::ios::binary | std::ios::trunc);
    if (!out.is_open()){
        a.vm->raiseError("FileNotFoundError",
            "[Errno 2] No such file or directory: '" + joinParts(*st) + "'");
        return a.none();
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    return a.none();
}

static value fnPathTouch(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    bool existOk = a.has_kwarg("exist_ok") ? a.kwarg_bool("exist_ok", true) : true;
    std::error_code ec;
    fs::path p = toFsPath(*st);
    if (!fs::exists(p, ec)){
        std::ofstream out(p, std::ios::binary | std::ios::app);
        if (!out.is_open()){
            a.vm->raiseError("FileNotFoundError",
                "[Errno 2] No such file or directory: '" + joinParts(*st) + "'");
        }
        return a.none();
    }
    if (!existOk){
        a.vm->raiseError("FileExistsError", "File exists: '" + joinParts(*st) + "'");
    }
    return a.none();
}

static value fnPathUnlink(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    bool missingOk = a.has_kwarg("missing_ok") ? a.kwarg_bool("missing_ok", false) : false;
    std::error_code ec;
    fs::remove(toFsPath(*st), ec);
    if (ec && !missingOk){
        a.vm->raiseError("FileNotFoundError",
            "[WinError 2] The system cannot find the file specified: '" + joinParts(*st) + "'");
    }
    return a.none();
}

static value fnPathRmdir(cffi& a){
    auto* st = pathOf(a.self());
    if (!st) return a.none();
    std::error_code ec;
    fs::remove(toFsPath(*st), ec);
    if (ec)
        a.vm->raiseError("OSError", "Cannot remove directory: '" + joinParts(*st) + "'");
    return a.none();
}

static value fnPathOpen(cffi& a){
    auto* st = pathOf(a.self());
    ffilib* builtins = a.vm->findCffiLibrary("builtins");
    if (!st || !builtins){
        a.vm->raiseError("RuntimeError", "open() unavailable");
        return a.none();
    }
    std::vector<value> args;
    args.push_back(a.mkstr(joinParts(*st)));
    for (size_t i = 0; i < a.argc(); ++i) args.push_back(a.arg(i));
    value result = a.none();
    if (!cffiCallFunction(*builtins, "open", *a.vm, args, result, a.kwargv)){
        a.vm->raiseError("RuntimeError", "open() failed");
        return a.none();
    }
    return result;
}

static value fnPathEnter(cffi& a){ return a.self(); }

}

ffilib cffi_pathlib_lib(){
    std::map<std::string, void*> methods = {
        {"__str__", (void*)fnPathStr},
        {"__repr__", (void*)fnPathRepr},
        {"__fspath__", (void*)fnPathFspath},
        {"__truediv__", (void*)fnPathTruediv},
        {"__eq__", (void*)fnPathEq},
        {"__hash__", (void*)fnPathHash},
        {"__enter__", (void*)fnPathEnter},
        {"name", (void*)fnPathName},
        {"stem", (void*)fnPathStem},
        {"suffix", (void*)fnPathSuffix},
        {"suffixes", (void*)fnPathSuffixes},
        {"parts", (void*)fnPathParts},
        {"parent", (void*)fnPathParent},
        {"parents", (void*)fnPathParents},
        {"as_posix", (void*)fnPathAsPosix},
        {"is_absolute", (void*)fnPathIsAbsolute},
        {"joinpath", (void*)fnPathJoinpath},
        {"with_name", (void*)fnPathWithName},
        {"with_suffix", (void*)fnPathWithSuffix},
        {"match", (void*)fnPathMatch},
        {"relative_to", (void*)fnPathRelativeTo},
        {"exists", (void*)fnPathExists},
        {"is_file", (void*)fnPathIsFile},
        {"is_dir", (void*)fnPathIsDir},
        {"iterdir", (void*)fnPathIterdir},
        {"glob", (void*)fnPathGlob},
        {"rglob", (void*)fnPathRglob},
        {"mkdir", (void*)fnPathMkdir},
        {"read_text", (void*)fnPathReadText},
        {"write_text", (void*)fnPathWriteText},
        {"touch", (void*)fnPathTouch},
        {"unlink", (void*)fnPathUnlink},
        {"rmdir", (void*)fnPathRmdir},
        {"open", (void*)fnPathOpen}
    };

    std::map<std::string, ffi_class> classes;
    {
        ffi_class c; c.name = "PurePath"; c.constructor = (void*)fnPurePathCtor; c.methods = methods;
        classes["PurePath"] = c;
    }
    {
        ffi_class c; c.name = "PurePosixPath"; c.constructor = (void*)fnPurePosixCtor; c.methods = methods;
        classes["PurePosixPath"] = c;
    }
    {
        ffi_class c; c.name = "PureWindowsPath"; c.constructor = (void*)fnPureWindowsCtor; c.methods = methods;
        classes["PureWindowsPath"] = c;
    }
    {
        ffi_class c; c.name = "Path"; c.constructor = (void*)fnPathCtor; c.methods = methods;
        classes["Path"] = c;
    }
    {
        ffi_class c; c.name = "PosixPath"; c.constructor = (void*)fnPosixCtor; c.methods = methods;
        classes["PosixPath"] = c;
    }
    {
        ffi_class c; c.name = "WindowsPath"; c.constructor = (void*)fnWindowsCtor; c.methods = methods;
        classes["WindowsPath"] = c;
    }

    return cffi_lib("pathlib", {}, {}, classes);
}

}
