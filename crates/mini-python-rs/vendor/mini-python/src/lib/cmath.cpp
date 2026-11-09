#include "../cffi.hpp"

#include <cmath>
#include <complex>
#include <string>

namespace minipy {

namespace {

constexpr double CM_PI = 3.14159265358979323846;
constexpr double CM_E = 2.71828182845904523536;
constexpr double CM_TAU = 6.28318530717958647692;

// 从单个 value 提取复数 (不做参数个数检查)
// extract complex from single value (no argc check)
// ᠭᠠᠭᠴᠠ ᠤᠲᠤᠯᠭ᠎ᠠ ᠠ᠋ᠴᠠ ᠰᠢᠯᠢ ᠲᠣ᠎
bool complexValueOf(cffi& a, const value& v, std::complex<double>& out, const char* func){
    if (v.type == "int" || v.type == "bool" || v.type == "float"){
        out = std::complex<double>(a.vm->toFloat(v), 0.0);
        return true;
    }
    double re = 0.0, im = 0.0;
    if (complexPartsEx(v, re, im)){
        out = std::complex<double>(re, im);
        return true;
    }
    std::string tname = v.type == "none" ? "NoneType" : v.type;
    a.vm->raiseError("TypeError", std::string(func) + "() argument must be a real number, not '" +
                                  tname + "'");
    return false;
}

// 统一取复数参数
// Unified complex argument extraction
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠠᠷᠭᠤᠮᠧᠨᠲ
bool complexArg(cffi& a, std::complex<double>& out, const char* func){
    if (a.argc() != 1){
        a.vm->raiseError("TypeError", std::string(func) + "() takes exactly one argument (" +
                                      std::to_string(a.argc()) + " given)");
        return false;
    }
    return complexValueOf(a, a.arg(0), out, func);
}

value unaryCplx(cffi& a, const char* func, std::complex<double> (*op)(const std::complex<double>&)){
    std::complex<double> z;
    if (!complexArg(a, z, func)) return a.none();
    std::complex<double> r = op(z);
    return a.vm->makeComplex(r.real(), r.imag());
}

}

static value fnSqrt(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "sqrt")) return a.none();
    std::complex<double> r = std::sqrt(z);
    return a.vm->makeComplex(r.real(), r.imag());
}

static value fnExp(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "exp")) return a.none();
    std::complex<double> r = std::exp(z);
    return a.vm->makeComplex(r.real(), r.imag());
}

static value fnLog(cffi& a){
    std::complex<double> z;
    if (a.argc() < 1 || a.argc() > 2){
        a.vm->raiseError("TypeError", "log() takes 1 or 2 arguments");
        return a.none();
    }
    if (!complexValueOf(a, a.arg(0), z, "log")) return a.none();
    if (z.real() == 0.0 && z.imag() == 0.0){
        a.vm->raiseError("ValueError", "math domain error");
        return a.none();
    }
    std::complex<double> r = std::log(z);
    if (a.argc() == 2){
        value baseV = a.arg(1);
        std::complex<double> base(0.0, 0.0);
        if (baseV.type == "int" || baseV.type == "bool" || baseV.type == "float"){
            base = std::complex<double>(a.vm->toFloat(baseV), 0.0);
        } else {
            double re = 0.0, im = 0.0;
            if (complexPartsEx(baseV, re, im)) base = std::complex<double>(re, im);
        }
        if (base.real() == 0.0 && base.imag() == 0.0){
            a.vm->raiseError("ValueError", "math domain error");
            return a.none();
        }
        r = r / std::log(base);
    }
    return a.vm->makeComplex(r.real(), r.imag());
}

static value fnLog10(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "log10")) return a.none();
    if (z.real() == 0.0 && z.imag() == 0.0){
        a.vm->raiseError("ValueError", "math domain error");
        return a.none();
    }
    std::complex<double> r = std::log(z) / std::log(std::complex<double>(10.0, 0.0));
    return a.vm->makeComplex(r.real(), r.imag());
}

static value fnSin(cffi& a){ return unaryCplx(a, "sin", [](const std::complex<double>& z){ return std::sin(z); }); }
static value fnCos(cffi& a){ return unaryCplx(a, "cos", [](const std::complex<double>& z){ return std::cos(z); }); }
static value fnTan(cffi& a){ return unaryCplx(a, "tan", [](const std::complex<double>& z){ return std::tan(z); }); }
static value fnAsin(cffi& a){ return unaryCplx(a, "asin", [](const std::complex<double>& z){ return std::asin(z); }); }
static value fnAcos(cffi& a){ return unaryCplx(a, "acos", [](const std::complex<double>& z){ return std::acos(z); }); }
static value fnAtan(cffi& a){ return unaryCplx(a, "atan", [](const std::complex<double>& z){ return std::atan(z); }); }
static value fnSinh(cffi& a){ return unaryCplx(a, "sinh", [](const std::complex<double>& z){ return std::sinh(z); }); }
static value fnCosh(cffi& a){ return unaryCplx(a, "cosh", [](const std::complex<double>& z){ return std::cosh(z); }); }
static value fnTanh(cffi& a){ return unaryCplx(a, "tanh", [](const std::complex<double>& z){ return std::tanh(z); }); }
static value fnAsinh(cffi& a){ return unaryCplx(a, "asinh", [](const std::complex<double>& z){ return std::asinh(z); }); }
static value fnAcosh(cffi& a){ return unaryCplx(a, "acosh", [](const std::complex<double>& z){ return std::acosh(z); }); }
static value fnAtanh(cffi& a){ return unaryCplx(a, "atanh", [](const std::complex<double>& z){ return std::atanh(z); }); }

static value fnPhase(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "phase")) return a.none();
    return a.mkfloat(std::arg(z));
}

static value fnPolar(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "polar")) return a.none();
    return a.vm->makeTuple({a.mkfloat(std::abs(z)), a.mkfloat(std::arg(z))});
}

static value fnRect(cffi& a){
    if (a.argc() != 2){
        a.vm->raiseError("TypeError", "rect() takes exactly 2 arguments");
        return a.none();
    }
    double r = a.vm->toFloat(a.arg(0));
    double phi = a.vm->toFloat(a.arg(1));
    std::complex<double> z = std::polar(r, phi);
    return a.vm->makeComplex(z.real(), z.imag());
}

static value fnAbs(cffi& a){
    std::complex<double> z;
    if (!complexArg(a, z, "abs")) return a.none();
    return a.mkfloat(std::abs(z));
}

static value fnIsnan(cffi& a){
    double re = 0.0, im = 0.0;
    value v = a.arg(0);
    if (v.type == "int" || v.type == "bool" || v.type == "float"){
        re = a.vm->toFloat(v);
    } else if (!complexPartsEx(v, re, im)){
        a.vm->raiseError("TypeError", "isnan() argument must be a real number, not '" + v.type + "'");
        return a.none();
    }
    return a.mkbool(std::isnan(re) || std::isnan(im));
}

static value fnIsinf(cffi& a){
    double re = 0.0, im = 0.0;
    value v = a.arg(0);
    if (v.type == "int" || v.type == "bool" || v.type == "float"){
        re = a.vm->toFloat(v);
    } else if (!complexPartsEx(v, re, im)){
        a.vm->raiseError("TypeError", "isinf() argument must be a real number, not '" + v.type + "'");
        return a.none();
    }
    return a.mkbool(std::isinf(re) || std::isinf(im));
}

static value fnIsfinite(cffi& a){
    double re = 0.0, im = 0.0;
    value v = a.arg(0);
    if (v.type == "int" || v.type == "bool" || v.type == "float"){
        re = a.vm->toFloat(v);
    } else if (!complexPartsEx(v, re, im)){
        a.vm->raiseError("TypeError", "isfinite() argument must be a real number, not '" + v.type + "'");
        return a.none();
    }
    return a.mkbool(std::isfinite(re) && std::isfinite(im));
}

static value dataPi(cffi& a){ return a.mkfloat(CM_PI); }
static value dataE(cffi& a){ return a.mkfloat(CM_E); }
static value dataTau(cffi& a){ return a.mkfloat(CM_TAU); }
static value dataInf(cffi& a){ return a.mkfloat(HUGE_VAL); }
static value dataInfj(cffi& a){ return a.vm->makeComplex(0.0, HUGE_VAL); }
static value dataNan(cffi& a){ return a.mkfloat(std::nan("")); }
static value dataNanj(cffi& a){ return a.vm->makeComplex(0.0, std::nan("")); }

// cffi cmath 库
// cffi cmath library
// cffi cmath ᠨᠣᠮ
ffilib cffi_cmath_lib(){
    return cffi_lib(
        "cmath",
        {
            {"sqrt", (void*)fnSqrt},
            {"exp", (void*)fnExp},
            {"log", (void*)fnLog},
            {"log10", (void*)fnLog10},
            {"sin", (void*)fnSin},
            {"cos", (void*)fnCos},
            {"tan", (void*)fnTan},
            {"asin", (void*)fnAsin},
            {"acos", (void*)fnAcos},
            {"atan", (void*)fnAtan},
            {"sinh", (void*)fnSinh},
            {"cosh", (void*)fnCosh},
            {"tanh", (void*)fnTanh},
            {"asinh", (void*)fnAsinh},
            {"acosh", (void*)fnAcosh},
            {"atanh", (void*)fnAtanh},
            {"phase", (void*)fnPhase},
            {"polar", (void*)fnPolar},
            {"rect", (void*)fnRect},
            {"abs", (void*)fnAbs},
            {"isnan", (void*)fnIsnan},
            {"isinf", (void*)fnIsinf},
            {"isfinite", (void*)fnIsfinite}
        },
        {
            {"pi", (void*)dataPi},
            {"e", (void*)dataE},
            {"tau", (void*)dataTau},
            {"inf", (void*)dataInf},
            {"infj", (void*)dataInfj},
            {"nan", (void*)dataNan},
            {"nanj", (void*)dataNanj}
        });
}

}
