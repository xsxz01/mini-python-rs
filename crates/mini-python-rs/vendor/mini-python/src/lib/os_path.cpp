#include "../cffi.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace minipy {




using namespace minipy;
namespace fs = std::filesystem;

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
}

// fn 拼接
// fn join
// fn ᠨᠢᠯᠡᠨ᠎ᠠ
static value fnJoin(cffi& a){
    if (a.argc() == 0) return a.mkstr("");
    fs::path result(a.arg_str(0));
    for (size_t i = 1; i < a.argc(); ++i){
        std::string part = a.arg_str(i);
        if (part.empty()) continue;
        fs::path next(part);
        if (next.is_absolute()) result = next;
        else result /= next;
    }
    return a.mkstr(result.string());
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
    std::error_code ec;
    return a.mkbool(fs::exists(fs::path(a.arg_str(0)), ec));
}

// fn Isfile
// fn isfile
// fn Isfile
static value fnIsfile(cffi& a){
    std::error_code ec;
    return a.mkbool(fs::is_regular_file(fs::path(a.arg_str(0)), ec));
}

// fn Isdir
// fn isdir
// fn Isdir
static value fnIsdir(cffi& a){
    std::error_code ec;
    return a.mkbool(fs::is_directory(fs::path(a.arg_str(0)), ec));
}

// fn Islink
// fn islink
// fn Islink
static value fnIslink(cffi& a){
    std::error_code ec;
    return a.mkbool(fs::is_symlink(fs::symlink_status(fs::path(a.arg_str(0)), ec)));
}

// fn Isabs
// fn isabs
// fn Isabs
static value fnIsabs(cffi& a){
    return a.mkbool(fs::path(a.arg_str(0)).is_absolute());
}

// fn Abspath
// fn abspath
// fn Abspath
static value fnAbspath(cffi& a){
    std::error_code ec;
    fs::path result = fs::absolute(fs::path(a.arg_str(0)), ec);
    if (ec) return pathError(a, ec.message());
    return a.mkstr(result.lexically_normal().string());
}

// fn Normpath
// fn normpath
// fn Normpath
static value fnNormpath(cffi& a){
    return a.mkstr(fs::path(a.arg_str(0)).lexically_normal().string());
}

// fn Getsize
// fn getsize
// fn Getsize
static value fnGetsize(cffi& a){
    std::error_code ec;
    auto size = fs::file_size(fs::path(a.arg_str(0)), ec);
    if (ec) return pathError(a, "No such file or directory: '" + a.arg_str(0) + "'");
    return a.mkint(static_cast<long long>(size));
}

// fn Relpath
// fn relpath
// fn Relpath
static value fnRelpath(cffi& a){
    std::error_code ec;
    fs::path target = fs::absolute(fs::path(a.arg_str(0)), ec);
    fs::path start = (a.argc() > 1) ? fs::absolute(fs::path(a.arg_str(1)), ec)
                                    : fs::current_path(ec);
    if (ec) return pathError(a, ec.message());
    fs::path result = fs::relative(target, start, ec);
    if (ec) return pathError(a, ec.message());
    return a.mkstr(result.string());
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
    fs::path result(a.vm->toString(items[0]));
    for (size_t i = 1; i < items.size(); ++i){
        fs::path next(a.vm->toString(items[i]));
        fs::path common;
        auto aIt = result.begin();
        auto bIt = next.begin();
        while (aIt != result.end() && bIt != next.end() && *aIt == *bIt){
            common /= *aIt;
            ++aIt;
            ++bIt;
        }
        result = common;
    }
    return a.mkstr(result.string());
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
    std::error_code ec;
    bool same = fs::equivalent(fs::path(a.arg_str(0)), fs::path(a.arg_str(1)), ec);
    if (ec) return pathError(a, ec.message());
    return a.mkbool(same);
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
