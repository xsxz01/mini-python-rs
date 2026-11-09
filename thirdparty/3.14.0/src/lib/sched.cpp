#include "../cffi.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace minipy {

namespace {

// 调度事件
// Scheduled event
// ᠲᠥᠯᠥᠪᠯᠡᠭᠳᠡᠭᠰᠡᠨ ᠦᠢᠯᠡ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct SchedEvent{
    double time = 0.0;
    long long priority = 0;
    long long sequence = 0;
    value action;
    value argument;
    std::map<std::string, value> kwargs;
};

// 调度器状态
// Scheduler state
// ᠲᠥᠯᠥᠪᠯᠡᠭᠴᠢ ᠪᠠᠶᠢᠳᠠᠯ
struct SchedulerState{
    value timefunc;
    value delayfunc;
    std::vector<std::shared_ptr<SchedEvent>> all;
    std::vector<SchedEvent*> queue;
    long long sequence = 0;
};

// 单调时钟 (秒)
// Monotonic clock in seconds
// ᠮᠣᠨᠣᠲᠣᠨ ᠴᠠᠭ
double monotonicSeconds(){
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// 默认时间函数动态调用
// Default timer dynamic call
// ᠡᠩ ᠦᠨ ᠴᠠᠭ ᠦᠨ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
bool callerNow(vm& v, ffi_instance&, const std::vector<value>&,
               const std::map<std::string, value>&, value& result){
    result = v.makeFloat(monotonicSeconds());
    return true;
}

// 默认延时函数动态调用
// Default delay dynamic call
// ᠡᠩ ᠦᠨ ᠬᠦᠯᠡᠶ᠎ᠡ ᠶᠢᠨ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
bool callerSleep(vm& v, ffi_instance&, const std::vector<value>& args,
                 const std::map<std::string, value>&, value& result){
    double seconds = args.empty() ? 0.0 : v.toFloat(args[0]);
    if (seconds > 0.0){
        std::chrono::duration<double> span(seconds);
        std::this_thread::sleep_for(span);
    }
    result = v.makeNone();
    return true;
}

// 取调度器状态
// Get scheduler state from self
// ᠲᠥᠯᠥᠪᠯᠡᠭᠴᠢ ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
SchedulerState* schedulerState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    return static_cast<SchedulerState*>(static_cast<ffi_instance*>(self.ptr)->handle);
}

// 生成事件对象
// Build an event object
// ᠦᠢᠯᠡ ᠪᠠᠶᠢᠳᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeEventObject(cffi& a, const std::shared_ptr<SchedEvent>& ev){
    const ffi_class* klass = nullptr;
    if (a.library){
        auto it = a.library->classes.find("Event");
        if (it != a.library->classes.end()) klass = &it->second;
    }
    value obj = a.vm->makeNativeObject(a.library, klass, ev.get());
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["time"] = a.mkfloat(ev->time);
        inst->attributes["priority"] = a.mkint(ev->priority);
        inst->attributes["action"] = ev->action;
        inst->attributes["argument"] = ev->argument;
        inst->attributes["kwargs"] = a.mkdict({});
    }
    return obj;
}

// 刷新 scheduler.queue 属性
// Refresh scheduler.queue attribute
// ᠲᠥᠯᠥᠪᠯᠡᠭᠴᠢ ᠦᠨ ᠰᠡᠳᠭᠦᠯ
void refreshQueue(cffi& a, SchedulerState* s, const value& self){
    if (self.type != "native_object" || !self.ptr) return;
    std::vector<value> items;
    for (auto* ev : s->queue) items.push_back(makeEventObject(a, std::shared_ptr<SchedEvent>(ev, [](SchedEvent*){})));
    static_cast<ffi_instance*>(self.ptr)->attributes["queue"] = a.vm->makeList(items);
}

// 读取当前时间
// Read current time
// ᠣᠳᠣᠮᠤ ᠴᠠᠭ ᠬᠠᠯᠠᠬᠤ
double currentTime(cffi& a, SchedulerState* s){
    value now = a.vm->callValue(s->timefunc, {});
    return a.vm->toFloat(now);
}

// 以可能带关键字参数的方式调用动作
// Invoke an action with optional keyword arguments
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠲᠡᠢ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
value callAction(vm& v, const value& action, const value& argument,
                 const std::map<std::string, value>& kwargs){
    std::vector<value> args;
    if (argument.type == "tuple" && argument.ptr){
        args = *static_cast<std::vector<value>*>(argument.ptr);
    } else if (argument.type == "list" && argument.ptr){
        args = *static_cast<std::vector<value>*>(argument.ptr);
    } else if (argument.type != "none"){
        args.push_back(argument);
    }
    if (!kwargs.empty() && action.type == "function" && action.ptr){
        auto* f = static_cast<function_t*>(action.ptr);
        return v.invoke(f, args, nullptr, &v.script, f->name, kwargs);
    }
    return v.callValue(action, args);
}

// 插入事件到有序队列
// Insert an event into the ordered queue
// ᠦᠢᠯᠡ ᠬᠠᠷᠲᠠᠯᠠᠬᠤ
void insertEventImpl(SchedulerState* s, SchedEvent* ev){
    size_t pos = s->queue.size();
    for (size_t i = 0; i < s->queue.size(); ++i){
        SchedEvent* cur = s->queue[i];
        if (ev->time < cur->time ||
            (ev->time == cur->time && ev->priority < cur->priority) ||
            (ev->time == cur->time && ev->priority == cur->priority &&
             ev->sequence < cur->sequence)){
            pos = i;
            break;
        }
    }
    s->queue.insert(s->queue.begin() + static_cast<long>(pos), ev);
}

// scheduler 构造
// scheduler constructor
// scheduler ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnSchedulerCtor(cffi& a){
    auto* s = new SchedulerState();
    if (a.argc() > 0 && a.arg(0).type != "none"){
        s->timefunc = a.arg(0);
    } else {
        s->timefunc = a.vm->makeDynamicObject("builtin_function_or_method",
                                              (void*)1, nullptr, callerNow);
    }
    if (a.argc() > 1 && a.arg(1).type != "none"){
        s->delayfunc = a.arg(1);
    } else {
        s->delayfunc = a.vm->makeDynamicObject("builtin_function_or_method",
                                               (void*)1, nullptr, callerSleep);
    }
    value obj = a.mkobject(s);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["timefunc"] = s->timefunc;
        inst->attributes["delayfunc"] = s->delayfunc;
        inst->attributes["queue"] = a.vm->makeList({});
    }
    return obj;
}

// scheduler.enterabs
// scheduler.enterabs
// scheduler.enterabs
static value fnEnterabs(cffi& a){
    SchedulerState* s = schedulerState(a);
    if (!s) return a.none();
    auto ev = std::make_shared<SchedEvent>();
    ev->time = a.vm->toFloat(a.arg(0));
    ev->priority = a.argc() > 1 ? a.arg_int(1) : 0;
    ev->action = a.argc() > 2 ? a.arg(2) : a.none();
    ev->argument = a.argc() > 3 ? a.arg(3) : a.vm->makeTuple({});
    ev->kwargs = a.kwargv;
    ev->sequence = s->sequence++;
    s->all.push_back(ev);
    insertEventImpl(s, ev.get());
    value eventObj = makeEventObject(a, ev);
    refreshQueue(a, s, a.self());
    return eventObj;
}

// scheduler.enter
// scheduler.enter
// scheduler.enter
static value fnEnter(cffi& a){
    SchedulerState* s = schedulerState(a);
    if (!s) return a.none();
    double delay = a.arg_float(0);
    auto ev = std::make_shared<SchedEvent>();
    ev->time = currentTime(a, s) + delay;
    ev->priority = a.argc() > 1 ? a.arg_int(1) : 0;
    ev->action = a.argc() > 2 ? a.arg(2) : a.none();
    ev->argument = a.argc() > 3 ? a.arg(3) : a.vm->makeTuple({});
    ev->kwargs = a.kwargv;
    ev->sequence = s->sequence++;
    s->all.push_back(ev);
    insertEventImpl(s, ev.get());
    value eventObj = makeEventObject(a, ev);
    refreshQueue(a, s, a.self());
    return eventObj;
}

// scheduler.cancel
// scheduler.cancel
// scheduler.cancel
static value fnCancel(cffi& a){
    SchedulerState* s = schedulerState(a);
    if (!s) return a.none();
    value eventObj = a.arg(0);
    if (eventObj.type != "native_object" || !eventObj.ptr) return a.none();
    auto* ev = static_cast<SchedEvent*>(static_cast<ffi_instance*>(eventObj.ptr)->handle);
    for (size_t i = 0; i < s->queue.size(); ++i){
        if (s->queue[i] == ev){
            s->queue.erase(s->queue.begin() + static_cast<long>(i));
            break;
        }
    }
    refreshQueue(a, s, a.self());
    return a.none();
}

// scheduler.empty
// scheduler.empty
// scheduler.empty
static value fnEmpty(cffi& a){
    SchedulerState* s = schedulerState(a);
    return a.mkbool(!s || s->queue.empty());
}

// scheduler.run
// scheduler.run
// scheduler.run
static value fnRun(cffi& a){
    SchedulerState* s = schedulerState(a);
    if (!s) return a.none();
    bool blocking = a.argc() > 0 ? a.arg_bool(0) : true;
    value self = a.self();
    while (!s->queue.empty()){
        SchedEvent* ev = s->queue.front();
        if (blocking){
            double now = currentTime(a, s);
            if (ev->time >= now){
                double wait = ev->time - now;
                if (wait > 0.0) a.vm->callValue(s->delayfunc, {a.mkfloat(wait)});
            }
        }
        s->queue.erase(s->queue.begin());
        refreshQueue(a, s, self);
        value ignored = callAction(*a.vm, ev->action, ev->argument, ev->kwargs);
        if (a.vm->hasError()) return a.none();
    }
    return a.none();
}

}

// cffi sched lib
// cffi sched lib
// cffi sched lib
ffilib cffi_sched_lib(){
    ffi_class eventClass;
    eventClass.name = "Event";

    ffi_class schedulerClass;
    schedulerClass.name = "scheduler";
    schedulerClass.constructor = (void*)fnSchedulerCtor;
    schedulerClass.methods["enterabs"] = (void*)fnEnterabs;
    schedulerClass.methods["enter"] = (void*)fnEnter;
    schedulerClass.methods["cancel"] = (void*)fnCancel;
    schedulerClass.methods["empty"] = (void*)fnEmpty;
    schedulerClass.methods["run"] = (void*)fnRun;

    return cffi_lib(
        "sched",
        {},
        {},
        {
            {"scheduler", schedulerClass},
            {"Event", eventClass}
        });
}

}
