#include "../cffi.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// SimpleNamespace 状态
// SimpleNamespace state
// SimpleNamespace ᠪᠠᠶᠢᠳᠠᠯ
struct NsState{
    int dummy = 0;
};

// MappingProxyType 状态
// MappingProxyType state
// MappingProxyType ᠪᠠᠶᠢᠳᠠᠯ
struct ProxyState{
    value target;
};

ProxyState* proxyOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass || inst->klass->name != "MappingProxyType") return nullptr;
    return static_cast<ProxyState*>(inst->handle);
}

dict_t* proxyDict(ProxyState* st){
    if (!st) return nullptr;
    if (st->target.type == "dict" && st->target.ptr)
        return static_cast<dict_t*>(st->target.ptr);
    return nullptr;
}

value dataTypeObj(cffi& a, const char* name){
    value v = a.vm->getBuiltinTypeObj(name);
    if (v.type == "none")
        v = a.vm->makeDynamicObject(name, nullptr, nullptr, nullptr);
    return v;
}

#define TYPE_DATA_FN(fnName, typeName) \
    static value fnName(cffi& a){ return dataTypeObj(a, typeName); }

TYPE_DATA_FN(dataNoneType, "NoneType")
TYPE_DATA_FN(dataFunctionType, "function")
TYPE_DATA_FN(dataGeneratorType, "generator")
TYPE_DATA_FN(dataCoroutineType, "coroutine")
TYPE_DATA_FN(dataAsyncGenType, "async_generator")
TYPE_DATA_FN(dataMethodType, "method")
TYPE_DATA_FN(dataBuiltinFnType, "builtin_function_or_method")
TYPE_DATA_FN(dataModuleType, "module")
TYPE_DATA_FN(dataCodeType, "code")
TYPE_DATA_FN(dataCellType, "cell")
TYPE_DATA_FN(dataUnionType, "UnionType")
TYPE_DATA_FN(dataEllipsisType, "ellipsis")
TYPE_DATA_FN(dataNotImplType, "NotImplementedType")

}

// SimpleNamespace 构造: 仅关键字参数
// SimpleNamespace constructor: kwargs only
// SimpleNamespace ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnNsCtor(cffi& a){
    value obj = a.mkobject(new NsState());
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        for (auto& kv : a.kwargv){
            inst->attributes[kv.first] = kv.second;
        }
    }
    return obj;
}

static value fnNsRepr(cffi& a){
    value self = a.self();
    std::string text = "namespace(";
    bool first = true;
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        std::vector<std::string> keys;
        for (auto& kv : inst->attributes) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (auto& k : keys){
            if (!first) text += ", ";
            first = false;
            text += k + "=" + pythonRepr(*a.vm, inst->attributes[k]);
        }
    }
    text += ")";
    return a.mkstr(text);
}

static value fnNsEq(cffi& a){
    value self = a.self();
    value other = a.arg(0);
    if (self.type != "native_object" || other.type != "native_object" ||
        !self.ptr || !other.ptr)
        return a.mkbool(false);
    auto* li = static_cast<ffi_instance*>(self.ptr);
    auto* ri = static_cast<ffi_instance*>(other.ptr);
    if (!li->klass || !ri->klass || li->klass->name != "SimpleNamespace" ||
        ri->klass->name != "SimpleNamespace")
        return a.mkbool(false);
    if (li->attributes.size() != ri->attributes.size()) return a.mkbool(false);
    for (auto& kv : li->attributes){
        auto it = ri->attributes.find(kv.first);
        if (it == ri->attributes.end()) return a.mkbool(false);
        if (!a.vm->valuesEqual(kv.second, it->second)) return a.mkbool(false);
    }
    return a.mkbool(true);
}

static value fnNsNe(cffi& a){
    value r = fnNsEq(a);
    if (a.vm->hasError()) return a.none();
    return a.mkbool(!a.vm->truthy(r));
}

// MappingProxyType 构造
// MappingProxyType constructor
// MappingProxyType ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnProxyCtor(cffi& a){
    if (a.argc() != 1 || a.arg(0).type != "dict"){
        a.vm->raiseError("TypeError", "mappingproxy() argument must be a mapping");
        return a.none();
    }
    auto* st = new ProxyState();
    st->target = a.arg(0);
    return a.mkobject(st);
}

static value fnProxyRepr(cffi& a){
    auto* st = proxyOf(a.self());
    if (!st) return a.mkstr("mappingproxy({})");
    return a.mkstr("mappingproxy(" + pythonRepr(*a.vm, st->target) + ")");
}

static value fnProxyStr(cffi& a){
    auto* st = proxyOf(a.self());
    if (!st) return a.mkstr("{}");
    return a.mkstr(a.vm->toString(st->target));
}

static value fnProxySetitem(cffi& a){
    (void)a;
    a.vm->raiseError("TypeError", "'mappingproxy' object does not support item assignment");
    return a.none();
}

static value fnProxyDelitem(cffi& a){
    (void)a;
    a.vm->raiseError("TypeError", "'mappingproxy' object does not support item deletion");
    return a.none();
}

static value fnProxyGetitem(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    if (!d){
        a.vm->raiseError("TypeError", "mappingproxy target is not a dict");
        return a.none();
    }
    value key = a.arg(0);
    for (auto& kv : *d){
        if (a.vm->valuesEqual(kv.first, key)) return kv.second;
    }
    a.vm->raiseError("KeyError", pythonRepr(*a.vm, key));
    return a.none();
}

static value fnProxyLen(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    return a.mkint(d ? static_cast<long long>(d->size()) : 0);
}

static value fnProxyContains(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    if (!d) return a.mkbool(false);
    value key = a.arg(0);
    for (auto& kv : *d){
        if (a.vm->valuesEqual(kv.first, key)) return a.mkbool(true);
    }
    return a.mkbool(false);
}

static value fnProxyGet(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    value key = a.arg(0);
    value fallback = a.argc() > 1 ? a.arg(1) : a.none();
    if (d){
        for (auto& kv : *d){
            if (a.vm->valuesEqual(kv.first, key)) return kv.second;
        }
    }
    return fallback;
}

static value fnProxyKeys(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    std::vector<value> out;
    if (d){
        for (auto& kv : *d) out.push_back(kv.first);
    }
    return a.vm->makeList(out);
}

static value fnProxyValues(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    std::vector<value> out;
    if (d){
        for (auto& kv : *d) out.push_back(kv.second);
    }
    return a.vm->makeList(out);
}

static value fnProxyItems(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    std::vector<value> out;
    if (d){
        for (auto& kv : *d) out.push_back(a.vm->makeTuple({kv.first, kv.second}));
    }
    return a.vm->makeList(out);
}

static value fnProxyCopy(cffi& a){
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    if (!d) return a.vm->makeDict({});
    return a.vm->makeDict(*d);
}

static value fnProxyIter(cffi& a){
    // 迭代键: 用列表迭代协议由 VM 处理 → 返回列表自身, __next__ 由 VM 处理 list
    // iterate keys
    // ᠲᠤᠯᠬᠢᠭᠤᠷ ᠢ ᠳᠠᠶᠢᠨ᠎ᠠ
    auto* st = proxyOf(a.self());
    auto* d = proxyDict(st);
    std::vector<value> out;
    if (d){
        for (auto& kv : *d) out.push_back(kv.first);
    }
    return a.vm->makeList(out);
}

// new_class(name, bases=(), kwds=None, exec_body=None)
// new_class
// ᠰᠢᠨ᠎ᠡ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
static value fnNewClass(cffi& a){
    if (a.argc() < 1 || a.arg(0).type != "str"){
        a.vm->raiseError("TypeError", "new_class() argument 1 must be str");
        return a.none();
    }
    std::string name = *static_cast<std::string*>(a.arg(0).ptr);
    value basesVal = a.argc() > 1 ? a.arg(1) : a.vm->makeTuple({});
    value execBody = a.argc() > 3 ? a.arg(3) : a.none();
    if (a.has_kwarg("exec_body")) execBody = a.kwarg("exec_body");
    if (a.has_kwarg("bases")) basesVal = a.kwarg("bases");

    auto* klass = new class_t();
    klass->name = name;
    klass->attributes = new std::map<std::string, value>();

    std::vector<value> bases;
    a.vm->materializeIterable(basesVal, bases);
    if (!bases.empty() && bases[0].type == "class" && bases[0].ptr){
        klass->baseClass = static_cast<class_t*>(bases[0].ptr);
    }

    if (execBody.type == "function" || execBody.type == "bound_method"){
        value ns = a.vm->makeDict({});
        a.vm->callValue(execBody, {ns});
        if (a.vm->hasError()) return a.none();
        if (ns.type == "dict" && ns.ptr){
            auto* d = static_cast<dict_t*>(ns.ptr);
            for (auto& kv : *d){
                if (kv.first.type == "str" && kv.first.ptr){
                    std::string key = *static_cast<std::string*>(kv.first.ptr);
                    function_t newMethod;
                    if (kv.second.type == "function" && kv.second.ptr){
                        newMethod = *static_cast<function_t*>(kv.second.ptr);
                        newMethod.name = key;
                        klass->methods.push_back(newMethod);
                    } else if (closureFunctionCopy(kv.second, newMethod)){
                        newMethod.name = key;
                        klass->methods.push_back(newMethod);
                    } else {
                        (*static_cast<std::map<std::string, value>*>(klass->attributes))[key] = kv.second;
                    }
                }
            }
        }
    }
    return a.vm->adoptClass(klass);
}

// cffi types 库
// cffi types library
// cffi types ᠨᠣᠮ
ffilib cffi_types_lib(){
    ffi_class nsCls;
    nsCls.name = "SimpleNamespace";
    nsCls.constructor = (void*)fnNsCtor;
    nsCls.methods = {
        {"__repr__", (void*)fnNsRepr},
        {"__eq__", (void*)fnNsEq},
        {"__ne__", (void*)fnNsNe}
    };

    ffi_class proxyCls;
    proxyCls.name = "MappingProxyType";
    proxyCls.constructor = (void*)fnProxyCtor;
    proxyCls.methods = {
        {"__repr__", (void*)fnProxyRepr},
        {"__str__", (void*)fnProxyStr},
        {"__setitem__", (void*)fnProxySetitem},
        {"__delitem__", (void*)fnProxyDelitem},
        {"__getitem__", (void*)fnProxyGetitem},
        {"__len__", (void*)fnProxyLen},
        {"__contains__", (void*)fnProxyContains},
        {"get", (void*)fnProxyGet},
        {"keys", (void*)fnProxyKeys},
        {"values", (void*)fnProxyValues},
        {"items", (void*)fnProxyItems},
        {"copy", (void*)fnProxyCopy},
        {"__iter__", (void*)fnProxyIter}
    };

    return cffi_lib(
        "types",
        {
            {"new_class", (void*)fnNewClass}
        },
        {
            {"NoneType", (void*)dataNoneType},
            {"FunctionType", (void*)dataFunctionType},
            {"LambdaType", (void*)dataFunctionType},
            {"GeneratorType", (void*)dataGeneratorType},
            {"CoroutineType", (void*)dataCoroutineType},
            {"AsyncGeneratorType", (void*)dataAsyncGenType},
            {"MethodType", (void*)dataMethodType},
            {"BuiltinFunctionType", (void*)dataBuiltinFnType},
            {"BuiltinMethodType", (void*)dataBuiltinFnType},
            {"ModuleType", (void*)dataModuleType},
            {"CodeType", (void*)dataCodeType},
            {"CellType", (void*)dataCellType},
            {"UnionType", (void*)dataUnionType},
            {"EllipsisType", (void*)dataEllipsisType},
            {"NotImplementedType", (void*)dataNotImplType}
        },
        {
            {"SimpleNamespace", nsCls},
            {"MappingProxyType", proxyCls}
        });
}

}
