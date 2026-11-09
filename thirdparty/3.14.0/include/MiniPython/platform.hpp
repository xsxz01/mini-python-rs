// 平台抽象层: 集中隔离动态库加载、可执行内存、错误码与字符编码等平台差异
// Platform layer: centralizes dynamic library loading, executable memory,
// error codes and character encoding so the rest of the interpreter stays portable
// ᠲᠠᠯᠠᠪᠠᠢ ᠶᠢᠨ ᠬᠤᠪᠢᠶ᠎ᠠᠷᠢᠯ ᠤᠨ ᠨᠢᠭᠡᠳᠦᠭᠰᠡᠨ ᠵᠠᠮ
#ifndef MINIPY_PLATFORM_HPP
#define MINIPY_PLATFORM_HPP

#include <string>
#include <vector>

#if defined(_WIN32)
#define MINIPY_OS_WINDOWS 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define MINIPY_OS_IOS 1
#else
#define MINIPY_OS_MACOS 1
#endif
#define MINIPY_OS_POSIX 1
#elif defined(__EMSCRIPTEN__)
#define MINIPY_OS_EMSCRIPTEN 1
#define MINIPY_OS_WASM 1
#elif defined(__wasi__)
#define MINIPY_OS_WASI 1
#define MINIPY_OS_WASM 1
#elif defined(__wasm__)
#define MINIPY_OS_WASM 1
#else
#if defined(__ANDROID__)
#define MINIPY_OS_ANDROID 1
#elif defined(__OHOS__) || defined(__HARMONY__) || defined(__LITEOS__)
#define MINIPY_OS_HARMONYOS 1
#else
#define MINIPY_OS_LINUX 1
#endif
#define MINIPY_OS_POSIX 1
#endif

#if defined(MINIPY_OS_WINDOWS)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(MINIPY_OS_EMSCRIPTEN)
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#elif defined(MINIPY_OS_WASM)
#include <unistd.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace minipy{

// 共享库句柄类型, Windows 为 HMODULE, 其余平台为 dlopen 返回的指针
// Shared library handle: HMODULE on Windows, the dlopen pointer elsewhere
// ᠬᠤᠪᠢᠶ᠎ᠠᠯᠴᠠᠬᠤ ᠰᠠᠩ ᠤᠨ ᠪᠠᠷᠢᠶ᠎ᠠ
#if defined(MINIPY_OS_WINDOWS)
using mp_module = HMODULE;
#else
using mp_module = void*;
#endif

// UTF-8 字符串转宽字符
// Convert UTF-8 to wide string
// UTF-8 ᠢ ᠦ᠋ᠰᠲᠡ ᠴᠠᠷᠠᠭ ᠲᠤ ᠪᠡᠷᠢᠷᠡᠩ᠎ᠡ
std::wstring utf8ToWide(const std::string& text);

// 宽字符转 UTF-8 字符串
// Convert wide string to UTF-8
// ᠦ᠋ᠰᠲᠡ ᠴᠠᠷᠠᠭ ᠢ UTF-8 ᠪᠣᠯᠭ᠎ᠠᠨ ᠬᠤᠪᠢᠶ᠎ᠠᠨ᠎ᠠ
std::string wideToUtf8(const std::wstring& text);

// 加载共享库, 失败返回空句柄
// Load a shared library, returns a null handle on failure
// ᠰᠠᠩ ᠢ ᠲᠠᠲᠠᠬᠤ
mp_module mpLoadLibrary(const std::string& path);

// 按名称取导出符号, 失败返回空指针
// Resolve an exported symbol by name, returns null on failure
// ᠨᠡᠷᠡ ᠪᠡᠷ ᠭᠠᠷᠭᠠᠬᠤ ᠲᠡᠮᠳᠡᠭ
void* mpGetProcAddress(mp_module module, const char* name);

// 按序号取导出符号, 仅 Windows 支持, 其余平台返回空指针
// Resolve an exported symbol by ordinal, Windows only, null elsewhere
// ᠳᠤᠭ᠎ᠠ ᠪᠡᠷ ᠭᠠᠷᠭᠠᠬᠤ ᠲᠡᠮᠳᠡᠭ
void* mpGetProcAddressOrdinal(mp_module module, unsigned int ordinal);

// 释放共享库句柄
// Release a shared library handle
// ᠰᠠᠩ ᠤᠨ ᠪᠠᠷᠢᠶ᠎ᠠ ᠢ ᠴᠡᠯᠡᠭᠡᠨ᠎ᠡ
void mpUnloadLibrary(mp_module module);

// 最近一次系统错误码, Windows 取 GetLastError, 其余平台取 errno
// Last system error code: GetLastError on Windows, errno elsewhere
// ᠰᠡᠦᠯᠴᠢ ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠳᠤᠭ᠎ᠠ
unsigned long mpLastError();

// 设置系统错误码
// Store a system error code
// ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠳᠤᠭ᠎ᠠ ᠢ ᠲᠣᠭᠲᠠᠭ᠎ᠠᠬᠤ
void mpSetLastError(unsigned long code);

// 最近一次共享库加载失败的原因, 供错误信息展示
// Reason of the last failed shared library load, for error messages
// ᠰᠡᠦᠯᠴᠢ ᠠᠯᠳᠠᠭ᠎ᠠ ᠦᠨ ᠰᠢᠯᠲᠠᠭ᠎ᠠᠨ
std::string mpLibraryError();

// 申请可执行内存, 失败返回空指针
// Allocate executable memory, returns null on failure
// ᠭᠦᠢᠴᠡᠳᠬᠡᠬᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠭᠤᠶᠤᠴᠠᠬᠤ
void* mpAllocateExecutable(size_t size);

// 刷新指令缓存, 让新写入的机器码立刻可见
// Flush the instruction cache so freshly written machine code is visible
// ᠵᠢᠴᠡᠢ ᠤᠨ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠰᠡᠷᠢᠭᠡᠭᠦᠯᠬᠦ
void mpFlushInstructionCache(void* code, size_t size);

// 归还可执行内存
// Release executable memory
// ᠭᠦᠢᠴᠡᠳᠬᠡᠬᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠡᠷᠭᠢᠬᠦ
void mpFreeExecutable(void* code, size_t size);

// 当前可执行文件的绝对路径, 取不到时返回空串
// Absolute path of the running executable, empty when unavailable
// ᠠᠭᠰᠠᠬᠤ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠦᠨ ᠵᠠᠮ
std::string mpExecutablePath();

// 当前可执行文件所在目录, 末尾不带分隔符, 取不到时返回空串
// Directory of the running executable without trailing separator, empty when unavailable
// ᠠᠭᠰᠠᠬᠤ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠦᠨ ᠭᠠᠵᠠᠷ
std::string mpExecutableDirectory();

// 当前平台路径分隔符
// Path separator of the current platform
// ᠵᠠᠮ ᠤᠨ ᠰᠠᠯᠭᠠᠭᠴᠢ
char mpPathSeparator();

// 共享库名称的候选列表, 按优先级排列, 用于自动补全平台后缀
// Candidate shared library names in priority order, used to append platform suffixes
// ᠬᠤᠪᠢᠶ᠎ᠠᠯᠴᠠᠬᠤ ᠰᠠᠩ ᠤᠨ ᠨᠡᠷᠡ ᠦᠨ ᠵᠢᠭᠰᠠᠭᠠᠯ
std::vector<std::string> mpSharedLibraryCandidates(const std::string& name);

// 路径条目信息, 统一描述存在性、类型与大小
// Path entry information: existence, type and size in one place
// ᠵᠠᠮ ᠤᠨ ᠵᠥᠪᠰᠢᠶ᠎ᠡ ᠦᠨ ᠮᠡᠳᠡᠭᠡᠯᠡᠯ
struct mpFileInfo{
    bool exists = false;
    bool targetExists = false;
    bool isDirectory = false;
    bool isRegularFile = false;
    bool isSymlink = false;
    long long size = 0;
};

// 目录条目, 含名称与是否目录/符号链接
// Directory entry with name and directory/symlink flags
// ᠭᠠᠵᠠᠷ ᠤᠨ ᠵᠥᠪᠰᠢᠶ᠎ᠡ
struct mpDirEntry{
    std::string name;
    bool isDirectory = false;
    bool isSymlink = false;
};

// 读取路径信息, 路径不存在时返回 false
// Read path information, returns false when the path does not exist
// ᠵᠠᠮ ᠤᠨ ᠮᠡᠳᠡᠭᠡᠯᠡᠯ ᠢ ᠤᠨᠰᠢᠬᠤ
bool mpPathInfo(const std::string& path, mpFileInfo& info);

// 当前工作目录, 取不到时返回空串
// Current working directory, empty when unavailable
// ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ
std::string mpCurrentDirectory();

// 切换工作目录, 成功返回 true
// Change the working directory, returns true on success
// ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ ᠢ ᠰᠣᠯᠢᠬᠤ
bool mpSetCurrentDirectory(const std::string& path);

// 判断是否为绝对路径
// Test whether a path is absolute
// ᠪᠦᠷᠬᠡ ᠨᠡᠷᠢᠴᠢᠯ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool mpIsAbsolutePath(const std::string& path);

// 转为绝对路径, 纯词法处理, 不解析符号链接
// Make a path absolute lexically without resolving symlinks
// ᠨᠡᠷᠢᠴᠢᠯ ᠵᠠᠮ ᠪᠣᠯᠭ᠎ᠠᠬᠤ
std::string mpAbsolutePath(const std::string& path);

// 词法归一化路径, 去掉 "." 与多余的 "..", 等价于 os.path.normpath
// Lexically normalize a path, collapsing "." and redundant "..", like os.path.normpath
// ᠵᠠᠮ ᠢ ᠰᠢᠳᠤᠷᠭᠦᠯᠬᠦ
std::string mpNormalizePath(const std::string& path);

// 列出目录内容, 目录不可读时返回空列表
// List directory contents, empty when the directory cannot be read
// ᠭᠠᠵᠠᠷ ᠤᠨ ᠠᠭᠤᠯᠤᠮᠵᠢ ᠢ ᠵᠢᠭᠰᠠᠬᠤ
std::vector<mpDirEntry> mpListDirectory(const std::string& path);

// 创建单级目录: 1 新建, 0 已存在, -1 失败
// Create one directory level: 1 created, 0 already exists, -1 failure
// ᠨᠢᠭᠡ ᠵᠡᠷᠭᠡ ᠭᠠᠵᠠᠷ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int mpCreateDirectory(const std::string& path);

// 删除空目录, 成功返回 true
// Remove an empty directory, returns true on success
// ᠬᠣᠭᠤᠰᠤᠨ ᠭᠠᠵᠠᠷ ᠢ ᠬᠠᠯᠠᠬᠤ
bool mpRemoveDirectory(const std::string& path);

// 删除文件或符号链接, 成功返回 true
// Remove a file or symlink, returns true on success
// ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠢ ᠬᠠᠯᠠᠬᠤ
bool mpRemoveFile(const std::string& path);

// 移动或重命名文件, 目标存在时覆盖, 成功返回 true
// Move or rename a file, replacing an existing target, returns true on success
// ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠢ ᠵᠢᠭᠵᠢᠭᠦᠯᠬᠦ
bool mpMoveFile(const std::string& from, const std::string& to);

// 临时目录, 不存在的平台回退到 /tmp, 末尾不带分隔符
// Temporary directory, falling back to /tmp, without a trailing separator
// ᠲᠡᠮᠳᠡᠭ ᠦᠨ ᠭᠠᠵᠠᠷ
std::string mpTempDirectory();

// 判断两个路径是否指向同一文件: 1 相同, 0 不同, -1 路径不存在
// Test whether two paths refer to the same file: 1 same, 0 different, -1 missing
// ᠬᠣᠶᠠᠷ ᠵᠠᠮ ᠨᠢᠭᠡ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ ᠡᠰᠡᠬᠢ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
int mpEquivalent(const std::string& first, const std::string& second);

}

#endif
