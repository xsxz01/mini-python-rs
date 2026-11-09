#include "../cffi.hpp"

#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 退出回调记录
// Exit callback record
// ᠭᠠᠷᠤᠯᠳᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠴᠢᠭ
struct ExitEntry{
    value func;
    std::vector<value> args;
    std::map<std::string, value> kwargs;
};

// 按 VM 分组的回调表
// Per-VM callback registry
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ ᠪᠡᠷ ᠬᠠᠷᠢᠶ᠎ᠠ
std::map<vm*, std::vector<ExitEntry>>& registry(){
    static std::map<vm*, std::vector<ExitEntry>> table;
    return table;
}

// 以可能带关键字参数的方式调用可调用值
// Invoke a callable, passing keyword arguments when supported
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠲᠡᠢ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
value callExitCallable(vm& v, const value& fn, const std::vector<value>& args,
                       const std::map<std::string, value>& kwargs){
    if (fn.type == "function" && fn.ptr){
        auto* f = static_cast<function_t*>(fn.ptr);
        std::string name = f->name.empty() ? "<atexit>" : f->name;
        return v.invoke(f, args, nullptr, &v.script, name, kwargs);
    }
    return v.callValue(fn, args);
}

}

// atexit.register 实现
// atexit.register
// atexit.register
static value fnRegister(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "register expected at least 1 argument, got 0");
        return a.none();
    }
    value fn = a.arg(0);
    if (!(fn.type == "function" || fn.type == "native_object" ||
          fn.type == "native_method" || fn.type == "bound_function" ||
          fn.type == "object" || fn.type == "class")){
        a.vm->raiseError("TypeError", "'" + a.vm->pythonTypeName(fn) +
                        "' object is not callable");
        return a.none();
    }
    ExitEntry entry;
    entry.func = fn;
    for (size_t i = 1; i < a.argc(); ++i) entry.args.push_back(a.arg(i));
    entry.kwargs = a.kwargv;
    registry()[a.vm].push_back(entry);
    return fn;
}

// atexit.unregister 实现
// atexit.unregister
// atexit.unregister
static value fnUnregister(cffi& a){
    value fn = a.arg(0);
    auto it = registry().find(a.vm);
    if (it != registry().end()){
        auto& entries = it->second;
        for (size_t i = entries.size(); i-- > 0;){
            bool same = entries[i].func.ptr == fn.ptr &&
                        entries[i].func.type == fn.type;
            if (same) entries.erase(entries.begin() + static_cast<long>(i));
        }
        if (entries.empty()) registry().erase(it);
    }
    return fn;
}

// atexit._clear 实现
// atexit._clear
// atexit._clear
static value fnClear(cffi& a){
    registry().erase(a.vm);
    return a.none();
}

// atexit._run_exitfuncs 实现 (LIFO 顺序)
// atexit._run_exitfuncs (LIFO order)
// atexit._run_exitfuncs
static value fnRunExitfuncs(cffi& a){
    auto it = registry().find(a.vm);
    if (it == registry().end()) return a.none();
    std::vector<ExitEntry> entries;
    entries.swap(it->second);
    registry().erase(it);
    for (size_t i = entries.size(); i-- > 0;){
        value ignored = callExitCallable(*a.vm, entries[i].func,
                                          entries[i].args, entries[i].kwargs);
        if (a.vm->hasError()) a.vm->clearErr();
    }
    return a.none();
}

// data ncallbacks
// data ncallbacks
// data ncallbacks
static value dataNcallbacks(cffi& a){
    auto it = registry().find(a.vm);
    if (it == registry().end()) return a.mkint(0);
    return a.mkint(static_cast<long long>(it->second.size()));
}

// cffi atexit lib
// cffi atexit lib
// cffi atexit lib
ffilib cffi_atexit_lib(){
    return cffi_lib(
        "atexit",
        {
            {"register", (void*)fnRegister},
            {"unregister", (void*)fnUnregister},
            {"_clear", (void*)fnClear},
            {"_run_exitfuncs", (void*)fnRunExitfuncs}
        },
        {
            {"ncallbacks", (void*)dataNcallbacks}
        });
}

// VM 结束前执行全部退出回调
// Run all exit callbacks before VM shutdown
// VM ᠭᠠᠷᠤᠯᠳᠤ ᠡᠮᠦᠨ᠎ᠡ ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
void minipyRunAtexit(vm& v){
    auto it = registry().find(&v);
    if (it == registry().end()) return;
    std::vector<ExitEntry> entries;
    entries.swap(it->second);
    registry().erase(it);
    for (size_t i = entries.size(); i-- > 0;){
        value ignored = callExitCallable(v, entries[i].func,
                                          entries[i].args, entries[i].kwargs);
        if (v.hasError()) v.clearErr();
    }
}

// 释放某个 VM 的回调表 (避免 value 引用悬空)
// Drop a VM's callback registry (avoid dangling value refs)
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ ᠪᠠᠶᠢᠮᠠᠭ᠎ᠠ
void minipyCleanupAtexit(vm& v){
    registry().erase(&v);
}

}
