// CFFI 外部函数接口头文件
// C Foreign-Function Interface header
// ᠰᠡᠴᠡᠳ ᠦᠨ ᠶᠢᠳᠡ ᠦᠨᠳᠡᠬᠡ ᠢᠨᠲᠡᠷᠽᠢᠰ ᠦᠨ ᠲᠣᠣᠯᠭ᠎ᠠ ᠪᠢᠴᠢᠭ
#ifndef CFFI_HPP
#define CFFI_HPP

#include <map>
#include <string>
#include <vector>
#include "vmware.hpp"

namespace minipy{
class cffi;


// native 函数签名: 接收 cffi 上下文, 返回一个 value
// Native function signature: takes cffi context, returns a value
// ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ ᠶᠢᠨ ᠰᠢᠭᠨᠠᠲᠤᠷ᠎ᠠ
using cffi_native_function = value (*)(cffi&);


// FFI 类: 描述一个可从 Python 侧构造与访问的原生类
// FFI class: a native class constructible/accessible from Python
// FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠲᠥᠷᠥᠯ
struct ffi_class{
    std::string name = "";
    void* constructor = nullptr;
    std::map<std::string, void*> methods;
    std::map<std::string, void*> data;
};


// FFI 库: 一组函数/数据/类的集合 (对应一个 Python 模块)
// FFI library: a bundle of functions/data/classes (a Python module)
// FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ
struct ffilib{
    std::string name = "";
    std::map<std::string, void*> functions;
    std::map<std::string, void*> data;
    std::map<std::string, ffi_class> classes;
};


// FFI 类实例: 持有原生指针与属性表
// FFI class instance: native handle plus attribute table
// FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠶᠢᠨᠢᠰ
struct ffi_instance{
    const ffilib* library = nullptr;
    const ffi_class* klass = nullptr;
    void* handle = nullptr;
    std::map<std::string, value> attributes;

    value ownedValue;

    std::string typeName = "";
    dynamic_get dynamicGet = nullptr;
    dynamic_call dynamicCall = nullptr;
};


// cffi 上下文: native 函数的执行上下文, 提供参数/关键字/构造器/属性访问
// cffi context: execution context for native functions, provides args/kwargs/makers/attr access
// cffi ᠦᠨᠳᠡᠬᠡ: ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ ᠶᠢᠨ ᠬᠤᠷᠢᠶᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ
class cffi{
public:
    vm* vm = nullptr;
    const ffilib* library = nullptr;
    const ffi_class* currentClass = nullptr;
    std::vector<value> argv;
    std::map<std::string, value> kwargv;
    value selfValue;

// 当前被调用的原生函数原始指针 (即 ffilib 槽位里的 void* 原值)
// Raw pointer stored in the ffilib slot currently being invoked
// ᠣᠳᠣᠮᠤ ᠳᠤᠭᠤᠳᠠᠭᠳᠠᠵᠤ ᠪᠠᠶᠢᠭ᠎ᠠ ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ
    void* nativeEntry = nullptr;
public:

// 取得位置参数个数
// Number of positional arguments
// ᠪᠠᠶᠢᠳᠠᠯ ᠬᠣᠶᠠᠳᠠᠯ ᠦᠨ ᠲᠣᠨᠢ
    size_t argc() const;

// 取得第 index 个位置参数 (越界返回 None)
// Get positional arg at index (returns None if out of range)
// index ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠯᠠᠬᠤ
    value arg(size_t index) const;

// 第 index 个位置参数转为 string (失败返回空串 "")
// Get positional arg as string (returns "" on failure)
// index ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠯᠠᠬᠤ → string
    std::string arg_str(size_t index) const;

// 第 index 个位置参数转为整数 (失败返回 0)
// Get positional arg as integer (returns 0 on failure)
// index ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠯᠠᠬᠤ → int
    long long arg_int(size_t index) const;

// 第 index 个位置参数转为浮点 (失败返回 0.0)
// Get positional arg as float (returns 0.0 on failure)
// index ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠯᠠᠬᠤ → float
    double arg_float(size_t index) const;

// 第 index 个位置参数转为布尔 (truthy 判定)
// Get positional arg as bool (truthiness)
// index ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠯᠠᠬᠤ → bool
    bool arg_bool(size_t index) const;

// 是否存在某个命名关键字参数
// Whether a named keyword argument exists
// ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠣᠯᠨ᠎ᠡ ᠶᠤᠤ
    bool has_kwarg(const std::string& name) const;

// 取得某个关键字参数 (不存在返回 None)
// Get a keyword argument (returns None if missing)
// ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠬᠠᠯᠠᠬᠤ
    value kwarg(const std::string& name) const;

// 取得关键字参数为整数 (失败/不存在返回 fallback)
// Get keyword arg as int (fallback on missing / wrong type)
// keyword ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ → int
    long long kwarg_int(const std::string& name, long long fallback) const;

// 取得关键字参数为字符串 (失败/不存在返回 fallback)
// Get keyword arg as string (fallback on missing / wrong type)
// keyword ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ → string
    std::string kwarg_str(const std::string& name, const std::string& fallback) const;

// 取得关键字参数为布尔 (失败/不存在返回 fallback)
// Get keyword arg as bool (fallback on missing / wrong type)
// keyword ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ → bool
    bool kwarg_bool(const std::string& name, bool fallback) const;

// ---- 返回值构造器 (mk*) ----
// None value 构造
// Build a None value
// None value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value none() const;

// 整数 value 构造
// Build an integer value
// int value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkint(long long v) const;

// 浮点 value 构造
// Build a float value
// float value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkfloat(double v) const;

// 布尔 value 构造
// Build a bool value
// bool value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkbool(bool v) const;

// 字符串 value 构造
// Build a string value
// string value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkstr(const std::string& v) const;

// 列表 value 构造
// Build a list value
// list value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mklist(const std::vector<value>& elements) const;

// 字典 value 构造
// Build a dict value
// dict value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkdict(const std::vector<std::pair<value, value>>& items) const;

// 原生对象 value 构造 (用于 FFI 类 constructor)
// Build a native-object value (used by FFI class constructors)
// native object value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value mkobject(void* handle = nullptr) const;

// ---- self 与属性操作 ----
// 取得当前 self 值 (类方法调用时自动绑定)
// Get current self (auto-bound for class method calls)
// ᠣᠳᠣᠮᠤ self ᠬᠠᠯᠠᠬᠤ
    value self() const;

// 读 self.name (触发描述器协议)
// Read self.name (descriptor protocol)
// self.name ᠢ ᠬᠠᠯᠠᠬᠤ
    value attr(const std::string& name) const;

// 写 self.name = v (触发描述器协议)
// Set self.name = v (descriptor protocol)
// self.name ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void setattr(const std::string& name, const value& v);

// 读 obj.name (通用版)
// Read obj.name (generic)
// obj.name ᠢ ᠬᠠᠯᠠᠬᠤ
    value attr(const value& object, const std::string& name) const;

// 写 obj.name = v (通用版)
// Set obj.name = v (generic)
// obj.name ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void setattr(const value& object, const std::string& name, const value& v);
};


// 构造一个 ffilib 描述对象
// Build an ffilib descriptor
// ffilib ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
ffilib cffi_lib(const std::string& name,
                const std::map<std::string, void*>& functions,
                const std::map<std::string, void*>& data = {},
                const std::map<std::string, ffi_class>& classes = {});


// 构造一个 ffi_class 描述对象
// Build an ffi_class descriptor
// ffi_class ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
ffi_class cffi_class(const std::string& name,
                     void* constructor,
                     const std::map<std::string, void*>& methods,
                     const std::map<std::string, void*>& data = {});


// 内置 builtins 库
// Builtins library
// builtins ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ
ffilib cffi_builtins_lib();

// math 模块
ffilib cffi_math_lib();
// random 模块
ffilib cffi_random_lib();
// time 模块
ffilib cffi_time_lib();
// sys 模块
ffilib cffi_sys_lib();
// os 模块
ffilib cffi_os_lib();
// os.path 模块
ffilib cffi_os_path_lib();
// string 模块
ffilib cffi_string_lib();
// json 模块
ffilib cffi_json_lib();
// re 模块
ffilib cffi_re_lib();
// itertools 模块
ffilib cffi_itertools_lib();
// statistics 模块
ffilib cffi_statistics_lib();
// base64 模块
ffilib cffi_base64_lib();
// bisect 模块
ffilib cffi_bisect_lib();
// heapq 模块
ffilib cffi_heapq_lib();
// copy 模块
ffilib cffi_copy_lib();
// operator 模块
ffilib cffi_operator_lib();
// ctypes 模块
ffilib cffi_ctypes_lib();



// ctypes 结构字段的属性访问
// ctypes struct field attribute access
// ctypes ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠲᠠᠯᠠᠭ᠌ ᠦᠨ ᠬᠡᠷᠡᠴᠡᠯᠳᠡ
bool ctypesStructGet(vm& vm, void* handle, const std::string& name, value& result);


// UTF-8 字符串转宽字符
// Convert UTF-8 to wide string
// UTF-8 ᠢ ᠦ᠋ᠰᠲᠡ ᠴᠠᠷᠠᠭ᠌ ᠲᠤ ᠪᠡᠷᠢᠷᠡᠩ᠌ᠡ
std::wstring utf8ToWide(const std::string& text);


// FFI 调用入口: 函数 / 数据 / 构造 / 方法 / 类数据
// FFI dispatch: function / data / constructor / method / class data
// FFI ᠶᠢᠨᠳᠡᠬᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ
bool cffiCallFunction(const ffilib& library, const std::string& name,
                      vm& vm, const std::vector<value>& args, value& result,
                      const std::map<std::string, value>& kwargs = {});
bool cffiCallData(const ffilib& library, const std::string& name,
                  vm& vm, value& result);
bool cffiCallConstructor(const ffilib& library, const ffi_class& klass,
                         vm& vm, const std::vector<value>& args, value& result,
                         const std::map<std::string, value>& kwargs = {});
bool cffiCallMethod(vm& vm, const value& self, const std::string& name,
                    const std::vector<value>& args, value& result,
                    const std::map<std::string, value>& kwargs = {});
bool cffiCallClassData(const ffilib& library, const ffi_class& klass,
                       const std::string& name, vm& vm, value& result);


// ctypes 结构字段描述
// ctypes struct field descriptor
// ctypes ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠲᠠᠯᠠᠭ᠌ ᠦᠨ ᠳ᠋ᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
struct CtypesFieldDesc{
    std::string name;
    size_t offset = 0;
    size_t size = 0;
    size_t align = 0;
    bool isPointer = false;
    std::string typeName;
};


// ctypes 结构整体信息
// ctypes struct aggregate info
// ctypes ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠶᠡᠷᠡᠩᠬᠡᠢ ᠳ᠋ᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
struct CtypesStructInfo{
    size_t totalSize = 0;
    size_t align = 0;
    std::vector<CtypesFieldDesc> fields;
    std::vector<unsigned char> buffer;
    std::vector<std::string> fieldNames;
    size_t cursor = 0;
};


// ctypes 句柄类型枚举
// ctypes handle kind enum
// ctypes ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠵᠢᠰᠢᠶ᠎ᠡ
enum CtypesKind{
    ctypes_loader = 0,
    ctypes_library = 1,
    ctypes_function = 2
};



// ctypes 句柄
// ctypes handle
// ctypes ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct CtypesHandle{
    int kind = 0;
    std::string dllName;
    std::string procName;
    void* procAddress = nullptr;
};


// ctypes 回调上下文与封装
// ctypes callback context and wrapper
// ctypes ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠢᠨ ᠲᠥᠪ ᠬᠡᠷᠡᠴᠡᠯ
struct CtypesCallbackContext;
struct CtypesCBWrap{
    CtypesCallbackContext* context = nullptr;
    std::string funcName;
    void* trampoline = nullptr;
};

}

#endif
