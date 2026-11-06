#include "../cffi.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace minipy {





namespace {

// 值 Less
// value less
// ᠭᠠᠷᠴᠠᠭ Less
bool valueLess(minipy::cffi& a, const minipy::value& left, const minipy::value& right){
    if (left.type == "str" && right.type == "str"){
        return a.vm->toString(left) < a.vm->toString(right);
    }
    return a.vm->toFloat(left) < a.vm->toFloat(right);
}

// heap 列表
// heap list
// heap ᠲᠠᠯᠠᠭ᠌
std::vector<minipy::value>* heapList(minipy::cffi& a, const char* name){
    auto value = a.arg(0);
    if (value.type != "list" || value.ptr == nullptr){
        a.vm->raiseError("TypeError", std::string("object of type '") + value.type +
                                          "' passed to " + name + "() does not support indexing");
        return nullptr;
    }
    return static_cast<std::vector<minipy::value>*>(value.ptr);
}
template <typename Func>

// heap Operation
// heap operation
// heap Operation
void heapOperation(minipy::cffi& a, std::vector<minipy::value>& list, Func operation){
    auto greater = [&a](const auto& left, const auto& right){
        return valueLess(a, right, left);
    };
    operation(greater);
}
}

// fn Heapify
// fn heapify
// fn Heapify
static minipy::value fnHeapify(minipy::cffi& a){
    auto* list = heapList(a, "heapify");
    if (!list) return a.none();
    heapOperation(a, *list, [&](auto greater){ std::make_heap(list->begin(), list->end(), greater); });
    return a.none();
}

// fn Heappush
// fn heappush
// fn Heappush
static minipy::value fnHeappush(minipy::cffi& a){
    auto* list = heapList(a, "heappush");
    if (!list) return a.none();
    list->push_back(a.arg(1));
    heapOperation(a, *list, [&](auto greater){ std::push_heap(list->begin(), list->end(), greater); });
    return a.none();
}

// fn Heappop
// fn heappop
// fn Heappop
static minipy::value fnHeappop(minipy::cffi& a){
    auto* list = heapList(a, "heappop");
    if (!list) return a.none();
    if (list->empty()){
        a.vm->raiseError("IndexError", "index out of range");
        return a.none();
    }
    minipy::value result = list->front();
    heapOperation(a, *list, [&](auto greater){ std::pop_heap(list->begin(), list->end(), greater); });
    list->pop_back();
    return result;
}

// fn Heappushpop
// fn heappushpop
// fn Heappushpop
static minipy::value fnHeappushpop(minipy::cffi& a){
    auto* list = heapList(a, "heappushpop");
    if (!list) return a.none();
    minipy::value item = a.arg(1);
    if (list->empty() || !valueLess(a, item, list->front())) return fnHeappop(a);
    minipy::value result = list->front();
    list->front() = item;
    heapOperation(a, *list, [&](auto greater){ std::make_heap(list->begin(), list->end(), greater); });
    return result;
}

// fn Heapreplace
// fn heapreplace
// fn Heapreplace
static minipy::value fnHeapreplace(minipy::cffi& a){
    auto* list = heapList(a, "heapreplace");
    if (!list) return a.none();
    if (list->empty()){
        a.vm->raiseError("IndexError", "index out of range");
        return a.none();
    }
    minipy::value result = list->front();
    list->front() = a.arg(1);
    heapOperation(a, *list, [&](auto greater){ std::make_heap(list->begin(), list->end(), greater); });
    return result;
}

// fn Nsmallest
// fn nsmallest
// fn Nsmallest
static minipy::value fnNsmallest(minipy::cffi& a){
    long long n = a.arg_int(0);
    if (n <= 0) return a.mklist({});
    std::vector<minipy::value> items;
    a.vm->materializeIterable(a.arg(1), items);
    auto less = [&a](const minipy::value& left, const minipy::value& right){ return valueLess(a, left, right); };
    std::sort(items.begin(), items.end(), less);
    if (n < static_cast<long long>(items.size())) items.resize(static_cast<size_t>(n));
    return a.mklist(items);
}

// fn Nlargest
// fn nlargest
// fn Nlargest
static minipy::value fnNlargest(minipy::cffi& a){
    long long n = a.arg_int(0);
    if (n <= 0) return a.mklist({});
    std::vector<minipy::value> items;
    a.vm->materializeIterable(a.arg(1), items);
    auto greater = [&a](const minipy::value& left, const minipy::value& right){ return valueLess(a, right, left); };
    std::sort(items.begin(), items.end(), greater);
    if (n < static_cast<long long>(items.size())) items.resize(static_cast<size_t>(n));
    return a.mklist(items);
}

// cffi heapq lib
// cffi heapq lib
// cffi heapq lib
minipy::ffilib cffi_heapq_lib(){
    return minipy::cffi_lib(
        "heapq",
        {
            {"heapify", (void*)fnHeapify},
            {"heappush", (void*)fnHeappush},
            {"heappop", (void*)fnHeappop},
            {"heappushpop", (void*)fnHeappushpop},
            {"heapreplace", (void*)fnHeapreplace},
            {"nlargest", (void*)fnNlargest},
            {"nsmallest", (void*)fnNsmallest}
        });
}


}
