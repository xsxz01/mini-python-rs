#include "../cffi.hpp"

#include <string>

namespace minipy {

namespace {

value abstractCtor(cffi& a, const char* name){
    a.vm->raiseError("TypeError", std::string("Can't instantiate abstract class ") + name);
    return a.none();
}

}

static value fnNumberCtor(cffi& a){ return a.mkobject(nullptr); }
static value fnComplexCtor(cffi& a){ return abstractCtor(a, "Complex"); }
static value fnRealCtor(cffi& a){ return abstractCtor(a, "Real"); }
static value fnRationalCtor(cffi& a){ return abstractCtor(a, "Rational"); }
static value fnIntegralCtor(cffi& a){ return abstractCtor(a, "Integral"); }

// cffi numbers 库
// cffi numbers library
// cffi numbers ᠨᠣᠮ
ffilib cffi_numbers_lib(){
    ffi_class numberCls;
    numberCls.name = "Number";
    numberCls.constructor = (void*)fnNumberCtor;

    ffi_class complexCls;
    complexCls.name = "Complex";
    complexCls.constructor = (void*)fnComplexCtor;

    ffi_class realCls;
    realCls.name = "Real";
    realCls.constructor = (void*)fnRealCtor;

    ffi_class rationalCls;
    rationalCls.name = "Rational";
    rationalCls.constructor = (void*)fnRationalCtor;

    ffi_class integralCls;
    integralCls.name = "Integral";
    integralCls.constructor = (void*)fnIntegralCtor;

    return cffi_lib(
        "numbers",
        {},
        {},
        {
            {"Number", numberCls},
            {"Complex", complexCls},
            {"Real", realCls},
            {"Rational", rationalCls},
            {"Integral", integralCls}
        });
}

}
