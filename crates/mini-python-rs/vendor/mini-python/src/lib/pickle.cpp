#include "../cffi.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>

namespace minipy {

// 读取 bytes/str 原始字节
// read raw bytes of bytes/str
// bytes/str ᠦᠨ ᠬᠠᠭᠤᠴᠢᠨ ᠪᠠᠶᠢᠲ᠋
static std::string rawOf(const value& v){
    if ((v.type == "bytes" || v.type == "bytearray" || v.type == "str") && v.ptr)
        return *static_cast<std::string*>(v.ptr);
    return "";
}

// 构造 bytes 值
// make bytes value
// bytes ᠪᠠᠶᠢᠳᠠᠯ ᠪᠣᠯᠭᠠᠬᠤ
static value mkBytes(cffi& a, const std::string& raw){
    value v = a.mkstr(raw);
    v.type = "bytes";
    return v;
}

// 构造 bytearray 值
// make bytearray value
// bytearray ᠪᠠᠶᠢᠳᠠᠯ ᠪᠣᠯᠭᠠᠬᠤ
static value mkByteArray(cffi& a, const std::string& raw){
    value v = a.mkstr(raw);
    v.type = "bytearray";
    return v;
}

// UTF-8 编码一个码点
// encode one codepoint as UTF-8
// UTF-8 ᠬᠢᠯᠪᠢᠯᠲᠡ
static void utf8Encode(std::string& out, unsigned int cp){
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

// UTF-8 解码下一个码点
// decode next UTF-8 codepoint
// UTF-8 ᠳ᠋ᠢᠺᠣᠳ
static unsigned int utf8Decode(const std::string& s, size_t& i){
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80){ i += 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < s.size()){
        unsigned int cp = ((c & 0x1F) << 6) |
                          (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        i += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < s.size()){
        unsigned int cp = ((c & 0x0F) << 12) |
                          ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                          (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        i += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < s.size()){
        unsigned int cp = ((c & 0x07) << 18) |
                          ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                          ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                          (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        i += 4;
        return cp;
    }
    i += 1;
    return c;
}

// raw-unicode-escape 编码 (pickle 协议 0 字符串)
// raw-unicode-escape encode (pickle protocol 0 strings)
// raw-unicode-escape ᠬᠢᠯᠪᠢᠯᠲᠡ
static std::string rawUnicodeEscape(const std::string& utf8){
    static const char* kHex = "0123456789abcdef";
    std::string out;
    size_t i = 0;
    while (i < utf8.size()){
        unsigned int cp = utf8Decode(utf8, i);
        if (cp == 0x5C) out += "\\u005c";
        else if (cp == 0x00) out += "\\u0000";
        else if (cp == 0x0A) out += "\\u000a";
        else if (cp == 0x0D) out += "\\u000d";
        else if (cp == 0x1A) out += "\\u001a";
        else if (cp < 0x100) out.push_back(static_cast<char>(cp));
        else if (cp < 0x10000){
            out += "\\u";
            for (int shift = 12; shift >= 0; shift -= 4)
                out.push_back(kHex[(cp >> shift) & 0xF]);
        } else {
            out += "\\U";
            for (int shift = 28; shift >= 0; shift -= 4)
                out.push_back(kHex[(cp >> shift) & 0xF]);
        }
    }
    return out;
}

// UNICODE 操作码行解码 (latin-1 字节 + \uXXXX)
// decode UNICODE opcode line (latin-1 bytes plus \uXXXX)
// UNICODE ᠳ᠋ᠢᠺᠣᠳ
static std::string decodeUnicodeLine(const std::string& line){
    std::string out;
    size_t i = 0;
    while (i < line.size()){
        char c = line[i];
        if (c == '\\' && i + 1 < line.size() &&
            (line[i + 1] == 'u' || line[i + 1] == 'U')){
            int digits = line[i + 1] == 'u' ? 4 : 8;
            if (i + 1 + static_cast<size_t>(digits) <= line.size()){
                unsigned int cp = 0;
                bool ok = true;
                for (int d = 0; d < digits; ++d){
                    char h = line[i + 2 + static_cast<size_t>(d)];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= static_cast<unsigned int>(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned int>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned int>(h - 'A' + 10);
                    else { ok = false; break; }
                }
                if (ok){
                    utf8Encode(out, cp);
                    i += 2 + static_cast<size_t>(digits);
                    continue;
                }
            }
        }
        utf8Encode(out, static_cast<unsigned char>(c));
        i += 1;
    }
    return out;
}

// latin-1 字节转 UTF-8
// latin-1 bytes to UTF-8
// latin-1 → UTF-8
static std::string utf8FromLatin1(const std::string& raw){
    std::string out;
    for (unsigned char c : raw) utf8Encode(out, c);
    return out;
}

// UTF-8 字符串转 latin-1 字节 (码点须 <= 255)
// UTF-8 string to latin-1 bytes (codepoints must be <= 255)
// UTF-8 → latin-1
static std::string latin1FromUtf8(const std::string& utf8){
    std::string out;
    size_t i = 0;
    while (i < utf8.size()){
        unsigned int cp = utf8Decode(utf8, i);
        out.push_back(static_cast<char>(cp & 0xFF));
    }
    return out;
}

// pickle 写出器 (字节格式与 CPython 一致, 协议 0-5)
// pickle writer (byte-compatible with CPython, protocols 0-5)
// pickle ᠪᠢᠴᠢᠭᠯᠡᠭᠴᠢ
struct PickleWriter {
    vm* machine;
    int proto;
    std::string body;
    std::map<std::string, size_t> memo;
    std::string errType;
    std::string errMsg;

    void u8(unsigned char b){ body.push_back(static_cast<char>(b)); }
    void le16(unsigned int v){ u8(v & 0xFF); u8((v >> 8) & 0xFF); }
    void le32(uint32_t v){
        u8(v & 0xFF); u8((v >> 8) & 0xFF);
        u8((v >> 16) & 0xFF); u8((v >> 24) & 0xFF);
    }
    void beDouble(double d){
        uint64_t bits;
        std::memcpy(&bits, &d, sizeof(bits));
        for (int i = 7; i >= 0; --i) u8(static_cast<unsigned char>((bits >> (8 * i)) & 0xFF));
    }

    bool fail(const std::string& type, const std::string& msg){
        if (errType.empty()){ errType = type; errMsg = msg; }
        return false;
    }

// 结构键 (str/bytes/tuple/complex) 或指针键 (list/dict)
// structural key (str/bytes/tuple/complex) or pointer key (list/dict)
// ᠲᠦᠯᠬᠢᠬᠦᠷ
    std::string keyOf(const value& v){
        if (v.type == "str") return "s" + machine->toString(v);
        if (v.type == "bytes") return "b" + rawOf(v);
        if (v.type == "bytearray") return "y" + rawOf(v);
        if (isComplexValueEx(v)){
            double re = 0.0, im = 0.0;
            complexPartsEx(v, re, im);
            uint64_t rb, ib;
            std::memcpy(&rb, &re, sizeof(rb));
            std::memcpy(&ib, &im, sizeof(ib));
            return "c" + std::to_string(rb) + ":" + std::to_string(ib);
        }
        if (v.type == "tuple" && v.ptr){
            auto* items = static_cast<std::vector<value>*>(v.ptr);
            std::string key = "t(";
            for (size_t i = 0; i < items->size(); ++i){
                if (i) key += ",";
                key += keyOf((*items)[i]);
            }
            key += ")";
            return key;
        }
        if ((v.type == "list" || v.type == "dict") && v.ptr)
            return v.type + "@" + std::to_string(reinterpret_cast<uintptr_t>(v.ptr));
        return "";
    }

// 命中 memo 时写 GET
// emit GET when memo hit
// memo ᠳ᠋ᠤ ᠣᠷᠣᠯᠳᠤᠬᠤ
    bool memoGet(const std::string& key){
        auto it = memo.find(key);
        if (it == memo.end()) return false;
        size_t idx = it->second;
        if (proto == 0){
            body += "g" + std::to_string(idx) + "\n";
        } else if (idx < 256){
            u8('h');
            u8(static_cast<unsigned char>(idx));
        } else {
            u8('j');
            le32(static_cast<uint32_t>(idx));
        }
        return true;
    }

// 登记 memo
// register memo
// memo ᠪᠦᠷᠢᠲ᠋ᠭᠡᠬᠦ
    void memoPut(const std::string& key){
        size_t idx = memo.size();
        memo.emplace(key, idx);
        if (proto == 0){
            body += "p" + std::to_string(idx) + "\n";
        } else if (proto < 4){
            if (idx < 256){
                u8('q');
                u8(static_cast<unsigned char>(idx));
            } else {
                u8('r');
                le32(static_cast<uint32_t>(idx));
            }
        } else {
            u8(0x94);
        }
    }

// 写 GLOBAL/STACK_GLOBAL
// emit GLOBAL / STACK_GLOBAL
// GLOBAL ᠪᠢᠴᠢᠬᠦ
    void writeGlobal(const std::string& module, const std::string& name){
        std::string key = "g" + module + ":" + name;
        if (memoGet(key)) return;
        if (proto >= 4){
            writeStr("builtins");
            writeStr(name);
            u8(0x93);
        } else {
            std::string mod = proto <= 2 && module == "builtins" ? "__builtin__" : module;
            body += "c" + mod + "\n" + name + "\n";
        }
        memoPut(key);
    }

// 写字符串原始内容 (协议 0 → V, 1-3 → X, 4+ → 8c/X)
// emit raw string payload (proto 0 → V, 1-3 → X, 4+ → SHORT_BINUNICODE/BINUNICODE)
// ᠦᠰᠦᠭ ᠪᠦᠷᠢᠨ ᠪᠢᠴᠢᠬᠦ
    void writeStrRaw(const std::string& s){
        if (proto == 0){
            u8('V');
            body += rawUnicodeEscape(s);
            u8('\n');
        } else if (proto < 4){
            u8('X');
            le32(static_cast<uint32_t>(s.size()));
            body += s;
        } else {
            if (s.size() <= 0xFF){
                u8(0x8C);
                u8(static_cast<unsigned char>(s.size()));
            } else {
                u8('X');
                le32(static_cast<uint32_t>(s.size()));
            }
            body += s;
        }
    }

// 写 str (带 memo)
// emit str (memoized)
// str ᠪᠢᠴᠢᠬᠦ
    void writeStr(const std::string& s){
        std::string key = "s" + s;
        if (memoGet(key)) return;
        writeStrRaw(s);
        memoPut(key);
    }

// 写字节原始内容 (协议 >= 3 → C/B)
// emit raw bytes payload (protocol >= 3)
// ᠪᠠᠶᠢᠲ᠋ ᠪᠢᠴᠢᠬᠦ
    void writeBytesRaw(const std::string& raw){
        if (raw.size() < 256){
            u8('C');
            u8(static_cast<unsigned char>(raw.size()));
        } else {
            u8('B');
            le32(static_cast<uint32_t>(raw.size()));
        }
        body += raw;
    }

// 写字节 (协议 <= 2 走 codecs.encode 链, 空 bytes 走 bytes() reduce)
// emit bytes (protocol <= 2 uses codecs.encode reduce; empty bytes uses bytes())
// bytes ᠪᠢᠴᠢᠬᠦ
    void writeBytes(const std::string& raw){
        std::string key = "b" + raw;
        if (memoGet(key)) return;
        if (proto >= 3){
            writeBytesRaw(raw);
            memoPut(key);
            return;
        }
        if (raw.empty()){
            writeGlobal("builtins", "bytes");
            if (proto == 0){ u8('('); u8('t'); }
            else u8(')');
            u8('R');
            memoPut(key);
            return;
        }
        writeGlobal("_codecs", "encode");
        if (proto < 2) u8('(');
        std::string asStr = utf8FromLatin1(raw);
        writeStr(asStr);
        writeStr("latin1");
        if (proto == 2) u8(0x86); else u8('t');
        memoPut("t(" + ("s" + asStr) + ",slatin1)");
        u8('R');
        memoPut(key);
    }

// 写浮点
// emit float
// float ᠪᠢᠴᠢᠬᠦ
    void writeFloat(double d){
        if (proto == 0){
            u8('F');
            body += machine->toString(machine->makeFloat(d));
            u8('\n');
        } else {
            u8('G');
            beDouble(d);
        }
    }

// 写整数
// emit int
// int ᠪᠢᠴᠢᠬᠦ
    void writeInt(long long n){
        if (proto == 0){
            if (n >= -2147483648LL && n <= 2147483647LL){
                u8('I');
                body += std::to_string(n);
                u8('\n');
            } else {
                u8('L');
                body += std::to_string(n);
                body += "L\n";
            }
            return;
        }
        if (n >= 0 && n <= 0xFF){
            u8('K');
            u8(static_cast<unsigned char>(n));
            return;
        }
        if (n >= 0 && n <= 0xFFFF){
            u8('M');
            le16(static_cast<unsigned int>(n));
            return;
        }
        if (n >= -2147483648LL && n <= 2147483647LL){
            u8('J');
            le32(static_cast<uint32_t>(static_cast<int32_t>(n)));
            return;
        }
        if (proto < 2){
            u8('L');
            body += std::to_string(n);
            body += "L\n";
            return;
        }
        unsigned char bytes[8];
        for (int i = 0; i < 8; ++i)
            bytes[i] = static_cast<unsigned char>((static_cast<unsigned long long>(n) >> (8 * i)) & 0xFF);
        int len = 8;
        while (len > 1){
            unsigned char last = bytes[len - 1];
            unsigned char prev = bytes[len - 2];
            if ((last == 0x00 && prev < 0x80) || (last == 0xFF && prev >= 0x80)) len--;
            else break;
        }
        u8(0x8A);
        u8(static_cast<unsigned char>(len));
        for (int i = 0; i < len; ++i) u8(bytes[i]);
    }

// 写一个值
// write one value
// ᠨᠢᠭᠡ ᠤᠳᠠᠭ᠌ ᠪᠢᠴᠢᠬᠦ
    void save(const value& v){
        if (!errType.empty()) return;
        if (v.type == "none"){ u8('N'); return; }
        if (v.type == "bool"){
            if (proto >= 2) u8(machine->truthy(v) ? 0x88 : 0x89);
            else body += machine->truthy(v) ? "I01\n" : "I00\n";
            return;
        }
        if (v.type == "int"){ writeInt(machine->toInt(v)); return; }
        if (v.type == "float"){ writeFloat(machine->toFloat(v)); return; }
        if (v.type == "str"){ writeStr(machine->toString(v)); return; }
        if (v.type == "bytes"){ writeBytes(rawOf(v)); return; }
        if (isComplexValueEx(v)){
            double re = 0.0, im = 0.0;
            complexPartsEx(v, re, im);
            std::string key = keyOf(v);
            if (memoGet(key)) return;
            writeGlobal("builtins", "complex");
            if (proto < 2) u8('(');
            writeFloat(re);
            writeFloat(im);
            if (proto >= 2) u8(0x86);
            else u8('t');
            memoPut("t(f" + std::to_string(re) + ",f" + std::to_string(im) + ")");
            u8('R');
            memoPut(key);
            return;
        }
        if (v.type == "bytearray"){
            std::string raw = rawOf(v);
            std::string key = "y" + raw;
            if (memoGet(key)) return;
            if (proto >= 5){
                u8(0x96);
                uint64_t len = raw.size();
                for (int i = 0; i < 8; ++i)
                    u8(static_cast<unsigned char>((len >> (8 * i)) & 0xFF));
                body += raw;
                memoPut(key);
                return;
            }
            writeGlobal("builtins", "bytearray");
            if (proto <= 1) u8('(');
            writeBytes(raw);
            if (proto >= 2) u8(0x85);
            else u8('t');
            memoPut("t(" + ("b" + raw) + ")");
            u8('R');
            memoPut(key);
            return;
        }
        if (v.type == "tuple" && v.ptr){
            auto* items = static_cast<std::vector<value>*>(v.ptr);
            if (items->empty()){
                if (proto >= 1) u8(')');
                else { u8('('); u8('t'); }
                return;
            }
            std::string key = keyOf(v);
            if (memoGet(key)) return;
            size_t n = items->size();
            if (n <= 3 && proto >= 2){
                for (const auto& item : *items) save(item);
                u8(n == 1 ? 0x85 : (n == 2 ? 0x86 : 0x87));
            } else {
                u8('(');
                for (const auto& item : *items) save(item);
                u8('t');
            }
            memoPut(key);
            return;
        }
        if (v.type == "list" && v.ptr){
            auto* items = static_cast<std::vector<value>*>(v.ptr);
            std::string key = keyOf(v);
            if (memoGet(key)) return;
            if (proto == 0){
                u8('(');
                u8('l');
                memoPut(key);
                for (const auto& item : *items){
                    save(item);
                    u8('a');
                }
                return;
            }
            u8(']');
            memoPut(key);
            if (items->size() == 1){
                save((*items)[0]);
                u8('a');
            } else if (items->size() > 1){
                u8('(');
                for (const auto& item : *items) save(item);
                u8('e');
            }
            return;
        }
        if (v.type == "dict" && v.ptr){
            auto* items = static_cast<dict_t*>(v.ptr);
            std::string key = keyOf(v);
            if (memoGet(key)) return;
            if (proto == 0){
                u8('(');
                u8('d');
                memoPut(key);
                for (const auto& pair : *items){
                    save(pair.first);
                    save(pair.second);
                    u8('s');
                }
                return;
            }
            u8('}');
            memoPut(key);
            if (items->size() == 1){
                save((*items)[0].first);
                save((*items)[0].second);
                u8('s');
            } else if (items->size() > 1){
                u8('(');
                for (const auto& pair : *items){
                    save(pair.first);
                    save(pair.second);
                }
                u8('u');
            }
            return;
        }
        std::string name = machine->pythonTypeName(v);
// native_object 用内部 typeName (如 generator) 对齐 CPython 类型名
// native_object uses internal typeName (e.g. generator) to match CPython names
// native_object ᠶᠢᠨᠢᠰ ᠤᠨ typeName ᠢ ᠬᠡᠷᠡᠭᠯᠡᠬᠦ
        if (v.type == "native_object" && v.ptr){
            auto* obj = static_cast<ffi_instance*>(v.ptr);
            if (obj && !obj->typeName.empty()) name = obj->typeName;
        }
        fail("TypeError", "cannot pickle '" + name + "' object");
    }
};

// pickle 读取器 (栈机, 支持协议 0-5 基本操作码)
// pickle reader (stack machine, protocols 0-5 base opcodes)
// pickle ᠤᠩᠰᠢᠭᠴᠢ
struct PickleReader {
    vm* machine;
    const std::string& data;
    size_t pos = 0;
    std::vector<value> stack;
    std::vector<value> memo;
    std::string errType;
    std::string errMsg;

    bool fail(const std::string& type, const std::string& msg){
        if (errType.empty()){ errType = type; errMsg = msg; }
        return false;
    }
    bool need(size_t n){
        if (pos + n > data.size())
            return fail("EOFError", "Ran out of input");
        return true;
    }
    unsigned char u8(){ return static_cast<unsigned char>(data[pos++]); }
    uint16_t le16(){
        uint16_t v = static_cast<uint16_t>(u8());
        v |= static_cast<uint16_t>(u8()) << 8;
        return v;
    }
    uint32_t le32(){
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(u8()) << (8 * i);
        return v;
    }
    uint64_t le64(){
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(u8()) << (8 * i);
        return v;
    }
    double beDouble(){
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits = (bits << 8) | u8();
        double d;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }

// 读一行 (含换行)
// read one line (through newline)
// ᠨᠢᠭᠡ ᠮᠥᠷ ᠤᠩᠰᠢᠬᠤ
    std::string readLine(){
        size_t start = pos;
        while (pos < data.size() && data[pos] != '\n') pos++;
        if (pos >= data.size()){
            fail("EOFError", "Ran out of input");
            return "";
        }
        std::string line = data.substr(start, pos - start);
        pos++;
        return line;
    }

// 是否标记值
// is mark value
// ᠲᠡᠮᠳᠡᠭ ᠶᠤᠮ ᠤᠤ
    static bool isMark(const value& v){ return v.type == "__mark__"; }

// 弹出标记之后的全部元素 (并移除标记)
// pop all items after last mark (removing the mark)
// ᠲᠡᠮᠳᠡᠭ ᠡᠴᠡ ᠬᠣᠶᠢᠰᠢ ᠶᠢᠨ ᠪᠦᠬᠦ ᠶᠢ ᠭᠠᠷᠭᠠᠬᠤ
    bool popAfterMark(std::vector<value>& items){
        for (size_t i = stack.size(); i-- > 0;){
            if (isMark(stack[i])){
                items.assign(stack.begin() + static_cast<long long>(i) + 1, stack.end());
                stack.resize(i);
                return true;
            }
        }
        return fail("UnpicklingError", "could not find MARK");
    }

// 弹出栈顶
// pop stack top
// ᠳᠡᠭᠡᠳ᠋ᠤ ᠶᠢ ᠭᠠᠷᠭᠠᠬᠤ
    value pop(){
        if (stack.empty()){
            fail("UnpicklingError", "pickle data was truncated");
            return machine->makeNone();
        }
        value v = stack.back();
        stack.pop_back();
        return v;
    }

// memo 写入
// memo store
// memo ᠪᠦᠷᠢᠲ᠋ᠭᠡᠬᠦ
    void memoStore(size_t idx){
        if (stack.empty()){
            fail("UnpicklingError", "pickle data was truncated");
            return;
        }
        if (idx >= memo.size()) memo.resize(idx + 1);
        memo[idx] = stack.back();
    }

// memo 读取
// memo fetch
// memo ᠤᠩᠰᠢᠬᠤ
    void memoFetch(size_t idx){
        if (idx >= memo.size() || (memo[idx].type == "none" && !memo[idx].ptr)){
            fail("UnpicklingError", "Memo value not found at index " + std::to_string(idx));
            return;
        }
        stack.push_back(memo[idx]);
    }

// REDUCE 应用
// apply REDUCE
// REDUCE ᠬᠡᠷᠡᠭᠵᠢᠬᠦᠯᠬᠦ
    void applyReduce(){
        value args = pop();
        value callable = pop();
        if (errType.size()) return;
        if (callable.type != "__global__" || !callable.ptr){
            fail("UnpicklingError", "cannot reduce object");
            return;
        }
        auto* names = static_cast<std::pair<std::string, std::string>*>(callable.ptr);
        std::vector<value>* argv = nullptr;
        if ((args.type == "tuple" || args.type == "list") && args.ptr)
            argv = static_cast<std::vector<value>*>(args.ptr);
        if ((names->first == "_codecs" || names->first == "__codecs__" ||
             names->first == "codecs") &&
            names->second == "encode" && argv && argv->size() == 2){
            std::string raw = latin1FromUtf8(machine->toString((*argv)[0]));
            stack.push_back(mkBytesVal(raw));
            return;
        }
        if ((names->first == "builtins" || names->first == "__builtin__") &&
            names->second == "bytes" && argv && argv->empty()){
            stack.push_back(mkBytesVal(""));
            return;
        }
        if ((names->first == "builtins" || names->first == "__builtin__") &&
            names->second == "bytearray" && argv && argv->size() == 1){
            stack.push_back(mkByteArrayVal(rawOf((*argv)[0])));
            return;
        }
        if ((names->first == "builtins" || names->first == "__builtin__") &&
            names->second == "complex" && argv && argv->size() == 2){
            stack.push_back(machine->makeComplex(machine->toFloat((*argv)[0]),
                                                 machine->toFloat((*argv)[1])));
            return;
        }
        fail("UnpicklingError", "unsupported REDUCE: " + names->first + "." + names->second);
    }

    value mkBytesVal(const std::string& raw){
        value v = machine->makeString(raw);
        v.type = "bytes";
        return v;
    }
    value mkByteArrayVal(const std::string& raw){
        value v = machine->makeString(raw);
        v.type = "bytearray";
        return v;
    }

// 主循环: 返回反序列化的值
// main loop: return the unpickled value
// ᠭᠤᠯ ᠳ᠋ᠤᠭᠤᠶᠢᠯᠠᠩ
    value load(){
        for (;;){
            if (!errType.empty()) return machine->makeNone();
            if (pos >= data.size()){
                fail("EOFError", "Ran out of input");
                return machine->makeNone();
            }
            unsigned char op = u8();
            switch (op){
            case '.': { // STOP
                if (stack.empty()){
                    fail("UnpicklingError", "pickle data was truncated");
                    return machine->makeNone();
                }
                return stack.back();
            }
            case 0x80: { // PROTO
                if (!need(1)) return machine->makeNone();
                unsigned int version = u8();
                if (version > 5){
                    fail("ValueError", "unsupported pickle protocol: " + std::to_string(version));
                    return machine->makeNone();
                }
                break;
            }
            case 0x95: { // FRAME
                if (!need(8)) return machine->makeNone();
                uint64_t len = le64();
                if (len > data.size() - pos){
                    fail("UnpicklingError", "pickle data was truncated");
                    return machine->makeNone();
                }
                break;
            }
            case 'N': stack.push_back(machine->makeNone()); break;
            case 0x88: stack.push_back(machine->makeBool(true)); break;
            case 0x89: stack.push_back(machine->makeBool(false)); break;
            case 'I': { // INT / BOOL
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                if (line == "01") stack.push_back(machine->makeBool(true));
                else if (line == "00") stack.push_back(machine->makeBool(false));
                else stack.push_back(machine->makeInt(std::strtoll(line.c_str(), nullptr, 10)));
                break;
            }
            case 'J': {
                if (!need(4)) return machine->makeNone();
                stack.push_back(machine->makeInt(static_cast<int32_t>(le32())));
                break;
            }
            case 'K': {
                if (!need(1)) return machine->makeNone();
                stack.push_back(machine->makeInt(u8()));
                break;
            }
            case 'M': {
                if (!need(2)) return machine->makeNone();
                stack.push_back(machine->makeInt(le16()));
                break;
            }
            case 'L': { // LONG (text)
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                if (!line.empty() && line.back() == 'L') line.pop_back();
                stack.push_back(machine->makeInt(std::strtoll(line.c_str(), nullptr, 10)));
                break;
            }
            case 0x8A:   // LONG1
            case 0x8B: { // LONG4
                size_t len;
                if (op == 0x8A){
                    if (!need(1)) return machine->makeNone();
                    len = u8();
                } else {
                    if (!need(4)) return machine->makeNone();
                    len = le32();
                }
                if (!need(len)) return machine->makeNone();
                if (len == 0){
                    stack.push_back(machine->makeInt(0));
                    break;
                }
                uint64_t raw = 0;
                size_t take = len > 8 ? 8 : len;
                for (size_t i = 0; i < take; ++i)
                    raw |= static_cast<uint64_t>(u8()) << (8 * i);
                pos += len - take;
                if (len < 8 && (raw & (1ULL << (len * 8 - 1))))
                    raw |= ~0ULL << (len * 8);
                stack.push_back(machine->makeInt(static_cast<long long>(raw)));
                break;
            }
            case 'F': { // FLOAT (text)
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                stack.push_back(machine->makeFloat(std::strtod(line.c_str(), nullptr)));
                break;
            }
            case 'G': { // BINFLOAT
                if (!need(8)) return machine->makeNone();
                stack.push_back(machine->makeFloat(beDouble()));
                break;
            }
            case 'V': { // UNICODE
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                stack.push_back(machine->makeString(decodeUnicodeLine(line)));
                break;
            }
            case 'X':   // BINUNICODE
            case 'T': { // BINSTRING
                if (!need(4)) return machine->makeNone();
                uint32_t len = le32();
                if (!need(len)) return machine->makeNone();
                stack.push_back(machine->makeString(data.substr(pos, len)));
                pos += len;
                break;
            }
            case 0x8C: { // SHORT_BINUNICODE
                if (!need(1)) return machine->makeNone();
                size_t len = u8();
                if (!need(len)) return machine->makeNone();
                stack.push_back(machine->makeString(data.substr(pos, len)));
                pos += len;
                break;
            }
            case 0x8D: { // BINUNICODE8
                if (!need(8)) return machine->makeNone();
                uint64_t len = 0;
                for (int i = 7; i >= 0; --i) len = (len << 8) | u8();
                if (!need(static_cast<size_t>(len))) return machine->makeNone();
                stack.push_back(machine->makeString(data.substr(pos, static_cast<size_t>(len))));
                pos += static_cast<size_t>(len);
                break;
            }
            case 'U': { // SHORT_BINSTRING
                if (!need(1)) return machine->makeNone();
                size_t len = u8();
                if (!need(len)) return machine->makeNone();
                stack.push_back(machine->makeString(utf8FromLatin1(data.substr(pos, len))));
                pos += len;
                break;
            }
            case 'S': { // STRING (legacy quoted)
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                stack.push_back(machine->makeString(unquote(line)));
                break;
            }
            case 'C': { // SHORT_BINBYTES
                if (!need(1)) return machine->makeNone();
                size_t len = u8();
                if (!need(len)) return machine->makeNone();
                stack.push_back(mkBytesVal(data.substr(pos, len)));
                pos += len;
                break;
            }
            case 'B': { // BINBYTES
                if (!need(4)) return machine->makeNone();
                uint32_t len = le32();
                if (!need(len)) return machine->makeNone();
                stack.push_back(mkBytesVal(data.substr(pos, len)));
                pos += len;
                break;
            }
            case 0x8E: { // BINBYTES8
                if (!need(8)) return machine->makeNone();
                uint64_t len = 0;
                for (int i = 7; i >= 0; --i) len = (len << 8) | u8();
                if (!need(static_cast<size_t>(len))) return machine->makeNone();
                stack.push_back(mkBytesVal(data.substr(pos, static_cast<size_t>(len))));
                pos += static_cast<size_t>(len);
                break;
            }
            case 0x96: { // BYTEARRAY8
                if (!need(8)) return machine->makeNone();
                uint64_t len = le64();
                if (!need(static_cast<size_t>(len))) return machine->makeNone();
                stack.push_back(mkByteArrayVal(data.substr(pos, static_cast<size_t>(len))));
                pos += static_cast<size_t>(len);
                break;
            }
            case ']': stack.push_back(machine->makeList({})); break;
            case 'l': { // LIST
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                stack.push_back(machine->makeList(items));
                break;
            }
            case 'a': { // APPEND
                value item = pop();
                if (!errType.empty()) return machine->makeNone();
                if (stack.empty() || stack.back().type != "list" || !stack.back().ptr){
                    fail("UnpicklingError", "APPEND expects list");
                    return machine->makeNone();
                }
                static_cast<std::vector<value>*>(stack.back().ptr)->push_back(item);
                break;
            }
            case 'e': { // APPENDS
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                if (stack.empty() || stack.back().type != "list" || !stack.back().ptr){
                    fail("UnpicklingError", "APPENDS expects list");
                    return machine->makeNone();
                }
                auto* list = static_cast<std::vector<value>*>(stack.back().ptr);
                for (const auto& item : items) list->push_back(item);
                break;
            }
            case '}': stack.push_back(machine->makeDict({})); break;
            case 'd': { // DICT
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                std::vector<std::pair<value, value>> pairs;
                for (size_t i = 0; i + 1 < items.size(); i += 2)
                    pairs.emplace_back(items[i], items[i + 1]);
                stack.push_back(machine->makeDict(pairs));
                break;
            }
            case 's': { // SETITEM
                value val = pop();
                value key = pop();
                if (!errType.empty()) return machine->makeNone();
                if (stack.empty() || stack.back().type != "dict"){
                    fail("UnpicklingError", "SETITEM expects dict");
                    return machine->makeNone();
                }
                machine->dictSet(stack.back(), key, val);
                break;
            }
            case 'u': { // SETITEMS
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                if (stack.empty() || stack.back().type != "dict"){
                    fail("UnpicklingError", "SETITEMS expects dict");
                    return machine->makeNone();
                }
                for (size_t i = 0; i + 1 < items.size(); i += 2)
                    machine->dictSet(stack.back(), items[i], items[i + 1]);
                break;
            }
            case ')': stack.push_back(machine->makeTuple({})); break;
            case 't': { // TUPLE
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                stack.push_back(machine->makeTuple(items));
                break;
            }
            case 0x85: { // TUPLE1
                value a1 = pop();
                stack.push_back(machine->makeTuple({a1}));
                break;
            }
            case 0x86: { // TUPLE2
                value a2 = pop();
                value a1 = pop();
                stack.push_back(machine->makeTuple({a1, a2}));
                break;
            }
            case 0x87: { // TUPLE3
                value a3 = pop();
                value a2 = pop();
                value a1 = pop();
                stack.push_back(machine->makeTuple({a1, a2, a3}));
                break;
            }
            case '(': { // MARK
                value mark("__mark__", nullptr);
                stack.push_back(mark);
                break;
            }
            case '0': { // POP
                pop();
                if (!errType.empty()) return machine->makeNone();
                break;
            }
            case '1': { // POP_MARK
                std::vector<value> items;
                if (!popAfterMark(items)) return machine->makeNone();
                break;
            }
            case '2': { // DUP
                if (stack.empty()){
                    fail("UnpicklingError", "pickle data was truncated");
                    return machine->makeNone();
                }
                stack.push_back(stack.back());
                break;
            }
            case 'p': { // PUT
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                memoStore(static_cast<size_t>(std::strtoull(line.c_str(), nullptr, 10)));
                break;
            }
            case 'q': { // BINPUT
                if (!need(1)) return machine->makeNone();
                memoStore(u8());
                break;
            }
            case 'r': { // LONG_BINPUT
                if (!need(4)) return machine->makeNone();
                memoStore(le32());
                break;
            }
            case 'g': { // GET
                std::string line = readLine();
                if (!errType.empty()) return machine->makeNone();
                memoFetch(static_cast<size_t>(std::strtoull(line.c_str(), nullptr, 10)));
                if (!errType.empty()) return machine->makeNone();
                break;
            }
            case 'h': { // BINGET
                if (!need(1)) return machine->makeNone();
                memoFetch(u8());
                if (!errType.empty()) return machine->makeNone();
                break;
            }
            case 'j': { // LONG_BINGET
                if (!need(4)) return machine->makeNone();
                memoFetch(le32());
                if (!errType.empty()) return machine->makeNone();
                break;
            }
            case 0x94: { // MEMOIZE
                if (stack.empty()){
                    fail("UnpicklingError", "pickle data was truncated");
                    return machine->makeNone();
                }
                memo.push_back(stack.back());
                break;
            }
            case 'c': { // GLOBAL
                std::string module = readLine();
                if (!errType.empty()) return machine->makeNone();
                std::string name = readLine();
                if (!errType.empty()) return machine->makeNone();
                pushGlobal(module, name);
                break;
            }
            case 0x93: { // STACK_GLOBAL
                value name = pop();
                value module = pop();
                if (!errType.empty()) return machine->makeNone();
                pushGlobal(machine->toString(module), machine->toString(name));
                break;
            }
            case 'R': { // REDUCE
                applyReduce();
                if (!errType.empty()) return machine->makeNone();
                break;
            }
            case 'P':   // PERSID
            case 'Q': { // BINPERSID
                fail("UnpicklingError", "A load persistent id instruction was encountered");
                return machine->makeNone();
            }
            default: {
                char c = static_cast<char>(op);
                fail("UnpicklingError", std::string("invalid load key, '") + c + "'.");
                return machine->makeNone();
            }
            }
        }
    }

// 记录全局引用标记
// record global reference marker
// ᠪᠦᠬᠦᠳᠡ ᠶᠢᠨ ᠲᠡᠮᠳᠡᠭ
    void pushGlobal(const std::string& module, const std::string& name){
        auto* names = new std::pair<std::string, std::string>(module, name);
        value v("__global__", names);
        stack.push_back(v);
    }

// 解除旧式 STRING 引号与转义
// unquote legacy STRING payload
// ᠬᠠᠭᠤᠴᠢᠨ ᠦᠰᠦᠭ ᠳ᠋ᠢᠺᠣᠳ
    std::string unquote(const std::string& line){
        if (line.size() < 2) return line;
        char quote = line.front();
        std::string body = line.substr(1, line.size() - 2);
        std::string out;
        for (size_t i = 0; i < body.size(); ++i){
            if (body[i] == '\\' && i + 1 < body.size()){
                char e = body[++i];
                switch (e){
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                case '\'': out.push_back('\''); break;
                case '"': out.push_back('"'); break;
                case 'x': {
                    if (i + 2 < body.size()){
                        auto hv = [](char h) -> int {
                            if (h >= '0' && h <= '9') return h - '0';
                            if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                            if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                            return 0;
                        };
                        out.push_back(static_cast<char>((hv(body[i + 1]) << 4) | hv(body[i + 2])));
                        i += 2;
                    }
                    break;
                }
                default: out.push_back(e); break;
                }
            } else {
                out.push_back(body[i]);
            }
        }
        (void)quote;
        return utf8FromLatin1(out);
    }
};

// 解析协议参数
// resolve protocol argument
// ᠫᠷᠣᠲᠣᠺᠣᠯ ᠠᠷᠭᠤᠮᠡᠨᠲ
static bool resolveProtocol(cffi& a, int index, int& proto){
    value v = index >= 0 ? a.arg(static_cast<size_t>(index)) : a.kwarg("protocol");
    if (index < 0 && !a.has_kwarg("protocol")) v = value();
    if (v.type == "none"){
        proto = 5;
        return true;
    }
    long long p = a.vm->toInt(v);
    if (p < 0) p = 5;
    if (p > 5){
        a.vm->raiseError("ValueError", "pickle protocol must be <= 5");
        return false;
    }
    proto = static_cast<int>(p);
    return true;
}

// dumps 实现核心
// core of dumps
// dumps ᠦᠨᠳᠡᠰᠦ
static value dumpsCore(cffi& a, const value& obj, int proto){
    PickleWriter w{a.vm, proto, "", {}, "", ""};
    w.save(obj);
    if (!w.errType.empty()){
        a.vm->raiseError(w.errType, w.errMsg);
        return a.none();
    }
    w.body.push_back('.');
    std::string out;
    if (proto >= 2){
        out.push_back(static_cast<char>(0x80));
        out.push_back(static_cast<char>(proto));
    }
    if (proto >= 4 && w.body.size() >= 4){
        out.push_back(static_cast<char>(0x95));
        uint64_t len = w.body.size();
        for (int i = 0; i < 8; ++i)
            out.push_back(static_cast<char>((len >> (8 * i)) & 0xFF));
    }
    out += w.body;
    return mkBytes(a, out);
}

// loads 实现核心
// core of loads
// loads ᠦᠨᠳᠡᠰᠦ
static value loadsCore(cffi& a, const std::string& raw){
    PickleReader r{a.vm, raw, 0, {}, {}, "", ""};
    value result = r.load();
    if (!r.errType.empty()){
        a.vm->raiseError(r.errType, r.errMsg);
        return a.none();
    }
    return result;
}

// dumps
value fnPickleDumps(cffi& a){
    int proto = 5;
    if (a.has_kwarg("protocol")){
        if (!resolveProtocol(a, -1, proto)) return a.none();
    } else if (a.argc() >= 2 && a.arg(1).type != "none"){
        if (!resolveProtocol(a, 1, proto)) return a.none();
    }
    return dumpsCore(a, a.arg(0), proto);
}

// loads
value fnPickleLoads(cffi& a){
    return loadsCore(a, rawOf(a.arg(0)));
}

// dump
value fnPickleDump(cffi& a){
    int proto = 5;
    if (a.has_kwarg("protocol")){
        if (!resolveProtocol(a, -1, proto)) return a.none();
    } else if (a.argc() >= 3 && a.arg(2).type != "none"){
        if (!resolveProtocol(a, 2, proto)) return a.none();
    }
    value bytes = dumpsCore(a, a.arg(0), proto);
    if (a.vm->hasError()) return a.none();
    value writeFn = a.vm->getAttribute(a.arg(1), "write");
    if (writeFn.type == "none" || a.vm->hasError()) return a.none();
    a.vm->callValue(writeFn, {bytes});
    if (a.vm->hasError()) return a.none();
    return a.none();
}

// load
value fnPickleLoad(cffi& a){
    value readFn = a.vm->getAttribute(a.arg(0), "read");
    if (readFn.type == "none" || a.vm->hasError()) return a.none();
    value result = a.vm->callValue(readFn, {});
    if (a.vm->hasError()) return a.none();
    return loadsCore(a, rawOf(result));
}

// Pickler 构造器
// Pickler constructor
// Pickler ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value picklerCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["file"] = a.arg(0);
    int proto = 5;
    if (a.has_kwarg("protocol")){
        if (!resolveProtocol(a, -1, proto)) return a.none();
    } else if (a.argc() >= 2 && a.arg(1).type != "none"){
        if (!resolveProtocol(a, 1, proto)) return a.none();
    }
    inst->attributes["protocol"] = a.mkint(proto);
    return obj;
}

// Pickler.dump
value picklerDump(cffi& a){
    value self = a.self();
    value file = a.attr(self, "file");
    value proto = a.attr(self, "protocol");
    value bytes = dumpsCore(a, a.arg(0), static_cast<int>(a.vm->toInt(proto)));
    if (a.vm->hasError()) return a.none();
    value writeFn = a.vm->getAttribute(file, "write");
    if (writeFn.type == "none" || a.vm->hasError()) return a.none();
    a.vm->callValue(writeFn, {bytes});
    if (a.vm->hasError()) return a.none();
    return a.none();
}

// Unpickler 构造器
// Unpickler constructor
// Unpickler ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value unpicklerCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["file"] = a.arg(0);
    return obj;
}

// Unpickler.load
value unpicklerLoad(cffi& a){
    value self = a.self();
    value file = a.attr(self, "file");
    value readFn = a.vm->getAttribute(file, "read");
    if (readFn.type == "none" || a.vm->hasError()) return a.none();
    value result = a.vm->callValue(readFn, {});
    if (a.vm->hasError()) return a.none();
    return loadsCore(a, rawOf(result));
}

// cffi pickle lib
// cffi pickle library
// cffi pickle ᠨᠣᠮ
ffilib cffi_pickle_lib(){
    ffi_class pickleError;
    pickleError.name = "PickleError";
    ffi_class picklingError;
    picklingError.name = "PicklingError";
    ffi_class unpicklingError;
    unpicklingError.name = "UnpicklingError";
    ffi_class pickler = cffi_class(
        "Pickler", (void*)picklerCtor,
        {
            {"dump", (void*)picklerDump}
        });
    ffi_class unpickler = cffi_class(
        "Unpickler", (void*)unpicklerCtor,
        {
            {"load", (void*)unpicklerLoad}
        });

    return cffi_lib(
        "pickle",
        {
            {"dumps", (void*)fnPickleDumps},
            {"loads", (void*)fnPickleLoads},
            {"dump", (void*)fnPickleDump},
            {"load", (void*)fnPickleLoad}
        },
        {
            {"HIGHEST_PROTOCOL", (void*)+[] (cffi& a) -> value { return a.mkint(5); }},
            {"DEFAULT_PROTOCOL", (void*)+[] (cffi& a) -> value { return a.mkint(5); }}
        },
        {
            {"PickleError", pickleError},
            {"PicklingError", picklingError},
            {"UnpicklingError", unpicklingError},
            {"Pickler", pickler},
            {"Unpickler", unpickler}
        });
}

}
