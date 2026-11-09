#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "platform.hpp"

#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(MINIPY_OS_POSIX)
#include <cerrno>
#include <sys/stat.h>
#elif defined(MINIPY_OS_WASM)
#include <cerrno>
#include <sys/stat.h>
#endif

#if defined(MINIPY_OS_POSIX) || defined(MINIPY_OS_EMSCRIPTEN)
#include <dirent.h>
#endif

#if defined(MINIPY_OS_MACOS) || defined(MINIPY_OS_IOS)
#include <mach-o/dyld.h>
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace{

#if defined(MINIPY_OS_ANDROID) || defined(MINIPY_OS_HARMONYOS)
// 定位锚点, dladdr 通过它取所在共享库的路径
// Anchor used by dladdr to locate the shared library that contains this code
// dladdr ᠤᠨ ᠲᠡᠮᠳᠡᠭ ᠪᠣᠯᠭ᠎ᠠ
void executableAnchor(){}
#endif

// 把一个 Unicode 码位写成 UTF-8 字节
// Append one Unicode code point as UTF-8 bytes
// ᠨᠢᠭᠡ ᠬᠣᠳᠦᠯᠭ᠎ᠠ ᠢ UTF-8 ᠪᠡᠷ ᠪᠢᠴᠢᠨ᠎ᠡ
void appendUtf8(std::string& out, unsigned long code){
    if (code < 0x80){
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800){
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000){
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

// 判断字符是否为路径分隔符
// Test whether a character is a path separator
// ᠵᠠᠮ ᠤᠨ ᠰᠠᠯᠭᠠᠭᠴᠢ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool isSeparatorChar(char c){
    return c == '/' || c == '\\';
}

#if defined(MINIPY_OS_WINDOWS)
// Windows 词法归一化: 折叠分隔符, 处理盘符/UNC 根与 ".."
// Windows lexical normalization: collapse separators, handle drive/UNC roots and ".."
// Windows ᠤᠨ ᠵᠠᠮ ᠢ ᠰᠢᠳᠤᠷᠭᠦᠯᠬᠦ
std::string normalizeWindowsPath(const std::string& input){
    if (input.empty()) return ".";
    std::string path = input;
    for (char& c : path) if (c == '/') c = '\\';

    std::string root;
    size_t i = 0;
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\'){
        size_t third = path.find('\\', 2);
        if (third == std::string::npos) return path;
        size_t fourth = path.find('\\', third + 1);
        if (fourth == std::string::npos) return path;
        root = path.substr(0, fourth);
        i = fourth;
    } else if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':'){
        root = path.substr(0, 2);
        if (path.size() >= 3 && path[2] == '\\'){
            root.push_back('\\');
            i = 3;
        } else {
            i = 2;
        }
    } else if (!path.empty() && path[0] == '\\'){
        root = "\\";
        i = 1;
    }

    std::vector<std::string> segments;
    size_t pos = i;
    while (pos < path.size()){
        size_t end = path.find('\\', pos);
        if (end == std::string::npos) end = path.size();
        std::string segment = path.substr(pos, end - pos);
        pos = end + 1;
        if (segment.empty() || segment == ".") continue;
        if (segment == ".."){
            if (!segments.empty() && segments.back() != "..") segments.pop_back();
            else if (root.empty()) segments.push_back("..");
        } else {
            segments.push_back(segment);
        }
    }

    std::string result = root;
    for (const auto& segment : segments){
        if (!result.empty() && result.back() != '\\' && result.back() != ':'){
            result.push_back('\\');
        }
        result += segment;
    }
    if (result.empty()) return ".";
    return result;
}
#else
// POSIX 词法归一化: 保留恰好两个前导斜杠, 处理根与 ".."
// POSIX lexical normalization: keep exactly two leading slashes, handle root and ".."
// POSIX ᠤᠨ ᠵᠠᠮ ᠢ ᠰᠢᠳᠤᠷᠭᠦᠯᠬᠦ
std::string normalizePosixPath(const std::string& input){
    if (input.empty()) return ".";
    size_t slash = 0;
    while (slash < input.size() && input[slash] == '/') ++slash;
    std::string prefix;
    if (slash == 1) prefix = "/";
    else if (slash == 2) prefix = "//";

    std::vector<std::string> segments;
    size_t pos = slash;
    while (pos < input.size()){
        size_t end = input.find('/', pos);
        if (end == std::string::npos) end = input.size();
        std::string segment = input.substr(pos, end - pos);
        pos = end + 1;
        if (segment.empty() || segment == ".") continue;
        if (segment == ".."){
            if (!segments.empty() && segments.back() != "..") segments.pop_back();
            else if (prefix.empty()) segments.push_back("..");
        } else {
            segments.push_back(segment);
        }
    }

    std::string result = prefix;
    for (size_t k = 0; k < segments.size(); ++k){
        if (k > 0) result.push_back('/');
        result += segments[k];
    }
    if (result.empty()) return ".";
    return result;
}
#endif

// 把路径拆成根标记与各段名字
// Split a path into a root marker and name segments
// ᠵᠠᠮ ᠢ ᠦᠰᠦᠭ ᠪᠣᠯᠭ᠎ᠠᠨ ᠰᠠᠯᠭᠠᠬᠤ
std::vector<std::string> splitPathTokens(const std::string& path){
    std::vector<std::string> tokens;
    size_t i = 0;
    std::string root;
#if defined(MINIPY_OS_WINDOWS)
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
        if (i < path.size() && isSeparatorChar(path[i])){
            root.push_back(path[i]);
            ++i;
        }
    } else if (!path.empty() && isSeparatorChar(path[0])){
        root = path.substr(0, 1);
        i = 1;
    }
#else
    if (!path.empty() && path[0] == '/'){
        root = "/";
    }
#endif
    if (!root.empty()) tokens.push_back(root);
    while (i < path.size()){
        size_t end = i;
        while (end < path.size() && !isSeparatorChar(path[end])) ++end;
        if (end > i) tokens.push_back(path.substr(i, end - i));
        i = end + 1;
    }
    return tokens;
}

// 把根标记与段名字重新拼成路径
// Join a root marker and name segments back into a path
// ᠨᠢᠭᠡᠳᠦᠭᠰᠡᠨ ᠵᠠᠮ ᠪᠣᠯᠭ᠎ᠠᠬᠤ
std::string joinPathTokens(const std::vector<std::string>& tokens){
    std::string result;
    for (size_t i = 0; i < tokens.size(); ++i){
        const std::string& token = tokens[i];
        bool root = (token == "/" || token == "\\") ||
                    (token.size() >= 2 && token[1] == ':') ||
                    (token.size() >= 2 && token[0] == '\\' && token[1] == '\\');
        if (result.empty()){
            result = token;
            continue;
        }
        if (root){
            result = token;
            continue;
        }
        if (result.back() != '/' && result.back() != '\\' && result.back() != ':'){
            result.push_back(minipy::mpPathSeparator());
        }
        result += token;
    }
    return result;
}

}

namespace minipy{

// UTF-8 字符串转宽字符
// Convert UTF-8 to wide string
// UTF-8 ᠢ ᠦ᠋ᠰᠲᠡ ᠴᠠᠷᠠᠭ ᠲᠤ ᠪᠡᠷᠢᠷᠡᠩ᠎ᠡ
std::wstring utf8ToWide(const std::string& text){
    std::wstring result;
    size_t i = 0;
    while (i < text.size()){
        unsigned char c = static_cast<unsigned char>(text[i]);
        long long code = 0;
        size_t extra = 0;
        if (c < 0x80){ code = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0){ code = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0){ code = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0){ code = c & 0x07; extra = 3; }
        else { i++; continue; }
        bool valid = (i + extra < text.size());
        for (size_t k = 1; valid && k <= extra; ++k){
            unsigned char next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0) != 0x80) valid = false;
            else code = (code << 6) | (next & 0x3F);
        }
        i += extra + 1;
        if (!valid) continue;

// 2 字节 wchar_t 的平台需要把辅助平面拆成代理对
// Platforms with 2-byte wchar_t need surrogate pairs for supplementary planes
// ᠬᠣᠶᠠᠷ ᠪᠠᠶᠢᠲᠤᠲᠠᠢ wchar_t ᠲᠠᠢ ᠲᠠᠯᠠᠪᠠᠢ ᠳᠤ ᠰᠤᠷᠤᠭᠠᠲ᠋ᠠᠢ ᠬᠣᠰ
#if WCHAR_MAX <= 0xFFFF
        if (code >= 0x10000){
            long long offset = code - 0x10000;
            result.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
            result.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));
        } else if (code > 0){
            result.push_back(static_cast<wchar_t>(code));
        }
#else
        if (code > 0){
            result.push_back(static_cast<wchar_t>(code));
        }
#endif
    }
    return result;
}

// 宽字符转 UTF-8 字符串
// Convert wide string to UTF-8
// ᠦ᠋ᠰᠲᠡ ᠴᠠᠷᠠᠭ ᠢ UTF-8 ᠪᠣᠯᠭ᠎ᠠᠨ ᠬᠤᠪᠢᠶ᠎ᠠᠨ᠎ᠠ
std::string wideToUtf8(const std::wstring& text){
    std::string result;
    result.reserve(text.size() * 2);
    size_t i = 0;
    while (i < text.size()){
        unsigned long code = static_cast<unsigned long>(text[i]);
#if WCHAR_MAX <= 0xFFFF
        if (code >= 0xD800 && code <= 0xDBFF && i + 1 < text.size()){
            unsigned long low = static_cast<unsigned long>(text[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF){
                code = 0x10000UL + ((code - 0xD800UL) << 10) + (low - 0xDC00UL);
                i++;
            }
        }
#endif
        appendUtf8(result, code);
        i++;
    }
    return result;
}

// 加载共享库, 失败返回空句柄
// Load a shared library, returns a null handle on failure
// ᠰᠠᠩ ᠢ ᠲᠠᠲᠠᠬᠤ
mp_module mpLoadLibrary(const std::string& path){
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    return LoadLibraryW(wide.c_str());
#elif defined(MINIPY_OS_EMSCRIPTEN)
    return static_cast<mp_module>(dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL));
#elif defined(MINIPY_OS_WASM)
    (void)path;
    return nullptr;
#else
    return static_cast<mp_module>(dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL));
#endif
}

// 按名称取导出符号, 失败返回空指针
// Resolve an exported symbol by name, returns null on failure
// ᠨᠡᠷᠡ ᠪᠡᠷ ᠭᠠᠷᠭᠠᠬᠤ ᠲᠡᠮᠳᠡᠭ
void* mpGetProcAddress(mp_module module, const char* name){
    if (!module || !name) return nullptr;
#if defined(MINIPY_OS_WINDOWS)
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#elif defined(MINIPY_OS_EMSCRIPTEN)
    return dlsym(module, name);
#elif defined(MINIPY_OS_WASM)
    return nullptr;
#else
    return dlsym(module, name);
#endif
}

// 按序号取导出符号, 仅 Windows 支持
// Resolve an exported symbol by ordinal, Windows only
// ᠳᠤᠭ᠎ᠠ ᠪᠡᠷ ᠭᠠᠷᠭᠠᠬᠤ ᠲᠡᠮᠳᠡᠭ Windows ᠳᠤ ᠵᠥᠪᠬᠡᠨ
void* mpGetProcAddressOrdinal(mp_module module, unsigned int ordinal){
#if defined(MINIPY_OS_WINDOWS)
    if (!module) return nullptr;
    return reinterpret_cast<void*>(
        GetProcAddress(module, MAKEINTRESOURCEA(static_cast<WORD>(ordinal))));
#else
    (void)module;
    (void)ordinal;
    return nullptr;
#endif
}

// 释放共享库句柄
// Release a shared library handle
// ᠰᠠᠩ ᠤᠨ ᠪᠠᠷᠢᠶ᠎ᠠ ᠢ ᠴᠡᠯᠡᠭᠡᠨ᠎ᠡ
void mpUnloadLibrary(mp_module module){
    if (!module) return;
#if defined(MINIPY_OS_WINDOWS)
    FreeLibrary(module);
#elif defined(MINIPY_OS_EMSCRIPTEN)
    dlclose(module);
#elif defined(MINIPY_OS_WASM)
    (void)module;
#else
    dlclose(module);
#endif
}

// 最近一次系统错误码
// Last system error code
// ᠰᠡᠦᠯᠴᠢ ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠳᠤᠭ᠎ᠠ
unsigned long mpLastError(){
#if defined(MINIPY_OS_WINDOWS)
    return static_cast<unsigned long>(GetLastError());
#else
    return static_cast<unsigned long>(errno);
#endif
}

// 设置系统错误码
// Store a system error code
// ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠳᠤᠭ᠎ᠠ ᠢ ᠲᠣᠭᠲᠠᠭ᠎ᠠᠬᠤ
void mpSetLastError(unsigned long code){
#if defined(MINIPY_OS_WINDOWS)
    SetLastError(static_cast<DWORD>(code));
#else
    errno = static_cast<int>(code);
#endif
}

// 最近一次共享库加载失败的原因
// Reason of the last failed shared library load
// ᠰᠡᠦᠯᠴᠢ ᠠᠯᠳᠠᠭ᠎ᠠ ᠦᠨ ᠰᠢᠯᠲᠠᠭ᠎ᠠᠨ
std::string mpLibraryError(){
#if defined(MINIPY_OS_WINDOWS)
    return "";
#elif defined(MINIPY_OS_EMSCRIPTEN)
    const char* message = dlerror();
    return message ? std::string(message) : std::string();
#elif defined(MINIPY_OS_WASM)
    return "";
#else
    const char* message = dlerror();
    return message ? std::string(message) : std::string();
#endif
}

// 申请可执行内存, 失败返回空指针
// Allocate executable memory, returns null on failure
// ᠭᠦᠢᠴᠡᠳᠬᠡᠬᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠭᠤᠶᠤᠴᠠᠬᠤ
void* mpAllocateExecutable(size_t size){
#if defined(MINIPY_OS_WINDOWS)
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#elif defined(MINIPY_OS_WASM)
    (void)size;
    return nullptr;
#else
#ifdef MAP_JIT
    // Apple 平台 (macOS/iOS) 的可执行页必须带 MAP_JIT, 否则 mmap 会失败
    // Apple platforms (macOS/iOS) require MAP_JIT for executable pages, plain mmap fails
    // Apple ᠲᠠᠯᠠᠪᠠᠢ ᠳᠤ MAP_JIT ᠬᠡᠷᠡᠭᠲᠡᠢ
    void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
#else
    void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    return (memory == MAP_FAILED) ? nullptr : memory;
#endif
}

// 刷新指令缓存, 让新写入的机器码立刻可见
// Flush the instruction cache so freshly written machine code is visible
// ᠵᠢᠴᠡᠢ ᠤᠨ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠰᠡᠷᠢᠭᠡᠭᠦᠯᠬᠦ
void mpFlushInstructionCache(void* code, size_t size){
    if (!code) return;
#if defined(MINIPY_OS_WINDOWS)
    FlushInstructionCache(GetCurrentProcess(), code, size);
#else
    __builtin___clear_cache(static_cast<char*>(code), static_cast<char*>(code) + size);
#endif
}

// 归还可执行内存
// Release executable memory
// ᠭᠦᠢᠴᠡᠳᠬᠡᠬᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠡᠷᠭᠢᠬᠦ
void mpFreeExecutable(void* code, size_t size){
    if (!code) return;
#if defined(MINIPY_OS_WINDOWS)
    (void)size;
    VirtualFree(code, 0, MEM_RELEASE);
#elif defined(MINIPY_OS_WASM)
    (void)code;
    (void)size;
#else
    munmap(code, size);
#endif
}

// 当前可执行文件的绝对路径
// Absolute path of the running executable
// ᠠᠭᠰᠠᠬᠤ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠦᠨ ᠵᠠᠮ
std::string mpExecutablePath(){
#if defined(MINIPY_OS_WINDOWS)
    char buffer[MAX_PATH] = {0};
    if (GetModuleFileNameA(nullptr, buffer, MAX_PATH) == 0) return "";
    return std::string(buffer);
#elif defined(MINIPY_OS_WASM)
    return "";
#elif defined(MINIPY_OS_ANDROID) || defined(MINIPY_OS_HARMONYOS)

    // 移动端优先用 dladdr 取所在共享库路径, 失败再退回 /proc/self/exe
    // On mobile prefer dladdr for the containing shared object, falling back to /proc/self/exe
    // ᠭᠠᠷ ᠤᠨ ᠲᠠᠯᠠᠪᠠᠢ ᠳᠤ dladdr ᠢ ᠲᠦᠷᠦᠩᠬᠢᠯᠡᠨ ᠠᠰᠠᠭᠯᠠᠨ᠎ᠠ
    Dl_info addressInfo;
    if (dladdr(reinterpret_cast<const void*>(&executableAnchor), &addressInfo) != 0 &&
        addressInfo.dli_fname != nullptr && addressInfo.dli_fname[0] != '\0'){
        return std::string(addressInfo.dli_fname);
    }
    char buffer[4096] = {0};
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return "";
    return std::string(buffer, static_cast<size_t>(length));
#elif defined(MINIPY_OS_IOS) || defined(MINIPY_OS_MACOS)
    char buffer[4096] = {0};
    uint32_t size = static_cast<uint32_t>(sizeof(buffer));
    if (_NSGetExecutablePath(buffer, &size) != 0) return "";
    return std::string(buffer);
#else
    char buffer[4096] = {0};
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return "";
    return std::string(buffer, static_cast<size_t>(length));
#endif
}

// 当前可执行文件所在目录
// Directory of the running executable
// ᠠᠭᠰᠠᠬᠤ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠦᠨ ᠭᠠᠵᠠᠷ
std::string mpExecutableDirectory(){
    std::string path = mpExecutablePath();
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return "";
    return path.substr(0, slash);
}

// 当前平台路径分隔符
// Path separator of the current platform
// ᠵᠠᠮ ᠤᠨ ᠰᠠᠯᠭᠠᠭᠴᠢ
char mpPathSeparator(){
#if defined(MINIPY_OS_WINDOWS)
    return '\\';
#else
    return '/';
#endif
}

// 共享库名称的候选列表
// Candidate shared library names
// ᠬᠤᠪᠢᠶ᠎ᠠᠯᠴᠠᠬᠤ ᠰᠠᠩ ᠤᠨ ᠨᠡᠷᠡ ᠦᠨ ᠵᠢᠭᠰᠠᠭᠠᠯ
std::vector<std::string> mpSharedLibraryCandidates(const std::string& name){
    std::vector<std::string> candidates;
    if (name.find('.') != std::string::npos){
        candidates.push_back(name);
        return candidates;
    }
#if defined(MINIPY_OS_WINDOWS)
    candidates.push_back(name + ".dll");
#elif defined(MINIPY_OS_MACOS) || defined(MINIPY_OS_IOS)
    candidates.push_back(name + ".dylib");
    candidates.push_back(name + ".so");
#else
    candidates.push_back(name + ".so");
#endif
    return candidates;
}

// 读取路径信息
// Read path information
// ᠵᠠᠮ ᠤᠨ ᠮᠡᠳᠡᠭᠡᠯᠡᠯ ᠢ ᠤᠨᠰᠢᠬᠤ
bool mpPathInfo(const std::string& path, mpFileInfo& info){
    info = mpFileInfo();
    if (path.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    DWORD attributes = GetFileAttributesW(wide.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    info.exists = true;
    info.targetExists = true;
    info.isSymlink = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &data)){
        info.isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.isRegularFile = !info.isDirectory;
        info.size = (static_cast<long long>(data.nFileSizeHigh) << 32) |
                    static_cast<long long>(data.nFileSizeLow);
    } else {
        info.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.isRegularFile = !info.isDirectory;
    }
    return true;
#else
#if defined(MINIPY_OS_WASM)
    struct stat targetInfo;
    if (::stat(path.c_str(), &targetInfo) != 0) return false;
    info.exists = true;
    info.targetExists = true;
    info.isSymlink = false;
    info.isDirectory = S_ISDIR(targetInfo.st_mode) != 0;
    info.isRegularFile = S_ISREG(targetInfo.st_mode) != 0;
    info.size = static_cast<long long>(targetInfo.st_size);
    return true;
#else
    struct stat linkInfo;
    if (::lstat(path.c_str(), &linkInfo) != 0) return false;
    info.exists = true;
    info.isSymlink = S_ISLNK(linkInfo.st_mode) != 0;
    struct stat targetInfo;
    if (::stat(path.c_str(), &targetInfo) != 0){
        info.targetExists = false;
        return true;
    }
    info.targetExists = true;
    info.isDirectory = S_ISDIR(targetInfo.st_mode) != 0;
    info.isRegularFile = S_ISREG(targetInfo.st_mode) != 0;
    info.size = static_cast<long long>(targetInfo.st_size);
    return true;
#endif
#endif
}

// 当前工作目录
// Current working directory
// ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ
std::string mpCurrentDirectory(){
#if defined(MINIPY_OS_WINDOWS)
    DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0) return "";
    std::vector<wchar_t> buffer(needed);
    DWORD length = GetCurrentDirectoryW(needed, buffer.data());
    if (length == 0 || length >= needed) return "";
    return wideToUtf8(std::wstring(buffer.data(), length));
#else
#if defined(__wasi__)
    // WASI 无工作目录概念, 统一返回根目录
    // WASI has no working directory, report the root
    // WASI ᠳᠤ ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ ᠦᠭᠡᠢ
    return "/";
#else
    char buffer[4096];
    if (::getcwd(buffer, sizeof(buffer)) == nullptr) return "";
    return std::string(buffer);
#endif
#endif
}

// 切换工作目录
// Change the working directory
// ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ ᠢ ᠰᠣᠯᠢᠬᠤ
bool mpSetCurrentDirectory(const std::string& path){
    if (path.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    return SetCurrentDirectoryW(wide.c_str()) != 0;
#elif defined(__wasi__)
    (void)path;
    return false;
#else
    return ::chdir(path.c_str()) == 0;
#endif
}

// 判断是否为绝对路径
// Test whether a path is absolute
// ᠪᠦᠷᠬᠡ ᠨᠡᠷᠢᠴᠢᠯ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool mpIsAbsolutePath(const std::string& path){
    if (path.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
        isSeparatorChar(path[2])) return true;
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') return true;
    return false;
#else
    return path[0] == '/';
#endif
}

// 转为绝对路径
// Make a path absolute
// ᠨᠡᠷᠢᠴᠢᠯ ᠵᠠᠮ ᠪᠣᠯᠭ᠎ᠠᠬᠤ
std::string mpAbsolutePath(const std::string& path){
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    DWORD needed = GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    if (needed == 0) return path;
    std::vector<wchar_t> buffer(needed);
    DWORD length = GetFullPathNameW(wide.c_str(), needed, buffer.data(), nullptr);
    if (length == 0 || length >= needed) return path;
    return wideToUtf8(std::wstring(buffer.data(), length));
#else
    if (mpIsAbsolutePath(path)) return path;
    std::string current = mpCurrentDirectory();
    if (current.empty()) return path;
    if (current.size() == 1 && current[0] == '/') return "/" + path;
    return current + "/" + path;
#endif
}

// 词法归一化路径
// Lexically normalize a path
// ᠵᠠᠮ ᠢ ᠰᠢᠳᠤᠷᠭᠦᠯᠬᠦ
std::string mpNormalizePath(const std::string& path){
#if defined(MINIPY_OS_WINDOWS)
    return normalizeWindowsPath(path);
#else
    return normalizePosixPath(path);
#endif
}

// 列出目录内容
// List directory contents
// ᠭᠠᠵᠠᠷ ᠤᠨ ᠠᠭᠤᠯᠤᠮᠵᠢ ᠢ ᠵᠢᠭᠰᠠᠬᠤ
std::vector<mpDirEntry> mpListDirectory(const std::string& path){
    std::vector<mpDirEntry> entries;
    std::string dir = path.empty() ? std::string(".") : path;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(dir);
    std::wstring pattern = wide;
    if (pattern.back() != L'\\' && pattern.back() != L'/'){
        pattern.push_back(L'\\');
    }
    pattern.push_back(L'*');
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return entries;
    do {
        std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") continue;
        mpDirEntry entry;
        entry.name = wideToUtf8(name);
        entry.isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.isSymlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        entries.push_back(entry);
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
#elif defined(MINIPY_OS_WASM) && !defined(MINIPY_OS_EMSCRIPTEN)
    // 无目录枚举能力的 WebAssembly 环境, 返回空列表
    // WebAssembly environments without directory enumeration, return an empty list
    // ᠭᠠᠵᠠᠷ ᠤᠨ ᠵᠢᠭᠰᠠᠯᠭ᠎ᠠ ᠦᠭᠡᠢ WebAssembly ᠲᠠᠯᠠᠪᠠᠢ
    (void)dir;
#else
    DIR* dirHandle = ::opendir(dir.c_str());
    if (!dirHandle) return entries;
    while (struct dirent* item = ::readdir(dirHandle)){
        std::string name = item->d_name;
        if (name == "." || name == "..") continue;
        mpDirEntry entry;
        entry.name = name;
        std::string full = dir;
        if (full.empty() || full.back() != '/') full.push_back('/');
        full += name;
        struct stat linkInfo;
        if (::lstat(full.c_str(), &linkInfo) == 0){
            entry.isSymlink = S_ISLNK(linkInfo.st_mode) != 0;
            if (entry.isSymlink){
                struct stat targetInfo;
                entry.isDirectory = ::stat(full.c_str(), &targetInfo) == 0 &&
                                    S_ISDIR(targetInfo.st_mode) != 0;
            } else {
                entry.isDirectory = S_ISDIR(linkInfo.st_mode) != 0;
            }
        }
        entries.push_back(entry);
    }
    ::closedir(dirHandle);
#endif
    return entries;
}

// 创建单级目录
// Create one directory level
// ᠨᠢᠭᠡ ᠵᠡᠷᠭᠡ ᠭᠠᠵᠠᠷ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int mpCreateDirectory(const std::string& path){
    if (path.empty()) return -1;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    if (CreateDirectoryW(wide.c_str(), nullptr)) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS){
        DWORD attributes = GetFileAttributesW(wide.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0){
            return 0;
        }
    }
    return -1;
#else
    if (::mkdir(path.c_str(), 0777) == 0) return 1;
    if (errno == EEXIST){
        struct stat info;
        if (::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) return 0;
    }
    return -1;
#endif
}

// 删除空目录
// Remove an empty directory
// ᠬᠣᠭᠤᠰᠤᠨ ᠭᠠᠵᠠᠷ ᠢ ᠬᠠᠯᠠᠬᠤ
bool mpRemoveDirectory(const std::string& path){
    if (path.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    return RemoveDirectoryW(wide.c_str()) != 0;
#else
    return ::rmdir(path.c_str()) == 0;
#endif
}

// 删除文件或符号链接
// Remove a file or symlink
// ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠢ ᠬᠠᠯᠠᠬᠤ
bool mpRemoveFile(const std::string& path){
    if (path.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring wide = utf8ToWide(path);
    return DeleteFileW(wide.c_str()) != 0;
#else
    return ::unlink(path.c_str()) == 0;
#endif
}

// 移动或重命名文件
// Move or rename a file
// ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠢ ᠵᠢᠭᠵᠢᠭᠦᠯᠬᠦ
bool mpMoveFile(const std::string& from, const std::string& to){
    if (from.empty() || to.empty()) return false;
#if defined(MINIPY_OS_WINDOWS)
    std::wstring fromWide = utf8ToWide(from);
    std::wstring toWide = utf8ToWide(to);
    return MoveFileExW(fromWide.c_str(), toWide.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != 0;
#else
    return ::rename(from.c_str(), to.c_str()) == 0;
#endif
}

// 临时目录
// Temporary directory
// ᠲᠡᠮᠳᠡᠭ ᠦᠨ ᠭᠠᠵᠠᠷ
std::string mpTempDirectory(){
#if defined(MINIPY_OS_WINDOWS)
    DWORD needed = GetTempPathW(0, nullptr);
    if (needed == 0) return "";
    std::vector<wchar_t> buffer(needed);
    DWORD length = GetTempPathW(needed, buffer.data());
    if (length == 0 || length >= needed) return "";
    std::wstring result(buffer.data(), length);
    while (!result.empty() && (result.back() == L'\\' || result.back() == L'/')){
        result.pop_back();
    }
    return wideToUtf8(result);
#else
    const char* environment = std::getenv("TMPDIR");
    std::string candidate;
    if (environment != nullptr && *environment != '\0'){
        candidate = environment;
    } else {
#if defined(MINIPY_OS_ANDROID)
        candidate = "/data/local/tmp";
#else
        candidate = "/tmp";
#endif
    }
    while (!candidate.empty() && candidate.back() == '/') candidate.pop_back();
    mpFileInfo info;
    if (mpPathInfo(candidate, info) && info.isDirectory) return candidate;
    return ".";
#endif
}

// 判断两个路径是否指向同一文件
// Test whether two paths refer to the same file
// ᠬᠣᠶᠠᠷ ᠵᠠᠮ ᠨᠢᠭᠡ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
int mpEquivalent(const std::string& first, const std::string& second){
    if (first.empty() || second.empty()) return -1;
#if defined(MINIPY_OS_WINDOWS)
    auto openForInfo = [](const std::string& path) -> HANDLE {
        std::wstring wide = utf8ToWide(path);
        return CreateFileW(wide.c_str(), FILE_READ_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    };
    HANDLE firstHandle = openForInfo(first);
    if (firstHandle == INVALID_HANDLE_VALUE) return -1;
    HANDLE secondHandle = openForInfo(second);
    if (secondHandle == INVALID_HANDLE_VALUE){
        CloseHandle(firstHandle);
        return -1;
    }
    BY_HANDLE_FILE_INFORMATION firstInfo;
    BY_HANDLE_FILE_INFORMATION secondInfo;
    bool firstOk = GetFileInformationByHandle(firstHandle, &firstInfo) != 0;
    bool secondOk = GetFileInformationByHandle(secondHandle, &secondInfo) != 0;
    CloseHandle(firstHandle);
    CloseHandle(secondHandle);
    if (!firstOk || !secondOk) return -1;
    bool same = firstInfo.dwVolumeSerialNumber == secondInfo.dwVolumeSerialNumber &&
                firstInfo.nFileIndexHigh == secondInfo.nFileIndexHigh &&
                firstInfo.nFileIndexLow == secondInfo.nFileIndexLow;
    return same ? 1 : 0;
#else
    struct stat firstInfo;
    struct stat secondInfo;
    if (::stat(first.c_str(), &firstInfo) != 0) return -1;
    if (::stat(second.c_str(), &secondInfo) != 0) return -1;
    return (firstInfo.st_dev == secondInfo.st_dev &&
            firstInfo.st_ino == secondInfo.st_ino) ? 1 : 0;
#endif
}

}
