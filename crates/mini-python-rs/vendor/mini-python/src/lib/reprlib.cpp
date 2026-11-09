#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {

namespace {

// 递归表示对象
// Represent an object recursively
// ᠭᠠᠷᠴᠠᠭ ᠢ ᠳᠠᠪᠬᠠᠷᠢᠶ᠎ᠠ ᠢᠯᠡᠳᠭᠡᠬᠦ
std::string reprValue(cffi& a, const value& v, long long level,
                      long long maxlevel, long long maxstring, long long maxlong,
                      long long maxlist, long long maxtuple, long long maxdict,
                      const std::string& fill){
    if (level <= 0) return fill;
    if (v.type == "str"){
        std::string text = a.vm->toString(v);
        if (static_cast<long long>(text.size()) > maxstring){
            std::string part = text.substr(0, static_cast<size_t>(maxstring));
            std::string quoted = pythonRepr(*a.vm, a.mkstr(part));
            if (!quoted.empty() && quoted.back() == '\''){
                quoted.pop_back();
                quoted += "..." + std::string("'");
            } else {
                quoted = pythonRepr(*a.vm, a.mkstr(text.substr(0, static_cast<size_t>(maxstring)))) + "...";
            }
            return quoted;
        }
        return pythonRepr(*a.vm, v);
    }
    if (v.type == "int"){
        std::string text = a.vm->toString(v);
        if (static_cast<long long>(text.size()) > maxlong){
            return text.substr(0, static_cast<size_t>(maxlong)) + "...";
        }
        return text;
    }
    if (v.type == "list" && v.ptr){
        auto* items = static_cast<std::vector<value>*>(v.ptr);
        std::string out = "[";
        for (size_t i = 0; i < items->size(); ++i){
            if (static_cast<long long>(i) >= maxlist){
                if (i) out += ", ";
                out += fill;
                break;
            }
            if (i) out += ", ";
            out += reprValue(a, (*items)[i], level - 1, maxlevel, maxstring, maxlong,
                             maxlist, maxtuple, maxdict, fill);
        }
        out += "]";
        return out;
    }
    if (v.type == "tuple" && v.ptr){
        auto* items = static_cast<std::vector<value>*>(v.ptr);
        std::string out = "(";
        bool truncated = false;
        for (size_t i = 0; i < items->size(); ++i){
            if (static_cast<long long>(i) >= maxtuple){
                if (i) out += ", ";
                out += fill;
                truncated = true;
                break;
            }
            if (i) out += ", ";
            out += reprValue(a, (*items)[i], level - 1, maxlevel, maxstring, maxlong,
                             maxlist, maxtuple, maxdict, fill);
        }
        if (items->size() == 1 && !truncated) out += ",";
        out += ")";
        return out;
    }
    if (v.type == "dict" && v.ptr){
        auto* items = static_cast<dict_t*>(v.ptr);
        std::string out = "{";
        for (size_t i = 0; i < items->size(); ++i){
            if (static_cast<long long>(i) >= maxdict){
                if (i) out += ", ";
                out += fill;
                break;
            }
            if (i) out += ", ";
            out += reprValue(a, (*items)[i].first, level - 1, maxlevel, maxstring, maxlong,
                             maxlist, maxtuple, maxdict, fill);
            out += ": ";
            out += reprValue(a, (*items)[i].second, level - 1, maxlevel, maxstring, maxlong,
                             maxlist, maxtuple, maxdict, fill);
        }
        out += "}";
        return out;
    }
    return pythonRepr(*a.vm, v);
}

// 从属性读取截断限制
// Read truncation limits from attributes
// ᠲᠠᠯᠠᠭ᠌ ᠡᠴᠡ ᠬᠡᠮᠵᠢᠶ᠎ᠡ ᠦᠨᠰᠢᠬᠦ
struct ReprLimits{
    long long maxlevel = 6;
    long long maxstring = 30;
    long long maxlong = 40;
    long long maxlist = 6;
    long long maxtuple = 6;
    long long maxdict = 4;
    long long maxother = 30;
    std::string fill = "...";
};

// 读取实例限制
// Read limits from a Repr instance
// Repr ᠬᠢᠶ᠎ᠡ ᠡᠴᠡ ᠬᠡᠮᠵᠢᠶ᠎ᠡ ᠦᠨᠰᠢᠬᠦ
ReprLimits limitsOf(cffi& a){
    ReprLimits limits;
    auto readInt = [&](const std::string& name, long long fallback){
        value v = a.attr(name);
        return v.type == "none" ? fallback : a.vm->toInt(v);
    };
    limits.maxlevel = readInt("maxlevel", 6);
    limits.maxstring = readInt("maxstring", 30);
    limits.maxlong = readInt("maxlong", 40);
    limits.maxlist = readInt("maxlist", 6);
    limits.maxtuple = readInt("maxtuple", 6);
    limits.maxdict = readInt("maxdict", 4);
    limits.maxother = readInt("maxother", 30);
    value fill = a.attr("fillvalue");
    if (fill.type != "none") limits.fill = a.vm->toString(fill);
    return limits;
}
}

// Repr 构造器
// Repr constructor
// Repr ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnReprConstructor(cffi& a){
    value obj = a.mkobject(nullptr);
    a.setattr(obj, "maxlevel", a.mkint(6));
    a.setattr(obj, "maxtuple", a.mkint(6));
    a.setattr(obj, "maxlist", a.mkint(6));
    a.setattr(obj, "maxarray", a.mkint(5));
    a.setattr(obj, "maxdict", a.mkint(4));
    a.setattr(obj, "maxset", a.mkint(6));
    a.setattr(obj, "maxfrozenset", a.mkint(6));
    a.setattr(obj, "maxdeque", a.mkint(6));
    a.setattr(obj, "maxstring", a.mkint(30));
    a.setattr(obj, "maxlong", a.mkint(40));
    a.setattr(obj, "maxother", a.mkint(30));
    a.setattr(obj, "fillvalue", a.mkstr("..."));
    return obj;
}

// Repr.repr1
// Repr.repr1
// Repr.repr1
static value fnReprRepr1(cffi& a){
    ReprLimits limits = limitsOf(a);
    long long level = (a.argc() > 1) ? a.arg_int(1) : limits.maxlevel;
    return a.mkstr(reprValue(a, a.arg(0), level, limits.maxlevel, limits.maxstring,
                             limits.maxlong, limits.maxlist, limits.maxtuple,
                             limits.maxdict, limits.fill));
}

// Repr.repr
// Repr.repr
// Repr.repr
static value fnReprRepr(cffi& a){
    ReprLimits limits = limitsOf(a);
    return a.mkstr(reprValue(a, a.arg(0), limits.maxlevel, limits.maxlevel,
                             limits.maxstring, limits.maxlong, limits.maxlist,
                             limits.maxtuple, limits.maxdict, limits.fill));
}

// fn Repr
// fn repr
// fn Repr
static value fnRepr(cffi& a){
    ReprLimits limits;
    return a.mkstr(reprValue(a, a.arg(0), limits.maxlevel, limits.maxlevel,
                             limits.maxstring, limits.maxlong, limits.maxlist,
                             limits.maxtuple, limits.maxdict, limits.fill));
}

// cffi reprlib lib
// cffi reprlib lib
// cffi reprlib lib
ffilib cffi_reprlib_lib(){
    ffi_class reprClass;
    reprClass.name = "Repr";
    reprClass.constructor = (void*)fnReprConstructor;
    reprClass.methods = {
        {"repr", (void*)fnReprRepr},
        {"repr1", (void*)fnReprRepr1}
    };
    return cffi_lib(
        "reprlib",
        {
            {"repr", (void*)fnRepr}
        },
        {},
        {
            {"Repr", reprClass}
        });
}

}
