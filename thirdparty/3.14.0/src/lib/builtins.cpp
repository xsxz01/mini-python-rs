#include "../cffi.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace minipy {





// fn 打印
// fn print
// fn ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
static minipy::value fnPrint(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::string sep = " ";
    std::string end = "\n";
    if (args.has_kwarg("sep")){
        minipy::value v = args.kwarg("sep");
        if (v.type != "none") sep = vm.toString(v);
    }
    if (args.has_kwarg("end")){
        minipy::value v = args.kwarg("end");
        if (v.type != "none") end = vm.toString(v);
    }
    for (size_t i = 0; i < args.argc(); ++i){
        if (i > 0) std::cout << sep;
        std::cout << vm.toString(args.arg(i));
    }
    std::cout << end;
    std::cout.flush();
    return args.none();
}

// fn Input
// fn input
// fn Input
static minipy::value fnInput(minipy::cffi& args){
    if (args.argc() > 0) std::cout << args.arg_str(0);
    std::string line;
    std::getline(std::cin, line);
    return args.mkstr(line);
}

// fn Len
// fn len
// fn Len
static minipy::value fnLen(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() != 1){
        vm.raiseError("TypeError", "len() takes exactly one argument (" +
                                       std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value value = args.arg(0);
    if (value.type == "str"){
        return args.mkint(static_cast<long long>(vm.toString(value).size()));
    }
    if (value.type == "list" || value.type == "tuple"){
        return args.mkint(static_cast<long long>(
            static_cast<std::vector<minipy::value>*>(value.ptr)->size()));
    }
    if (value.type == "dict"){
        return args.mkint(static_cast<long long>(
            static_cast<minipy::dict_t*>(value.ptr)->size()));
    }
    if (value.type == "dict_keys" || value.type == "dict_values" ||
        value.type == "dict_items"){
        auto* view = static_cast<minipy::dict_view*>(value.ptr);
        return args.mkint(static_cast<long long>(
            static_cast<minipy::dict_t*>(view->dict.ptr)->size()));
    }
    if (value.type == "range" && value.ptr){
        return args.mkint(static_cast<minipy::range_t*>(value.ptr)->count());
    }
    if (value.type == "bytes"){
        return args.mkint(static_cast<long long>(
            static_cast<std::string*>(value.ptr)->size()));
    }

// minipy
// minipy
// minipy
    struct minipy::value container = vm.containerOf(value);
    if (container.type == "dict" && container.ptr){
        return args.mkint(static_cast<long long>(static_cast<minipy::dict_t*>(container.ptr)->size()));
    }
    if (container.type == "list" && container.ptr){
        return args.mkint(static_cast<long long>(
            static_cast<std::vector<minipy::value>*>(container.ptr)->size()));
    }
    std::string typeName = value.type == "none" ? "NoneType" : value.type;
    vm.raiseError("TypeError", "object of type '" + typeName + "' has no len()");
    return args.none();
}

// fn Str
// fn str
// fn Str
static minipy::value fnStr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.mkstr("");
    return args.mkstr(vm.toString(args.arg(0)));
}


// fn Repr
// fn repr
// fn Repr
static minipy::value fnRepr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.mkstr("");
    minipy::value value = args.arg(0);

    minipy::function_t* reprMethod = vm.findObjectMethod(value, "__repr__");
    if (reprMethod){
        minipy::value selfCopy = value;
        minipy::value r = vm.invoke(reprMethod, {}, &selfCopy);
        if (!vm.hasError() && r.type == "str" && r.ptr){
            return args.mkstr(*static_cast<std::string*>(r.ptr));
        }
        vm.clearErr();
    }

    return args.mkstr(pythonRepr(vm, value));
}

// fn 整数
// fn int
// fn ᠳ᠋ᠠᠩ ᠲᠣ᠎
static minipy::value fnInt(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() > 2){
        vm.raiseError("TypeError", "int expected at most 2 arguments, got " +
                                       std::to_string(args.argc()));
        return args.none();
    }
    if (args.argc() == 0) return args.mkint(0);
    minipy::value value = args.arg(0);
    if (args.argc() >= 2 && value.type != "str"){
        vm.raiseError("TypeError", "int() can't convert non-string with explicit base");
        return args.none();
    }
    if (value.type == "str"){
        std::string text = vm.toString(value);
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) begin++;
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
        std::string trimmed = text.substr(begin, end - begin);
        if (trimmed.empty()){
            vm.raiseError("ValueError", "invalid literal for int() with base 10: '" + text + "'");
            return args.none();
        }
        char* parseEnd = nullptr;
        errno = 0;
        long long result = std::strtoll(trimmed.c_str(), &parseEnd, 10);
        if (errno != 0 || parseEnd == nullptr || *parseEnd != '\0'){
            vm.raiseError("ValueError", "invalid literal for int() with base 10: '" + text + "'");
            return args.none();
        }
        return args.mkint(result);
    }
    return args.mkint(vm.toInt(value));
}

// fn 浮点
// fn float
// fn ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ
static minipy::value fnFloat(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.mkfloat(0.0);
    if (args.argc() > 1){
        vm.raiseError("TypeError", "float() takes at most 1 argument (" +
                                       std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value value = args.arg(0);
    if (value.type != "str" && value.type != "int" && value.type != "float" &&
        value.type != "bool"){
        std::string typeName = value.type == "none" ? "NoneType" : value.type;
        vm.raiseError("TypeError", "float() argument must be a string or a real number, not '" +
                                       typeName + "'");
        return args.none();
    }
    if (value.type == "str"){
        std::string text = vm.toString(value);
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) begin++;
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
        std::string trimmed = text.substr(begin, end - begin);
        char* parseEnd = nullptr;
        errno = 0;
        double result = std::strtod(trimmed.c_str(), &parseEnd);
        if (trimmed.empty() || errno != 0 || parseEnd == nullptr || *parseEnd != '\0'){
            vm.raiseError("ValueError", "could not convert string to float: '" + text + "'");
            return args.none();
        }
        return args.mkfloat(result);
    }
    return args.mkfloat(vm.toFloat(value));
}

// fn 布尔
// fn bool
// fn ᠪᠤᠯ
static minipy::value fnBool(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.mkbool(false);
    return args.mkbool(vm.truthy(args.arg(0)));
}

// fn Abs
// fn abs
// fn Abs
static minipy::value fnAbs(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() != 1){
        vm.raiseError("TypeError", "abs() takes exactly one argument (" +
                                       std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value value = args.arg(0);
    if (value.type != "int" && value.type != "float" && value.type != "bool"){
        std::string typeName = value.type == "none" ? "NoneType" : value.type;
        vm.raiseError("TypeError", "bad operand type for abs(): '" + typeName + "'");
        return args.none();
    }
    if (value.type == "float"){
        return args.mkfloat(std::fabs(vm.toFloat(value)));
    }
    long long number = vm.toInt(value);
    return args.mkint(number < 0 ? -number : number);
}

// fn Min Max
// fn min max
// fn Min Max
static minipy::value fnMinMax(minipy::cffi& args, bool wantMax){
    minipy::vm& vm = *args.vm;
    std::string funcName = wantMax ? "max" : "min";
    std::vector<minipy::value> items;
    if (args.argc() == 0){
        vm.raiseError("TypeError", funcName + " expected at least 1 argument, got 0");
        return args.none();
    }
    if (args.argc() == 1){
        vm.materializeIterable(args.arg(0), items);
    } else {
        for (size_t i = 0; i < args.argc(); ++i) items.push_back(args.arg(i));
    }
    if (items.empty()){
        vm.raiseError("ValueError", funcName + "() iterable argument is empty");
        return args.none();
    }
    auto numeric = [](const minipy::value& value){
        return value.type == "int" || value.type == "float" || value.type == "bool";
    };
    auto typeName = [](const minipy::value& value){
        return value.type == "none" ? std::string("NoneType") : value.type;
    };
    auto less = [&vm](const minipy::value& a, const minipy::value& b){
        if (a.type == "str" && b.type == "str"){
            return vm.toString(a) < vm.toString(b);
        }
        return vm.toFloat(a) < vm.toFloat(b);
    };
    minipy::value best = items[0];
    for (size_t i = 1; i < items.size(); ++i){
        bool compatible = (items[i].type == "str" && best.type == "str") ||
                          (numeric(items[i]) && numeric(best));
        if (!compatible){
            std::string op = wantMax ? "'>'" : "'<'";
            vm.raiseError("TypeError", op + " not supported between instances of '" +
                                           typeName(items[i]) + "' and '" +
                                           typeName(best) + "'");
            return args.none();
        }
        bool replace = wantMax ? less(best, items[i]) : less(items[i], best);
        if (replace) best = items[i];
    }
    return best;
}

// fn Min
// fn min
// fn Min
static minipy::value fnMin(minipy::cffi& args){
    return fnMinMax(args, false);
}

// fn Max
// fn max
// fn Max
static minipy::value fnMax(minipy::cffi& args){
    return fnMinMax(args, true);
}

// fn Sum
// fn sum
// fn Sum
static minipy::value fnSum(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.mkint(0);
    std::vector<minipy::value> items;
    vm.materializeIterable(args.arg(0), items);
    minipy::value total = (args.argc() > 1) ? args.arg(1) : args.mkint(0);
    for (auto& item : items){
        total = vm.applyBinary("+", total, item);
    }
    return total;
}

// fn 范围
// fn range
// fn ᠬᠠᠷᠢᠶ᠎ᠠ
static minipy::value fnRange(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    long long start = 0;
    long long stop = 0;
    long long step = 1;
    if (args.argc() == 1){
        stop = args.arg_int(0);
    } else if (args.argc() >= 2){
        start = args.arg_int(0);
        stop = args.arg_int(1);
        if (args.argc() >= 3) step = args.arg_int(2);
    }
    if (step == 0){
        vm.raiseError("ValueError", "range() arg 3 must not be zero");
        return args.none();
    }

    return vm.makeRange(start, stop, step);
}

// fn 列表
// fn list
// fn ᠲᠠᠯᠠᠭ᠌
static minipy::value fnList(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() > 0) vm.materializeIterable(args.arg(0), items);
    return args.mklist(items);
}

// 字典 赋值
// dict assign
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠣᠷᠣᠭᠤᠯᠭ᠎ᠠ
static void dictAssign(minipy::vm& vm, minipy::dict_t& target, const minipy::value& key, const minipy::value& value){
    if (!vm.hashable(key)){
        std::string keyType = key.type == "none" ? "NoneType" : key.type;
        vm.raiseError("TypeError", "cannot use '" + keyType +
                                   "' as a dict key (unhashable type: '" + keyType + "')");
        return;
    }
    for (auto& existing : target){
        if (vm.valuesEqual(existing.first, key)){
            existing.second = value;
            return;
        }
    }
    target.push_back({key, value});
}

// fn 字典
// fn dict
// fn ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
static minipy::value fnDict(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::dict_t result;
    if (args.argc() > 0){
        minipy::value source = args.arg(0);
        if (source.type == "dict"){
            for (auto& item : *static_cast<minipy::dict_t*>(source.ptr)){
                dictAssign(vm, result, item.first, item.second);
                if (vm.hasError()) return args.none();
            }
        } else {
            std::vector<minipy::value> pairs;
            vm.materializeIterable(source, pairs);
            for (size_t i = 0; i < pairs.size(); ++i){
                std::vector<minipy::value>* entry = nullptr;
                if ((pairs[i].type == "list" || pairs[i].type == "tuple") && pairs[i].ptr){
                    entry = static_cast<std::vector<minipy::value>*>(pairs[i].ptr);
                }
                if (entry == nullptr){
                    vm.raiseError("TypeError", "cannot convert dictionary update sequence element #" +
                                                   std::to_string(i) + " to a sequence");
                    return args.none();
                }
                if (entry->size() != 2){
                    vm.raiseError("ValueError", "dictionary update sequence element #" +
                                                    std::to_string(i) + " has length " +
                                                    std::to_string(entry->size()) + "; 2 is required");
                    return args.none();
                }
                dictAssign(vm, result, (*entry)[0], (*entry)[1]);
                if (vm.hasError()) return args.none();
            }
        }
    }

    for (auto& entry : args.kwargv){
        dictAssign(vm, result, args.mkstr(entry.first), entry.second);
        if (vm.hasError()) return args.none();
    }
    return args.mkdict(result);
}

// fn 元组
// fn tuple
// fn ᠬᠣᠶᠠᠳᠠᠯ ᠲᠠᠯᠠᠭ᠌
static minipy::value fnTuple(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() > 0) vm.materializeIterable(args.arg(0), items);
    return vm.makeTuple(items);
}

// fn 类型
// fn type
// fn ᠲᠥᠷᠥᠯ
static minipy::value fnType(minipy::cffi& args){
    if (args.argc() == 0){
        minipy::vm& vm = *args.vm;
        vm.raiseError("TypeError", "type() takes at least 1 argument (0 given)");
        return args.none();
    }
    if (args.argc() == 1){
        minipy::vm& vm = *args.vm;
        minipy::value v = args.arg(0);

        if (v.type == "object"){
            minipy::class_t* klass = vm.getClassOfScriptObject(v);
            if (klass) return minipy::value("class", klass, &vm);
            return vm.makeNone();
        }

        if (v.type == "native_object" && v.ptr){
            auto* inst = static_cast<minipy::ffi_instance*>(v.ptr);
            if (inst->typeName == "exception" && inst->handle){
                auto* payload = static_cast<minipy::ExceptionPayload*>(inst->handle);
                minipy::value tobj = vm.getBuiltinTypeObj(payload->type);
                if (tobj.type != "none") return tobj;

                auto* ni = new minipy::ffi_instance();
                ni->library = nullptr;
                ni->klass = nullptr;
                ni->typeName = payload->type;
                ni->handle = nullptr;
                return vm.makeDynamicObject(payload->type, nullptr, nullptr, nullptr);
            }
            if (inst->klass) return minipy::value("class", const_cast<minipy::ffi_class*>(inst->klass), &vm);
            return vm.getBuiltinTypeObj(inst->typeName);
        }

        if (v.type == "class") return vm.getBuiltinTypeObj("type");
        if (v.type == "function") return vm.getBuiltinTypeObj("function");
        if (v.type == "generator") return vm.getBuiltinTypeObj("generator");

        static const std::map<std::string, std::string> typeMap = {
            {"int", "int"}, {"float", "float"}, {"bool", "bool"},
            {"str", "str"}, {"list", "list"}, {"tuple", "tuple"},
            {"dict", "dict"}, {"none", "NoneType"}, {"range", "range"}
        };
        auto it = typeMap.find(v.type);
        if (it != typeMap.end()){
            return vm.getBuiltinTypeObj(it->second);
        }

        return args.mkstr(vm.pythonTypeName(v));
    }
    if (args.argc() == 3){

        minipy::vm& vm = *args.vm;
        minipy::value nameVal = args.arg(0);
        minipy::value basesVal = args.arg(1);
        minipy::value nsVal = args.arg(2);
        if (nameVal.type != "str"){
            vm.raiseError("TypeError", "type() argument 1 must be str, not '" + vm.pythonTypeName(nameVal) + "'");
            return args.none();
        }
        if (nsVal.type != "dict"){
            vm.raiseError("TypeError", "type() argument 3 must be dict, not '" + vm.pythonTypeName(nsVal) + "'");
            return args.none();
        }
        std::string className = vm.toString(nameVal);

        std::vector<minipy::value> basesList;
        if (basesVal.type == "tuple" || basesVal.type == "list"){
            auto* items = static_cast<std::vector<minipy::value>*>(basesVal.ptr);
            if (items){
                for (auto& b : *items) basesList.push_back(b);
            }
        } else if (basesVal.type == "class"){
            basesList.push_back(basesVal);
        }
        return vm.createDynamicClass(className, basesList, nsVal);
    }

    minipy::vm& vm = *args.vm;
    vm.raiseError("TypeError", "type() takes 1 or 3 arguments (" +
                                   std::to_string(args.argc()) + " given)");
    return args.none();
}

// fn Round
// fn round
// fn Round
static minipy::value fnRound(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    minipy::value value = args.arg(0);
    if (value.type == "int" || value.type == "bool"){
        return args.mkint(vm.toInt(value));
    }
    double number = vm.toFloat(value);
    if (args.argc() >= 2){
        long long digits = args.arg_int(1);
        if (digits >= 0 && digits <= 17){

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(digits), number);
            return args.mkfloat(std::strtod(buffer, nullptr));
        }
        double scale = std::pow(10.0, static_cast<double>(digits));
        return args.mkfloat(std::nearbyint(number * scale) / scale);
    }

    return args.mkint(static_cast<long long>(std::nearbyint(number)));
}

// fn Sorted
// fn sorted
// fn Sorted
static minipy::value fnSorted(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() > 0) vm.materializeIterable(args.arg(0), items);
    minipy::value keyFunc = args.has_kwarg("key") ? args.kwarg("key") : args.none();
    bool reverse = args.has_kwarg("reverse") && args.kwarg_bool("reverse", false);
    bool hasKey = keyFunc.type != "none";
    std::vector<std::pair<minipy::value, size_t>> keyed;
    keyed.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i){
        minipy::value k = hasKey ? vm.callValue(keyFunc, {items[i]}) : items[i];
        keyed.push_back({k, i});
    }
    auto less = [&vm](const std::pair<minipy::value, size_t>& a, const std::pair<minipy::value, size_t>& b){
        const minipy::value& x = a.first;
        const minipy::value& y = b.first;
        if (x.type == "str" && y.type == "str") return vm.toString(x) < vm.toString(y);
        if (x.type == "tuple" && y.type == "tuple"){
            auto* xv = static_cast<std::vector<minipy::value>*>(x.ptr);
            auto* yv = static_cast<std::vector<minipy::value>*>(y.ptr);
            size_t n = std::min(xv->size(), yv->size());
            for (size_t i = 0; i < n; ++i){
                if (vm.valuesEqual(xv->at(i), yv->at(i))) continue;
                return vm.truthy(vm.applyBinary("<", xv->at(i), yv->at(i)));
            }
            return xv->size() < yv->size();
        }
        return vm.toFloat(x) < vm.toFloat(y);
    };
    if (reverse){
        std::stable_sort(keyed.begin(), keyed.end(), [&](const auto& a, const auto& b){
            return less(b, a);
        });
    } else {
        std::stable_sort(keyed.begin(), keyed.end(), less);
    }
    std::vector<minipy::value> result;
    result.reserve(items.size());
    for (auto& kv : keyed) result.push_back(items[kv.second]);
    return args.mklist(result);
}

// fn Reversed
// fn reversed
// fn Reversed
static minipy::value fnReversed(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() > 0) vm.materializeIterable(args.arg(0), items);
    std::reverse(items.begin(), items.end());
    return args.mklist(items);
}

// fn Enumerate
// fn enumerate
// fn Enumerate
static minipy::value fnEnumerate(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() > 0) vm.materializeIterable(args.arg(0), items);
    long long start = (args.argc() > 1) ? args.arg_int(1) :
                      (args.has_kwarg("start") ? args.kwarg_int("start", 0) : 0);
    std::vector<minipy::value> result;
    for (size_t i = 0; i < items.size(); ++i){
        result.push_back(vm.makeTuple({args.mkint(start + static_cast<long long>(i)), items[i]}));
    }
    return args.mklist(result);
}

// fn Ord
// fn ord
// fn Ord
static minipy::value fnOrd(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0 || args.arg(0).type != "str") return args.none();
    std::string text = vm.toString(args.arg(0));
    if (text.empty()) return args.none();
    return args.mkint(static_cast<unsigned char>(text[0]));
}

// fn Chr
// fn chr
// fn Chr
static minipy::value fnChr(minipy::cffi& args){
    if (args.argc() == 0) return args.none();
    return args.mkstr(std::string(1, static_cast<char>(args.arg_int(0))));
}


// fn Next
// fn next
// fn Next
static minipy::value fnNext(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() != 1 && args.argc() != 2){
        vm.raiseError("TypeError", "next expected 1 argument (" +
                                   std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    if (obj.type == "native_object"){
        auto* inst = static_cast<minipy::ffi_instance*>(obj.ptr);
        if (inst){

            if (inst->typeName == "coroutine"){
                vm.raiseError("TypeError", "'coroutine' object is not an iterator");
                return args.none();
            }
            if (inst->typeName == "async_generator"){
                vm.raiseError("TypeError", "'async_generator' object is not an iterator");
                return args.none();
            }
            if (inst->typeName == "generator" || inst->typeName == "generator_method"){

                inst->attributes["method_name"] = args.mkstr("__next__");
                minipy::value result = args.none();
                std::map<std::string, minipy::value> kwargs;
                inst->dynamicCall(vm, *inst, {}, kwargs, result);
                if (vm.hasError() && vm.exceptionType() == "StopIteration" &&
                    args.argc() == 2){

                    vm.clearErr();
                    return args.arg(1);
                }
                return result;
            }
        }
    }
    vm.raiseError("TypeError", "'" + vm.pythonTypeName(obj) + "' object is not an iterator");
    return args.none();
}







// fn Property Constructor
// fn property constructor
// fn Property Constructor
static minipy::value fnPropertyConstructor(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.mkobject(nullptr);
    if (self.type != "native_object" || self.ptr == nullptr) return self;
    auto* inst = static_cast<minipy::ffi_instance*>(self.ptr);

    if (args.argc() >= 1){
        inst->attributes["fget"] = args.arg(0);
    } else {
        inst->attributes["fget"] = args.none();
    }

    if (args.argc() >= 2){
        inst->attributes["fset"] = args.arg(1);
    } else {
        inst->attributes["fset"] = args.none();
    }

    if (args.argc() >= 3){
        inst->attributes["fdel"] = args.arg(2);
    } else {
        inst->attributes["fdel"] = args.none();
    }

    if (args.argc() >= 4){
        inst->attributes["__doc__"] = args.arg(3);
    } else {
        inst->attributes["__doc__"] = args.none();
    }


    return self;
}


static minipy::value callCallable(minipy::vm& vm, const minipy::value& callable,
                            const std::vector<minipy::value>& callArgs){
    if (callable.type == "function" && callable.ptr){
        minipy::function_t* fn = static_cast<minipy::function_t*>(callable.ptr);
        return vm.invoke(fn, callArgs, nullptr);
    }
    if (callable.type == "native_object" && callable.ptr){
        auto* inst = static_cast<minipy::ffi_instance*>(callable.ptr);
        if (inst->dynamicCall){
            minipy::value result = vm.makeNone();
            std::map<std::string, minipy::value> kwargs;
            inst->dynamicCall(vm, *inst, callArgs, kwargs, result);
            return result;
        }

        minipy::value result = vm.makeNone();
        if (cffiCallMethod(vm, callable, "__call__", callArgs, result)) return result;
    }
    return vm.makeNone();
}


// fn Property 获取
// fn property get
// fn Property ᠬᠠᠯᠠᠬᠤ
static minipy::value fnPropertyGet(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    minipy::value instance = args.arg(0);

    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);

    if (instance.type == "none"){
        return self;
    }
    minipy::value fget = selfInst->attributes.count("fget") ? selfInst->attributes["fget"] : args.none();
    if (fget.type == "none"){
        vm.raiseError("AttributeError", "can't set attribute");
        return args.none();
    }

// 调用 Callable
// call callable
// ᠢᠳᠡᠬᠡᠯᠳᠡ Callable
    return callCallable(vm, fget, {instance});
}


// fn Property 集合
// fn property set
// fn Property ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ
static minipy::value fnPropertySet(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    minipy::value instance = args.arg(0);
    minipy::value value = args.arg(1);
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    minipy::value fset = selfInst->attributes.count("fset") ? selfInst->attributes["fset"] : args.none();
    if (fset.type == "none"){
        vm.raiseError("AttributeError", "can't set attribute");
        return args.none();
    }

// 调用 Callable
// call callable
// ᠢᠳᠡᠬᠡᠯᠳᠡ Callable
    return callCallable(vm, fset, {instance, value});
}


// fn Property 删除
// fn property delete
// fn Property ᠦᠬᠡᠬᠡᠵᠡ
static minipy::value fnPropertyDelete(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    minipy::value instance = args.arg(0);
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    minipy::value fdel = selfInst->attributes.count("fdel") ? selfInst->attributes["fdel"] : args.none();
    if (fdel.type == "none"){
        vm.raiseError("AttributeError", "can't delete attribute");
        return args.none();
    }

// 调用 Callable
// call callable
// ᠢᠳᠡᠬᠡᠯᠳᠡ Callable
    return callCallable(vm, fdel, {instance});
}



// fn Property Getter
// fn property getter
// fn Property Getter
static minipy::value fnPropertyGetter(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    if (args.argc() >= 1){
        selfInst->attributes["fget"] = args.arg(0);
    }
    return self;
}

// fn Property Setter
// fn property setter
// fn Property Setter
static minipy::value fnPropertySetter(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    if (args.argc() >= 1){
        selfInst->attributes["fset"] = args.arg(0);
    }
    return self;
}

// fn Property Deleter
// fn property deleter
// fn Property Deleter
static minipy::value fnPropertyDeleter(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    if (args.argc() >= 1){
        selfInst->attributes["fdel"] = args.arg(0);
    }
    return self;
}








static bool classmethodBoundCall(minipy::vm& vm, minipy::ffi_instance& self,
                                 const std::vector<minipy::value>& args,
                                 const std::map<std::string, minipy::value>& kwargs,
                                 minipy::value& result){
    auto itFunc = self.attributes.find("__func__");
    auto itOwner = self.attributes.find("__owner__");
    if (itFunc == self.attributes.end() || itOwner == self.attributes.end()){
        vm.raiseError("RuntimeError", "classmethod_bound missing __func__ or __owner__");
        return false;
    }
    const minipy::value& func = itFunc->second;
    minipy::value cls = itOwner->second;

    std::vector<minipy::value> fullArgs;
    fullArgs.push_back(cls);
    for (auto& a : args) fullArgs.push_back(a);
    if (func.type == "function" && func.ptr){
        minipy::function_t* fn = static_cast<minipy::function_t*>(func.ptr);
        result = vm.invoke(fn, fullArgs, nullptr);
        return true;
    }
    if (func.type == "native_object" && func.ptr){
        auto* fInst = static_cast<minipy::ffi_instance*>(func.ptr);
        if (fInst->dynamicCall){
            fInst->dynamicCall(vm, *fInst, fullArgs, kwargs, result);
            return true;
        }

        if (cffiCallMethod(vm, func, "__call__", fullArgs, result)) return true;
    }
    vm.raiseError("TypeError", "'" + vm.pythonTypeName(func) + "' object is not callable");
    return false;
}


// 构造 Classmethod Bound
// make classmethod bound
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Classmethod Bound
static minipy::value makeClassmethodBound(minipy::vm& vm, const minipy::value& func, const minipy::value& owner){
    minipy::value result = vm.makeDynamicObject("classmethod_bound", nullptr, nullptr, classmethodBoundCall);
    auto* inst = static_cast<minipy::ffi_instance*>(result.ptr);
    inst->attributes["__func__"] = func;
    inst->attributes["__owner__"] = owner;
    return result;
}


// fn Classmethod Constructor
// fn classmethod constructor
// fn Classmethod Constructor
static minipy::value fnClassmethodConstructor(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.mkobject(nullptr);
    if (self.type != "native_object" || self.ptr == nullptr) return self;
    auto* inst = static_cast<minipy::ffi_instance*>(self.ptr);
    if (args.argc() < 1){
        vm.raiseError("TypeError", "classmethod() takes at least 1 argument (0 given)");
        return self;
    }
    inst->attributes["__func__"] = args.arg(0);
    return self;
}


// fn Classmethod 获取
// fn classmethod get
// fn Classmethod ᠬᠠᠯᠠᠬᠤ
static minipy::value fnClassmethodGet(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    minipy::value instance = args.arg(0);
    minipy::value owner = args.arg(1);
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    minipy::value func = selfInst->attributes.count("__func__") ?
                   selfInst->attributes["__func__"] : args.none();
    if (func.type == "none"){
        vm.raiseError("TypeError", "classmethod has no __func__");
        return args.none();
    }

    minipy::value cls;
    if (instance.type != "none"){

        if (instance.type == "object" && instance.ptr){
            auto* inst = static_cast<void*>(instance.ptr);





            cls = owner;
        } else {
            cls = owner;
        }
    } else {
        cls = owner;
    }
    return makeClassmethodBound(vm, func, cls);
}








// fn Staticmethod Constructor
// fn staticmethod constructor
// fn Staticmethod Constructor
static minipy::value fnStaticmethodConstructor(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.mkobject(nullptr);
    if (self.type != "native_object" || self.ptr == nullptr) return self;
    auto* inst = static_cast<minipy::ffi_instance*>(self.ptr);
    if (args.argc() < 1){
        vm.raiseError("TypeError", "staticmethod() takes at least 1 argument (0 given)");
        return self;
    }
    inst->attributes["__func__"] = args.arg(0);
    return self;
}


// fn Staticmethod 获取
// fn staticmethod get
// fn Staticmethod ᠬᠠᠯᠠᠬᠤ
static minipy::value fnStaticmethodGet(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value self = args.self();
    auto* selfInst = static_cast<minipy::ffi_instance*>(self.ptr);
    auto it = selfInst->attributes.find("__func__");
    if (it == selfInst->attributes.end()){
        vm.raiseError("TypeError", "staticmethod has no __func__");
        return args.none();
    }
    return it->second;
}


// fn Hash
// fn hash
// fn Hash
static minipy::value fnHash(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() != 1){
        vm.raiseError("TypeError", "hash() takes exactly one argument (" +
                                       std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value value = args.arg(0);
    if (!vm.hashable(value)){
        vm.raiseError("TypeError", "unhashable type: '" + vm.pythonTypeName(value) + "'");
        return args.none();
    }
    long long h = vm.hashKey(value);
    return args.mkint(h);
}


// fn Format
// fn format
// fn Format
static minipy::value fnFormat(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1 || args.argc() > 2){
        vm.raiseError("TypeError", "format() takes at least 1 argument (" +
                                       std::to_string(args.argc()) + " given)");
        return args.none();
    }
    minipy::value value = args.arg(0);
    std::string spec = args.argc() >= 2 ? args.arg_str(1) : "";
    return vm.applyFormat(value, spec);
}


// fn Dir
// fn dir
// fn Dir
static minipy::value fnDir(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> listItems;
    if (args.argc() == 0){

        return args.mklist(listItems);
    }
    minipy::value value = args.arg(0);

    minipy::function_t* dirMethod = vm.findObjectMethod(value, "__dir__");
    if (dirMethod){
        minipy::value selfCopy = value;
        minipy::value result = vm.invoke(dirMethod, {}, &selfCopy);
        if (vm.hasError()) return args.none();

        std::vector<minipy::value> rawItems;
        vm.materializeIterable(result, rawItems);
        std::vector<std::string> tmp;
        for (auto& item : rawItems){
            tmp.push_back(vm.toString(item));
        }
        std::sort(tmp.begin(), tmp.end());
        for (auto& n : tmp) listItems.push_back(args.mkstr(n));
        return args.mklist(listItems);
    }

    std::vector<std::string> names = vm.collectDirNames(value);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    for (auto& n : names) listItems.push_back(args.mkstr(n));
    return args.mklist(listItems);
}


// data 空值
// data none
// data ᠭᠣᠷᠣᠰ
static minipy::value dataNone(minipy::cffi& args){
    return args.none();
}

// data 真
// data true
// data ᠨᠲᠠ
static minipy::value dataTrue(minipy::cffi& args){
    return args.mkbool(true);
}

// data 假
// data false
// data ᠦᠭᠡᠢ
static minipy::value dataFalse(minipy::cffi& args){
    return args.mkbool(false);
}





// fn 是 实例
// fn is instance
// fn ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠶᠢᠨᠢᠰ
static minipy::value fnIsInstance(minipy::cffi& args){
    if (args.argc() < 2){
        args.vm->raiseError("TypeError", "isinstance() takes exactly 2 arguments");
        return args.none();
    }
    return args.mkbool(args.vm->isInstance(args.arg(0), args.arg(1)));
}




// fn 超类
// fn super
// fn ᠪᠡᠶᠡᠨ
static minipy::value fnSuper(minipy::cffi& args){
    if (args.argc() == 0){
        return args.vm->makeSuper();
    }
    if (args.argc() != 2){
        args.vm->raiseError("TypeError", "super() takes at most 2 arguments");
        return args.none();
    }
    minipy::value cls = args.arg(0);
    minipy::value obj = args.arg(1);
    return args.vm->makeSuper(&cls, &obj);
}

// fn 是 Subclass
// fn is subclass
// fn ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Subclass
static minipy::value fnIsSubclass(minipy::cffi& args){
    if (args.argc() < 2){
        args.vm->raiseError("TypeError", "issubclass() takes exactly 2 arguments");
        return args.none();
    }
    minipy::value sub = args.arg(0);
    minipy::value cls = args.arg(1);

    auto isBuiltinTypeName = [](const minipy::value& value, const std::string& name){
        if (value.type == "class" && value.ptr)
            return static_cast<minipy::class_t*>(value.ptr)->name == name;
        if (value.type == "native_object" && value.ptr){
            auto* inst = static_cast<minipy::ffi_instance*>(value.ptr);
            return inst->typeName == name;
        }
        return false;
    };
    bool subIsBool = isBuiltinTypeName(sub, "bool");
    if (sub.type != "class" && !subIsBool){
        args.vm->raiseError("TypeError", "issubclass() arg 1 must be a class");
        return args.none();
    }

    if (subIsBool){
        if (isBuiltinTypeName(cls, "bool") || isBuiltinTypeName(cls, "int") ||
            isBuiltinTypeName(cls, "object"))
            return args.mkbool(true);
        if (cls.type == "class" && cls.ptr){
            auto* tk = static_cast<minipy::class_t*>(cls.ptr);
            if (tk->name == "int" || tk->name == "object" || tk->name == "bool")
                return args.mkbool(true);
        }
        return args.mkbool(false);
    }
    auto* subKlass = static_cast<minipy::class_t*>(sub.ptr);
    std::vector<minipy::value> clsList;
    if (cls.type == "tuple" && cls.ptr){
        auto* tp = static_cast<std::vector<minipy::value>*>(cls.ptr);
        for (auto& item : *tp) clsList.push_back(item);
    } else {
        clsList.push_back(cls);
    }
    for (auto& target : clsList){
        if (target.type == "class" && target.ptr){
            auto* tk = static_cast<minipy::class_t*>(target.ptr);
            if (tk->name == "object") return args.mkbool(true);
            int guard = 0;
            for (minipy::class_t* current = subKlass; current != nullptr && guard < 1000;
                 current = current->baseClass, ++guard){
                if (current == tk) return args.mkbool(true);
            }
        } else if (target.type == "native_object" && target.ptr){
            auto* tk = static_cast<minipy::ffi_instance*>(target.ptr);
            if (tk->klass && tk->klass->name == "object") return args.mkbool(true);
            if (!tk->klass && (tk->typeName == "dict" || tk->typeName == "list")){
                if (args.vm->isSubclassOfBuiltin(sub, tk->typeName))
                    return args.mkbool(true);
            }
        }
    }
    return args.mkbool(false);
}




// fn Callable
// fn callable
// fn Callable
static minipy::value fnCallable(minipy::cffi& args){
    if (args.argc() < 1) return args.mkbool(false);
    minipy::value obj = args.arg(0);
    if (obj.type == "function" || obj.type == "class") return args.mkbool(true);
    if (obj.type == "native_object" && obj.ptr){
        auto* ni = static_cast<minipy::ffi_instance*>(obj.ptr);

        if (ni->klass || ni->dynamicCall || ni->handle) return args.mkbool(true);
    }
    return args.mkbool(false);
}




// fn Id
// fn id
// fn Id
static minipy::value fnId(minipy::cffi& args){
    if (args.argc() < 1) return args.mkint(0);
    minipy::value obj = args.arg(0);
    if (obj.ptr) return args.mkint(reinterpret_cast<intptr_t>(obj.ptr));
    return args.mkint(0);
}





// fn 拥有 属性
// fn has attr
// fn ᠲᠡᠢ ᠰᠢᠨᠡ
static minipy::value fnHasAttr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2){
        vm.raiseError("TypeError", "hasattr expected 2 arguments");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    std::string name = args.arg_str(1);
    return args.mkbool(vm.hasAttribute(obj, name));
}

// fn 获取 属性
// fn get attr
// fn ᠬᠠᠯᠠᠬᠤ ᠰᠢᠨᠡ
static minipy::value fnGetAttr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2){
        vm.raiseError("TypeError", "getattr expected 2 arguments");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    std::string name = args.arg_str(1);
    minipy::value result = vm.getAttribute(obj, name);
    if (vm.hasError()){

        if (args.argc() >= 3){
            vm.clearErr();
            return args.arg(2);
        }
        return args.none();
    }
    return result;
}

// fn 集合 属性
// fn set attr
// fn ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ ᠰᠢᠨᠡ
static minipy::value fnSetAttr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 3){
        vm.raiseError("TypeError", "setattr expected 3 arguments");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    std::string name = args.arg_str(1);
    minipy::value value = args.arg(2);
    vm.setAttribute(obj, name, value);
    return args.none();
}


// fn All
// fn all
// fn All
static minipy::value fnAll(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1) return args.mkbool(true);
    std::vector<minipy::value> items;
    vm.materializeIterable(args.arg(0), items);
    if (vm.hasError()) return args.none();
    for (auto& item : items){
        if (!vm.truthy(item)) return args.mkbool(false);
    }
    return args.mkbool(true);
}

// fn Any
// fn any
// fn Any
static minipy::value fnAny(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1) return args.mkbool(false);
    std::vector<minipy::value> items;
    vm.materializeIterable(args.arg(0), items);
    if (vm.hasError()) return args.none();
    for (auto& item : items){
        if (vm.truthy(item)) return args.mkbool(true);
    }
    return args.mkbool(false);
}


// fn Zip
// fn zip
// fn Zip
static minipy::value fnZip(minipy::cffi& args){
    minipy::vm& vm = *args.vm;

    if (args.argc() == 0) return args.mklist({});

    std::vector<std::vector<minipy::value>> allItems;
    size_t minLen = (size_t)-1;
    for (size_t i = 0; i < args.argc(); ++i){
        std::vector<minipy::value> items;
        vm.materializeIterable(args.arg(i), items);
        if (vm.hasError()) return args.none();
        if (items.size() < minLen) minLen = items.size();
        allItems.push_back(std::move(items));
    }
    std::vector<minipy::value> result;
    for (size_t i = 0; i < minLen; ++i){
        std::vector<minipy::value> tupleItems;
        for (auto& col : allItems){
            tupleItems.push_back(col[i]);
        }
        result.push_back(vm.makeTuple(tupleItems));
    }
    return args.mklist(result);
}




// fn Del 属性
// fn del attr
// fn Del ᠰᠢᠨᠡ
static minipy::value fnDelAttr(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2){
        vm.raiseError("TypeError", "delattr expected 2 arguments");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    std::string name = args.arg_str(1);
    vm.deleteAttribute(obj, name);
    return args.none();
}





// fn Vars
// fn vars
// fn Vars
static minipy::value fnVars(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0){

        return args.mkdict({});
    }
    minipy::value obj = args.arg(0);

    minipy::value dictVal = vm.getAttribute(obj, "__dict__");
    if (vm.hasError()){

        vm.clearErr();
        vm.raiseError("TypeError", "'" + vm.pythonTypeName(obj) + "' object has no __dict__ attribute");
        return args.none();
    }
    return dictVal;
}






// fn Iter
// fn iter
// fn Iter
static minipy::value fnIter(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1){
        vm.raiseError("TypeError", "iter expected at least 1 argument");
        return args.none();
    }
    minipy::value obj = args.arg(0);
    if (args.argc() == 1){

        if (obj.type == "list" || obj.type == "tuple" || obj.type == "range" ||
            obj.type == "str" || obj.type == "dict" || obj.type == "object"){
            return obj;
        }

        minipy::function_t* iterMethod = vm.findObjectMethod(obj, "__iter__");
        if (iterMethod){
            minipy::value selfCopy = obj;
            minipy::value result = vm.invoke(iterMethod, {}, &selfCopy);
            if (!vm.hasError()) return result;
            vm.clearErr();
        }

        vm.raiseError("TypeError", "'" + vm.pythonTypeName(obj) + "' object is not iterable");
        return args.none();
    }

    minipy::value sentinel = args.arg(1);
    std::vector<minipy::value> result;
    size_t safety = 1000000;

    if (obj.type == "function" && obj.ptr){
        minipy::function_t* fn = static_cast<minipy::function_t*>(obj.ptr);
        while (safety-- > 0){
            minipy::value r = vm.invoke(fn, {}, nullptr);
            if (vm.hasError()) return args.none();
            if (vm.valuesEqual(r, sentinel)) break;
            result.push_back(r);
        }
    } else if (obj.type == "native_object" && obj.ptr){

        while (safety-- > 0){
            minipy::value r = vm.makeNone();
            std::vector<minipy::value> callArgs;
            if (minipy::cffiCallMethod(vm, obj, "__call__", callArgs, r)){
                if (vm.valuesEqual(r, sentinel)) break;
                result.push_back(r);
            } else {
                vm.raiseError("TypeError", "'" + vm.pythonTypeName(obj) + "' object is not callable");
                return args.none();
            }
        }
    } else {
        vm.raiseError("TypeError", "'" + vm.pythonTypeName(obj) + "' object is not callable");
        return args.none();
    }
    return args.mklist(result);
}






// fn 对象 新
// fn object new
// fn ᠣᠪᠶᠡᠭᠲ ᠰᠢᠨ᠎ᠡ
static minipy::value fnObjectNew(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1){
        vm.raiseError("TypeError", "object.__new__() takes exactly one argument (the class to instantiate)");
        return args.none();
    }
    minipy::value cls = args.arg(0);
    if (cls.type == "class" && cls.ptr){
        auto* klass = static_cast<minipy::class_t*>(cls.ptr);
        minipy::value instance = vm.makeObject(klass);
        return instance;
    }
    if (cls.type == "native_object" && cls.ptr){
        auto* cobj = static_cast<minipy::ffi_instance*>(cls.ptr);
        if (cobj->klass){
            return vm.makeNativeObject(cobj->library, cobj->klass, nullptr);
        }
    }
    vm.raiseError("TypeError", "object.__new__() arg must be class");
    return args.none();
}

// fn 对象 Init
// fn object init
// fn ᠣᠪᠶᠡᠭᠲ Init
static minipy::value fnObjectInit(minipy::cffi& args){
    return args.none();
}

// 整数 转 Base
// int to base
// ᠳ᠋ᠠᠩ ᠲᠣ᠎ ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Base
static std::string intToBase(long long value, int base, const std::string& prefix){
    bool neg = value < 0;
    unsigned long long v = neg ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                               : static_cast<unsigned long long>(value);
    std::string digits;
    if (v == 0) digits = "0";
    while (v){
        unsigned long long d = v % static_cast<unsigned long long>(base);
        digits.push_back(static_cast<char>(d < 10 ? '0' + static_cast<int>(d)
                                                  : 'a' + static_cast<int>(d) - 10));
        v /= static_cast<unsigned long long>(base);
    }
    std::reverse(digits.begin(), digits.end());
    return (neg ? "-" : "") + prefix + digits;
}

// fn Divmod
// fn divmod
// fn Divmod
static minipy::value fnDivmod(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2) return args.none();
    minipy::value a = args.arg(0), b = args.arg(1);
    if (a.type == "float" || b.type == "float"){
        double x = vm.toFloat(a), y = vm.toFloat(b);
        if (y == 0.0){ vm.raiseError("ZeroDivisionError", "float division by zero"); return args.none(); }
        double q = std::floor(x / y);
        return vm.makeTuple({args.mkfloat(q), args.mkfloat(x - q * y)});
    }
    long long x = args.arg_int(0), y = args.arg_int(1);
    if (y == 0){ vm.raiseError("ZeroDivisionError", "integer division or modulo by zero"); return args.none(); }
    long long q = x / y;
    if ((x % y) != 0 && ((x < 0) != (y < 0))){
        q -= 1;
    }
    return vm.makeTuple({args.mkint(q), args.mkint(x - q * y)});
}

// fn Bin
// fn bin
// fn Bin
static minipy::value fnBin(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    return args.mkstr(intToBase(args.arg_int(0), 2, "0b"));
}

// fn Oct
// fn oct
// fn Oct
static minipy::value fnOct(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    return args.mkstr(intToBase(args.arg_int(0), 8, "0o"));
}

// fn Hex
// fn hex
// fn Hex
static minipy::value fnHex(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    return args.mkstr(intToBase(args.arg_int(0), 16, "0x"));
}

// fn Map
// fn map
// fn Map
static minipy::value fnMap(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2) return args.mklist({});
    minipy::value fn = args.arg(0);
    std::vector<std::vector<minipy::value>> seqs;
    size_t minLen = static_cast<size_t>(-1);
    for (size_t i = 1; i < args.argc(); ++i){
        std::vector<minipy::value> items;
        vm.materializeIterable(args.arg(i), items);
        minLen = std::min(minLen, items.size());
        seqs.push_back(std::move(items));
    }
    std::vector<minipy::value> result;
    for (size_t k = 0; k < minLen; ++k){
        std::vector<minipy::value> callArgs;
        for (auto& s : seqs) callArgs.push_back(s[k]);
        result.push_back(vm.callValue(fn, callArgs));
    }
    return args.mklist(result);
}

// fn Filter
// fn filter
// fn Filter
static minipy::value fnFilter(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2) return args.mklist({});
    minipy::value fn = args.arg(0);
    std::vector<minipy::value> items;
    vm.materializeIterable(args.arg(1), items);
    std::vector<minipy::value> result;
    for (auto& item : items){
        bool keep = (fn.type == "none") ? vm.truthy(item) : vm.truthy(vm.callValue(fn, {item}));
        if (keep) result.push_back(item);
    }
    return args.mklist(result);
}

// fn Exec
// fn exec
// fn Exec
static minipy::value fnExec(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    vm.execSource(args.arg_str(0));
    return args.none();
}

// fn Ascii
// fn ascii
// fn Ascii
static minipy::value fnAscii(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return args.none();
    return args.mkstr(vm.toString(args.arg(0)));
}

// ============================================================
// open() / 文件对象 (纯 std::fstream, 无平台相关头)
// ============================================================

struct FileState{
    std::fstream stream;
    std::string path;
    std::string mode;
    bool binary = false;
    bool isopen = true;
};

static FileState* fileStateOf(minipy::cffi& args){
    minipy::value self = args.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<minipy::ffi_instance*>(self.ptr);
    return static_cast<FileState*>(inst->handle);
}

static void requireOpenFile(minipy::cffi& args, FileState* st){
    if (!st){
        args.vm->raiseError("ValueError", "I/O operation on uninitialized file");
        return;
    }
    if (!st->isopen){
        args.vm->raiseError("ValueError", "I/O operation on closed file");
    }
}

// fn 文件 __enter__
static minipy::value fnFileEnter(minipy::cffi& args){
    return args.self();
}

// fn 文件 __exit__ (返回 False, 让异常继续传播)
static minipy::value fnFileExit(minipy::cffi& args){
    (void)args;
    FileState* st = fileStateOf(args);
    if (st && st->isopen) st->stream.close();
    if (st) st->isopen = false;
    args.setattr("closed", args.mkbool(true));
    return args.mkbool(false);
}

// fn 文件 close
static minipy::value fnFileClose(minipy::cffi& args){
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (args.vm->hasError()) return args.none();
    st->stream.close();
    st->isopen = false;
    args.setattr("closed", args.mkbool(true));
    return args.none();
}

// fn 文件 flush
static minipy::value fnFileFlush(minipy::cffi& args){
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (args.vm->hasError()) return args.none();
    st->stream.flush();
    return args.none();
}

// fn 文件 read(size=-1)
static minipy::value fnFileRead(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (vm.hasError()) return args.none();
    long long n = -1;
    if (args.argc() >= 1 && args.arg(0).type != "none"){
        n = args.arg_int(0);
        if (n < -1){
            vm.raiseError("ValueError", "read() length must be non-negative");
            return args.none();
        }
    }
    std::string out;
    if (n < 0){
        std::ostringstream ss;
        ss << st->stream.rdbuf();
        out = ss.str();
    } else {
        out.resize(static_cast<size_t>(n));
        st->stream.read(&out[0], static_cast<std::streamsize>(n));
        out.resize(static_cast<size_t>(st->stream.gcount() > 0 ? st->stream.gcount() : 0));
    }
    return args.mkstr(out);
}

// fn 文件 readline()
static minipy::value fnFileReadline(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (vm.hasError()) return args.none();
    std::string line;
    if (!std::getline(st->stream, line)) return args.mkstr("");
    if (!st->stream.eof()) line += '\n';
    return args.mkstr(line);
}

// fn 文件 readlines()
static minipy::value fnFileReadlines(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (vm.hasError()) return args.none();
    std::vector<minipy::value> lines;
    std::string line;
    while (std::getline(st->stream, line)){
        if (!st->stream.eof()) line += '\n';
        lines.push_back(args.mkstr(line));
        line.clear();
    }
    return args.mklist(lines);
}

// fn 文件 write(s)
static minipy::value fnFileWrite(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (vm.hasError()) return args.none();
    std::string text = args.arg_str(0);
    st->stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!st->stream.good()){
        st->stream.close();
        st->isopen = false;
        vm.raiseError("OSError", "[Errno 28] No space left on device: '" + st->path + "'");
        return args.none();
    }
    return args.mkint(static_cast<long long>(text.size()));
}

// fn 文件 writelines(list)
static minipy::value fnFileWritelines(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (vm.hasError()) return args.none();
    std::vector<minipy::value> items;
    if (args.argc() >= 1) vm.materializeIterable(args.arg(0), items);
    long long total = 0;
    for (const auto& item : items){
        std::string text = vm.toString(item);
        st->stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        total += static_cast<long long>(text.size());
    }
    return args.mkint(total);
}

// fn 文件 tell()
static minipy::value fnFileTell(minipy::cffi& args){
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (args.vm->hasError()) return args.none();
    return args.mkint(static_cast<long long>(st->stream.tellg()));
}

// fn 文件 seek(offset, whence=0)
static minipy::value fnFileSeek(minipy::cffi& args){
    FileState* st = fileStateOf(args);
    requireOpenFile(args, st);
    if (args.vm->hasError()) return args.none();
    long long offset = args.arg_int(0);
    long long whence = args.argc() >= 2 ? args.arg_int(1) : 0;
    std::ios::seekdir dir = std::ios::beg;
    if (whence == 1) dir = std::ios::cur;
    else if (whence == 2) dir = std::ios::end;
    else if (whence != 0){
        args.vm->raiseError("ValueError", "invalid whence (" + std::to_string(whence) +
                                         ", should be 0, 1 or 2)");
        return args.none();
    }
    st->stream.clear();
    st->stream.seekg(offset, dir);
    return args.mkint(static_cast<long long>(st->stream.tellg()));
}

// fn open(file, mode='r', encoding=None, ...)
static minipy::value fnOpen(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1){
        vm.raiseError("TypeError", "open() missing required argument 'file' (pos 1)");
        return args.none();
    }
    std::string path = args.arg_str(0);
    std::string mode = "r";
    if (args.has_kwarg("mode") && args.kwarg("mode").type != "none") mode = args.kwarg_str("mode", "r");
    else if (args.argc() >= 2) mode = args.arg_str(1);
    if (args.has_kwarg("encoding") && args.kwarg("encoding").type != "none"){
        std::string enc = args.kwarg_str("encoding", "utf-8");
        std::string lower = enc;
        for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower != "utf-8" && lower != "utf8" && lower != "utf_8" && lower != "ascii"){
            vm.raiseError("ValueError", "unknown encoding: " + enc + " (only utf-8 is supported)");
            return args.none();
        }
    }

    bool readFlag = false, writeFlag = false, appendFlag = false, plusFlag = false;
    bool truncFlag = false;
    for (char ch : mode){
        switch (ch){
            case 'r': readFlag = true; break;
            case 'w': writeFlag = true; truncFlag = true; break;
            case 'a': appendFlag = true; writeFlag = true; break;
            case 'x': writeFlag = true; truncFlag = true; break;
            case '+': plusFlag = true; break;
            case 'b': case 't': break;
            default:
                vm.raiseError("ValueError", "invalid mode: '" + mode + "'");
                return args.none();
        }
    }
    if (!readFlag && !writeFlag && !plusFlag){ readFlag = true; }

    bool exclusive = mode.find('x') != std::string::npos;
    if (exclusive){
        std::ifstream probe(path, std::ios::binary);
        if (probe.good()){
            probe.close();
            vm.raiseError("FileExistsError", "[Errno 17] File exists: '" + path + "'");
            return args.none();
        }
    }

    std::ios::openmode flags = std::ios::binary;
    if (readFlag || plusFlag) flags |= std::ios::in;
    if (writeFlag) flags |= std::ios::out;
    if (truncFlag) flags |= std::ios::trunc;
    if (appendFlag) flags |= std::ios::app;

    auto* st = new FileState();
    st->path = path;
    st->mode = mode;
    st->binary = mode.find('b') != std::string::npos;
    st->stream.open(path, flags);
    if (!st->stream.is_open()){
        delete st;
        vm.raiseError("FileNotFoundError", "[Errno 2] No such file or directory: '" + path + "'");
        return args.none();
    }

    auto it = args.library->classes.find("file");
    if (it == args.library->classes.end() || !args.library){
        vm.raiseError("RuntimeError", "internal error: file class not registered");
        return args.none();
    }
    minipy::value obj = vm.makeNativeObject(args.library, &it->second, st);
    args.setattr(obj, "closed", args.mkbool(false));
    args.setattr(obj, "name", args.mkstr(path));
    args.setattr(obj, "mode", args.mkstr(mode));
    return obj;
}

// ============================================================
// bytes / bytearray / memoryview
// ============================================================

// bytes value 底层复用 heap 的 vk_str (std::string) 存放原始字节串, 仅改 type 标记
static minipy::value mkBytesValue(minipy::vm& vm, const std::string& raw){
    minipy::value v = vm.makeString(raw);
    v.type = "bytes";
    return v;
}

static std::string bytesSourceOf(minipy::vm& vm, const minipy::value& v, bool& ok){
    ok = true;
    if (v.type == "bytes") return *static_cast<std::string*>(v.ptr);
    if (v.type == "str") return vm.toString(v);
    if (v.type == "list" || v.type == "tuple"){
        std::string raw;
        auto* items = static_cast<std::vector<minipy::value>*>(v.ptr);
        for (const auto& item : *items){
            long long b = vm.toInt(item) & 0xFF;
            raw.push_back(static_cast<char>(b));
        }
        return raw;
    }
    ok = false;
    return "";
}

static minipy::value fnBytes(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0) return mkBytesValue(vm, "");
    minipy::value v = args.arg(0);
    if (v.type == "int"){
        long long n = args.arg_int(0);
        if (n < 0){
            vm.raiseError("ValueError", "negative count");
            return args.none();
        }
        return mkBytesValue(vm, std::string(static_cast<size_t>(n), '\0'));
    }
    bool ok = false;
    std::string raw = bytesSourceOf(vm, v, ok);
    if (!ok){
        vm.raiseError("TypeError", "cannot convert '" + vm.pythonTypeName(v) + "' object to bytes");
        return args.none();
    }
    return mkBytesValue(vm, raw);
}

static minipy::value fnBytearray(minipy::cffi& args){
    return fnBytes(args);
}

static minipy::value fnMemoryviewConstructor(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1){
        vm.raiseError("TypeError", "memoryview() missing required argument");
        return args.none();
    }
    bool ok = false;
    std::string raw = bytesSourceOf(vm, args.arg(0), ok);
    if (!ok){
        vm.raiseError("TypeError", "memoryview: a bytes-like object is required, not '" +
                                     vm.pythonTypeName(args.arg(0)) + "'");
        return args.none();
    }
    minipy::value obj = args.mkobject(new std::string(raw));
    args.setattr(obj, "obj", args.arg(0));
    return obj;
}

static minipy::value fnMemoryviewTobytes(minipy::cffi& args){
    minipy::value self = args.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<minipy::ffi_instance*>(self.ptr);
        auto* raw = static_cast<std::string*>(inst->handle);
        if (raw) return mkBytesValue(*args.vm, *raw);
    }
    return args.none();
}

// ============================================================
// set / frozenset (以 dict (key -> True) 形式承载, 支持 in/len/迭代)
// ============================================================

static minipy::value setFromIterable(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<minipy::value> items;
    if (args.argc() >= 1 && args.arg(0).type != "none"){
        vm.materializeIterable(args.arg(0), items);
        if (vm.hasError()) return args.none();
    }
    std::vector<std::pair<minipy::value, minipy::value>> kv;
    for (const auto& item : items) kv.push_back({item, args.mkbool(true)});
    return args.mkdict(kv);
}

static minipy::value fnSet(minipy::cffi& args){
    return setFromIterable(args);
}

static minipy::value fnFrozenset(minipy::cffi& args){
    return setFromIterable(args);
}

// ============================================================
// pow / slice / complex / compile / breakpoint / help / globals / locals / __import__
// ============================================================

static minipy::value fnPow(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 2){
        vm.raiseError("TypeError", "pow() missing required argument (expected 2-3, got " +
                                     std::to_string(args.argc()) + ")");
        return args.none();
    }
    minipy::value base = args.arg(0), exp = args.arg(1);
    if (args.argc() >= 3 && args.arg(2).type != "none"){
        long long b = vm.toInt(base), e = vm.toInt(exp), m = vm.toInt(args.arg(2));
        if (m == 0){
            vm.raiseError("ValueError", "pow() 3rd argument cannot be 0");
            return args.none();
        }
        long long result = 1;
        b %= m;
        bool neg = m < 0;
        unsigned long long mm = neg ? static_cast<unsigned long long>(-(m + 1)) + 1ULL
                                    : static_cast<unsigned long long>(m);
        unsigned long long ee = e < 0 ? static_cast<unsigned long long>(-(e + 1)) + 1ULL
                                      : static_cast<unsigned long long>(e);
        while (ee){
            if (ee & 1ULL) result = static_cast<long long>(
                (static_cast<unsigned long long>(result) * static_cast<unsigned long long>(b)) % mm);
            b = static_cast<long long>(
                (static_cast<unsigned long long>(b) * static_cast<unsigned long long>(b)) % mm);
            ee >>= 1;
        }
        return args.mkint(result);
    }
    if (base.type == "float" || exp.type == "float"){
        return args.mkfloat(std::pow(vm.toFloat(base), vm.toFloat(exp)));
    }
    long long b = vm.toInt(base), e = vm.toInt(exp);
    if (e >= 0){
        long long result = 1;
        while (e){
            if (e & 1) result *= b;
            b *= b;
            e >>= 1;
        }
        return args.mkint(result);
    }
    return args.mkfloat(std::pow(static_cast<double>(vm.toInt(base)),
                                 static_cast<double>(vm.toInt(exp))));
}

static minipy::value fnSliceConstructor(minipy::cffi& args){
    minipy::value obj = args.mkobject(nullptr);
    args.setattr(obj, "start", args.argc() >= 1 ? args.arg(0) : args.none());
    args.setattr(obj, "stop", args.argc() >= 2 ? args.arg(1) : args.none());
    args.setattr(obj, "step", args.argc() >= 3 ? args.arg(2) : args.none());
    return obj;
}

static minipy::value fnComplexConstructor(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    minipy::value obj = args.mkobject(nullptr);
    double real = args.argc() >= 1 ? args.arg_float(0) : 0.0;
    double imag = args.argc() >= 2 ? args.arg_float(1) : 0.0;
    args.setattr(obj, "real", args.mkfloat(real));
    args.setattr(obj, "imag", args.mkfloat(imag));
    (void)vm;
    return obj;
}

static minipy::value fnCompile(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() < 1){
        vm.raiseError("TypeError", "compile() missing required argument 'source'");
        return args.none();
    }
    if (args.argc() >= 3){
        std::string mode = args.arg_str(2);
        if (mode != "exec" && mode != "eval" && mode != "single"){
            vm.raiseError("ValueError", "compile() mode must be 'exec', 'eval' or 'single'");
            return args.none();
        }
    }
    return args.mkstr(args.arg_str(0));
}

static minipy::value fnBreakpoint(minipy::cffi& args){
    (void)args;
    return args.none();
}

static minipy::value fnHelp(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() == 0){
        std::cout << "Welcome to MiniPython! This is the interactive help utility.\n"
                     "help(obj) prints information about obj.\n";
    } else {
        std::cout << "No documentation available for '" +
                     vm.pythonTypeName(args.arg(0)) + "' objects.\n";
    }
    return args.none();
}

static minipy::value scopeSnapshot(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    std::vector<std::string> names = vm.collectScopeNames();
    std::vector<std::pair<minipy::value, minipy::value>> kv;
    for (const auto& name : names){
        minipy::value v = vm.findVariable(name);
        kv.push_back({args.mkstr(name), v});
    }
    return args.mkdict(kv);
}

static minipy::value fnGlobals(minipy::cffi& args){
    return scopeSnapshot(args);
}

static minipy::value fnLocals(minipy::cffi& args){
    return scopeSnapshot(args);
}

static minipy::value fnImport(minipy::cffi& args){
    minipy::vm& vm = *args.vm;
    if (args.argc() >= 1){
        vm.raiseError("ImportError", "__import__('" + args.arg_str(0) +
                                       "') is not supported; use the import statement");
    }
    return args.none();
}

// cffi 内置 lib
// cffi builtins lib
// cffi ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ lib
minipy::ffilib cffi_builtins_lib(){
    minipy::ffi_class objectClass;
    objectClass.name = "object";
    objectClass.constructor = (void*)fnObjectNew;
    objectClass.methods = {
        {"__new__", (void*)fnObjectNew},
        {"__init__", (void*)fnObjectInit},
    };
    minipy::ffi_class propertyClass;
    propertyClass.name = "property";
    propertyClass.constructor = (void*)fnPropertyConstructor;
    propertyClass.methods = {
        {"__get__", (void*)fnPropertyGet},
        {"__set__", (void*)fnPropertySet},
        {"__delete__", (void*)fnPropertyDelete},
        {"getter", (void*)fnPropertyGetter},
        {"setter", (void*)fnPropertySetter},
        {"deleter", (void*)fnPropertyDeleter},
    };
    minipy::ffi_class classmethodClass;
    classmethodClass.name = "classmethod";
    classmethodClass.constructor = (void*)fnClassmethodConstructor;
    classmethodClass.methods = {
        {"__get__", (void*)fnClassmethodGet},
    };
    minipy::ffi_class staticmethodClass;
    staticmethodClass.name = "staticmethod";
    staticmethodClass.constructor = (void*)fnStaticmethodConstructor;
    staticmethodClass.methods = {
        {"__get__", (void*)fnStaticmethodGet},
    };
    minipy::ffi_class fileClass;
    fileClass.name = "file";
    fileClass.methods = {
        {"__enter__", (void*)fnFileEnter},
        {"__exit__", (void*)fnFileExit},
        {"read", (void*)fnFileRead},
        {"readline", (void*)fnFileReadline},
        {"readlines", (void*)fnFileReadlines},
        {"write", (void*)fnFileWrite},
        {"writelines", (void*)fnFileWritelines},
        {"close", (void*)fnFileClose},
        {"flush", (void*)fnFileFlush},
        {"tell", (void*)fnFileTell},
        {"seek", (void*)fnFileSeek},
    };
    minipy::ffi_class memoryviewClass;
    memoryviewClass.name = "memoryview";
    memoryviewClass.constructor = (void*)fnMemoryviewConstructor;
    memoryviewClass.methods = {
        {"tobytes", (void*)fnMemoryviewTobytes},
    };
    minipy::ffi_class sliceClass;
    sliceClass.name = "slice";
    sliceClass.constructor = (void*)fnSliceConstructor;
    minipy::ffi_class complexClass;
    complexClass.name = "complex";
    complexClass.constructor = (void*)fnComplexConstructor;
    return minipy::cffi_lib(
        "builtins",
        {
            {"print", (void*)fnPrint},
            {"input", (void*)fnInput},
            {"len", (void*)fnLen},
            {"str", (void*)fnStr},
            {"repr", (void*)fnRepr},
            {"int", (void*)fnInt},
            {"float", (void*)fnFloat},
            {"bool", (void*)fnBool},
            {"abs", (void*)fnAbs},
            {"min", (void*)fnMin},
            {"max", (void*)fnMax},
            {"sum", (void*)fnSum},
            {"range", (void*)fnRange},
            {"list", (void*)fnList},
            {"tuple", (void*)fnTuple},
            {"dict", (void*)fnDict},
            {"type", (void*)fnType},
            {"format", (void*)fnFormat},
            {"dir", (void*)fnDir},
            {"hash", (void*)fnHash},
            {"round", (void*)fnRound},
            {"sorted", (void*)fnSorted},
            {"reversed", (void*)fnReversed},
            {"enumerate", (void*)fnEnumerate},
            {"ord", (void*)fnOrd},
            {"chr", (void*)fnChr},
            {"next", (void*)fnNext},
            {"isinstance", (void*)fnIsInstance},
            {"issubclass", (void*)fnIsSubclass},
            {"super", (void*)fnSuper},
            {"callable", (void*)fnCallable},
            {"id", (void*)fnId},
            {"hasattr", (void*)fnHasAttr},
            {"getattr", (void*)fnGetAttr},
            {"setattr", (void*)fnSetAttr},
            {"all", (void*)fnAll},
            {"any", (void*)fnAny},
            {"zip", (void*)fnZip},
            {"delattr", (void*)fnDelAttr},
            {"vars", (void*)fnVars},
            {"iter", (void*)fnIter},
            {"divmod", (void*)fnDivmod},
            {"bin", (void*)fnBin},
            {"oct", (void*)fnOct},
            {"hex", (void*)fnHex},
            {"map", (void*)fnMap},
            {"filter", (void*)fnFilter},
            {"exec", (void*)fnExec},
            {"ascii", (void*)fnAscii},
            {"open", (void*)fnOpen},
            {"pow", (void*)fnPow},
            {"bytes", (void*)fnBytes},
            {"bytearray", (void*)fnBytearray},
            {"memoryview", nullptr},
            {"set", (void*)fnSet},
            {"frozenset", (void*)fnFrozenset},
            {"slice", nullptr},
            {"complex", nullptr},
            {"compile", (void*)fnCompile},
            {"breakpoint", (void*)fnBreakpoint},
            {"help", (void*)fnHelp},
            {"globals", (void*)fnGlobals},
            {"locals", (void*)fnLocals},
            {"__import__", (void*)fnImport},
            {"property", nullptr},
            {"classmethod", nullptr},
            {"staticmethod", nullptr},
            {"object", nullptr},
        },
        {
            {"None", (void*)dataNone},
            {"True", (void*)dataTrue},
            {"False", (void*)dataFalse}
        },
        {
            {"object", objectClass},
            {"property", propertyClass},
            {"classmethod", classmethodClass},
            {"staticmethod", staticmethodClass},
            {"file", fileClass},
            {"memoryview", memoryviewClass},
            {"slice", sliceClass},
            {"complex", complexClass}
        });
}


}
