#include "../cffi.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace minipy {




using namespace minipy;
namespace {

// as Numbers
// as numbers
// as Numbers
std::vector<double> asNumbers(cffi& a, size_t index){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(index), items);
    std::vector<double> numbers;
    for (auto& item : items) numbers.push_back(a.vm->toFloat(item));
    return numbers;
}

// require Data
// require data
// require Data
bool requireData(cffi& a, const std::vector<double>& numbers){
    if (numbers.empty()){
        a.vm->raiseError("statistics.StatisticsError", "no data");
        return false;
    }
    return true;
}

// mean Of
// mean of
// mean Of
double meanOf(const std::vector<double>& numbers){
    double total = 0.0;
    for (double value : numbers) total += value;
    return total / static_cast<double>(numbers.size());
}

// variance Of
// variance of
// variance Of
double varianceOf(const std::vector<double>& numbers, bool sample){
    size_t n = numbers.size();
    if (n < (sample ? 2u : 1u)) return 0.0;
    double mean = meanOf(numbers);
    double total = 0.0;
    for (double value : numbers) total += (value - mean) * (value - mean);
    return total / static_cast<double>(sample ? n - 1 : n);
}
}

// fn Mean
// fn mean
// fn Mean
static value fnMean(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    return a.mkfloat(meanOf(numbers));
}

// fn Median
// fn median
// fn Median
static value fnMedian(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    std::sort(numbers.begin(), numbers.end());
    size_t n = numbers.size();
    if (n % 2 == 1) return a.mkfloat(numbers[n / 2]);
    return a.mkfloat((numbers[n / 2 - 1] + numbers[n / 2]) / 2.0);
}

// fn Median Low
// fn median low
// fn Median Low
static value fnMedianLow(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    std::sort(numbers.begin(), numbers.end());
    return a.mkfloat(numbers[(numbers.size() - 1) / 2]);
}

// fn Median High
// fn median high
// fn Median High
static value fnMedianHigh(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    std::sort(numbers.begin(), numbers.end());
    return a.mkfloat(numbers[numbers.size() / 2]);
}

// fn Mode
// fn mode
// fn Mode
static value fnMode(cffi& a){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (items.empty()){
        a.vm->raiseError("statistics.StatisticsError", "no mode for empty data");
        return a.none();
    }
    value best = items[0];
    size_t bestCount = 0;
    for (auto& candidate : items){
        size_t count = 0;
        for (auto& item : items){
            if (a.vm->valuesEqual(candidate, item)) count++;
        }
        if (count > bestCount){
            bestCount = count;
            best = candidate;
        }
    }
    return best;
}

// fn Multimode
// fn multimode
// fn Multimode
static value fnMultimode(cffi& a){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (items.empty()){
        a.vm->raiseError("statistics.StatisticsError", "no mode for empty data");
        return a.none();
    }
    size_t bestCount = 0;
    std::vector<value> result;
    for (auto& candidate : items){
        size_t count = 0;
        for (auto& item : items){
            if (a.vm->valuesEqual(candidate, item)) count++;
        }
        bool exists = false;
        for (auto& item : result) if (a.vm->valuesEqual(item, candidate)) exists = true;
        if (count > bestCount){
            bestCount = count;
            result.clear();
            result.push_back(candidate);
        } else if (count == bestCount && !exists){
            result.push_back(candidate);
        }
    }
    return a.mklist(result);
}

// fn Pstdev
// fn pstdev
// fn Pstdev
static value fnPstdev(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    return a.mkfloat(std::sqrt(varianceOf(numbers, false)));
}

// fn Stdev
// fn stdev
// fn Stdev
static value fnStdev(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (numbers.size() < 2){
        a.vm->raiseError("statistics.StatisticsError", "stdev requires at least two data points");
        return a.none();
    }
    return a.mkfloat(std::sqrt(varianceOf(numbers, true)));
}

// fn Pvariance
// fn pvariance
// fn Pvariance
static value fnPvariance(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    return a.mkfloat(varianceOf(numbers, false));
}

// fn Variance
// fn variance
// fn Variance
static value fnVariance(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (numbers.size() < 2){
        a.vm->raiseError("statistics.StatisticsError", "variance requires at least two data points");
        return a.none();
    }
    return a.mkfloat(varianceOf(numbers, true));
}

// fn Harmonic Mean
// fn harmonic mean
// fn Harmonic Mean
static value fnHarmonicMean(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    double total = 0.0;
    for (double value : numbers){
        if (value <= 0.0){
            a.vm->raiseError("statistics.StatisticsError", "harmonic mean does not support negative values");
            return a.none();
        }
        total += 1.0 / value;
    }
    return a.mkfloat(static_cast<double>(numbers.size()) / total);
}

// fn Geometric Mean
// fn geometric mean
// fn Geometric Mean
static value fnGeometricMean(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    double total = 0.0;
    for (double value : numbers){
        if (value <= 0.0){
            a.vm->raiseError("statistics.StatisticsError", "geometric mean does not support negative values");
            return a.none();
        }
        total += std::log(value);
    }
    return a.mkfloat(std::exp(total / static_cast<double>(numbers.size())));
}



static bool requirePairData(cffi& a, const std::vector<double>& xs, const std::vector<double>& ys,
                            const std::string& func){
    if (xs.size() != ys.size()){
        a.vm->raiseError("statistics.StatisticsError", "x and y must have the same length");
        return false;
    }
    if (xs.size() < 2){
        a.vm->raiseError("statistics.StatisticsError", func + " requires at least two data points");
        return false;
    }
    return true;
}

// fn Quantiles
// fn quantiles
// fn Quantiles
static value fnQuantiles(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (numbers.size() < 2){
        a.vm->raiseError("statistics.StatisticsError", "must have at least two data points");
        return a.none();
    }
    long long n = a.kwarg_int("n", 4);
    if (n < 1){
        a.vm->raiseError("statistics.StatisticsError", "n must be at least 1");
        return a.none();
    }
    std::string method = a.has_kwarg("method") ? a.vm->toString(a.kwarg("method")) : "exclusive";
    std::sort(numbers.begin(), numbers.end());
    long long length = static_cast<long long>(numbers.size());
    std::vector<value> result;
    if (method == "inclusive"){
        long long m = length - 1;
        for (long long i = 1; i < n; ++i){
            long long j = i * m / n;
            long long delta = (i * m) % n;
            double value = (numbers[j] * static_cast<double>(n - delta) +
                            numbers[j + 1] * static_cast<double>(delta)) / static_cast<double>(n);
            result.push_back(a.mkfloat(value));
        }
    } else if (method == "exclusive"){
        long long m = length + 1;
        for (long long i = 1; i < n; ++i){
            long long j = i * m / n;
            long long delta = i * m - j * n;
            long long lowerIndex = (j - 1 < 0) ? j - 1 + length : j - 1;
            if (j >= length){
                a.vm->raiseError("IndexError", "list index out of range");
                return a.none();
            }
            double value = (numbers[lowerIndex] * static_cast<double>(n - delta) +
                            numbers[j] * static_cast<double>(delta)) / static_cast<double>(n);
            result.push_back(a.mkfloat(value));
        }
    } else {
        a.vm->raiseError("statistics.StatisticsError", "Unknown method " + method);
        return a.none();
    }
    return a.mklist(result);
}

// fn Covariance
// fn covariance
// fn Covariance
static value fnCovariance(cffi& a){
    std::vector<double> xs = asNumbers(a, 0);
    std::vector<double> ys = asNumbers(a, 1);
    if (!requirePairData(a, xs, ys, "covariance")) return a.none();
    double xbar = meanOf(xs);
    double ybar = meanOf(ys);
    double total = 0.0;
    for (size_t i = 0; i < xs.size(); ++i) total += (xs[i] - xbar) * (ys[i] - ybar);
    return a.mkfloat(total / static_cast<double>(xs.size() - 1));
}

// fn Correlation
// fn correlation
// fn Correlation
static value fnCorrelation(cffi& a){
    std::vector<double> xs = asNumbers(a, 0);
    std::vector<double> ys = asNumbers(a, 1);
    if (!requirePairData(a, xs, ys, "correlation")) return a.none();
    double xbar = meanOf(xs);
    double ybar = meanOf(ys);
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (size_t i = 0; i < xs.size(); ++i){
        double dx = xs[i] - xbar;
        double dy = ys[i] - ybar;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    double denominator = std::sqrt(sxx * syy);
    if (denominator == 0.0){
        a.vm->raiseError("statistics.StatisticsError", "at least one of the inputs is constant");
        return a.none();
    }
    return a.mkfloat(sxy / denominator);
}

// fn Linear Regression
// fn linear regression
// fn Linear Regression
static value fnLinearRegression(cffi& a){
    std::vector<double> xs = asNumbers(a, 0);
    std::vector<double> ys = asNumbers(a, 1);
    if (!requirePairData(a, xs, ys, "linear regression")) return a.none();
    double xbar = meanOf(xs);
    double ybar = meanOf(ys);
    double sxx = 0.0, sxy = 0.0;
    for (size_t i = 0; i < xs.size(); ++i){
        double dx = xs[i] - xbar;
        sxx += dx * dx;
        sxy += dx * (ys[i] - ybar);
    }
    if (sxx == 0.0){
        a.vm->raiseError("statistics.StatisticsError", "the input x is constant");
        return a.none();
    }
    double slope = sxy / sxx;
    double intercept = ybar - slope * xbar;
    return a.vm->makeTuple({a.mkfloat(slope), a.mkfloat(intercept)});
}

// fn Median Grouped
// fn median grouped
// fn Median Grouped
static value fnMedianGrouped(cffi& a){
    std::vector<double> numbers = asNumbers(a, 0);
    if (!requireData(a, numbers)) return a.none();
    if (numbers.size() == 1) return a.mkfloat(numbers[0]);
    double interval = (a.argc() >= 2) ? a.arg_float(1)
                     : (a.has_kwarg("interval") ? a.vm->toFloat(a.kwarg("interval")) : 1.0);
    std::sort(numbers.begin(), numbers.end());
    size_t n = numbers.size();
    double x = numbers[n / 2];
    size_t frequency = 0;
    size_t below = 0;
    for (double value : numbers){
        if (value == x) frequency++;
        else if (value < x) below++;
    }
    double lower = x - interval / 2.0;
    return a.mkfloat(lower + interval * (static_cast<double>(n) / 2.0 - static_cast<double>(below)) /
                                    static_cast<double>(frequency));
}

// cffi statistics lib
// cffi statistics lib
// cffi statistics lib
ffilib cffi_statistics_lib(){
    return cffi_lib(
        "statistics",
        {
            {"mean", (void*)fnMean},
            {"fmean", (void*)fnMean},
            {"median", (void*)fnMedian},
            {"median_low", (void*)fnMedianLow},
            {"median_high", (void*)fnMedianHigh},
            {"mode", (void*)fnMode},
            {"multimode", (void*)fnMultimode},
            {"pstdev", (void*)fnPstdev},
            {"stdev", (void*)fnStdev},
            {"pvariance", (void*)fnPvariance},
            {"variance", (void*)fnVariance},
            {"harmonic_mean", (void*)fnHarmonicMean},
            {"geometric_mean", (void*)fnGeometricMean},
            {"quantiles", (void*)fnQuantiles},
            {"covariance", (void*)fnCovariance},
            {"correlation", (void*)fnCorrelation},
            {"linear_regression", (void*)fnLinearRegression},
            {"median_grouped", (void*)fnMedianGrouped}
        });
}


}
