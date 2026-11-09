#include "../cffi.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace minipy {

namespace {

// Fraction 状态 (已约分, 分母为正)
// Fraction state (normalized, denominator positive)
// Fraction ᠪᠠᠶᠢᠳᠠᠯ
struct FracState{
    long long num = 0;
    long long den = 1;
};

long long gcdAbs(long long a, long long b){
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b){
        long long t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

void normalize(FracState& st){
    if (st.den < 0){
        st.num = -st.num;
        st.den = -st.den;
    }
    long long g = gcdAbs(st.num, st.den);
    st.num /= g;
    st.den /= g;
}

FracState* fracOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass || inst->klass->name != "Fraction") return nullptr;
    return static_cast<FracState*>(inst->handle);
}

const ffi_class* fracClass(cffi& a){
    if (!a.library) return nullptr;
    auto it = a.library->classes.find("Fraction");
    return it == a.library->classes.end() ? nullptr : &it->second;
}

value makeFrac(cffi& a, long long num, long long den){
    if (den == 0){
        a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(num) + ", 0)");
        return a.none();
    }
    auto* st = new FracState();
    st->num = num;
    st->den = den;
    normalize(*st);
    const ffi_class* klass = fracClass(a);
    value obj = a.vm->makeNativeObject(a.library, klass, st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["numerator"] = a.vm->makeInt(st->num);
        inst->attributes["denominator"] = a.vm->makeInt(st->den);
    }
    return obj;
}

// float → 精确分数 (as_integer_ratio 算法)
// float → exact ratio (as_integer_ratio algorithm)
// float → ᠦᠯᠢᠭᠡᠷ
bool floatToRatio(double x, long long& num, long long& den){
    if (!std::isfinite(x)) return false;
    if (x == 0.0){
        num = 0;
        den = 1;
        return true;
    }
    int exp = 0;
    double m = std::frexp(x, &exp); // x = m * 2^exp, m in [0.5, 1)
    long long mantissa = static_cast<long long>(m * 9007199254740992.0); // 2^53
    int shift = exp - 53;
    if (shift >= 0){
        num = mantissa << shift;
        den = 1;
    } else {
        num = mantissa;
        den = 1LL << (-shift);
    }
    return true;
}

// 解析分数/小数字符串 (CPython 语义: 精确十进制)
// Parse fraction/decimal string (CPython semantics: exact decimal)
// ᠲᠦᠰᠢᠭ ᠦᠰᠦᠭ ᠢ ᠰᠢᠨᠵᠢᠯᠡᠨ᠎ᠡ
bool parseFractionString(const std::string& raw, long long& num, long long& den,
                         std::string& errType, std::string& errMsg){
    std::string s = raw;
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    s = s.substr(a, b - a);

    size_t slash = s.find('/');
    if (slash != std::string::npos){
        std::string ln = s.substr(0, slash);
        std::string rn = s.substr(slash + 1);
        char* e1 = nullptr;
        char* e2 = nullptr;
        long long n1 = std::strtoll(ln.c_str(), &e1, 10);
        long long n2 = std::strtoll(rn.c_str(), &e2, 10);
        while (e1 && *e1 == ' ') e1++;
        while (e2 && *e2 == ' ') e2++;
        if (!e1 || *e1 != '\0' || !e2 || *e2 != '\0' || ln.empty() || rn.empty()){
            errType = "ValueError";
            errMsg = "Invalid literal for Fraction: '" + raw + "'";
            return false;
        }
        if (n2 == 0){
            errType = "ZeroDivisionError";
            errMsg = "Fraction(" + std::to_string(n1) + ", 0)";
            return false;
        }
        num = n1;
        den = n2;
        return true;
    }

    // 十进制/科学计数法精确解析
    // exact decimal / scientific parse
    // ᠠᠷᠪᠠᠨ ᠲᠣ᠎ ᠶᠢᠨ ᠨᠠᠷᠢᠨ ᠰᠢᠨᠵᠢᠯᠡᠯ
    size_t i = 0;
    bool neg = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')){
        neg = s[i] == '-';
        i++;
    }
    std::string intPart, fracPart, expPart;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) intPart += s[i++];
    if (i < s.size() && s[i] == '.'){
        i++;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) fracPart += s[i++];
    }
    bool hasExp = false;
    bool expNeg = false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')){
        hasExp = true;
        i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')){
            expNeg = s[i] == '-';
            i++;
        }
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) expPart += s[i++];
    }
    if (i != s.size() || (intPart.empty() && fracPart.empty()) ||
        (hasExp && expPart.empty())){
        errType = "ValueError";
        errMsg = "Invalid literal for Fraction: '" + raw + "'";
        return false;
    }
    std::string digits = intPart + fracPart;
    long long n = 0;
    for (char c : digits){
        n = n * 10 + (c - '0');
    }
    if (neg) n = -n;
    long long exponent = hasExp ? std::strtoll(expPart.c_str(), nullptr, 10) : 0;
    if (expNeg) exponent = -exponent;
    exponent -= static_cast<long long>(fracPart.size());
    num = n;
    den = 1;
    if (exponent >= 0){
        for (long long k = 0; k < exponent; ++k) num *= 10;
    } else {
        for (long long k = 0; k < -exponent; ++k) den *= 10;
    }
    return true;
}

// 从任意值构造分数
// Build fraction from arbitrary value
// ᠠliᠪᠠ value ᠠᠴᠠ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
bool valueToFrac(cffi& a, const value& v, FracState& out, std::string& errT, std::string& errM){
    if (v.type == "int" || v.type == "bool"){
        out.num = *static_cast<long long*>(v.ptr);
        out.den = 1;
        return true;
    }
    if (v.type == "float"){
        double x = *static_cast<double*>(v.ptr);
        if (!floatToRatio(x, out.num, out.den)){
            if (std::isnan(x)){
                errT = "ValueError";
                errM = "cannot convert NaN to integer ratio";
            } else {
                errT = "OverflowError";
                errM = "cannot convert Infinity to integer ratio";
            }
            return false;
        }
        return true;
    }
    if (auto* fs = fracOf(v)){
        out = *fs;
        return true;
    }
    if (v.type == "str" && v.ptr){
        return parseFractionString(*static_cast<std::string*>(v.ptr), out.num, out.den, errT, errM);
    }
    std::string tname = v.type == "none" ? "NoneType" : v.type;
    errT = "TypeError";
    errM = "conversion from " + tname + " to Fraction is not supported";
    return false;
}

// 算术运算
// arithmetic
// ᠲᠣ᠎ ᠶᠢᠨ ᠦᠢᠯᠡᠳᠬᠦ
value fracBinary(cffi& a, const std::string& op){
    auto* ls = fracOf(a.self());
    if (!ls) return a.none();
    value other = a.arg(0);
    if (other.type == "float"){
        // 与 float 混合 → float 结果
        // mixing with float → float result
        // float ᠲᠠᠢ ᠬᠣᠯᠢᠮᠠᠭ → float
        double lv = static_cast<double>(ls->num) / static_cast<double>(ls->den);
        double rv = *static_cast<double*>(other.ptr);
        if (op == "+") return a.mkfloat(lv + rv);
        if (op == "-") return a.mkfloat(lv - rv);
        if (op == "*") return a.mkfloat(lv * rv);
        if (op == "/"){
            if (rv == 0.0){
                a.vm->raiseError("ZeroDivisionError", "float division by zero");
                return a.none();
            }
            return a.mkfloat(lv / rv);
        }
        if (op == "//") return a.mkfloat(std::floor(lv / rv));
        if (op == "%") return a.mkfloat(lv - rv * std::floor(lv / rv));
        if (op == "**") return a.mkfloat(std::pow(lv, rv));
        a.vm->raiseError("TypeError", "unsupported operand type(s)");
        return a.none();
    }
    FracState rs;
    std::string errT, errM;
    if (!valueToFrac(a, other, rs, errT, errM)){
        a.vm->raiseError(errT, errM);
        return a.none();
    }
    if (op == "+") return makeFrac(a, ls->num * rs.den + rs.num * ls->den, ls->den * rs.den);
    if (op == "-") return makeFrac(a, ls->num * rs.den - rs.num * ls->den, ls->den * rs.den);
    if (op == "*") return makeFrac(a, ls->num * rs.num, ls->den * rs.den);
    if (op == "/"){
        if (rs.num == 0){
            a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(ls->num) + ", 0)");
            return a.none();
        }
        return makeFrac(a, ls->num * rs.den, ls->den * rs.num);
    }
    if (op == "//"){
        // floor(ad / bc)
        // floor(ad / bc)
        // floor(ad / bc)
        if (rs.num == 0){
            a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(ls->num) + ", 0)");
            return a.none();
        }
        long long n = ls->num * rs.den;
        long long d = ls->den * rs.num;
        long long q = n / d;
        if ((n % d != 0) && ((n < 0) != (d < 0))) q--;
        return a.mkint(q);
    }
    if (op == "%"){
        if (rs.num == 0){
            a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(ls->num) + ", 0)");
            return a.none();
        }
        long long n = ls->num * rs.den;
        long long d = ls->den * rs.num;
        long long q = n / d;
        if ((n % d != 0) && ((n < 0) != (d < 0))) q--;
        long long mn = ls->num * rs.den - q * rs.num * ls->den;
        long long md = ls->den * rs.den;
        return makeFrac(a, mn, md);
    }
    if (op == "**"){
        if (rs.den != 1){
            double lv = static_cast<double>(ls->num) / static_cast<double>(ls->den);
            double rv = static_cast<double>(rs.num) / static_cast<double>(rs.den);
            return a.mkfloat(std::pow(lv, rv));
        }
        long long e = rs.num;
        if (e >= 0){
            long long rn = 1, rd = 1;
            for (long long k = 0; k < e; ++k){
                rn *= ls->num;
                rd *= ls->den;
            }
            return makeFrac(a, rn, rd);
        }
        if (ls->num == 0){
            a.vm->raiseError("ZeroDivisionError", "Fraction(0, 0)");
            return a.none();
        }
        long long rn = 1, rd = 1;
        for (long long k = 0; k < -e; ++k){
            rn *= ls->num;
            rd *= ls->den;
        }
        return makeFrac(a, rd, rn);
    }
    a.vm->raiseError("TypeError", "unsupported operand type(s)");
    return a.none();
}

value fracCompare(cffi& a, const std::string& op){
    auto* ls = fracOf(a.self());
    if (!ls) return a.none();
    value other = a.arg(0);
    if (other.type == "float"){
        double lv = static_cast<double>(ls->num) / static_cast<double>(ls->den);
        double rv = *static_cast<double*>(other.ptr);
        if (op == "==") return a.mkbool(lv == rv);
        if (op == "!=") return a.mkbool(lv != rv);
        if (op == "<") return a.mkbool(lv < rv);
        if (op == "<=") return a.mkbool(lv <= rv);
        if (op == ">") return a.mkbool(lv > rv);
        if (op == ">=") return a.mkbool(lv >= rv);
    }
    FracState rs;
    std::string errT, errM;
    if (!valueToFrac(a, other, rs, errT, errM)){
        if (op == "==") return a.mkbool(false);
        if (op == "!=") return a.mkbool(true);
        a.vm->raiseError("TypeError", "'" + op + "' not supported between instances of 'Fraction' and '" +
                         (other.type == "none" ? "NoneType" : other.type) + "'");
        return a.none();
    }
    long long l = ls->num * rs.den;
    long long r = rs.num * ls->den;
    if (op == "==") return a.mkbool(l == r);
    if (op == "!=") return a.mkbool(l != r);
    if (op == "<") return a.mkbool(l < r);
    if (op == "<=") return a.mkbool(l <= r);
    if (op == ">") return a.mkbool(l > r);
    if (op == ">=") return a.mkbool(l >= r);
    return a.mkbool(false);
}

}

// Fraction 构造
// Fraction constructor
// Fraction ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnFracCtor(cffi& a){
    FracState st;
    if (a.argc() == 0){
        st.num = 0;
        st.den = 1;
    } else if (a.argc() == 1){
        std::string errT, errM;
        if (!valueToFrac(a, a.arg(0), st, errT, errM)){
            a.vm->raiseError(errT, errM);
            return a.none();
        }
    } else if (a.argc() == 2){
        value n = a.arg(0), d = a.arg(1);
        if ((n.type != "int" && n.type != "bool") || (d.type != "int" && d.type != "bool")){
            a.vm->raiseError("TypeError", "both arguments should be Rational");
            return a.none();
        }
        st.num = *static_cast<long long*>(n.ptr);
        st.den = *static_cast<long long*>(d.ptr);
        if (st.den == 0){
            a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(st.num) + ", 0)");
            return a.none();
        }
    } else {
        a.vm->raiseError("TypeError", "Fraction expected at most 2 arguments, got " +
                                      std::to_string(a.argc()));
        return a.none();
    }
    normalize(st);
    auto* heapSt = new FracState(st);
    value obj = a.mkobject(heapSt);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["numerator"] = a.vm->makeInt(st.num);
        inst->attributes["denominator"] = a.vm->makeInt(st.den);
    }
    return obj;
}

static value fnFracRepr(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.mkstr("Fraction(0, 1)");
    return a.mkstr("Fraction(" + std::to_string(st->num) + ", " + std::to_string(st->den) + ")");
}

static value fnFracStr(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.mkstr("0");
    if (st->den == 1) return a.mkstr(std::to_string(st->num));
    return a.mkstr(std::to_string(st->num) + "/" + std::to_string(st->den));
}

static value fnFracLimitDen(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.none();
    long long maxDen = a.argc() > 0 ? a.arg_int(0) : 1000000;
    if (maxDen < 1){
        a.vm->raiseError("ValueError", "max_denominator should be at least 1");
        return a.none();
    }
    if (st->den <= maxDen) return makeFrac(a, st->num, st->den);
    // 连分数算法 (CPython)
    // continued fraction algorithm (CPython)
    // ᠳᠠᠷᠠᠭᠠᠯᠠᠯ ᠲᠤᠰᠢᠭ ᠠᠷᠭᠠ
    long long p0 = 0, q0 = 1, p1 = 1, q1 = 0;
    long long n = st->num, d = st->den;
    while (d != 0){
        long long aQ = n / d;
        long long q2 = q0 + aQ * q1;
        if (q2 > maxDen) break;
        long long p2 = p0 + aQ * p1;
        p0 = p1; q0 = q1; p1 = p2; q1 = q2;
        long long r = n % d;
        n = d; d = r;
    }
    long long k = (maxDen - q0) / q1;
    long long bound1n = p0 + k * p1, bound1d = q0 + k * q1;
    long long bound2n = p1, bound2d = q1;
    // |f - b1| <= |f - b2| → b1, 否则 b2 (CPython: 2*|f-b1| <= |f-b2| 时取 b1)
    // |f - b1| <= |f - b2| → b1 else b2
    // |f - b1| ᠪᠠ |f - b2| ᠬᠠᠪᠢᠴᠢᠨ᠎ᠠ
    long long f1n = st->num * bound1d - bound1n * st->den; // 与 den*b1d 同号
    long long f2n = bound2n * st->den - st->num * bound2d;
    if (f1n < 0) f1n = -f1n;
    if (f2n < 0) f2n = -f2n;
    // 比较 f1n/(den*b1d) 与 f2n/(den*b2d)
    // compare f1n/(den*b1d) vs f2n/(den*b2d)
    // f1n/(den*b1d) ᠪᠠ f2n/(den*b2d) ᠬᠠᠪᠢᠴᠢᠨ᠎ᠠ
    long long lhs = f1n * bound2d;
    long long rhs = f2n * bound1d;
    if (lhs <= rhs)
        return makeFrac(a, bound1n, bound1d);
    return makeFrac(a, bound2n, bound2d);
}

static value fnFracAsRatio(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.vm->makeTuple({a.mkint(0), a.mkint(1)});
    return a.vm->makeTuple({a.mkint(st->num), a.mkint(st->den)});
}

static value fnFracFromFloat(cffi& a){
    // 类级调用: self 是类, arg(0) 是 float
    // class-level call: self is the class, arg(0) is float
    // ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠳᠡᠭᠡᠷᠡ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
    value target = a.self();
    if (target.type == "native_object" && target.ptr){
        auto* inst = static_cast<ffi_instance*>(target.ptr);
        if (inst->handle == nullptr && a.argc() > 0) target = a.arg(0);
    }
    double x = a.vm->toFloat(target);
    long long n, d;
    if (!floatToRatio(x, n, d)){
        if (std::isnan(x)){
            a.vm->raiseError("ValueError", "cannot convert NaN to integer ratio");
        } else {
            a.vm->raiseError("OverflowError", "cannot convert Infinity to integer ratio");
        }
        return a.none();
    }
    return makeFrac(a, n, d);
}

static value fnFracFloat(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.mkfloat(0.0);
    return a.mkfloat(static_cast<double>(st->num) / static_cast<double>(st->den));
}

static value fnFracInt(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.mkint(0);
    return a.mkint(st->num / st->den);
}

static value fnFracBool(cffi& a){
    auto* st = fracOf(a.self());
    return a.mkbool(st && st->num != 0);
}

static value fnFracNeg(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.none();
    return makeFrac(a, -st->num, st->den);
}

static value fnFracPos(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.none();
    return makeFrac(a, st->num, st->den);
}

static value fnFracAbs(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.none();
    return makeFrac(a, st->num < 0 ? -st->num : st->num, st->den);
}

static value fnFracAdd(cffi& a){ return fracBinary(a, "+"); }
static value fnFracSub(cffi& a){ return fracBinary(a, "-"); }
static value fnFracMul(cffi& a){ return fracBinary(a, "*"); }
static value fnFracDiv(cffi& a){ return fracBinary(a, "/"); }
static value fnFracFloorDiv(cffi& a){ return fracBinary(a, "//"); }
static value fnFracMod(cffi& a){
    auto* ls = fracOf(a.self());
    if (!ls) return a.none();
    FracState rs;
    std::string errT, errM;
    if (!valueToFrac(a, a.arg(0), rs, errT, errM)){
        a.vm->raiseError(errT, errM);
        return a.none();
    }
    if (rs.num == 0){
        a.vm->raiseError("ZeroDivisionError", "Fraction(" + std::to_string(ls->num) + ", 0)");
        return a.none();
    }
    long long n = ls->num * rs.den;
    long long d = ls->den * rs.num;
    long long q = n / d;
    if ((n % d != 0) && ((n < 0) != (d < 0))) q--;
    // a - q*b
    // a - q*b
    // a - q*b
    long long mn = ls->num * rs.den - q * rs.num * ls->den;
    long long md = ls->den * rs.den;
    return makeFrac(a, mn, md);
}
static value fnFracPow(cffi& a){ return fracBinary(a, "**"); }

static value fnFracRadd(cffi& a){ return fracBinary(a, "+"); }
static value fnFracRsub(cffi& a){
    auto* ls = fracOf(a.self());
    if (!ls) return a.none();
    FracState rs;
    std::string errT, errM;
    if (!valueToFrac(a, a.arg(0), rs, errT, errM)){
        a.vm->raiseError(errT, errM);
        return a.none();
    }
    return makeFrac(a, rs.num * ls->den - ls->num * rs.den, ls->den * rs.den);
}
static value fnFracRmul(cffi& a){ return fracBinary(a, "*"); }
static value fnFracRdiv(cffi& a){
    auto* ls = fracOf(a.self());
    if (!ls) return a.none();
    if (ls->num == 0){
        a.vm->raiseError("ZeroDivisionError", "Fraction(0, 1)");
        return a.none();
    }
    FracState rs;
    std::string errT, errM;
    if (!valueToFrac(a, a.arg(0), rs, errT, errM)){
        a.vm->raiseError(errT, errM);
        return a.none();
    }
    return makeFrac(a, rs.num * ls->den, rs.den * ls->num);
}

static value fnFracEq(cffi& a){ return fracCompare(a, "=="); }
static value fnFracNe(cffi& a){ return fracCompare(a, "!="); }
static value fnFracLt(cffi& a){ return fracCompare(a, "<"); }
static value fnFracLe(cffi& a){ return fracCompare(a, "<="); }
static value fnFracGt(cffi& a){ return fracCompare(a, ">"); }
static value fnFracGe(cffi& a){ return fracCompare(a, ">="); }

static value fnFracHash(cffi& a){
    auto* st = fracOf(a.self());
    if (!st) return a.mkint(0);
    double f = static_cast<double>(st->num) / static_cast<double>(st->den);
    return a.mkint(a.vm->hashKey(a.vm->makeFloat(f)));
}

// cffi fractions 库
// cffi fractions library
// cffi fractions ᠨᠣᠮ
ffilib cffi_fractions_lib(){
    ffi_class fracCls;
    fracCls.name = "Fraction";
    fracCls.constructor = (void*)fnFracCtor;
    fracCls.methods = {
        {"__repr__", (void*)fnFracRepr},
        {"__str__", (void*)fnFracStr},
        {"limit_denominator", (void*)fnFracLimitDen},
        {"as_integer_ratio", (void*)fnFracAsRatio},
        {"from_float", (void*)fnFracFromFloat},
        {"__float__", (void*)fnFracFloat},
        {"__int__", (void*)fnFracInt},
        {"__bool__", (void*)fnFracBool},
        {"__hash__", (void*)fnFracHash},
        {"__neg__", (void*)fnFracNeg},
        {"__pos__", (void*)fnFracPos},
        {"__abs__", (void*)fnFracAbs},
        {"__add__", (void*)fnFracAdd},
        {"__sub__", (void*)fnFracSub},
        {"__mul__", (void*)fnFracMul},
        {"__truediv__", (void*)fnFracDiv},
        {"__floordiv__", (void*)fnFracFloorDiv},
        {"__mod__", (void*)fnFracMod},
        {"__pow__", (void*)fnFracPow},
        {"__radd__", (void*)fnFracRadd},
        {"__rsub__", (void*)fnFracRsub},
        {"__rmul__", (void*)fnFracRmul},
        {"__rtruediv__", (void*)fnFracRdiv},
        {"__eq__", (void*)fnFracEq},
        {"__ne__", (void*)fnFracNe},
        {"__lt__", (void*)fnFracLt},
        {"__le__", (void*)fnFracLe},
        {"__gt__", (void*)fnFracGt},
        {"__ge__", (void*)fnFracGe}
    };
    return cffi_lib(
        "fractions",
        {},
        {},
        {
            {"Fraction", fracCls}
        });
}

}
