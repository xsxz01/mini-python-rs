#include "../cffi.hpp"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 定时器状态
// Timer state
// ᠴᠠᠭ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct TimerState{
    std::string stmt = "pass";
    std::string setup = "pass";
    value timer;
};

// 默认计时函数动态调用
// Default timer dynamic call
// ᠡᠩ ᠦᠨ ᠴᠠᠭ ᠦᠨ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
bool callerPerfCounter(vm& v, ffi_instance&, const std::vector<value>&,
                       const std::map<std::string, value>&, value& result){
    using namespace std::chrono;
    double seconds = duration<double>(steady_clock::now().time_since_epoch()).count();
    result = v.makeFloat(seconds);
    return true;
}

// 生成默认计时器
// Build default timer object
// ᠡᠩ ᠦᠨ ᠴᠠᠭ ᠦᠨ ᠬᠡᠷᠡᠭᠰᠡᠯ
value makeDefaultTimer(cffi& a){
    return a.vm->makeDynamicObject("builtin_function_or_method", (void*)1,
                                   nullptr, callerPerfCounter);
}

// 执行一次计时循环
// Run the timing loop
// ᠴᠠᠭ ᠯᠣᠭᠢᠬᠣ
double measure(cffi& a, TimerState* s, long long number){
    if (a.vm->execSource(s->setup) == false) return -1.0;
    value start = a.vm->callValue(s->timer, {});
    double t0 = a.vm->toFloat(start);
    for (long long i = 0; i < number; ++i){
        if (!a.vm->execSource(s->stmt)) return -1.0;
    }
    value stop = a.vm->callValue(s->timer, {});
    double t1 = a.vm->toFloat(stop);
    return t1 - t0;
}

// Timer 构造
// Timer constructor
// Timer ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnTimerCtor(cffi& a){
    auto* s = new TimerState();
    auto pick = [&](size_t index, const std::string& key, const std::string& fallback) {
        if (a.has_kwarg(key)) return a.kwarg_str(key, fallback);
        if (a.argc() > index && a.arg(index).type != "none") return a.arg_str(index);
        return fallback;
    };
    s->stmt = pick(0, "stmt", "pass");
    s->setup = pick(1, "setup", "pass");
    if (a.has_kwarg("timer")) s->timer = a.kwarg("timer");
    else if (a.argc() > 2 && a.arg(2).type != "none") s->timer = a.arg(2);
    else s->timer = makeDefaultTimer(a);
    value obj = a.mkobject(s);
    if (obj.type == "native_object" && obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["timer"] = s->timer;
    return obj;
}

// Timer.timeit
// Timer.timeit
// Timer.timeit
static value fnTimeit(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mkfloat(0.0);
    auto* s = static_cast<TimerState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!s) return a.mkfloat(0.0);
    long long number = 1000000LL;
    if (a.has_kwarg("number")) number = a.kwarg_int("number", number);
    else if (a.argc() > 0 && a.arg(0).type != "none") number = a.arg_int(0);
    if (number < 0) number = 0;
    double elapsed = measure(a, s, number);
    if (a.vm->hasError()) return a.mkfloat(0.0);
    return a.mkfloat(elapsed < 0.0 ? 0.0 : elapsed);
}

// Timer.repeat
// Timer.repeat
// Timer.repeat
static value fnRepeat(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mklist({});
    auto* s = static_cast<TimerState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!s) return a.mklist({});
    long long repeat = 5;
    long long number = 1000000LL;
    if (a.has_kwarg("repeat")) repeat = a.kwarg_int("repeat", repeat);
    else if (a.argc() > 0 && a.arg(0).type != "none") repeat = a.arg_int(0);
    if (a.has_kwarg("number")) number = a.kwarg_int("number", number);
    else if (a.argc() > 1 && a.arg(1).type != "none") number = a.arg_int(1);
    std::vector<value> results;
    for (long long i = 0; i < repeat; ++i){
        double elapsed = measure(a, s, number < 0 ? 0 : number);
        if (a.vm->hasError()) break;
        results.push_back(a.mkfloat(elapsed < 0.0 ? 0.0 : elapsed));
    }
    return a.mklist(results);
}

// Timer.autorange
// Timer.autorange
// Timer.autorange
static value fnAutorange(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr)
        return a.vm->makeTuple({a.mkint(1), a.mkfloat(0.0)});
    auto* s = static_cast<TimerState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    double elapsed = measure(a, s, 10);
    return a.vm->makeTuple({a.mkint(10), a.mkfloat(elapsed < 0.0 ? 0.0 : elapsed)});
}

// 模块级 timeit
// Module-level timeit
// ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠦᠨ timeit
static value fnModuleTimeit(cffi& a){
    auto* s = new TimerState();
    if (a.has_kwarg("stmt")) s->stmt = a.kwarg_str("stmt", "pass");
    else if (a.argc() > 0 && a.arg(0).type != "none") s->stmt = a.arg_str(0);
    if (a.has_kwarg("setup")) s->setup = a.kwarg_str("setup", "pass");
    else if (a.argc() > 1 && a.arg(1).type != "none") s->setup = a.arg_str(1);
    if (a.has_kwarg("timer")) s->timer = a.kwarg("timer");
    else if (a.argc() > 2 && a.arg(2).type != "none") s->timer = a.arg(2);
    else s->timer = makeDefaultTimer(a);
    long long number = 1000000LL;
    if (a.has_kwarg("number")) number = a.kwarg_int("number", number);
    else if (a.argc() > 3 && a.arg(3).type != "none") number = a.arg_int(3);
    double elapsed = measure(a, s, number < 0 ? 0 : number);
    return a.mkfloat(elapsed < 0.0 ? 0.0 : elapsed);
}

// 模块级 repeat
// Module-level repeat
// ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠦᠨ repeat
static value fnModuleRepeat(cffi& a){
    auto* s = new TimerState();
    if (a.has_kwarg("stmt")) s->stmt = a.kwarg_str("stmt", "pass");
    else if (a.argc() > 0 && a.arg(0).type != "none") s->stmt = a.arg_str(0);
    if (a.has_kwarg("setup")) s->setup = a.kwarg_str("setup", "pass");
    else if (a.argc() > 1 && a.arg(1).type != "none") s->setup = a.arg_str(1);
    if (a.has_kwarg("timer")) s->timer = a.kwarg("timer");
    else if (a.argc() > 2 && a.arg(2).type != "none") s->timer = a.arg(2);
    else s->timer = makeDefaultTimer(a);
    long long repeat = 5;
    long long number = 1000000LL;
    if (a.has_kwarg("repeat")) repeat = a.kwarg_int("repeat", repeat);
    else if (a.argc() > 3 && a.arg(3).type != "none") repeat = a.arg_int(3);
    if (a.has_kwarg("number")) number = a.kwarg_int("number", number);
    else if (a.argc() > 4 && a.arg(4).type != "none") number = a.arg_int(4);
    std::vector<value> results;
    for (long long i = 0; i < repeat; ++i){
        double elapsed = measure(a, s, number < 0 ? 0 : number);
        if (a.vm->hasError()) break;
        results.push_back(a.mkfloat(elapsed < 0.0 ? 0.0 : elapsed));
    }
    return a.mklist(results);
}

// 数据: default_timer
// data: default_timer
// data: default_timer
static value dataDefaultTimer(cffi& a){
    return makeDefaultTimer(a);
}

// 数据: default_number
// data: default_number
// data: default_number
static value dataDefaultNumber(cffi& a){
    return a.mkint(1000000);
}

// 数据: default_repeat
// data: default_repeat
// data: default_repeat
static value dataDefaultRepeat(cffi& a){
    return a.mkint(5);
}

}

// cffi timeit lib
// cffi timeit lib
// cffi timeit lib
ffilib cffi_timeit_lib(){
    ffi_class timerClass;
    timerClass.name = "Timer";
    timerClass.constructor = (void*)fnTimerCtor;
    timerClass.methods["timeit"] = (void*)fnTimeit;
    timerClass.methods["repeat"] = (void*)fnRepeat;
    timerClass.methods["autorange"] = (void*)fnAutorange;

    return cffi_lib(
        "timeit",
        {
            {"timeit", (void*)fnModuleTimeit},
            {"repeat", (void*)fnModuleRepeat}
        },
        {
            {"default_timer", (void*)dataDefaultTimer},
            {"default_number", (void*)dataDefaultNumber},
            {"default_repeat", (void*)dataDefaultRepeat}
        },
        {
            {"Timer", timerClass}
        });
}

}
