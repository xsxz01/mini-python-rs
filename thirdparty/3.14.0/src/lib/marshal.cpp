#include "../cffi.hpp"

#include <cstdint>
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

// 是否 ASCII 字符串
// is ascii string
// ASCII ᠶᠤᠮ ᠤᠤ
static bool isAscii(const std::string& s){
    for (unsigned char c : s)
        if (c >= 0x80) return false;
    return true;
}

// marshal 写出器 (格式与 CPython marshal 一致)
// marshal writer (format compatible with CPython marshal)
// marshal ᠪᠢᠴᠢᠭᠯᠡᠭᠴᠢ
struct MarshalWriter {
    vm* machine;
    std::string out;
    std::map<std::string, size_t> refs;
    bool bad = false;
// 顶层实参的瞬态引用开销 (调用机制引入, CPython 无)
// transient ref overhead of the top-level argument (call machinery; not in CPython)
// ᠳᠡᠭᠡᠳᠤ ᠲᠦᠷ ᠤᠨ ᠬᠠᠯᠠᠭᠤᠨ ᠲᠣᠭ᠎ᠠ
    const void* rootPtr = nullptr;
    static constexpr long long kRootRefBias = 3;

// 判断容器是否被共享 (对齐 CPython 的 refcount > 1 规则)
// whether a container is shared (mirrors CPython's refcount > 1 rule)
// ᠬᠠᠮᠲᠤᠳᠤ ᠡᠰᠡᠬᠦ ᠪᠡᠷ ᠰᠢᠢᠳᠬᠦ
    bool isShared(const void* ptr) const {
        long long rc = machine->refCountOf(const_cast<void*>(ptr));
        if (ptr == rootPtr) rc -= kRootRefBias;
        return rc > 1;
    }

    void u8(unsigned char b){ out.push_back(static_cast<char>(b)); }
    void le16(unsigned int v){
        u8(v & 0xFF); u8((v >> 8) & 0xFF);
    }
    void le32(uint32_t v){
        u8(v & 0xFF); u8((v >> 8) & 0xFF);
        u8((v >> 16) & 0xFF); u8((v >> 24) & 0xFF);
    }
    void leDouble(double d){
        uint64_t bits;
        std::memcpy(&bits, &d, sizeof(bits));
        for (int i = 0; i < 8; ++i) u8(static_cast<unsigned char>((bits >> (8 * i)) & 0xFF));
    }

// 结构键 (用于引用去重, 对应 CPython 常量池行为)
// structural key (dedup references, mirrors CPython constant pool)
// ᠪᠦᠲ᠋ᠤᠴᠡ ᠶᠢᠨ ᠲᠦᠯᠬᠢᠬᠦᠷ
    std::string keyOf(const value& v){
        if (v.type == "int" || v.type == "bool")
            return "i" + std::to_string(machine->toInt(v));
        if (v.type == "float"){
            double d = machine->toFloat(v);
            uint64_t bits;
            std::memcpy(&bits, &d, sizeof(bits));
            return "f" + std::to_string(bits);
        }
        if (isComplexValueEx(v)){
            double re = 0.0, im = 0.0;
            complexPartsEx(v, re, im);
            uint64_t rb, ib;
            std::memcpy(&rb, &re, sizeof(rb));
            std::memcpy(&ib, &im, sizeof(ib));
            return "c" + std::to_string(rb) + ":" + std::to_string(ib);
        }
        if (v.type == "str") return "s" + machine->toString(v);
        if (v.type == "bytes" || v.type == "bytearray") return "b" + rawOf(v);
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
        return "";
    }

// 写引用: 已见过则写 TYPE_REF 并返回 true, 否则登记并给 flag
// write reference: emit TYPE_REF if seen, else register and flag
// ᠰᠠᠯᠭᠠᠭᠤᠷ ᠪᠢᠴᠢᠬᠦ
    bool wRef(const std::string& key, unsigned char& flag){
        auto it = refs.find(key);
        if (it != refs.end()){
            u8('r');
            le32(static_cast<uint32_t>(it->second));
            return true;
        }
        refs.emplace(key, refs.size());
        flag = 0x80;
        return false;
    }

// 写一个值
// write one value
// ᠨᠢᠭᠡ ᠤᠳᠠᠭ᠌ ᠪᠢᠴᠢᠬᠦ
    void w(const value& v){
        if (bad) return;
        if (v.type == "none"){ u8('N'); return; }
        if (v.type == "bool"){
            u8(machine->truthy(v) ? 'T' : 'F');
            return;
        }
        if (v.type == "int"){
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            long long n = machine->toInt(v);
            if (n >= -2147483648LL && n <= 2147483647LL){
                u8(static_cast<unsigned char>('i') | flag);
                le32(static_cast<uint32_t>(static_cast<int32_t>(n)));
                return;
            }
            u8(static_cast<unsigned char>('l') | flag);
            unsigned long long mag = n < 0 ? ~static_cast<unsigned long long>(n) + 1ULL
                                           : static_cast<unsigned long long>(n);
            unsigned int digits[8];
            int count = 0;
            do {
                digits[count++] = static_cast<unsigned int>(mag & 0x7FFFULL);
                mag >>= 15;
            } while (mag && count < 8);
            le32(static_cast<uint32_t>(n < 0 ? -count : count));
            for (int i = 0; i < count; ++i) le16(digits[i]);
            return;
        }
        if (v.type == "float"){
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            u8(static_cast<unsigned char>('g') | flag);
            leDouble(machine->toFloat(v));
            return;
        }
        if (isComplexValueEx(v)){
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            double re = 0.0, im = 0.0;
            complexPartsEx(v, re, im);
            u8(static_cast<unsigned char>('y') | flag);
            leDouble(re);
            leDouble(im);
            return;
        }
        if (v.type == "str"){
            std::string s = machine->toString(v);
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            if (isAscii(s)){
                if (s.size() < 256){
                    u8(static_cast<unsigned char>('Z') | flag);
                    u8(static_cast<unsigned char>(s.size()));
                } else {
                    u8(static_cast<unsigned char>('A') | flag);
                    le32(static_cast<uint32_t>(s.size()));
                }
            } else {
                u8(static_cast<unsigned char>('u') | flag);
                le32(static_cast<uint32_t>(s.size()));
            }
            out += s;
            return;
        }
        if (v.type == "bytes" || v.type == "bytearray"){
            std::string s = rawOf(v);
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            u8(static_cast<unsigned char>('s') | flag);
            le32(static_cast<uint32_t>(s.size()));
            out += s;
            return;
        }
        if (v.type == "tuple" && v.ptr){
            auto* items = static_cast<std::vector<value>*>(v.ptr);
            unsigned char flag = 0;
            if (wRef(keyOf(v), flag)) return;
            if (items->size() < 256){
                u8(static_cast<unsigned char>(')') | flag);
                u8(static_cast<unsigned char>(items->size()));
            } else {
                u8(static_cast<unsigned char>('(') | flag);
                le32(static_cast<uint32_t>(items->size()));
            }
            for (const auto& item : *items) w(item);
            return;
        }
        if (v.type == "list" && v.ptr){
            auto* items = static_cast<std::vector<value>*>(v.ptr);
// CPython 仅对引用计数 > 1 的容器做引用跟踪
// CPython only ref-tracks containers with refcount > 1
// ᠬᠠᠯᠠᠭᠤᠨ ᠲᠣᠭ᠎ᠠ > 1 ᠪᠣᠯ ᠰᠠᠯᠭᠠᠭᠤᠷ ᠪᠢᠴᠢᠬᠦ
            unsigned char flag = 0;
            if (isShared(v.ptr)){
                std::string key = "p" + std::to_string(reinterpret_cast<uintptr_t>(v.ptr));
                if (wRef(key, flag)) return;
            }
            u8(static_cast<unsigned char>('[') | flag);
            le32(static_cast<uint32_t>(items->size()));
            for (const auto& item : *items) w(item);
            return;
        }
        if (v.type == "dict" && v.ptr){
            auto* items = static_cast<dict_t*>(v.ptr);
            unsigned char flag = 0;
            if (isShared(v.ptr)){
                std::string key = "p" + std::to_string(reinterpret_cast<uintptr_t>(v.ptr));
                if (wRef(key, flag)) return;
            }
            u8(static_cast<unsigned char>('{') | flag);
            for (const auto& pair : *items){
                w(pair.first);
                w(pair.second);
            }
            u8('0');
            return;
        }
        bad = true;
    }
};

// marshal 读取器
// marshal reader
// marshal ᠤᠩᠰᠢᠭᠴᠢ
struct MarshalReader {
    vm* machine;
    const std::string& data;
    size_t pos = 0;
    std::vector<value> refs;
    std::string errType;
    std::string errMsg;

    bool fail(const std::string& type, const std::string& msg){
        if (errType.empty()){ errType = type; errMsg = msg; }
        return false;
    }
    bool need(size_t n){
        if (pos + n > data.size())
            return fail("EOFError", "marshal data too short");
        return true;
    }
    unsigned char u8(){
        return static_cast<unsigned char>(data[pos++]);
    }
    uint32_t le32(){
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(u8()) << (8 * i);
        return v;
    }
    double leDouble(){
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits |= static_cast<uint64_t>(u8()) << (8 * i);
        double d;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }

// 读一个值
// read one value
// ᠨᠢᠭᠡ ᠤᠳᠠᠭ᠌ ᠤᠩᠰᠢᠬᠤ
    value r(){
        if (!errType.empty()) return machine->makeNone();
        if (pos >= data.size()){
            fail("EOFError", "EOF read where object expected");
            return machine->makeNone();
        }
        unsigned char head = u8();
        bool flagged = (head & 0x80) != 0;
        unsigned char type = head & 0x7F;

// 引用槽登记 (先占位后填充, 与写出顺序一致)
// reserve ref slot (fill later, matches writer order)
// ᠰᠠᠯᠭᠠᠭᠤᠷ ᠰᠠᠯᠭᠢᠯ
        size_t slot = refs.size();
        auto track = [&](const value& result){
            if (flagged){
                if (refs.size() == slot) refs.push_back(result);
                else refs[slot] = result;
            }
            return result;
        };
        if (flagged) refs.push_back(machine->makeNone());

        switch (type){
        case 'N': return track(machine->makeNone());
        case 'T': return track(machine->makeBool(true));
        case 'F': return track(machine->makeBool(false));
        case 'i': {
            if (!need(4)) return machine->makeNone();
            uint32_t raw = le32();
            return track(machine->makeInt(static_cast<int32_t>(raw)));
        }
        case 'l': {
            if (!need(4)) return machine->makeNone();
            int32_t count = static_cast<int32_t>(le32());
            int n = count < 0 ? -count : count;
            if (n > 100){ fail("ValueError", "bad marshal data (long size out of range)"); return machine->makeNone(); }
            if (!need(static_cast<size_t>(n) * 2)) return machine->makeNone();
            unsigned long long raw = 0;
            for (int i = 0; i < n; ++i){
                unsigned int digit = static_cast<unsigned int>(u8()) |
                                     (static_cast<unsigned int>(u8()) << 8);
                if (i < 5)
                    raw |= static_cast<unsigned long long>(digit) << (15 * i);
            }
            long long result = count < 0
                ? static_cast<long long>(0ULL - raw)
                : static_cast<long long>(raw);
            return track(machine->makeInt(result));
        }
        case 'g': {
            if (!need(8)) return machine->makeNone();
            return track(machine->makeFloat(leDouble()));
        }
        case 'y': {
            if (!need(16)) return machine->makeNone();
            double re = leDouble();
            double im = leDouble();
            return track(machine->makeComplex(re, im));
        }
        case 'z':
        case 'Z': {
            if (!need(1)) return machine->makeNone();
            size_t len = u8();
            if (!need(len)) return machine->makeNone();
            std::string s = data.substr(pos, len);
            pos += len;
            return track(machine->makeString(s));
        }
        case 'a':
        case 'A':
        case 'u':
        case 's': {
            if (!need(4)) return machine->makeNone();
            uint32_t len = le32();
            if (!need(len)) return machine->makeNone();
            std::string s = data.substr(pos, len);
            pos += len;
            if (type == 's'){
                value v = machine->makeString(s);
                v.type = "bytes";
                return track(v);
            }
            return track(machine->makeString(s));
        }
        case ')':
        case '(': {
            size_t count;
            if (type == ')'){
                if (!need(1)) return machine->makeNone();
                count = u8();
            } else {
                if (!need(4)) return machine->makeNone();
                count = le32();
            }
            std::vector<value> items;
            items.reserve(count);
            for (size_t i = 0; i < count; ++i){
                items.push_back(r());
                if (!errType.empty()) return machine->makeNone();
            }
            return track(machine->makeTuple(items));
        }
        case '[': {
            if (!need(4)) return machine->makeNone();
            size_t count = le32();
            std::vector<value> items;
            items.reserve(count);
            for (size_t i = 0; i < count; ++i){
                items.push_back(r());
                if (!errType.empty()) return machine->makeNone();
            }
            return track(machine->makeList(items));
        }
        case '{': {
            std::vector<std::pair<value, value>> pairs;
            for (;;){
                if (pos >= data.size()){
                    fail("EOFError", "EOF read where object expected");
                    return machine->makeNone();
                }
                if ((data[pos] & 0x7F) == '0'){ pos++; break; }
                value key = r();
                if (!errType.empty()) return machine->makeNone();
                value val = r();
                if (!errType.empty()) return machine->makeNone();
                pairs.emplace_back(key, val);
            }
            return track(machine->makeDict(pairs));
        }
        case 'r': {
            if (!need(4)) return machine->makeNone();
            uint32_t idx = le32();
            if (idx >= refs.size()){
                fail("ValueError", "bad marshal data (reference out of range)");
                return machine->makeNone();
            }
            return refs[idx];
        }
        case '.': {
            fail("ValueError", "bad marshal data (unknown type code)");
            return machine->makeNone();
        }
        default:
            fail("ValueError", "bad marshal data (unknown type code)");
            return machine->makeNone();
        }
    }
};

// 从文件对象读取全部内容
// read all content from a file object
// ᠪᠠᠶᠢᠲ᠋ ᠡᠴᠡ ᠪᠦᠬᠦ ᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠩᠰᠢᠬᠤ
static bool readAllFromFile(cffi& a, const value& file, std::string& out){
    value readFn = a.vm->getAttribute(file, "read");
    if (readFn.type == "none" || a.vm->hasError()) return false;
    value result = a.vm->callValue(readFn, {});
    if (a.vm->hasError()) return false;
    out = rawOf(result);
    return true;
}

// 向文件对象写字节
// write bytes to a file object
// ᠪᠠᠶᠢᠲ᠋ ᠳ᠋ᠤ ᠪᠢᠴᠢᠬᠦ
static bool writeToFile(cffi& a, const value& file, const std::string& raw){
    value writeFn = a.vm->getAttribute(file, "write");
    if (writeFn.type == "none" || a.vm->hasError()) return false;
    a.vm->callValue(writeFn, {mkBytes(a, raw)});
    return !a.vm->hasError();
}

// dumps
value fnMarshalDumps(cffi& a){
    MarshalWriter w{a.vm, "", {}, false, a.arg(0).ptr};
    w.w(a.arg(0));
    if (w.bad){
        a.vm->raiseError("ValueError", "unmarshallable object");
        return a.none();
    }
    return mkBytes(a, w.out);
}

// loads
value fnMarshalLoads(cffi& a){
    std::string raw = rawOf(a.arg(0));
    MarshalReader r{a.vm, raw, 0, {}, "", ""};
    value result = r.r();
    if (!r.errType.empty()){
        a.vm->raiseError(r.errType, r.errMsg);
        return a.none();
    }
    return result;
}

// dump
value fnMarshalDump(cffi& a){
    MarshalWriter w{a.vm, "", {}, false, a.arg(0).ptr};
    w.w(a.arg(0));
    if (w.bad){
        a.vm->raiseError("ValueError", "unmarshallable object");
        return a.none();
    }
    if (!writeToFile(a, a.arg(1), w.out)) return a.none();
    return a.none();
}

// load
value fnMarshalLoad(cffi& a){
    std::string raw;
    if (!readAllFromFile(a, a.arg(0), raw)) return a.none();
    MarshalReader r{a.vm, raw, 0, {}, "", ""};
    value result = r.r();
    if (!r.errType.empty()){
        a.vm->raiseError(r.errType, r.errMsg);
        return a.none();
    }
    return result;
}

// cffi marshal lib
// cffi marshal library
// cffi marshal ᠨᠣᠮ
ffilib cffi_marshal_lib(){
    return cffi_lib(
        "marshal",
        {
            {"dumps", (void*)fnMarshalDumps},
            {"loads", (void*)fnMarshalLoads},
            {"dump", (void*)fnMarshalDump},
            {"load", (void*)fnMarshalLoad}
        },
        {
            {"version", (void*)+[] (cffi& a) -> value { return a.mkint(5); }}
        },
        {});
}

}
