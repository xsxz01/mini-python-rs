#ifndef VMWARE_HPP
#define VMWARE_HPP

#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include "lexer.hpp"
#include "parser.hpp"

namespace minipy{

// 虚拟机
// vm
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ
class vm;

// 值
// value
// ᠭᠠᠷᠴᠠᠭ
struct value{
    std::string type = "none";
    void* ptr = nullptr;

    vm* owner = nullptr;
    value() = default;

    value(std::string t, void* v) : type(std::move(t)), ptr(v) {}

    value(std::string t, void* v, vm* o);
    value(const value& other);
    value(value&& other) noexcept;
    value& operator=(const value& other);
    value& operator=(value&& other) noexcept;
    ~value();
};

// ffilib
// ffilib
// ffilib
struct ffilib;

// FFI 类
// FFI class
// FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
struct ffi_class;

// FFI 实例
// FFI instance
// FFI ᠶᠢᠨᠢᠰ
struct ffi_instance;


// 异常 Payload
// exception payload
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ Payload
struct ExceptionPayload{
    std::string type;
    std::string msg;
    value value;
};


// 循环 状态
// for state
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ ᠪᠠᠶᠢᠳᠠᠯ
struct ForState{
    std::vector<value> items;
    long long rangeStart = 0;
    long long rangeStep = 1;
    long long rangeCount = 0;
    long long index = 0;
    int mode = 0;
    bool initialized = false;
    bool done = false;
    value* target = nullptr;
    size_t bodyStart = 0;
    size_t endIndex = 0;

    value genObj;
    bool pendingReady = false;
    value pendingValue;
};


// Active 尝试
// active try
// Active ᠢᠶᠡᠬᠡ ᠵᠢᠳᠡ
struct ActiveTry{
    size_t tryIndex = 0;
    size_t endIndex = 0;
    std::vector<std::string> types;
    std::vector<std::string> vars;
    std::vector<size_t> handlerIndices;
    size_t finallyIndex = (size_t)(-1);
    size_t afterIndex = 0;
    bool inHandler = false;
    bool handledExc = false;
};

// Pending Finish
// pending finish
// Pending Finish
struct PendingFinish{
    int kind = 0;
    size_t jumpTarget = 0;
    value returnValue;
    std::string excType;
    std::string excMsg;
    value excValue;
    size_t finallyIdx = 0;
    size_t endJmpIdx = 0;
};

using dynamic_get = bool (*)(vm& vm, void* handle, const std::string& name,

                                value& result);

using dynamic_call = bool (*)(vm& vm, ffi_instance& self,

                                 const std::vector<value>& args,

                                 const std::map<std::string, value>& kwargs,

                                 value& result);



// 字典 t
// dict t
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ t
struct dict_t : public std::vector<std::pair<value, value>> {

    std::unordered_map<long long, std::vector<size_t>> buckets;
};


// 字典 view
// dict view
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ view
struct dict_view{
    value dict;
    int kind = 0;
};


// 范围 t
// range t
// ᠬᠠᠷᠢᠶ᠎ᠠ t
struct range_t{
    long long start = 0;
    long long stop = 0;
    long long step = 1;

// count
// count
// count
    long long count() const {
        if (step > 0){
            if (start >= stop) return 0;
            return (stop - start + step - 1) / step;
        }
        if (step < 0){
            if (start <= stop) return 0;
            return (start - stop - step - 1) / (-step);
        }
        return 0;
    }

// at
// at
// at
    long long at(long long index) const { return start + index * step; }
};


// Deleg 状态
// deleg state
// Deleg ᠪᠠᠶᠢᠳᠠᠯ
struct DelegState{
    value sub;
    std::string target;
    int mode = 0;
    bool active = false;
    size_t index = 0;
    std::vector<value> items;
    range_t range;
    value result;
};




// 生成器 状态
// generator state
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭᠤᠮᠤᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct GeneratorState{

// Status
// status
// Status
    enum Status { CREATED = 0, SUSPENDED = 1, CLOSED = 2 };
    function_t* fn = nullptr;
    std::map<std::string, value> localScope;
    size_t pc = 0;
    int status = CREATED;
    bool isCoroutine = false;
    bool asyncGen = false;
    bool closing = false;
    bool running = false;

    value pendingSend;
    bool hasPendingSend = false;
    std::string pendingTarget;

    bool hasPendingThrow = false;
    std::string pendingThrowType;
    std::string pendingThrowMsg;
    value pendingThrowValue;

    std::map<size_t, ForState> forStates;
    std::map<size_t, DelegState> delegStates;
    std::vector<ActiveTry> tryStack;
    std::vector<PendingFinish> pendingFinishes;

    const std::vector<std::string>* globalNames = nullptr;

    size_t savedScopeDepth = 0;
};


// heap kind
// heap kind
// heap kind
enum heap_kind{
    vk_int = 0, vk_float, vk_bool, vk_str, vk_list, vk_tuple, vk_dict,
    vk_dict_view, vk_range, vk_object, vk_native_object, vk_exception,
    vk_class,
    vk_super,
    vk_bound_function
};


// heap 值
// heap value
// heap ᠭᠠᠷᠴᠠᠭ
struct heap_value{
    int kind = vk_int;
    void* pointer = nullptr;
};


// 方法 context
// method context
// ᠠᠷᠭᠠ context
struct method_context{
    class_t* definingClass = nullptr;
    value self;
};


// eval item t
// eval item t
// eval item t
struct eval_item_t{
    std::string text;
    span_t span;
};

// 虚拟机
// vm
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ
class vm{
    friend bool generatorStateGet(vm& vm, void* handle, const std::string& name, value& result);
    friend bool generatorStateCall(vm& vm, ffi_instance& self,
                        const std::vector<value>& args,
                        const std::map<std::string, value>& kwargs,
                        value& result);
public:
    vm();
    ~vm();
// 当前正在运行/最后一次载入的脚本 (成员)
// Currently running / last loaded script (member)
// ᠣᠳᠣᠮᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ
    script_t script;

// 把 Python 源码编译为 script_t (AST + 字节码)
// Compile Python source into a script_t (AST + bytecode)
// ᠫᠢᠲ᠋ᠢᠣᠨ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ script_t ᠪᠣᠯᠭᠠᠨ᠎ᠠ
    script_t compile(const std::string& source);

// 执行已装载的字节码/脚本 (返回空指针, 仅供签名兼容)
// Execute loaded bytecode/script (returns null, signature only)
// ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
    void* run();

// 直接执行一段源码字符串 (compile + run 便捷入口)
// Execute a source string directly (compile + run shortcut)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    bool execSource(const std::string& source);

// 把源码编译为序列化的字节码 (独立保存用)
// Serialize source into raw bytecode (for standalone save/load)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠲ᠋ᠢ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
    std::vector<unsigned char> compileBytecode(const std::string& source);

// 把已有 script_t 再序列化为字节码 (重载)
// Serialize an existing script_t into bytecode (overload)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠢᠵᠢᠨ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠲ᠋ᠢ
    std::vector<unsigned char> complieBytecode(script_t& script);

// 从原始字节码加载脚本
// Load a script from raw bytecode
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠲ᠋ᠢ ᠡᠴᠡ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    bool setBytecode(const std::vector<unsigned char>& bytecode);

// 从 script_t 直接装载
// Load directly from script_t
// script_t ᠡᠴᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    bool setBytecode(script_t script);

// 加载一个 FFI 库 (内置/native 模块)
// Register an FFI library (builtin/native module)
// FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void loadlib(const ffilib& library);

// 按模块名查找已装载的 FFI 库 (未找到返回 nullptr)
// Find a loaded FFI library by module name (nullptr if absent)
// ᠨᠡᠷᠡ ᠪᠡᠷ FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠬᠠᠭᠤᠴᠠᠯᠳᠤ
    ffilib* findCffiLibrary(const std::string& name);

// 一次性加载全部内置标准库 (builtins / math / os / json ...)
// Load all builtin stdlib at once (builtins, math, os, json ...)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠢ ᠪᠡᠷᠢᠨ᠎ᠠ
    void usestdlib();

// 从已编译脚本加载一个 Python 模块 (匿名, 以脚本名作为模块名)
// Load a Python module from compiled script (anonymous, uses script name)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠡᠴᠡ Python ᠮᠣᠳᠤᠯᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void loadpy(const script_t& libraryScript);

// 从已编译脚本加载一个 Python 模块 (指定模块名)
// Load a Python module from compiled script (with explicit name)
// ᠳᠠᠩᠰᠠᠨ ᠮᠣᠳᠤᠯᠢ ᠪᠠᠢᠳᠠᠯᠳᠡᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void loadpy(const std::string& name, const script_t& libraryScript);

// 从字节序列加载一个 .pyd/DLL 扩展模块 (ctypes/FFI)
// Load a .pyd / DLL extension module from bytes (ctypes / FFI)
// .pyd ᠬᠡᠯᠪᠡᠷᠢ ᠪᠢᠲ᠋ᠢ ᠡᠴᠡ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void loadpyd(const std::string& name, std::vector<unsigned char> pydfile);

// 注册一个脚本库 (用于模块缓存 / import 查找)
// Register a script library (module cache / import lookup)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void addScriptLibrary(const std::string& name, const script_t& libraryScript);

// 按名查找已注册的脚本库
// Look up a registered script library by name
// ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠬᠠᠯᠠᠬᠤ
    const script_t* findScriptLibrary(const std::string& name) const;

// 执行已注册脚本库的顶层代码 (mainFunction.body), 只执行一次
// Execute the top-level code (mainFunction.body) of a registered script library, exactly once
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠶᠢᠨ ᠳᠠᠷᠠᠯᠠᠯᠳ᠋ᠭ᠎ᠠ ᠺᠣᠳ᠋ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    void executeScriptModule(const std::string& name);

// 构造一个整数 value (堆分配, 自带引用计数)
// Build an integer value (heap-allocated, reference-counted)
// ᠲᠣ᠎ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeInt(long long v);

// 构造一个浮点 value
// Build a float value
// ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeFloat(double v);

// 构造一个布尔 value
// Build a bool value
// ᠪᠤᠯ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeBool(bool v);

// 构造一个字符串 value
// Build a string value
// ᠦᠰᠦᠭ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeString(const std::string& v);

// 构造一个 None value (空值, 不进堆)
// Build a None value (not heap-allocated)
// None value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeNone();

// 构造一个列表 value (元素拷贝进堆)
// Build a list value (elements copied onto the heap)
// ᠲᠠᠯᠠᠭ᠌ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeList(const std::vector<value>& elements);

// 构造一个元组 value
// Build a tuple value
// ᠬᠣᠶᠠᠳᠠᠯ ᠲᠠᠯᠠᠭ᠌ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeTuple(const std::vector<value>& elements);

// 构造一个字典 value (可选初值列表)
// Build a dict value (optional initializer list)
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeDict(const std::vector<std::pair<value, value>>& items = {});

// 构造 dict_keys / dict_values / dict_items 视图 (kind 0/1/2)
// Build dict_keys / dict_values / dict_items view (kind 0/1/2)
// dict ᠦᠶᠢᠯᠳᠡᠯ ᠶᠢᠨ ᠪᠦᠳᠳᠦᠪᠡᠷᠢ
    value makeDictView(const value& dict, int kind);

// 构造 range(start, stop, step)
// Build a range(start, stop, step)
// range ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeRange(long long start, long long stop, long long step);

// 构造一个脚本类的实例 value (klass 来自 script_t.classes)
// Build a script-class instance value
// script ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠶᠢᠨᠢᠰ
    value makeObject(class_t* klass);

// 调用任意可调用 value (函数/方法/内建函数/FFI 函数)
// Invoke any callable value (function / method / builtin / FFI)
// ᠢᠯᠡᠳᠬᠡᠯᠳᠡ value ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value callValue(const value& callee, const std::vector<value>& args);

// 在字典中查找 key, 返回 value 指针 (找不到返回 nullptr)
// Look up key in dict, return value pointer (nullptr if missing)
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠡᠴᠡ ᠬᠡᠯᠲᠡᠭᠡᠨ ᠬᠠᠯᠠᠬᠤ
    value* dictFind(const value& dict, const value& key);

// 向字典写入键值对, 返回错误字符串 (空串表示成功)
// Set a key/value pair in dict, returns error string (empty = ok)
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠳ᠋ᠡᠨᠳᠡ ᠣᠷᠣᠭᠤᠯᠬᠤ
    std::string dictSet(const value& dict, const value& key, const value& v);

// 从字典中删除 key, 返回是否删除成功
// Remove key from dict, returns whether it existed
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠡᠴᠡ ᠦᠬᠡᠬᠡᠵᠡ
    bool dictRemove(const value& dict, const value& key);

// 判断 value 是否可哈希 (字符串/整数/浮点/布尔/tuple/None 为真)
// Test whether a value is hashable
// value ᠰᠢᠯᠵᠢᠭᠦᠷᠢᠶ᠎ᠡᠨ ᠪᠣᠯᠨ᠎ᠡ ᠶᠤᠤ
    bool hashable(const value& key);

// 返回 value 的整数哈希值 (CPython 算法兼容)
// Return integer hash of a value (CPython compatible)
// value ᠶᠢᠨ ᠰᠢᠯᠵᠢᠭᠦᠷᠢᠶ᠎ᠡ
    long long hashKey(const value& key);

// 字典桶重建 (插入/删除到一定负载因子时触发)
// Rebuild dict bucket table (triggered by load factor)
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠦᠳᠳᠦᠪᠡᠷᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void dictRebuildBuckets(dict_t& d);

// 比较两个 value 是否相等 (== 语义)
// Compare two values for equality (== semantics)
// ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠬᠤ
    bool valuesEqual(const value& left, const value& right) const;

// 判断 value 的真值 (bool(v))
// Test truthiness of a value (bool(v))
// value ᠶᠢᠨ ᠨᠲᠠ/ᠦᠭᠡᠢ
    bool truthy(const value& v);

// 把 value 转成 Python str (str() 语义)
// Convert a value to Python str (str() semantics)
// value ᠢ str ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
    std::string toString(const value& v);

// 把 value 转成整数 (int() 语义, 失败抛异常并返回 0)
// Convert a value to int (int() semantics, returns 0 on failure)
// value ᠢ int ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
    long long toInt(const value& v) const;

// 把 value 转成浮点 (float() 语义)
// Convert a value to float (float() semantics)
// value ᠢ float ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
    double toFloat(const value& v) const;

// 把可迭代对象实体化为 value 列表 (for/range/生成器等)
// Materialize an iterable into a value list
// ᠶᠢᠨᠳᠡᠬᠡ ᠪᠣᠯᠨ᠎ᠠ value ᠢ ᠳᠠᠷᠠᠯᠠᠯᠳ᠋ᠭ᠎ᠠ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
    void materializeIterable(const value& v, std::vector<value>& items);

// 执行二元运算符 (+ / - / * / / // % ** << >> & ^ | ...)
// Apply a binary operator (+ - * / // % ** << >> & ^ | ...)
// ᠬᠣᠶᠠᠳᠠᠯ ᠦᠶᠢᠯᠳᠡᠯ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value applyBinary(const std::string& op, const value& left, const value& right);

// 执行一元运算符 (+ / - / ~ / not)
// Apply a unary operator (+ - ~ not)
// ᠨᠢᠳᠡᠳᠡᠯ ᠦᠶᠢᠯᠳᠡᠯ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value applyUnary(const std::string& op, const value& v);

// 执行 format(v, spec) 等价调用
// Execute format(v, spec)
// format ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value applyFormat(const value& v, const std::string& spec = "");

// 执行 % 格式化 (printf 风格)
// Execute % formatting (printf style)
// % ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value percentFormat(const value& format, const value& arg);

// 动态创建一个类 (type(name, bases, dict) 等价)
// Dynamically create a class (type(name, bases, dict))
// type() ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ
    value createDynamicClass(const std::string& name,
                               const std::vector<value>& bases,
                               const value& namespaceDict);

// 收集对象的 dir() 名字列表 (含继承链)
// Collect dir() names from an object (includes inheritance chain)
// dir() ᠶᠢᠨ ᠨᠡᠷᠡ ᠵᠢᠰᠢᠶ᠎ᠡ
    std::vector<std::string> collectDirNames(const value& v);

// 在对象上查找某个方法 (优先实例属性, 再走类/继承链)
// Look up a method on an object (instance attrs, then class chain)
// object ᠡᠴᠡ ᠠᠷᠭᠠ ᠬᠠᠯᠠᠬᠤ
    function_t* findObjectMethod(const value& v, const std::string& methodName);

// 构造一个原生 (FFI) 对象
// Build a native (FFI) object value
// FFI ᠶᠢᠨᠢᠰ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeNativeObject(const ffilib* library, const ffi_class* klass, void* handle);

// 构造一个复数 value (real + imag*1j)
// Build a complex value (real + imag*1j)
// ᠰᠢᠯᠢ ᠲᠣ᠎ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeComplex(double real, double imag);

// 构造一个动态对象 (由 getter / caller 回调提供属性和调用)
// Build a dynamic object (property + callable via callbacks)
// ᠳ᠋ᠶᠨᠠᠮᠢᠭ ᠪᠣᠯᠭᠠᠨ᠎ᠠ ᠪᠢᠶᠡ
    value makeDynamicObject(const std::string& typeName, void* handle,
                              dynamic_get getter, dynamic_call caller);

// 供 types.new_class 使用: 初始化并托管一个 class_t, 返回 class value
// For types.new_class: initialize + adopt a class_t, return class value
// types.new_class ᠳ᠋ᠦ ᠵᠣᠷᠢᠭᠤᠯᠤᠭᠰᠠᠨ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠬᠢᠵᠦ ᠠᠪᠬᠤ
    value adoptClass(class_t* klass);

// 构造一个异常对象
// Build an exception object
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠶᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeExceptionObject(const std::string& type, const std::string& msg,
                                const value& v);

// 按类名获取已注册的内置类对象 (int/list/dict/Exception/...)
// Look up a builtin type object by name (int / list / dict / Exception ...)
// ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠪᠢᠶᠡ ᠬᠠᠯᠠᠬᠤ
    value getBuiltinTypeObj(const std::string& typeName);

// 取得对象所属的脚本类 (script object → class_t*)
// Get the script class of a script object
// script object ᠶᠢᠨ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
    class_t* getClassOfScriptObject(const value& obj);

// 堆对象数量 (只读统计, 供 gc 模块)
// Number of heap objects (read-only stats, used by gc)
// ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠲᠣᠭ᠎ᠠ
    long long heapObjectCount() const;

// 堆对象快照 (供 gc.get_objects 只读枚举)
// Snapshot of heap objects (read-only, used by gc.get_objects)
// ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠭᠠᠷᠴᠠᠭ
    std::vector<value> heapSnapshot();

// 取得最后一次错误字符串
// Get the last error string
// ᠬᠤᠶᠢᠷᠲᠠ ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠶᠢᠨ ᠪᠢᠴᠢᠭ
    std::string getLastErr() const;

// 是否有未处理的错误
// Whether an error has been set
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠲᠡᠢ ᠶᠤᠤ
    bool hasError() const;

// 清空错误状态
// Clear the error state
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠪᠡᠯᠴᠡᠡᠷᠢᠶᠡᠨ᠎ᠠ
    void clearErr();

// 当前 pending 异常的类型名 (供 builtins/lib 内部判断 StopIteration 等)
// Current pending exception type name (used by builtins/lib to detect StopIteration etc.)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠲᠥᠷᠥᠯ ᠨᠡᠷᠡ
    const std::string& exceptionType() const;

// 抛出异常 (设置 pending 状态, 后续由 run/invoke 中断控制流)
// Raise an exception (sets pending state, control flow interrupted later)
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠡᠷᠭᠢᠵᠡ
    void raiseError(const std::string& type, const std::string& message,
                    const value& v = value());

// 错误快照: 保存/恢复当前错误状态 (配合 SaveErrorState 使用)
// Error snapshot: save / restore the current error state
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠶᠢᠨ ᠲᠠᠰᠪᠢᠮᠠᠭ
    struct ErrorSnapshot{
        bool failed = false;
        std::string lastErr;
        span_t caret;
    };

// 保存当前错误状态
// Save the current error state
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠲᠠᠰᠪᠢᠮᠠᠭ᠎ᠠ
    ErrorSnapshot saveErrorState() const;

// 恢复错误状态
// Restore the error state
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void restoreErrorState(const ErrorSnapshot& snapshot);

// 调用 script 函数 (内部入口, 支持 selfValue / kwargs / 显示名)
// Invoke a script-side function (internal entry, supports self/kwargs/display)
// script ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    value invoke(function_t* function, const std::vector<value>& args, value* selfValue = nullptr,
                   const script_t* sourceScript = nullptr, const std::string& displayName = "",
                   const std::map<std::string, value>& kwargs = {});

// 在所有作用域中查找变量, 返回其 value 指针 (找不到返回 nullptr)
// Find a variable across all scopes, returns value pointer (nullptr if missing)
// ᠪᠦᠳᠳᠦ ᠬᠠᠷᠢᠶ᠎ᠠ ᠡᠴᠡ ᠪᠢᠴᠢᠭ ᠬᠠᠯᠠᠬᠤ
    value* findVariablePtr(const std::string& name);

// 在所有作用域中查找变量 (值拷贝)
// Find a variable across all scopes (by value copy)
// ᠪᠢᠴᠢᠭ ᠬᠠᠯᠠᠬᠤ (ᠭᠠᠷᠴᠠᠭ ᠶᠢᠨ ᠺᠣᠱᠢᠶ᠎ᠡ)
    value findVariable(const std::string& name);

// 在当前 (最内层) 作用域设置变量
// Set a variable in the current (innermost) scope
// ᠪᠢᠴᠢᠭ ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void setVariable(const std::string& name, const value& v);

// 从所有作用域移除变量 (若存在)
// Remove a variable from all scopes (if present)
// ᠪᠢᠴᠢᠭ ᠢ ᠦᠬᠡᠬᠡᠵᠡ
    void removeVariable(const std::string& name);

// 取得全部作用域里已定义的名字 (去重, 按作用域由内向外顺序; 供宿主/C-API 枚举)
// Collect every name defined in any scope (deduplicated, innermost first; for hosts/C-API)
// ᠪᠦᠳᠳᠦ ᠬᠠᠷᠢᠶ᠎ᠠ ᠡᠴᠡ ᠪᠢᠴᠢᠭ ᠶᠢᠨ ᠨᠡᠷᠡ ᠵᠢᠰᠢᠶ᠎ᠡ
    std::vector<std::string> collectScopeNames() const;

// 帧
// frame
// ᠦᠷᠡ
    struct Frame{
        std::string file;
        std::string function;
        int line = 0;
        const script_t* sourceScript = nullptr;
        span_t caret;
    };
// 取得 value 的 Python 类型名字符串 ("NoneType"/"int"/"str"/类名/...)
// Get Python type-name string of a value ("NoneType", "int", "str", class name ...)
// value ᠶᠢᠨ Python ᠲᠥᠷᠥᠯ ᠨᠡᠷᠡ ᠪᠢᠴᠢᠭ
    std::string pythonTypeName(const value& v) const;

// 在类/继承链里按名查找方法定义 (返回 function_t*, 找不到 nullptr)
// Look up a method name across class + inheritance chain
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ / ᠡᠪᠦᠭ ᠦᠨᠳᠡᠭᠡ ᠠᠷᠭᠠ ᠬᠠᠯᠠᠬᠤ
    function_t* findMethod(class_t* klass, const std::string& name);

// 同 findMethod, 额外返回定义该方法的类 (用于 super / 绑定方法)
// Like findMethod, also returns the defining class
// ᠬᠠᠯᠠᠬᠤ + ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠢ ᠪᠡᠷᠢᠨ᠎ᠠ
    class_t* findMethodOwner(class_t* klass, const std::string& name, function_t*& out);

// 返回类所继承的内置基类名 ("list"/"dict"/"" 等)
// Return the builtin-base name a class inherits ("list", "dict", "" ...)
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠰᠢᠨ᠎ᠡ
    std::string builtinBaseName(class_t* klass);

// 取得 value 所属的容器基类 (dict/list/tuple 等, 若无返回 None value)
// Return the container "base" of a value (dict / list / tuple ...)
// value ᠶᠢᠨ ᠦᠮᠢᠰᠢ ᠰᠢᠨ᠎ᠡ
    value containerOf(const value& v) const;

// 判断 sub 是否是某个内置类的子类 (含自身)
// Whether sub is a subclass (or same class) of a builtin
// sub ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠡᠪᠦᠭ ᠶᠤᠤ
    bool isSubclassOfBuiltin(const value& sub, const std::string& builtinName);

// 构造一个 super() 代理对象 (klassArg/objArg 对应 super(type, obj))
// Build a super() proxy object (klassArg/objArg for super(type, obj))
// super() ᠪᠢᠶᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    value makeSuper(const value* klassArg = nullptr, const value* objArg = nullptr);

// 压入方法调用上下文 (配合 super() 实现)
// Push method-call context (super() support)
// ᠠᠷᠭᠠ ᠢᠨᠳᠡᠬᠡ ᠢ ᠰᠲᠠᠭᠭᠡᠷ ᠳᠤ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void pushMethodContext(class_t* definingClass, const value& self);

// 弹出方法调用上下文
// Pop method-call context
// ᠠᠷᠭᠠ ᠢᠨᠳᠡᠬᠡ ᠢ ᠰᠲᠠᠭᠭᠡᠷ ᠡᠴᠡ ᠪᠡᠷᠢᠵᠡ
    void popMethodContext();

// 把脚本函数绑定到 self 上, 返回 bound method value
// Bind a script function to self, returns bound method value
// script ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ ᠢ self ᠳ᠋ᠡᠨᠳᠡ ᠳᠡᠦᠯᠬᠡᠪᠡᠷᠢᠶᠡᠨ᠎ᠠ
    value makeBoundFunction(function_t* fn, const value& self);

// isinstance(obj, target) 语义
// isinstance(obj, target) semantics
// isinstance() ᠬᠡᠯᠪᠡᠷᠢ
    bool isInstance(const value& obj, const value& target);

// 运行模式匹配 subject (用于 match-case)
// Run pattern matching on subject (match-case)
// match-case ᠬᠡᠯᠪᠡᠷᠢ
    bool matchPattern(const value& subject, const std::string& ir,
                      std::vector<std::pair<std::string, value>>& captures);

// 重载: 对 const value 做 truthy 判定 (与上面非 const 版本相同语义, 用于 const 上下文)
// Overload: truthiness test in const context
// const ᠦᠷᠡᠨ᠎ᠡ ᠪᠠᠶᠢᠳᠠᠯᠳᠡᠢ truthy ᠬᠡᠯᠪᠡᠷᠢ
    bool truthy(const value& v) const;

// 读属性: obj.name (触发描述器协议 __get__ / 实例查找 / 类查找)
// Read attribute obj.name (invokes descriptor protocol __get__)
// obj.name ᠢ ᠬᠠᠯᠠᠬᠤ (__get__ ᠬᠡᠯᠪᠡᠷᠢ)
    value getAttribute(const value& obj, const std::string& name);

// 写属性: obj.name = v (触发 __set__ 描述器或写入实例字典)
// Set attribute obj.name = v
// obj.name ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void setAttribute(const value& obj, const std::string& name, const value& v);

// 删除属性: del obj.name (触发 __delete__ 描述器)
// Delete attribute del obj.name
// obj.name ᠢ ᠦᠬᠡᠬᠡᠵᠡ
    void deleteAttribute(const value& obj, const std::string& name);

// hasattr(obj, name) 语义
// hasattr(obj, name) semantics
// hasattr() ᠬᠡᠯᠪᠡᠷᠢ
    bool hasAttribute(const value& obj, const std::string& name);

// obj[key] 取值 (dict / list / tuple / str / range / 自定义 __getitem__)
// Read obj[key] (dict / list / tuple / str / range / custom __getitem__)
// obj[key] ᠬᠠᠯᠠᠬᠤ
    value indexValue(const value& obj, const value& key);

// obj[key] = v 赋值 (触发 __setitem__ 或容器写入), 返回错误字符串 (空串表示成功)
// Assign obj[key] = v, returns an error string (empty means ok)
// obj[key] = v ᠣᠷᠣᠭᠤᠯᠬᠤ
    std::string indexAssign(const value& obj, const value& key, const value& v);

// del obj[key] 删除 (触发 __delitem__ 或容器删除), 返回错误字符串
// Delete obj[key], returns an error string (empty means ok)
// del obj[key] ᠦᠬᠡᠬᠡᠵᠡ
    std::string indexDelete(const value& obj, const value& key);

// ---- private ----
private:
// 取得当前文件名 (供 traceback 显示用)
// Get current file name (used in tracebacks)
// ᠪᠣᠶᠢᠨ ᠪᠢᠴᠢᠭ ᠨᠡᠷᠡ
    std::string currentFileName() const;

// 为未定义名字给出 "Did you mean ..." 候选
// Provide "Did you mean ..." suggestions for undefined names
// ᠲᠠᠰᠤᠯᠭᠠ ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠶᠡᠨᠳᠡᠯ
    std::string suggestName(const std::string& name) const;

    bool failed = false;
    std::string lastErr;
    std::vector<Frame> frames;
    int recursionDepth = 0;

// 按名字在内置 builtins 中查找并调用 (内部快捷入口)
// Call a builtins function by name (internal shortcut)
// builtins ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠬᠠᠯᠠᠬᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    value callFunc(std::string funcName, std::vector<value> args,
                     const std::map<std::string, value>& kwargs = {});

// 泛用调用入口: 可被调用 value (函数/方法/FFI) 统一调度
// Generic dispatch for any callable value
// ᠢᠯᠡᠳᠬᠡᠯᠳᠡ value ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    value invokeAny(value callee, std::vector<value> args);

// 计算表达式字符串为 value (供 eval / 内部使用)
// Evaluate an expression string into a value
// ᠢᠯᠡᠳᠬᠡᠯᠳᠡ ᠪᠢᠴᠢᠭ ᠢ ᠭᠠᠷᠴᠠᠭ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
    value evalExpr(std::string expr);

// 字节码执行主循环: 执行 body 中 pc 起始的指令, 直到 return/exception/暂停
// Bytecode main loop: run instructions from pc until return/exception/pause
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠲᠣᠪ ᠴᠠᠭᠰᠠᠨ
    void executeBody(std::vector<execute_t>& body, bool& returned, value& returnValue,
                     size_t startPc = 0, bool* pauseOut = nullptr, size_t* pausePcOut = nullptr,
                     GeneratorState* gs = nullptr);

// 恢复生成器/协程的执行体 (resume / send / throw)
// Resume a generator / coroutine body (resume / send / throw)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭᠤᠮᠤᠨ / ᠺᠣᠷᠣᠣᠲ᠋ᠢᠨ ᠢ ᠰᠡᠳᠬᠡᠵᠡ
    void generatorResumeBody(GeneratorState* gs, value& out, bool asyncCompletion = false);

// 处理 yield from / await 委派 (DELEG 指令)
// Handle yield-from / await delegation (DELEG op)
// yield-from / await ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠡᠯᠪᠡᠷᠢ
    int delegNextValue(DelegState& ds, int action, const value& payload, value& out);

// 清空当前 pending 异常状态 (except handler 进入时)
// Clear pending exception state (when entering except handler)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠪᠡᠯᠴᠡᠡᠷᠢᠶᠡᠨ᠎ᠠ
    void clearPendingExceptionState();

// 在 script_t.functions 里按名查找函数
// Look up a function by name in script_t.functions
// script ᠶᠢᠨ ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ ᠡᠴᠡ ᠬᠠᠯᠠᠬᠤ
    function_t* findFunction(const std::string& name);

// 在 script_t.classes 里按名查找类
// Look up a class by name in script_t.classes
// script ᠶᠢᠨ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠡᠴᠡ ᠬᠠᠯᠠᠬᠤ
    class_t* findClass(const std::string& name);

// 查找某个函数/方法所属的脚本 (主脚本或已注册的脚本库)
// Find the script that owns a function/method (main script or registered library)
// ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ ᠶᠢᠨ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠬᠠᠯᠠᠬᠤ
    const script_t* findFunctionOwner(const function_t* fn) const;

// 解析类的基类 (把 name/Bases 里的 string 变成实际 class_t*)
// Resolve a class's base-class string names into class_t*
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠡᠪᠦᠭ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void resolveClassBase(class_t* klass);

// 调用 object.__init_subclass__(**kwargs) 钩子
// Invoke object.__init_subclass__(**kwargs) hook
// __init_subclass__ ᠬᠡᠯᠪᠡᠷᠢ
    void applyInitSubclass(class_t* klass, const std::map<std::string, value>& kwargs);

// 初始化脚本类 (准备方法表/类属性/继承)
// Initialize a script class (methods / class attrs / inheritance)
// script ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    bool initializeClass(class_t& klass);

// 尝试把调用分发给 builtins 内建函数 (返回 handled=true 表示已处理)
// Try to dispatch a call to builtins builtin function (handled=true means taken)
// builtins ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠬᠠᠯᠠᠬᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    value callBuiltin(const std::string& name, const std::vector<value>& args, bool& handled,
                        const std::map<std::string, value>& kwargs = {});

// 尝试把 obj.method(...) 分发给 builtins 内建方法 (handled=true 表示已处理)
// Try to dispatch obj.method(...) to builtins method
// obj.method ᠢ builtins ᠲᠤ ᠣᠷᠣᠭᠤᠯᠬᠤ
    value callBuiltinMethod(const std::string& method, value& target,
                              const std::vector<value>& args, bool& handled,
                              const std::map<std::string, value>& kwargs = {});

// 向 target (名字或下标) 赋值 (对应赋值指令)
// Assign to target (name or subscript), used by assign opcodes
// target ᠳ᠋ᠡᠨᠳᠡ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void assignTarget(const std::string& target, const value& v);

// 把 value 解包到 targets 列表 (a, *b, c = val 语义)
// Unpack a value into target names (a, *b, c = val semantics)
// value ᠢ targets ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ
    void unpackValues(const value& v, const std::vector<std::string>& targets);

// 把 value 解包到单个 target (内部递归入口)
// Unpack a value into a single target (internal recursive entry)
// value ᠢ target ᠳ᠋ᠡᠨᠳᠡ ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ (内部)
    void unpackTarget(const std::string& target, const value& v);

// 解析切片 text 得到 (start, stop, step), length 是被切片序列长度
// Parse slice text into (start, stop, step) given the sequence length
// ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠴᠢᠭ ᠢ ᠨᠡᠭᠡᠯᠬᠡᠨ᠎ᠠ
    bool sliceBounds(const std::string& text, long long length,
                     long long& start, long long& stop, long long& step);

// 判断表达式文本是否为纯切片 (无其他内容)
// Whether an expression text is a pure slice
// ᠢᠯᠡᠳᠬᠡᠯᠳᠡ ᠪᠢᠴᠢᠭ ᠪᠣᠯᠭᠠᠨ᠎ᠠ ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠤᠤ
    static bool isSliceText(const std::string& text);

// 计算一系列 eval_item_t (表达式/优先级递下降解)
// Evaluate a list of eval_item_t (expression degrading by precedence)
// eval_item_t ᠶᠢᠨ ᠳᠠᠷᠠᠯᠠᠯᠳ᠋ᠭ᠎ᠠ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    value evalItems(const std::vector<eval_item_t>& items, size_t& index, int minPrec,
                      span_t* outSpan = nullptr);

// 计算一元 item (数字/标识符/字符串/括号表达式)
// Evaluate a unary item (number / ident / string / paren expr)
// ᠨᠢᠳᠡᠳᠡᠯ item ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
    value evalUnaryItem(const std::vector<eval_item_t>& items, size_t& index,
                          span_t* outSpan = nullptr);

// 把一个 operand 字符串解析为 value (数字/字符串/标识符)
// Parse an operand string into a value (number / string / identifier)
// operand ᠪᠢᠴᠢᠭ ᠢ value ᠪᠣᠯᠭᠠᠨ᠎ᠠ
    value evalOperand(const std::string& item, const span_t& span);

// 在堆上登记一块内存, 初始引用计数 0 (封装成 value 后变 1)
// Register a pointer on the heap, initial refcount 0
// ᠭᠠᠷᠴᠠᠭ ᠢ ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void* keep(int kind, void* pointer);

// ---- public (ctypes 相关) ----
public:
// 为 ctypes.Structure 实例初始化字段 (自动按 field/offset 读二进制)
// Initialize ctypes.Structure instance fields from binary buffer
// ctypes.Structure ᠶᠢᠨ ᠲᠠᠯᠠᠭ᠌ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    void initCtypesStruct(value& instance, class_t* klass);

// 取出 ctypes 实例背后的原始二进制缓冲区指针
// Get the raw binary buffer pointer of a ctypes instance
// ctypes instance ᠶᠢᠨ ᠪᠢᠨᠠᠷᠢ ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
    void* ctypesStructBufferOf(const value& v);

// 把堆对象引用计数 +1 (value 构造/拷贝时自动调用)
// Increment heap object refcount (+1, called by value ctor/copy)
// refcount +1
    void retainValue(void* pointer);

// 把堆对象引用计数 -1 (value 析构/移动赋值时自动调用, 到 0 时自动释放)
// Decrement heap object refcount (-1, auto-releases when hitting 0)
// refcount -1
    void releaseValue(void* pointer);

// 查询堆对象当前引用计数 (不存在返回 0)
// Query current heap-object refcount (0 if unknown)
// refcount ᠬᠠᠯᠠᠬᠤ
    long long refCountOf(void* pointer) const {
        auto it = refCounts.find(pointer);
        return it != refCounts.end() ? it->second : 0;
    }

// ---- private 成员变量 ----
private:
// 作用域栈, scopes.back() 是当前作用域
// Scope stack; scopes.back() is the current scope
// ᠬᠠᠷᠢᠶ᠎ᠠ ᠶᠢᠨ ᠰᠲᠠᠭᠭᠡᠷ
    std::vector<std::map<std::string, value>> scopes;

// 堆对象登记表 (pointer → heap_value)
// Heap object registry (pointer → heap_value)
// ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠠᠷᠢᠶᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ
    std::unordered_map<void*, heap_value> heapObjects;

// 引用计数表 (pointer → 计数)
// Reference-count table (pointer → count)
// refcount ᠵᠢᠰᠢᠶ᠎ᠡ
    std::unordered_map<void*, long long> refCounts;

// 析构期标志: true 时 retain/release 空转, 避免级联释放
// Shutdown flag: retain/release become no-op while true
// ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ
    bool shuttingDown = false;

// 真正释放一个堆对象 (按 kind 分发 delete)
// Actually destroy a heap object (dispatch delete by kind)
// ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠦᠬᠡᠬᠡᠵᠡ
    void destroyHeapObject(void* pointer);

    value lastCallResult;
    std::map<long long, value> callResults;

    std::vector<std::pair<std::string, script_t>> scriptLibraries;

// 已执行的脚本库模块名集合 (防止 import 时重复执行顶层代码)
// Set of already-executed script library module names (prevents repeated top-level execution)
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠨᠡᠷᠡ ᠵᠢᠰᠢᠶ᠎ᠡ
    std::set<std::string> executedScriptModules;

    std::vector<ffilib*> cffiLibraries;

    const script_t* currentScript = nullptr;

    span_t pendingCaret;

    span_t lastCalleeSpan;

    std::string pendingExceptionType;
    std::string pendingExceptionMsg;

    value pendingExceptionValue;

    std::vector<ExceptionPayload> handledExcStack;

    std::vector<const std::vector<std::string>*> globalNameStack;

    std::vector<const std::vector<std::string>*> nonlocalNameStack;

    std::vector<size_t> captureScopeIndices;

    std::vector<method_context> methodContexts;

    std::set<class_t*> classInitializing;
};


std::string pythonRepr(vm& vm, const value& v);

// 判定/读取复数 (cmath 等模块使用)
// Complex predicate/parts accessors (used by cmath etc.)
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠰᠢᠯᠲᠡ ᠬᠠᠯᠠᠬᠤ
bool isComplexValueEx(const value& v);
bool complexPartsEx(const value& v, double& real, double& imag);

// 提取闭包底层函数 (types.new_class 等方法使用)
// Extract underlying function from a closure value (used by types.new_class etc.)
// ᠬᠠᠭᠳᠠᠯᠲᠠ ᠶᠢᠨ ᠦᠨᠳᠦᠰᠦᠨ ᠦᠢᠯᠡᠳᠦᠯ
bool closureFunctionCopy(const value& v, function_t& out);

}

#endif
