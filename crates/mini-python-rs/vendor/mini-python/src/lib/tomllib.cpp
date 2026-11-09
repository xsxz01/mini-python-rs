#include "../cffi.hpp"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <limits>
#include <vector>

namespace minipy {

namespace {

// TOML 解析器
// TOML parser
// TOML ᠰᠢᠨᠵᠢᠯᠡᠭᠴᠢ
struct TomlParser{
    vm* vmPtr = nullptr;
    std::string text;
    size_t pos = 0;
    int line = 1;
    std::string error;
    std::string errorType = "TOMLDecodeError";

    bool fail(const std::string& message){
        errorType = "TOMLDecodeError";
        error = message + " (at line " + std::to_string(line) + ", column " +
                std::to_string(column()) + ")";
        return false;
    }

    int column() const {
        size_t begin = pos;
        while (begin > 0 && text[begin - 1] != '\n') --begin;
        return static_cast<int>(pos - begin) + 1;
    }

    char peek() const {
        return pos < text.size() ? text[pos] : '\0';
    }

    char peekAt(size_t offset) const {
        return pos + offset < text.size() ? text[pos + offset] : '\0';
    }

    char take(){
        if (pos >= text.size()) return '\0';
        char c = text[pos++];
        if (c == '\n') ++line;
        return c;
    }

    void skipWhitespace(){
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    }

    void skipComment(){
        if (peek() != '#') return;
        while (pos < text.size() && text[pos] != '\n') ++pos;
    }

    void skipWhitespaceAndComment(){
        skipWhitespace();
        skipComment();
    }

    // 跳过空白与注释 (多行数组/表用)
    // skip whitespace, comments and newlines
    // ᠬᠣᠭᠤᠰᠤᠨ ᠠᠷᠢᠯᠭᠠᠬᠤ
    bool skipWsCommentNewline(){
        while (pos < text.size()){
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\r'){
                ++pos;
                continue;
            }
            if (c == '\n'){
                ++pos;
                ++line;
                continue;
            }
            if (c == '#'){
                skipComment();
                continue;
            }
            break;
        }
        return true;
    }

    // 期望换行或文件尾
    // expect newline or EOF
    // ᠮᠥᠷ ᠰᠡᠷᠭᠢᠯᠡᠯᠲᠡ
    bool expectLineEnd(){
        skipWhitespaceAndComment();
        char c = peek();
        if (c == '\0') return true;
        if (c == '\r'){
            if (peekAt(1) == '\n'){
                pos += 2;
                ++line;
                return true;
            }
            return fail("Expected newline or end of document after a statement");
        }
        if (c == '\n'){
            ++pos;
            ++line;
            return true;
        }
        return fail("Expected newline or end of document after a statement");
    }

    // 解析简单键
    // parse a bare or quoted key
    // ᠲᠤᠯᠬᠢᠭᠤᠷ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseKeyPart(std::string& out){
        skipWhitespace();
        char c = peek();
        if (c == '"' || c == '\''){
            bool multiline = peekAt(1) == c && peekAt(2) == c;
            std::string value;
            if (!parseString(c, multiline, value)) return false;
            out = value;
            return true;
        }
        std::string part;
        while (pos < text.size()){
            char ch = text[pos];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-'){
                part.push_back(ch);
                ++pos;
            } else {
                break;
            }
        }
        if (part.empty()){
            fail("Invalid statement");
            return false;
        }
        out = part;
        return true;
    }

    bool parseKey(std::vector<std::string>& parts){
        while (true){
            std::string part;
            if (!parseKeyPart(part)) return false;
            parts.push_back(part);
            skipWhitespace();
            if (peek() == '.'){
                ++pos;
                skipWhitespace();
                continue;
            }
            break;
        }
        return true;
    }

    // 解析字符串
    // parse string value
    // ᠦᠰᠦᠭ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseString(char quote, bool multiline, std::string& out){
        if (multiline) pos += 3;
        else ++pos;
        std::string value;
        while (pos < text.size()){
            char c = take();
            if (multiline){
                if (c == quote && peek() == quote && peekAt(1) == quote){
                    pos += 2;
                    out = value;
                    return true;
                }
                if (c == quote && peek() == quote && peekAt(1) != quote){
                    pos += 1;
                    if (peek() == quote){
                        value.push_back(quote);
                        ++pos;
                    }
                    out = value;
                    return true;
                }
            } else {
                if (c == quote){
                    out = value;
                    return true;
                }
                if (c == '\n' || c == '\r'){
                    return fail("Illegal character");
                }
            }
            if (quote == '"' && c == '\\'){
                char esc = take();
                switch (esc){
                    case 'n': value.push_back('\n'); break;
                    case 't': value.push_back('\t'); break;
                    case 'r': value.push_back('\r'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'u': {
                        unsigned int cp = 0;
                        for (int i = 0; i < 4; ++i){
                            char h = take();
                            int v = hexVal(h);
                            if (v < 0) return fail("Invalid unicode escape");
                            cp = cp * 16 + static_cast<unsigned int>(v);
                        }
                        appendUtf8Codepoint(value, cp);
                        break;
                    }
                    case 'U': {
                        unsigned int cp = 0;
                        for (int i = 0; i < 8; ++i){
                            char h = take();
                            int v = hexVal(h);
                            if (v < 0) return fail("Invalid unicode escape");
                            cp = cp * 16 + static_cast<unsigned int>(v);
                        }
                        appendUtf8Codepoint(value, cp);
                        break;
                    }
                    case '\n': {
                        skipWsCommentNewlineStr();
                        break;
                    }
                    case '\r': {
                        if (peek() == '\n'){ ++pos; }
                        ++line;
                        skipWsCommentNewlineStr();
                        break;
                    }
                    case '\0':
                        return fail("Unterminated string");
                    default:
                        return fail("Invalid escape sequence");
                }
                continue;
            }
            value.push_back(c);
        }
        return fail("Unterminated string");
    }

    static int hexVal(char c){
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static void appendUtf8Codepoint(std::string& out, unsigned int cp){
        if (cp < 0x80){
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800){
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000){
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    void skipWsCommentNewlineStr(){
        while (pos < text.size()){
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\r'){
                ++pos;
            } else if (c == '\n'){
                ++pos;
                ++line;
            } else {
                break;
            }
        }
    }

    // 解析值
    // parse value
    // ᠤᠨ᠎ᠠ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseValue(value& out);

    // 数字解析
    // parse number
    // ᠲᠣᠭ᠎ᠠ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseNumber(const std::string& raw, value& out){
        std::string cleaned;
        for (char c : raw) if (c != '_') cleaned.push_back(c);
        std::string lower = cleaned;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == "inf" || lower == "+inf"){
            out = vmPtr->makeFloat(std::numeric_limits<double>::infinity());
            return true;
        }
        if (lower == "-inf"){
            out = vmPtr->makeFloat(-std::numeric_limits<double>::infinity());
            return true;
        }
        if (lower == "nan" || lower == "+nan" || lower == "-nan"){
            out = vmPtr->makeFloat(std::numeric_limits<double>::quiet_NaN());
            return true;
        }
        if (lower.size() > 2 && lower[0] == '0' &&
            (lower[1] == 'x' || lower[1] == 'o' || lower[1] == 'b')){
            int base = lower[1] == 'o' ? 8 : (lower[1] == 'b' ? 2 : 16);
            long long number = 0;
            for (size_t i = 2; i < lower.size(); ++i){
                int v = hexVal(lower[i]);
                if (v < 0 || v >= base) return fail("Invalid value");
                number = number * base + v;
            }
            out = vmPtr->makeInt(number);
            return true;
        }
        bool isFloat = lower.find('.') != std::string::npos ||
                       lower.find('e') != std::string::npos;
        if (isFloat){
            char* end = nullptr;
            double number = std::strtod(cleaned.c_str(), &end);
            if (end != cleaned.c_str() + cleaned.size())
                return fail("Invalid value");
            out = vmPtr->makeFloat(number);
            return true;
        }
        try {
            long long number = std::stoll(cleaned);
            out = vmPtr->makeInt(number);
            return true;
        } catch (...){
            return fail("Invalid value");
        }
    }

    // 数组解析
    // parse array
    // ᠳᠠᠷᠠᠯᠠᠯᠳᠠ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseArray(std::vector<value>& items){
        ++pos;
        skipWsCommentNewline();
        if (peek() == ']'){
            ++pos;
            return true;
        }
        while (true){
            value item;
            if (!parseValue(item)) return false;
            items.push_back(item);
            skipWsCommentNewline();
            char c = peek();
            if (c == ','){
                ++pos;
                skipWsCommentNewline();
                if (peek() == ']'){
                    ++pos;
                    return true;
                }
                continue;
            }
            if (c == ']'){
                ++pos;
                return true;
            }
            return fail("Expected ']' at the end of an array declaration");
        }
    }

    // 内联表解析
    // parse inline table
    // ᠳᠣᠲᠣᠷ᠎ᠠ ᠬᠦᠰᠦᠨᠦᠭ
    bool parseInlineTable(value& out){
        ++pos;
        skipWhitespace();
        value dict = vmPtr->makeDict({});
        if (peek() == '}'){
            ++pos;
            out = dict;
            return true;
        }
        while (true){
            std::vector<std::string> parts;
            if (!parseKey(parts)) return false;
            skipWhitespace();
            if (peek() != '=') return fail("Invalid statement");
            ++pos;
            skipWhitespace();
            value item;
            if (!parseValue(item)) return false;
            if (!setNested(dict, parts, item, true)) return false;
            skipWhitespace();
            char c = peek();
            if (c == ','){
                ++pos;
                skipWhitespace();
                continue;
            }
            if (c == '}'){
                ++pos;
                out = dict;
                return true;
            }
            return fail("Invalid statement");
        }
    }

    // 嵌套赋值
    // set nested key
    // ᠬᠠᠲᠠᠭᠠᠯᠠᠭᠤᠯᠤᠯ
    bool setNested(value& dict, const std::vector<std::string>& parts, const value& item,
                   bool inlineTable){
        value current = dict;
        for (size_t i = 0; i < parts.size(); ++i){
            bool last = i + 1 == parts.size();
            auto* entries = static_cast<dict_t*>(current.ptr);
            const value* existing = nullptr;
            for (auto& entry : *entries){
                if (entry.first.type == "str" && entry.first.ptr &&
                    *static_cast<std::string*>(entry.first.ptr) == parts[i]){
                    existing = &entry.second;
                    break;
                }
            }
            if (last){
                if (existing){
                    return fail("Cannot overwrite a value");
                }
                std::string err = vmPtr->dictSet(current, vmPtr->makeString(parts[i]), item);
                if (!err.empty()) return fail(err);
                return true;
            }
            if (existing){
                if (existing->type == "dict"){
                    current = *existing;
                    continue;
                }
                if (existing->type == "list" && existing->ptr){
                    auto* list = static_cast<std::vector<value>*>(existing->ptr);
                    if (!list->empty() && list->back().type == "dict"){
                        current = list->back();
                        continue;
                    }
                }
                return fail("Cannot overwrite a value");
            }
            value next = vmPtr->makeDict({});
            std::string err = vmPtr->dictSet(current, vmPtr->makeString(parts[i]), next);
            if (!err.empty()) return fail(err);
            current = next;
        }
        (void)inlineTable;
        return true;
    }

    // 解析日期时间 (简化: 保持字符串)
    // parse datetime as string
    // ᠡᠳᠦᠷ ᠴᠠᠭ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseDatetime(const std::string& raw, value& out){
        out = vmPtr->makeString(raw);
        return true;
    }

    // 收集原始标量文本
    // collect scalar text
    // ᠲᠡᠮᠳᠡᠭ ᠬᠤᠪᠢᠶ᠎ᠠ
    std::string collectScalar(){
        size_t start = pos;
        while (pos < text.size()){
            char c = text[pos];
            if (c == '\n' || c == '\r' || c == '#' || c == ',' || c == ']' || c == '}')
                break;
            ++pos;
        }
        std::string raw = text.substr(start, pos - start);
        while (!raw.empty() &&
               (raw.back() == ' ' || raw.back() == '\t')) raw.pop_back();
        return raw;
    }

    // 解析顶层文档
    // parse document
    // ᠪᠢᠴᠢᠭ ᠬᠠᠶᠢᠯᠤᠯ
    bool parseDocument(value& root){
        root = vmPtr->makeDict({});
        value current = root;
        while (pos < text.size()){
            skipWhitespaceAndComment();
            char c = peek();
            if (c == '\0') break;
            if (c == '\n' || c == '\r'){
                if (!expectLineEnd()) return false;
                continue;
            }
            if (c == '['){
                if (peekAt(1) == '['){
                    pos += 2;
                    std::vector<std::string> parts;
                    if (!parseKey(parts)) return false;
                    skipWhitespace();
                    if (peek() != ']' || peekAt(1) != ']')
                        return fail("Expected ']]' at the end of an array declaration");
                    pos += 2;
                    if (!expectLineEnd()) return false;
                    if (!createTableArray(root, parts, current)) return false;
                    continue;
                }
                ++pos;
                std::vector<std::string> parts;
                if (!parseKey(parts)) return false;
                skipWhitespace();
                if (peek() != ']') return fail("Invalid statement");
                ++pos;
                if (!expectLineEnd()) return false;
                if (!createTable(root, parts, current)) return false;
                continue;
            }
            std::vector<std::string> parts;
            if (!parseKey(parts)) return false;
            skipWhitespace();
            if (peek() != '=') return fail("Invalid statement");
            ++pos;
            skipWhitespace();
            value item;
            if (!parseValue(item)) return false;
            if (!setNested(current, parts, item, false)) return false;
            if (!expectLineEnd()) return false;
        }
        return true;
    }

    bool createTable(value& root, const std::vector<std::string>& parts, value& current){
        value node = root;
        for (size_t i = 0; i < parts.size(); ++i){
            auto* entries = static_cast<dict_t*>(node.ptr);
            value* existing = nullptr;
            for (auto& entry : *entries){
                if (entry.first.type == "str" && entry.first.ptr &&
                    *static_cast<std::string*>(entry.first.ptr) == parts[i]){
                    existing = &entry.second;
                    break;
                }
            }
            bool last = i + 1 == parts.size();
            if (last){
                if (existing){
                    if (existing->type == "dict"){
                        current = *existing;
                        return true;
                    }
                    return fail("Cannot overwrite a value");
                }
                value dict = vmPtr->makeDict({});
                std::string err = vmPtr->dictSet(node, vmPtr->makeString(parts[i]), dict);
                if (!err.empty()) return fail(err);
                current = dict;
                return true;
            }
            if (existing){
                if (existing->type == "dict"){
                    node = *existing;
                    continue;
                }
                if (existing->type == "list" && existing->ptr){
                    auto* list = static_cast<std::vector<value>*>(existing->ptr);
                    if (!list->empty() && list->back().type == "dict"){
                        node = list->back();
                        continue;
                    }
                }
                return fail("Cannot overwrite a value");
            }
            value dict = vmPtr->makeDict({});
            std::string err = vmPtr->dictSet(node, vmPtr->makeString(parts[i]), dict);
            if (!err.empty()) return fail(err);
            node = dict;
        }
        return true;
    }

    bool createTableArray(value& root, const std::vector<std::string>& parts, value& current){
        value node = root;
        for (size_t i = 0; i < parts.size(); ++i){
            auto* entries = static_cast<dict_t*>(node.ptr);
            value* existing = nullptr;
            for (auto& entry : *entries){
                if (entry.first.type == "str" && entry.first.ptr &&
                    *static_cast<std::string*>(entry.first.ptr) == parts[i]){
                    existing = &entry.second;
                    break;
                }
            }
            bool last = i + 1 == parts.size();
            if (last){
                if (!existing){
                    value list = vmPtr->makeList({});
                    std::string err = vmPtr->dictSet(node, vmPtr->makeString(parts[i]), list);
                    if (!err.empty()) return fail(err);
                    value dict = vmPtr->makeDict({});
                    static_cast<std::vector<value>*>(list.ptr)->push_back(dict);
                    current = dict;
                    return true;
                }
                if (existing->type != "list"){
                    return fail("Cannot overwrite a value");
                }
                auto* list = static_cast<std::vector<value>*>(existing->ptr);
                value dict = vmPtr->makeDict({});
                list->push_back(dict);
                current = dict;
                return true;
            }
            if (existing){
                if (existing->type == "dict"){
                    node = *existing;
                    continue;
                }
                if (existing->type == "list" && existing->ptr){
                    auto* list = static_cast<std::vector<value>*>(existing->ptr);
                    if (!list->empty() && list->back().type == "dict"){
                        node = list->back();
                        continue;
                    }
                }
                return fail("Cannot overwrite a value");
            }
            value dict = vmPtr->makeDict({});
            std::string err = vmPtr->dictSet(node, vmPtr->makeString(parts[i]), dict);
            if (!err.empty()) return fail(err);
            node = dict;
        }
        return true;
    }
};

bool TomlParser::parseValue(value& out){
    skipWhitespace();
    char c = peek();
    if (c == '\0') return fail("Invalid value");
    if (c == '"' || c == '\''){
        bool multiline = peekAt(1) == c && peekAt(2) == c;
        std::string value;
        if (!parseString(c, multiline, value)) return false;
        out = vmPtr->makeString(value);
        return true;
    }
    if (c == '['){
        std::vector<value> items;
        if (!parseArray(items)) return false;
        out = vmPtr->makeList(items);
        return true;
    }
    if (c == '{'){
        return parseInlineTable(out);
    }
    std::string raw = collectScalar();
    if (raw.empty()) return fail("Invalid value");
    if (raw == "true"){ out = vmPtr->makeBool(true); return true; }
    if (raw == "false"){ out = vmPtr->makeBool(false); return true; }
    bool datetimeLike = raw.size() >= 10 && std::isdigit((unsigned char)raw[0]) &&
                        std::isdigit((unsigned char)raw[1]) &&
                        (raw[4] == '-' || raw.find(':') != std::string::npos);
    if (datetimeLike && raw.find_first_not_of("0123456789TtZz:+-.") == std::string::npos){
        return parseDatetime(raw, out);
    }
    return parseNumber(raw, out);
}

}

// loads
// loads
// loads
static value fnTomlLoads(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "loads() missing required argument: 's'");
        return a.none();
    }
    TomlParser parser;
    parser.vmPtr = a.vm;
    value input = a.arg(0);
    if (input.type == "str" && input.ptr){
        parser.text = *static_cast<std::string*>(input.ptr);
    } else if (input.type == "bytes"){
        a.vm->raiseError("TypeError", "Expected str object, not 'bytes'");
        return a.none();
    } else {
        a.vm->raiseError("TypeError", "Expected str object, not '" +
                                      a.vm->pythonTypeName(input) + "'");
        return a.none();
    }
    value root;
    if (!parser.parseDocument(root)){
        a.vm->raiseError("TOMLDecodeError", parser.error);
        return a.none();
    }
    return root;
}

// load
// load
// load
static value fnTomlLoad(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "load() missing required argument: 'fp'");
        return a.none();
    }
    value result;
    if (!cffiCallMethod(*a.vm, a.arg(0), "read", {}, result)){
        a.vm->raiseError("TypeError", "File must be opened in binary mode, e.g. use `open('foo.toml', 'rb')`");
        return a.none();
    }
    if (result.type != "str" && result.type != "bytes"){
        a.vm->raiseError("TypeError", "File must be opened in binary mode, e.g. use `open('foo.toml', 'rb')`");
        return a.none();
    }
    TomlParser parser;
    parser.vmPtr = a.vm;
    parser.text = *static_cast<std::string*>(result.ptr);
    value root;
    if (!parser.parseDocument(root)){
        a.vm->raiseError("TOMLDecodeError", parser.error);
        return a.none();
    }
    return root;
}

// cffi tomllib 库
// cffi tomllib library
// cffi tomllib ᠨᠣᠮ
ffilib cffi_tomllib_lib(){
    ffi_class decodeErrorClass;
    decodeErrorClass.name = "TOMLDecodeError";
    return cffi_lib(
        "tomllib",
        {
            {"load", (void*)fnTomlLoad},
            {"loads", (void*)fnTomlLoads}
        },
        {},
        {
            {"TOMLDecodeError", decodeErrorClass}
        });
}

}
