#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace minipy {

namespace {

// pprint 配置
// pprint settings
// pprint ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct PPSettings{
    long long indent = 1;
    long long width = 80;
    long long depth = -1;
    bool compact = false;
    bool sortDicts = true;
    bool underscore = false;
};

// pprint 运行状态
// pprint runtime state
// pprint ᠪᠠᠶᠢᠳᠠᠯ
struct PPState{
    vm* vm = nullptr;
    PPSettings settings;
    value stream;
    bool hasStream = false;
    bool recursive = false;
    bool readable = true;
};

// 安全 repr 结果
// safe repr result
// ᠠᠶᠤᠯᠭᠦᠢ repr ᠦᠷᠡ
struct SafeResult{
    std::string repr;
    bool readable = true;
    bool recursive = false;
};

// 追加空格
// append spaces
// ᠬᠣᠭᠤᠰᠤᠨ ᠨᠡᠮᠡᠬᠦ
void appendSpaces(std::string& out, long long count){
    if (count > 0) out.append(static_cast<size_t>(count), ' ');
}

// 递归表示文本
// recursion representation text
// ᠳᠠᠪᠬᠠᠭᠳᠠᠯ ᠲᠡᠮᠳᠡᠭ
std::string recursionText(vm& vm, const value& v){
    unsigned long long address = static_cast<unsigned long long>(
        reinterpret_cast<uintptr_t>(v.ptr));
    return "<Recursion on " + vm.pythonTypeName(v) + " with id=" +
           std::to_string(address) + ">";
}

// 整数下划线格式
// int with underscore separators
// ᠲᠣᠭ᠎ᠠ ᠳᠤ ᠤᠷᠤᠰᠬᠠᠯ
std::string intUnderscore(long long value){
    bool neg = value < 0;
    unsigned long long magnitude = neg ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                                       : static_cast<unsigned long long>(value);
    std::string digits = std::to_string(magnitude);
    std::string result;
    size_t count = digits.size();
    for (size_t i = 0; i < count; ++i){
        if (i > 0 && (count - i) % 3 == 0) result.push_back('_');
        result.push_back(digits[i]);
    }
    if (neg) result.insert(result.begin(), '-');
    return result;
}

// 是否 标量
// whether a value is a builtin scalar
// ᠪᠠᠶᠢᠳᠠᠯ ᠰᠢᠯᠢᠳᠡᠭ
bool isScalar(const value& v){
    return v.type == "none" || v.type == "bool" || v.type == "int" ||
           v.type == "float" || v.type == "str" || v.type == "bytes" ||
           v.type == "bytearray";
}

// 是否 容器
// whether a value is a pretty-printed container
// ᠰᠠᠪᠬᠠᠷᠢ ᠪᠠᠶᠢᠳᠠᠯ ᠮᠦᠨ
bool isContainer(const value& v){
    return v.ptr != nullptr &&
           (v.type == "dict" || v.type == "list" || v.type == "tuple");
}

// 字符串 行拆分 (保留换行)
// split string into lines keeping terminators
// ᠮᠥᠷ ᠬᠤᠪᠢᠶ᠎ᠠ
std::vector<std::string> splitLinesKeep(const std::string& text){
    std::vector<std::string> lines;
    size_t start = 0;
    size_t i = 0;
    while (i < text.size()){
        char c = text[i];
        if (c == '\n' || c == '\r'){
            size_t end = i + 1;
            if (c == '\r' && end < text.size() && text[end] == '\n') ++end;
            lines.push_back(text.substr(start, end - start));
            start = end;
            i = end;
            continue;
        }
        ++i;
    }
    if (start < text.size()) lines.push_back(text.substr(start));
    return lines;
}

// 字符串 repr
// repr of a Python string
// ᠦᠰᠦᠭ repr
std::string stringRepr(vm& vm, const std::string& text){
    return pythonRepr(vm, vm.makeString(text));
}

// 值 排序比较
// compare values for dict sorting
// ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠤᠯ
int compareForSort(vm& vm, const value& left, const value& right, bool& comparable){
    comparable = true;
    if ((left.type == "int" || left.type == "bool") &&
        (right.type == "int" || right.type == "bool")){
        long long a = vm.toInt(left), b = vm.toInt(right);
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    if ((left.type == "int" || left.type == "bool" || left.type == "float") &&
        (right.type == "int" || right.type == "bool" || right.type == "float")){
        double a = vm.toFloat(left), b = vm.toFloat(right);
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    if (left.type == "str" && right.type == "str"){
        std::string a = *static_cast<std::string*>(left.ptr);
        std::string b = *static_cast<std::string*>(right.ptr);
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    if (left.type == "tuple" && right.type == "tuple"){
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        size_t count = std::min(a->size(), b->size());
        for (size_t i = 0; i < count; ++i){
            int r = compareForSort(vm, (*a)[i], (*b)[i], comparable);
            if (!comparable || r != 0) return r;
        }
        if (a->size() != b->size())
            return a->size() < b->size() ? -1 : 1;
        return 0;
    }
    comparable = false;
    return 0;
}

// 安全比较 (混合类型回退到类型名 + 地址)
// safe compare fallback to type name then address
// ᠠᠶᠤᠯᠭᠦᠢ ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠤᠯ
bool safeLess(vm& vm, const value& left, const value& right){
    bool comparable = false;
    int r = compareForSort(vm, left, right, comparable);
    if (comparable) return r < 0;
    std::string leftType = vm.pythonTypeName(left);
    std::string rightType = vm.pythonTypeName(right);
    if (leftType != rightType) return leftType < rightType;
    return reinterpret_cast<uintptr_t>(left.ptr) < reinterpret_cast<uintptr_t>(right.ptr);
}

SafeResult safeRepr(PPState& st, const value& object, std::set<const void*>& context,
                    long long maxlevels, long long level);

void formatObject(PPState& st, const value& object, std::string& out,
                  long long indent, long long allowance,
                  std::set<const void*>& context, long long level);

// 字典条目格式化
// format dict items
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠬᠡᠰᠡᠭ
void formatDictItems(PPState& st, const std::vector<std::pair<value, value>>& items,
                     std::string& out, long long indent, long long allowance,
                     std::set<const void*>& context, long long level){
    long long innerIndent = indent + st.settings.indent;
    std::string delimNewline = ",\n";
    appendSpaces(delimNewline, innerIndent);
    long long lastIndex = static_cast<long long>(items.size()) - 1;
    for (long long i = 0; i <= lastIndex; ++i){
        bool last = i == lastIndex;
        std::set<const void*> copy = context;
        SafeResult keyRep = safeRepr(st, items[static_cast<size_t>(i)].first, copy,
                                     st.settings.depth > 0 ? st.settings.depth : 0, level);
        if (!keyRep.readable) st.readable = false;
        if (keyRep.recursive) st.recursive = true;
        out += keyRep.repr;
        out += ": ";
        formatObject(st, items[static_cast<size_t>(i)].second, out,
                     innerIndent + static_cast<long long>(keyRep.repr.size()) + 2,
                     last ? allowance : 1, context, level);
        if (!last) out += delimNewline;
    }
}

// 序列条目格式化
// format sequence items
// ᠳᠠᠷᠠᠯᠠᠯᠳᠠ ᠶᠢᠨ ᠬᠡᠰᠡᠭ
void formatItems(PPState& st, const std::vector<value>& items, std::string& out,
                 long long indent, long long allowance,
                 std::set<const void*>& context, long long level){
    long long innerIndent = indent + st.settings.indent;
    if (st.settings.indent > 1) appendSpaces(out, st.settings.indent - 1);
    std::string delimNewline = ",\n";
    appendSpaces(delimNewline, innerIndent);
    std::string delim;
    long long width = st.settings.width - innerIndent + 1;
    long long maxWidth = width;
    size_t index = 0;
    if (items.empty()) return;
    bool last = false;
    while (!last){
        const value& entry = items[index];
        ++index;
        if (index >= items.size()){
            last = true;
            maxWidth -= allowance;
            width -= allowance;
        }
        if (st.settings.compact){
            std::set<const void*> copy = context;
            SafeResult rep = safeRepr(st, entry, copy,
                                      st.settings.depth > 0 ? st.settings.depth : 0, level);
            if (!rep.readable) st.readable = false;
            if (rep.recursive) st.recursive = true;
            long long w = static_cast<long long>(rep.repr.size()) + 2;
            if (width < w){
                width = maxWidth;
                if (!delim.empty()) delim = delimNewline;
            }
            if (width >= w){
                width -= w;
                out += delim;
                delim = ", ";
                out += rep.repr;
                continue;
            }
        }
        out += delim;
        delim = delimNewline;
        formatObject(st, entry, out, innerIndent,
                     last ? allowance : 1, context, level);
    }
}

// 字典格式化
// pretty-print a dict
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠡᠪᠯᠡᠬᠦ
void pprintDict(PPState& st, const value& object, std::string& out,
                long long indent, long long allowance,
                std::set<const void*>& context, long long level){
    out += "{";
    if (st.settings.indent > 1) appendSpaces(out, st.settings.indent - 1);
    auto* entries = static_cast<dict_t*>(object.ptr);
    if (!entries->empty()){
        std::vector<std::pair<value, value>> items(entries->begin(), entries->end());
        if (st.settings.sortDicts){
            std::stable_sort(items.begin(), items.end(),
                             [&](const std::pair<value, value>& a,
                                 const std::pair<value, value>& b){
                                 if (safeLess(*st.vm, a.first, b.first)) return true;
                                 if (safeLess(*st.vm, b.first, a.first)) return false;
                                 return safeLess(*st.vm, a.second, b.second);
                             });
        }
        formatDictItems(st, items, out, indent, allowance + 1, context, level);
    }
    out += "}";
}

// 列表格式化
// pretty-print a list
// ᠲᠠᠯᠠᠭ᠌ ᠬᠡᠪᠯᠡᠬᠦ
void pprintList(PPState& st, const value& object, std::string& out,
                long long indent, long long allowance,
                std::set<const void*>& context, long long level){
    out += "[";
    auto* items = static_cast<std::vector<value>*>(object.ptr);
    formatItems(st, *items, out, indent, allowance + 1, context, level);
    out += "]";
}

// 元组格式化
// pretty-print a tuple
// ᠬᠣᠶᠠᠳᠠᠯ ᠬᠡᠪᠯᠡᠬᠦ
void pprintTuple(PPState& st, const value& object, std::string& out,
                 long long indent, long long allowance,
                 std::set<const void*>& context, long long level){
    out += "(";
    auto* items = static_cast<std::vector<value>*>(object.ptr);
    std::string endChar = items->size() == 1 ? ",)" : ")";
    formatItems(st, *items, out, indent,
                allowance + static_cast<long long>(endChar.size()), context, level);
    out += endChar;
}

// 字符串 分行格式化
// pretty-print a string with line wrapping
// ᠦᠰᠦᠭ ᠮᠥᠷᠯᠦᠯ
void pprintStr(PPState& st, const value& object, std::string& out,
               long long indent, long long allowance,
               std::set<const void*>&, long long level){
    std::string text = *static_cast<std::string*>(object.ptr);
    if (text.empty()){
        out += stringRepr(*st.vm, text);
        return;
    }
    std::vector<std::string> lines = splitLinesKeep(text);
    if (level == 1){
        indent += 1;
        allowance += 1;
    }
    long long maxWidth = st.settings.width - indent;
    long long maxWidth1 = maxWidth;
    std::vector<std::string> chunks;
    std::string lastRep;
    for (size_t i = 0; i < lines.size(); ++i){
        std::string rep = stringRepr(*st.vm, lines[i]);
        lastRep = rep;
        if (i + 1 == lines.size()) maxWidth1 -= allowance;
        if (static_cast<long long>(rep.size()) <= maxWidth1){
            chunks.push_back(rep);
            continue;
        }
        std::vector<std::string> parts;
        size_t pos = 0;
        while (pos < lines[i].size()){
            size_t begin = pos;
            while (pos < lines[i].size() &&
                   !std::isspace(static_cast<unsigned char>(lines[i][pos]))) ++pos;
            while (pos < lines[i].size() &&
                   std::isspace(static_cast<unsigned char>(lines[i][pos]))) ++pos;
            parts.push_back(lines[i].substr(begin, pos - begin));
        }
        long long maxWidth2 = maxWidth;
        std::string current;
        for (size_t j = 0; j < parts.size(); ++j){
            std::string candidate = current + parts[j];
            if (j + 1 == parts.size() && i + 1 == lines.size()) maxWidth2 -= allowance;
            if (static_cast<long long>(stringRepr(*st.vm, candidate).size()) > maxWidth2){
                if (!current.empty()) chunks.push_back(stringRepr(*st.vm, current));
                current = parts[j];
            } else {
                current = candidate;
            }
        }
        if (!current.empty()) chunks.push_back(stringRepr(*st.vm, current));
    }
    if (chunks.size() == 1){
        out += lastRep;
        return;
    }
    bool parens = level == 1;
    if (parens) out += "(";
    for (size_t i = 0; i < chunks.size(); ++i){
        if (i > 0){
            out += "\n";
            appendSpaces(out, indent);
        }
        out += chunks[i];
    }
    if (parens) out += ")";
}

// 字节串格式化
// pretty-print bytes
// ᠪᠠᠶᠢᠲ᠋ ᠬᠡᠪᠯᠡᠬᠦ
void pprintBytesValue(PPState& st, const std::string& raw,
                      std::string& out, long long indent, long long allowance,
                      long long level){
    if (raw.size() <= 4){
        value partValue = st.vm->makeString(raw);
        partValue.type = "bytes";
        out += pythonRepr(*st.vm, partValue);
        return;
    }
    bool parens = level == 1;
    if (parens){
        indent += 1;
        allowance += 1;
        out += "(";
    }
    long long width = st.settings.width - indent;
    std::string current;
    std::string delim;
    size_t last = raw.size() / 4 * 4;
    for (size_t i = 0; i < raw.size(); i += 4){
        std::string part = raw.substr(i, std::min<size_t>(4, raw.size() - i));
        std::string candidate = current + part;
        if (i == last) width -= allowance;
        value partValue = st.vm->makeString(candidate);
        partValue.type = "bytes";
        if (static_cast<long long>(pythonRepr(*st.vm, partValue).size()) > width){
            if (!current.empty()){
                value chunk = st.vm->makeString(current);
                chunk.type = "bytes";
                out += delim;
                out += pythonRepr(*st.vm, chunk);
                delim = "\n";
                appendSpaces(delim, indent);
            }
            current = part;
        } else {
            current = candidate;
        }
    }
    if (!current.empty()){
        value chunk = st.vm->makeString(current);
        chunk.type = "bytes";
        out += delim;
        out += pythonRepr(*st.vm, chunk);
    }
    if (parens) out += ")";
}

// 安全 repr 计算
// compute safe repr
// ᠠᠶᠤᠯᠭᠦᠢ repr ᠪᠣᠳᠣᠯ
SafeResult safeRepr(PPState& st, const value& object, std::set<const void*>& context,
                    long long maxlevels, long long level){
    SafeResult result;
    if (isScalar(object)){
        if (object.type == "int" && st.settings.underscore){
            result.repr = intUnderscore(st.vm->toInt(object));
        } else {
            result.repr = pythonRepr(*st.vm, object);
        }
        return result;
    }
    if (object.type == "dict"){
        auto* entries = static_cast<dict_t*>(object.ptr);
        if (entries->empty()){
            result.repr = "{}";
            return result;
        }
        const void* objid = object.ptr;
        if (maxlevels > 0 && level >= maxlevels){
            result.repr = "{...}";
            result.readable = false;
            result.recursive = context.count(objid) > 0;
            return result;
        }
        if (context.count(objid)){
            result.repr = recursionText(*st.vm, object);
            result.readable = false;
            result.recursive = true;
            return result;
        }
        context.insert(objid);
        std::string joined;
        std::vector<std::pair<value, value>> items(entries->begin(), entries->end());
        if (st.settings.sortDicts){
            std::stable_sort(items.begin(), items.end(),
                             [&](const std::pair<value, value>& a,
                                 const std::pair<value, value>& b){
                                 if (safeLess(*st.vm, a.first, b.first)) return true;
                                 if (safeLess(*st.vm, b.first, a.first)) return false;
                                 return safeLess(*st.vm, a.second, b.second);
                             });
        }
        long long childLevel = level + 1;
        for (size_t i = 0; i < items.size(); ++i){
            if (i > 0) joined += ", ";
            SafeResult keyRep = safeRepr(st, items[i].first, context, maxlevels, childLevel);
            SafeResult valueRep = safeRepr(st, items[i].second, context, maxlevels, childLevel);
            joined += keyRep.repr + ": " + valueRep.repr;
            if (!keyRep.readable || !valueRep.readable) result.readable = false;
            if (keyRep.recursive || valueRep.recursive) result.recursive = true;
        }
        context.erase(objid);
        result.repr = "{" + joined + "}";
        return result;
    }
    if (object.type == "list" || object.type == "tuple"){
        auto* items = static_cast<std::vector<value>*>(object.ptr);
        std::string openText = object.type == "list" ? "[" : "(";
        std::string closeText;
        if (object.type == "tuple"){
            closeText = items->size() == 1 ? ",)" : ")";
        } else {
            closeText = "]";
        }
        if (items->empty()){
            result.repr = openText + closeText;
            return result;
        }
        const void* objid = object.ptr;
        if (maxlevels > 0 && level >= maxlevels){
            result.repr = openText + "..." + closeText;
            result.readable = false;
            result.recursive = context.count(objid) > 0;
            return result;
        }
        if (context.count(objid)){
            result.repr = recursionText(*st.vm, object);
            result.readable = false;
            result.recursive = true;
            return result;
        }
        context.insert(objid);
        std::string joined;
        long long childLevel = level + 1;
        for (size_t i = 0; i < items->size(); ++i){
            if (i > 0) joined += ", ";
            SafeResult itemRep = safeRepr(st, (*items)[i], context, maxlevels, childLevel);
            joined += itemRep.repr;
            if (!itemRep.readable) result.readable = false;
            if (itemRep.recursive) result.recursive = true;
        }
        context.erase(objid);
        result.repr = openText + joined + closeText;
        return result;
    }
    result.repr = pythonRepr(*st.vm, object);
    result.readable = !result.repr.empty() && result.repr[0] != '<';
    return result;
}

// 递归格式化入口
// recursive formatting entry
// ᠳᠠᠪᠬᠠᠭᠳᠠᠯ ᠬᠡᠪᠯᠡᠯ
void formatObject(PPState& st, const value& object, std::string& out,
                  long long indent, long long allowance,
                  std::set<const void*>& context, long long level){
    const void* objid = object.ptr;
    if (objid && isContainer(object) && context.count(objid)){
        out += recursionText(*st.vm, object);
        st.recursive = true;
        st.readable = false;
        return;
    }
    std::set<const void*> copy = context;
    SafeResult rep = safeRepr(st, object, copy,
                              st.settings.depth > 0 ? st.settings.depth : 0, level);
    if (!rep.readable) st.readable = false;
    if (rep.recursive) st.recursive = true;
    long long maxWidth = st.settings.width - indent - allowance;
    if (static_cast<long long>(rep.repr.size()) > maxWidth){
        if (object.type == "dict"){
            context.insert(objid);
            pprintDict(st, object, out, indent, allowance, context, level + 1);
            context.erase(objid);
            return;
        }
        if (object.type == "list"){
            context.insert(objid);
            pprintList(st, object, out, indent, allowance, context, level + 1);
            context.erase(objid);
            return;
        }
        if (object.type == "tuple"){
            context.insert(objid);
            pprintTuple(st, object, out, indent, allowance, context, level + 1);
            context.erase(objid);
            return;
        }
        if (object.type == "str"){
            pprintStr(st, object, out, indent, allowance, context, level + 1);
            return;
        }
        if (object.type == "bytes" && object.ptr){
            std::string raw = *static_cast<std::string*>(object.ptr);
            pprintBytesValue(st, raw, out, indent, allowance, level + 1);
            return;
        }
        if (object.type == "bytearray" && object.ptr){
            std::string raw = *static_cast<std::string*>(object.ptr);
            out += "bytearray(";
            pprintBytesValue(st, raw, out, indent + 10, allowance + 1, level + 1);
            out += ")";
            return;
        }
    }
    out += rep.repr;
}

// 生成格式化文本
// produce formatted text
// ᠬᠡᠪᠯᠡᠭᠰᠡᠨ ᠪᠢᠴᠢᠭ
std::string formatValue(PPState& st, const value& object){
    std::set<const void*> context;
    std::string out;
    st.readable = true;
    st.recursive = false;
    formatObject(st, object, out, 0, 0, context, 0);
    return out;
}

// 写流
// write to a stream object
// ᠠᠭᠤᠰᠬᠠ ᠳᠤ ᠪᠢᠴᠢᠬᠦ
void writeStream(vm& vm, const value& stream, const std::string& text){
    if (stream.type == "none" || stream.ptr == nullptr){
        std::cout << text;
        return;
    }
    value result;
    if (cffiCallMethod(vm, stream, "write", {vm.makeString(text)}, result)) return;
    vm.clearErr();
    value writer = vm.getAttribute(stream, "write");
    if (vm.hasError()){
        vm.clearErr();
        return;
    }
    vm.callValue(writer, {vm.makeString(text)});
    if (vm.hasError()) vm.clearErr();
}

// 解析设置
// parse settings from arguments
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠠᠶᠢᠯᠤᠯ
PPSettings settingsFrom(cffi& a, bool hasStream){
    PPSettings settings;
    size_t streamOffset = hasStream ? 1 : 0;
    size_t indentIndex = streamOffset + 1;
    size_t widthIndex = indentIndex + 1;
    size_t depthIndex = widthIndex + 1;
    if (a.argc() > indentIndex) settings.indent = a.arg_int(indentIndex);
    if (a.argc() > widthIndex && a.arg(widthIndex).type != "none")
        settings.width = a.arg_int(widthIndex);
    if (a.argc() > depthIndex && a.arg(depthIndex).type != "none")
        settings.depth = a.arg_int(depthIndex);
    if (a.has_kwarg("indent")) settings.indent = a.kwarg_int("indent", settings.indent);
    if (a.has_kwarg("width")) settings.width = a.kwarg_int("width", settings.width);
    if (a.has_kwarg("depth") && a.kwarg("depth").type != "none")
        settings.depth = a.kwarg_int("depth", settings.depth);
    settings.compact = a.kwarg_bool("compact", false);
    settings.sortDicts = a.kwarg_bool("sort_dicts", true);
    settings.underscore = a.kwarg_bool("underscore_numbers", false);
    return settings;
}

// 从关键字或位置参数取设置 (兼容 PrettyPrinter 构造)
// settings from PrettyPrinter constructor
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ ᠶᠢᠨ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
bool readPrinterSettings(cffi& a, PPSettings& settings, value& stream, bool& hasStream){
    settings = PPSettings();
    if (a.argc() >= 1) settings.indent = a.arg_int(0);
    if (a.argc() >= 2) settings.width = a.arg_int(1);
    if (a.argc() >= 3 && a.arg(2).type != "none") settings.depth = a.arg_int(2);
    if (a.argc() >= 4){
        stream = a.arg(3);
        hasStream = stream.type != "none" && stream.ptr != nullptr;
    }
    if (a.has_kwarg("indent")) settings.indent = a.kwarg_int("indent", settings.indent);
    if (a.has_kwarg("width")) settings.width = a.kwarg_int("width", settings.width);
    if (a.has_kwarg("depth") && a.kwarg("depth").type != "none")
        settings.depth = a.kwarg_int("depth", settings.depth);
    if (a.has_kwarg("stream")){
        stream = a.kwarg("stream");
        hasStream = stream.type != "none" && stream.ptr != nullptr;
    }
    settings.compact = a.kwarg_bool("compact", false);
    settings.sortDicts = a.kwarg_bool("sort_dicts", true);
    settings.underscore = a.kwarg_bool("underscore_numbers", false);
    if (settings.indent < 0){
        a.vm->raiseError("ValueError", "indent must be >= 0");
        return false;
    }
    if (settings.depth == 0 || (a.has_kwarg("depth") && settings.depth <= 0)){
        a.vm->raiseError("ValueError", "depth must be > 0");
        return false;
    }
    if (settings.width == 0){
        a.vm->raiseError("ValueError", "width must be != 0");
        return false;
    }
    return true;
}

// 从实例取状态
// get state from instance
// ᠶᠢᠨᠢᠰ ᠡᠴᠡ ᠪᠠᠶᠢᠳᠠᠯ
PPState* stateOf(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        return static_cast<PPState*>(inst->handle);
    }
    return nullptr;
}

}

// pprint
// pprint
// pprint
static value fnPprint(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "pprint() missing 1 required positional argument: 'object'");
        return a.none();
    }
    PPState st;
    st.vm = a.vm;
    st.settings = settingsFrom(a, true);
    if (a.argc() > 1){
        st.stream = a.arg(1);
        st.hasStream = st.stream.type != "none" && st.stream.ptr != nullptr;
    }
    std::string text = formatValue(st, a.arg(0));
    if (st.hasStream) writeStream(*a.vm, st.stream, text + "\n");
    else std::cout << text << "\n";
    return a.none();
}

// pformat
// pformat
// pformat
static value fnPformat(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "pformat() missing 1 required positional argument: 'object'");
        return a.none();
    }
    PPState st;
    st.vm = a.vm;
    st.settings = settingsFrom(a, false);
    return a.mkstr(formatValue(st, a.arg(0)));
}

// pp
// pp
// pp
static value fnPp(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "pp() missing 1 required positional argument: 'object'");
        return a.none();
    }
    PPState st;
    st.vm = a.vm;
    if (a.argc() > 1 && a.arg(1).type != "none") st.stream = a.arg(1);
    if (a.argc() > 2) st.settings.indent = a.arg_int(2);
    if (a.argc() > 3) st.settings.width = a.arg_int(3);
    if (a.argc() > 4 && a.arg(4).type != "none") st.settings.depth = a.arg_int(4);
    if (a.has_kwarg("indent")) st.settings.indent = a.kwarg_int("indent", st.settings.indent);
    if (a.has_kwarg("width")) st.settings.width = a.kwarg_int("width", st.settings.width);
    if (a.has_kwarg("depth") && a.kwarg("depth").type != "none")
        st.settings.depth = a.kwarg_int("depth", st.settings.depth);
    st.settings.sortDicts = a.kwarg_bool("sort_dicts", false);
    st.settings.compact = a.kwarg_bool("compact", false);
    st.settings.underscore = a.kwarg_bool("underscore_numbers", false);
    std::string text = formatValue(st, a.arg(0));
    if (st.stream.type != "none" && st.stream.ptr != nullptr){
        writeStream(*a.vm, st.stream, text + "\n");
    } else {
        std::cout << text << "\n";
    }
    return a.none();
}

// saferepr
// saferepr
// saferepr
static value fnSaferepr(cffi& a){
    PPState st;
    st.vm = a.vm;
    std::set<const void*> context;
    SafeResult result = safeRepr(st, a.argc() ? a.arg(0) : a.none(), context, 0, 0);
    return a.mkstr(result.repr);
}

// isreadable
// isreadable
// isreadable
static value fnIsreadable(cffi& a){
    PPState st;
    st.vm = a.vm;
    std::set<const void*> context;
    SafeResult result = safeRepr(st, a.argc() ? a.arg(0) : a.none(), context, 0, 0);
    return a.mkbool(result.readable && !result.recursive);
}

// isrecursive
// isrecursive
// isrecursive
static value fnIsrecursive(cffi& a){
    PPState st;
    st.vm = a.vm;
    std::set<const void*> context;
    SafeResult result = safeRepr(st, a.argc() ? a.arg(0) : a.none(), context, 0, 0);
    return a.mkbool(result.recursive);
}

// PrettyPrinter 构造
// PrettyPrinter constructor
// PrettyPrinter ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnPrinterConstructor(cffi& a){
    PPState* st = new PPState();
    st->vm = a.vm;
    value stream;
    bool hasStream = false;
    if (!readPrinterSettings(a, st->settings, stream, hasStream)){
        delete st;
        return a.none();
    }
    st->stream = stream;
    st->hasStream = hasStream;
    return a.mkobject(st);
}

// PrettyPrinter.pformat
// PrettyPrinter.pformat
// PrettyPrinter.pformat
static value fnPrinterPformat(cffi& a){
    PPState* st = stateOf(a);
    if (!st) return a.none();
    return a.mkstr(formatValue(*st, a.arg(0)));
}

// PrettyPrinter.pprint
// PrettyPrinter.pprint
// PrettyPrinter.pprint
static value fnPrinterPprint(cffi& a){
    PPState* st = stateOf(a);
    if (!st) return a.none();
    std::string text = formatValue(*st, a.arg(0));
    if (st->hasStream) writeStream(*a.vm, st->stream, text + "\n");
    else std::cout << text << "\n";
    return a.none();
}

// PrettyPrinter.isrecursive
// PrettyPrinter.isrecursive
// PrettyPrinter.isrecursive
static value fnPrinterIsrecursive(cffi& a){
    PPState* st = stateOf(a);
    if (!st) return a.none();
    std::set<const void*> context;
    SafeResult result = safeRepr(*st, a.arg(0), context, 0, 0);
    return a.mkbool(result.recursive);
}

// PrettyPrinter.isreadable
// PrettyPrinter.isreadable
// PrettyPrinter.isreadable
static value fnPrinterIsreadable(cffi& a){
    PPState* st = stateOf(a);
    if (!st) return a.none();
    std::set<const void*> context;
    SafeResult result = safeRepr(*st, a.arg(0), context, 0, 0);
    return a.mkbool(result.readable && !result.recursive);
}

// PrettyPrinter.format
// PrettyPrinter.format
// PrettyPrinter.format
static value fnPrinterFormat(cffi& a){
    PPState* st = stateOf(a);
    if (!st) return a.none();
    std::set<const void*> context;
    long long maxlevels = a.argc() > 2 && a.arg(2).type != "none" ? a.arg_int(2) : 0;
    long long level = a.argc() > 3 ? a.arg_int(3) : 0;
    SafeResult result = safeRepr(*st, a.arg(0), context, maxlevels, level);
    return a.vm->makeTuple({a.mkstr(result.repr), a.mkbool(result.readable),
                            a.mkbool(result.recursive)});
}

// cffi pprint lib
// cffi pprint library
// cffi pprint ᠨᠣᠮ
ffilib cffi_pprint_lib(){
    ffi_class printerClass;
    printerClass.name = "PrettyPrinter";
    printerClass.constructor = (void*)fnPrinterConstructor;
    printerClass.methods = {
        {"pformat", (void*)fnPrinterPformat},
        {"pprint", (void*)fnPrinterPprint},
        {"isrecursive", (void*)fnPrinterIsrecursive},
        {"isreadable", (void*)fnPrinterIsreadable},
        {"format", (void*)fnPrinterFormat}
    };
    return cffi_lib(
        "pprint",
        {
            {"pprint", (void*)fnPprint},
            {"pformat", (void*)fnPformat},
            {"pp", (void*)fnPp},
            {"saferepr", (void*)fnSaferepr},
            {"isreadable", (void*)fnIsreadable},
            {"isrecursive", (void*)fnIsrecursive}
        },
        {},
        {
            {"PrettyPrinter", printerClass}
        });
}

}
