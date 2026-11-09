#include "../cffi.hpp"
#include "../platform.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace minipy {

using namespace minipy;

namespace {
const char kSep =
#if defined(_WIN32)
    '\\';
#else
    '/';
#endif

// path 错误
// path error
// path ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
value pathError(cffi& a, const std::string& message){
    a.vm->raiseError("OSError", message);
    return a.none();
}

// dir Part
// dir part
// dir Part
std::string dirPart(const std::string& p){
    size_t pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    if (pos == 0) return p.substr(0, 1);
    return p.substr(0, pos);
}

// base Part
// base part
// base Part
std::string basePart(const std::string& p){
    size_t pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return p;
    return p.substr(pos + 1);
}

// home Directory
// home directory
// home Directory
std::string homeDirectory(){
#if defined(_WIN32)
    const char* profile = std::getenv("USERPROFILE");
    if (profile != nullptr && *profile != '\0') return profile;
#endif
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') return home;
    const char* drive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (drive != nullptr && homePath != nullptr){
        return std::string(drive) + homePath;
    }
    return "";
}

// 是否为根成分, 如 "/"、"C:"、"\\server\\share"
// Test whether a token is a root component such as "/", "C:" or "\\server\\share"
// ᠦᠨᠳᠦᠰᠦ ᠬᠡᠰᠡᠭ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool isRootComponent(const std::string& token){
    if (token == "/" || token == "\\") return true;
    if (token.size() >= 2 && token[1] == ':') return true;
    if (token.size() >= 2 && token[0] == '\\' && token[1] == '\\') return true;
    return false;
}

// 把路径拆成根成分与各段名字
// Split a path into a root component and name segments
// ᠵᠠᠮ ᠢ ᠦᠰᠦᠭ ᠪᠣᠯᠭ᠎ᠠᠨ ᠰᠠᠯᠭᠠᠬᠤ
std::vector<std::string> splitComponents(const std::string& path){
    std::vector<std::string> tokens;
    size_t i = 0;
    std::string root;
#if defined(_WIN32)
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\'){
        size_t first = path.find_first_of("/\\", 2);
        if (first != std::string::npos){
            size_t second = path.find_first_of("/\\", first + 1);
            if (second != std::string::npos){
                root = path.substr(0, second);
                i = second;
            } else {
                root = path;
                i = path.size();
            }
        } else {
            root = path;
            i = path.size();
        }
    } else if (path.size() >= 2 && path[1] == ':'){
        root = path.substr(0, 2);
        i = 2;
        if (i < path.size() && (path[i] == '/' || path[i] == '\\')){
            root.push_back(path[i]);
            ++i;
        }
    } else if (!path.empty() && (path[0] == '/' || path[0] == '\\')){
        root = path.substr(0, 1);
        i = 1;
    }
#else
    if (!path.empty() && path[0] == '/') root = "/";
#endif
    if (!root.empty()) tokens.push_back(root);
    while (i < path.size()){
        size_t end = i;
        while (end < path.size() && path[end] != '/' && path[end] != '\\') ++end;
        if (end > i) tokens.push_back(path.substr(i, end - i));
        i = end + 1;
    }
    return tokens;
}

// 把根成分与段名字重新拼成路径
// Join a root component and name segments back into a path
// ᠨᠢᠭᠡᠳᠦᠭᠰᠡᠨ ᠵᠠᠮ ᠪᠣᠯᠭ᠎ᠠᠬᠤ
std::string joinComponents(const std::vector<std::string>& tokens){
    std::string result;
    for (size_t i = 0; i < tokens.size(); ++i){
        const std::string& token = tokens[i];
        if (result.empty() || isRootComponent(token)){
            result = token;
            continue;
        }
        if (result.back() != '/' && result.back() != '\\' && result.back() != ':'){
            result.push_back(kSep);
        }
        result += token;
    }
    return result;
}
}

// fn 拼接
// fn join
// fn ᠨᠢᠯᠡᠨ᠎ᠠ
static value fnJoin(cffi& a){
    if (a.argc() == 0) return a.mkstr("");
    std::string result = a.arg_str(0);
    for (size_t i = 1; i < a.argc(); ++i){
        std::string part = a.arg_str(i);
        if (part.empty()) continue;
        if (mpIsAbsolutePath(part)){
            result = part;
            continue;
        }
#if defined(_WIN32)
        if (part.size() >= 2 && part[0] != '\\' && part[0] != '/' && part[1] == ':'){
            result = part;
            continue;
        }
        if ((part[0] == '\\' || part[0] == '/') &&
            !(result.size() >= 2 && result[1] == ':')){
            result = part;
            continue;
        }
#endif
        if (result.empty()){
            result = part;
            continue;
        }
        if (result.back() != '/' && result.back() != '\\' && result.back() != ':'){
            result.push_back(mpPathSeparator());
        }
        result += part;
    }
    return a.mkstr(result);
}

// fn 拆分
// fn split
// fn ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ
static value fnSplit(cffi& a){
    std::string p = a.arg_str(0);
    size_t pos = p.find_last_of("/\\");
    std::string head;
    std::string tail;
    if (pos == std::string::npos){
        head = "";
        tail = p;
    } else {
        size_t end = pos;
        while (end > 0 && (p[end] == '/' || p[end] == '\\')) end--;
        head = p.substr(0, end + 1);
        tail = p.substr(pos + 1);
    }
    return a.mklist({a.mkstr(head), a.mkstr(tail)});
}

// fn Basename
// fn basename
// fn Basename
static value fnBasename(cffi& a){
    return a.mkstr(basePart(a.arg_str(0)));
}

// fn Dirname
// fn dirname
// fn Dirname
static value fnDirname(cffi& a){
    return a.mkstr(dirPart(a.arg_str(0)));
}

// fn Splitext
// fn splitext
// fn Splitext
static value fnSplitext(cffi& a){
    std::string p = a.arg_str(0);
    std::string base = basePart(p);
    std::string root = p.substr(0, p.size() - base.size());
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos || dot == 0){
        return a.mklist({a.mkstr(p), a.mkstr("")});
    }
    return a.mklist({a.mkstr(root + base.substr(0, dot)), a.mkstr(base.substr(dot))});
}

// fn Exists
// fn exists
// fn Exists
static value fnExists(cffi& a){
    mpFileInfo info;
    return a.mkbool(mpPathInfo(a.arg_str(0), info) && info.targetExists);
}

// fn Isfile
// fn isfile
// fn Isfile
static value fnIsfile(cffi& a){
    mpFileInfo info;
    return a.mkbool(mpPathInfo(a.arg_str(0), info) && info.targetExists && info.isRegularFile);
}

// fn Isdir
// fn isdir
// fn Isdir
static value fnIsdir(cffi& a){
    mpFileInfo info;
    return a.mkbool(mpPathInfo(a.arg_str(0), info) && info.targetExists && info.isDirectory);
}

// fn Islink
// fn islink
// fn Islink
static value fnIslink(cffi& a){
    mpFileInfo info;
    return a.mkbool(mpPathInfo(a.arg_str(0), info) && info.isSymlink);
}

// fn Isabs
// fn isabs
// fn Isabs
static value fnIsabs(cffi& a){
    return a.mkbool(mpIsAbsolutePath(a.arg_str(0)));
}

// fn Abspath
// fn abspath
// fn Abspath
static value fnAbspath(cffi& a){
    std::string absolute = mpAbsolutePath(a.arg_str(0));
    if (absolute.empty()) return pathError(a, "cannot determine absolute path");
    return a.mkstr(mpNormalizePath(absolute));
}

// fn Normpath
// fn normpath
// fn Normpath
static value fnNormpath(cffi& a){
    return a.mkstr(mpNormalizePath(a.arg_str(0)));
}

// fn Getsize
// fn getsize
// fn Getsize
static value fnGetsize(cffi& a){
    mpFileInfo info;
    if (!mpPathInfo(a.arg_str(0), info) || !info.targetExists){
        return pathError(a, "No such file or directory: '" + a.arg_str(0) + "'");
    }
    return a.mkint(info.size);
}

// fn Relpath
// fn relpath
// fn Relpath
static value fnRelpath(cffi& a){
    std::string target = mpNormalizePath(mpAbsolutePath(a.arg_str(0)));
    std::string start = mpNormalizePath((a.argc() > 1) ? mpAbsolutePath(a.arg_str(1))
                                                     : mpCurrentDirectory());
    if (start.empty()) return pathError(a, "cannot determine current directory");
    std::vector<std::string> targetTokens = splitComponents(target);
    std::vector<std::string> startTokens = splitComponents(start);
    if (!targetTokens.empty() && !startTokens.empty() &&
        isRootComponent(targetTokens[0]) && isRootComponent(startTokens[0]) &&
        targetTokens[0] != startTokens[0]){
        return pathError(a, "path is on a different drive: '" + a.arg_str(0) + "'");
    }
    size_t index = 0;
    while (index < targetTokens.size() && index < startTokens.size() &&
           targetTokens[index] == startTokens[index]){
        ++index;
    }
    std::vector<std::string> result;
    for (size_t k = index; k < startTokens.size(); ++k) result.push_back("..");
    for (size_t k = index; k < targetTokens.size(); ++k) result.push_back(targetTokens[k]);
    if (result.empty()) return a.mkstr(".");
    return a.mkstr(joinComponents(result));
}

// fn Commonpath
// fn commonpath
// fn Commonpath
static value fnCommonpath(cffi& a){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (items.empty()){
        a.vm->raiseError("ValueError", "commonpath() arg is an empty sequence");
        return a.none();
    }
    std::vector<std::string> common =
        splitComponents(mpNormalizePath(a.vm->toString(items[0])));
    for (size_t i = 1; i < items.size(); ++i){
        std::vector<std::string> tokens =
            splitComponents(mpNormalizePath(a.vm->toString(items[i])));
        size_t matched = 0;
        while (matched < common.size() && matched < tokens.size() &&
               common[matched] == tokens[matched]){
            ++matched;
        }
        common.resize(matched);
    }
    if (common.empty()) return a.mkstr("");
    return a.mkstr(joinComponents(common));
}

// fn Ismount
// fn ismount
// fn Ismount
static value fnIsmount(cffi& a){
    std::string p = a.arg_str(0);
#if defined(_WIN32)
    if (p.empty()) return a.mkbool(false);
    size_t end = p.find_last_not_of("/\\");
    if (end == std::string::npos) return a.mkbool(false);
    std::string trimmed = p.substr(0, end + 1);
    if (p.size() <= trimmed.size()) return a.mkbool(false);
    bool driveRoot = trimmed.size() == 2 && trimmed[1] == ':' &&
                     ((trimmed[0] >= 'A' && trimmed[0] <= 'Z') ||
                      (trimmed[0] >= 'a' && trimmed[0] <= 'z'));
    bool uncRoot = false;
    if (trimmed.size() > 2 && trimmed[0] == '\\' && trimmed[1] == '\\'){
        size_t count = 0;
        for (char ch : trimmed) if (ch == '\\') ++count;
        uncRoot = (count == 3);
    }
    return a.mkbool(driveRoot || uncRoot);
#else
    return a.mkbool(p == "/");
#endif
}

// fn Samefile
// fn samefile
// fn Samefile
static value fnSamefile(cffi& a){
    int same = mpEquivalent(a.arg_str(0), a.arg_str(1));
    if (same < 0) return pathError(a, "No such file or directory");
    return a.mkbool(same != 0);
}

// fn Expanduser
// fn expanduser
// fn Expanduser
static value fnExpanduser(cffi& a){
    std::string p = a.arg_str(0);
    if (p.empty() || p[0] != '~') return a.mkstr(p);
    if (p.size() > 1 && p[1] != '/' && p[1] != '\\') return a.mkstr(p);
    std::string home = homeDirectory();
    if (home.empty()) return a.mkstr(p);
    return a.mkstr(home + p.substr(1));
}

// fn Expandvars
// fn expandvars
// fn Expandvars
static value fnExpandvars(cffi& a){
    std::string p = a.arg_str(0);
    std::string result;
    result.reserve(p.size());
    size_t i = 0;
    while (i < p.size()){
        if (p[i] == '%'){
            size_t end = p.find('%', i + 1);
            if (end != std::string::npos && end > i + 1){
                std::string name = p.substr(i + 1, end - i - 1);
                const char* value = std::getenv(name.c_str());
                if (value != nullptr) result += value;
                else result.append(p, i, end - i + 1);
                i = end + 1;
                continue;
            }
            result.push_back(p[i++]);
            continue;
        }
        if (p[i] == '$'){
            size_t start = i + 1;
            size_t end = start;
            bool brace = false;
            if (start < p.size() && p[start] == '{'){
                brace = true;
                start++;
                end = start;
                while (end < p.size() && p[end] != '}') ++end;
            } else {
                while (end < p.size() &&
                       (std::isalnum(static_cast<unsigned char>(p[end])) || p[end] == '_')){
                    ++end;
                }
            }
            bool complete = (brace) ? (end < p.size()) : true;
            if (end > start && complete){
                std::string name = p.substr(start, end - start);
                const char* value = std::getenv(name.c_str());
                if (value != nullptr){
                    result += value;
                    i = brace ? end + 1 : end;
                    continue;
                }
            }
            result.push_back(p[i++]);
            continue;
        }
        result.push_back(p[i++]);
    }
    return a.mkstr(result);
}

// fn Normcase
// fn normcase
// fn Normcase
static value fnNormcase(cffi& a){
    std::string p = a.arg_str(0);
#if defined(_WIN32)
    for (char& ch : p){
        if (ch == '/') ch = '\\';
        else ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
#endif
    return a.mkstr(p);
}

// fn Commonprefix
// fn commonprefix
// fn Commonprefix
static value fnCommonprefix(cffi& a){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (items.empty()) return a.mkstr("");
    std::string prefix = a.vm->toString(items[0]);
    for (size_t i = 1; i < items.size(); ++i){
        std::string next = a.vm->toString(items[i]);
        size_t n = 0;
        while (n < prefix.size() && n < next.size() && prefix[n] == next[n]) ++n;
        prefix.resize(n);
        if (prefix.empty()) break;
    }
    return a.mkstr(prefix);
}

// data Supports Unicode Filenames
// data supports unicode filenames
// data Supports Unicode Filenames
static value dataSupportsUnicodeFilenames(cffi& a){ return a.mkbool(true); }

// data Sep
// data sep
// data Sep
static value dataSep(cffi& a){ return a.mkstr(std::string(1, kSep)); }

// data Altsep
// data altsep
// data Altsep
static value dataAltsep(cffi& a){
#if defined(_WIN32)
    return a.mkstr("/");
#else
    return a.none();
#endif
}

// data Extsep
// data extsep
// data Extsep
static value dataExtsep(cffi& a){ return a.mkstr("."); }

// data Curdir
// data curdir
// data Curdir
static value dataCurdir(cffi& a){ return a.mkstr("."); }

// data Pardir
// data pardir
// data Pardir
static value dataPardir(cffi& a){ return a.mkstr(".."); }

// data Devnull
// data devnull
// data Devnull
static value dataDevnull(cffi& a){
#if defined(_WIN32)
    return a.mkstr("nul");
#else
    return a.mkstr("/dev/null");
#endif
}

// cffi os path lib
// cffi os path lib
// cffi os path lib
ffilib cffi_os_path_lib(){
    return cffi_lib(
        "os.path",
        {
            {"join", (void*)fnJoin},
            {"split", (void*)fnSplit},
            {"basename", (void*)fnBasename},
            {"dirname", (void*)fnDirname},
            {"splitext", (void*)fnSplitext},
            {"exists", (void*)fnExists},
            {"isfile", (void*)fnIsfile},
            {"isdir", (void*)fnIsdir},
            {"islink", (void*)fnIslink},
            {"isabs", (void*)fnIsabs},
            {"abspath", (void*)fnAbspath},
            {"normpath", (void*)fnNormpath},
            {"getsize", (void*)fnGetsize},
            {"relpath", (void*)fnRelpath},
            {"commonpath", (void*)fnCommonpath},
            {"ismount", (void*)fnIsmount},
            {"samefile", (void*)fnSamefile},
            {"expanduser", (void*)fnExpanduser},
            {"expandvars", (void*)fnExpandvars},
            {"normcase", (void*)fnNormcase},
            {"commonprefix", (void*)fnCommonprefix}
        },
        {
            {"supports_unicode_filenames", (void*)dataSupportsUnicodeFilenames},
            {"sep", (void*)dataSep},
            {"altsep", (void*)dataAltsep},
            {"extsep", (void*)dataExtsep},
            {"curdir", (void*)dataCurdir},
            {"pardir", (void*)dataPardir},
            {"devnull", (void*)dataDevnull}
        });
}


}
