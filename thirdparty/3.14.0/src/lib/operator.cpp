#include "../cffi.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace minipy {





using namespace minipy;

// fn 加
// fn add
// fn ᠨᠡᠮᠡᠵᠡ
static value fnAdd(cffi& a){ return a.vm->applyBinary("+", a.arg(0), a.arg(1)); }

// fn 减
// fn sub
// fn ᠬᠠᠯᠠᠬᠤ
static value fnSub(cffi& a){ return a.vm->applyBinary("-", a.arg(0), a.arg(1)); }

// fn 乘
// fn mul
// fn ᠦᠯᠳᠡᠨ᠎ᠠ
static value fnMul(cffi& a){ return a.vm->applyBinary("*", a.arg(0), a.arg(1)); }

// fn 真 除
// fn true div
// fn ᠨᠲᠠ ᠬᠠᠷᠢᠭᠤᠯᠭᠤ
static value fnTrueDiv(cffi& a){ return a.vm->applyBinary("/", a.arg(0), a.arg(1)); }

// fn Floor 除
// fn floor div
// fn Floor ᠬᠠᠷᠢᠭᠤᠯᠭᠤ
static value fnFloorDiv(cffi& a){ return a.vm->applyBinary("//", a.arg(0), a.arg(1)); }

// fn 模
// fn mod
// fn ᠣᠷᠣᠭᠤᠯᠭ᠎ᠠ
static value fnMod(cffi& a){ return a.vm->applyBinary("%", a.arg(0), a.arg(1)); }

// fn 幂
// fn pow
// fn ᠬᠦᠴᠡᠪᠡᠷ
static value fnPow(cffi& a){ return a.vm->applyBinary("**", a.arg(0), a.arg(1)); }

// fn 等于
// fn eq
// fn ᠲᠣᠬᠢᠷᠠᠭᠤᠯᠳᠠᠭ᠎ᠠ
static value fnEq(cffi& a){ return a.mkbool(a.vm->valuesEqual(a.arg(0), a.arg(1))); }

// fn 不等于
// fn ne
// fn ᠲᠣᠬᠢᠷᠠᠭᠤᠯᠳᠠᠭᠤᠯ ᠦᠭᠡᠢ
static value fnNe(cffi& a){ return a.mkbool(!a.vm->valuesEqual(a.arg(0), a.arg(1))); }

// fn 小于
// fn lt
// fn ᠪᠠᠭ᠎ᠠ
static value fnLt(cffi& a){ return a.vm->applyBinary("<", a.arg(0), a.arg(1)); }

// fn 小于等于
// fn le
// fn ᠪᠠᠭ᠎ᠠ ᠪᠡᠶᠡᠨ
static value fnLe(cffi& a){ return a.vm->applyBinary("<=", a.arg(0), a.arg(1)); }

// fn 大于
// fn gt
// fn ᠶᠡᠬᠡ
static value fnGt(cffi& a){ return a.vm->applyBinary(">", a.arg(0), a.arg(1)); }

// fn 大于等于
// fn ge
// fn ᠶᠡᠬᠡ ᠪᠡᠶᠡᠨ
static value fnGe(cffi& a){ return a.vm->applyBinary(">=", a.arg(0), a.arg(1)); }

// fn 是
// fn is
// fn ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ
static value fnIs(cffi& a){ return a.mkbool(a.arg(0).ptr == a.arg(1).ptr); }

// fn 是 非
// fn is not
// fn ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠦᠭᠡᠢ
static value fnIsNot(cffi& a){ return a.mkbool(a.arg(0).ptr != a.arg(1).ptr); }

// fn 负号
// fn neg
// fn ᠰᠣᠩᠭᠣᠨ ᠲᠥᠪ
static value fnNeg(cffi& a){ return a.vm->applyUnary("-", a.arg(0)); }

// fn 正号
// fn pos
// fn ᠡᠪᠬᠡᠲᠡᠶᠢ ᠲᠥᠪ
static value fnPos(cffi& a){ return a.vm->applyUnary("+", a.arg(0)); }

// fn 取反
// fn invert
// fn ᠦᠬᠡᠬᠡᠵᠡ
static value fnInvert(cffi& a){ return a.vm->applyUnary("~", a.arg(0)); }

// fn 非
// fn not
// fn ᠦᠭᠡᠢ
static value fnNot(cffi& a){ return a.mkbool(!a.vm->truthy(a.arg(0))); }

// fn Truth
// fn truth
// fn Truth
static value fnTruth(cffi& a){ return a.mkbool(a.vm->truthy(a.arg(0))); }

// fn Abs
// fn abs
// fn Abs
static value fnAbs(cffi& a){
    value value = a.arg(0);
    if (value.type == "float") return a.mkfloat(std::fabs(a.vm->toFloat(value)));
    long long number = a.vm->toInt(value);
    return a.mkint(number < 0 ? -number : number);
}

// fn Contains
// fn contains
// fn Contains
static value fnContains(cffi& a){
    return a.vm->applyBinary("in", a.arg(1), a.arg(0));
}

// fn 拼接
// fn concat
// fn ᠨᠢᠯᠡᠨ᠎ᠠ
static value fnConcat(cffi& a){
    return a.vm->applyBinary("+", a.arg(0), a.arg(1));
}

// fn Getitem
// fn getitem
// fn Getitem
static value fnGetitem(cffi& a){
    value container = a.arg(0);
    if (container.type == "dict" && container.ptr != nullptr){
        if (!a.vm->hashable(a.arg(1))){
            std::string keyType = a.arg(1).type == "none" ? "NoneType" : a.arg(1).type;
            a.vm->raiseError("TypeError", "cannot use '" + keyType +
                                          "' as a dict key (unhashable type: '" +
                                          keyType + "')");
            return a.none();
        }
        value* found = a.vm->dictFind(container, a.arg(1));
        if (found == nullptr){
            a.vm->raiseError("KeyError", pythonRepr(*a.vm, a.arg(1)));
            return a.none();
        }
        return *found;
    }
    value key = a.arg(1);
    if ((container.type == "list" || container.type == "tuple") && container.ptr != nullptr){
        if (key.type != "int" && key.type != "bool"){
            std::string keyType = key.type == "none" ? "NoneType" : key.type;
            a.vm->raiseError("TypeError", container.type + " indices must be integers or slices, not " +
                                          keyType);
            return a.none();
        }
        long long index = a.vm->toInt(key);
        auto* list = static_cast<std::vector<value>*>(container.ptr);
        if (index < 0) index += static_cast<long long>(list->size());
        if (index < 0 || index >= static_cast<long long>(list->size())){
            a.vm->raiseError("IndexError", container.type + " index out of range");
            return a.none();
        }
        return (*list)[static_cast<size_t>(index)];
    }
    long long index = a.arg_int(1);
    if (container.type == "str" && container.ptr != nullptr){
        std::string text = a.vm->toString(container);
        if (index < 0) index += static_cast<long long>(text.size());
        if (index < 0 || index >= static_cast<long long>(text.size())){
            a.vm->raiseError("IndexError", "string index out of range");
            return a.none();
        }
        return a.mkstr(std::string(1, text[static_cast<size_t>(index)]));
    }
    a.vm->raiseError("TypeError", "'" + a.vm->toString(container) + "' object is not subscriptable");
    return a.none();
}

// fn Setitem
// fn setitem
// fn Setitem
static value fnSetitem(cffi& a){
    value container = a.arg(0);
    if (container.type == "dict" && container.ptr != nullptr){
        std::string error = a.vm->dictSet(container, a.arg(1), a.arg(2));
        if (!error.empty()) a.vm->raiseError("TypeError", error);
        return a.none();
    }
    long long index = a.arg_int(1);
    if (container.type == "list" && container.ptr != nullptr){
        auto* list = static_cast<std::vector<value>*>(container.ptr);
        if (index < 0) index += static_cast<long long>(list->size());
        if (index < 0 || index >= static_cast<long long>(list->size())){
            a.vm->raiseError("IndexError", "list assignment index out of range");
            return a.none();
        }
        (*list)[static_cast<size_t>(index)] = a.arg(2);
        return a.none();
    }
    if (container.type == "tuple"){
        a.vm->raiseError("TypeError", "'tuple' object does not support item assignment");
        return a.none();
    }
    a.vm->raiseError("TypeError", "object does not support item assignment");
    return a.none();
}

// fn Count Of
// fn count of
// fn Count Of
static value fnCountOf(cffi& a){
    value container = a.arg(0);
    value target = a.arg(1);
    long long count = 0;
    if ((container.type == "list" || container.type == "tuple") && container.ptr != nullptr){
        auto* list = static_cast<std::vector<value>*>(container.ptr);
        for (auto& item : *list) if (a.vm->valuesEqual(item, target)) count++;
        return a.mkint(count);
    }
    if (container.type == "str" && container.ptr != nullptr){
        std::string text = a.vm->toString(container);
        std::string needle = a.vm->toString(target);
        if (!needle.empty()){
            size_t begin = 0;
            while ((begin = text.find(needle, begin)) != std::string::npos){
                count++;
                begin += needle.size();
            }
        }
        return a.mkint(count);
    }
    a.vm->raiseError("TypeError", "object is not iterable");
    return a.none();
}

// fn 索引 Of
// fn index of
// fn ᠢᠨᠳᠡᠭᠡᠰ Of
static value fnIndexOf(cffi& a){
    value container = a.arg(0);
    value target = a.arg(1);
    if ((container.type == "list" || container.type == "tuple") && container.ptr != nullptr){
        auto* list = static_cast<std::vector<value>*>(container.ptr);
        for (size_t i = 0; i < list->size(); ++i){
            if (a.vm->valuesEqual((*list)[i], target)) return a.mkint(static_cast<long long>(i));
        }
        a.vm->raiseError("ValueError", "x is not in list");
        return a.none();
    }
    if (container.type == "str" && container.ptr != nullptr){
        size_t found = a.vm->toString(container).find(a.vm->toString(target));
        if (found == std::string::npos){
            a.vm->raiseError("ValueError", "substring not found");
            return a.none();
        }
        return a.mkint(static_cast<long long>(found));
    }
    a.vm->raiseError("TypeError", "object is not iterable");
    return a.none();
}

// fn 与
// fn and
// fn ᠪᠠ
static value fnAnd(cffi& a){ return a.vm->applyBinary("&", a.arg(0), a.arg(1)); }

// fn 或
// fn or
// fn ᠡᠰᠡᠪᠡᠯ
static value fnOr(cffi& a){ return a.vm->applyBinary("|", a.arg(0), a.arg(1)); }

// fn Xor
// fn xor
// fn Xor
static value fnXor(cffi& a){ return a.vm->applyBinary("^", a.arg(0), a.arg(1)); }

// fn Lshift
// fn lshift
// fn Lshift
static value fnLshift(cffi& a){ return a.vm->applyBinary("<<", a.arg(0), a.arg(1)); }

// fn Rshift
// fn rshift
// fn Rshift
static value fnRshift(cffi& a){ return a.vm->applyBinary(">>", a.arg(0), a.arg(1)); }

// fn Matmul
// fn matmul
// fn Matmul
static value fnMatmul(cffi& a){
    value left = a.arg(0);
    value right = a.arg(1);
    a.vm->raiseError("TypeError", "unsupported operand type(s) for @: '" +
                                    a.vm->pythonTypeName(left) + "' and '" +
                                    a.vm->pythonTypeName(right) + "'");
    return a.none();
}

// fn Iadd
// fn iadd
// fn Iadd
static value fnIadd(cffi& a){
    value left = a.arg(0);
    value right = a.arg(1);
    if (left.type == "list" && left.ptr != nullptr &&
        (right.type == "list" || right.type == "tuple") && right.ptr != nullptr){
        auto* list = static_cast<std::vector<value>*>(left.ptr);
        auto* items = static_cast<std::vector<value>*>(right.ptr);
        list->insert(list->end(), items->begin(), items->end());
        return left;
    }
    return a.vm->applyBinary("+", left, right);
}

// fn Imul
// fn imul
// fn Imul
static value fnImul(cffi& a){
    value left = a.arg(0);
    value right = a.arg(1);
    if (left.type == "list" && left.ptr != nullptr &&
        (right.type == "int" || right.type == "bool")){
        auto* list = static_cast<std::vector<value>*>(left.ptr);
        long long count = a.vm->toInt(right);
        std::vector<value> original = *list;
        list->clear();
        for (long long i = 0; i < count; ++i){
            list->insert(list->end(), original.begin(), original.end());
        }
        return left;
    }
    return a.vm->applyBinary("*", left, right);
}

// fn Isub
// fn isub
// fn Isub
static value fnIsub(cffi& a){ return a.vm->applyBinary("-", a.arg(0), a.arg(1)); }

// fn Itruediv
// fn itruediv
// fn Itruediv
static value fnItruediv(cffi& a){ return a.vm->applyBinary("/", a.arg(0), a.arg(1)); }

// fn Ifloordiv
// fn ifloordiv
// fn Ifloordiv
static value fnIfloordiv(cffi& a){ return a.vm->applyBinary("//", a.arg(0), a.arg(1)); }

// fn Imod
// fn imod
// fn Imod
static value fnImod(cffi& a){ return a.vm->applyBinary("%", a.arg(0), a.arg(1)); }

// fn Ipow
// fn ipow
// fn Ipow
static value fnIpow(cffi& a){ return a.vm->applyBinary("**", a.arg(0), a.arg(1)); }

// fn Ilshift
// fn ilshift
// fn Ilshift
static value fnIlshift(cffi& a){ return a.vm->applyBinary("<<", a.arg(0), a.arg(1)); }

// fn Irshift
// fn irshift
// fn Irshift
static value fnIrshift(cffi& a){ return a.vm->applyBinary(">>", a.arg(0), a.arg(1)); }

// fn Iand
// fn iand
// fn Iand
static value fnIand(cffi& a){ return a.vm->applyBinary("&", a.arg(0), a.arg(1)); }

// fn Ior
// fn ior
// fn Ior
static value fnIor(cffi& a){ return a.vm->applyBinary("|", a.arg(0), a.arg(1)); }

// fn Ixor
// fn ixor
// fn Ixor
static value fnIxor(cffi& a){ return a.vm->applyBinary("^", a.arg(0), a.arg(1)); }

// fn Length Hint
// fn length hint
// fn Length Hint
static value fnLengthHint(cffi& a){
    value object = a.arg(0);
    long long fallback = (a.argc() > 1) ? a.arg_int(1) : 0;
    if ((object.type == "list" || object.type == "tuple") && object.ptr != nullptr){
        auto* items = static_cast<std::vector<value>*>(object.ptr);
        return a.mkint(static_cast<long long>(items->size()));
    }
    if (object.type == "str" && object.ptr != nullptr){
        return a.mkint(static_cast<long long>(a.vm->toString(object).size()));
    }
    return a.mkint(fallback);
}

// fn Delitem
// fn delitem
// fn Delitem
static value fnDelitem(cffi& a){
    value container = a.arg(0);
    if (container.type == "dict" && container.ptr != nullptr){
        if (!a.vm->dictRemove(container, a.arg(1))){
            a.vm->raiseError("KeyError", pythonRepr(*a.vm, a.arg(1)));
        }
        return a.none();
    }
    if (container.type == "list" && container.ptr != nullptr){
        auto* list = static_cast<std::vector<value>*>(container.ptr);
        long long index = a.arg_int(1);
        if (index < 0) index += static_cast<long long>(list->size());
        if (index < 0 || index >= static_cast<long long>(list->size())){
            a.vm->raiseError("IndexError", "list assignment index out of range");
            return a.none();
        }
        list->erase(list->begin() + static_cast<size_t>(index));
        return a.none();
    }
    a.vm->raiseError("TypeError", "'" + a.vm->pythonTypeName(container) +
                                    "' object doesn't support item deletion");
    return a.none();
}

// cffi operator lib
// cffi operator lib
// cffi operator lib
ffilib cffi_operator_lib(){
    return cffi_lib(
        "operator",
        {
            {"add", (void*)fnAdd},
            {"sub", (void*)fnSub},
            {"mul", (void*)fnMul},
            {"truediv", (void*)fnTrueDiv},
            {"floordiv", (void*)fnFloorDiv},
            {"mod", (void*)fnMod},
            {"pow", (void*)fnPow},
            {"eq", (void*)fnEq},
            {"ne", (void*)fnNe},
            {"lt", (void*)fnLt},
            {"le", (void*)fnLe},
            {"gt", (void*)fnGt},
            {"ge", (void*)fnGe},
            {"is_", (void*)fnIs},
            {"is_not", (void*)fnIsNot},
            {"neg", (void*)fnNeg},
            {"pos", (void*)fnPos},
            {"invert", (void*)fnInvert},
            {"not_", (void*)fnNot},
            {"truth", (void*)fnTruth},
            {"abs", (void*)fnAbs},
            {"contains", (void*)fnContains},
            {"concat", (void*)fnConcat},
            {"getitem", (void*)fnGetitem},
            {"setitem", (void*)fnSetitem},
            {"delitem", (void*)fnDelitem},
            {"and_", (void*)fnAnd},
            {"or_", (void*)fnOr},
            {"xor_", (void*)fnXor},
            {"lshift", (void*)fnLshift},
            {"rshift", (void*)fnRshift},
            {"matmul", (void*)fnMatmul},
            {"iadd", (void*)fnIadd},
            {"isub", (void*)fnIsub},
            {"imul", (void*)fnImul},
            {"itruediv", (void*)fnItruediv},
            {"ifloordiv", (void*)fnIfloordiv},
            {"imod", (void*)fnImod},
            {"ipow", (void*)fnIpow},
            {"ilshift", (void*)fnIlshift},
            {"irshift", (void*)fnIrshift},
            {"iand", (void*)fnIand},
            {"ior", (void*)fnIor},
            {"ixor", (void*)fnIxor},
            {"length_hint", (void*)fnLengthHint},
            {"index", (void*)fnIndexOf},
            {"countOf", (void*)fnCountOf},
            {"indexOf", (void*)fnIndexOf}
        });
}


}
