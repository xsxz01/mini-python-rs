#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {

namespace {

// 队列类型
// Queue flavor
// ᠰᠡᠳᠭᠦᠯ ᠦᠨ ᠲᠥᠷᠥᠯ
enum QueueKind{ kFifo = 0, kLifo = 1, kPriority = 2, kSimple = 3 };

// 队列状态
// Queue state
// ᠰᠡᠳᠭᠦᠯ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct QueueState{
    int kind = kFifo;
    long long maxsize = 0;
    std::vector<value> items;
    long long unfinished = 0;
};

QueueState* queueState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    return static_cast<QueueState*>(static_cast<ffi_instance*>(self.ptr)->handle);
}

// 同步实例属性
// Sync instance attributes
// ᠪᠠᠶᠢᠳᠠᠯ ᠲᠠᠯᠠᠭ᠌
void syncAttrs(cffi& a, QueueState* s, const value& self){
    if (self.type != "native_object" || !self.ptr) return;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    inst->attributes["unfinished_tasks"] = a.mkint(s->unfinished);
    inst->attributes["maxsize"] = a.mkint(s->maxsize);
}

// 按类型插入
// Insert item according to queue kind
// ᠲᠥᠷᠥᠯ ᠪᠡᠷ ᠪᠠᠭᠤᠯᠭ᠎ᠠ
void insertItem(cffi& a, QueueState* s, const value& item){
    if (s->kind == kLifo || s->kind == kSimple){
        s->items.push_back(item);
        return;
    }
    if (s->kind == kPriority){
        size_t pos = s->items.size();
        while (pos > 0){
            value less = a.vm->applyBinary("<", item, s->items[pos - 1]);
            if (a.vm->hasError()) return;
            if (!a.vm->truthy(less)) break;
            --pos;
        }
        s->items.insert(s->items.begin() + static_cast<long>(pos), item);
        return;
    }
    s->items.push_back(item);
}

// 按类型取出
// Pop an item according to queue kind
// ᠲᠥᠷᠥᠯ ᠪᠡᠷ ᠭᠠᠷᠭ᠎ᠠ
value popItem(QueueState* s){
    if (s->items.empty()) return value();
    value item = s->items.front();
    if (s->kind == kLifo) item = s->items.back();
    s->items.erase(s->kind == kLifo ? s->items.end() - 1 : s->items.begin());
    return item;
}

// 通用构造
// Shared constructor
// ᠡᠩ ᠦᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value makeQueue(cffi& a, int kind){
    auto* s = new QueueState();
    s->kind = kind;
    if (kind != kSimple){
        if (a.has_kwarg("maxsize")) s->maxsize = a.kwarg_int("maxsize", 0);
        else if (a.argc() > 0 && a.arg(0).type != "none")
            s->maxsize = a.arg_int(0);
    }
    value obj = a.mkobject(s);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["unfinished_tasks"] = a.mkint(0);
        inst->attributes["maxsize"] = a.mkint(s->maxsize);
    }
    return obj;
}

static value fnQueueCtor(cffi& a){ return makeQueue(a, kFifo); }
static value fnLifoQueueCtor(cffi& a){ return makeQueue(a, kLifo); }
static value fnPriorityQueueCtor(cffi& a){ return makeQueue(a, kPriority); }
static value fnSimpleQueueCtor(cffi& a){ return makeQueue(a, kSimple); }

// queue.put
// queue.put
// queue.put
static value fnPut(cffi& a){
    QueueState* s = queueState(a);
    if (!s) return a.none();
    value item = a.arg(0);
    bool block = a.argc() > 1 ? a.arg_bool(1) : true;
    if (s->kind != kSimple && s->maxsize > 0 &&
        static_cast<long long>(s->items.size()) >= s->maxsize){
        if (!block){
            a.vm->raiseError("Full", "queue.Full");
            return a.none();
        }
        // 无线程模型: 无法真正等待, 直接报满
        a.vm->raiseError("Full", "queue.Full");
        return a.none();
    }
    insertItem(a, s, item);
    if (a.vm->hasError()) return a.none();
    ++s->unfinished;
    syncAttrs(a, s, a.self());
    return a.none();
}

// queue.put_nowait
// queue.put_nowait
// queue.put_nowait
static value fnPutNowait(cffi& a){
    QueueState* s = queueState(a);
    if (!s) return a.none();
    if (s->kind != kSimple && s->maxsize > 0 &&
        static_cast<long long>(s->items.size()) >= s->maxsize){
        a.vm->raiseError("Full", "queue.Full");
        return a.none();
    }
    insertItem(a, s, a.arg(0));
    if (a.vm->hasError()) return a.none();
    ++s->unfinished;
    syncAttrs(a, s, a.self());
    return a.none();
}

// queue.get
// queue.get
// queue.get
static value fnGet(cffi& a){
    QueueState* s = queueState(a);
    if (!s) return a.none();
    if (s->items.empty()){
        a.vm->raiseError("Empty", "queue.Empty");
        return a.none();
    }
    value item = popItem(s);
    syncAttrs(a, s, a.self());
    return item;
}

// queue.get_nowait
// queue.get_nowait
// queue.get_nowait
static value fnGetNowait(cffi& a){
    return fnGet(a);
}

// queue.qsize
// queue.qsize
// queue.qsize
static value fnQsize(cffi& a){
    QueueState* s = queueState(a);
    return a.mkint(s ? static_cast<long long>(s->items.size()) : 0);
}

// queue.empty
// queue.empty
// queue.empty
static value fnEmpty(cffi& a){
    QueueState* s = queueState(a);
    return a.mkbool(!s || s->items.empty());
}

// queue.full
// queue.full
// queue.full
static value fnFull(cffi& a){
    QueueState* s = queueState(a);
    if (!s) return a.mkbool(false);
    if (s->kind == kSimple) return a.mkbool(false);
    return a.mkbool(s->maxsize > 0 &&
                    static_cast<long long>(s->items.size()) >= s->maxsize);
}

// queue.task_done
// queue.task_done
// queue.task_done
static value fnTaskDone(cffi& a){
    QueueState* s = queueState(a);
    if (!s) return a.none();
    if (s->unfinished <= 0){
        a.vm->raiseError("ValueError", "task_done() called too many times");
        return a.none();
    }
    --s->unfinished;
    syncAttrs(a, s, a.self());
    return a.none();
}

// queue.join (无线程模型下仅当未完成数为 0 时立即返回)
// queue.join (returns immediately when nothing is unfinished)
// queue.join
static value fnJoin(cffi& a){
    QueueState* s = queueState(a);
    return a.none();
}

// queue.__len__
// queue.__len__
// queue.__len__
static value fnLen(cffi& a){
    QueueState* s = queueState(a);
    return a.mkint(s ? static_cast<long long>(s->items.size()) : 0);
}

}

// cffi queue lib
// cffi queue lib
// cffi queue lib
ffilib cffi_queue_lib(){
    ffi_class queueClass;
    queueClass.name = "Queue";
    queueClass.constructor = (void*)fnQueueCtor;
    queueClass.methods["put"] = (void*)fnPut;
    queueClass.methods["put_nowait"] = (void*)fnPutNowait;
    queueClass.methods["get"] = (void*)fnGet;
    queueClass.methods["get_nowait"] = (void*)fnGetNowait;
    queueClass.methods["qsize"] = (void*)fnQsize;
    queueClass.methods["empty"] = (void*)fnEmpty;
    queueClass.methods["full"] = (void*)fnFull;
    queueClass.methods["task_done"] = (void*)fnTaskDone;
    queueClass.methods["join"] = (void*)fnJoin;
    queueClass.methods["__len__"] = (void*)fnLen;

    ffi_class lifoClass = queueClass;
    lifoClass.name = "LifoQueue";
    lifoClass.constructor = (void*)fnLifoQueueCtor;

    ffi_class priorityClass = queueClass;
    priorityClass.name = "PriorityQueue";
    priorityClass.constructor = (void*)fnPriorityQueueCtor;

    ffi_class simpleClass;
    simpleClass.name = "SimpleQueue";
    simpleClass.constructor = (void*)fnSimpleQueueCtor;
    simpleClass.methods["put"] = (void*)fnPut;
    simpleClass.methods["put_nowait"] = (void*)fnPutNowait;
    simpleClass.methods["get"] = (void*)fnGet;
    simpleClass.methods["get_nowait"] = (void*)fnGetNowait;
    simpleClass.methods["qsize"] = (void*)fnQsize;
    simpleClass.methods["empty"] = (void*)fnEmpty;
    simpleClass.methods["full"] = (void*)fnFull;
    simpleClass.methods["__len__"] = (void*)fnLen;

    ffi_class emptyClass;
    emptyClass.name = "Empty";
    ffi_class fullClass;
    fullClass.name = "Full";

    return cffi_lib(
        "queue",
        {},
        {},
        {
            {"Queue", queueClass},
            {"LifoQueue", lifoClass},
            {"PriorityQueue", priorityClass},
            {"SimpleQueue", simpleClass},
            {"Empty", emptyClass},
            {"Full", fullClass}
        });
}

}
