#include "../cffi.hpp"

#include <vector>

namespace minipy {





namespace {

// deep 复制
// deep copy
// deep ᠬᠣᠷᠢᠶ᠎ᠠ
minipy::value deepCopy(minipy::vm& vm, const minipy::value& value){
    if (value.type == "list" && value.ptr != nullptr){
        auto* list = static_cast<std::vector<minipy::value>*>(value.ptr);
        std::vector<minipy::value> items;
        for (auto& item : *list) items.push_back(deepCopy(vm, item));
        return vm.makeList(items);
    }
    if (value.type == "dict" && value.ptr != nullptr){
        auto* items = static_cast<minipy::dict_t*>(value.ptr);
        minipy::dict_t result;
        for (auto& item : *items){
            result.push_back({deepCopy(vm, item.first), deepCopy(vm, item.second)});
        }
        return vm.makeDict(result);
    }
    if (value.type == "tuple" && value.ptr != nullptr){
        auto* items = static_cast<std::vector<minipy::value>*>(value.ptr);
        std::vector<minipy::value> result;
        for (auto& item : *items) result.push_back(deepCopy(vm, item));
        return vm.makeTuple(result);
    }
    return value;
}
}

// fn 复制
// fn copy
// fn ᠬᠣᠷᠢᠶ᠎ᠠ
static minipy::value fnCopy(minipy::cffi& a){
    auto value = a.arg(0);
    if (value.type == "list" && value.ptr != nullptr){
        auto* list = static_cast<std::vector<minipy::value>*>(value.ptr);
        return a.mklist(*list);
    }
    if (value.type == "dict" && value.ptr != nullptr){
        return a.mkdict(*static_cast<minipy::dict_t*>(value.ptr));
    }
    return value;
}

// fn Deepcopy
// fn deepcopy
// fn Deepcopy
static minipy::value fnDeepcopy(minipy::cffi& a){
    return deepCopy(*a.vm, a.arg(0));
}

minipy::ffilib minipy::cffi_copy_lib(){
    return minipy::cffi_lib(
        "copy",
        {
            {"copy", (void*)fnCopy},
            {"deepcopy", (void*)fnDeepcopy}
        });
}


}
