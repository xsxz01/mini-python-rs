#include "../cffi.hpp"

#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace minipy {





// fn Sqrt
// fn sqrt
// fn Sqrt
static minipy::value fnSqrt(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x < 0){
        a.vm->raiseError("ValueError", "expected a nonnegative input, got " +
                                       a.vm->toString(a.mkfloat(x)));
        return a.none();
    }
    return a.mkfloat(std::sqrt(x));
}

// fn 幂
// fn pow
// fn ᠬᠦᠴᠡᠪᠡᠷ
static minipy::value fnPow(minipy::cffi& a){
    return a.mkfloat(std::pow(a.arg_float(0), a.arg_float(1)));
}

// fn Fabs
// fn fabs
// fn Fabs
static minipy::value fnFabs(minipy::cffi& a){ return a.mkfloat(std::fabs(a.arg_float(0))); }

// fn Floor
// fn floor
// fn Floor
static minipy::value fnFloor(minipy::cffi& a){ return a.mkint(static_cast<long long>(std::floor(a.arg_float(0)))); }

// fn Ceil
// fn ceil
// fn Ceil
static minipy::value fnCeil(minipy::cffi& a){ return a.mkint(static_cast<long long>(std::ceil(a.arg_float(0)))); }

// fn Trunc
// fn trunc
// fn Trunc
static minipy::value fnTrunc(minipy::cffi& a){ return a.mkint(static_cast<long long>(std::trunc(a.arg_float(0)))); }

// fn Sin
// fn sin
// fn Sin
static minipy::value fnSin(minipy::cffi& a){ return a.mkfloat(std::sin(a.arg_float(0))); }

// fn Cos
// fn cos
// fn Cos
static minipy::value fnCos(minipy::cffi& a){ return a.mkfloat(std::cos(a.arg_float(0))); }

// fn Tan
// fn tan
// fn Tan
static minipy::value fnTan(minipy::cffi& a){ return a.mkfloat(std::tan(a.arg_float(0))); }

// fn Asin
// fn asin
// fn Asin
static minipy::value fnAsin(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x < -1.0 || x > 1.0){
        a.vm->raiseError("ValueError", "expected a number in range from -1 up to 1, got " +
                                       a.vm->toString(a.mkfloat(x)));
        return a.none();
    }
    return a.mkfloat(std::asin(x));
}

// fn Acos
// fn acos
// fn Acos
static minipy::value fnAcos(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x < -1.0 || x > 1.0){
        a.vm->raiseError("ValueError", "expected a number in range from -1 up to 1, got " +
                                       a.vm->toString(a.mkfloat(x)));
        return a.none();
    }
    return a.mkfloat(std::acos(x));
}

// fn Atan
// fn atan
// fn Atan
static minipy::value fnAtan(minipy::cffi& a){ return a.mkfloat(std::atan(a.arg_float(0))); }

// fn Atan2
// fn atan2
// fn Atan2
static minipy::value fnAtan2(minipy::cffi& a){ return a.mkfloat(std::atan2(a.arg_float(0), a.arg_float(1))); }

// fn Sinh
// fn sinh
// fn Sinh
static minipy::value fnSinh(minipy::cffi& a){ return a.mkfloat(std::sinh(a.arg_float(0))); }

// fn Cosh
// fn cosh
// fn Cosh
static minipy::value fnCosh(minipy::cffi& a){ return a.mkfloat(std::cosh(a.arg_float(0))); }

// fn Tanh
// fn tanh
// fn Tanh
static minipy::value fnTanh(minipy::cffi& a){ return a.mkfloat(std::tanh(a.arg_float(0))); }

// fn Asinh
// fn asinh
// fn Asinh
static minipy::value fnAsinh(minipy::cffi& a){ return a.mkfloat(std::asinh(a.arg_float(0))); }

// fn Acosh
// fn acosh
// fn Acosh
static minipy::value fnAcosh(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x < 1.0){
        a.vm->raiseError("ValueError", "expected argument value not less than 1, got " +
                                       a.vm->toString(a.mkfloat(x)));
        return a.none();
    }
    return a.mkfloat(std::acosh(x));
}

// fn Atanh
// fn atanh
// fn Atanh
static minipy::value fnAtanh(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x <= -1.0 || x >= 1.0){
        a.vm->raiseError("ValueError", "expected a number between -1 and 1, got " +
                                       a.vm->toString(a.mkfloat(x)));
        return a.none();
    }
    return a.mkfloat(std::atanh(x));
}

// fn Exp
// fn exp
// fn Exp
static minipy::value fnExp(minipy::cffi& a){ return a.mkfloat(std::exp(a.arg_float(0))); }

// fn Expm1
// fn expm1
// fn Expm1
static minipy::value fnExpm1(minipy::cffi& a){ return a.mkfloat(std::expm1(a.arg_float(0))); }

// fn Log
// fn log
// fn Log
static minipy::value fnLog(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x <= 0.0){
        a.vm->raiseError("ValueError", "expected a positive input");
        return a.none();
    }
    if (a.argc() >= 2) return a.mkfloat(std::log(x) / std::log(a.arg_float(1)));
    return a.mkfloat(std::log(x));
}

// fn Log10
// fn log10
// fn Log10
static minipy::value fnLog10(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x <= 0.0){
        a.vm->raiseError("ValueError", "expected a positive input");
        return a.none();
    }
    return a.mkfloat(std::log10(x));
}

// fn Log2
// fn log2
// fn Log2
static minipy::value fnLog2(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x <= 0.0){
        a.vm->raiseError("ValueError", "expected a positive input");
        return a.none();
    }
    return a.mkfloat(std::log2(x));
}

// fn Log1p
// fn log1p
// fn Log1p
static minipy::value fnLog1p(minipy::cffi& a){
    double x = a.arg_float(0);
    if (x <= -1.0){
        a.vm->raiseError("ValueError", "math domain error");
        return a.none();
    }
    return a.mkfloat(std::log1p(x));
}

// fn Fmod
// fn fmod
// fn Fmod
static minipy::value fnFmod(minipy::cffi& a){ return a.mkfloat(std::fmod(a.arg_float(0), a.arg_float(1))); }

// fn Hypot
// fn hypot
// fn Hypot
static minipy::value fnHypot(minipy::cffi& a){ return a.mkfloat(std::hypot(a.arg_float(0), a.arg_float(1))); }

// fn Copysign
// fn copysign
// fn Copysign
static minipy::value fnCopysign(minipy::cffi& a){ return a.mkfloat(std::copysign(a.arg_float(0), a.arg_float(1))); }

// fn Remainder
// fn remainder
// fn Remainder
static minipy::value fnRemainder(minipy::cffi& a){ return a.mkfloat(std::remainder(a.arg_float(0), a.arg_float(1))); }

// fn Degrees
// fn degrees
// fn Degrees
static minipy::value fnDegrees(minipy::cffi& a){ return a.mkfloat(a.arg_float(0) * 180.0 / 3.14159265358979323846); }

// fn Radians
// fn radians
// fn Radians
static minipy::value fnRadians(minipy::cffi& a){ return a.mkfloat(a.arg_float(0) * 3.14159265358979323846 / 180.0); }

// fn Factorial
// fn factorial
// fn Factorial
static minipy::value fnFactorial(minipy::cffi& a){
    long long n = a.arg_int(0);
    if (n < 0){
        a.vm->raiseError("ValueError", "factorial() not defined for negative values");
        return a.none();
    }
    long long result = 1;
    for (long long i = 2; i <= n; ++i) result *= i;
    return a.mkint(result);
}

// fn Gcd
// fn gcd
// fn Gcd
static minipy::value fnGcd(minipy::cffi& a){
    long long result = 0;
    for (size_t i = 0; i < a.argc(); ++i) result = std::gcd(result, a.arg_int(i));
    return a.mkint(std::llabs(result));
}

// fn Lcm
// fn lcm
// fn Lcm
static minipy::value fnLcm(minipy::cffi& a){
    long long result = 1;
    for (size_t i = 0; i < a.argc(); ++i){
        long long value = std::llabs(a.arg_int(i));
        if (value == 0) return a.mkint(0);
        result = std::lcm(result, value);
    }
    return a.mkint(result);
}

// fn Isqrt
// fn isqrt
// fn Isqrt
static minipy::value fnIsqrt(minipy::cffi& a){
    long long n = a.arg_int(0);
    if (n < 0){
        a.vm->raiseError("ValueError", "isqrt() argument must be nonnegative");
        return a.none();
    }
    long long root = static_cast<long long>(std::sqrt(static_cast<double>(n)));
    while ((root + 1) * (root + 1) <= n) root++;
    while (root * root > n) root--;
    return a.mkint(root);
}

// fn Comb
// fn comb
// fn Comb
static minipy::value fnComb(minipy::cffi& a){
    long long n = a.arg_int(0);
    long long k = a.arg_int(1);
    if (n < 0 || k < 0){
        a.vm->raiseError("ValueError", "math.comb() requires non-negative arguments");
        return a.none();
    }
    if (k > n) return a.mkint(0);
    if (k > n - k) k = n - k;
    long long result = 1;
    for (long long i = 1; i <= k; ++i) result = result * (n - k + i) / i;
    return a.mkint(result);
}

// fn Perm
// fn perm
// fn Perm
static minipy::value fnPerm(minipy::cffi& a){
    long long n = a.arg_int(0);
    long long k = (a.argc() >= 2) ? a.arg_int(1) : n;
    if (n < 0 || k < 0){
        a.vm->raiseError("ValueError", "math.perm() requires non-negative arguments");
        return a.none();
    }
    if (k > n) return a.mkint(0);
    long long result = 1;
    for (long long i = 0; i < k; ++i) result *= (n - i);
    return a.mkint(result);
}

// fn Prod
// fn prod
// fn Prod
static minipy::value fnProd(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    std::vector<minipy::value> items;
    if (a.argc() > 0) vm.materializeIterable(a.arg(0), items);
    auto total = a.mkint(1);
    for (auto& item : items) total = vm.applyBinary("*", total, item);
    return total;
}

// fn Isfinite
// fn isfinite
// fn Isfinite
static minipy::value fnIsfinite(minipy::cffi& a){ return a.mkbool(std::isfinite(a.arg_float(0))); }

// fn Isinf
// fn isinf
// fn Isinf
static minipy::value fnIsinf(minipy::cffi& a){ return a.mkbool(std::isinf(a.arg_float(0))); }

// fn Isnan
// fn isnan
// fn Isnan
static minipy::value fnIsnan(minipy::cffi& a){ return a.mkbool(std::isnan(a.arg_float(0))); }



namespace {

// positional 或 Keyword
// positional or keyword
// positional ᠡᠰᠡᠪᠡᠯ Keyword
minipy::value positionalOrKeyword(minipy::cffi& a, size_t index, const char* name, const char* func, bool& ok){
    if (index < a.argc()){
        ok = true;
        return a.arg(index);
    }
    if (a.has_kwarg(name)){
        ok = true;
        return a.kwarg(name);
    }
    a.vm->raiseError("TypeError", std::string(func) + "() missing required argument '" + name + "'");
    ok = false;
    return a.none();
}

// compensated Sum
// compensated sum
// compensated Sum
double compensatedSum(const std::vector<double>& values, bool& hasNan, bool& posInf, bool& negInf){
    double sum = 0.0;
    double compensation = 0.0;
    for (double value : values){
        if (std::isnan(value)){
            hasNan = true;
            continue;
        }
        if (std::isinf(value)){
            if (value > 0.0) posInf = true;
            else negInf = true;
            continue;
        }
        double total = sum + value;
        if (std::fabs(sum) >= std::fabs(value)) compensation += (sum - total) + value;
        else compensation += (value - total) + sum;
        sum = total;
    }
    return sum + compensation;
}

bool nonFiniteSum(minipy::cffi& a, bool hasNan, bool posInf, bool negInf,
                  const std::string& message, minipy::value& result){
    if (hasNan){
        result = a.mkfloat(std::numeric_limits<double>::quiet_NaN());
        return true;
    }
    if (posInf && negInf){
        a.vm->raiseError("ValueError", message);
        result = a.none();
        return true;
    }
    if (posInf){
        result = a.mkfloat(std::numeric_limits<double>::infinity());
        return true;
    }
    if (negInf){
        result = a.mkfloat(-std::numeric_limits<double>::infinity());
        return true;
    }
    return false;
}
}

// fn Dist
// fn dist
// fn Dist
static minipy::value fnDist(minipy::cffi& a){
    bool ok = false;
    minipy::value p = positionalOrKeyword(a, 0, "p", "dist", ok);
    if (!ok) return a.none();
    minipy::value q = positionalOrKeyword(a, 1, "q", "dist", ok);
    if (!ok) return a.none();
    std::vector<minipy::value> pItems;
    std::vector<minipy::value> qItems;
    a.vm->materializeIterable(p, pItems);
    a.vm->materializeIterable(q, qItems);
    if (pItems.size() != qItems.size()){
        a.vm->raiseError("ValueError", "both points must have the same number of dimensions");
        return a.none();
    }
    double total = 0.0;
    for (size_t i = 0; i < pItems.size(); ++i){
        double diff = a.vm->toFloat(pItems[i]) - a.vm->toFloat(qItems[i]);
        total = std::hypot(total, diff);
    }
    return a.mkfloat(total);
}

// fn Fsum
// fn fsum
// fn Fsum
static minipy::value fnFsum(minipy::cffi& a){
    std::vector<minipy::value> items;
    a.vm->materializeIterable(a.arg(0), items);
    std::vector<double> values;
    for (auto& item : items) values.push_back(a.vm->toFloat(item));
    bool hasNan = false, posInf = false, negInf = false;
    double sum = compensatedSum(values, hasNan, posInf, negInf);
    minipy::value result;
    if (nonFiniteSum(a, hasNan, posInf, negInf, "-inf + inf in fsum", result)) return result;
    return a.mkfloat(sum);
}

// fn Sumprod
// fn sumprod
// fn Sumprod
static minipy::value fnSumprod(minipy::cffi& a){
    std::vector<minipy::value> left;
    std::vector<minipy::value> right;
    a.vm->materializeIterable(a.arg(0), left);
    a.vm->materializeIterable(a.arg(1), right);
    if (left.size() != right.size()){
        a.vm->raiseError("ValueError", "Inputs are not the same length");
        return a.none();
    }
    std::vector<double> products;
    for (size_t i = 0; i < left.size(); ++i){
        products.push_back(a.vm->toFloat(left[i]) * a.vm->toFloat(right[i]));
    }
    bool hasNan = false, posInf = false, negInf = false;
    double sum = compensatedSum(products, hasNan, posInf, negInf);
    minipy::value result;
    if (nonFiniteSum(a, hasNan, posInf, negInf, "-inf + inf in sumprod", result)) return result;
    return a.mkfloat(sum);
}

// fn Isclose
// fn isclose
// fn Isclose
static minipy::value fnIsclose(minipy::cffi& a){
    double x = a.arg_float(0);
    double y = a.arg_float(1);
    double relTol = a.has_kwarg("rel_tol") ? a.vm->toFloat(a.kwarg("rel_tol")) : 1e-09;
    double absTol = a.has_kwarg("abs_tol") ? a.vm->toFloat(a.kwarg("abs_tol")) : 0.0;
    if (relTol < 0.0 || absTol < 0.0){
        a.vm->raiseError("ValueError", "tolerances must be non-negative");
        return a.none();
    }
    if (x == y) return a.mkbool(true);
    if (std::isinf(x) || std::isinf(y)) return a.mkbool(false);
    double diff = std::fabs(x - y);
    double scale = std::fmax(std::fabs(x), std::fabs(y));
    return a.mkbool(diff <= std::fmax(relTol * scale, absTol));
}

// fn Nextafter
// fn nextafter
// fn Nextafter
static minipy::value fnNextafter(minipy::cffi& a){
    return a.mkfloat(std::nextafter(a.arg_float(0), a.arg_float(1)));
}

// fn Ulp
// fn ulp
// fn Ulp
static minipy::value fnUlp(minipy::cffi& a){
    double x = a.arg_float(0);
    if (std::isnan(x)) return a.mkfloat(x);
    if (std::isinf(x)) return a.mkfloat(std::fabs(x));
    if (x == 0.0) return a.mkfloat(std::numeric_limits<double>::denorm_min());
    x = std::fabs(x);
    return a.mkfloat(std::nextafter(x, std::numeric_limits<double>::infinity()) - x);
}

// fn Frexp
// fn frexp
// fn Frexp
static minipy::value fnFrexp(minipy::cffi& a){
    double x = a.arg_float(0);
    if (std::isinf(x) || std::isnan(x)){
        return a.vm->makeTuple({a.mkfloat(x), a.mkint(0)});
    }
    int exponent = 0;
    double mantissa = std::frexp(x, &exponent);
    return a.vm->makeTuple({a.mkfloat(mantissa), a.mkint(exponent)});
}

// fn Ldexp
// fn ldexp
// fn Ldexp
static minipy::value fnLdexp(minipy::cffi& a){
    double x = a.arg_float(0);
    long long raw = a.arg_int(1);
    int exponent = (raw > 100000) ? 100000 : (raw < -100000 ? -100000 : static_cast<int>(raw));
    double result = std::ldexp(x, exponent);
    if (std::isinf(result) && std::isfinite(x)){
        a.vm->raiseError("OverflowError", "math range error");
        return a.none();
    }
    return a.mkfloat(result);
}

// fn Modf
// fn modf
// fn Modf
static minipy::value fnModf(minipy::cffi& a){
    double x = a.arg_float(0);
    if (std::isnan(x)) return a.vm->makeTuple({a.mkfloat(x), a.mkfloat(x)});
    if (std::isinf(x)) return a.vm->makeTuple({a.mkfloat(std::copysign(0.0, x)), a.mkfloat(x)});
    double integerPart = 0.0;
    double fractionalPart = std::modf(x, &integerPart);
    return a.vm->makeTuple({a.mkfloat(fractionalPart), a.mkfloat(integerPart)});
}

// fn Erf
// fn erf
// fn Erf
static minipy::value fnErf(minipy::cffi& a){ return a.mkfloat(std::erf(a.arg_float(0))); }

// fn Erfc
// fn erfc
// fn Erfc
static minipy::value fnErfc(minipy::cffi& a){ return a.mkfloat(std::erfc(a.arg_float(0))); }

// fn Gamma
// fn gamma
// fn Gamma
static minipy::value fnGamma(minipy::cffi& a){
    double x = a.arg_float(0);
    if (std::isnan(x)) return a.mkfloat(x);
    if (x == std::numeric_limits<double>::infinity()) return a.mkfloat(x);
    if (std::isinf(x) || x == 0.0 || (x < 0.0 && std::floor(x) == x)){
        a.vm->raiseError("ValueError", "math domain error");
        return a.none();
    }
    double result = std::tgamma(x);
    if (std::isinf(result) || std::isnan(result)){
        a.vm->raiseError("OverflowError", "math range error");
        return a.none();
    }
    return a.mkfloat(result);
}

// fn Lgamma
// fn lgamma
// fn Lgamma
static minipy::value fnLgamma(minipy::cffi& a){
    double x = a.arg_float(0);
    if (std::isnan(x)) return a.mkfloat(x);
    if (x == std::numeric_limits<double>::infinity()) return a.mkfloat(x);
    if (std::isinf(x) || x == 0.0 || (x < 0.0 && std::floor(x) == x)){
        a.vm->raiseError("ValueError", "math domain error");
        return a.none();
    }
    double result = std::lgamma(x);
    if (std::isinf(result) && std::isfinite(x)){
        a.vm->raiseError("OverflowError", "math range error");
        return a.none();
    }
    return a.mkfloat(result);
}

// fn Cbrt
// fn cbrt
// fn Cbrt
static minipy::value fnCbrt(minipy::cffi& a){ return a.mkfloat(std::cbrt(a.arg_float(0))); }

// fn Exp2
// fn exp2
// fn Exp2
static minipy::value fnExp2(minipy::cffi& a){
    double x = a.arg_float(0);
    double result = std::exp2(x);
    if (std::isinf(result) && std::isfinite(x)){
        a.vm->raiseError("OverflowError", "math range error");
        return a.none();
    }
    return a.mkfloat(result);
}


// data Pi
// data pi
// data Pi
static minipy::value dataPi(minipy::cffi& a){ return a.mkfloat(3.14159265358979323846); }

// data E
// data e
// data E
static minipy::value dataE(minipy::cffi& a){ return a.mkfloat(2.71828182845904523536); }

// data Tau
// data tau
// data Tau
static minipy::value dataTau(minipy::cffi& a){ return a.mkfloat(6.28318530717958647692); }

// data Inf
// data inf
// data Inf
static minipy::value dataInf(minipy::cffi& a){ return a.mkfloat(std::numeric_limits<double>::infinity()); }

// data Nan
// data nan
// data Nan
static minipy::value dataNan(minipy::cffi& a){ return a.mkfloat(std::numeric_limits<double>::quiet_NaN()); }

// cffi math lib
// cffi math lib
// cffi math lib
minipy::ffilib cffi_math_lib(){
    return minipy::cffi_lib(
        "math",
        {
            {"sqrt", (void*)fnSqrt},
            {"pow", (void*)fnPow},
            {"fabs", (void*)fnFabs},
            {"floor", (void*)fnFloor},
            {"ceil", (void*)fnCeil},
            {"trunc", (void*)fnTrunc},
            {"sin", (void*)fnSin},
            {"cos", (void*)fnCos},
            {"tan", (void*)fnTan},
            {"asin", (void*)fnAsin},
            {"acos", (void*)fnAcos},
            {"atan", (void*)fnAtan},
            {"atan2", (void*)fnAtan2},
            {"sinh", (void*)fnSinh},
            {"cosh", (void*)fnCosh},
            {"tanh", (void*)fnTanh},
            {"asinh", (void*)fnAsinh},
            {"acosh", (void*)fnAcosh},
            {"atanh", (void*)fnAtanh},
            {"exp", (void*)fnExp},
            {"expm1", (void*)fnExpm1},
            {"log", (void*)fnLog},
            {"log10", (void*)fnLog10},
            {"log2", (void*)fnLog2},
            {"log1p", (void*)fnLog1p},
            {"fmod", (void*)fnFmod},
            {"hypot", (void*)fnHypot},
            {"copysign", (void*)fnCopysign},
            {"remainder", (void*)fnRemainder},
            {"degrees", (void*)fnDegrees},
            {"radians", (void*)fnRadians},
            {"factorial", (void*)fnFactorial},
            {"gcd", (void*)fnGcd},
            {"lcm", (void*)fnLcm},
            {"isqrt", (void*)fnIsqrt},
            {"comb", (void*)fnComb},
            {"perm", (void*)fnPerm},
            {"prod", (void*)fnProd},
            {"isfinite", (void*)fnIsfinite},
            {"isinf", (void*)fnIsinf},
            {"isnan", (void*)fnIsnan},
            {"dist", (void*)fnDist},
            {"fsum", (void*)fnFsum},
            {"isclose", (void*)fnIsclose},
            {"nextafter", (void*)fnNextafter},
            {"ulp", (void*)fnUlp},
            {"frexp", (void*)fnFrexp},
            {"ldexp", (void*)fnLdexp},
            {"modf", (void*)fnModf},
            {"erf", (void*)fnErf},
            {"erfc", (void*)fnErfc},
            {"gamma", (void*)fnGamma},
            {"lgamma", (void*)fnLgamma},
            {"cbrt", (void*)fnCbrt},
            {"exp2", (void*)fnExp2},
            {"sumprod", (void*)fnSumprod}
        },
        {
            {"pi", (void*)dataPi},
            {"e", (void*)dataE},
            {"tau", (void*)dataTau},
            {"inf", (void*)dataInf},
            {"nan", (void*)dataNan}
        });
}


}
