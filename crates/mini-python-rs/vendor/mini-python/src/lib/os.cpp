#include "../cffi.hpp"
#include "../platform.hpp"

#if defined(_WIN32)
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace minipy {




using namespace minipy;

namespace {

// os 错误
// os error
// os ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
value osError(cffi& a, const std::string& message){
    a.vm->raiseError("OSError", message);
    return a.none();
}

// file 非 Found 错误
// file not found error
// file ᠦᠭᠡᠢ Found ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
value fileNotFoundError(cffi& a, const std::string& message){
    a.vm->raiseError("FileNotFoundError", message);
    return a.none();
}
#if defined(_WIN32)

// file Time Seconds
// file time seconds
// file Time Seconds
long long fileTimeSeconds(const FILETIME& ft){
    unsigned long long ticks =
        (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) |
        static_cast<unsigned long long>(ft.dwLowDateTime);
    const unsigned long long epoch = 116444736000000000ULL;
    if (ticks <= epoch) return 0;
    return static_cast<long long>((ticks - epoch) / 10000000ULL);
}

// 拼接 Path
// join path
// ᠨᠢᠯᠡᠨ᠎ᠠ Path
std::wstring joinPath(const std::wstring& dir, const std::wstring& name){
    if (!dir.empty() && (dir.back() == L'\\' || dir.back() == L'/')) return dir + name;
    return dir + L"\\" + name;
}

// walk Into
// walk into
// walk Into
void walkInto(cffi& a, const std::wstring& dirPath, std::vector<value>& out){
    WIN32_FIND_DATAW entry;
    std::wstring pattern = joinPath(dirPath, L"*");
    HANDLE handle = FindFirstFileW(pattern.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) return;
    std::vector<value> dirNames;
    std::vector<value> fileNames;
    std::vector<std::wstring> subDirs;
    do {
        std::wstring name = entry.cFileName;
        if (name == L"." || name == L"..") continue;
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0){
            dirNames.push_back(a.mkstr(wideToUtf8(name)));

            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0){
                subDirs.push_back(name);
            }
        } else {
            fileNames.push_back(a.mkstr(wideToUtf8(name)));
        }
    } while (FindNextFileW(handle, &entry));
    FindClose(handle);
    out.push_back(a.vm->makeTuple(
        {a.mkstr(wideToUtf8(dirPath)), a.mklist(dirNames), a.mklist(fileNames)}));
    for (const auto& name : subDirs){
        walkInto(a, joinPath(dirPath, name), out);
    }
}
#endif

// 逐级创建目录, 返回 1 新建, 0 全部已存在, -1 失败
// Create directory levels one by one: 1 created, 0 all existed, -1 failure
// ᠵᠡᠷᠭᠡ ᠵᠡᠷᠭᠡᠪᠡᠷ ᠭᠠᠵᠠᠷ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int makeDirectoryLevels(const std::string& path){
    if (path.empty()) return -1;
    std::string current;
    size_t i = 0;
#if defined(_WIN32)
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\'){
        size_t first = path.find_first_of("/\\", 2);
        if (first == std::string::npos) return 0;
        size_t second = path.find_first_of("/\\", first + 1);
        if (second == std::string::npos) return 0;
        current = path.substr(0, second);
        i = second + 1;
    } else if (path.size() >= 2 && path[1] == ':'){
        current = path.substr(0, 2);
        i = 2;
        if (i < path.size() && (path[i] == '/' || path[i] == '\\')){
            current += '\\';
            ++i;
        }
    } else if (path[0] == '/' || path[0] == '\\'){
        current += '\\';
        i = 1;
    }
#else
    if (path[0] == '/'){
        current = "/";
        i = 1;
    }
#endif
    int created = 0;
    while (i < path.size()){
        size_t end = i;
        while (end < path.size() && path[end] != '/' && path[end] != '\\') ++end;
        if (end > i){
            if (!current.empty() && current.back() != '/' && current.back() != '\\'){
                current.push_back(mpPathSeparator());
            }
            current += path.substr(i, end - i);
            int result = mpCreateDirectory(current);
            if (result < 0) return -1;
            if (result > 0) created = 1;
        }
        i = end + 1;
    }
    return created;
}
}

// fn Close fd
// fn close
// fn Close
static value fnClose(cffi& a){
    long long fd = a.arg_int(0);
#if defined(_WIN32)
    if (_close(static_cast<int>(fd)) != 0)
        return osError(a, "close failed");
#else
    if (::close(static_cast<int>(fd)) != 0)
        return osError(a, "close failed");
#endif
    return a.none();
}

// fn Getcwd
// fn getcwd
// fn Getcwd
static value fnGetcwd(cffi& a){
    std::string current = mpCurrentDirectory();
    if (current.empty()) return osError(a, "cannot determine current directory");
    return a.mkstr(current);
}

// fn Chdir
// fn chdir
// fn Chdir
static value fnChdir(cffi& a){
    if (!mpSetCurrentDirectory(a.arg_str(0))){
        return osError(a, "cannot change directory to '" + a.arg_str(0) + "'");
    }
    return a.none();
}

// fn Listdir
// fn listdir
// fn Listdir
static value fnListdir(cffi& a){
    std::string dir = (a.argc() > 0) ? a.arg_str(0) : mpCurrentDirectory();
    if (dir.empty()) return osError(a, "cannot determine current directory");
    mpFileInfo info;
    if (!mpPathInfo(dir, info) || !info.isDirectory){
        return osError(a, "No such file or directory: '" + dir + "'");
    }
    std::vector<value> items;
    for (const auto& entry : mpListDirectory(dir)){
        items.push_back(a.mkstr(entry.name));
    }
    return a.mklist(items);
}

// fn Mkdir
// fn mkdir
// fn Mkdir
static value fnMkdir(cffi& a){
    std::string path = a.arg_str(0);
    if (mpCreateDirectory(path) < 0){
        return osError(a, "cannot create directory '" + path + "'");
    }
    return a.none();
}

// fn Makedirs
// fn makedirs
// fn Makedirs
static value fnMakedirs(cffi& a){
    std::string path = a.arg_str(0);
    if (makeDirectoryLevels(path) < 0){
        return osError(a, "cannot create directory '" + path + "'");
    }
    return a.none();
}

// fn Rmdir
// fn rmdir
// fn Rmdir
static value fnRmdir(cffi& a){
    std::string path = a.arg_str(0);
    if (!mpRemoveDirectory(path)){
        return osError(a, "cannot remove directory '" + path + "'");
    }
    return a.none();
}

// fn 移除
// fn remove
// fn ᠬᠠᠯᠠᠬᠤ
static value fnRemove(cffi& a){
    std::string path = a.arg_str(0);
    mpFileInfo info;
    if (mpPathInfo(path, info) && info.isDirectory && !info.isSymlink){
        if (!mpRemoveDirectory(path)){
            return osError(a, "cannot remove directory '" + path + "'");
        }
        return a.none();
    }
    if (!mpRemoveFile(path)){
        return osError(a, "cannot remove file '" + path + "'");
    }
    return a.none();
}

// fn Rename
// fn rename
// fn Rename
static value fnRename(cffi& a){
    std::string from = a.arg_str(0);
    std::string to = a.arg_str(1);
    if (!mpMoveFile(from, to)){
        return osError(a, "cannot rename '" + from + "' to '" + to + "'");
    }
    return a.none();
}

// fn Getenv
// fn getenv
// fn Getenv
static value fnGetenv(cffi& a){
    const char* value = std::getenv(a.arg_str(0).c_str());
    if (value == nullptr){
        if (a.argc() >= 2) return a.arg(1);
        return a.none();
    }
    return a.mkstr(value);
}

// fn Putenv
// fn putenv
// fn Putenv
static value fnPutenv(cffi& a){
    std::string text = a.arg_str(0) + "=" + a.arg_str(1);
#if defined(_WIN32)
    _putenv(text.c_str());
#else
    setenv(a.arg_str(0).c_str(), a.arg_str(1).c_str(), 1);
#endif
    return a.none();
}

// fn System
// fn system
// fn System
static value fnSystem(cffi& a){
#if defined(__wasi__)
    // WASI 无子进程能力, 始终返回失败
    // WASI has no subprocess support, always report failure
    // WASI ᠳᠤ ᠬᠤᠪᠢᠰᠤᠨ ᠠᠵᠢᠯᠤᠨ ᠦ ᠴᠠᠳᠠᠯ ᠦᠭᠡᠢ
    (void)a.arg_str(0);
    return a.mkint(-1);
#else
    int result = std::system(a.arg_str(0).c_str());
    return a.mkint(result);
#endif
}

// fn Cpu Count
// fn cpu count
// fn Cpu Count
static value fnCpuCount(cffi& a){
    long long count = static_cast<long long>(std::thread::hardware_concurrency());
    return a.mkint(count > 0 ? count : 1);
}

// fn Walk
// fn walk
// fn Walk
static value fnWalk(cffi& a){
#if defined(_WIN32)
    std::string top = a.arg_str(0);
    std::wstring wide = utf8ToWide(top);
    DWORD attrs = GetFileAttributesW(wide.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0){
        return fileNotFoundError(a, "No such file or directory: '" + top + "'");
    }
    std::vector<value> items;
    walkInto(a, wide, items);
    return a.mklist(items);
#else
    std::string top = a.arg_str(0);
    mpFileInfo info;
    if (!mpPathInfo(top, info) || !info.isDirectory){
        return fileNotFoundError(a, "No such file or directory: '" + top + "'");
    }
    std::vector<value> items;
    std::vector<std::string> pending;
    pending.push_back(top);
    while (!pending.empty()){
        std::string dirPath = pending.back();
        pending.pop_back();
        std::vector<value> dirNames;
        std::vector<value> fileNames;
        std::vector<std::string> subDirs;
        for (const auto& entry : mpListDirectory(dirPath)){
            if (entry.isDirectory){
                dirNames.push_back(a.mkstr(entry.name));
                if (!entry.isSymlink){
                    std::string child = dirPath;
                    if (!child.empty() && child.back() != '/' && child.back() != '\\'){
                        child.push_back(mpPathSeparator());
                    }
                    child += entry.name;
                    subDirs.push_back(child);
                }
            } else {
                fileNames.push_back(a.mkstr(entry.name));
            }
        }
        items.push_back(a.vm->makeTuple(
            {a.mkstr(dirPath), a.mklist(dirNames), a.mklist(fileNames)}));
        for (const auto& sub : subDirs) pending.push_back(sub);
    }
    return a.mklist(items);
#endif
}

// fn Getpid
// fn getpid
// fn Getpid
static value fnGetpid(cffi& a){
#if defined(_WIN32)
    return a.mkint(static_cast<long long>(GetCurrentProcessId()));
#elif defined(__wasi__)
    // WASI 无进程标识, 固定返回 0
    // WASI has no process id, always report 0
    // WASI ᠳᠤ ᠭᠠᠷᠠᠭ ᠤᠨ ᠳᠤᠭ᠎ᠠ ᠦᠭᠡᠢ
    return a.mkint(0);
#else
    return a.mkint(static_cast<long long>(::getpid()));
#endif
}

// fn Urandom
// fn urandom
// fn Urandom
static value fnUrandom(cffi& a){
    long long count = a.arg_int(0);
    if (count < 0){
        a.vm->raiseError("ValueError", "negative argument not allowed");
        return a.none();
    }


    static const char* digits = "0123456789abcdef";
    std::string result;
    result.reserve(static_cast<size_t>(count) * 2);
    for (long long i = 0; i < count; ++i){
        unsigned int value = static_cast<unsigned int>(std::rand()) & 0xFF;
        result.push_back(digits[value >> 4]);
        result.push_back(digits[value & 0xF]);
    }
    return a.mkstr(result);
}

// fn Access
// fn access
// fn Access
static value fnAccess(cffi& a){
    std::string path = a.arg_str(0);
    long long mode = a.arg_int(1);
    if (mode < 0){
        a.vm->raiseError("ValueError", "invalid mode");
        return a.none();
    }
#if defined(_WIN32)
    std::wstring wide = utf8ToWide(path);
    DWORD attrs = GetFileAttributesW(wide.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return a.mkbool(false);
    if (mode == 0) return a.mkbool(true);

    int crtMode = static_cast<int>(mode & 6);
    if (crtMode == 0) return a.mkbool(true);
    return a.mkbool(_waccess(wide.c_str(), crtMode) == 0);
#elif defined(__wasi__)
    // WASI 无权限位概念, 按路径是否存在判断
    // WASI has no permission bits, report based on existence
    // WASI ᠳᠤ ᠡᠷᠬᠡ ᠦᠢᠯᠡᠳᠦᠯ ᠦᠭᠡᠢ
    mpFileInfo info;
    return a.mkbool(mpPathInfo(path, info) && info.targetExists);
#else
    return a.mkbool(::access(path.c_str(), static_cast<int>(mode)) == 0);
#endif
}

// fn Stat
// fn stat
// fn Stat
static value fnStat(cffi& a){
#if defined(_WIN32)
    std::string path = a.arg_str(0);
    std::wstring wide = utf8ToWide(path);
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &info)){
        return fileNotFoundError(a, "No such file or directory: '" + path + "'");
    }
    long long size = (static_cast<long long>(info.nFileSizeHigh) << 32) |
                     static_cast<long long>(info.nFileSizeLow);
    bool directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    long long mode = directory ? 0040777 : 0100666;
    if (!directory && (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0){
        mode = 0100444;
    }
    return a.mkdict({
        {a.mkstr("st_mode"), a.mkint(mode)},
        {a.mkstr("st_size"), a.mkint(size)},
        {a.mkstr("st_atime"), a.mkint(fileTimeSeconds(info.ftLastAccessTime))},
        {a.mkstr("st_mtime"), a.mkint(fileTimeSeconds(info.ftLastWriteTime))},
        {a.mkstr("st_ctime"), a.mkint(fileTimeSeconds(info.ftCreationTime))}
    });
#else
    std::string path = a.arg_str(0);

// stat
// stat
// stat
    struct stat info;
    if (::stat(path.c_str(), &info) != 0){
        return fileNotFoundError(a, "No such file or directory: '" + path + "'");
    }
    return a.mkdict({
        {a.mkstr("st_mode"), a.mkint(static_cast<long long>(info.st_mode))},
        {a.mkstr("st_size"), a.mkint(static_cast<long long>(info.st_size))},
        {a.mkstr("st_atime"), a.mkint(static_cast<long long>(info.st_atime))},
        {a.mkstr("st_mtime"), a.mkint(static_cast<long long>(info.st_mtime))},
        {a.mkstr("st_ctime"), a.mkint(static_cast<long long>(info.st_ctime))}
    });
#endif
}

// data F Access
// data f access
// data F Access
static value dataFAccess(cffi& a){ return a.mkint(0); }

// data R Access
// data r access
// data R Access
static value dataRAccess(cffi& a){ return a.mkint(4); }

// data W Access
// data w access
// data W Access
static value dataWAccess(cffi& a){ return a.mkint(2); }

// data X Access
// data x access
// data X Access
static value dataXAccess(cffi& a){ return a.mkint(1); }

// data 标识符
// data name
// data ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
static value dataName(cffi& a){
#if defined(_WIN32)
    return a.mkstr("nt");
#else
    return a.mkstr("posix");
#endif
}

// data Sep
// data sep
// data Sep
static value dataSep(cffi& a){
#if defined(_WIN32)
    return a.mkstr("\\");
#else
    return a.mkstr("/");
#endif
}

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

// data Linesep
// data linesep
// data Linesep
static value dataLinesep(cffi& a){
#if defined(_WIN32)
    return a.mkstr("\r\n");
#else
    return a.mkstr("\n");
#endif
}

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

// cffi os lib
// cffi os lib
// cffi os lib
ffilib cffi_os_lib(){
    return cffi_lib(
        "os",
        {
            {"getcwd", (void*)fnGetcwd},
            {"chdir", (void*)fnChdir},
            {"listdir", (void*)fnListdir},
            {"mkdir", (void*)fnMkdir},
            {"makedirs", (void*)fnMakedirs},
            {"rmdir", (void*)fnRmdir},
            {"remove", (void*)fnRemove},
            {"unlink", (void*)fnRemove},
            {"rename", (void*)fnRename},
            {"replace", (void*)fnRename},
            {"walk", (void*)fnWalk},
            {"getpid", (void*)fnGetpid},
            {"close", (void*)fnClose},
            {"urandom", (void*)fnUrandom},
            {"access", (void*)fnAccess},
            {"stat", (void*)fnStat},
            {"getenv", (void*)fnGetenv},
            {"putenv", (void*)fnPutenv},
            {"system", (void*)fnSystem},
            {"cpu_count", (void*)fnCpuCount}
        },
        {
            {"F_OK", (void*)dataFAccess},
            {"R_OK", (void*)dataRAccess},
            {"W_OK", (void*)dataWAccess},
            {"X_OK", (void*)dataXAccess},
            {"name", (void*)dataName},
            {"sep", (void*)dataSep},
            {"altsep", (void*)dataAltsep},
            {"linesep", (void*)dataLinesep},
            {"curdir", (void*)dataCurdir},
            {"pardir", (void*)dataPardir},
            {"devnull", (void*)dataDevnull}
        });
}


}
