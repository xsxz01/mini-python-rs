#include "../cffi.hpp"

#include <algorithm>
#include <vector>

namespace minipy {







namespace {

// sequence
// sequence
// sequence
std::vector<minipy::value> sequence(minipy::cffi& a, size_t index){
    std::vector<minipy::value> items;
    if (index < a.argc()) a.vm->materializeIterable(a.arg(index), items);
    return items;
}
void generatePermutations(const std::vector<minipy::value>& source, size_t r,
                          std::vector<bool>& used, std::vector<minipy::value>& current,
                          std::vector<minipy::value>& output, minipy::cffi& a){
    if (current.size() == r){
        output.push_back(a.mklist(current));
        return;
    }
    for (size_t i = 0; i < source.size(); ++i){
        if (used[i]) continue;
        used[i] = true;
        current.push_back(source[i]);
        generatePermutations(source, r, used, current, output, a);
        current.pop_back();
        used[i] = false;
    }
}
void generateCombinations(const std::vector<minipy::value>& source, size_t r, size_t start,
                          std::vector<minipy::value>& current, std::vector<minipy::value>& output,
                          minipy::cffi& a, bool replacement){
    if (current.size() == r){
        output.push_back(a.mklist(current));
        return;
    }
    for (size_t i = start; i < source.size(); ++i){
        current.push_back(source[i]);
        generateCombinations(source, r, replacement ? i : i + 1, current, output, a, replacement);
        current.pop_back();
    }
}
}

// fn Chain
// fn chain
// fn Chain
static minipy::value fnChain(minipy::cffi& a){
    std::vector<minipy::value> result;
    for (size_t i = 0; i < a.argc(); ++i){
        std::vector<minipy::value> items;
        a.vm->materializeIterable(a.arg(i), items);
        result.insert(result.end(), items.begin(), items.end());
    }
    return a.mklist(result);
}

// fn 重复
// fn repeat
// fn ᠳᠠᠨᠳᠠᠭᠤᠯᠬᠤ
static minipy::value fnRepeat(minipy::cffi& a){
    std::vector<minipy::value> result;
    long long count = (a.argc() > 1) ? a.arg_int(1) : 0;
    for (long long i = 0; i < count; ++i) result.push_back(a.arg(0));
    return a.mklist(result);
}

// fn Product
// fn product
// fn Product
static minipy::value fnProduct(minipy::cffi& a){
    std::vector<std::vector<minipy::value>> groups;
    for (size_t i = 0; i < a.argc(); ++i) groups.push_back(sequence(a, i));
    if (groups.empty()) return a.mklist({a.mklist({})});
    std::vector<minipy::value> result;
    std::vector<size_t> indices(groups.size(), 0);
    while (true){
        std::vector<minipy::value> current;
        for (size_t i = 0; i < groups.size(); ++i) current.push_back(groups[i][indices[i]]);
        result.push_back(a.mklist(current));
        size_t position = groups.size();
        while (position > 0){
            position--;
            indices[position]++;
            if (indices[position] < groups[position].size()) break;
            indices[position] = 0;
        }
        if (position == 0 && indices[0] == 0) break;
    }
    return a.mklist(result);
}

// fn Permutations
// fn permutations
// fn Permutations
static minipy::value fnPermutations(minipy::cffi& a){
    std::vector<minipy::value> source = sequence(a, 0);
    size_t r = (a.argc() > 1) ? static_cast<size_t>(a.arg_int(1)) : source.size();
    if (r > source.size()) return a.mklist({});
    std::vector<bool> used(source.size(), false);
    std::vector<minipy::value> current;
    std::vector<minipy::value> output;
    generatePermutations(source, r, used, current, output, a);
    return a.mklist(output);
}

// fn Combinations
// fn combinations
// fn Combinations
static minipy::value fnCombinations(minipy::cffi& a){
    std::vector<minipy::value> source = sequence(a, 0);
    size_t r = static_cast<size_t>(a.arg_int(1));
    if (r > source.size()) return a.mklist({});
    std::vector<minipy::value> current;
    std::vector<minipy::value> output;
    generateCombinations(source, r, 0, current, output, a, false);
    return a.mklist(output);
}

// fn Combinations With Replacement
// fn combinations with replacement
// fn Combinations With Replacement
static minipy::value fnCombinationsWithReplacement(minipy::cffi& a){
    std::vector<minipy::value> source = sequence(a, 0);
    size_t r = static_cast<size_t>(a.arg_int(1));
    if (source.empty() && r > 0) return a.mklist({});
    std::vector<minipy::value> current;
    std::vector<minipy::value> output;
    generateCombinations(source, r, 0, current, output, a, true);
    return a.mklist(output);
}

// fn Accumulate
// fn accumulate
// fn Accumulate
static minipy::value fnAccumulate(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    std::vector<minipy::value> result;
    if (items.empty()) return a.mklist(result);
    minipy::value total = items[0];
    result.push_back(total);
    for (size_t i = 1; i < items.size(); ++i){
        total = a.vm->applyBinary("+", total, items[i]);
        result.push_back(total);
    }
    return a.mklist(result);
}

// fn Compress
// fn compress
// fn Compress
static minipy::value fnCompress(minipy::cffi& a){
    std::vector<minipy::value> data = sequence(a, 0);
    std::vector<minipy::value> selectors = sequence(a, 1);
    std::vector<minipy::value> result;
    size_t count = std::min(data.size(), selectors.size());
    for (size_t i = 0; i < count; ++i){
        if (a.vm->truthy(selectors[i])) result.push_back(data[i]);
    }
    return a.mklist(result);
}

// fn Islice
// fn islice
// fn Islice
static minipy::value fnIslice(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    long long start = 0;
    long long stop = static_cast<long long>(items.size());
    long long step = 1;
    if (a.argc() == 2){
        stop = a.arg_int(1);
    } else if (a.argc() >= 3){
        start = a.arg_int(1);
        stop = a.arg_int(2);
        if (a.argc() >= 4) step = a.arg_int(3);
    }
    std::vector<minipy::value> result;
    if (step <= 0) return a.mklist(result);
    for (long long i = start; i < stop && i < static_cast<long long>(items.size()); i += step){
        result.push_back(items[static_cast<size_t>(i)]);
    }
    return a.mklist(result);
}

// fn Zip Longest
// fn zip longest
// fn Zip Longest
static minipy::value fnZipLongest(minipy::cffi& a){
    std::vector<std::vector<minipy::value>> groups;
    size_t longest = 0;
    for (size_t i = 0; i < a.argc(); ++i){
        std::vector<minipy::value> items;
        a.vm->materializeIterable(a.arg(i), items);
        longest = std::max(longest, items.size());
        groups.push_back(items);
    }
    std::vector<minipy::value> result;
    for (size_t row = 0; row < longest; ++row){
        std::vector<minipy::value> current;
        for (auto& group : groups){
            current.push_back(row < group.size() ? group[row] : a.none());
        }
        result.push_back(a.mklist(current));
    }
    return a.mklist(result);
}

// fn Pairwise
// fn pairwise
// fn Pairwise
static minipy::value fnPairwise(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    std::vector<minipy::value> result;
    for (size_t i = 1; i < items.size(); ++i){
        result.push_back(a.mklist({items[i - 1], items[i]}));
    }
    return a.mklist(result);
}

// fn Batched
// fn batched
// fn Batched
static minipy::value fnBatched(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    long long size = a.arg_int(1);
    if (size <= 0){
        a.vm->raiseError("ValueError", "batch size must be at least one");
        return a.none();
    }
    std::vector<minipy::value> result;
    for (size_t i = 0; i < items.size(); i += static_cast<size_t>(size)){
        size_t end = std::min(items.size(), i + static_cast<size_t>(size));
        std::vector<minipy::value> chunk(items.begin() + i, items.begin() + end);
        result.push_back(a.mklist(chunk));
    }
    return a.mklist(result);
}

// fn Starmap
// fn starmap
// fn Starmap
static minipy::value fnStarmap(minipy::cffi& a){
    minipy::value function = a.arg(0);
    std::vector<minipy::value> items = sequence(a, 1);
    std::vector<minipy::value> result;
    for (auto& item : items){
        std::vector<minipy::value> callArgs;
        a.vm->materializeIterable(item, callArgs);
        result.push_back(a.vm->callValue(function, callArgs));
    }
    return a.mklist(result);
}

// fn Filterfalse
// fn filterfalse
// fn Filterfalse
static minipy::value fnFilterfalse(minipy::cffi& a){
    minipy::value predicate = a.arg(0);
    std::vector<minipy::value> items = sequence(a, 1);
    std::vector<minipy::value> result;
    for (auto& item : items){
        bool hit = (predicate.type == "none")
                       ? a.vm->truthy(item)
                       : a.vm->truthy(a.vm->callValue(predicate, {item}));
        if (!hit) result.push_back(item);
    }
    return a.mklist(result);
}

// fn Takewhile
// fn takewhile
// fn Takewhile
static minipy::value fnTakewhile(minipy::cffi& a){
    minipy::value predicate = a.arg(0);
    std::vector<minipy::value> items = sequence(a, 1);
    std::vector<minipy::value> result;
    for (auto& item : items){
        bool hit = (predicate.type == "none")
                       ? a.vm->truthy(item)
                       : a.vm->truthy(a.vm->callValue(predicate, {item}));
        if (!hit) break;
        result.push_back(item);
    }
    return a.mklist(result);
}

// fn Dropwhile
// fn dropwhile
// fn Dropwhile
static minipy::value fnDropwhile(minipy::cffi& a){
    minipy::value predicate = a.arg(0);
    std::vector<minipy::value> items = sequence(a, 1);
    size_t index = 0;
    while (index < items.size()){
        bool hit = (predicate.type == "none")
                       ? a.vm->truthy(items[index])
                       : a.vm->truthy(a.vm->callValue(predicate, {items[index]}));
        if (!hit) break;
        index++;
    }
    std::vector<minipy::value> result;
    for (; index < items.size(); ++index) result.push_back(items[index]);
    return a.mklist(result);
}

// fn Tee
// fn tee
// fn Tee
static minipy::value fnTee(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    long long count = (a.argc() > 1) ? a.arg_int(1) : 2;
    if (count < 0){
        a.vm->raiseError("ValueError", "n must be >= 0");
        return a.none();
    }
    std::vector<minipy::value> copies;
    for (long long i = 0; i < count; ++i) copies.push_back(a.mklist(items));
    return a.vm->makeTuple(copies);
}

// fn Groupby
// fn groupby
// fn Groupby
static minipy::value fnGroupby(minipy::cffi& a){
    std::vector<minipy::value> items = sequence(a, 0);
    minipy::value keyFunction = a.arg(1);
    if (a.has_kwarg("key")) keyFunction = a.kwarg("key");
    std::vector<minipy::value> keys;
    for (auto& item : items){
        keys.push_back((keyFunction.type == "none") ? item
                                                    : a.vm->callValue(keyFunction, {item}));
    }
    std::vector<minipy::value> result;
    size_t index = 0;
    while (index < items.size()){
        size_t end = index + 1;
        while (end < items.size() && a.vm->valuesEqual(keys[end], keys[index])) end++;
        std::vector<minipy::value> group(items.begin() + index, items.begin() + end);
        result.push_back(a.vm->makeTuple({keys[index], a.mklist(group)}));
        index = end;
    }
    return a.mklist(result);
}

// cffi itertools lib
// cffi itertools lib
// cffi itertools lib
minipy::ffilib cffi_itertools_lib(){
    return minipy::cffi_lib(
        "itertools",
        {
            {"chain", (void*)fnChain},
            {"repeat", (void*)fnRepeat},
            {"product", (void*)fnProduct},
            {"permutations", (void*)fnPermutations},
            {"combinations", (void*)fnCombinations},
            {"combinations_with_replacement", (void*)fnCombinationsWithReplacement},
            {"accumulate", (void*)fnAccumulate},
            {"compress", (void*)fnCompress},
            {"islice", (void*)fnIslice},
            {"zip_longest", (void*)fnZipLongest},
            {"pairwise", (void*)fnPairwise},
            {"batched", (void*)fnBatched},
            {"starmap", (void*)fnStarmap},
            {"filterfalse", (void*)fnFilterfalse},
            {"takewhile", (void*)fnTakewhile},
            {"dropwhile", (void*)fnDropwhile},
            {"tee", (void*)fnTee},
            {"groupby", (void*)fnGroupby}
        });
}


}
