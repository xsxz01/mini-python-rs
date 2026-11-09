#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace minipy {




// 原始字节内容
// raw byte content
// ᠬᠠᠭᠤᠴᠢᠨ ᠪᠠᠶᠢᠲ᠋ ᠠᠭᠤᠯᠭ᠎ᠠ
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

// 是否 十六进制 字符
// is hex digit
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠰᠢᠷᠠᠭᠤ ᠲᠣᠭ᠎ᠠ
static bool isHexDigit(char c){
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// 十六进制 数值
// hex value
// ᠰᠢᠷᠠᠭᠤ ᠲᠣᠭ᠎ᠠ ᠤᠳᠠᠭ᠌
static int hexValue(char c){
    unsigned char u = static_cast<unsigned char>(c);
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'a' && u <= 'f') return u - 'a' + 10;
    if (u >= 'A' && u <= 'F') return u - 'A' + 10;
    return -1;
}

// 是否 空白 字符
// is space char
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠰᠢᠯᠠᠭᠤᠨ ᠦᠰᠦᠭ
static bool isSpaceChar(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// Base64 编码表
// base64 alphabet
// base64 ᠦᠰᠦᠭ ᠨᠠᠮᠠᠭ᠌
static const char* kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 编码
// base64 encode
// base64 ᠺᠣᠳ᠋ᠯᠦᠯ
static std::string encodeBase64(const std::string& raw){
    std::string out;
    size_t i = 0;
    while (i + 2 < raw.size()){
        unsigned int n = (static_cast<unsigned char>(raw[i]) << 16) |
                         (static_cast<unsigned char>(raw[i + 1]) << 8) |
                          static_cast<unsigned char>(raw[i + 2]);
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3F]);
        out.push_back(kBase64Alphabet[(n >> 6) & 0x3F]);
        out.push_back(kBase64Alphabet[n & 0x3F]);
        i += 3;
    }
    if (i < raw.size()){
        unsigned int n = static_cast<unsigned char>(raw[i]) << 16;
        bool two = (i + 1 < raw.size());
        if (two) n |= static_cast<unsigned char>(raw[i + 1]) << 8;
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3F]);
        out.push_back(two ? kBase64Alphabet[(n >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

// Base64 解码
// base64 decode
// base64 ᠬᠡᠯᠡᠬᠦ
static int base64Value(char c){
    unsigned char u = static_cast<unsigned char>(c);
    if (u >= 'A' && u <= 'Z') return u - 'A';
    if (u >= 'a' && u <= 'z') return u - 'a' + 26;
    if (u >= '0' && u <= '9') return u - '0' + 52;
    if (u == '+') return 62;
    if (u == '/') return 63;
    return 64;
}

static bool decodeBase64(cffi& a, const std::string& input, bool strict,
                         std::string& out, std::string& error){
    (void)a;
    out.clear();
    int quadPos = 0;
    unsigned char leftChar = 0;
    int pads = 0;
    size_t outBytes = 0;
    bool failed = false;
    for (size_t i = 0; i < input.size() && !failed; ++i){
        unsigned char ch = static_cast<unsigned char>(input[i]);
        if (ch == '='){
            ++pads;
            if (quadPos >= 2 && quadPos + pads <= 4) continue;
            if (!strict) continue;
            if (quadPos == 1){
                break;
            }
            error = (quadPos == 0 && i == 0) ? "Leading padding not allowed"
                                             : "Excess padding not allowed";
            failed = true;
            break;
        }
        int v = base64Value(static_cast<char>(ch));
        if (v >= 64){
            if (strict){
                error = "Only base64 data is allowed";
                failed = true;
                break;
            }
            continue;
        }
        if (pads && strict){
            error = (quadPos + pads == 4) ? "Excess data after padding"
                                          : "Discontinuous padding not allowed";
            failed = true;
            break;
        }
        pads = 0;
        switch (quadPos){
            case 0:
                quadPos = 1;
                leftChar = static_cast<unsigned char>(v);
                break;
            case 1:
                quadPos = 2;
                out.push_back(static_cast<char>((leftChar << 2) | (v >> 4)));
                ++outBytes;
                leftChar = static_cast<unsigned char>(v & 0x0f);
                break;
            case 2:
                quadPos = 3;
                out.push_back(static_cast<char>((leftChar << 4) | (v >> 2)));
                ++outBytes;
                leftChar = static_cast<unsigned char>(v & 0x03);
                break;
            default:
                quadPos = 0;
                out.push_back(static_cast<char>((leftChar << 6) | v));
                ++outBytes;
                leftChar = 0;
                break;
        }
    }
    if (failed) return false;
    if (quadPos == 1){
        error = "Invalid base64-encoded string: number of data characters (" +
                std::to_string((outBytes / 3) * 4 + 1) +
                ") cannot be 1 more than a multiple of 4";
        return false;
    }
    if (quadPos != 0 && quadPos + pads < 4){
        error = "Incorrect padding";
        return false;
    }
    return true;
}

// fn A2b Base64
// fn a2b base64
// fn a2b base64
static value fnA2bBase64(cffi& a){
    std::string raw = rawOf(a.arg(0));
    bool strict = a.has_kwarg("strict_mode") ? a.kwarg_bool("strict_mode", false) : false;
    std::string out, error;
    if (!decodeBase64(a, raw, strict, out, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    return mkBytes(a, out);
}

// fn B2a Base64
// fn b2a base64
// fn b2a base64
static value fnB2aBase64(cffi& a){
    std::string raw = rawOf(a.arg(0));
    bool newline = a.has_kwarg("newline") ? a.kwarg_bool("newline", true) : true;
    std::string out = encodeBase64(raw);
    if (newline) out += '\n';
    return mkBytes(a, out);
}

// fn A2b Hex
// fn a2b hex
// fn a2b hex
static value fnA2bHex(cffi& a){
    std::string raw = rawOf(a.arg(0));
    if (raw.size() % 2 != 0){
        a.vm->raiseError("Error", "Odd-length string");
        return a.none();
    }
    std::string out;
    for (size_t i = 0; i + 1 < raw.size(); i += 2){
        int hi = hexValue(raw[i]);
        int lo = hexValue(raw[i + 1]);
        if (hi < 0 || lo < 0){
            a.vm->raiseError("Error", "Non-hexadecimal digit found");
            return a.none();
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return mkBytes(a, out);
}

// fn B2a Hex
// fn b2a hex
// fn b2a hex
static value fnB2aHex(cffi& a){
    std::string raw = rawOf(a.arg(0));
    std::string out;
    char buf[4];
    for (unsigned char ch : raw){
        std::snprintf(buf, sizeof(buf), "%02x", ch);
        out += buf;
    }
    return mkBytes(a, out);
}

// fn Crc32
// fn crc32
// fn crc32
static value fnCrc32(cffi& a){
    static uint32_t table[256];
    static bool ready = false;
    if (!ready){
        for (uint32_t i = 0; i < 256; ++i){
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    std::string raw = rawOf(a.arg(0));
    unsigned long long crc = 0;
    if (a.argc() >= 2) crc = static_cast<unsigned long long>(a.arg_int(1));
    uint32_t c = static_cast<uint32_t>(crc) ^ 0xFFFFFFFFu;
    for (unsigned char ch : raw)
        c = table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return a.mkint(static_cast<long long>(c ^ 0xFFFFFFFFu));
}

// fn Crc Hqx
// fn crc hqx
// fn crc hqx
static value fnCrcHqx(cffi& a){
    static uint16_t table[256];
    static bool ready = false;
    if (!ready){
        for (uint16_t i = 0; i < 256; ++i){
            uint16_t c = static_cast<uint16_t>(i << 8);
            for (int k = 0; k < 8; ++k)
                c = static_cast<uint16_t>((c & 0x8000) ? ((c << 1) ^ 0x1021) : (c << 1));
            table[i] = c;
        }
        ready = true;
    }
    std::string raw = rawOf(a.arg(0));
    unsigned long long crc = 0;
    if (a.argc() >= 2) crc = static_cast<unsigned long long>(a.arg_int(1));
    uint16_t c = static_cast<uint16_t>(crc);
    for (unsigned char ch : raw)
        c = static_cast<uint16_t>((c << 8) ^ table[((c >> 8) ^ ch) & 0xFF]);
    return a.mkint(static_cast<long long>(c));
}

// fn A2b Qp
// fn a2b qp
// fn a2b qp
static value fnA2bQp(cffi& a){
    std::string raw = rawOf(a.arg(0));
    bool header = false;
    if (a.has_kwarg("header")) header = a.kwarg_bool("header", false);
    else if (a.argc() >= 2) header = a.vm->truthy(a.arg(1));
    std::string out;
    size_t i = 0;
    while (i < raw.size()){
        if (raw[i] == '='){
            ++i;
            if (i >= raw.size()) break;
            if (raw[i] == '\n' || raw[i] == '\r'){
                if (raw[i] != '\n'){
                    while (i < raw.size() && raw[i] != '\n') ++i;
                }
                if (i < raw.size()) ++i;
            } else if (raw[i] == '='){
                out.push_back('=');
                ++i;
            } else if (i + 1 < raw.size() && isHexDigit(raw[i]) && isHexDigit(raw[i + 1])){
                int hi = hexValue(raw[i]);
                int lo = hexValue(raw[i + 1]);
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back('=');
            }
        } else if (header && raw[i] == '_'){
            out.push_back(' ');
            ++i;
        } else {
            out.push_back(raw[i]);
            ++i;
        }
    }
    return mkBytes(a, out);
}

// fn B2a Qp
// fn b2a qp
// fn b2a qp
static value fnB2aQp(cffi& a){
    std::string raw = rawOf(a.arg(0));
    bool quotetabs = false;
    bool istext = true;
    bool header = false;
    if (a.has_kwarg("quotetabs")) quotetabs = a.kwarg_bool("quotetabs", false);
    else if (a.argc() >= 2) quotetabs = a.vm->truthy(a.arg(1));
    if (a.has_kwarg("istext")) istext = a.kwarg_bool("istext", true);
    else if (a.argc() >= 3) istext = a.vm->truthy(a.arg(2));
    if (a.has_kwarg("header")) header = a.kwarg_bool("header", false);
    else if (a.argc() >= 4) header = a.vm->truthy(a.arg(3));

    size_t n = raw.size();
    bool crlf = false;
    for (size_t i = 1; i < n; ++i){
        if (raw[i] == '\n' && raw[i - 1] == '\r'){
            crlf = true;
            break;
        }
    }

    std::string out;
    size_t lineLen = 0;
    size_t in = 0;
    while (in < n){
        unsigned char ch = static_cast<unsigned char>(raw[in]);
        bool encode = false;
        if (ch > 126 || ch == '=' || (header && ch == '_')){
            encode = true;
        } else if (ch == '.' && lineLen == 0 &&
                   (in + 1 == n || raw[in + 1] == '\n' || raw[in + 1] == '\r' ||
                    raw[in + 1] == '\0')){
            encode = true;
        } else if (!istext && (ch == '\r' || ch == '\n')){
            encode = true;
        } else if ((ch == '\t' || ch == ' ') && in + 1 == n){
            encode = true;
        } else if (ch < 33 && ch != '\r' && ch != '\n' &&
                   (quotetabs || (ch != '\t' && ch != ' '))){
            encode = true;
        }

        if (encode){
            if (lineLen + 3 >= 76){
                out.push_back('=');
                if (crlf) out.push_back('\r');
                out.push_back('\n');
                lineLen = 0;
            }
            char buf[8];
            std::snprintf(buf, sizeof(buf), "=%02X", ch);
            out += buf;
            ++in;
            lineLen += 3;
        } else if (istext &&
                   (ch == '\n' || (in + 1 < n && ch == '\r' && raw[in + 1] == '\n'))){
            lineLen = 0;
            if (!out.empty() && (out.back() == ' ' || out.back() == '\t')){
                char prev = out.back();
                out.pop_back();
                char buf[8];
                std::snprintf(buf, sizeof(buf), "=%02X", static_cast<unsigned char>(prev));
                out += buf;
            }
            if (crlf) out.push_back('\r');
            out.push_back('\n');
            if (ch == '\r') in += 2;
            else ++in;
        } else {
            if (in + 1 != n && raw[in + 1] != '\n' && lineLen + 1 >= 76){
                out.push_back('=');
                if (crlf) out.push_back('\r');
                out.push_back('\n');
                lineLen = 0;
            }
            ++lineLen;
            if (header && ch == ' ') out.push_back('_');
            else out.push_back(static_cast<char>(ch));
            ++in;
        }
    }
    return mkBytes(a, out);
}

// fn A2b Uu
// fn a2b uu
// fn a2b uu
static value fnA2bUu(cffi& a){
    std::string raw = rawOf(a.arg(0));
    if (raw.empty()){
        a.vm->raiseError("Error", "Missing length byte");
        return a.none();
    }
    long long binLen = (static_cast<unsigned char>(raw[0]) - ' ') & 077;
    long long asciiLen = static_cast<long long>(raw.size()) - 1;
    size_t idx = 1;
    std::string out(static_cast<size_t>(binLen), '\0');
    size_t binPos = 0;
    unsigned int leftchar = 0;
    int leftbits = 0;
    long long remaining = binLen;
    for (; remaining > 0; --asciiLen, ++idx){
        unsigned char thisCh = (asciiLen > 0 && idx < raw.size())
                                   ? static_cast<unsigned char>(raw[idx])
                                   : 0;
        if (thisCh == '\n' || thisCh == '\r' || asciiLen <= 0){
            thisCh = 0;
        } else {
            if (thisCh < ' ' || thisCh > (' ' + 64)){
                a.vm->raiseError("Error", "Illegal char");
                return a.none();
            }
            thisCh = static_cast<unsigned char>((static_cast<int>(thisCh) - ' ') & 077);
        }
        leftchar = (leftchar << 6) | thisCh;
        leftbits += 6;
        if (leftbits >= 8){
            leftbits -= 8;
            out[binPos++] = static_cast<char>((leftchar >> leftbits) & 0xFF);
            leftchar &= ((1u << leftbits) - 1);
            --remaining;
        }
    }
    while (asciiLen-- > 0){
        unsigned char thisCh = (idx < raw.size())
                                   ? static_cast<unsigned char>(raw[idx])
                                   : 0;
        ++idx;
        if (thisCh != ' ' && thisCh != static_cast<unsigned char>(' ' + 64) &&
            thisCh != '\n' && thisCh != '\r'){
            a.vm->raiseError("Error", "Trailing garbage");
            return a.none();
        }
    }
    return mkBytes(a, out);
}

// fn B2a Uu
// fn b2a uu
// fn b2a uu
static value fnB2aUu(cffi& a){
    std::string raw = rawOf(a.arg(0));
    bool backtick = a.has_kwarg("backtick") ? a.kwarg_bool("backtick", false)
                                            : (a.argc() >= 2 ? a.vm->truthy(a.arg(1)) : false);
    if (raw.size() > 45){
        a.vm->raiseError("Error", "At most 45 bytes at once");
        return a.none();
    }
    std::string out;
    if (backtick && raw.empty()) out.push_back('`');
    else out.push_back(static_cast<char>(' ' + static_cast<unsigned char>(raw.size())));

    unsigned int leftchar = 0;
    int leftbits = 0;
    long long binLen = static_cast<long long>(raw.size());
    size_t idx = 0;
    for (; binLen > 0 || leftbits != 0; --binLen, ++idx){
        if (binLen > 0) leftchar = (leftchar << 8) | static_cast<unsigned char>(raw[idx]);
        else leftchar <<= 8;
        leftbits += 8;
        while (leftbits >= 6){
            unsigned char ch = static_cast<unsigned char>((leftchar >> (leftbits - 6)) & 0x3F);
            leftbits -= 6;
            if (backtick && ch == 0) out.push_back('`');
            else out.push_back(static_cast<char>(ch + ' '));
        }
    }
    out.push_back('\n');
    return mkBytes(a, out);
}

// cffi binascii lib
// cffi binascii library
// cffi binascii ᠨᠣᠮ
ffilib cffi_binascii_lib(){
    ffi_class errorClass;
    errorClass.name = "Error";
    ffi_class incompleteClass;
    incompleteClass.name = "Incomplete";

    return cffi_lib(
        "binascii",
        {
            {"a2b_base64", (void*)fnA2bBase64},
            {"b2a_base64", (void*)fnB2aBase64},
            {"a2b_hex", (void*)fnA2bHex},
            {"unhexlify", (void*)fnA2bHex},
            {"b2a_hex", (void*)fnB2aHex},
            {"hexlify", (void*)fnB2aHex},
            {"crc32", (void*)fnCrc32},
            {"crc_hqx", (void*)fnCrcHqx},
            {"a2b_qp", (void*)fnA2bQp},
            {"b2a_qp", (void*)fnB2aQp},
            {"a2b_uu", (void*)fnA2bUu},
            {"b2a_uu", (void*)fnB2aUu}
        },
        {},
        {
            {"Error", errorClass},
            {"Incomplete", incompleteClass}
        });
}


}
