#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {

namespace {

// 全局 gc 状态 (按 VM 区分启用标志与调试标志)
// Per-VM gc state (enabled flag / debug flags)
// gc ᠪᠠᠶᠢᠳᠠᠯ
struct GcState{
    bool enabled = true;
    long long debug = 0;
    long long threshold0 = 2000;
    long long threshold1 = 10;
    long long threshold2 = 10;
};

std::map<vm*, GcState>& gcStates(){
    static std::map<vm*, GcState> table;
    return table;
}

GcState& stateOf(vm* v){
    return gcStates()[v];
}

}

// gc.enable
// gc.enable
// gc.enable
static value fnEnable(cffi& a){
    stateOf(a.vm).enabled = true;
    return a.none();
}

// gc.disable
// gc.disable
// gc.disable
static value fnDisable(cffi& a){
    stateOf(a.vm).enabled = false;
    return a.none();
}

// gc.isenabled
// gc.isenabled
// gc.isenabled
static value fnIsenabled(cffi& a){
    return a.mkbool(stateOf(a.vm).enabled);
}

// gc.collect (引用计数 VM 不做环回收, 返回 0)
// gc.collect (refcount VM has no cycle collector, always 0)
// gc.collect
static value fnCollect(cffi& a){
    if (a.argc() > 0 && a.arg(0).type != "none"){
        long long generation = a.arg_int(0);
        if (generation < 0 || generation > 2){
            a.vm->raiseError("ValueError", "invalid generation " +
                            std::to_string(generation));
            return a.none();
        }
    }
    return a.mkint(0);
}

// gc.get_count
// gc.get_count
// gc.get_count
static value fnGetCount(cffi& a){
    return a.vm->makeTuple({a.mkint(0), a.mkint(0), a.mkint(0)});
}

// gc.get_stats
// gc.get_stats
// gc.get_stats
static value fnGetStats(cffi& a){
    std::vector<value> gens;
    for (int i = 0; i < 3; ++i){
        std::vector<std::pair<value, value>> items;
        items.push_back({a.mkstr("collections"), a.mkint(0)});
        items.push_back({a.mkstr("collected"), a.mkint(0)});
        items.push_back({a.mkstr("uncollectable"), a.mkint(0)});
        gens.push_back(a.mkdict(items));
    }
    return a.mklist(gens);
}

// gc.get_objects (枚举 VM 堆对象表)
// gc.get_objects (enumerate the VM heap registry)
// gc.get_objects
static value fnGetObjects(cffi& a){
    std::vector<value> objects = a.vm->heapSnapshot();
    return a.mklist(objects);
}

// gc.get_referrers (引用计数模型无环追踪, 返回空列表)
// gc.get_referrers (no cycle tracking, empty list)
// gc.get_referrers
static value fnGetReferrers(cffi& a){
    return a.mklist({});
}

// gc.get_referents
// gc.get_referents
// gc.get_referents
static value fnGetReferents(cffi& a){
    return a.mklist({});
}

// gc.is_tracked
// gc.is_tracked
// gc.is_tracked
static value fnIsTracked(cffi& a){
    value obj = a.arg(0);
    bool tracked = !(obj.type == "int" || obj.type == "float" ||
                     obj.type == "bool" || obj.type == "none" ||
                     obj.type == "str");
    return a.mkbool(tracked);
}

// gc.is_finalized
// gc.is_finalized
// gc.is_finalized
static value fnIsFinalized(cffi& a){
    return a.mkbool(false);
}

// gc.freeze / unfreeze
// gc.freeze / unfreeze
// gc.freeze / unfreeze
static value fnFreeze(cffi& a){
    return a.none();
}

static value fnUnfreeze(cffi& a){
    return a.none();
}

// gc.get_debug
// gc.get_debug
// gc.get_debug
static value fnGetDebug(cffi& a){
    return a.mkint(stateOf(a.vm).debug);
}

// gc.set_debug
// gc.set_debug
// gc.set_debug
static value fnSetDebug(cffi& a){
    stateOf(a.vm).debug = a.arg_int(0);
    return a.none();
}

// gc.get_threshold
// gc.get_threshold
// gc.get_threshold
static value fnGetThreshold(cffi& a){
    GcState& s = stateOf(a.vm);
    return a.vm->makeTuple({a.mkint(s.threshold0), a.mkint(s.threshold1),
                            a.mkint(s.threshold2)});
}

// gc.set_threshold
// gc.set_threshold
// gc.set_threshold
static value fnSetThreshold(cffi& a){
    GcState& s = stateOf(a.vm);
    if (a.argc() > 0) s.threshold0 = a.arg_int(0);
    if (a.argc() > 1) s.threshold1 = a.arg_int(1);
    if (a.argc() > 2) s.threshold2 = a.arg_int(2);
    return a.none();
}

// gc.__sizeof__ 之类的辅助数据: 调试位常量
// gc debug flags
// gc ᠳ᠋ᠡᠪᠤᠭ ᠺᠣᠳ᠋
static value dataDebugStats(cffi& a){ return a.mkint(1); }
static value dataSaveall(cffi& a){ return a.mkint(32); }

// cffi gc lib
// cffi gc lib
// cffi gc lib
ffilib cffi_gc_lib(){
    return cffi_lib(
        "gc",
        {
            {"enable", (void*)fnEnable},
            {"disable", (void*)fnDisable},
            {"isenabled", (void*)fnIsenabled},
            {"collect", (void*)fnCollect},
            {"get_count", (void*)fnGetCount},
            {"get_stats", (void*)fnGetStats},
            {"get_objects", (void*)fnGetObjects},
            {"get_referrers", (void*)fnGetReferrers},
            {"get_referents", (void*)fnGetReferents},
            {"is_tracked", (void*)fnIsTracked},
            {"is_finalized", (void*)fnIsFinalized},
            {"freeze", (void*)fnFreeze},
            {"unfreeze", (void*)fnUnfreeze},
            {"get_debug", (void*)fnGetDebug},
            {"set_debug", (void*)fnSetDebug},
            {"get_threshold", (void*)fnGetThreshold},
            {"set_threshold", (void*)fnSetThreshold}
        },
        {
            {"DEBUG_STATS", (void*)dataDebugStats},
            {"DEBUG_SAVEALL", (void*)dataSaveall}
        });
}

}
