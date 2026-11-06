#include "../cffi.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <thread>
#include <vector>

namespace minipy {




using namespace minipy;

namespace {

// 转 Time T
// to time t
// ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Time T
std::time_t toTimeT(cffi& a, size_t index){
    if (index < a.argc()) return static_cast<std::time_t>(a.arg_float(index));
    return std::time(nullptr);
}

// struct Time 转 Data
// struct time to data
// struct Time ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Data
value structTimeToData(cffi& a, const std::tm& tm){
    return a.mklist({
        a.mkint(tm.tm_year + 1900),
        a.mkint(tm.tm_mon + 1),
        a.mkint(tm.tm_mday),
        a.mkint(tm.tm_hour),
        a.mkint(tm.tm_min),
        a.mkint(tm.tm_sec),
        a.mkint((tm.tm_wday + 6) % 7),
        a.mkint(tm.tm_yday + 1),
        a.mkint(tm.tm_isdst)
    });
}

// data 转 Struct Time
// data to struct time
// data ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Struct Time
bool dataToStructTime(cffi& a, const value& val, std::tm& tm){
    if (val.type != "list" || val.ptr == nullptr) return false;
    auto* list = static_cast<std::vector<value>*>(val.ptr);
    if (list->size() < 6) return false;
    std::tm zero{};
    tm = zero;
    tm.tm_year = static_cast<int>(a.vm->toInt((*list)[0])) - 1900;
    tm.tm_mon = static_cast<int>(a.vm->toInt((*list)[1])) - 1;
    tm.tm_mday = static_cast<int>(a.vm->toInt((*list)[2]));
    tm.tm_hour = static_cast<int>(a.vm->toInt((*list)[3]));
    tm.tm_min = static_cast<int>(a.vm->toInt((*list)[4]));
    tm.tm_sec = static_cast<int>(a.vm->toInt((*list)[5]));
    if (list->size() > 6) tm.tm_wday = static_cast<int>((a.vm->toInt((*list)[6]) + 1) % 7);
    if (list->size() > 7) tm.tm_yday = static_cast<int>(a.vm->toInt((*list)[7])) - 1;
    if (list->size() > 8) tm.tm_isdst = static_cast<int>(a.vm->toInt((*list)[8]));
    return true;
}

// 读取 Decimal
// read decimal
// ᠪᠡᠷᠢᠵᠡ Decimal
bool readDecimal(const std::string& text, size_t& pos, size_t maxDigits, int& out){
    size_t start = pos;
    out = 0;
    while (pos < text.size() && pos - start < maxDigits &&
           text[pos] >= '0' && text[pos] <= '9'){
        out = out * 10 + (text[pos] - '0');
        ++pos;
    }
    return pos > start;
}

// days 从 Civil
// days from civil
// days ᠡᠴᠡ Civil
long long daysFromCivil(int y, int m, int d){
    y -= (m <= 2) ? 1 : 0;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - static_cast<int>(era * 400));
    unsigned doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// fill Derived Time Fields
// fill derived time fields
// fill Derived Time Fields
void fillDerivedTimeFields(std::tm& tm){
    int year = tm.tm_year + 1900;
    int month = tm.tm_mon + 1;
    long long days = daysFromCivil(year, month, tm.tm_mday);
    tm.tm_yday = static_cast<int>(days - daysFromCivil(year, 1, 1));

    long long weekday = ((days % 7) + 7 + 3) % 7;
    tm.tm_wday = static_cast<int>((weekday + 1) % 7);
}
}

// fn Time
// fn time
// fn Time
static value fnTime(cffi& a){
    double seconds = static_cast<double>(
        std::chrono::system_clock::now().time_since_epoch().count());
    return a.mkfloat(seconds / 1000000000.0);
}

// fn Time Ns
// fn time ns
// fn Time Ns
static value fnTimeNs(cffi& a){
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    return a.mkint(ns);
}

// fn Perf Counter
// fn perf counter
// fn Perf Counter
static value fnPerfCounter(cffi& a){
    double seconds = static_cast<double>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return a.mkfloat(seconds / 1000000000.0);
}

// fn Monotonic
// fn monotonic
// fn Monotonic
static value fnMonotonic(cffi& a){
    double seconds = static_cast<double>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return a.mkfloat(seconds / 1000000000.0);
}

// fn Perf Counter Ns
// fn perf counter ns
// fn Perf Counter Ns
static value fnPerfCounterNs(cffi& a){
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
    return a.mkint(ns);
}

// fn Monotonic Ns
// fn monotonic ns
// fn Monotonic Ns
static value fnMonotonicNs(cffi& a){
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
    return a.mkint(ns);
}

// fn Process Time
// fn process time
// fn Process Time
static value fnProcessTime(cffi& a){
    return a.mkfloat(static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC));
}

// fn Sleep
// fn sleep
// fn Sleep
static value fnSleep(cffi& a){
    double seconds = a.arg_float(0);
    if (seconds < 0){
        a.vm->raiseError("ValueError", "sleep length must be non-negative");
        return a.none();
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    return a.none();
}

// fn Ctime
// fn ctime
// fn Ctime
static value fnCtime(cffi& a){
    std::time_t value = toTimeT(a, 0);
    const char* raw = std::ctime(&value);
    std::string text(raw ? raw : "");
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    return a.mkstr(text);
}

// fn Gmtime
// fn gmtime
// fn Gmtime
static value fnGmtime(cffi& a){
    std::time_t value = toTimeT(a, 0);
    std::tm tm{};
    std::tm* result = std::gmtime(&value);
    if (result) tm = *result;
    return structTimeToData(a, tm);
}

// fn Localtime
// fn localtime
// fn Localtime
static value fnLocaltime(cffi& a){
    std::time_t value = toTimeT(a, 0);
    std::tm tm{};
    std::tm* result = std::localtime(&value);
    if (result) tm = *result;
    return structTimeToData(a, tm);
}

// fn Mktime
// fn mktime
// fn Mktime
static value fnMktime(cffi& a){
    std::tm tm{};
    if (!dataToStructTime(a, a.arg(0), tm)){
        a.vm->raiseError("TypeError", "mktime() argument must be a 9-sequence");
        return a.none();
    }
    return a.mkfloat(static_cast<double>(std::mktime(&tm)));
}

// fn Strftime
// fn strftime
// fn Strftime
static value fnStrftime(cffi& a){
    std::string format = a.arg_str(0);
    std::time_t value = toTimeT(a, 1);
    std::tm tm{};
    std::tm* result = std::localtime(&value);
    if (result) tm = *result;
    char buffer[512];
    size_t written = std::strftime(buffer, sizeof(buffer), format.c_str(), &tm);
    return a.mkstr(std::string(buffer, written));
}

// fn Strptime
// fn strptime
// fn Strptime
static value fnStrptime(cffi& a){
    std::string text = a.arg_str(0);
    std::string format = (a.argc() > 1) ? a.arg_str(1) : "%Y-%m-%d %H:%M:%S";
    std::tm tm{};
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    size_t pos = 0;
    bool matched = true;
    std::string badDirective;
    for (size_t i = 0; i < format.size() && matched; ++i){
        char ch = format[i];
        if (ch != '%'){
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'){
                while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' ||
                                             text[pos] == '\n' || text[pos] == '\r')) ++pos;
            } else if (pos < text.size() && text[pos] == ch){
                ++pos;
            } else {
                matched = false;
            }
            continue;
        }
        if (i + 1 >= format.size()){
            matched = false;
            break;
        }
        char directive = format[++i];
        if (directive == '%'){
            if (pos < text.size() && text[pos] == '%') ++pos;
            else matched = false;
            continue;
        }
        if (directive == 'f'){

            size_t start = pos;
            while (pos < text.size() && pos - start < 6 &&
                   text[pos] >= '0' && text[pos] <= '9') ++pos;
            if (pos == start) matched = false;
            continue;
        }
        if (directive != 'Y' && directive != 'm' && directive != 'd' &&
            directive != 'H' && directive != 'M' && directive != 'S'){
            badDirective = std::string("%") + directive;
            matched = false;
            break;
        }
        int value = 0;
        size_t maxDigits = (directive == 'Y') ? 4 : 2;
        if (!readDecimal(text, pos, maxDigits, value)){
            matched = false;
            break;
        }
        switch (directive){
            case 'Y': tm.tm_year = value - 1900; break;
            case 'm':
                if (value < 1 || value > 12) matched = false;
                else tm.tm_mon = value - 1;
                break;
            case 'd':
                if (value < 1 || value > 31) matched = false;
                else tm.tm_mday = value;
                break;
            case 'H':
                if (value > 23) matched = false;
                else tm.tm_hour = value;
                break;
            case 'M':
                if (value > 59) matched = false;
                else tm.tm_min = value;
                break;
            case 'S':
                if (value > 61) matched = false;
                else tm.tm_sec = value;
                break;
        }
    }
    if (!matched){
        if (!badDirective.empty()){
            a.vm->raiseError("ValueError", "'" + badDirective + "' is a bad directive in format '" + format + "'");
        } else {
            a.vm->raiseError("ValueError", "time data '" + text + "' does not match format '" + format + "'");
        }
        return a.none();
    }
    if (pos < text.size()){
        a.vm->raiseError("ValueError", "unconverted data remains: " + text.substr(pos));
        return a.none();
    }
    fillDerivedTimeFields(tm);
    tm.tm_isdst = -1;
    return structTimeToData(a, tm);
}

// fn Asctime
// fn asctime
// fn Asctime
static value fnAsctime(cffi& a){
    std::tm tm{};
    if (a.argc() > 0 && dataToStructTime(a, a.arg(0), tm)){
    } else {
        std::time_t value = std::time(nullptr);
        std::tm* result = std::localtime(&value);
        if (result) tm = *result;
    }
    const char* raw = std::asctime(&tm);
    std::string text(raw ? raw : "");
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    return a.mkstr(text);
}

// data Clock Realtime
// data clock realtime
// data Clock Realtime
static value dataClockRealtime(cffi& a){ return a.mkint(0); }

// data Clock Monotonic
// data clock monotonic
// data Clock Monotonic
static value dataClockMonotonic(cffi& a){ return a.mkint(1); }

// cffi time lib
// cffi time lib
// cffi time lib
ffilib cffi_time_lib(){
    return cffi_lib(
        "time",
        {
            {"time", (void*)fnTime},
            {"time_ns", (void*)fnTimeNs},
            {"perf_counter", (void*)fnPerfCounter},
            {"perf_counter_ns", (void*)fnPerfCounterNs},
            {"monotonic", (void*)fnMonotonic},
            {"monotonic_ns", (void*)fnMonotonicNs},
            {"process_time", (void*)fnProcessTime},
            {"sleep", (void*)fnSleep},
            {"ctime", (void*)fnCtime},
            {"gmtime", (void*)fnGmtime},
            {"localtime", (void*)fnLocaltime},
            {"mktime", (void*)fnMktime},
            {"strftime", (void*)fnStrftime},
            {"strptime", (void*)fnStrptime},
            {"asctime", (void*)fnAsctime}
        },
        {
            {"CLOCK_REALTIME", (void*)dataClockRealtime},
            {"CLOCK_MONOTONIC", (void*)dataClockMonotonic}
        });
}


}
