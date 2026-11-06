#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {





namespace {

// less Than
// less than
// less Than
bool lessThan(minipy::cffi& a, const minipy::value& left, const minipy::value& right){
    if (left.type == "str" && right.type == "str"){
        return a.vm->toString(left) < a.vm->toString(right);
    }
    return a.vm->toFloat(left) < a.vm->toFloat(right);
}

// as 列表
// as list
// as ᠲᠠᠯᠠᠭ᠌
std::vector<minipy::value>* asList(minipy::cffi& a, const char* name){
    auto value = a.arg(0);
    if (value.type != "list" || value.ptr == nullptr){
        a.vm->raiseError("TypeError", std::string("object of type '") + value.type +
                                          "' passed to " + name + "() does not support indexing");
        return nullptr;
    }
    return static_cast<std::vector<minipy::value>*>(value.ptr);
}

// bounds
// bounds
// bounds
size_t bounds(minipy::cffi& a, const std::vector<minipy::value>& list, size_t& lo, size_t& hi){
    lo = (a.argc() > 2) ? static_cast<size_t>(a.arg_int(2)) : 0;
    hi = (a.argc() > 3) ? static_cast<size_t>(a.arg_int(3)) : list.size();
    if (lo > list.size()) lo = list.size();
    if (hi > list.size()) hi = list.size();
    if (lo > hi) lo = hi;
    return hi;
}
size_t bisectLeft(minipy::cffi& a, const std::vector<minipy::value>& list,
                  const minipy::value& target, size_t lo, size_t hi){
    while (lo < hi){
        size_t middle = lo + (hi - lo) / 2;
        if (lessThan(a, list[middle], target)) lo = middle + 1;
        else hi = middle;
    }
    return lo;
}
size_t bisectRight(minipy::cffi& a, const std::vector<minipy::value>& list,
                   const minipy::value& target, size_t lo, size_t hi){
    while (lo < hi){
        size_t middle = lo + (hi - lo) / 2;
        if (!lessThan(a, target, list[middle])) lo = middle + 1;
        else hi = middle;
    }
    return lo;
}
}

// fn Bisect Left
// fn bisect left
// fn Bisect Left
static minipy::value fnBisectLeft(minipy::cffi& a){
    auto* list = asList(a, "bisect_left");
    if (!list) return a.none();
    size_t lo = 0, hi = 0;
    bounds(a, *list, lo, hi);
    return a.mkint(static_cast<long long>(bisectLeft(a, *list, a.arg(1), lo, hi)));
}

// fn Bisect Right
// fn bisect right
// fn Bisect Right
static minipy::value fnBisectRight(minipy::cffi& a){
    auto* list = asList(a, "bisect_right");
    if (!list) return a.none();
    size_t lo = 0, hi = 0;
    bounds(a, *list, lo, hi);
    return a.mkint(static_cast<long long>(bisectRight(a, *list, a.arg(1), lo, hi)));
}

// fn Insort Left
// fn insort left
// fn Insort Left
static minipy::value fnInsortLeft(minipy::cffi& a){
    auto* list = asList(a, "insort_left");
    if (!list) return a.none();
    size_t lo = 0, hi = 0;
    bounds(a, *list, lo, hi);
    size_t index = bisectLeft(a, *list, a.arg(1), lo, hi);
    list->insert(list->begin() + static_cast<long long>(index), a.arg(1));
    return a.none();
}

// fn Insort Right
// fn insort right
// fn Insort Right
static minipy::value fnInsortRight(minipy::cffi& a){
    auto* list = asList(a, "insort_right");
    if (!list) return a.none();
    size_t lo = 0, hi = 0;
    bounds(a, *list, lo, hi);
    size_t index = bisectRight(a, *list, a.arg(1), lo, hi);
    list->insert(list->begin() + static_cast<long long>(index), a.arg(1));
    return a.none();
}

// cffi bisect lib
// cffi bisect lib
// cffi bisect lib
minipy::ffilib cffi_bisect_lib(){
    return minipy::cffi_lib(
        "bisect",
        {
            {"bisect_left", (void*)fnBisectLeft},
            {"bisect_right", (void*)fnBisectRight},
            {"bisect", (void*)fnBisectRight},
            {"insort_left", (void*)fnInsortLeft},
            {"insort_right", (void*)fnInsortRight},
            {"insort", (void*)fnInsortRight}
        });
}


}
