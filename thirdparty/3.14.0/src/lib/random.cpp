#include "../cffi.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace minipy {




using namespace minipy;

namespace {
std::mt19937_64 engine(
    static_cast<unsigned long long>(std::chrono::high_resolution_clock::now()
                                        .time_since_epoch().count()));

// as Sequence
// as sequence
// as Sequence
std::vector<value> asSequence(cffi& a, size_t index){
    std::vector<value> items;
    if (index >= a.argc()) return items;
    a.vm->materializeIterable(a.arg(index), items);
    return items;
}
const double kPi = 3.14159265358979323846264338327950288;
const double kTwoPi = 6.28318530717958647692528676655900577;
const double kE = 2.71828182845904523536028747135266250;
const double kLog4 = std::log(4.0);
const double kSgMagicConst = 1.0 + std::log(4.5);
const double kNVMagicConst = 4.0 * std::exp(-0.5) / std::sqrt(2.0);

bool gaussNextValid = false;
double gaussNextValue = 0.0;

// random Double
// random double
// random Double
double randomDouble(){
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(engine);
}

// normal 值
// normal value
// normal ᠭᠠᠷᠴᠠᠭ
double normalValue(){
    while (true){
        double u1 = randomDouble();
        double u2 = 1.0 - randomDouble();
        double z = kNVMagicConst * (u1 - 0.5) / u2;
        double zz = z * z / 4.0;
        if (zz <= -std::log(u2)) return z;
    }
}

// gamma 值
// gamma value
// gamma ᠭᠠᠷᠴᠠᠭ
double gammaValue(double alpha, double beta){
    if (alpha > 1.0){
        double ainv = std::sqrt(2.0 * alpha - 1.0);
        double bbb = alpha - kLog4;
        double ccc = alpha + ainv;
        while (true){
            double u1 = randomDouble();
            if (!(u1 > 1e-7 && u1 < 0.9999999)) continue;
            double u2 = 1.0 - randomDouble();
            double v = std::log(u1 / (1.0 - u1)) / ainv;
            double x = alpha * std::exp(v);
            double z = u1 * u1 * u2;
            double r = bbb + ccc * v - x;
            if (r + kSgMagicConst - 4.5 * z >= 0.0 || r >= std::log(z)){
                return x * beta;
            }
        }
    }
    if (alpha == 1.0){
        double u = randomDouble();
        while (u <= 1e-7) u = randomDouble();
        return -std::log(u) * beta;
    }
    double x = 0.0;
    while (true){
        double u = randomDouble();
        double b = (kE + alpha) / kE;
        double p = b * u;
        if (p <= 1.0) x = std::pow(p, 1.0 / alpha);
        else x = -std::log((b - p) / alpha);
        double u1 = randomDouble();
        if (p > 1.0){
            if (u1 <= std::pow(x, alpha - 1.0)) break;
        } else if (u1 <= std::exp(-x)){
            break;
        }
    }
    return x * beta;
}

// mod Two Pi
// mod two pi
// mod Two Pi
double modTwoPi(double value){
    double result = std::fmod(value, kTwoPi);
    return result < 0.0 ? result + kTwoPi : result;
}
}

// fn Seed
// fn seed
// fn Seed
static value fnSeed(cffi& a){
    long long value = (a.argc() > 0) ? a.arg_int(0)
                                     : static_cast<long long>(
                                           std::chrono::high_resolution_clock::now()
                                               .time_since_epoch().count());
    engine.seed(static_cast<unsigned long long>(value));
    gaussNextValid = false;
    return a.none();
}

// fn Random
// fn random
// fn Random
static value fnRandom(cffi& a){
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return a.mkfloat(dist(engine));
}

// fn Uniform
// fn uniform
// fn Uniform
static value fnUniform(cffi& a){
    double low = a.arg_float(0);
    double high = a.arg_float(1);
    std::uniform_real_distribution<double> dist(low, high);
    return a.mkfloat(dist(engine));
}

// fn Randint
// fn randint
// fn Randint
static value fnRandint(cffi& a){
    long long low = a.arg_int(0);
    long long high = a.arg_int(1);
    if (low > high){
        a.vm->raiseError("ValueError", "empty range for randrange()");
        return a.none();
    }
    std::uniform_int_distribution<long long> dist(low, high);
    return a.mkint(dist(engine));
}

// fn Randrange
// fn randrange
// fn Randrange
static value fnRandrange(cffi& a){
    long long start = 0;
    long long stop = 0;
    long long step = 1;
    if (a.argc() == 1){
        stop = a.arg_int(0);
    } else {
        start = a.arg_int(0);
        stop = a.arg_int(1);
        if (a.argc() >= 3) step = a.arg_int(2);
    }
    if (step == 0){
        a.vm->raiseError("ValueError", "zero step for randrange()");
        return a.none();
    }
    long long count = 0;
    if (step > 0){
        if (start >= stop){
            a.vm->raiseError("ValueError", "empty range for randrange()");
            return a.none();
        }
        count = (stop - start + step - 1) / step;
    } else {
        if (start <= stop){
            a.vm->raiseError("ValueError", "empty range for randrange()");
            return a.none();
        }
        count = (start - stop - step - 1) / (-step);
    }
    std::uniform_int_distribution<long long> dist(0, count - 1);
    return a.mkint(start + dist(engine) * step);
}

// fn Choice
// fn choice
// fn Choice
static value fnChoice(cffi& a){
    std::vector<value> items = asSequence(a, 0);
    if (items.empty()){
        a.vm->raiseError("IndexError", "Cannot choose from an empty sequence");
        return a.none();
    }
    std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
    return items[dist(engine)];
}

// fn Choices
// fn choices
// fn Choices
static value fnChoices(cffi& a){
    std::vector<value> items = asSequence(a, 0);
    long long k = (a.argc() >= 2) ? a.arg_int(1) : 1;
    if (items.empty()){
        a.vm->raiseError("IndexError", "Cannot choose from an empty sequence");
        return a.none();
    }
    std::vector<value> result;
    std::uniform_int_distribution<size_t> dist(0, items.size() - 1);
    for (long long i = 0; i < k; ++i) result.push_back(items[dist(engine)]);
    return a.mklist(result);
}

// fn Shuffle
// fn shuffle
// fn Shuffle
static value fnShuffle(cffi& a){
    value val = a.arg(0);
    if (val.type != "list" || val.ptr == nullptr){
        a.vm->raiseError("TypeError", "object of type '" +
                                          a.vm->toString(a.mkstr(val.type)) +
                                          "' has no len()");
        return a.none();
    }
    auto* list = static_cast<std::vector<value>*>(val.ptr);
    std::shuffle(list->begin(), list->end(), engine);
    return a.none();
}

// fn Sample
// fn sample
// fn Sample
static value fnSample(cffi& a){
    std::vector<value> items = asSequence(a, 0);
    long long k = a.arg_int(1);
    if (k < 0 || static_cast<size_t>(k) > items.size()){
        a.vm->raiseError("ValueError", "Sample larger than population or is negative");
        return a.none();
    }
    std::shuffle(items.begin(), items.end(), engine);
    items.resize(static_cast<size_t>(k));
    return a.mklist(items);
}

// fn Getrandbits
// fn getrandbits
// fn Getrandbits
static value fnGetrandbits(cffi& a){
    long long k = a.arg_int(0);
    if (k < 0){
        a.vm->raiseError("ValueError", "number of bits must be non-negative");
        return a.none();
    }
    unsigned long long mask = (k >= 64) ? ~0ULL : ((1ULL << k) - 1);
    return a.mkint(static_cast<long long>(engine() & mask));
}

// fn Randbytes
// fn randbytes
// fn Randbytes
static value fnRandbytes(cffi& a){
    long long count = a.arg_int(0);
    if (count < 0){
        a.vm->raiseError("ValueError", "number of bytes must be non-negative");
        return a.none();
    }
    std::uniform_int_distribution<int> dist(0, 255);
    std::string result;
    for (long long i = 0; i < count; ++i) result += static_cast<char>(dist(engine));
    return a.mkstr(result);
}

// fn Gauss
// fn gauss
// fn Gauss
static value fnGauss(cffi& a){
    double mu = (a.argc() > 0) ? a.arg_float(0) : 0.0;
    double sigma = (a.argc() > 1) ? a.arg_float(1) : 1.0;
    double z;
    if (gaussNextValid){
        z = gaussNextValue;
        gaussNextValid = false;
    } else {
        double x2pi = randomDouble() * kTwoPi;
        double g2rad = std::sqrt(-2.0 * std::log(1.0 - randomDouble()));
        z = std::cos(x2pi) * g2rad;
        gaussNextValue = std::sin(x2pi) * g2rad;
        gaussNextValid = true;
    }
    return a.mkfloat(mu + z * sigma);
}

// fn Normalvariate
// fn normalvariate
// fn Normalvariate
static value fnNormalvariate(cffi& a){
    double mu = (a.argc() > 0) ? a.arg_float(0) : 0.0;
    double sigma = (a.argc() > 1) ? a.arg_float(1) : 1.0;
    return a.mkfloat(mu + normalValue() * sigma);
}

// fn Lognormvariate
// fn lognormvariate
// fn Lognormvariate
static value fnLognormvariate(cffi& a){
    double mu = (a.argc() > 0) ? a.arg_float(0) : 0.0;
    double sigma = (a.argc() > 1) ? a.arg_float(1) : 1.0;
    return a.mkfloat(std::exp(mu + normalValue() * sigma));
}

// fn Expovariate
// fn expovariate
// fn Expovariate
static value fnExpovariate(cffi& a){
    double lambda = a.arg_float(0);
    if (lambda == 0.0){
        a.vm->raiseError("ZeroDivisionError", "float division by zero");
        return a.none();
    }
    double u = 1.0 - randomDouble();
    return a.mkfloat(-std::log(u) / lambda);
}

// fn Betavariate
// fn betavariate
// fn Betavariate
static value fnBetavariate(cffi& a){
    double alpha = a.arg_float(0);
    double beta = a.arg_float(1);
    if (alpha <= 0.0 || beta <= 0.0){
        a.vm->raiseError("ValueError", "gammavariate: alpha and beta must be > 0.0");
        return a.none();
    }
    double y = gammaValue(alpha, 1.0);
    if (y == 0.0) return a.mkfloat(0.0);
    return a.mkfloat(y / (y + gammaValue(beta, 1.0)));
}

// fn Gammavariate
// fn gammavariate
// fn Gammavariate
static value fnGammavariate(cffi& a){
    double alpha = a.arg_float(0);
    double beta = a.arg_float(1);
    if (alpha <= 0.0 || beta <= 0.0){
        a.vm->raiseError("ValueError", "gammavariate: alpha and beta must be > 0.0");
        return a.none();
    }
    return a.mkfloat(gammaValue(alpha, beta));
}

// fn Triangular
// fn triangular
// fn Triangular
static value fnTriangular(cffi& a){
    double low = (a.argc() > 0) ? a.arg_float(0) : 0.0;
    double high = (a.argc() > 1) ? a.arg_float(1) : 1.0;
    bool hasMode = (a.argc() > 2) && a.arg(2).type != "none";
    double u = randomDouble();
    double c = 0.5;
    if (hasMode){
        double mode = a.arg_float(2);
        if (mode != low && mode != high) c = (mode - low) / (high - low);
    }
    if (u > c){
        u = 1.0 - u;
        c = 1.0 - c;
        std::swap(low, high);
    }
    return a.mkfloat(low + (high - low) * std::sqrt(u * c));
}

// fn Vonmisesvariate
// fn vonmisesvariate
// fn Vonmisesvariate
static value fnVonmisesvariate(cffi& a){
    double mu = a.arg_float(0);
    double kappa = a.arg_float(1);
    if (kappa <= 1e-6) return a.mkfloat(kTwoPi * randomDouble());
    double alpha = 1.0 + std::sqrt(1.0 + 4.0 * kappa * kappa);
    double b = (alpha - std::sqrt(2.0 * alpha)) / (2.0 * kappa);
    double r = (1.0 + b * b) / (2.0 * b);
    double f = 0.0;
    while (true){
        double u1 = randomDouble();
        double z = std::cos(kPi * u1);
        f = (1.0 + r * z) / (r + z);
        double c = kappa * (r - f);
        double u2 = randomDouble();
        if (u2 < c * (2.0 - c) || u2 <= c * std::exp(1.0 - c)) break;
    }
    double u3 = randomDouble();
    if (u3 > 0.5) return a.mkfloat(modTwoPi(mu) + std::acos(f));
    return a.mkfloat(modTwoPi(mu) - std::acos(f));
}

// fn Paretovariate
// fn paretovariate
// fn Paretovariate
static value fnParetovariate(cffi& a){
    double alpha = a.arg_float(0);
    double u = 1.0 - randomDouble();
    return a.mkfloat(1.0 / std::pow(u, 1.0 / alpha));
}

// fn Weibullvariate
// fn weibullvariate
// fn Weibullvariate
static value fnWeibullvariate(cffi& a){
    double alpha = a.arg_float(0);
    double beta = a.arg_float(1);
    double u = 1.0 - randomDouble();
    return a.mkfloat(alpha * std::pow(-std::log(u), 1.0 / beta));
}

// cffi random lib
// cffi random lib
// cffi random lib
ffilib cffi_random_lib(){
    return cffi_lib(
        "random",
        {
            {"seed", (void*)fnSeed},
            {"random", (void*)fnRandom},
            {"uniform", (void*)fnUniform},
            {"randint", (void*)fnRandint},
            {"randrange", (void*)fnRandrange},
            {"choice", (void*)fnChoice},
            {"choices", (void*)fnChoices},
            {"shuffle", (void*)fnShuffle},
            {"sample", (void*)fnSample},
            {"getrandbits", (void*)fnGetrandbits},
            {"randbytes", (void*)fnRandbytes},
            {"gauss", (void*)fnGauss},
            {"normalvariate", (void*)fnNormalvariate},
            {"lognormvariate", (void*)fnLognormvariate},
            {"expovariate", (void*)fnExpovariate},
            {"betavariate", (void*)fnBetavariate},
            {"gammavariate", (void*)fnGammavariate},
            {"triangular", (void*)fnTriangular},
            {"vonmisesvariate", (void*)fnVonmisesvariate},
            {"paretovariate", (void*)fnParetovariate},
            {"weibullvariate", (void*)fnWeibullvariate}
        });
}


}
