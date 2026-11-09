#include "../cffi.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 全局默认每周起始日 (0=Monday)
// Global default first weekday (0=Monday)
// ᠡᠷ᠎ᠡ ᠡᠬᠢᠯᠡᠯ ᠦᠨ ᠡᠳᠦᠷ
int gFirstWeekday = 0;

// 公历转儒略日偏移
// Civil date to days since 1970-01-01
// ᠴᠠᠭ᠎ᠠ ᠪᠤᠯᠤᠨ ᠡᠳᠦᠷ ᠦᠨ ᠬᠤᠪᠢᠶ᠎ᠠ
long long daysFromCivil(long long year, int month, int day){
    year -= (month <= 2);
    long long era = (year >= 0 ? year : year - 399) / 400;
    unsigned yoe = static_cast<unsigned>(year - era * 400);
    unsigned doy = static_cast<unsigned>((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

// 向下取整模
// Floor modulo
// ᠳᠣᠣᠷᠠ ᠮᠣᠳᠤᠯᠢ
long long floorMod(long long value, long long mod){
    long long result = value % mod;
    if (result < 0) result += mod;
    return result;
}

// 闰年判断
// Leap year test
// ᠢᠯᠳᠦᠯᠡᠭᠴᠢ ᠵᠢᠯ
bool isLeapYear(long long year){
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

// 某月天数
// Number of days in a month
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠡᠳᠦᠷ ᠦᠨ ᠲᠣᠭ᠎ᠠ
int monthLength(long long year, int month){
    static const int lengths[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return lengths[month];
}

// 检查月份范围
// Validate month range
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠬᠦᠷᠢᠶ᠎ᠡ ᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool checkMonth(cffi& a, long long month){
    if (month < 1 || month > 12){
        a.vm->raiseError("IllegalMonthError", "month must be in 1..12");
        return false;
    }
    return true;
}

// 某月第一天是星期几 (Monday=0)
// Weekday of the first day of a month (Monday=0)
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠨᠢᠭᠡᠳᠦᠭᠡᠷ ᠡᠳᠦᠷ
int firstWeekdayOfMonth(long long year, int month){
    return static_cast<int>(floorMod(daysFromCivil(year, month, 1) + 3, 7));
}

// 取实例上的 firstweekday
// Read firstweekday from a Calendar instance
// ᠬᠢᠶ᠎ᠡ ᠡᠴᠡ ᠡᠬᠢᠯᠡᠯ ᠦᠨ ᠡᠳᠦᠷ
int instanceFirstWeekday(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        auto it = inst->attributes.find("firstweekday");
        if (it != inst->attributes.end() && it->second.type == "int"){
            return static_cast<int>(a.vm->toInt(it->second));
        }
        if (inst->handle){
            return *static_cast<int*>(inst->handle);
        }
    }
    return gFirstWeekday;
}

// 生成某月的 [(day, weekday)] 周结构
// Build the (day, weekday) week grid for a month
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠭᠠᠷᠠᠭ ᠪᠠᠶᠢᠴᠠᠭ᠎ᠠ
std::vector<std::vector<std::pair<int, int>>> monthGrid(long long year, int month, int firstDay){
    std::vector<std::vector<std::pair<int, int>>> weeks;
    int startWeekday = firstWeekdayOfMonth(year, month);
    int offset = static_cast<int>(floorMod(startWeekday - firstDay, 7));
    int length = monthLength(year, month);
    std::vector<std::pair<int, int>> week;
    for (int i = 0; i < offset; ++i){
        week.push_back({0, static_cast<int>(floorMod(firstDay + i, 7))});
    }
    for (int day = 1; day <= length; ++day){
        week.push_back({day, static_cast<int>(floorMod(startWeekday + day - 1, 7))});
        if (week.size() == 7){
            weeks.push_back(week);
            week.clear();
        }
    }
    if (!week.empty()){
        while (week.size() < 7){
            week.push_back({0, static_cast<int>(floorMod(firstDay + static_cast<int>(week.size()), 7))});
        }
        weeks.push_back(week);
    }
    return weeks;
}

// 格式化一天单元格
// Format one day cell
// ᠨᠢᠭᠡ ᠡᠳᠦᠷ ᠦᠨ ᠬᠡᠰᠡᠭ
std::string formatDayCell(int day, int width){
    char buffer[64];
    if (day == 0){
        std::snprintf(buffer, sizeof(buffer), "%*s", width, "");
    } else {
        std::snprintf(buffer, sizeof(buffer), "%*d", width, day);
    }
    return buffer;
}

// 格式化一周
// Format one week row
// ᠨᠢᠭᠡ ᠳᠣᠯᠣᠭᠠᠨ ᠡᠳᠦᠷ ᠦᠨ ᠮᠥᠷ
std::string formatWeek(const std::vector<std::pair<int, int>>& week, int width){
    std::string line;
    for (size_t i = 0; i < week.size(); ++i){
        if (i) line.push_back(' ');
        line += formatDayCell(week[i].first, width);
    }
    return line;
}

// 格式化星期表头
// Format the weekday header
// ᠳᠣᠯᠣᠭᠠᠨ ᠡᠳᠦᠷ ᠦᠨ ᠲᠣᠯᠣᠭᠠᠢ
std::string formatWeekHeader(cffi& a, int width, int firstDay){
    (void)a;
    std::string line;
    for (int i = 0; i < 7; ++i){
        if (i) line += " ";
        static const char* names[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        std::string text = names[static_cast<size_t>(floorMod(firstDay + i, 7))];
        if (static_cast<int>(text.size()) < width){
            text = std::string(static_cast<size_t>(width - static_cast<int>(text.size())), ' ') + text;
        }
        line += text;
    }
    return line;
}

// 月份标题
// Month title line
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠲᠣᠯᠣᠭᠠᠢ
std::string monthTitle(cffi& a, long long year, int month, int width){
    static const char* names[13] = {"", "January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};
    std::string title = std::string(names[month]) + " " + std::to_string(year);
    int space = width - static_cast<int>(title.size());
    if (space < 0) space = 0;
    int left = space / 2;
    int right = space - left;
    return std::string(static_cast<size_t>(left), ' ') + title +
           std::string(static_cast<size_t>(right), ' ');
}

// 构造一个 Python 列表
// Build a Python list
// Python ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeIntList(cffi& a, const std::vector<long long>& items){
    std::vector<value> values;
    for (long long item : items) values.push_back(a.mkint(item));
    return a.mklist(values);
}
}

// fn Isleap
// fn isleap
// fn Isleap
static value fnIsleap(cffi& a){
    return a.mkbool(isLeapYear(a.arg_int(0)));
}

// fn Leapdays
// fn leapdays
// fn Leapdays
static value fnLeapdays(cffi& a){
    long long y1 = a.arg_int(0), y2 = a.arg_int(1);
    long long count = 0;
    for (long long year = y1; year < y2; ++year){
        if (isLeapYear(year)) count++;
    }
    return a.mkint(count);
}

// fn Weekday
// fn weekday
// fn Weekday
static value fnWeekday(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    long long day = a.arg_int(2);
    return a.mkint(floorMod(daysFromCivil(year, static_cast<int>(month), static_cast<int>(day)) + 3, 7));
}

// fn Monthrange
// fn monthrange
// fn Monthrange
static value fnMonthrange(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    std::vector<value> items = {
        a.mkint(firstWeekdayOfMonth(year, static_cast<int>(month))),
        a.mkint(monthLength(year, static_cast<int>(month)))
    };
    return a.vm->makeTuple(items);
}

// fn Monthcalendar
// fn monthcalendar
// fn Monthcalendar
static value fnMonthcalendar(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    auto grid = monthGrid(year, static_cast<int>(month), gFirstWeekday);
    std::vector<value> rows;
    for (auto& week : grid){
        std::vector<long long> days;
        for (auto& cell : week) days.push_back(cell.first);
        rows.push_back(makeIntList(a, days));
    }
    return a.mklist(rows);
}

// fn Firstweekday
// fn firstweekday
// fn Firstweekday
static value fnFirstweekday(cffi& a){
    return a.mkint(gFirstWeekday);
}

// fn Setfirstweekday
// fn setfirstweekday
// fn Setfirstweekday
static value fnSetfirstweekday(cffi& a){
    long long day = a.arg_int(0);
    if (day < 0 || day > 6){
        a.vm->raiseError("IllegalWeekdayError", "day must be in 0..6");
        return a.none();
    }
    gFirstWeekday = static_cast<int>(day);
    return a.none();
}

// fn Timegm
// fn timegm
// fn Timegm
static value fnTimegm(cffi& a){
    std::vector<value> fields;
    a.vm->materializeIterable(a.arg(0), fields);
    if (a.vm->hasError()) return a.none();
    while (fields.size() < 6) fields.push_back(a.mkint(0));
    long long year = a.vm->toInt(fields[0]);
    long long month = a.vm->toInt(fields[1]);
    long long day = a.vm->toInt(fields[2]);
    long long hour = a.vm->toInt(fields[3]);
    long long minute = a.vm->toInt(fields[4]);
    long long second = a.vm->toInt(fields[5]);
    long long days = daysFromCivil(year, static_cast<int>(month), static_cast<int>(day));
    return a.mkint(days * 86400 + hour * 3600 + minute * 60 + second);
}

// 格式化月份文本
// Format a month as text
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠪᠢᠴᠢᠭ
static std::string formatMonthText(cffi& a, long long year, int month, int w, int l, int firstDay){
    if (w < 2) w = 2;
    if (l < 1) l = 1;
    int width = 7 * (w + 1) - 1;
    std::string out;
    out += monthTitle(a, year, month, width) + "\n";
    out += formatWeekHeader(a, w, firstDay) + "\n";
    auto grid = monthGrid(year, month, firstDay);
    for (auto& week : grid){
        out += formatWeek(week, w) + "\n";
        for (int k = 1; k < l; ++k) out += "\n";
    }
    return out;
}

// fn Prmonth
// fn prmonth
// fn Prmonth
static value fnPrmonth(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    int w = (a.argc() > 2) ? static_cast<int>(a.arg_int(2)) : 0;
    int l = (a.argc() > 3) ? static_cast<int>(a.arg_int(3)) : 0;
    std::string text = formatMonthText(a, year, static_cast<int>(month), w, l, gFirstWeekday);
    std::printf("%s", text.c_str());
    return a.none();
}

// fn Prcal
// fn prcal
// fn Prcal
static value fnPrcal(cffi& a){
    long long year = a.arg_int(0);
    int w = (a.argc() > 1) ? static_cast<int>(a.arg_int(1)) : 0;
    int l = (a.argc() > 2) ? static_cast<int>(a.arg_int(2)) : 0;
    (void)w;
    (void)l;
    std::printf("%lld\n", year);
    for (int month = 1; month <= 12; ++month){
        std::printf("%s", formatMonthText(a, year, month, 2, 1, gFirstWeekday).c_str());
    }
    return a.none();
}

// fn Calendar
// fn calendar
// fn Calendar
static value fnCalendarText(cffi& a){
    long long year = a.arg_int(0);
    int w = (a.argc() > 1) ? static_cast<int>(a.arg_int(1)) : 2;
    int l = (a.argc() > 2) ? static_cast<int>(a.arg_int(2)) : 1;
    (void)l;
    std::string out = std::to_string(year) + "\n";
    for (int month = 1; month <= 12; ++month){
        out += formatMonthText(a, year, month, w, 1, gFirstWeekday);
    }
    return a.mkstr(out);
}

// Calendar 构造器
// Calendar constructor
// Calendar ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnCalendarConstructor(cffi& a){
    int firstDay = gFirstWeekday;
    if (a.argc() >= 1 && a.arg(0).type != "none") firstDay = static_cast<int>(a.arg_int(0));
    value obj = a.mkobject(new int(firstDay));
    a.setattr(obj, "firstweekday", a.mkint(firstDay));
    return obj;
}

// Calendar.getfirstweekday
// Calendar.getfirstweekday
// Calendar.getfirstweekday
static value fnCalendarGetFirst(cffi& a){
    return a.mkint(instanceFirstWeekday(a));
}

// Calendar.setfirstweekday
// Calendar.setfirstweekday
// Calendar.setfirstweekday
static value fnCalendarSetFirst(cffi& a){
    value self = a.self();
    long long day = a.arg_int(0);
    if (day < 0 || day > 6){
        a.vm->raiseError("IllegalWeekdayError", "day must be in 0..6");
        return a.none();
    }
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        if (inst->handle) *static_cast<int*>(inst->handle) = static_cast<int>(day);
        a.setattr("firstweekday", a.mkint(day));
    }
    return a.none();
}

// Calendar.iterweekdays
// Calendar.iterweekdays
// Calendar.iterweekdays
static value fnCalendarIterWeekdays(cffi& a){
    int firstDay = instanceFirstWeekday(a);
    std::vector<long long> days;
    for (int i = 0; i < 7; ++i) days.push_back(floorMod(firstDay + i, 7));
    return makeIntList(a, days);
}

// Calendar.monthdayscalendar
// Calendar.monthdayscalendar
// Calendar.monthdayscalendar
static value fnCalendarMonthDays(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    auto grid = monthGrid(year, static_cast<int>(month), instanceFirstWeekday(a));
    std::vector<value> rows;
    for (auto& week : grid){
        std::vector<long long> days;
        for (auto& cell : week) days.push_back(cell.first);
        rows.push_back(makeIntList(a, days));
    }
    return a.mklist(rows);
}

// Calendar.monthdays2calendar
// Calendar.monthdays2calendar
// Calendar.monthdays2calendar
static value fnCalendarMonthDays2(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    auto grid = monthGrid(year, static_cast<int>(month), instanceFirstWeekday(a));
    std::vector<value> rows;
    for (auto& week : grid){
        std::vector<value> cells;
        for (auto& cell : week){
            std::vector<value> pair = {a.mkint(cell.first), a.mkint(cell.second)};
            cells.push_back(a.vm->makeTuple(pair));
        }
        rows.push_back(a.mklist(cells));
    }
    return a.mklist(rows);
}

// Calendar.itermonthdays
// Calendar.itermonthdays
// Calendar.itermonthdays
static value fnCalendarIterMonthDays(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    auto grid = monthGrid(year, static_cast<int>(month), instanceFirstWeekday(a));
    std::vector<long long> days;
    for (auto& week : grid){
        for (auto& cell : week) days.push_back(cell.first);
    }
    return makeIntList(a, days);
}

// Calendar.itermonthdays2
// Calendar.itermonthdays2
// Calendar.itermonthdays2
static value fnCalendarIterMonthDays2(cffi& a){
    long long year = a.arg_int(0);
    long long month = a.arg_int(1);
    if (!checkMonth(a, month)) return a.none();
    auto grid = monthGrid(year, static_cast<int>(month), instanceFirstWeekday(a));
    std::vector<value> cells;
    for (auto& week : grid){
        for (auto& cell : week){
            std::vector<value> pair = {a.mkint(cell.first), a.mkint(cell.second)};
            cells.push_back(a.vm->makeTuple(pair));
        }
    }
    return a.mklist(cells);
}

// Calendar.yeardays2calendar
// Calendar.yeardays2calendar
// Calendar.yeardays2calendar
static value fnCalendarYearDays2(cffi& a){
    long long year = a.arg_int(0);
    int width = (a.argc() > 1) ? static_cast<int>(a.arg_int(1)) : 3;
    if (width < 1) width = 1;
    int firstDay = instanceFirstWeekday(a);
    std::vector<value> groups;
    std::vector<value> months;
    for (int month = 1; month <= 12; ++month){
        auto grid = monthGrid(year, month, firstDay);
        std::vector<value> rows;
        for (auto& week : grid){
            std::vector<value> cells;
            for (auto& cell : week){
                std::vector<value> pair = {a.mkint(cell.first), a.mkint(cell.second)};
                cells.push_back(a.vm->makeTuple(pair));
            }
            rows.push_back(a.mklist(cells));
        }
        months.push_back(a.mklist(rows));
        if (static_cast<int>(months.size()) == width){
            groups.push_back(a.mklist(months));
            months.clear();
        }
    }
    if (!months.empty()) groups.push_back(a.mklist(months));
    return a.mklist(groups);
}

// Calendar.yeardayscalendar
// Calendar.yeardayscalendar
// Calendar.yeardayscalendar
static value fnCalendarYearDays(cffi& a){
    long long year = a.arg_int(0);
    int width = (a.argc() > 1) ? static_cast<int>(a.arg_int(1)) : 3;
    if (width < 1) width = 1;
    int firstDay = instanceFirstWeekday(a);
    std::vector<value> groups;
    std::vector<value> months;
    for (int month = 1; month <= 12; ++month){
        auto grid = monthGrid(year, month, firstDay);
        std::vector<value> rows;
        for (auto& week : grid){
            std::vector<long long> days;
            for (auto& cell : week) days.push_back(cell.first);
            rows.push_back(makeIntList(a, days));
        }
        months.push_back(a.mklist(rows));
        if (static_cast<int>(months.size()) == width){
            groups.push_back(a.mklist(months));
            months.clear();
        }
    }
    if (!months.empty()) groups.push_back(a.mklist(months));
    return a.mklist(groups);
}

// 整型数据项定义宏
// Macro for integer data entries
// ᠲᠣᠭ᠎ᠠ ᠳᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠮᠠᠺᠷᠣ
#define CALENDAR_INT_DATA(fnName, valueText) \
    static value fnName(cffi& a){ return a.mkint(valueText); }

CALENDAR_INT_DATA(dataJanuary, 1)
CALENDAR_INT_DATA(dataFebruary, 2)
CALENDAR_INT_DATA(dataMarch, 3)
CALENDAR_INT_DATA(dataApril, 4)
CALENDAR_INT_DATA(dataMay, 5)
CALENDAR_INT_DATA(dataJune, 6)
CALENDAR_INT_DATA(dataJuly, 7)
CALENDAR_INT_DATA(dataAugust, 8)
CALENDAR_INT_DATA(dataSeptember, 9)
CALENDAR_INT_DATA(dataOctober, 10)
CALENDAR_INT_DATA(dataNovember, 11)
CALENDAR_INT_DATA(dataDecember, 12)
CALENDAR_INT_DATA(dataMonday, 0)
CALENDAR_INT_DATA(dataTuesday, 1)
CALENDAR_INT_DATA(dataWednesday, 2)
CALENDAR_INT_DATA(dataThursday, 3)
CALENDAR_INT_DATA(dataFriday, 4)
CALENDAR_INT_DATA(dataSaturday, 5)
CALENDAR_INT_DATA(dataSunday, 6)
#undef CALENDAR_INT_DATA

// 星期名称数据
// Weekday name data
// ᠡᠳᠦᠷ ᠦᠨ ᠨᠡᠷ᠎ᠡ ᠳᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
static value dataDayNames(cffi& a){
    std::vector<value> items;
    static const char* names[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    for (auto* name : names) items.push_back(a.mkstr(name));
    return a.mklist(items);
}

// 星期缩写数据
// Weekday abbreviation data
// ᠡᠳᠦᠷ ᠦᠨ ᠲᠣᠪᠴᠢ ᠳᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
static value dataDayAbbr(cffi& a){
    std::vector<value> items;
    static const char* names[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (auto* name : names) items.push_back(a.mkstr(name));
    return a.mklist(items);
}

// 月份名称数据
// Month name data
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠨᠡᠷ᠎ᠡ ᠳᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
static value dataMonthNames(cffi& a){
    std::vector<value> items;
    static const char* names[13] = {"", "January", "February", "March", "April", "May", "June",
                                    "July", "August", "September", "October", "November", "December"};
    for (auto* name : names) items.push_back(a.mkstr(name));
    return a.mklist(items);
}

// 月份缩写数据
// Month abbreviation data
// ᠰᠠᠷ᠎ᠠ ᠶᠢᠨ ᠲᠣᠪᠴᠢ ᠳᠡᠯᠳ᠋ᠡᠭᠡᠷᠢ
static value dataMonthAbbr(cffi& a){
    std::vector<value> items;
    static const char* names[13] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (auto* name : names) items.push_back(a.mkstr(name));
    return a.mklist(items);
}

// cffi calendar lib
// cffi calendar lib
// cffi calendar lib
ffilib cffi_calendar_lib(){
    ffi_class calendarClass;
    calendarClass.name = "Calendar";
    calendarClass.constructor = (void*)fnCalendarConstructor;
    calendarClass.methods = {
        {"getfirstweekday", (void*)fnCalendarGetFirst},
        {"setfirstweekday", (void*)fnCalendarSetFirst},
        {"iterweekdays", (void*)fnCalendarIterWeekdays},
        {"monthdayscalendar", (void*)fnCalendarMonthDays},
        {"monthdays2calendar", (void*)fnCalendarMonthDays2},
        {"itermonthdays", (void*)fnCalendarIterMonthDays},
        {"itermonthdays2", (void*)fnCalendarIterMonthDays2},
        {"yeardayscalendar", (void*)fnCalendarYearDays},
        {"yeardays2calendar", (void*)fnCalendarYearDays2}
    };
    return cffi_lib(
        "calendar",
        {
            {"isleap", (void*)fnIsleap},
            {"leapdays", (void*)fnLeapdays},
            {"weekday", (void*)fnWeekday},
            {"monthrange", (void*)fnMonthrange},
            {"monthcalendar", (void*)fnMonthcalendar},
            {"firstweekday", (void*)fnFirstweekday},
            {"setfirstweekday", (void*)fnSetfirstweekday},
            {"timegm", (void*)fnTimegm},
            {"prmonth", (void*)fnPrmonth},
            {"prcal", (void*)fnPrcal},
            {"calendar", (void*)fnCalendarText}
        },
        {
            {"JANUARY", (void*)dataJanuary},
            {"FEBRUARY", (void*)dataFebruary},
            {"MARCH", (void*)dataMarch},
            {"APRIL", (void*)dataApril},
            {"MAY", (void*)dataMay},
            {"JUNE", (void*)dataJune},
            {"JULY", (void*)dataJuly},
            {"AUGUST", (void*)dataAugust},
            {"SEPTEMBER", (void*)dataSeptember},
            {"OCTOBER", (void*)dataOctober},
            {"NOVEMBER", (void*)dataNovember},
            {"DECEMBER", (void*)dataDecember},
            {"MONDAY", (void*)dataMonday},
            {"TUESDAY", (void*)dataTuesday},
            {"WEDNESDAY", (void*)dataWednesday},
            {"THURSDAY", (void*)dataThursday},
            {"FRIDAY", (void*)dataFriday},
            {"SATURDAY", (void*)dataSaturday},
            {"SUNDAY", (void*)dataSunday},
            {"day_name", (void*)dataDayNames},
            {"day_abbr", (void*)dataDayAbbr},
            {"month_name", (void*)dataMonthNames},
            {"month_abbr", (void*)dataMonthAbbr}
        },
        {
            {"Calendar", calendarClass}
        });
}

}
