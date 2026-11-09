#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace minipy {

namespace {

// ============ 通用工具 ============
// generic helpers
// ᠤᠯᠠᠨ ᠠᠷᠭ᠎ᠠ

bool classIs(const value& v, const char* name){
    if (v.type != "native_object" || !v.ptr) return false;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    return inst->klass && inst->klass->name == name;
}

// 通用迭代器状态 (快照语义弱, 基于索引)
// generic iterator state (index based)
// ᠪᠠᠶᠢᠳᠠᠯ ᠳᠠᠶᠢᠯᠤᠭᠴᠢ
struct IterState{
    value target;
    long long index = 0;
};

// ============ deque ============
struct DequeState{
    std::deque<value> items;
    long long maxlen = -1;
    value maxlenValue; // None 或 int
};

DequeState* dequeOf(const value& v){
    if (!classIs(v, "deque")) return nullptr;
    return static_cast<DequeState*>(static_cast<ffi_instance*>(v.ptr)->handle);
}

}

namespace {

// 限制 maxlen
// enforce maxlen
// maxlen ᠬᠢᠭᠰᠡᠨ
void dequeTrim(DequeState* st){
    if (st->maxlen < 0) return;
    while (static_cast<long long>(st->items.size()) > st->maxlen) st->items.pop_front();
}
void dequeTrimLeft(DequeState* st){
    if (st->maxlen < 0) return;
    while (static_cast<long long>(st->items.size()) > st->maxlen) st->items.pop_back();
}

}

// deque 构造: deque(iterable=(), maxlen=None)
// deque constructor
// deque ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnDequeCtor(cffi& a){
    auto* st = new DequeState();
    st->maxlenValue = a.none();
    if (a.has_kwarg("maxlen") && a.kwarg("maxlen").type == "int"){
        st->maxlen = a.kwarg_int("maxlen", -1);
        st->maxlenValue = a.kwarg("maxlen");
    }
    if (a.argc() >= 2 && a.arg(1).type == "int"){
        st->maxlen = a.arg_int(1);
        st->maxlenValue = a.arg(1);
    }
    if (st->maxlen < -1 || (st->maxlenValue.type == "int" && st->maxlen < 0)){
        a.vm->raiseError("ValueError", "maxlen must be non-negative");
        return a.none();
    }
    if (a.argc() >= 1 && a.arg(0).type != "none"){
        std::vector<value> elems;
        a.vm->materializeIterable(a.arg(0), elems);
        for (auto& v : elems) st->items.push_back(v);
        dequeTrim(st);
    }
    value obj = a.mkobject(st);
    if (obj.type == "native_object" && obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["maxlen"] = st->maxlenValue;
    return obj;
}

static value fnDequeAppend(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    st->items.push_back(a.arg(0));
    dequeTrim(st);
    return a.none();
}

static value fnDequeAppendLeft(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    st->items.push_front(a.arg(0));
    dequeTrimLeft(st);
    return a.none();
}

static value fnDequePop(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    if (st->items.empty()){
        a.vm->raiseError("IndexError", "pop from an empty deque");
        return a.none();
    }
    value v = st->items.back();
    st->items.pop_back();
    return v;
}

static value fnDequePopLeft(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    if (st->items.empty()){
        a.vm->raiseError("IndexError", "pop from an empty deque");
        return a.none();
    }
    value v = st->items.front();
    st->items.pop_front();
    return v;
}

static value fnDequeExtend(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    std::vector<value> extra;
    a.vm->materializeIterable(a.arg(0), extra);
    for (auto& v : extra) st->items.push_back(v);
    dequeTrim(st);
    return a.none();
}

static value fnDequeExtendLeft(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    std::vector<value> extra;
    a.vm->materializeIterable(a.arg(0), extra);
    for (auto& v : extra) st->items.push_front(v);
    dequeTrimLeft(st);
    return a.none();
}

static value fnDequeClear(cffi& a){
    auto* st = dequeOf(a.self());
    if (st) st->items.clear();
    return a.none();
}

static value fnDequeCopy(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    auto* ns = new DequeState();
    ns->items = st->items;
    ns->maxlen = st->maxlen;
    ns->maxlenValue = st->maxlenValue;
    const ffi_class* klass = static_cast<ffi_instance*>(a.self().ptr)->klass;
    return a.vm->makeNativeObject(a.library, klass, ns);
}

static value fnDequeCount(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.mkint(0);
    long long n = 0;
    for (auto& v : st->items){
        if (a.vm->valuesEqual(v, a.arg(0))) n++;
    }
    return a.mkint(n);
}

static value fnDequeIndex(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    long long start = a.argc() > 1 ? a.arg_int(1) : 0;
    long long stop = a.argc() > 2 ? a.arg_int(2) : static_cast<long long>(st->items.size());
    long long n = static_cast<long long>(st->items.size());
    if (start < 0) start += n;
    if (start < 0) start = 0;
    if (stop < 0) stop += n;
    if (stop > n) stop = n;
    for (long long i = start; i < stop; ++i){
        if (a.vm->valuesEqual(st->items[static_cast<size_t>(i)], a.arg(0)))
            return a.mkint(i);
    }
    a.vm->raiseError("ValueError", "deque.index(x): x not in deque");
    return a.none();
}

static value fnDequeInsert(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 2) return a.none();
    long long i = a.arg_int(0);
    long long n = static_cast<long long>(st->items.size());
    if (i < 0) i += n;
    if (i < 0) i = 0;
    if (i > n) i = n;
    st->items.insert(st->items.begin() + i, a.arg(1));
    dequeTrim(st);
    return a.none();
}

static value fnDequeRemove(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.none();
    for (size_t i = 0; i < st->items.size(); ++i){
        if (a.vm->valuesEqual(st->items[i], a.arg(0))){
            st->items.erase(st->items.begin() + static_cast<long long>(i));
            return a.none();
        }
    }
    a.vm->raiseError("ValueError", "deque.remove(x): x not in deque");
    return a.none();
}

static value fnDequeReverse(cffi& a){
    auto* st = dequeOf(a.self());
    if (st) std::reverse(st->items.begin(), st->items.end());
    return a.none();
}

static value fnDequeRotate(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    long long n = a.argc() > 0 ? a.arg_int(0) : 1;
    long long len = static_cast<long long>(st->items.size());
    if (len == 0) return a.none();
    n %= len;
    if (n < 0) n += len;
    std::deque<value> rotated;
    for (long long i = 0; i < len; ++i){
        rotated.push_back(st->items[static_cast<size_t>((i - n + len) % len)]);
    }
    st->items = rotated;
    return a.none();
}

static value fnDequeLen(cffi& a){
    auto* st = dequeOf(a.self());
    return a.mkint(st ? static_cast<long long>(st->items.size()) : 0);
}

static value fnDequeGetitem(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    value idx = a.arg(0);
    if (idx.type != "int"){
        a.vm->raiseError("TypeError", "deque indices must be integers");
        return a.none();
    }
    long long i = *static_cast<long long*>(idx.ptr);
    long long n = static_cast<long long>(st->items.size());
    if (i < 0) i += n;
    if (i < 0 || i >= n){
        a.vm->raiseError("IndexError", "deque index out of range");
        return a.none();
    }
    return st->items[static_cast<size_t>(i)];
}

static value fnDequeSetitem(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    long long i = a.arg_int(0);
    long long n = static_cast<long long>(st->items.size());
    if (i < 0) i += n;
    if (i < 0 || i >= n){
        a.vm->raiseError("IndexError", "deque index out of range");
        return a.none();
    }
    st->items[static_cast<size_t>(i)] = a.arg(1);
    return a.none();
}

static value fnDequeDelitem(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    long long i = a.arg_int(0);
    long long n = static_cast<long long>(st->items.size());
    if (i < 0) i += n;
    if (i < 0 || i >= n){
        a.vm->raiseError("IndexError", "deque index out of range");
        return a.none();
    }
    st->items.erase(st->items.begin() + i);
    return a.none();
}

static value fnDequeContains(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st || a.argc() < 1) return a.mkbool(false);
    for (auto& v : st->items){
        if (a.vm->valuesEqual(v, a.arg(0))) return a.mkbool(true);
    }
    return a.mkbool(false);
}

static value fnDequeRepr(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.mkstr("deque([])");
    std::string text = "deque([";
    for (size_t i = 0; i < st->items.size(); ++i){
        if (i > 0) text += ", ";
        text += pythonRepr(*a.vm, st->items[i]);
    }
    text += "]";
    if (st->maxlen >= 0) text += ", maxlen=" + std::to_string(st->maxlen);
    text += ")";
    return a.mkstr(text);
}

static value fnDequeEq(cffi& a){
    auto* ls = dequeOf(a.self());
    if (!ls) return a.mkbool(false);
    auto* rs = dequeOf(a.arg(0));
    if (!rs) return a.mkbool(false);
    if (ls->items.size() != rs->items.size()) return a.mkbool(false);
    for (size_t i = 0; i < ls->items.size(); ++i){
        if (!a.vm->valuesEqual(ls->items[i], rs->items[i])) return a.mkbool(false);
    }
    return a.mkbool(true);
}

static value fnDequeAdd(cffi& a){
    auto* ls = dequeOf(a.self());
    auto* rs = dequeOf(a.arg(0));
    if (!ls || !rs){
        a.vm->raiseError("TypeError", "can only concatenate deque to deque");
        return a.none();
    }
    auto* ns = new DequeState();
    ns->items = ls->items;
    for (auto& v : rs->items) ns->items.push_back(v);
    ns->maxlen = ls->maxlen;
    ns->maxlenValue = ls->maxlenValue;
    const ffi_class* klass = static_cast<ffi_instance*>(a.self().ptr)->klass;
    return a.vm->makeNativeObject(a.library, klass, ns);
}

static value fnDequeMul(cffi& a){
    auto* st = dequeOf(a.self());
    if (!st) return a.none();
    long long times = a.arg_int(0);
    auto* ns = new DequeState();
    ns->maxlen = st->maxlen;
    ns->maxlenValue = st->maxlenValue;
    if (times > 0){
        for (long long k = 0; k < times; ++k){
            for (auto& v : st->items) ns->items.push_back(v);
        }
    }
    const ffi_class* klass = static_cast<ffi_instance*>(a.self().ptr)->klass;
    return a.vm->makeNativeObject(a.library, klass, ns);
}

static value fnDequeIter(cffi& a){
    // 返回迭代器对象 (CollectionsIterator)
    // return iterator object
    // ᠳᠠᠶᠢᠯᠤᠭᠴᠢ ᠪᠢᠶᠡ
    auto itCls = a.library->classes.find("CollectionsIterator");
    if (itCls == a.library->classes.end()) return a.self();
    auto* st = new IterState();
    st->target = a.self();
    return a.vm->makeNativeObject(a.library, &itCls->second, st);
}

// 统一迭代器 __next__: 按目标类型分发 (前置声明, 定义在 dict-like/namedtuple 之后)
// unified iterator __next__ dispatch (forward decl)
// ᠨᠢᠶᠢᠲᠦ ᠳᠠᠶᠢᠯᠤᠭᠴᠢ __next__
static value fnIterNext(cffi& a);

// ============ dict 类容器共享逻辑 (Counter / OrderedDict / defaultdict) ============
// dict-like container shared logic
// dict ᠠᠯᠢᠰᠢᠮᠤ ᠬᠡᠪᠡᠯ

namespace {

struct DictLikeState{
    dict_t items;
    std::string flavor; // Counter / OrderedDict / defaultdict
    value defaultFactory;
    std::string factoryName;
};

DictLikeState* dictLikeOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass) return nullptr;
    const std::string& n = inst->klass->name;
    if (n != "Counter" && n != "OrderedDict" && n != "defaultdict") return nullptr;
    return static_cast<DictLikeState*>(inst->handle);
}

const ffi_class* dictLikeClass(cffi& a, const char* name){
    auto it = a.library->classes.find(name);
    return it == a.library->classes.end() ? nullptr : &it->second;
}

value makeDictLike(cffi& a, const std::string& flavor, const dict_t& items){
    const ffi_class* klass = dictLikeClass(a, flavor.c_str());
    if (!klass) return a.vm->makeDict(items);
    auto* st = new DictLikeState();
    st->items = items;
    st->flavor = flavor;
    st->defaultFactory = a.none();
    value obj = a.vm->makeNativeObject(a.library, klass, st);
    if (flavor == "defaultdict"){
        if (obj.ptr){
            auto* inst = static_cast<ffi_instance*>(obj.ptr);
            inst->attributes["default_factory"] = a.none();
        }
    }
    return obj;
}

// dict repr 文本
// dict repr text
// dict repr ᠲᠧᠺᠰᠲ
std::string dictRepr(vm& v, const dict_t& items){
    std::string text = "{";
    bool first = true;
    for (auto& kv : items){
        if (!first) text += ", ";
        first = false;
        text += pythonRepr(v, kv.first) + ": " + pythonRepr(v, kv.second);
    }
    text += "}";
    return text;
}

dict_t::iterator dictFind(dict_t& d, vm& v, const value& key){
    for (auto it = d.begin(); it != d.end(); ++it){
        if (v.valuesEqual(it->first, key)) return it;
    }
    return d.end();
}

}

static value fnDictLikeLen(cffi& a){
    auto* st = dictLikeOf(a.self());
    return a.mkint(st ? static_cast<long long>(st->items.size()) : 0);
}

static value fnDictLikeGetitem(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st){
        a.vm->raiseError("TypeError", "not a mapping");
        return a.none();
    }
    value key = a.arg(0);
    auto it = dictFind(st->items, *a.vm, key);
    if (it != st->items.end()) return it->second;
    if (st->flavor == "Counter") return a.mkint(0);
    if (st->flavor == "defaultdict" && st->defaultFactory.type != "none"){
        value produced = a.vm->callValue(st->defaultFactory, {});
        if (a.vm->hasError()) return a.none();
        std::string err = a.vm->dictSet(value("dict", &st->items, a.vm), key, produced);
        if (!err.empty()){
            a.vm->raiseError("TypeError", err);
            return a.none();
        }
        return produced;
    }
    a.vm->raiseError("KeyError", pythonRepr(*a.vm, key));
    return a.none();
}

static value fnDictLikeSetitem(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    std::string err = a.vm->dictSet(value("dict", &st->items, a.vm), a.arg(0), a.arg(1));
    if (!err.empty()) a.vm->raiseError("TypeError", err);
    return a.none();
}

static value fnDictLikeDelitem(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    auto it = dictFind(st->items, *a.vm, a.arg(0));
    if (it == st->items.end()){
        a.vm->raiseError("KeyError", pythonRepr(*a.vm, a.arg(0)));
        return a.none();
    }
    st->items.erase(it);
    return a.none();
}

static value fnDictLikeContains(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.mkbool(false);
    return a.mkbool(dictFind(st->items, *a.vm, a.arg(0)) != st->items.end());
}

static value fnDictLikeGet(cffi& a){
    auto* st = dictLikeOf(a.self());
    value fallback = a.argc() > 1 ? a.arg(1) : a.none();
    if (!st) return fallback;
    auto it = dictFind(st->items, *a.vm, a.arg(0));
    return it != st->items.end() ? it->second : fallback;
}

static value fnDictLikeKeys(cffi& a){
    auto* st = dictLikeOf(a.self());
    std::vector<value> out;
    if (st){
        for (auto& kv : st->items) out.push_back(kv.first);
    }
    return a.vm->makeList(out);
}

static value fnDictLikeValues(cffi& a){
    auto* st = dictLikeOf(a.self());
    std::vector<value> out;
    if (st){
        for (auto& kv : st->items) out.push_back(kv.second);
    }
    return a.vm->makeList(out);
}

static value fnDictLikeItems(cffi& a){
    auto* st = dictLikeOf(a.self());
    std::vector<value> out;
    if (st){
        for (auto& kv : st->items) out.push_back(a.vm->makeTuple({kv.first, kv.second}));
    }
    return a.vm->makeList(out);
}

static value fnDictLikeUpdate(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value src = a.arg(0);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr))
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else if (auto* ss = dictLikeOf(src)){
            for (auto& kv : ss->items)
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else {
            std::vector<value> entries;
            a.vm->materializeIterable(src, entries);
            for (size_t i = 0; i < entries.size(); ++i){
                if (entries[i].type == "tuple" || entries[i].type == "list"){
                    auto* pair = static_cast<std::vector<value>*>(entries[i].ptr);
                    if (pair->size() == 2)
                        a.vm->dictSet(value("dict", &st->items, a.vm), (*pair)[0], (*pair)[1]);
                }
            }
        }
    }
    for (auto& kv : a.kwargv){
        a.vm->dictSet(value("dict", &st->items, a.vm), a.vm->makeString(kv.first), kv.second);
    }
    return a.none();
}

static value fnDictLikePop(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    auto it = dictFind(st->items, *a.vm, a.arg(0));
    if (it != st->items.end()){
        value out = it->second;
        st->items.erase(it);
        return out;
    }
    if (a.argc() > 1) return a.arg(1);
    a.vm->raiseError("KeyError", pythonRepr(*a.vm, a.arg(0)));
    return a.none();
}

static value fnDictLikeSetdefault(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    value fallback = a.argc() > 1 ? a.arg(1) : a.none();
    auto it = dictFind(st->items, *a.vm, a.arg(0));
    if (it != st->items.end()) return it->second;
    a.vm->dictSet(value("dict", &st->items, a.vm), a.arg(0), fallback);
    return fallback;
}

static value fnDictLikeClear(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (st) st->items.clear();
    return a.none();
}

static value fnDictLikeRepr(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.mkstr("{}");
    if (st->flavor == "Counter"){
        dict_t sorted = st->items;
        bool numeric = true;
        for (auto& kv : sorted){
            if (kv.second.type != "int" && kv.second.type != "bool"){
                numeric = false;
                break;
            }
        }
        if (numeric){
            std::stable_sort(sorted.begin(), sorted.end(),
                [](const std::pair<value, value>& x,
                   const std::pair<value, value>& y){
                    return *static_cast<long long*>(x.second.ptr) >
                           *static_cast<long long*>(y.second.ptr);
                });
        }
        return a.mkstr("Counter(" + dictRepr(*a.vm, sorted) + ")");
    }
    if (st->flavor == "OrderedDict")
        return a.mkstr("OrderedDict(" + dictRepr(*a.vm, st->items) + ")");
    return a.mkstr("defaultdict(" + st->factoryName + ", " + dictRepr(*a.vm, st->items) + ")");
}

static value fnDictLikeIter(cffi& a){
    auto itCls = a.library->classes.find("CollectionsIterator");
    if (itCls == a.library->classes.end()) return a.self();
    auto* st = new IterState();
    st->target = a.self();
    return a.vm->makeNativeObject(a.library, &itCls->second, st);
}

// ============ Counter ============

static value fnCounterCtor(cffi& a){
    auto* st = new DictLikeState();
    st->flavor = "Counter";
    st->defaultFactory = a.none();
    value obj = a.mkobject(st);
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value src = a.arg(0);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr))
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else if (auto* ss = dictLikeOf(src)){
            st->items = ss->items;
        } else {
            std::vector<value> elems;
            a.vm->materializeIterable(src, elems);
            for (auto& el : elems){
                auto it = dictFind(st->items, *a.vm, el);
                if (it != st->items.end()){
                    long long cur = (it->second.type == "int" || it->second.type == "bool")
                                        ? *static_cast<long long*>(it->second.ptr) : 0;
                    a.vm->dictSet(value("dict", &st->items, a.vm), el, a.vm->makeInt(cur + 1));
                } else {
                    a.vm->dictSet(value("dict", &st->items, a.vm), el, a.vm->makeInt(1));
                }
            }
        }
    }
    for (auto& kv : a.kwargv){
        a.vm->dictSet(value("dict", &st->items, a.vm), a.vm->makeString(kv.first), kv.second);
    }
    return obj;
}

static void counterAddInto(cffi& a, DictLikeState* st, const value& key, long long delta){
    auto it = dictFind(st->items, *a.vm, key);
    long long cur = 0;
    if (it != st->items.end() && (it->second.type == "int" || it->second.type == "bool"))
        cur = *static_cast<long long*>(it->second.ptr);
    a.vm->dictSet(value("dict", &st->items, a.vm), key, a.vm->makeInt(cur + delta));
}

static value fnCounterElements(cffi& a){
    auto* st = dictLikeOf(a.self());
    std::vector<value> out;
    if (st){
        for (auto& kv : st->items){
            long long n = (kv.second.type == "int" || kv.second.type == "bool")
                              ? *static_cast<long long*>(kv.second.ptr) : 0;
            for (long long i = 0; i < n; ++i) out.push_back(kv.first);
        }
    }
    return a.vm->makeList(out);
}

static value fnCounterMostCommon(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.vm->makeList({});
    std::vector<std::pair<value, long long>> rows;
    for (auto& kv : st->items){
        long long n = (kv.second.type == "int" || kv.second.type == "bool")
                          ? *static_cast<long long*>(kv.second.ptr) : 0;
        rows.push_back({kv.first, n});
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](const std::pair<value, long long>& x,
                        const std::pair<value, long long>& y){ return x.second > y.second; });
    long long limit = -1;
    if (a.argc() > 0 && a.arg(0).type != "none") limit = a.arg_int(0);
    std::vector<value> out;
    for (auto& p : rows){
        if (limit >= 0 && static_cast<long long>(out.size()) >= limit) break;
        out.push_back(a.vm->makeTuple({p.first, a.vm->makeInt(p.second)}));
    }
    return a.vm->makeList(out);
}

static value fnCounterTotal(cffi& a){
    auto* st = dictLikeOf(a.self());
    long long total = 0;
    if (st){
        for (auto& kv : st->items){
            if (kv.second.type == "int" || kv.second.type == "bool")
                total += *static_cast<long long*>(kv.second.ptr);
        }
    }
    return a.mkint(total);
}

static value fnCounterUpdate(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value src = a.arg(0);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr)){
                long long d = (kv.second.type == "int" || kv.second.type == "bool")
                                  ? *static_cast<long long*>(kv.second.ptr) : 1;
                counterAddInto(a, st, kv.first, d);
            }
        } else if (auto* ss = dictLikeOf(src)){
            for (auto& kv : ss->items){
                long long d = (kv.second.type == "int" || kv.second.type == "bool")
                                  ? *static_cast<long long*>(kv.second.ptr) : 1;
                counterAddInto(a, st, kv.first, d);
            }
        } else {
            std::vector<value> elems;
            a.vm->materializeIterable(src, elems);
            for (auto& el : elems) counterAddInto(a, st, el, 1);
        }
    }
    for (auto& kv : a.kwargv){
        long long d = (kv.second.type == "int" || kv.second.type == "bool")
                          ? *static_cast<long long*>(kv.second.ptr) : 1;
        counterAddInto(a, st, a.vm->makeString(kv.first), d);
    }
    return a.none();
}

static value fnCounterSubtract(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value src = a.arg(0);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr)){
                long long d = (kv.second.type == "int" || kv.second.type == "bool")
                                  ? *static_cast<long long*>(kv.second.ptr) : 1;
                counterAddInto(a, st, kv.first, -d);
            }
        } else if (auto* ss = dictLikeOf(src)){
            for (auto& kv : ss->items){
                long long d = (kv.second.type == "int" || kv.second.type == "bool")
                                  ? *static_cast<long long*>(kv.second.ptr) : 1;
                counterAddInto(a, st, kv.first, -d);
            }
        } else {
            std::vector<value> elems;
            a.vm->materializeIterable(src, elems);
            for (auto& el : elems) counterAddInto(a, st, el, -1);
        }
    }
    for (auto& kv : a.kwargv){
        long long d = (kv.second.type == "int" || kv.second.type == "bool")
                          ? *static_cast<long long*>(kv.second.ptr) : 1;
        counterAddInto(a, st, a.vm->makeString(kv.first), -d);
    }
    return a.none();
}

// Counter 集合运算: + - & |
// Counter set operations
// Counter ᠲᠣᠭᠠᠨ ᠦᠢᠯᠡᠳᠬᠦ
static value counterSetOp(cffi& a, const std::string& op){
    auto* ls = dictLikeOf(a.self());
    auto* rs = dictLikeOf(a.arg(0));
    if (!ls || !rs){
        a.vm->raiseError("TypeError", "unsupported operand type(s)");
        return a.none();
    }
    auto* out = new DictLikeState();
    out->flavor = "Counter";
    out->defaultFactory = a.none();
    auto getCount = [](DictLikeState* st, vm& v, const value& key) -> long long {
        auto it = dictFind(st->items, v, key);
        if (it != st->items.end() && (it->second.type == "int" || it->second.type == "bool"))
            return *static_cast<long long*>(it->second.ptr);
        return 0;
    };
    auto put = [&](const value& key, long long val){
        a.vm->dictSet(value("dict", &out->items, a.vm), key, a.vm->makeInt(val));
    };
    if (op == "+"){
        std::set<std::string> seen;
        for (auto& kv : ls->items){
            long long r = getCount(ls, *a.vm, kv.first) + getCount(rs, *a.vm, kv.first);
            if (r > 0) put(kv.first, r);
            seen.insert(a.vm->toString(kv.first));
        }
        for (auto& kv : rs->items){
            std::string ks = a.vm->toString(kv.first);
            if (seen.count(ks)) continue;
            long long r = getCount(ls, *a.vm, kv.first) + getCount(rs, *a.vm, kv.first);
            if (r > 0) put(kv.first, r);
        }
    } else if (op == "-"){
        for (auto& kv : ls->items){
            long long r = getCount(ls, *a.vm, kv.first) - getCount(rs, *a.vm, kv.first);
            if (r > 0) put(kv.first, r);
        }
    } else if (op == "&"){
        for (auto& kv : ls->items){
            long long r = std::min(getCount(ls, *a.vm, kv.first), getCount(rs, *a.vm, kv.first));
            if (r > 0) put(kv.first, r);
        }
    } else {
        std::set<std::string> seen;
        for (auto& kv : ls->items){
            long long r = std::max(getCount(ls, *a.vm, kv.first), getCount(rs, *a.vm, kv.first));
            if (r > 0) put(kv.first, r);
            seen.insert(a.vm->toString(kv.first));
        }
        for (auto& kv : rs->items){
            std::string ks = a.vm->toString(kv.first);
            if (seen.count(ks)) continue;
            long long r = std::max(getCount(ls, *a.vm, kv.first), getCount(rs, *a.vm, kv.first));
            if (r > 0) put(kv.first, r);
        }
    }
    const ffi_class* klass = dictLikeClass(a, "Counter");
    return a.vm->makeNativeObject(a.library, klass, out);
}

static value fnCounterAdd(cffi& a){ return counterSetOp(a, "+"); }
static value fnCounterSub(cffi& a){ return counterSetOp(a, "-"); }
static value fnCounterAnd(cffi& a){ return counterSetOp(a, "&"); }
static value fnCounterOr(cffi& a){ return counterSetOp(a, "|"); }

static value fnCounterEq(cffi& a){
    auto* ls = dictLikeOf(a.self());
    if (!ls) return a.mkbool(false);
    value other = a.arg(0);
    dict_t* rd = nullptr;
    auto* rs = dictLikeOf(other);
    if (rs) rd = &rs->items;
    else if (other.type == "dict" && other.ptr) rd = static_cast<dict_t*>(other.ptr);
    else return a.mkbool(false);
    if (ls->items.size() != rd->size()) return a.mkbool(false);
    for (auto& kv : ls->items){
        auto it = dictFind(*rd, *a.vm, kv.first);
        if (it == rd->end() || !a.vm->valuesEqual(it->second, kv.second)) return a.mkbool(false);
    }
    return a.mkbool(true);
}

// ============ OrderedDict ============

static value fnOrderedCtor(cffi& a){
    auto* st = new DictLikeState();
    st->flavor = "OrderedDict";
    st->defaultFactory = a.none();
    value obj = a.mkobject(st);
    // 初始数据 (位置参数 + 关键字)
    // initial data (positional + kwargs)
    // ᠡᠬᠢᠯᠡᠯ ᠤᠨ ᠳᠠᠲ᠋ᠠ
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value src = a.arg(0);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr))
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else if (auto* ss = dictLikeOf(src)){
            for (auto& kv : ss->items)
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else {
            std::vector<value> entries;
            a.vm->materializeIterable(src, entries);
            for (auto& el : entries){
                if (el.type == "tuple" || el.type == "list"){
                    auto* pair = static_cast<std::vector<value>*>(el.ptr);
                    if (pair->size() == 2)
                        a.vm->dictSet(value("dict", &st->items, a.vm), (*pair)[0], (*pair)[1]);
                }
            }
        }
    }
    for (auto& kv : a.kwargv){
        a.vm->dictSet(value("dict", &st->items, a.vm), a.vm->makeString(kv.first), kv.second);
    }
    return obj;
}

static value fnOrderedMoveToEnd(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    value key = a.arg(0);
    bool last = true;
    if (a.argc() > 1) last = a.vm->truthy(a.arg(1));
    if (a.has_kwarg("last")) last = a.kwarg_bool("last", true);
    auto it = dictFind(st->items, *a.vm, key);
    if (it == st->items.end()){
        a.vm->raiseError("KeyError", pythonRepr(*a.vm, key));
        return a.none();
    }
    auto pair = *it;
    st->items.erase(it);
    if (last) st->items.push_back(pair);
    else st->items.insert(st->items.begin(), pair);
    return a.none();
}

static value fnOrderedPopitem(cffi& a){
    auto* st = dictLikeOf(a.self());
    if (!st) return a.none();
    bool last = true;
    if (a.argc() > 0) last = a.vm->truthy(a.arg(0));
    if (a.has_kwarg("last")) last = a.vm->truthy(a.kwarg("last"));
    if (st->items.empty()){
        a.vm->raiseError("KeyError", "'dictionary is empty'");
        return a.none();
    }
    auto pair = last ? st->items.back() : st->items.front();
    if (last) st->items.pop_back();
    else st->items.erase(st->items.begin());
    return a.vm->makeTuple({pair.first, pair.second});
}

static value fnOrderedEq(cffi& a){
    auto* ls = dictLikeOf(a.self());
    if (!ls) return a.mkbool(false);
    value other = a.arg(0);
    auto* rs = dictLikeOf(other);
    if (rs && rs->flavor == "OrderedDict"){
        if (ls->items.size() != rs->items.size()) return a.mkbool(false);
        for (size_t i = 0; i < ls->items.size(); ++i){
            if (!a.vm->valuesEqual(ls->items[i].first, rs->items[i].first) ||
                !a.vm->valuesEqual(ls->items[i].second, rs->items[i].second))
                return a.mkbool(false);
        }
        return a.mkbool(true);
    }
    dict_t* rd = nullptr;
    if (rs) rd = &rs->items;
    else if (other.type == "dict" && other.ptr) rd = static_cast<dict_t*>(other.ptr);
    else return a.mkbool(false);
    if (ls->items.size() != rd->size()) return a.mkbool(false);
    for (auto& kv : ls->items){
        auto it = dictFind(*rd, *a.vm, kv.first);
        if (it == rd->end() || !a.vm->valuesEqual(it->second, kv.second)) return a.mkbool(false);
    }
    return a.mkbool(true);
}

// ============ defaultdict ============

static value fnDefaultdictCtor(cffi& a){
    auto* st = new DictLikeState();
    st->flavor = "defaultdict";
    st->defaultFactory = a.none();
    st->factoryName = "None";
    size_t argIdx = 0;
    if (a.argc() > 0){
        value f = a.arg(0);
        if (f.type != "none"){
            st->defaultFactory = f;
            if (f.type == "class" && f.ptr){
                st->factoryName = "<class '" +
                    static_cast<class_t*>(f.ptr)->name + "'>";
            } else if (f.type == "native_object" && f.ptr){
                auto* fi = static_cast<ffi_instance*>(f.ptr);
                st->factoryName = "<class '" +
                    (fi->klass ? fi->klass->name : fi->typeName) + "'>";
            } else {
                st->factoryName = pythonRepr(*a.vm, f);
            }
        }
        argIdx = 1;
    }
    value obj = a.mkobject(st);
    if (obj.type == "native_object" && obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["default_factory"] = st->defaultFactory;
    // 第二个位置参数为初始映射
    // second positional arg is initial mapping
    // ᠬᠣᠶᠠᠳᠤᠭᠠᠷ ᠠᠷᠭᠤᠮᠧᠨᠲ ᠨᠢ ᠡᠬᠢᠯᠡᠯ ᠤᠨ dict
    if (a.argc() > argIdx){
        value src = a.arg(argIdx);
        if (src.type == "dict" && src.ptr){
            for (auto& kv : *static_cast<dict_t*>(src.ptr))
                a.vm->dictSet(value("dict", &st->items, a.vm), kv.first, kv.second);
        } else {
            std::vector<value> elems;
            a.vm->materializeIterable(src, elems);
            for (auto& el : elems){
                if (el.type == "tuple" || el.type == "list"){
                    auto* pair = static_cast<std::vector<value>*>(el.ptr);
                    if (pair->size() == 2)
                        a.vm->dictSet(value("dict", &st->items, a.vm), (*pair)[0], (*pair)[1]);
                }
            }
        }
    }
    for (auto& kv : a.kwargv){
        a.vm->dictSet(value("dict", &st->items, a.vm), a.vm->makeString(kv.first), kv.second);
    }
    return obj;
}

// ============ ChainMap ============

namespace {

struct ChainMapState{
    std::vector<value> maps;
};

ChainMapState* chainMapOf(const value& v){
    if (!classIs(v, "ChainMap")) return nullptr;
    return static_cast<ChainMapState*>(static_cast<ffi_instance*>(v.ptr)->handle);
}

dict_t* mapDict(const value& v){
    if (v.type == "dict" && v.ptr) return static_cast<dict_t*>(v.ptr);
    auto* dl = dictLikeOf(v);
    if (dl) return &dl->items;
    return nullptr;
}

}

// 创建 ChainMap 实例并同步 maps/parents 属性
// Create a ChainMap instance and sync maps/parents attributes
// ChainMap ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value makeChainMapObject(cffi& a, const ffi_class* klass, ChainMapState* st){
    value obj = a.vm->makeNativeObject(a.library, klass, st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["maps"] = a.vm->makeList(st->maps);
        if (st->maps.size() > 1){
            auto* ps = new ChainMapState();
            for (size_t i = 1; i < st->maps.size(); ++i) ps->maps.push_back(st->maps[i]);
            value pobj = a.vm->makeNativeObject(a.library, klass, ps);
            if (pobj.type == "native_object" && pobj.ptr)
                static_cast<ffi_instance*>(pobj.ptr)->attributes["maps"] =
                    a.vm->makeList(ps->maps);
            inst->attributes["parents"] = pobj;
        }
    }
    return obj;
}

static value fnChainMapCtor(cffi& a){
    auto* st = new ChainMapState();
    if (a.argc() == 0){
        st->maps.push_back(a.vm->makeDict({}));
    } else {
        for (size_t i = 0; i < a.argc(); ++i) st->maps.push_back(a.arg(i));
    }
    return makeChainMapObject(a, a.currentClass, st);
}

static value fnChainMapGetitem(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st) return a.none();
    value key = a.arg(0);
    for (auto& m : st->maps){
        dict_t* d = mapDict(m);
        if (!d) continue;
        auto it = dictFind(*d, *a.vm, key);
        if (it != d->end()) return it->second;
    }
    a.vm->raiseError("KeyError", pythonRepr(*a.vm, key));
    return a.none();
}

static value fnChainMapSetitem(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st || st->maps.empty()) return a.none();
    dict_t* d = mapDict(st->maps[0]);
    if (d) a.vm->dictSet(value("dict", d, a.vm), a.arg(0), a.arg(1));
    return a.none();
}

static value fnChainMapDelitem(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st || st->maps.empty()) return a.none();
    dict_t* d = mapDict(st->maps[0]);
    if (d){
        auto it = dictFind(*d, *a.vm, a.arg(0));
        if (it != d->end()){
            d->erase(it);
            return a.none();
        }
    }
    a.vm->raiseError("KeyError", pythonRepr(*a.vm, a.arg(0)));
    return a.none();
}

static value fnChainMapContains(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st) return a.mkbool(false);
    for (auto& m : st->maps){
        dict_t* d = mapDict(m);
        if (d && dictFind(*d, *a.vm, a.arg(0)) != d->end()) return a.mkbool(true);
    }
    return a.mkbool(false);
}

// 联合键 (最后一张图优先, CPython 顺序)
// union keys (last map first, CPython order)
// ᠨᠢᠶᠢᠯᠦᠯᠭ᠎ᠡ ᠲᠤᠯᠬᠢᠭᠤᠷ
static std::vector<value> chainMapKeys(cffi& a, ChainMapState* st){
    std::vector<value> out;
    if (!st) return out;
    for (auto it = st->maps.rbegin(); it != st->maps.rend(); ++it){
        dict_t* d = mapDict(*it);
        if (!d) continue;
        for (auto& kv : *d){
            bool dup = false;
            for (auto& existing : out){
                if (a.vm->valuesEqual(existing, kv.first)){
                    dup = true;
                    break;
                }
            }
            if (!dup) out.push_back(kv.first);
        }
    }
    return out;
}

static value fnChainMapLen(cffi& a){
    auto* st = chainMapOf(a.self());
    return a.mkint(static_cast<long long>(chainMapKeys(a, st).size()));
}

static value fnChainMapKeys(cffi& a){
    auto* st = chainMapOf(a.self());
    return a.vm->makeList(chainMapKeys(a, st));
}

static value fnChainMapValues(cffi& a){
    auto* st = chainMapOf(a.self());
    std::vector<value> out;
    for (auto& k : chainMapKeys(a, st)){
        for (auto& m : st->maps){
            dict_t* d = mapDict(m);
            if (!d) continue;
            auto it = dictFind(*d, *a.vm, k);
            if (it != d->end()){
                out.push_back(it->second);
                break;
            }
        }
    }
    return a.vm->makeList(out);
}

static value fnChainMapItems(cffi& a){
    auto* st = chainMapOf(a.self());
    std::vector<value> keys = chainMapKeys(a, st);
    std::vector<value> vals = fnChainMapValues(a).type == "list"
        ? *static_cast<std::vector<value>*>(fnChainMapValues(a).ptr)
        : std::vector<value>();
    std::vector<value> out;
    for (size_t i = 0; i < keys.size() && i < vals.size(); ++i)
        out.push_back(a.vm->makeTuple({keys[i], vals[i]}));
    return a.vm->makeList(out);
}

static value fnChainMapGet(cffi& a){
    auto* st = chainMapOf(a.self());
    value fallback = a.argc() > 1 ? a.arg(1) : a.none();
    if (!st) return fallback;
    for (auto& m : st->maps){
        dict_t* d = mapDict(m);
        if (!d) continue;
        auto it = dictFind(*d, *a.vm, a.arg(0));
        if (it != d->end()) return it->second;
    }
    return fallback;
}

static value fnChainMapNewChild(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st) return a.none();
    auto* ns = new ChainMapState();
    if (a.argc() > 0 && a.arg(0).type != "none") ns->maps.push_back(a.arg(0));
    else ns->maps.push_back(a.vm->makeDict({}));
    for (auto& m : st->maps) ns->maps.push_back(m);
    const ffi_class* klass = static_cast<ffi_instance*>(a.self().ptr)->klass;
    return makeChainMapObject(a, klass, ns);
}

static value fnChainMapParents(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st) return a.none();
    auto* ns = new ChainMapState();
    if (st->maps.size() > 1){
        for (size_t i = 1; i < st->maps.size(); ++i) ns->maps.push_back(st->maps[i]);
    } else {
        ns->maps.push_back(a.vm->makeDict({}));
    }
    const ffi_class* klass = static_cast<ffi_instance*>(a.self().ptr)->klass;
    return a.vm->makeNativeObject(a.library, klass, ns);
}

static value fnChainMapRepr(cffi& a){
    auto* st = chainMapOf(a.self());
    if (!st) return a.mkstr("ChainMap({})");
    std::string text = "ChainMap(";
    for (size_t i = 0; i < st->maps.size(); ++i){
        if (i > 0) text += ", ";
        dict_t* d = mapDict(st->maps[i]);
        text += d ? dictRepr(*a.vm, *d) : a.vm->toString(st->maps[i]);
    }
    text += ")";
    return a.mkstr(text);
}

// ============ namedtuple ============

namespace {

// namedtuple 类注册信息
// namedtuple class registry entry
// namedtuple ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠪᠦᠷᠢᠲᠭᠡᠯ
struct NTClassInfo{
    ffi_class* klass = nullptr;
    std::string typeName;
    std::vector<std::string> fields;
    std::map<std::string, value> defaults;
};

std::vector<std::unique_ptr<ffi_class>>& ntKlassKeep(){
    static std::vector<std::unique_ptr<ffi_class>> keep;
    return keep;
}

std::vector<std::unique_ptr<NTClassInfo>>& ntRegistry(){
    static std::vector<std::unique_ptr<NTClassInfo>> reg;
    return reg;
}

NTClassInfo* ntFindClass(const ffi_class* klass){
    for (auto& info : ntRegistry()){
        if (info->klass == klass) return info.get();
    }
    return nullptr;
}

// namedtuple 实例状态
// namedtuple instance state
// namedtuple ᠪᠢᠶᠡ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct NTState{
    NTClassInfo* info = nullptr;
    std::vector<value> vals;
};

NTState* ntStateOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass) return nullptr;
    NTClassInfo* info = ntFindClass(inst->klass);
    if (!info) return nullptr;
    return static_cast<NTState*>(inst->handle);
}

value makeNT(cffi& a, NTClassInfo* info, const std::vector<value>& vals){
    auto* st = new NTState();
    st->info = info;
    st->vals = vals;
    value obj = a.vm->makeNativeObject(a.library, info->klass, st);
    if (obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        for (size_t i = 0; i < info->fields.size() && i < vals.size(); ++i)
            inst->attributes[info->fields[i]] = vals[i];
    }
    return obj;
}

}

static value fnNTRepr(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st || !st->info) return a.mkstr("()");
    std::string text = st->info->typeName + "(";
    for (size_t i = 0; i < st->info->fields.size(); ++i){
        if (i > 0) text += ", ";
        text += st->info->fields[i] + "=" + pythonRepr(*a.vm, st->vals[i]);
    }
    text += ")";
    return a.mkstr(text);
}

static value fnNTLen(cffi& a){
    auto* st = ntStateOf(a.self());
    return a.mkint(st ? static_cast<long long>(st->vals.size()) : 0);
}

static value fnNTGetitem(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st) return a.none();
    value idx = a.arg(0);
    if (idx.type != "int"){
        a.vm->raiseError("TypeError", "tuple indices must be integers or slices");
        return a.none();
    }
    long long i = *static_cast<long long*>(idx.ptr);
    long long n = static_cast<long long>(st->vals.size());
    if (i < 0) i += n;
    if (i < 0 || i >= n){
        a.vm->raiseError("IndexError", "tuple index out of range");
        return a.none();
    }
    return st->vals[static_cast<size_t>(i)];
}

static value fnNTAdd(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st) return a.none();
    value other = a.arg(0);
    std::vector<value> out = st->vals;
    if (other.type == "tuple" || other.type == "list"){
        auto* rv = static_cast<std::vector<value>*>(other.ptr);
        out.insert(out.end(), rv->begin(), rv->end());
        return a.vm->makeTuple(out);
    }
    auto* os = ntStateOf(other);
    if (os){
        out.insert(out.end(), os->vals.begin(), os->vals.end());
        return a.vm->makeTuple(out);
    }
    a.vm->raiseError("TypeError", "can only concatenate tuple (not \"" + other.type +
                                  "\") to tuple");
    return a.none();
}

static value fnNTHash(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st) return a.mkint(0);
    return a.mkint(a.vm->hashKey(a.vm->makeTuple(st->vals)));
}

static value fnNTEq(cffi& a){
    auto* ls = ntStateOf(a.self());
    if (!ls) return a.mkbool(false);
    value other = a.arg(0);
    std::vector<value>* rv = nullptr;
    auto* rs = ntStateOf(other);
    if (rs) rv = &rs->vals;
    else if (other.type == "tuple" || other.type == "list")
        rv = static_cast<std::vector<value>*>(other.ptr);
    else return a.mkbool(false);
    if (ls->vals.size() != rv->size()) return a.mkbool(false);
    for (size_t i = 0; i < ls->vals.size(); ++i){
        if (!a.vm->valuesEqual(ls->vals[i], (*rv)[i])) return a.mkbool(false);
    }
    return a.mkbool(true);
}

static value fnNTIter(cffi& a){
    auto itCls = a.library->classes.find("CollectionsIterator");
    if (itCls == a.library->classes.end()) return a.self();
    auto* st = new IterState();
    st->target = a.self();
    return a.vm->makeNativeObject(a.library, &itCls->second, st);
}

static value fnNTContains(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st) return a.mkbool(false);
    for (auto& v : st->vals){
        if (a.vm->valuesEqual(v, a.arg(0))) return a.mkbool(true);
    }
    return a.mkbool(false);
}

static value fnNTCount(cffi& a){
    auto* st = ntStateOf(a.self());
    long long n = 0;
    if (st){
        for (auto& v : st->vals){
            if (a.vm->valuesEqual(v, a.arg(0))) n++;
        }
    }
    return a.mkint(n);
}

static value fnNTIndex(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st) return a.none();
    for (size_t i = 0; i < st->vals.size(); ++i){
        if (a.vm->valuesEqual(st->vals[i], a.arg(0)))
            return a.mkint(static_cast<long long>(i));
    }
    a.vm->raiseError("ValueError", "tuple.index(x): x not in tuple");
    return a.none();
}

static value fnNTAsDict(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st || !st->info) return a.vm->makeDict({});
    std::vector<std::pair<value, value>> out;
    for (size_t i = 0; i < st->info->fields.size(); ++i)
        out.push_back({a.vm->makeString(st->info->fields[i]), st->vals[i]});
    return a.vm->makeDict(out);
}

static value fnNTReplace(cffi& a){
    auto* st = ntStateOf(a.self());
    if (!st || !st->info) return a.none();
    std::vector<value> vals = st->vals;
    for (auto& kv : a.kwargv){
        auto it = std::find(st->info->fields.begin(), st->info->fields.end(), kv.first);
        if (it == st->info->fields.end()){
            a.vm->raiseError("TypeError", "Got unexpected field names: ['" + kv.first + "']");
            return a.none();
        }
        vals[static_cast<size_t>(it - st->info->fields.begin())] = kv.second;
    }
    return makeNT(a, st->info, vals);
}

static value fnNTMake(cffi& a){
    // 类级调用: self 是类对象, arg(0) 是可迭代对象
    // class-level call: self is class, arg(0) iterable
    // ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳᠡᠭᠡᠷᠡ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
    value selfVal = a.self();
    NTClassInfo* info = nullptr;
    if (selfVal.type == "native_object" && selfVal.ptr){
        auto* inst = static_cast<ffi_instance*>(selfVal.ptr);
        if (inst->klass && inst->handle == nullptr) info = ntFindClass(inst->klass);
    }
    if (!info){
        auto* st = ntStateOf(selfVal);
        if (st) info = st->info;
    }
    if (!info){
        a.vm->raiseError("TypeError", "_make() called on invalid target");
        return a.none();
    }
    std::vector<value> vals;
    a.vm->materializeIterable(a.arg(0), vals);
    if (vals.size() != info->fields.size()){
        a.vm->raiseError("TypeError", "Expected " + std::to_string(info->fields.size()) +
                         " arguments, got " + std::to_string(vals.size()));
        return a.none();
    }
    return makeNT(a, info, vals);
}

// namedtuple 动态类构造器 (前置声明)
// namedtuple dynamic class constructor (forward decl)
// namedtuple ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnNTCtor(cffi& a);

// namedtuple(typename, field_names, *, rename=False, defaults=None)
// namedtuple
// namedtuple
static value fnNamedTuple(cffi& a){
    if (a.argc() < 2){
        a.vm->raiseError("TypeError", "namedtuple() missing required arguments");
        return a.none();
    }
    std::string typeName = a.arg_str(0);
    std::vector<std::string> fields;
    value fv = a.arg(1);
    if (fv.type == "str" && fv.ptr){
        std::string text = *static_cast<std::string*>(fv.ptr);
        for (char& c : text){
            if (c == ',') c = ' ';
        }
        std::string cur;
        for (char c : text){
            if (std::isspace(static_cast<unsigned char>(c))){
                if (!cur.empty()){
                    fields.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) fields.push_back(cur);
    } else {
        std::vector<value> items;
        a.vm->materializeIterable(fv, items);
        for (auto& it : items) fields.push_back(a.vm->toString(it));
    }
    bool rename = a.kwarg_bool("rename", false);
    if (a.argc() > 2) rename = a.vm->truthy(a.arg(2));
    value defaultsV = a.has_kwarg("defaults") ? a.kwarg("defaults") : a.none();
    if (a.argc() > 3) defaultsV = a.arg(3);

    std::set<std::string> seen;
    for (size_t i = 0; i < fields.size(); ++i){
        std::string f = fields[i];
        bool valid = !f.empty() &&
            (std::isalpha(static_cast<unsigned char>(f[0])) || f[0] == '_');
        for (char c : f){
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) valid = false;
        }
        bool isKw = f == "class" || f == "def" || f == "return" || f == "if" ||
                    f == "else" || f == "for" || f == "while" || f == "in" ||
                    f == "is" || f == "not" || f == "and" || f == "or" ||
                    f == "True" || f == "False" || f == "None" || f == "import" ||
                    f == "from" || f == "as" || f == "pass" || f == "del" ||
                    f == "with" || f == "try" || f == "except" || f == "finally" ||
                    f == "raise" || f == "lambda" || f == "global" || f == "nonlocal" ||
                    f == "assert" || f == "break" || f == "continue" || f == "yield";
        if (seen.count(f)){
            if (rename){
                f = "_" + std::to_string(i);
            } else {
                a.vm->raiseError("ValueError", "Encountered duplicate field name: '" + f + "'");
                return a.none();
            }
        }
        if (!valid){
            if (rename){
                f = "_" + std::to_string(i);
            } else {
                a.vm->raiseError("ValueError",
                    "Type names and field names must be valid identifiers: '" + f + "'");
                return a.none();
            }
        }
        if (isKw){
            if (rename){
                f = "_" + std::to_string(i);
            } else {
                a.vm->raiseError("ValueError",
                    "Type names and field names cannot be a keyword: '" + f + "'");
                return a.none();
            }
        }
        if (!f.empty() && f[0] == '_' && f.size() > 1 && std::isdigit(static_cast<unsigned char>(f[1])) && !rename){
            a.vm->raiseError("ValueError",
                "Field names cannot start with an underscore followed by a digit: '" + f + "'");
            return a.none();
        }
        fields[i] = f;
        seen.insert(f);
    }

    auto info = std::make_unique<NTClassInfo>();
    info->typeName = typeName;
    info->fields = fields;
    if (defaultsV.type == "list" || defaultsV.type == "tuple"){
        auto* dv = static_cast<std::vector<value>*>(defaultsV.ptr);
        size_t off = fields.size() > dv->size() ? fields.size() - dv->size() : 0;
        for (size_t i = 0; i < dv->size() && off + i < fields.size(); ++i)
            info->defaults[fields[off + i]] = (*dv)[i];
    }

    auto* klass = new ffi_class();
    klass->name = typeName;
    klass->constructor = (void*)fnNTCtor;
    klass->methods = {
        {"__repr__", (void*)fnNTRepr},
        {"__str__", (void*)fnNTRepr},
        {"__len__", (void*)fnNTLen},
        {"__getitem__", (void*)fnNTGetitem},
        {"__eq__", (void*)fnNTEq},
        {"__add__", (void*)fnNTAdd},
        {"__hash__", (void*)fnNTHash},
        {"__iter__", (void*)fnNTIter},
        {"__contains__", (void*)fnNTContains},
        {"count", (void*)fnNTCount},
        {"index", (void*)fnNTIndex},
        {"_asdict", (void*)fnNTAsDict},
        {"_replace", (void*)fnNTReplace},
        {"_make", (void*)fnNTMake}
    };
    info->klass = klass;
    NTClassInfo* infoPtr = info.get();
    ntRegistry().push_back(std::move(info));
    ntKlassKeep().push_back(std::unique_ptr<ffi_class>(klass));

    // 类值: native_object, handle=null, klass 指向动态 ffi_class
    // class value: native_object with null handle
    // ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠪᠢᠶᠡ
    value clsVal = a.vm->makeNativeObject(a.library, klass, nullptr);
    if (clsVal.ptr){
        auto* inst = static_cast<ffi_instance*>(clsVal.ptr);
        std::vector<value> fieldsList;
        for (auto& f : fields) fieldsList.push_back(a.vm->makeString(f));
        inst->attributes["_fields"] = a.vm->makeTuple(fieldsList);
        std::vector<std::pair<value, value>> fd;
        for (auto& kv : infoPtr->defaults)
            fd.push_back({a.vm->makeString(kv.first), kv.second});
        inst->attributes["_field_defaults"] = a.vm->makeDict(fd);
    }
    return clsVal;
}

// namedtuple 动态类构造: 由 cffiCallConstructor 调用, currentClass 指向动态类
// namedtuple dynamic class constructor: currentClass identifies the dynamic class
// namedtuple ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnNTCtor(cffi& a){
    NTClassInfo* info = ntFindClass(a.currentClass);
    if (!info){
        a.vm->raiseError("TypeError", "not a namedtuple class");
        return a.none();
    }
    std::vector<value> vals;
    vals.reserve(info->fields.size());
    for (size_t i = 0; i < info->fields.size(); ++i){
        if (i < a.argc()){
            vals.push_back(a.arg(i));
            continue;
        }
        bool found = false;
        for (auto& kv : a.kwargv){
            if (kv.first == info->fields[i]){
                vals.push_back(kv.second);
                found = true;
                break;
            }
        }
        if (found) continue;
        auto it = info->defaults.find(info->fields[i]);
        if (it != info->defaults.end()){
            vals.push_back(it->second);
            continue;
        }
        a.vm->raiseError("TypeError", info->typeName + ".__new__() missing 1 required positional argument: '" +
                         info->fields[i] + "'");
        return a.none();
    }
    if (a.argc() > info->fields.size()){
        a.vm->raiseError("TypeError", info->typeName + ".__new__() takes " +
                         std::to_string(info->fields.size() + 1) + " positional arguments but " +
                         std::to_string(a.argc() + 1) + " were given");
        return a.none();
    }
    auto* st = new NTState();
    st->info = info;
    st->vals = vals;
    value obj = a.mkobject(st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        for (size_t i = 0; i < info->fields.size(); ++i)
            inst->attributes[info->fields[i]] = vals[i];
    }
    return obj;
}

// ============ 注册 ============
// registration
// ᠪᠦᠷᠢᠲᠭᠡᠯ

// 统一迭代器 __next__ 定义: 按目标容器类型分发
// unified iterator __next__: dispatch by target container type
// ᠨᠢᠶᠢᠲᠦ ᠳᠠᠶᠢᠯᠤᠭᠴᠢ __next__
static value fnIterNext(cffi& a){
    auto* is = static_cast<IterState*>(static_cast<ffi_instance*>(a.self().ptr)->handle);
    if (!is){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    if (auto* dq = dequeOf(is->target)){
        if (is->index >= static_cast<long long>(dq->items.size())){
            a.vm->raiseError("StopIteration", "");
            return a.none();
        }
        return dq->items[static_cast<size_t>(is->index++)];
    }
    if (auto* dl = dictLikeOf(is->target)){
        if (is->index >= static_cast<long long>(dl->items.size())){
            a.vm->raiseError("StopIteration", "");
            return a.none();
        }
        return dl->items[static_cast<size_t>(is->index++)].first;
    }
    if (auto* nt = ntStateOf(is->target)){
        if (is->index >= static_cast<long long>(nt->vals.size())){
            a.vm->raiseError("StopIteration", "");
            return a.none();
        }
        return nt->vals[static_cast<size_t>(is->index++)];
    }
    a.vm->raiseError("StopIteration", "");
    return a.none();
}

ffilib cffi_collections_lib(){
    ffi_class iterCls;
    iterCls.name = "CollectionsIterator";
    iterCls.constructor = nullptr;
    iterCls.methods = {
        {"__next__", (void*)fnIterNext}
    };

    ffi_class dequeCls;
    dequeCls.name = "deque";
    dequeCls.constructor = (void*)fnDequeCtor;
    dequeCls.methods = {
        {"append", (void*)fnDequeAppend},
        {"appendleft", (void*)fnDequeAppendLeft},
        {"pop", (void*)fnDequePop},
        {"popleft", (void*)fnDequePopLeft},
        {"extend", (void*)fnDequeExtend},
        {"extendleft", (void*)fnDequeExtendLeft},
        {"clear", (void*)fnDequeClear},
        {"copy", (void*)fnDequeCopy},
        {"count", (void*)fnDequeCount},
        {"index", (void*)fnDequeIndex},
        {"insert", (void*)fnDequeInsert},
        {"remove", (void*)fnDequeRemove},
        {"reverse", (void*)fnDequeReverse},
        {"rotate", (void*)fnDequeRotate},
        {"__len__", (void*)fnDequeLen},
        {"__getitem__", (void*)fnDequeGetitem},
        {"__setitem__", (void*)fnDequeSetitem},
        {"__delitem__", (void*)fnDequeDelitem},
        {"__contains__", (void*)fnDequeContains},
        {"__repr__", (void*)fnDequeRepr},
        {"__eq__", (void*)fnDequeEq},
        {"__add__", (void*)fnDequeAdd},
        {"__mul__", (void*)fnDequeMul},
        {"__iter__", (void*)fnDequeIter}
    };

    ffi_class counterCls;
    counterCls.name = "Counter";
    counterCls.constructor = (void*)fnCounterCtor;
    counterCls.methods = {
        {"elements", (void*)fnCounterElements},
        {"most_common", (void*)fnCounterMostCommon},
        {"total", (void*)fnCounterTotal},
        {"update", (void*)fnCounterUpdate},
        {"subtract", (void*)fnCounterSubtract},
        {"__add__", (void*)fnCounterAdd},
        {"__sub__", (void*)fnCounterSub},
        {"__and__", (void*)fnCounterAnd},
        {"__or__", (void*)fnCounterOr},
        {"__eq__", (void*)fnCounterEq},
        {"__len__", (void*)fnDictLikeLen},
        {"__getitem__", (void*)fnDictLikeGetitem},
        {"__setitem__", (void*)fnDictLikeSetitem},
        {"__delitem__", (void*)fnDictLikeDelitem},
        {"__contains__", (void*)fnDictLikeContains},
        {"get", (void*)fnDictLikeGet},
        {"keys", (void*)fnDictLikeKeys},
        {"values", (void*)fnDictLikeValues},
        {"items", (void*)fnDictLikeItems},
        {"pop", (void*)fnDictLikePop},
        {"setdefault", (void*)fnDictLikeSetdefault},
        {"clear", (void*)fnDictLikeClear},
        {"__repr__", (void*)fnDictLikeRepr},
        {"__iter__", (void*)fnDictLikeIter}
    };

    ffi_class orderedCls;
    orderedCls.name = "OrderedDict";
    orderedCls.constructor = (void*)fnOrderedCtor;
    orderedCls.methods = {
        {"move_to_end", (void*)fnOrderedMoveToEnd},
        {"popitem", (void*)fnOrderedPopitem},
        {"__eq__", (void*)fnOrderedEq},
        {"__len__", (void*)fnDictLikeLen},
        {"__getitem__", (void*)fnDictLikeGetitem},
        {"__setitem__", (void*)fnDictLikeSetitem},
        {"__delitem__", (void*)fnDictLikeDelitem},
        {"__contains__", (void*)fnDictLikeContains},
        {"get", (void*)fnDictLikeGet},
        {"keys", (void*)fnDictLikeKeys},
        {"values", (void*)fnDictLikeValues},
        {"items", (void*)fnDictLikeItems},
        {"update", (void*)fnDictLikeUpdate},
        {"pop", (void*)fnDictLikePop},
        {"setdefault", (void*)fnDictLikeSetdefault},
        {"clear", (void*)fnDictLikeClear},
        {"__repr__", (void*)fnDictLikeRepr},
        {"__iter__", (void*)fnDictLikeIter}
    };

    ffi_class defaultdictCls;
    defaultdictCls.name = "defaultdict";
    defaultdictCls.constructor = (void*)fnDefaultdictCtor;
    defaultdictCls.methods = {
        {"__len__", (void*)fnDictLikeLen},
        {"__getitem__", (void*)fnDictLikeGetitem},
        {"__setitem__", (void*)fnDictLikeSetitem},
        {"__delitem__", (void*)fnDictLikeDelitem},
        {"__contains__", (void*)fnDictLikeContains},
        {"get", (void*)fnDictLikeGet},
        {"keys", (void*)fnDictLikeKeys},
        {"values", (void*)fnDictLikeValues},
        {"items", (void*)fnDictLikeItems},
        {"update", (void*)fnDictLikeUpdate},
        {"pop", (void*)fnDictLikePop},
        {"setdefault", (void*)fnDictLikeSetdefault},
        {"clear", (void*)fnDictLikeClear},
        {"__repr__", (void*)fnDictLikeRepr},
        {"__iter__", (void*)fnDictLikeIter}
    };

    ffi_class chainMapCls;
    chainMapCls.name = "ChainMap";
    chainMapCls.constructor = (void*)fnChainMapCtor;
    chainMapCls.methods = {
        {"__getitem__", (void*)fnChainMapGetitem},
        {"__setitem__", (void*)fnChainMapSetitem},
        {"__delitem__", (void*)fnChainMapDelitem},
        {"__contains__", (void*)fnChainMapContains},
        {"__len__", (void*)fnChainMapLen},
        {"keys", (void*)fnChainMapKeys},
        {"values", (void*)fnChainMapValues},
        {"items", (void*)fnChainMapItems},
        {"get", (void*)fnChainMapGet},
        {"new_child", (void*)fnChainMapNewChild},
        {"parents", (void*)fnChainMapParents},
        {"__repr__", (void*)fnChainMapRepr}
    };

    return cffi_lib(
        "collections",
        {
            {"namedtuple", (void*)fnNamedTuple}
        },
        {},
        {
            {"deque", dequeCls},
            {"Counter", counterCls},
            {"OrderedDict", orderedCls},
            {"defaultdict", defaultdictCls},
            {"ChainMap", chainMapCls},
            {"CollectionsIterator", iterCls}
        });
}

}
