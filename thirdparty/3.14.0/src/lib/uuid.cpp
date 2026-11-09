#include "../cffi.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace minipy {

namespace {

// UUID 状态 (16 个原始字节)
// UUID state (16 raw bytes)
// UUID ᠪᠠᠶᠢᠳᠠᠯ
struct UuidState{
    std::array<unsigned char, 16> bytes{};
};

// 构造 bytes 值
// Make a bytes value
// bytes ᠪᠠᠶᠢᠳᠠᠯ ᠪᠣᠯᠭᠠᠬᠤ
value mkBytes(cffi& a, const std::string& raw){
    value v = a.mkstr(raw);
    v.type = "bytes";
    return v;
}

// 原始字节读取
// Read raw bytes/str content
// ᠪᠠᠶᠢᠲ᠋ ᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
std::string rawOf(const value& v){
    if ((v.type == "bytes" || v.type == "bytearray" || v.type == "str") && v.ptr)
        return *static_cast<std::string*>(v.ptr);
    return "";
}

// 十六进制字符串
// Canonical hexadecimal string
// ᠰᠢᠷᠠᠭᠤ ᠲᠣᠭ᠎ᠠ ᠨᠠᠢᠮᠠ
std::string hexOf(const std::array<unsigned char, 16>& b){
    std::string out;
    out.reserve(32);
    for (int i = 0; i < 16; ++i){
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", b[i]);
        out += buf;
    }
    return out;
}

// 规范字符串表示
// Canonical UUID string
// ᠨᠠᠢᠮᠠ ᠶᠢᠨ ᠬᠡᠯᠪᠡᠷᠢ
std::string canonical(const std::array<unsigned char, 16>& b){
    std::string h = hexOf(b);
    return h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) +
           "-" + h.substr(16, 4) + "-" + h.substr(20, 12);
}

// 版本
// Version nibble
// ᠬᠤᠪᠢᠯᠪᠤᠷᠢ
int versionOf(const std::array<unsigned char, 16>& b){
    return (b[6] >> 4) & 0x0F;
}

// 变体描述
// Variant description
// ᠬᠤᠪᠢᠯᠪᠤᠷᠢ ᠶᠢᠨ ᠲᠣᠳᠣᠷᠬᠠᠢᠯᠠᠯ
std::string variantOf(const std::array<unsigned char, 16>& b){
    if ((b[8] & 0x80) == 0) return "reserved for NCS compatibility";
    if ((b[8] & 0xC0) == 0x80) return "specified in RFC 4122";
    if ((b[8] & 0xE0) == 0xC0) return "reserved for Microsoft compatibility";
    return "reserved for future definition";
}

// 随机生成器
// Random engine
// ᠰᠠᠨᠠᠮᠠᠭ᠎ᠠ ᠭᠠᠷᠭᠠᠭᠴᠢ
std::random_device& rngDevice(){
    static std::random_device dev;
    return dev;
}

uint64_t random64(){
    std::mt19937_64 eng(rngDevice()());
    return eng();
}

// 设置版本半字节
// Force the version nibble
// ᠬᠤᠪᠢᠯᠪᠤᠷᠢ ᠲᠣᠭ᠎ᠠ
void setVersion(std::array<unsigned char, 16>& b, int version){
    b[6] = static_cast<unsigned char>((b[6] & 0x0F) | ((version & 0x0F) << 4));
}

// 设置 RFC 变体位
// Set RFC 4122 variant bits
// ᠪᠠᠶᠢᠳᠠᠯ ᠦᠨ ᠪᠢᠲ᠋
void setRfcVariant(std::array<unsigned char, 16>& b){
    b[8] = static_cast<unsigned char>((b[8] & 0x3F) | 0x80);
}

// 取类描述
// Find class descriptor
// ᠪᠢᠲ᠋ᠢ ᠲᠥᠷᠥᠯ ᠬᠠᠯᠠᠬᠤ
const ffi_class* findClass(const ffilib* lib, const std::string& name){
    if (!lib) return nullptr;
    auto it = lib->classes.find(name);
    return it == lib->classes.end() ? nullptr : &it->second;
}

// 填充 UUID 对象属性
// Fill UUID object attributes
// UUID ᠲᠠᠯᠠᠭ᠌ ᠪᠦᠭᠯᠡᠬᠦ
value makeUuidObject(cffi& a, const std::array<unsigned char, 16>& b, int forcedVersion,
                     bool applyVersion){
    std::array<unsigned char, 16> bytes = b;
    if (applyVersion) setVersion(bytes, forcedVersion);
    auto* st = new UuidState();
    st->bytes = bytes;
    value obj = a.vm->makeNativeObject(a.library, findClass(a.library, "UUID"), st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["hex"] = a.mkstr(hexOf(bytes));
        inst->attributes["urn"] = a.mkstr("urn:uuid:" + canonical(bytes));
        std::string variant = variantOf(bytes);
        bool isRfc = variant == "specified in RFC 4122";
        inst->attributes["version"] = isRfc ? a.mkint(versionOf(bytes)) : a.none();
        inst->attributes["variant"] = a.mkstr(variant);
        std::string raw(bytes.begin(), bytes.end());
        inst->attributes["bytes"] = mkBytes(a, raw);
        std::array<unsigned char, 16> le = bytes;
        le[0] = bytes[3]; le[1] = bytes[2]; le[2] = bytes[1]; le[3] = bytes[0];
        le[4] = bytes[5]; le[5] = bytes[4]; le[6] = bytes[7]; le[7] = bytes[6];
        std::string rawLe(le.begin(), le.end());
        inst->attributes["bytes_le"] = mkBytes(a, rawLe);
        uint32_t timeLow = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
                           (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
        uint32_t timeMid = (uint32_t(bytes[4]) << 8) | uint32_t(bytes[5]);
        uint32_t timeHi = (uint32_t(bytes[6]) << 8) | uint32_t(bytes[7]);
        uint64_t node = 0;
        for (int i = 10; i < 16; ++i) node = (node << 8) | bytes[i];
        uint32_t clockSeq = (uint32_t(bytes[8] & 0x3F) << 8) | uint32_t(bytes[9]);
        std::vector<value> fields = {
            a.mkint(static_cast<long long>(timeLow)),
            a.mkint(static_cast<long long>(timeMid)),
            a.mkint(static_cast<long long>(timeHi)),
            a.mkint(static_cast<long long>(bytes[8])),
            a.mkint(static_cast<long long>(bytes[9])),
            a.mkint(static_cast<long long>(node))
        };
        inst->attributes["fields"] = a.vm->makeTuple(fields);
        inst->attributes["node"] = a.mkint(static_cast<long long>(node));
        inst->attributes["clock_seq"] = a.mkint(static_cast<long long>(clockSeq));
        // time 始终按字节计算 (CPython 不检查 version)
        // time is always computed from bytes (CPython does not check version)
        // time ᠪᠠᠶᠢᠲ᠋ ᠡᠴᠡ ᠲᠣᠭᠠᠴᠢᠯᠠᠬᠤ
        uint64_t timestamp = (uint64_t(timeHi & 0x0FFF) << 48) |
                             (uint64_t(timeMid) << 32) | uint64_t(timeLow);
        inst->attributes["time"] = a.mkint(static_cast<long long>(timestamp));
        // 128 位十进制串 (VM 整数为 64 位, 只能提供十进制文本)
        // 128-bit decimal text (VM ints are 64-bit, text only)
        // 128 ᠪᠢᠲ᠋ ᠲᠣᠭ᠎ᠠ
        std::string dec = "0";
        for (int i = 0; i < 16; ++i){
            int carry = bytes[i];
            for (size_t j = dec.size(); j-- > 0;){
                int cur = (dec[j] - '0') * 256 + carry;
                dec[j] = static_cast<char>('0' + (cur % 10));
                carry = cur / 10;
            }
            while (carry > 0){
                dec.insert(dec.begin(), static_cast<char>('0' + (carry % 10)));
                carry /= 10;
            }
        }
        inst->attributes["int"] = a.mkstr(dec);
    }
    return obj;
}

// 十六进制解析
// Parse a hexadecimal UUID string
// ᠰᠢᠷᠠᠭᠤ ᠲᠣᠭ᠎ᠠ ᠲᠠᠢᠯᠢᠬᠤ
bool parseHex(const std::string& input, std::array<unsigned char, 16>& out){
    std::string text = input;
    if (text.size() > 9){
        std::string prefix = text.substr(0, 9);
        for (auto& c : prefix) c = static_cast<char>(std::tolower(c));
        if (prefix == "urn:uuid:") text = text.substr(9);
    }
    std::string digits;
    for (char c : text){
        if (c == '-' || c == '{' || c == '}' || c == ' ' || c == '(' || c == ')') continue;
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        digits.push_back(c);
    }
    if (digits.size() != 32) return false;
    for (int i = 0; i < 16; ++i){
        std::string part = digits.substr(i * 2, 2);
        out[i] = static_cast<unsigned char>(std::stoi(part, nullptr, 16));
    }
    return true;
}

// 取 long long 整数值
// Read a long long from a value
// ᠲᠣᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
long long intOf(vm& v, const value& val){
    if (val.type == "str" && val.ptr){
        std::string s = *static_cast<std::string*>(val.ptr);
        try{ return std::stoll(s); } catch (...){ return 0; }
    }
    return v.toInt(val);
}

/* ---------- MD5 (RFC 1321) ---------- */

struct Md5Ctx{
    uint32_t a = 0x67452301;
    uint32_t b = 0xefcdab89;
    uint32_t c = 0x98badcfe;
    uint32_t d = 0x10325476;
};

uint32_t rotl32(uint32_t x, uint32_t n){
    return (x << n) | (x >> (32 - n));
}

void md5Block(Md5Ctx& ctx, const unsigned char* p){
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,
        0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,
        0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,
        0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,
        0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,
        0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const uint32_t S[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    uint32_t M[16];
    for (int i = 0; i < 16; ++i)
        M[i] = uint32_t(p[i*4]) | (uint32_t(p[i*4+1]) << 8) |
               (uint32_t(p[i*4+2]) << 16) | (uint32_t(p[i*4+3]) << 24);
    uint32_t A = ctx.a, B = ctx.b, C = ctx.c, D = ctx.d;
    for (int i = 0; i < 64; ++i){
        uint32_t F;
        int g;
        if (i < 16){ F = (B & C) | (~B & D); g = i; }
        else if (i < 32){ F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
        else if (i < 48){ F = B ^ C ^ D; g = (3 * i + 5) % 16; }
        else { F = C ^ (B | ~D); g = (7 * i) % 16; }
        F = F + A + K[i] + M[g];
        A = D;
        D = C;
        C = B;
        B = B + rotl32(F, S[i]);
    }
    ctx.a += A; ctx.b += B; ctx.c += C; ctx.d += D;
}

std::array<unsigned char, 16> md5(const std::string& msg){
    Md5Ctx ctx;
    std::string data = msg;
    size_t total = data.size();
    data.push_back('\x80');
    while (data.size() % 64 != 56) data.push_back('\0');
    uint64_t bitLen = static_cast<uint64_t>(total) * 8ULL;
    for (int i = 0; i < 8; ++i)
        data.push_back(static_cast<char>((bitLen >> (i * 8)) & 0xFF));
    for (size_t off = 0; off < data.size(); off += 64)
        md5Block(ctx, reinterpret_cast<const unsigned char*>(data.data() + off));
    std::array<unsigned char, 16> out{};
    uint32_t vals[4] = {ctx.a, ctx.b, ctx.c, ctx.d};
    for (int i = 0; i < 4; ++i){
        out[i*4] = static_cast<unsigned char>(vals[i] & 0xFF);
        out[i*4+1] = static_cast<unsigned char>((vals[i] >> 8) & 0xFF);
        out[i*4+2] = static_cast<unsigned char>((vals[i] >> 16) & 0xFF);
        out[i*4+3] = static_cast<unsigned char>((vals[i] >> 24) & 0xFF);
    }
    return out;
}

/* ---------- SHA1 ---------- */

std::array<unsigned char, 20> sha1(const std::string& msg){
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE,
             h3 = 0x10325476, h4 = 0xC3D2E1F0;
    std::string data = msg;
    size_t total = data.size();
    data.push_back('\x80');
    while (data.size() % 64 != 56) data.push_back('\0');
    uint64_t bitLen = static_cast<uint64_t>(total) * 8ULL;
    for (int i = 7; i >= 0; --i)
        data.push_back(static_cast<char>((bitLen >> (i * 8)) & 0xFF));
    for (size_t off = 0; off < data.size(); off += 64){
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(static_cast<unsigned char>(data[off+i*4])) << 24) |
                   (uint32_t(static_cast<unsigned char>(data[off+i*4+1])) << 16) |
                   (uint32_t(static_cast<unsigned char>(data[off+i*4+2])) << 8) |
                   uint32_t(static_cast<unsigned char>(data[off+i*4+3]));
        for (int i = 16; i < 80; ++i)
            w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i){
            uint32_t f, k;
            if (i < 20){ f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40){ f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60){ f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t tmp = rotl32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30); b = a; a = tmp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::array<unsigned char, 20> out{};
    uint32_t hs[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i){
        out[i*4] = static_cast<unsigned char>((hs[i] >> 24) & 0xFF);
        out[i*4+1] = static_cast<unsigned char>((hs[i] >> 16) & 0xFF);
        out[i*4+2] = static_cast<unsigned char>((hs[i] >> 8) & 0xFF);
        out[i*4+3] = static_cast<unsigned char>(hs[i] & 0xFF);
    }
    return out;
}

// UUID 构造
// UUID constructor
// UUID ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnUuidCtor(cffi& a){
    std::array<unsigned char, 16> bytes{};
    bool haveBytes = false;
    int forcedVersion = 0;
    bool applyVersion = false;
    if (a.has_kwarg("version")){
        value ver = a.kwarg("version");
        if (ver.type != "none"){
            forcedVersion = static_cast<int>(a.kwarg_int("version", 0));
            applyVersion = true;
        }
    }

    value first = a.argc() > 0 ? a.arg(0) : a.none();
    if (first.type == "native_object" && first.ptr){
        auto* inst = static_cast<ffi_instance*>(first.ptr);
        auto* src = static_cast<UuidState*>(inst->handle);
        if (src && inst->klass && inst->klass->name == "UUID"){
            bytes = src->bytes;
            haveBytes = true;
        }
    } else if (first.type == "str" && a.argc() > 0 && !a.has_kwarg("hex")){
        if (!parseHex(a.arg_str(0), bytes)){
            a.vm->raiseError("ValueError", "badly formed hexadecimal UUID string");
            return a.none();
        }
        haveBytes = true;
    }
    if (!haveBytes && a.has_kwarg("hex")){
        if (!parseHex(a.kwarg_str("hex", ""), bytes)){
            a.vm->raiseError("ValueError", "badly formed hexadecimal UUID string");
            return a.none();
        }
        haveBytes = true;
    }
    if (!haveBytes && a.has_kwarg("bytes")){
        std::string raw = rawOf(a.kwarg("bytes"));
        if (raw.size() != 16){
            a.vm->raiseError("ValueError", "bytes is not a 16-char string");
            return a.none();
        }
        std::memcpy(bytes.data(), raw.data(), 16);
        haveBytes = true;
    }
    if (!haveBytes && a.has_kwarg("bytes_le")){
        std::string raw = rawOf(a.kwarg("bytes_le"));
        if (raw.size() != 16){
            a.vm->raiseError("ValueError", "bytes_le is not a 16-char string");
            return a.none();
        }
        bytes[0] = static_cast<unsigned char>(raw[3]);
        bytes[1] = static_cast<unsigned char>(raw[2]);
        bytes[2] = static_cast<unsigned char>(raw[1]);
        bytes[3] = static_cast<unsigned char>(raw[0]);
        bytes[4] = static_cast<unsigned char>(raw[5]);
        bytes[5] = static_cast<unsigned char>(raw[4]);
        bytes[6] = static_cast<unsigned char>(raw[7]);
        bytes[7] = static_cast<unsigned char>(raw[6]);
        for (int i = 8; i < 16; ++i) bytes[i] = static_cast<unsigned char>(raw[i]);
        haveBytes = true;
    }
    if (!haveBytes && a.has_kwarg("fields")){
        value fieldsValue = a.kwarg("fields");
        std::vector<value> fields;
        if (fieldsValue.type == "tuple" || fieldsValue.type == "list"){
            fields = *static_cast<std::vector<value>*>(fieldsValue.ptr);
        }
        if (fields.size() != 6){
            a.vm->raiseError("ValueError", "fields is not a 6-tuple");
            return a.none();
        }
        long long f[6];
        for (int i = 0; i < 6; ++i) f[i] = intOf(*a.vm, fields[i]);
        bytes[0] = static_cast<unsigned char>((f[0] >> 24) & 0xFF);
        bytes[1] = static_cast<unsigned char>((f[0] >> 16) & 0xFF);
        bytes[2] = static_cast<unsigned char>((f[0] >> 8) & 0xFF);
        bytes[3] = static_cast<unsigned char>(f[0] & 0xFF);
        bytes[4] = static_cast<unsigned char>((f[1] >> 8) & 0xFF);
        bytes[5] = static_cast<unsigned char>(f[1] & 0xFF);
        bytes[6] = static_cast<unsigned char>((f[2] >> 8) & 0xFF);
        bytes[7] = static_cast<unsigned char>(f[2] & 0xFF);
        bytes[8] = static_cast<unsigned char>(f[3] & 0xFF);
        bytes[9] = static_cast<unsigned char>(f[4] & 0xFF);
        for (int i = 0; i < 6; ++i)
            bytes[10 + i] = static_cast<unsigned char>((f[5] >> (8 * (5 - i))) & 0xFF);
        haveBytes = true;
    }
    if (!haveBytes && a.has_kwarg("int")){
        value intValue = a.kwarg("int");
        unsigned long long low = static_cast<unsigned long long>(intOf(*a.vm, intValue));
        for (int i = 15; i >= 8; --i){
            bytes[i] = static_cast<unsigned char>(low & 0xFF);
            low >>= 8;
        }
        haveBytes = true;
    }
    if (!haveBytes){
        a.vm->raiseError("TypeError", "one of the hex, bytes, bytes_le, fields, "
                        "or int arguments must be given");
        return a.none();
    }
    return makeUuidObject(a, bytes, forcedVersion, applyVersion);
}

// UUID.__str__
// UUID.__str__
// UUID.__str__
static value fnUuidStr(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mkstr("");
    auto* st = static_cast<UuidState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!st) return a.mkstr("");
    return a.mkstr(canonical(st->bytes));
}

// UUID.__repr__
// UUID.__repr__
// UUID.__repr__
static value fnUuidRepr(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mkstr("UUID('')");
    auto* st = static_cast<UuidState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    return a.mkstr("UUID('" + (st ? canonical(st->bytes) : "") + "')");
}

// UUID.__hash__
// UUID.__hash__
// UUID.__hash__
static value fnUuidHash(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mkint(0);
    auto* st = static_cast<UuidState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!st) return a.mkint(0);
    unsigned long long h = 0;
    for (int i = 0; i < 16; ++i)
        h = (h ^ st->bytes[i]) * 0x100000001B3ULL;
    long long signedHash = static_cast<long long>(h & 0x7FFFFFFFFFFFFFFFULL);
    return a.mkint(signedHash);
}

// UUID.__eq__
// UUID.__eq__
// UUID.__eq__
static value fnUuidEq(cffi& a){
    value self = a.self();
    value other = a.arg(0);
    if (self.type != "native_object" || !self.ptr) return a.mkbool(false);
    if (other.type != "native_object" || !other.ptr) return a.mkbool(false);
    auto* a1 = static_cast<UuidState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    auto* a2 = static_cast<UuidState*>(static_cast<ffi_instance*>(other.ptr)->handle);
    if (!a1 || !a2) return a.mkbool(false);
    return a.mkbool(a1->bytes == a2->bytes);
}

// uuid4 生成
// Generate uuid4
// uuid4 ᠭᠠᠷᠭᠠᠬᠤ
value generateV4(cffi& a){
    std::array<unsigned char, 16> b{};
    uint64_t r1 = random64(), r2 = random64();
    for (int i = 0; i < 8; ++i) b[i] = static_cast<unsigned char>((r1 >> (8 * i)) & 0xFF);
    for (int i = 0; i < 8; ++i) b[8 + i] = static_cast<unsigned char>((r2 >> (8 * i)) & 0xFF);
    setVersion(b, 4);
    setRfcVariant(b);
    return makeUuidObject(a, b, 4, false);
}

// uuid1 生成
// Generate uuid1
// uuid1 ᠭᠠᠷᠭᠠᠬᠤ
value generateV1(cffi& a, long long nodeArg, long long clockSeqArg){
    using namespace std::chrono;
    uint64_t now = static_cast<uint64_t>(
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
    uint64_t timestamp = (now / 100ULL) + 0x01B21DD213814000ULL;
    uint64_t node = nodeArg >= 0 ? static_cast<uint64_t>(nodeArg) : (random64() & 0xFFFFFFFFFFFFULL);
    long long clockSeq = clockSeqArg >= 0 ? (clockSeqArg & 0x3FFF)
                                         : static_cast<long long>(random64() & 0x3FFF);
    std::array<unsigned char, 16> b{};
    b[0] = static_cast<unsigned char>((timestamp >> 24) & 0xFF);
    b[1] = static_cast<unsigned char>((timestamp >> 16) & 0xFF);
    b[2] = static_cast<unsigned char>((timestamp >> 8) & 0xFF);
    b[3] = static_cast<unsigned char>(timestamp & 0xFF);
    b[4] = static_cast<unsigned char>((timestamp >> 40) & 0xFF);
    b[5] = static_cast<unsigned char>((timestamp >> 32) & 0xFF);
    b[6] = static_cast<unsigned char>(((timestamp >> 56) & 0x0F) | 0x10);
    b[7] = static_cast<unsigned char>((timestamp >> 48) & 0xFF);
    b[8] = static_cast<unsigned char>(0x80 | ((clockSeq >> 8) & 0x3F));
    b[9] = static_cast<unsigned char>(clockSeq & 0xFF);
    for (int i = 0; i < 6; ++i)
        b[10 + i] = static_cast<unsigned char>((node >> (8 * (5 - i))) & 0xFF);
    return makeUuidObject(a, b, 1, false);
}

// 基于名字的 UUID
// Name-based UUID
// ᠨᠡᠷᠡ ᠲᠡᠢ UUID
value generateNamed(cffi& a, const value& namespaceValue, const std::string& name, bool useSha1){
    std::array<unsigned char, 16> nsBytes{};
    if (namespaceValue.type == "native_object" && namespaceValue.ptr){
        auto* st = static_cast<UuidState*>(
            static_cast<ffi_instance*>(namespaceValue.ptr)->handle);
        if (st) nsBytes = st->bytes;
    }
    std::string data;
    data.append(reinterpret_cast<const char*>(nsBytes.data()), 16);
    data += name;
    std::array<unsigned char, 16> b{};
    if (useSha1){
        auto digest = sha1(data);
        std::memcpy(b.data(), digest.data(), 16);
        setVersion(b, 5);
    } else {
        auto digest = md5(data);
        std::memcpy(b.data(), digest.data(), 16);
        setVersion(b, 3);
    }
    setRfcVariant(b);
    return makeUuidObject(a, b, useSha1 ? 5 : 3, false);
}

// uuid.uuid1
// uuid.uuid1
// uuid.uuid1
static value fnUuid1(cffi& a){
    long long node = -1, clockSeq = -1;
    if (a.has_kwarg("node")) node = a.kwarg_int("node", -1);
    else if (a.argc() > 0 && a.arg(0).type != "none") node = a.arg_int(0);
    if (a.has_kwarg("clock_seq")) clockSeq = a.kwarg_int("clock_seq", -1);
    else if (a.argc() > 1 && a.arg(1).type != "none") clockSeq = a.arg_int(1);
    return generateV1(a, node, clockSeq);
}

// uuid.uuid4
// uuid.uuid4
// uuid.uuid4
static value fnUuid4(cffi& a){
    return generateV4(a);
}

// uuid.uuid3
// uuid.uuid3
// uuid.uuid3
static value fnUuid3(cffi& a){
    return generateNamed(a, a.arg(0), a.arg_str(1), false);
}

// uuid.uuid5
// uuid.uuid5
// uuid.uuid5
static value fnUuid5(cffi& a){
    return generateNamed(a, a.arg(0), a.arg_str(1), true);
}

// uuid.getnode
// uuid.getnode
// uuid.getnode
static value fnGetnode(cffi& a){
    return a.mkint(static_cast<long long>(random64() & 0xFFFFFFFFFFFFULL));
}

// 命名空间常量
// Namespace constants
// ᠨᠡᠷᠡ ᠦᠨ ᠭᠠᠵᠠᠷ
value namespaceData(cffi& a, const std::string& hex){
    std::array<unsigned char, 16> b{};
    parseHex(hex, b);
    return makeUuidObject(a, b, 0, false);
}

static value dataNsDns(cffi& a){ return namespaceData(a, "6ba7b8109dad11d180b400c04fd430c8"); }
static value dataNsUrl(cffi& a){ return namespaceData(a, "6ba7b8119dad11d180b400c04fd430c8"); }
static value dataNsOid(cffi& a){ return namespaceData(a, "6ba7b8129dad11d180b400c04fd430c8"); }
static value dataNsX500(cffi& a){ return namespaceData(a, "6ba7b8149dad11d180b400c04fd430c8"); }

}

// cffi uuid lib
// cffi uuid lib
// cffi uuid lib
ffilib cffi_uuid_lib(){
    ffi_class uuidClass;
    uuidClass.name = "UUID";
    uuidClass.constructor = (void*)fnUuidCtor;
    uuidClass.methods["__str__"] = (void*)fnUuidStr;
    uuidClass.methods["__repr__"] = (void*)fnUuidRepr;
    uuidClass.methods["__hash__"] = (void*)fnUuidHash;
    uuidClass.methods["__eq__"] = (void*)fnUuidEq;

    return cffi_lib(
        "uuid",
        {
            {"uuid1", (void*)fnUuid1},
            {"uuid3", (void*)fnUuid3},
            {"uuid4", (void*)fnUuid4},
            {"uuid5", (void*)fnUuid5},
            {"getnode", (void*)fnGetnode}
        },
        {
            {"NAMESPACE_DNS", (void*)dataNsDns},
            {"NAMESPACE_URL", (void*)dataNsUrl},
            {"NAMESPACE_OID", (void*)dataNsOid},
            {"NAMESPACE_X500", (void*)dataNsX500}
        },
        {
            {"UUID", uuidClass}
        });
}

}
