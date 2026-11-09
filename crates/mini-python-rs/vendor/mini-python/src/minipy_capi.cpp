// MiniPython C 过程式 API 实现
// MiniPython C procedural API implementation
// ᠮᠢᠨᠢ ᠫᠢᠲ᠋ᠢᠣᠨ ᠦ C ᠬᠡᠯᠡᠨ ᠦ ᠠᠫᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠮᠵᠢ
//
// 设计要点 / Design notes / ᠵᠢᠷᠤᠮᠳᠠᠭᠠᠨ ᠰᠠᠨᠠᠭ᠎ᠠ:
// 1. 对外只有 void* 句柄与 C 函数, 所有内部结构都藏在匿名命名空间里。
//    Only void* handles and C functions are exported; internals stay hidden.
// 2. 每个入口都有 try/catch 兜底, 保证 C++ 异常不会跨越 ABI 边界。
//    Every entry point is wrapped in try/catch so no C++ exception escapes.
// 3. value 句柄遵循"持有一次引用"语义, 由 value 的拷贝构造/析构自动维护。
//    Value handles own exactly one reference via value copy semantics.
#include "minipy.h"
#include "cffi.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <map>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace minipy{

namespace{

// ---- 内部结构 / internal structures (完全不对外暴露) ----
// 解释器包装 / interpreter wrapper
struct VmBox{
    vm* engine = nullptr;
    bool owned = true;  // true = heap 分配 (mp_vm_new), false = thread_local (mp_ctx_vm)
    // 返回给 C 侧的字符串缓存 (保证 const char* 在下次调用前有效)
    // String cache keeping returned const char* valid until the next call
    std::string lastError;
    std::string exceptionType;
    std::string typeName;
    std::string scratch;   // 通用字符串暂存 (const char* 返回值)
};

// 值句柄: 持有一个 value (自动引用计数)
// Value handle owning one value (refcount maintained automatically)
struct ValueBox{
    vm* owner = nullptr;
    value v;
    int refcount = 1;     // 句柄级 refcount: mp_value_retain ++, mp_value_release --
    // 返回给 C 侧的字符串缓存 (保证 const char* 在下次调用前有效)
    // String cache keeping returned const char* valid until the next call
    std::string typeName;
};

// FFI 库描述
// FFI library descriptor
struct LibBox{
    ffilib lib;
};

// FFI 类描述
// FFI class descriptor
struct ClassBox{
    ffi_class klass;
};

// 错误快照
// Error snapshot
struct ErrorBox{
    vm::ErrorSnapshot snapshot;
};

// 原生回调上下文
// Native callback context
struct CtxBox{
    vm* engine = nullptr;
    const ffilib* library = nullptr;
    const ffi_class* klass = nullptr;
    const std::vector<value>* argv = nullptr;
    const std::map<std::string, value>* kwargv = nullptr;
    value* self = nullptr;
    std::string errorType;
    std::string errorMessage;
    bool failed = false;
};

// ---- 句柄转换 / handle casts ----
VmBox*    vmBox(void* h){ return static_cast<VmBox*>(h); }
ValueBox* valueBox(void* h){ return static_cast<ValueBox*>(h); }
LibBox*   libBox(void* h){ return static_cast<LibBox*>(h); }
ClassBox* classBox(void* h){ return static_cast<ClassBox*>(h); }
CtxBox*   ctxBox(void* h){ return static_cast<CtxBox*>(h); }

// 由句柄取得引擎
vm* engineOf(void* h){
    VmBox* box = vmBox(h);
    return box ? box->engine : nullptr;
}

// 由句柄取得 value 拷贝 (正确处理引用计数)
value valueOf(void* h){
    ValueBox* box = valueBox(h);
    if (!box) return value();
    return box->v;
}

// 把 value 包装为新句柄 (借用一次引用, 析构时归还)
void* wrapValue(vm* engine, const value& v){
    auto* box = new ValueBox();
    box->owner = engine;
    box->v = v;
    return box;
}

// 复制一段 C 字符串 (NULL 安全)
std::string safeText(const char* text){
    return text ? std::string(text) : std::string();
}

// 分配 C 字符串 (调用方用 mp_free 释放)
char* duplicateText(const std::string& text){
    char* out = static_cast<char*>(std::malloc(text.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, text.data(), text.size());
    out[text.size()] = '\0';
    return out;
}

// 把字符串列表转成 char** (调用方用 mp_free_names 释放)
int allocateNames(const std::vector<std::string>& collected, char*** names, size_t* count){
    if (!names || !count) return MP_ERROR;
    *names = nullptr;
    *count = 0;
    size_t total = collected.size();
    auto** list = static_cast<char**>(std::calloc(total ? total : 1, sizeof(char*)));
    if (!list) return MP_ERROR;
    for (size_t i = 0; i < total; ++i) list[i] = duplicateText(collected[i]);
    *names = list;
    *count = total;
    return MP_OK;
}

// 在 vm 内执行并捕获 C++ 异常
template <typename Fn>
bool guardRun(vm* engine, Fn&& fn){
    try{
        fn();
        return true;
    } catch (const std::exception& e){
        if (engine){
            engine->clearErr();
            engine->raiseError("SystemError", std::string("native failure: ") + e.what());
        }
    } catch (...){
        if (engine){
            engine->clearErr();
            engine->raiseError("SystemError", "native failure: unknown exception");
        }
    }
    return false;
}

// 统一收尾: 失败且没有任何错误信息时补一条
int finish(vm* engine, bool ok){
    if (!ok && engine && !engine->hasError()){
        engine->raiseError("RuntimeError", "operation failed");
    }
    return ok ? MP_OK : MP_ERROR;
}

// value.type → MP_KIND_*
int kindOf(const value& v){
    const std::string& t = v.type;
    if (t == "none" || t.empty()) return MP_KIND_NONE;
    if (t == "int" || t == "bool") return MP_KIND_INT;
    if (t == "float") return MP_KIND_FLOAT;
    if (t == "str") return MP_KIND_STR;
    if (t == "bytes" || t == "bytearray") return MP_KIND_BYTES;
    if (t == "list") return MP_KIND_LIST;
    if (t == "tuple") return MP_KIND_TUPLE;
    if (t == "dict" || t == "dict_keys" || t == "dict_values" || t == "dict_items")
        return MP_KIND_DICT;
    if (t == "range") return MP_KIND_RANGE;
    if (t == "object" || t == "class" || t == "super") return MP_KIND_OBJECT;
    if (t == "native_object"){
        if (v.ptr){
            auto* inst = static_cast<ffi_instance*>(v.ptr);
            if (inst->typeName == "complex" ||
                (inst->klass && inst->klass->name == "complex"))
                return MP_KIND_COMPLEX;
            if (inst->typeName == "exception") return MP_KIND_EXCEPTION;
        }
        return MP_KIND_NATIVE;
    }
    if (t == "function" || t == "bound_function" || t == "native_method") return MP_KIND_FUNC;
    if (t == "module") return MP_KIND_MODULE;
    return MP_KIND_OTHER;
}

// 字典存储 (dict 及 keys/values/items 视图)
dict_t* dictStorage(const value& v){
    if (v.type == "dict" && v.ptr) return static_cast<dict_t*>(v.ptr);
    if ((v.type == "dict_keys" || v.type == "dict_values" || v.type == "dict_items") && v.ptr)
        return static_cast<dict_t*>(static_cast<dict_view*>(v.ptr)->dict.ptr);
    return nullptr;
}

// 序列存储 (list / tuple)
std::vector<value>* seqStorage(const value& v){
    if ((v.type == "list" || v.type == "tuple") && v.ptr)
        return static_cast<std::vector<value>*>(v.ptr);
    return nullptr;
}

// 保证全局作用域里有 __name__ (execSource 等入口依赖 scopes[0])
void ensureGlobalScope(vm* engine){
    engine->setVariable("__name__", engine->makeString("__main__"));
}

// ---- 原生回调中转 / native callback bridge ----
// ffilib 的槽位是 void*, 我们把"C 回调函数指针 + user 数据"打包成一个 Node,
// 槽位里存 Node*; 所有槽位共用一个入口 bridge, 入口按 cffi 上下文反查 Node。
// ffilib slots are void*; each stores a Node* holding the C callback and its
// user data. All slots share one entry (bridge) that resolves the Node.
typedef int (*NativeFn)(void* user, void* ctx, void** out);
typedef void* (*DataFn)(void* user, void* vm);

struct Node{
    void* fn = nullptr;
    void* user = nullptr;
    int kind = 0; // 0 函数, 1 数据取值, 2 常量
    bool pending = true; // 已注册但尚未被匹配过
    value constant;
    // 所有权计数: 初始 1 (LibBox 持有); registerEntry 时 +1,
    // freeNode/unregisterEntry 时 -1, 减到 0 才真正释放。
    // 这样 lib 装载进 vm 后再 mp_lib_free() 不会杀掉仍在使用的回调。
    // Ownership count: starts at 1 (held by LibBox), +1 per registerEntry,
    // -1 per freeNode/unregisterEntry; deleted only at 0. Loading a lib into
    // the vm copies its slots, so loaded callbacks must survive mp_lib_free().
    int refs = 1;
};

std::deque<Node*>& nodePool(){
    static std::deque<Node*> pool;
    return pool;
}

// 入口登记表的前置声明 (定义见下方)
// Forward declaration of the entry registry (defined below)
struct EntryPool;
EntryPool& entryPool();
void unregisterEntry(Node* node);

Node* makeNode(void* fn, void* user, int kind){
    auto* node = new Node();
    node->fn = fn;
    node->user = user;
    node->kind = kind;
    nodePool().push_back(node);
    return node;
}

void freeNode(Node* node){
    if (!node) return;
    if (--node->refs > 0) return; // 仍被入口池引用 (已装载), 存活
    delete node;
}

// 统一执行一项 Node
value runNode(Node* node, cffi& context){
    vm* engine = context.vm;
    if (!engine) return value();
    if (!node || !node->fn){
        engine->raiseError("RuntimeError", "native callback is not registered");
        return engine->makeNone();
    }
    if (node->kind == 2) return node->constant;

    if (node->kind == 1){
        DataFn getter = reinterpret_cast<DataFn>(node->fn);
        void* out = nullptr;
        try{
            out = getter(node->user, nullptr);
        } catch (...){
            out = nullptr;
        }
        if (!out){
            engine->raiseError("RuntimeError", "native data getter returned null");
            return engine->makeNone();
        }
        value result = valueOf(out);
        mp_value_release(out);
        return result;
    }

    CtxBox ctx;
    ctx.engine = engine;
    ctx.library = context.library;
    ctx.klass = context.currentClass;
    ctx.argv = &context.argv;
    ctx.kwargv = &context.kwargv;
    ctx.self = &context.selfValue;

    NativeFn user = reinterpret_cast<NativeFn>(node->fn);
    void* out = nullptr;
    int status = MP_ERROR;
    try{
        status = user(node->user, &ctx, &out);
    } catch (...){
        status = MP_ERROR;
        ctx.failed = true;
        ctx.errorType = "SystemError";
        ctx.errorMessage = "native callback threw an exception";
    }

    value result = engine->makeNone();
    if (out){
        result = valueOf(out);
        mp_value_release(out);
    }
    if (status != MP_OK || ctx.failed){
        if (!ctx.errorType.empty() || !ctx.errorMessage.empty()){
            engine->raiseError(ctx.errorType.empty() ? std::string("RuntimeError") : ctx.errorType,
                               ctx.errorMessage);
        } else if (!engine->hasError()){
            engine->raiseError("RuntimeError", "native function failed");
        }
    }
    return result;
}

// vm 调用原生对象时会把槽位值当成 value(*)(cffi&) 的函数指针直接调用,
// 且槽位值就是回调的唯一身份标识。因此不能让所有槽位共用一个入口,
// 而是要给每个 Node 分配一个真正唯一、可调用的静态入口函数指针:
// 这里用模板按索引生成一整批入口函数 (入口池) 来满足这两点。
// When the vm invokes a native object it calls the slot value directly as a
// value(*)(cffi&) function pointer, and that slot value is the callback's
// identity. So slots cannot share one entry: every Node gets its own unique
// callable static entry pointer, generated in bulk by index templates.

constexpr size_t kEntryPoolSize = 512;

// entrySlot 前置声明 (定义见下方, EntryPool 构造时只取地址)
template <size_t N>
value entrySlot(cffi& context);

struct EntryPool{
    std::vector<Node*> nodes;    // 索引 -> Node
    std::deque<void*> entries;   // 索引 -> 唯一入口函数指针
    std::deque<size_t> freeList; // 空闲索引
    bool built = false;

    EntryPool(){
        nodes.assign(kEntryPoolSize, nullptr);
        for (size_t i = 0; i < kEntryPoolSize; ++i) freeList.push_back(i);
    }
    void buildEntries(); // 定义见下方 (延迟到全部模板可见之后)
};

EntryPool& entryPool(){
    static EntryPool pool;
    if (!pool.built){
        pool.buildEntries();
        pool.built = true;
    }
    return pool;
}

value dispatchEntry(size_t index, cffi& context){
    auto& pool = entryPool();
    if (index >= pool.nodes.size() || !pool.nodes[index]){
        if (context.vm){
            context.vm->raiseError("RuntimeError", "native callback is not registered");
        }
        return context.vm ? context.vm->makeNone() : value();
    }
    return runNode(pool.nodes[index], context);
}

template <size_t N>
value entrySlot(cffi& context){
    return dispatchEntry(N, context);
}

template <size_t... Is>
void buildEntryTable(EntryPool& pool, std::index_sequence<Is...>){
    (pool.entries.push_back(reinterpret_cast<void*>(&entrySlot<Is>)), ...);
}

void EntryPool::buildEntries(){
    buildEntryTable(*this, std::make_index_sequence<kEntryPoolSize>{});
}

// 为 Node 分配一个唯一入口; 池满返回 nullptr
// Allocate a unique entry for the Node; nullptr when the pool is exhausted
void* registerEntry(Node* node){
    if (!node) return nullptr;
    auto& pool = entryPool();
    if (pool.freeList.empty()) return nullptr;
    size_t index = pool.freeList.front();
    pool.freeList.pop_front();
    pool.nodes[index] = node;
    node->refs++; // 入口池接管一份所有权
    return pool.entries[index];
}

// 归还 Node 占用的入口索引
// Return the entry index used by the Node
void unregisterEntry(Node* node){
    if (!node) return;
    auto& pool = entryPool();
    for (size_t i = 0; i < pool.nodes.size(); ++i){
        if (pool.nodes[i] == node){
            pool.nodes[i] = nullptr;
            pool.freeList.push_back(i);
            return;
        }
    }
}

// 把句柄数组转换为 value 数组
std::vector<value> collectArgs(void** args, size_t argc){
    std::vector<value> result;
    if (!args) return result;
    result.reserve(argc);
    for (size_t i = 0; i < argc; ++i) result.push_back(valueOf(args[i]));
    return result;
}

// dict 句柄 → 关键字参数表
std::map<std::string, value> collectKwargs(vm* engine, void* kwargs){
    std::map<std::string, value> result;
    if (!engine || !kwargs) return result;
    value source = valueOf(kwargs);
    dict_t* entries = dictStorage(source);
    if (!entries) return result;
    for (const auto& entry : *entries){
        result[engine->toString(entry.first)] = entry.second;
    }
    return result;
}

// 可调用对象统一入口: 脚本函数带关键字参数走 invoke, 其它可调用值走 callValue
// (callValue 不接受关键字参数, 原生函数/方法需在无 kwargs 时才能走这条路)
// Unified callable entry: script functions honour kwargs through invoke(),
// every other callable goes through callValue().
value callWithKeywords(vm* engine, const value& callable,
                       const std::vector<value>& positional,
                       const std::map<std::string, value>& named){
    if (callable.type == "function" && callable.ptr){
        auto* function = static_cast<function_t*>(callable.ptr);
        return engine->invoke(function, positional, nullptr, nullptr,
                              function->name, named);
    }
    return engine->callValue(callable, positional);
}

// 运算符文本归一化 (去掉空白)
std::string normalizeOperator(const char* op){
    std::string text = safeText(op);
    std::string cleaned;
    for (char c : text){
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        cleaned.push_back(c);
    }
    return cleaned;
}



// 装载到解释器的库副本: vm::loadlib 内部会再拷贝一份, 这里保存的是"喂给它的源",
// 必须常驻到解释器销毁之后 (指向其中的槽位会被 vm 拷贝使用)。
// Libraries handed to vm::loadlib must outlive the interpreter.
std::deque<ffilib>& pendingLibraries(){
    static std::deque<ffilib> list;
    return list;
}

// 持久化库/类: 原生对象内部持有裸指针, 必须保证其生命周期足够长
std::deque<ffilib*>& persistentLibraries(){
    static std::deque<ffilib*> list;
    return list;
}

std::deque<ffi_class*>& persistentClasses(){
    static std::deque<ffi_class*> list;
    return list;
}

// 取得库的常驻副本 (同名库复用同一份)
ffilib* persistentLibrary(const ffilib& source){
    auto& list = persistentLibraries();
    for (auto* item : list){
        if (item->name == source.name) return item;
    }
    auto* copy = new ffilib(source);
    list.push_back(copy);
    return copy;
}

// 取得类的常驻副本 (挂在指定库下)
ffi_class* persistentClass(ffilib& library, const ffi_class& source){
    auto it = library.classes.find(source.name);
    if (it != library.classes.end()) it->second = source;
    else library.classes[source.name] = source;
    it = library.classes.find(source.name);
    return &it->second;
}

} // namespace

} // namespace minipy

// ================= 1. 解释器 / VM =================
using namespace minipy;

extern "C"{

void* mp_vm_new(void){
    auto* box = new (std::nothrow) VmBox();
    if (!box) return nullptr;
    try{
        box->engine = new vm();
    } catch (...){
        delete box;
        return nullptr;
    }
    return box;
}

void mp_vm_free(void* handle){
    VmBox* box = vmBox(handle);
    if (!box) return;
    if (!box->owned){
        // thread_local 盒子, 只清空 engine 指针不删除
        box->engine = nullptr;
        return;
    }
    try{
        minipyCleanupAtexit(*box->engine);
        minipyCleanupContextvars(*box->engine);
        minipyCleanupWarnings(*box->engine);
        minipyCleanupLogging(*box->engine);
        delete box->engine;
    } catch (...){}
    delete box;
}

int mp_vm_use_stdlib(void* handle){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    bool ok = guardRun(engine, [&]{
        engine->usestdlib();
        ensureGlobalScope(engine);
    });
    return finish(engine, ok);
}

int mp_vm_add_module(void* handle, const char* name, const char* source){
    vm* engine = engineOf(handle);
    if (!engine || !name || !*name) return MP_ERROR;
    bool ok = guardRun(engine, [&]{
        script_t compiled = engine->compile(safeText(source));
        if (engine->hasError()) return;
        engine->addScriptLibrary(name, compiled);
    });
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_vm_load_pyd(void* handle, const char* name,
                   const unsigned char* data, size_t size){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    std::vector<unsigned char> bytes;
    if (data && size > 0) bytes.assign(data, data + size);
    bool ok = guardRun(engine, [&]{
        engine->loadpyd(safeText(name), bytes);
    });
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_vm_load_source(void* handle, const char* source){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    bool ok = guardRun(engine, [&]{
        script_t compiled = engine->compile(safeText(source));
        if (engine->hasError()) return;
        engine->setBytecode(std::move(compiled));
    });
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_vm_load_bytecode(void* handle, const unsigned char* data, size_t size){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    std::vector<unsigned char> bytes;
    if (data && size > 0) bytes.assign(data, data + size);
    bool ok = guardRun(engine, [&]{
        engine->setBytecode(bytes);
    });
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_vm_compile_bytecode(void* handle, const char* source,
                           unsigned char** out, size_t* out_size){
    if (!out || !out_size) return MP_ERROR;
    *out = nullptr;
    *out_size = 0;
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;

    std::vector<unsigned char> bytes;
    bool ok = guardRun(engine, [&]{
        bytes = engine->compileBytecode(safeText(source));
    });
    if (ok && engine->hasError()) ok = false;
    if (!ok || bytes.empty()) return finish(engine, false);

    auto* buffer = static_cast<unsigned char*>(std::malloc(bytes.size()));
    if (!buffer) return MP_ERROR;
    std::memcpy(buffer, bytes.data(), bytes.size());
    *out = buffer;
    *out_size = bytes.size();
    return MP_OK;
}

int mp_vm_run(void* handle){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    bool ok = guardRun(engine, [&]{
        ensureGlobalScope(engine);
        engine->run();
    });
    if (ok && engine->hasError()) ok = false;
    if (ok){
        // 解释器退出: 执行 atexit 注册的回调 (输出在 OK 之前)
        // Interpreter exit: run atexit callbacks (output stays before OK)
        // ᠭᠠᠷᠤᠯᠳᠤ atexit ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
        minipyRunAtexit(*engine);
        if (engine->hasError()) engine->clearErr();
    }
    return finish(engine, ok);
}

int mp_vm_exec_source(void* handle, const char* source){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    bool ok = guardRun(engine, [&]{
        ensureGlobalScope(engine);
        engine->execSource(safeText(source));
    });
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_vm_exec_main(void* handle, const char* source){
    int status = mp_vm_load_source(handle, source);
    if (status != MP_OK) return status;
    return mp_vm_run(handle);
}

// ---- 错误状态 / Error state ----
const char* mp_vm_last_error(void* handle){
    VmBox* box = vmBox(handle);
    if (!box || !box->engine) return "";
    box->lastError = box->engine->getLastErr();
    return box->lastError.c_str();
}

int mp_vm_has_error(void* handle){
    vm* engine = engineOf(handle);
    if (!engine) return 0;
    return engine->hasError() ? 1 : 0;
}

const char* mp_vm_exception_type(void* handle){
    VmBox* box = vmBox(handle);
    if (!box || !box->engine) return "";
    box->exceptionType = box->engine->exceptionType();
    return box->exceptionType.c_str();
}

void mp_vm_clear_error(void* handle){
    vm* engine = engineOf(handle);
    if (!engine) return;
    engine->clearErr();
}

void mp_vm_raise(void* handle, const char* type, const char* message){
    vm* engine = engineOf(handle);
    if (!engine) return;
    engine->raiseError(safeText(type), safeText(message));
}

void* mp_vm_save_error(void* handle){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    auto* saved = new ErrorBox();
    saved->snapshot = engine->saveErrorState();
    return saved;
}

void mp_vm_restore_error(void* handle, void* state){
    vm* engine = engineOf(handle);
    auto* saved = static_cast<ErrorBox*>(state);
    if (!engine || !saved) return;
    engine->restoreErrorState(saved->snapshot);
}

void mp_error_state_free(void* state){
    delete static_cast<ErrorBox*>(state);
}

// ---- 全局变量 / Globals ----
void* mp_vm_get_global(void* handle, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    value* found = engine->findVariablePtr(name);
    if (!found) return nullptr;
    return wrapValue(engine, *found);
}

int mp_vm_set_global(void* handle, const char* name, void* item){
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    value v = valueOf(item);
    bool ok = guardRun(engine, [&]{
        ensureGlobalScope(engine);
        engine->setVariable(name, v);
    });
    return finish(engine, ok);
}

int mp_vm_del_global(void* handle, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    if (!engine->findVariablePtr(name)) return MP_NOT_FOUND;
    bool ok = guardRun(engine, [&]{
        engine->removeVariable(name);
    });
    return finish(engine, ok);
}

int mp_vm_global_names(void* handle, char*** names, size_t* count){
    if (!names || !count) return MP_ERROR;
    *names = nullptr;
    *count = 0;
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;

    std::vector<std::string> collected;
    bool ok = guardRun(engine, [&]{
        collected = engine->collectScopeNames();
    });
    if (!ok) return MP_ERROR;
    return allocateNames(collected, names, count);
}

void mp_free_names(char** names, size_t count){
    if (!names) return;
    for (size_t i = 0; i < count; ++i) std::free(names[i]);
    std::free(names);
}

// ---- 反射 / Introspection ----
int mp_vm_find_class(void* handle, const char* name, void** out){
    if (!out) return MP_ERROR;
    *out = nullptr;
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    std::string wanted = safeText(name);

    // 优先从运行时全局作用域取 (脚本执行后定义的类)
    value* found = engine->findVariablePtr(wanted);
    if (found && found->type == "class" && found->ptr){
        *out = wrapValue(engine, *found);
        return MP_OK;
    }
    // 兼容已编译但未执行的脚本
    for (auto& entry : engine->script.classes){
        if (entry.name == wanted){
            // 脚本类由 script_t 持有, 这里只返回一个非持有句柄
            *out = wrapValue(engine, value("class", &entry, nullptr));
            return MP_OK;
        }
    }
    return MP_NOT_FOUND;
}

int mp_vm_find_function(void* handle, const char* name, void** out){
    if (!out) return MP_ERROR;
    *out = nullptr;
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    std::string wanted = safeText(name);

    value* found = engine->findVariablePtr(wanted);
    if (found && found->type == "function" && found->ptr){
        *out = wrapValue(engine, *found);
        return MP_OK;
    }
    for (auto& entry : engine->script.functions){
        if (entry.name == wanted){
            *out = wrapValue(engine, value("function", &entry, nullptr));
            return MP_OK;
        }
    }
    return MP_NOT_FOUND;
}

int mp_vm_has_lib(void* handle, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return 0;
    std::string wanted = safeText(name);
    if (engine->findCffiLibrary(wanted)) return 1;
    return engine->findScriptLibrary(wanted) ? 1 : 0;
}

int mp_vm_heap_count(void* handle, long long* out_count){
    if (!out_count) return MP_ERROR;
    *out_count = 0;
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    *out_count = engine->heapObjectCount();
    return MP_OK;
}

int mp_vm_heap_snapshot(void* handle, void*** out, size_t* out_count){
    if (!out || !out_count) return MP_ERROR;
    *out = nullptr;
    *out_count = 0;
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;

    std::vector<value> items;
    bool ok = guardRun(engine, [&]{
        items = engine->heapSnapshot();
    });
    if (!ok) return MP_ERROR;

    size_t total = items.size();
    auto** list = static_cast<void**>(std::calloc(total ? total : 1, sizeof(void*)));
    if (!list) return MP_ERROR;
    for (size_t i = 0; i < total; ++i) list[i] = wrapValue(engine, items[i]);
    *out = list;
    *out_count = total;
    return MP_OK;
}

// ================= 2. 构造值 / Value builders =================
void* mp_none(void* handle){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeNone()) : nullptr;
}

void* mp_int(void* handle, long long v){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeInt(v)) : nullptr;
}

void* mp_float(void* handle, double v){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeFloat(v)) : nullptr;
}

void* mp_bool(void* handle, int v){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeBool(v != 0)) : nullptr;
}

void* mp_str(void* handle, const char* utf8){
    return mp_str_n(handle, utf8, utf8 ? std::strlen(utf8) : 0);
}

void* mp_str_n(void* handle, const char* utf8, size_t len){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    std::string text = utf8 ? std::string(utf8, len) : std::string();
    return wrapValue(engine, engine->makeString(text));
}

void* mp_list(void* handle){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeList({})) : nullptr;
}

void* mp_tuple(void* handle){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeTuple({})) : nullptr;
}

void* mp_dict(void* handle){
    vm* engine = engineOf(handle);
    return engine ? wrapValue(engine, engine->makeDict()) : nullptr;
}

void* mp_range(void* handle, long long start, long long stop, long long step){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    if (step == 0) step = 1;
    return wrapValue(engine, engine->makeRange(start, stop, step));
}

void* mp_dict_items(void* handle, void** keys, void** values, size_t count){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    std::vector<std::pair<value, value>> items;
    items.reserve(count);
    for (size_t i = 0; i < count; ++i){
        value k = (keys && keys[i]) ? valueOf(keys[i]) : engine->makeNone();
        value v = (values && values[i]) ? valueOf(values[i]) : engine->makeNone();
        items.emplace_back(std::move(k), std::move(v));
    }
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeDict(items);
    });
    return wrapValue(engine, result);
}

void* mp_dict_view(void* handle, void* dict, int kind){
    vm* engine = engineOf(handle);
    if (!engine || kind < 0 || kind > 2) return nullptr;
    value target = valueOf(dict);
    if (!dictStorage(target)) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeDictView(target, kind);
    });
    return wrapValue(engine, result);
}

void* mp_complex(void* handle, double real, double imag){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeComplex(real, imag);
    });
    return wrapValue(engine, result);
}

void* mp_exception(void* handle, const char* type, const char* message){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeExceptionObject(safeText(type), safeText(message), value());
    });
    return wrapValue(engine, result);
}

void* mp_super(void* handle, void* klass, void* obj){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value klassVal = klass ? valueOf(klass) : value();
    value objVal = obj ? valueOf(obj) : value();
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeSuper(klass ? &klassVal : nullptr,
                                   obj ? &objVal : nullptr);
    });
    return wrapValue(engine, result);
}

void* mp_make_object(void* handle, void* klass){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value classValue = valueOf(klass);
    if (classValue.type != "class" || !classValue.ptr) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeObject(static_cast<class_t*>(classValue.ptr));
    });
    return wrapValue(engine, result);
}

void* mp_create_class(void* handle, const char* name, void** bases,
                      size_t base_count, void* namespace_dict){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    std::vector<value> baseValues = collectArgs(bases, base_count);
    value namespaceValue = valueOf(namespace_dict);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->createDynamicClass(safeText(name), baseValues, namespaceValue);
    });
    return wrapValue(engine, result);
}

void* mp_bind_method(void* handle, void* obj, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    value target = valueOf(obj);
    function_t* method = engine->findObjectMethod(target, name);
    if (!method) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->makeBoundFunction(method, target);
    });
    return wrapValue(engine, result);
}

// ---- 容器原地修改 / In-place container mutation ----
int mp_seq_len(void* seq, size_t* out_len){
    if (!out_len) return MP_ERROR;
    *out_len = 0;
    std::vector<value>* items = seqStorage(valueOf(seq));
    if (!items) return MP_TYPE_ERROR;
    *out_len = items->size();
    return MP_OK;
}

int mp_seq_get(void* seq, size_t index, void** out){
    if (!out) return MP_ERROR;
    *out = nullptr;
    value container = valueOf(seq);
    std::vector<value>* items = seqStorage(container);
    if (!items) return MP_TYPE_ERROR;
    if (index >= items->size()) return MP_NOT_FOUND;
    ValueBox* box = valueBox(seq);
    *out = wrapValue(box ? box->owner : nullptr, (*items)[index]);
    return MP_OK;
}

int mp_seq_set(void* seq, size_t index, void* item){
    value container = valueOf(seq);
    std::vector<value>* items = seqStorage(container);
    if (!items) return MP_TYPE_ERROR;
    if (container.type != "list") return MP_TYPE_ERROR;
    if (index >= items->size()) return MP_NOT_FOUND;
    (*items)[index] = valueOf(item);
    return MP_OK;
}

int mp_seq_append(void* seq, void* item){
    value container = valueOf(seq);
    std::vector<value>* items = seqStorage(container);
    if (!items || container.type != "list") return MP_TYPE_ERROR;
    items->push_back(valueOf(item));
    return MP_OK;
}

int mp_seq_insert(void* seq, size_t index, void* item){
    value container = valueOf(seq);
    std::vector<value>* items = seqStorage(container);
    if (!items || container.type != "list") return MP_TYPE_ERROR;
    if (index > items->size()) index = items->size();
    items->insert(items->begin() + static_cast<std::ptrdiff_t>(index), valueOf(item));
    return MP_OK;
}

int mp_dict_len(void* dict, size_t* out_len){
    if (!out_len) return MP_ERROR;
    *out_len = 0;
    dict_t* entries = dictStorage(valueOf(dict));
    if (!entries) return MP_TYPE_ERROR;
    *out_len = entries->size();
    return MP_OK;
}

int mp_dict_get(void* dict, void* key, void** out){
    if (!out) return MP_ERROR;
    *out = nullptr;
    value target = valueOf(dict);
    if (!dictStorage(target)) return MP_TYPE_ERROR;
    ValueBox* box = valueBox(dict);
    vm* engine = box ? box->owner : nullptr;
    if (!engine) return MP_ERROR;
    value* found = engine->dictFind(target, valueOf(key));
    if (!found) return MP_NOT_FOUND;
    *out = wrapValue(engine, *found);
    return MP_OK;
}

int mp_dict_set(void* dict, void* key, void* item){
    value target = valueOf(dict);
    if (!dictStorage(target)) return MP_TYPE_ERROR;
    ValueBox* box = valueBox(dict);
    vm* engine = box ? box->owner : nullptr;
    if (!engine) return MP_ERROR;
    std::string error = engine->dictSet(target, valueOf(key), valueOf(item));
    return error.empty() ? MP_OK : MP_TYPE_ERROR;
}

int mp_dict_del(void* dict, void* key){
    value target = valueOf(dict);
    if (!dictStorage(target)) return MP_TYPE_ERROR;
    ValueBox* box = valueBox(dict);
    vm* engine = box ? box->owner : nullptr;
    if (!engine) return MP_ERROR;
    return engine->dictRemove(target, valueOf(key)) ? MP_OK : MP_NOT_FOUND;
}

int mp_dict_get_index(void* dict, size_t index, void** out_key, void** out_value){
    if (!out_key && !out_value) return MP_ERROR;
    if (out_key) *out_key = nullptr;
    if (out_value) *out_value = nullptr;
    value target = valueOf(dict);
    dict_t* storage = dictStorage(target);
    if (!storage) return MP_TYPE_ERROR;
    ValueBox* box = valueBox(dict);
    vm* engine = box ? box->owner : nullptr;
    if (!engine) return MP_ERROR;
    if (index >= storage->size()) return MP_NOT_FOUND;
    auto& pair = (*storage)[index];
    if (out_key) *out_key = wrapValue(engine, pair.first);
    if (out_value) *out_value = wrapValue(engine, pair.second);
    return MP_OK;
}

// ---- 引用计数 / Refcount ----
void* mp_value_retain(void* value){
    ValueBox* box = valueBox(value);
    if (!box) return nullptr;
    // 句柄级 refcount: 防止 ValueBox 被提前销毁
    ++box->refcount;
    return value;
}

void mp_value_release(void* value){
    ValueBox* box = valueBox(value);
    if (!box) return;
    if (--box->refcount <= 0) delete box;
}

// ================= 3. 读取值 / Value readers =================
int mp_value_kind(void* value){
    if (!value) return MP_KIND_NONE;
    return kindOf(valueOf(value));
}

const char* mp_value_type_name(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return "";
    box->typeName = box->owner->pythonTypeName(box->v);
    return box->typeName.c_str();
}

long long mp_value_as_int(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return 0;
    return box->owner->toInt(box->v);
}

double mp_value_as_float(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return 0.0;
    return box->owner->toFloat(box->v);
}

int mp_value_as_bool(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return 0;
    return box->owner->truthy(box->v) ? 1 : 0;
}

const char* mp_value_as_str(void* value){
    size_t ignored = 0;
    return mp_value_as_str_n(value, &ignored);
}

const char* mp_value_as_str_n(void* value, size_t* out_len){
    if (out_len) *out_len = 0;
    ValueBox* box = valueBox(value);
    if (!box || box->v.type != "str" || !box->v.ptr) return nullptr;
    auto* text = static_cast<std::string*>(box->v.ptr);
    if (out_len) *out_len = text->size();
    return text->c_str();
}

int mp_value_is_complex(void* value){
    ValueBox* box = valueBox(value);
    if (!box) return 0;
    return isComplexValueEx(box->v) ? 1 : 0;
}

int mp_value_as_complex(void* value, double* real, double* imag){
    if (real) *real = 0.0;
    if (imag) *imag = 0.0;
    ValueBox* box = valueBox(value);
    if (!box || !isComplexValueEx(box->v)) return 0;
    double re = 0.0, im = 0.0;
    if (!complexPartsEx(box->v, re, im)) return 0;
    if (real) *real = re;
    if (imag) *imag = im;
    return 1;
}

const char* mp_value_as_bytes(void* value, size_t* out_len){
    if (out_len) *out_len = 0;
    ValueBox* box = valueBox(value);
    if (!box || (box->v.type != "bytes" && box->v.type != "bytearray") || !box->v.ptr)
        return nullptr;
    auto* text = static_cast<std::string*>(box->v.ptr);
    if (out_len) *out_len = text->size();
    return text->c_str();
}

void* mp_value_class(void* val){
    ValueBox* box = valueBox(val);
    if (!box || !box->owner) return nullptr;
    if (box->v.type == "class" && box->v.ptr)
        return wrapValue(box->owner, value("class", box->v.ptr, nullptr));
    class_t* klass = box->owner->getClassOfScriptObject(box->v);
    if (klass) return wrapValue(box->owner, value("class", klass, nullptr));

    // 内置值: 回退到对应的内置类型对象 (native_object)
    std::string typeName = box->owner->pythonTypeName(box->v);
    value typeObj = box->owner->getBuiltinTypeObj(typeName);
    if (typeObj.type == "none" || !typeObj.ptr) return nullptr;
    return wrapValue(box->owner, typeObj);
}

int mp_value_dir_names(void* value, char*** names, size_t* count){
    if (!names || !count) return MP_ERROR;
    *names = nullptr;
    *count = 0;
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return MP_ERROR;
    vm* engine = box->owner;
    std::vector<std::string> collected;
    bool ok = guardRun(engine, [&]{
        collected = engine->collectDirNames(box->v);
    });
    if (!ok) return MP_ERROR;
    return allocateNames(collected, names, count);
}

void* mp_value_container(void* val){
    ValueBox* box = valueBox(val);
    if (!box || !box->owner) return nullptr;
    value result = box->owner->containerOf(box->v);
    if (result.type == "none" || !result.ptr) return nullptr;
    return wrapValue(box->owner, result);
}

int mp_value_equal(void* a, void* b){
    ValueBox* left = valueBox(a);
    if (!left || !left->owner) return 0;
    return left->owner->valuesEqual(left->v, valueOf(b)) ? 1 : 0;
}

int mp_value_compare(void* a, void* b){
    ValueBox* left = valueBox(a);
    if (!left || !left->owner) return 0;
    vm* engine = left->owner;
    value right = valueOf(b);

    int leftKind = kindOf(left->v);
    int rightKind = kindOf(right);
    bool leftNum = (leftKind == MP_KIND_INT || leftKind == MP_KIND_FLOAT);
    bool rightNum = (rightKind == MP_KIND_INT || rightKind == MP_KIND_FLOAT);
    if (leftNum && rightNum){
        if (leftKind == MP_KIND_INT && rightKind == MP_KIND_INT){
            long long l = engine->toInt(left->v);
            long long r = engine->toInt(right);
            return (l < r) ? -1 : ((l > r) ? 1 : 0);
        }
        double l = engine->toFloat(left->v);
        double r = engine->toFloat(right);
        return (l < r) ? -1 : ((l > r) ? 1 : 0);
    }
    int cmp = engine->toString(left->v).compare(engine->toString(right));
    return (cmp < 0) ? -1 : ((cmp > 0) ? 1 : 0);
}

long long mp_value_hash(void* value, int* hashable){
    if (hashable) *hashable = 0;
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return 0;
    if (!box->owner->hashable(box->v)) return 0;
    if (hashable) *hashable = 1;
    return box->owner->hashKey(box->v);
}

char* mp_value_to_str(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return duplicateText("None");
    return duplicateText(box->owner->toString(box->v));
}

char* mp_value_to_repr(void* value){
    ValueBox* box = valueBox(value);
    if (!box || !box->owner) return duplicateText("None");
    return duplicateText(pythonRepr(*box->owner, box->v));
}

void mp_free(void* ptr){
    std::free(ptr);
}

// ================= 4. 调用与运算符 / Calls & operators =================
void* mp_call(void* handle, void* callee, void** args, size_t argc){
    return mp_call_kw(handle, callee, args, argc, nullptr);
}

void* mp_call_kw(void* handle, void* callee, void** args, size_t argc, void* kwargs){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value callable = valueOf(callee);
    std::vector<value> positional = collectArgs(args, argc);
    std::map<std::string, value> named = collectKwargs(engine, kwargs);

    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = callWithKeywords(engine, callable, positional, named);
    });
    return wrapValue(engine, result);
}

void* mp_call_name(void* handle, const char* name, void** args, size_t argc){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    value* found = engine->findVariablePtr(name);
    if (!found) return nullptr;

    std::vector<value> positional = collectArgs(args, argc);
    value callable = *found;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = callWithKeywords(engine, callable, positional, {});
    });
    return wrapValue(engine, result);
}

void* mp_binary(void* handle, const char* op, void* left, void* right){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    std::string token = normalizeOperator(op);
    value a = valueOf(left);
    value b = valueOf(right);

    value result = engine->makeNone();
    guardRun(engine, [&]{
        if (token == "=="){ result = engine->makeBool(engine->valuesEqual(a, b)); return; }
        if (token == "!="){ result = engine->makeBool(!engine->valuesEqual(a, b)); return; }
        if (token == "is" || token == "isnot"){
            bool same = (a.type == b.type) && (a.ptr == b.ptr);
            result = engine->makeBool(token == "is" ? same : !same);
            return;
        }
        if (token == "in" || token == "notin"){
            bool found = false;
            std::vector<value> items;
            engine->materializeIterable(b, items);
            for (const auto& item : items){
                if (engine->valuesEqual(item, a)){ found = true; break; }
            }
            if (!found && dictStorage(b)) found = engine->dictFind(b, a) != nullptr;
            result = engine->makeBool(token == "in" ? found : !found);
            return;
        }
        if (token == "<" || token == "<=" || token == ">" || token == ">="){
            bool verdict = false;
            int ak = kindOf(a);
            int bk = kindOf(b);
            bool numeric = (ak == MP_KIND_INT || ak == MP_KIND_FLOAT) &&
                           (bk == MP_KIND_INT || bk == MP_KIND_FLOAT);
            if (numeric){
                double l = engine->toFloat(a);
                double r = engine->toFloat(b);
                if (token == "<") verdict = l < r;
                else if (token == "<=") verdict = l <= r;
                else if (token == ">") verdict = l > r;
                else verdict = l >= r;
            } else {
                int cmp = engine->toString(a).compare(engine->toString(b));
                if (token == "<") verdict = cmp < 0;
                else if (token == "<=") verdict = cmp <= 0;
                else if (token == ">") verdict = cmp > 0;
                else verdict = cmp >= 0;
            }
            result = engine->makeBool(verdict);
            return;
        }
        result = engine->applyBinary(token, a, b);
    });
    return wrapValue(engine, result);
}

void* mp_unary(void* handle, const char* op, void* operand){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    std::string token = normalizeOperator(op);
    value target = valueOf(operand);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->applyUnary(token, target);
    });
    return wrapValue(engine, result);
}

void* mp_getattr(void* handle, void* obj, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    value target = valueOf(obj);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->getAttribute(target, name);
    });
    return wrapValue(engine, result);
}

int mp_setattr(void* handle, void* obj, const char* name, void* item){
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    value target = valueOf(obj);
    value v = valueOf(item);
    bool ok = guardRun(engine, [&]{
        engine->setAttribute(target, name, v);
    });
    return finish(engine, ok);
}

int mp_delattr(void* handle, void* obj, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return MP_ERROR;
    value target = valueOf(obj);
    bool ok = guardRun(engine, [&]{
        engine->deleteAttribute(target, name);
    });
    return finish(engine, ok);
}

int mp_hasattr(void* handle, void* obj, const char* name){
    vm* engine = engineOf(handle);
    if (!engine || !name) return 0;
    value target = valueOf(obj);
    return engine->hasAttribute(target, name) ? 1 : 0;
}

void* mp_getitem(void* handle, void* obj, void* key){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value target = valueOf(obj);
    value k = valueOf(key);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->indexValue(target, k);
    });
    return wrapValue(engine, result);
}

int mp_setitem(void* handle, void* obj, void* key, void* item){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    value target = valueOf(obj);
    value k = valueOf(key);
    value v = valueOf(item);
    std::string error;
    bool ok = guardRun(engine, [&]{
        error = engine->indexAssign(target, k, v);
    });
    if (!error.empty()){
        engine->clearErr();
        engine->raiseError("TypeError", error);
        ok = false;
    }
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_delitem(void* handle, void* obj, void* key){
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    value target = valueOf(obj);
    value k = valueOf(key);
    std::string error;
    bool ok = guardRun(engine, [&]{
        error = engine->indexDelete(target, k);
    });
    if (!error.empty()){
        engine->clearErr();
        engine->raiseError("KeyError", pythonRepr(*engine, k));
        ok = false;
    }
    if (ok && engine->hasError()) ok = false;
    return finish(engine, ok);
}

int mp_iterate(void* handle, void* iterable, void*** out, size_t* out_count){
    if (!out || !out_count) return MP_ERROR;
    *out = nullptr;
    *out_count = 0;
    vm* engine = engineOf(handle);
    if (!engine) return MP_ERROR;
    value target = valueOf(iterable);

    std::vector<value> items;
    bool ok = guardRun(engine, [&]{
        engine->materializeIterable(target, items);
    });
    if (!ok) return MP_ERROR;

    size_t total = items.size();
    auto** list = static_cast<void**>(std::calloc(total ? total : 1, sizeof(void*)));
    if (!list) return MP_ERROR;
    for (size_t i = 0; i < total; ++i) list[i] = wrapValue(engine, items[i]);
    *out = list;
    *out_count = total;
    return MP_OK;
}

void mp_value_array_free(void** items, size_t count){
    if (!items) return;
    for (size_t i = 0; i < count; ++i) mp_value_release(items[i]);
    std::free(items);
}

int mp_isinstance(void* handle, void* obj, void* cls){
    vm* engine = engineOf(handle);
    if (!engine) return 0;
    return engine->isInstance(valueOf(obj), valueOf(cls)) ? 1 : 0;
}

void* mp_builtin_type(void* handle, const char* type_name){
    vm* engine = engineOf(handle);
    if (!engine || !type_name) return nullptr;
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->getBuiltinTypeObj(type_name);
    });
    if (result.type == "none") return nullptr;
    return wrapValue(engine, result);
}

void* mp_call_method(void* handle, void* obj, const char* name, void** args, size_t argc){
    vm* engine = engineOf(handle);
    if (!engine || !name) return nullptr;
    value target = valueOf(obj);
    value callable = engine->makeNone();
    bool ok = guardRun(engine, [&]{
        function_t* method = engine->findObjectMethod(target, name);
        if (method) callable = engine->makeBoundFunction(method, target);
        else callable = engine->getAttribute(target, safeText(name));
    });
    if (!ok || engine->hasError()) return nullptr;

    std::vector<value> positional = collectArgs(args, argc);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = callWithKeywords(engine, callable, positional, {});
    });
    return wrapValue(engine, result);
}

void* mp_apply_format(void* handle, void* val, const char* spec){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value source = valueOf(val);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->applyFormat(source, safeText(spec));
    });
    return wrapValue(engine, result);
}

void* mp_percent_format(void* handle, void* format, void* arg){
    vm* engine = engineOf(handle);
    if (!engine) return nullptr;
    value fmt = valueOf(format);
    value argument = valueOf(arg);
    value result = engine->makeNone();
    guardRun(engine, [&]{
        result = engine->percentFormat(fmt, argument);
    });
    return wrapValue(engine, result);
}

int mp_is_subclass_of(void* handle, void* sub, const char* builtin_name){
    vm* engine = engineOf(handle);
    if (!engine || !builtin_name) return 0;
    value subValue = valueOf(sub);
    bool matched = false;
    guardRun(engine, [&]{
        matched = engine->isSubclassOfBuiltin(subValue, safeText(builtin_name));
    });
    return matched ? 1 : 0;
}

const char* mp_builtin_base_name(void* handle, void* klass){
    vm* engine = engineOf(handle);
    VmBox* box = vmBox(handle);
    if (!engine || !box) return "";
    value classValue = valueOf(klass);
    if (classValue.type != "class" || !classValue.ptr) return "";
    std::string name;
    guardRun(engine, [&]{
        name = engine->builtinBaseName(static_cast<class_t*>(classValue.ptr));
    });
    box->scratch = std::move(name);
    return box->scratch.c_str();
}

int mp_match_pattern(void* handle, void* subject, const char* pattern,
                     void** captures_out){
    if (captures_out) *captures_out = nullptr;
    vm* engine = engineOf(handle);
    if (!engine || !pattern) return MP_ERROR;

    std::vector<std::pair<std::string, value>> captures;
    bool matched = false;
    bool ok = guardRun(engine, [&]{
        matched = engine->matchPattern(valueOf(subject), safeText(pattern), captures);
    });
    if (!ok) return MP_ERROR;
    if (matched && captures_out){
        std::vector<std::pair<value, value>> items;
        items.reserve(captures.size());
        for (auto& entry : captures)
            items.emplace_back(engine->makeString(entry.first), entry.second);
        *captures_out = wrapValue(engine, engine->makeDict(items));
    }
    return matched ? 1 : 0;
}

// ================= 5. 扩展注册 / Extension (FFI) =================
void* mp_lib_new(const char* name){
    auto* box = new (std::nothrow) LibBox();
    if (!box) return nullptr;
    box->lib.name = safeText(name);
    return box;
}

void mp_lib_free(void* lib){
    LibBox* box = libBox(lib);
    if (!box) return;
    for (auto& entry : box->lib.functions) freeNode(static_cast<Node*>(entry.second));
    for (auto& entry : box->lib.data) freeNode(static_cast<Node*>(entry.second));
    for (auto& entry : box->lib.classes){
        for (auto& method : entry.second.methods) freeNode(static_cast<Node*>(method.second));
        for (auto& item : entry.second.data) freeNode(static_cast<Node*>(item.second));
    }
    delete box;
}

int mp_lib_add_function(void* lib, const char* name, void* fn, void* user){
    LibBox* box = libBox(lib);
    if (!box || !name || !fn) return MP_ERROR;
    Node* node = makeNode(fn, user, 0);
    if (!node) return MP_ERROR;
    box->lib.functions[name] = node;
    return MP_OK;
}

int mp_lib_add_data(void* lib, const char* name, void* getter, void* user){
    LibBox* box = libBox(lib);
    if (!box || !name || !getter) return MP_ERROR;
    Node* node = makeNode(getter, user, 1);
    if (!node) return MP_ERROR;
    box->lib.data[name] = node;
    return MP_OK;
}

int mp_lib_add_const(void* lib, const char* name, void* item){
    LibBox* box = libBox(lib);
    if (!box || !name) return MP_ERROR;
    Node* node = makeNode(nullptr, nullptr, 2);
    if (!node) return MP_ERROR;
    node->constant = valueOf(item);
    box->lib.data[name] = node;
    return MP_OK;
}

void* mp_class_new(const char* name){
    auto* box = new (std::nothrow) ClassBox();
    if (!box) return nullptr;
    box->klass.name = safeText(name);
    return box;
}

void mp_class_free(void* klass){
    ClassBox* box = classBox(klass);
    if (!box) return;
    for (auto& method : box->klass.methods) freeNode(static_cast<Node*>(method.second));
    for (auto& item : box->klass.data) freeNode(static_cast<Node*>(item.second));
    delete box;
}

int mp_class_add_method(void* klass, const char* name, void* fn, void* user){
    ClassBox* box = classBox(klass);
    if (!box || !name || !fn) return MP_ERROR;
    Node* node = makeNode(fn, user, 0);
    if (!node) return MP_ERROR;
    box->klass.methods[name] = node;
    return MP_OK;
}

int mp_class_add_data(void* klass, const char* name, void* getter, void* user){
    ClassBox* box = classBox(klass);
    if (!box || !name || !getter) return MP_ERROR;
    Node* node = makeNode(getter, user, 1);
    if (!node) return MP_ERROR;
    box->klass.data[name] = node;
    return MP_OK;
}

int mp_lib_add_class(void* lib, const char* name, void* klass){
    LibBox* libHandle = libBox(lib);
    ClassBox* classHandle = classBox(klass);
    if (!libHandle || !classHandle || !name) return MP_ERROR;
    libHandle->lib.classes[name] = classHandle->klass;
    return MP_OK;
}

int mp_vm_load_lib(void* handle, void* lib){
    vm* engine = engineOf(handle);
    LibBox* box = libBox(lib);
    if (!engine || !box) return MP_ERROR;

    bool ok = guardRun(engine, [&]{
        ffilib prepared = box->lib;

        // 每个槽位里的 Node 都分配一个唯一可调用的入口函数指针,
        // vm 直接调用该入口, 入口按自身索引反查 Node。
        // Every Node slot gets a unique callable entry pointer; the vm calls
        // that entry directly and it resolves the Node by its own index.
        bool exhausted = false;
        auto prepareSlot = [&](void*& slot){
            if (exhausted) return;
            auto* node = static_cast<Node*>(slot);
            if (!node) return;
            void* entry = registerEntry(node);
            if (!entry){
                exhausted = true;
                return;
            }
            slot = entry;
        };
        for (auto& item : prepared.functions) prepareSlot(item.second);
        for (auto& item : prepared.data) prepareSlot(item.second);
        for (auto& cls : prepared.classes){
            for (auto& method : cls.second.methods) prepareSlot(method.second);
            for (auto& item : cls.second.data) prepareSlot(item.second);
        }
        if (exhausted){
            engine->raiseError("RuntimeError", "native callback entry pool exhausted");
            return;
        }
        pendingLibraries().push_back(prepared);
        engine->loadlib(pendingLibraries().back());
    });
    return finish(engine, ok);
}

void* mp_make_native(void* handle, void* lib, void* klass, void* nativeHandle){
    vm* engine = engineOf(handle);
    LibBox* libHandle = libBox(lib);
    ClassBox* classHandle = classBox(klass);
    if (!engine || !libHandle) return nullptr;

    ffilib* library = persistentLibrary(libHandle->lib);
    const ffi_class* classPtr = nullptr;
    if (classHandle) classPtr = persistentClass(*library, classHandle->klass);

    value result = engine->makeNativeObject(library, classPtr, nativeHandle);
    return wrapValue(engine, result);
}

void* mp_value_native_handle(void* value){
    ValueBox* box = valueBox(value);
    if (!box || box->v.type != "native_object" || !box->v.ptr) return nullptr;
    auto* obj = static_cast<ffi_instance*>(box->v.ptr);
    return obj ? obj->handle : nullptr;
}

int mp_value_native_set(void* value, const char* name, void* item){
    ValueBox* box = valueBox(value);
    if (!box || !name) return MP_ERROR;
    if (box->v.type != "native_object" || !box->v.ptr) return MP_TYPE_ERROR;
    auto* obj = static_cast<ffi_instance*>(box->v.ptr);
    obj->attributes[name] = valueOf(item);
    return MP_OK;
}

void* mp_value_native_get(void* value, const char* name){
    ValueBox* box = valueBox(value);
    if (!box || !name) return nullptr;
    if (box->v.type != "native_object" || !box->v.ptr) return nullptr;
    auto* obj = static_cast<ffi_instance*>(box->v.ptr);
    auto it = obj->attributes.find(name);
    if (it == obj->attributes.end()) return nullptr;
    return wrapValue(box->owner, it->second);
}

// ================= 6. 原生回调上下文 / Native ctx =================
size_t mp_ctx_argc(void* ctx){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->argv) return 0;
    return box->argv->size();
}

void* mp_ctx_arg(void* ctx, size_t index){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->argv || index >= box->argv->size()) return nullptr;
    return wrapValue(box->engine, (*box->argv)[index]);
}

void* mp_ctx_kwarg(void* ctx, const char* name){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->kwargv || !name) return nullptr;
    auto it = box->kwargv->find(name);
    if (it == box->kwargv->end()) return nullptr;
    return wrapValue(box->engine, it->second);
}

int mp_ctx_has_kwarg(void* ctx, const char* name){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->kwargv || !name) return 0;
    return box->kwargv->find(name) != box->kwargv->end() ? 1 : 0;
}

void* mp_ctx_vm(void* ctx){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->engine) return nullptr;
    // 使用 thread_local 盒子避免每次 new 的内存泄漏
    // Use thread_local to avoid per-call heap allocation
    thread_local VmBox tlBox;
    tlBox.engine = box->engine;
    tlBox.owned = false;
    return &tlBox;
}

void* mp_ctx_self(void* ctx){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->self || !box->engine) return nullptr;
    return wrapValue(box->engine, *box->self);
}

void mp_ctx_error(void* ctx, const char* type, const char* message){
    CtxBox* box = ctxBox(ctx);
    if (!box) return;
    box->failed = true;
    box->errorType = safeText(type);
    box->errorMessage = safeText(message);
    if (box->engine){
        std::string kind = box->errorType.empty() ? std::string("RuntimeError") : box->errorType;
        box->engine->raiseError(kind, box->errorMessage);
    }
}

void* mp_ctx_make_native(void* ctx, void* nativeHandle){
    CtxBox* box = ctxBox(ctx);
    if (!box || !box->engine) return nullptr;
    value result = box->engine->makeNativeObject(box->library, box->klass, nativeHandle);
    return wrapValue(box->engine, result);
}

} // extern "C"
