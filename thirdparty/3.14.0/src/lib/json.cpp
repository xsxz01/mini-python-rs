#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace minipy {






namespace {

// decode Utf8
// decode utf8
// decode Utf8
long long decodeUtf8(const std::string& text, size_t& pos){
    if (pos >= text.size()) return -1;
    unsigned char c = static_cast<unsigned char>(text[pos]);
    if (c < 0x80){
        pos += 1;
        return c;
    }
    size_t extra = 0;
    long long code = 0;
    if ((c & 0xE0) == 0xC0){ extra = 1; code = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0){ extra = 2; code = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0){ extra = 3; code = c & 0x07; }
    else { pos += 1; return -1; }
    if (pos + extra >= text.size()){ pos += 1; return -1; }
    for (size_t i = 1; i <= extra; ++i){
        unsigned char next = static_cast<unsigned char>(text[pos + i]);
        if ((next & 0xC0) != 0x80){ pos += 1; return -1; }
        code = (code << 6) | (next & 0x3F);
    }
    pos += extra + 1;
    return code;
}

// escape Json
// escape json
// escape Json
std::string escapeJson(const std::string& text, bool ensureAscii){
    std::string result = "\"";
    size_t pos = 0;
    while (pos < text.size()){
        char c = text[pos];
        switch (c){
            case '"':  result += "\\\""; pos++; continue;
            case '\\': result += "\\\\"; pos++; continue;
            case '\b': result += "\\b"; pos++; continue;
            case '\f': result += "\\f"; pos++; continue;
            case '\n': result += "\\n"; pos++; continue;
            case '\r': result += "\\r"; pos++; continue;
            case '\t': result += "\\t"; pos++; continue;
            default: break;
        }
        unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20){
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", byte);
            result += buffer;
            pos++;
            continue;
        }
        if (!ensureAscii || byte < 0x80){
            result += c;
            pos++;
            continue;
        }

        size_t start = pos;
        long long code = decodeUtf8(text, pos);
        if (code < 0){
            pos = start + 1;
            continue;
        }
        char buffer[16];
        if (code <= 0xFFFF){
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(code));
            result += buffer;
        } else {
            long long offset = code - 0x10000;
            unsigned int high = static_cast<unsigned int>(0xD800 + (offset >> 10));
            unsigned int low = static_cast<unsigned int>(0xDC00 + (offset & 0x3FF));
            std::snprintf(buffer, sizeof(buffer), "\\u%04x\\u%04x", high, low);
            result += buffer;
        }
    }
    result += "\"";
    return result;
}

// indent Newline
// indent newline
// indent Newline
std::string indentNewline(int indent, int depth){
    if (indent <= 0) return "";
    std::string result = "\n";
    result += std::string(static_cast<size_t>(indent) * static_cast<size_t>(depth), ' ');
    return result;
}
bool dumpValue(minipy::vm& vm, const minipy::value& value, std::string& out, std::string& error,
               int indent, int depth, bool ensureAscii, bool sortKeys,
               const std::string& itemSeparator, const std::string& keySeparator){
    if (value.type == "none"){
        out += "null";
        return true;
    }
    if (value.type == "bool"){
        out += vm.truthy(value) ? "true" : "false";
        return true;
    }
    if (value.type == "int"){ out += vm.toString(value); return true; }
    if (value.type == "float"){
        double number = vm.toFloat(value);
        if (std::isnan(number)){ out += "NaN"; return true; }
        if (std::isinf(number)){ out += number < 0 ? "-Infinity" : "Infinity"; return true; }
        out += vm.toString(value);
        return true;
    }
    if (value.type == "str"){ out += escapeJson(vm.toString(value), ensureAscii); return true; }
    if (value.type == "list" || value.type == "tuple"){
        auto* list = static_cast<std::vector<minipy::value>*>(value.ptr);
        if (list->empty()){ out += "[]"; return true; }
        out += "[";
        for (size_t i = 0; i < list->size(); ++i){
            if (i > 0) out += itemSeparator;
            out += indentNewline(indent, depth + 1);
            if (!dumpValue(vm, (*list)[i], out, error, indent, depth + 1,
                           ensureAscii, sortKeys, itemSeparator, keySeparator)) return false;
        }
        out += indentNewline(indent, depth);
        out += "]";
        return true;
    }
    if (value.type == "dict"){
        auto* items = static_cast<minipy::dict_t*>(value.ptr);
        if (items->empty()){ out += "{}"; return true; }
        std::vector<size_t> order;
        order.reserve(items->size());
        for (size_t i = 0; i < items->size(); ++i) order.push_back(i);
        if (sortKeys){
            std::stable_sort(order.begin(), order.end(), [&](size_t left, size_t right){
                return vm.toString((*items)[left].first) < vm.toString((*items)[right].first);
            });
        }
        out += "{";
        for (size_t i = 0; i < order.size(); ++i){
            if (i > 0) out += itemSeparator;
            out += indentNewline(indent, depth + 1);
            const minipy::value& key = (*items)[order[i]].first;
            if (key.type == "str"){
                out += escapeJson(vm.toString(key), ensureAscii);
            } else if (key.type == "none"){
                out += escapeJson("null", ensureAscii);
            } else if (key.type == "bool"){
                out += escapeJson(vm.truthy(key) ? "true" : "false", ensureAscii);
            } else if (key.type == "int" || key.type == "float"){
                out += escapeJson(vm.toString(key), ensureAscii);
            } else {
                error = "keys must be str, int, float, bool or None, not " +
                        (key.type == "none" ? std::string("NoneType") : key.type);
                return false;
            }
            out += keySeparator;
            if (!dumpValue(vm, (*items)[order[i]].second, out, error, indent, depth + 1,
                           ensureAscii, sortKeys, itemSeparator, keySeparator)) return false;
        }
        out += indentNewline(indent, depth);
        out += "}";
        return true;
    }
    error = "Object of type " + (value.type == "object" ? "object" : value.type) +
            " is not JSON serializable";
    return false;
}

// Json Reader
// json reader
// Json Reader
struct JsonReader{
    const std::string& text;
    size_t pos = 0;
    minipy::vm& vm;
    std::string error;
    JsonReader(const std::string& text, minipy::vm& machine) : text(text), vm(machine) {}

// skip Space
// skip space
// skip Space
    void skipSpace(){
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' ||
                text[pos] == '\n' || text[pos] == '\r')) pos++;
    }

// fail
// fail
// fail
    bool fail(const std::string& message){
        error = message;
        return false;
    }

// 解析 字符串
// parse string
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠦᠰᠦᠭ
    bool parseString(std::string& out){
        if (pos >= text.size() || text[pos] != '"') return fail("Expecting value");
        pos++;
        while (pos < text.size()){
            char c = text[pos++];
            if (c == '"') return true;
            if (c != '\\'){ out += c; continue; }
            if (pos >= text.size()) return fail("Invalid \\escape");
            char escaped = text[pos++];
            switch (escaped){
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (pos + 4 > text.size()) return fail("Invalid \\uXXXX escape");
                    std::string hex = text.substr(pos, 4);
                    pos += 4;
                    int code = std::stoi(hex, nullptr, 16);
                    if (code < 0x80){
                        out += static_cast<char>(code);
                    } else if (code < 0x800){
                        out += static_cast<char>(0xC0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default: return fail("Invalid \\escape");
            }
        }
        return fail("Unterminated string starting at");
    }

// 解析 值
// parse value
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠭᠠᠷᠴᠠᠭ
    bool parseValue(minipy::value& out){
        skipSpace();
        if (pos >= text.size()) return fail("Expecting value");
        char c = text[pos];
        if (c == '"'){
            std::string value;
            if (!parseString(value)) return false;
            out = vm.makeString(value);
            return true;
        }
        if (c == '['){
            pos++;
            std::vector<minipy::value> items;
            skipSpace();
            if (pos < text.size() && text[pos] == ']'){ pos++; out = vm.makeList(items); return true; }
            while (true){
                minipy::value item;
                if (!parseValue(item)) return false;
                items.push_back(item);
                skipSpace();
                if (pos < text.size() && text[pos] == ','){ pos++; continue; }
                if (pos < text.size() && text[pos] == ']'){ pos++; break; }
                return fail("Expecting ',' delimiter");
            }
            out = vm.makeList(items);
            return true;
        }
        if (c == '{'){
            pos++;
            minipy::dict_t pairs;
            skipSpace();
            if (pos < text.size() && text[pos] == '}'){ pos++; out = vm.makeDict(pairs); return true; }
            while (true){
                skipSpace();
                std::string key;
                if (!parseString(key)) return false;
                skipSpace();
                if (pos >= text.size() || text[pos] != ':') return fail("Expecting ':' delimiter");
                pos++;
                minipy::value value;
                if (!parseValue(value)) return false;
                pairs.push_back({vm.makeString(key), value});
                skipSpace();
                if (pos < text.size() && text[pos] == ','){ pos++; continue; }
                if (pos < text.size() && text[pos] == '}'){ pos++; break; }
                return fail("Expecting ',' delimiter");
            }
            out = vm.makeDict(pairs);
            return true;
        }
        if (text.compare(pos, 4, "true") == 0){ pos += 4; out = vm.makeBool(true); return true; }
        if (text.compare(pos, 5, "false") == 0){ pos += 5; out = vm.makeBool(false); return true; }
        if (text.compare(pos, 4, "null") == 0){ pos += 4; out = vm.makeNone(); return true; }
        size_t begin = pos;
        if (text[pos] == '-') pos++;
        while (pos < text.size() &&
               (std::isdigit(static_cast<unsigned char>(text[pos])) ||
                text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' ||
                text[pos] == '+' || text[pos] == '-')) pos++;
        if (begin == pos || pos == begin) return fail("Expecting value");
        std::string number = text.substr(begin, pos - begin);
        bool isFloat = number.find('.') != std::string::npos ||
                       number.find('e') != std::string::npos ||
                       number.find('E') != std::string::npos;
        try {
            if (isFloat) out = vm.makeFloat(std::stod(number));
            else out = vm.makeInt(std::stoll(number));
        } catch (...){
            return fail("Expecting value");
        }
        return true;
    }
};

}

// fn Dumps
// fn dumps
// fn Dumps
static minipy::value fnDumps(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    std::string out;
    std::string error;
    int indent = 0;
    if (a.has_kwarg("indent")){
        minipy::value indentValue = a.kwarg("indent");
        if (indentValue.type != "none"){
            if (indentValue.type == "str"){
                indent = static_cast<int>(vm.toString(indentValue).size());
            } else if (indentValue.type == "int" || indentValue.type == "bool"){
                indent = static_cast<int>(vm.toInt(indentValue));
            }
            if (indent <= 0) indent = 0;
        }
    }
    bool ensureAscii = a.has_kwarg("ensure_ascii") ? a.kwarg_bool("ensure_ascii", true) : true;
    bool sortKeys = a.has_kwarg("sort_keys") ? a.kwarg_bool("sort_keys", false) : false;

    std::string itemSeparator = indent > 0 ? "," : ", ";
    std::string keySeparator = ": ";
    if (a.has_kwarg("separators")){
        minipy::value separators = a.kwarg("separators");
        if (separators.type == "none"){

        } else if (separators.type == "list" || separators.type == "tuple"){
            auto* items = static_cast<std::vector<minipy::value>*>(separators.ptr);
            if (items->size() != 2){
                vm.raiseError("ValueError", "separators must be a 2-item sequence of strings");
                return a.none();
            }
            if ((*items)[0].type != "str" || (*items)[1].type != "str"){
                vm.raiseError("TypeError", "separators must be a 2-item sequence of strings");
                return a.none();
            }
            itemSeparator = vm.toString((*items)[0]);
            keySeparator = vm.toString((*items)[1]);
        } else {
            vm.raiseError("TypeError", "separators must be a tuple or list of two strings");
            return a.none();
        }
    }
    if (!dumpValue(vm, a.arg(0), out, error, indent, 0, ensureAscii, sortKeys,
                   itemSeparator, keySeparator)){
        vm.raiseError("TypeError", error);
        return a.none();
    }
    return a.mkstr(out);
}

// fn Loads
// fn loads
// fn Loads
static minipy::value fnLoads(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    std::string text = a.arg_str(0);
    JsonReader reader(text, vm);
    minipy::value value;
    if (!reader.parseValue(value)){
        vm.raiseError("ValueError", reader.error);
        return a.none();
    }
    reader.skipSpace();
    if (reader.pos < text.size()){
        vm.raiseError("ValueError", "Extra data");
        return a.none();
    }
    return value;
}

// cffi json lib
// cffi json lib
// cffi json lib
minipy::ffilib cffi_json_lib(){
    return minipy::cffi_lib(
        "json",
        {
            {"dumps", (void*)fnDumps},
            {"loads", (void*)fnLoads}
        });
}


}
