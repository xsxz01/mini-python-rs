#include "../cffi.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace minipy {




using namespace minipy;
namespace fs = std::filesystem;

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

// wide 转 Utf8
// wide to utf8
// wide ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Utf8
std::string wideToUtf8(const std::wstring& text){
    if (text.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        result.data(), len, nullptr, nullptr);
    return result;
}

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
}

// fn Getcwd
// fn getcwd
// fn Getcwd
static value fnGetcwd(cffi& a){
    std::error_code ec;
    fs::path current = fs::current_path(ec);
    if (ec) return osError(a, ec.message());
    return a.mkstr(current.string());
}

// fn Chdir
// fn chdir
// fn Chdir
static value fnChdir(cffi& a){
    std::error_code ec;
    fs::current_path(fs::path(a.arg_str(0)), ec);
    if (ec) return osError(a, ec.message());
    return a.none();
}

// fn Listdir
// fn listdir
// fn Listdir
static value fnListdir(cffi& a){
    std::error_code ec;
    fs::path dir;
    if (a.argc() > 0){
        dir = fs::path(a.arg_str(0));
    } else {
        dir = fs::current_path(ec);
        if (ec) return osError(a, ec.message());
    }
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)){
        return osError(a, "No such file or directory: '" + dir.string() + "'");
    }
    std::vector<value> items;
    for (const auto& entry : fs::directory_iterator(dir, ec)){
        items.push_back(a.mkstr(entry.path().filename().string()));
    }
    if (ec) return osError(a, ec.message());
    return a.mklist(items);
}

// fn Mkdir
// fn mkdir
// fn Mkdir
static value fnMkdir(cffi& a){
    std::error_code ec;
    fs::create_directory(fs::path(a.arg_str(0)), ec);
    if (ec) return osError(a, ec.message());
    return a.none();
}

// fn Makedirs
// fn makedirs
// fn Makedirs
static value fnMakedirs(cffi& a){
    std::error_code ec;
    fs::create_directories(fs::path(a.arg_str(0)), ec);
    if (ec) return osError(a, ec.message());
    return a.none();
}

// fn Rmdir
// fn rmdir
// fn Rmdir
static value fnRmdir(cffi& a){
    std::error_code ec;
    fs::remove(fs::path(a.arg_str(0)), ec);
    if (ec) return osError(a, ec.message());
    return a.none();
}

// fn 移除
// fn remove
// fn ᠬᠠᠯᠠᠬᠤ
static value fnRemove(cffi& a){
    std::error_code ec;
    fs::remove(fs::path(a.arg_str(0)), ec);
    if (ec) return osError(a, ec.message());
    return a.none();
}

// fn Rename
// fn rename
// fn Rename
static value fnRename(cffi& a){
    std::error_code ec;
    fs::rename(fs::path(a.arg_str(0)), fs::path(a.arg_str(1)), ec);
    if (ec) return osError(a, ec.message());
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
    int result = std::system(a.arg_str(0).c_str());
    return a.mkint(result);
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
    std::error_code ec;
    fs::path top(a.arg_str(0));
    if (!fs::is_directory(top, ec)){
        return fileNotFoundError(a, "No such file or directory: '" + a.arg_str(0) + "'");
    }
    std::vector<value> items;
    std::vector<fs::path> pending;
    pending.push_back(top);
    while (!pending.empty()){
        fs::path dirPath = pending.back();
        pending.pop_back();
        std::vector<value> dirNames;
        std::vector<value> fileNames;
        std::vector<fs::path> subDirs;
        fs::directory_iterator it(dirPath, ec);
        if (ec) continue;
        for (; it != fs::directory_iterator(); it.increment(ec)){
            if (ec) break;
            fs::path entry = it->path();
            std::string name = entry.filename().string();
            std::error_code typeEc;
            if (fs::is_directory(entry, typeEc)){
                dirNames.push_back(a.mkstr(name));
                if (!fs::is_symlink(entry, typeEc)) subDirs.push_back(entry);
            } else {
                fileNames.push_back(a.mkstr(name));
            }
        }
        items.push_back(a.vm->makeTuple(
            {a.mkstr(dirPath.string()), a.mklist(dirNames), a.mklist(fileNames)}));
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
