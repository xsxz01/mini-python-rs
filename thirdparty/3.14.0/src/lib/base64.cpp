#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace minipy {






namespace {
const std::string kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// encode Base64
// encode base64
// encode Base64
std::string encodeBase64(const std::string& input, bool urlsafe){
    std::string alphabet = kAlphabet;
    if (urlsafe){
        alphabet[62] = '-';
        alphabet[63] = '_';
    }
    std::string result;
    size_t i = 0;
    while (i + 2 < input.size()){
        unsigned int value = (static_cast<unsigned char>(input[i]) << 16) |
                             (static_cast<unsigned char>(input[i + 1]) << 8) |
                             static_cast<unsigned char>(input[i + 2]);
        result += alphabet[(value >> 18) & 0x3F];
        result += alphabet[(value >> 12) & 0x3F];
        result += alphabet[(value >> 6) & 0x3F];
        result += alphabet[value & 0x3F];
        i += 3;
    }
    if (i < input.size()){
        unsigned int value = static_cast<unsigned char>(input[i]) << 16;
        bool hasSecond = (i + 1 < input.size());
        if (hasSecond) value |= static_cast<unsigned char>(input[i + 1]) << 8;
        result += alphabet[(value >> 18) & 0x3F];
        result += alphabet[(value >> 12) & 0x3F];
        result += hasSecond ? alphabet[(value >> 6) & 0x3F] : '=';
        result += '=';
    }
    return result;
}

// decode Base64
// decode base64
// decode Base64
std::string decodeBase64(const std::string& input){
    std::string data;
    for (char c : input){
        if (c == '-' || c == '_') data += c == '-' ? '+' : '/';
        else if (c != '=' && (isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '/')){
            data += c;
        }
    }
    auto valueOf = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string result;
    for (size_t i = 0; i + 3 < data.size(); i += 4){
        int a = valueOf(data[i]);
        int b = valueOf(data[i + 1]);
        int c = valueOf(data[i + 2]);
        int d = valueOf(data[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) continue;
        unsigned int value = (a << 18) | (b << 12) | (c << 6) | d;
        result += static_cast<char>((value >> 16) & 0xFF);
        result += static_cast<char>((value >> 8) & 0xFF);
        result += static_cast<char>(value & 0xFF);
    }
    return result;
}

// encode Base32
// encode base32
// encode Base32
std::string encodeBase32(const std::string& input){
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string result;
    for (size_t i = 0; i < input.size(); i += 5){
        size_t count = std::min<size_t>(5, input.size() - i);
        unsigned long long value = 0;
        for (size_t j = 0; j < count; ++j){
            value |= static_cast<unsigned long long>(static_cast<unsigned char>(input[i + j]))
                     << (32 - 8 * j);
        }
        size_t chars = (count * 8 + 4) / 5;
        for (size_t j = 0; j < chars; ++j){
            result += alphabet[(value >> (35 - 5 * j)) & 0x1F];
        }
        for (size_t j = chars; j < 8; ++j) result += '=';
    }
    return result;
}

// decode Base32
// decode base32
// decode Base32
bool decodeBase32(const std::string& input, std::string& output){
    auto valueOf = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c >= '2' && c <= '7') return c - '2' + 26;
        return -1;
    };
    unsigned long long buffer = 0;
    int bits = 0;
    for (char c : input){
        if (c == '=') continue;
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int value = valueOf(c);
        if (value < 0) return false;
        buffer = (buffer << 5) | static_cast<unsigned long long>(value);
        bits += 5;
        if (bits >= 8){
            bits -= 8;
            output += static_cast<char>((buffer >> bits) & 0xFF);
        }
    }
    return true;
}

// encode Base16
// encode base16
// encode Base16
std::string encodeBase16(const std::string& input){
    const char* digits = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() * 2);
    for (char c : input){
        unsigned int value = static_cast<unsigned char>(c);
        result += digits[value >> 4];
        result += digits[value & 0x0F];
    }
    return result;
}

// decode Base16
// decode base16
// decode Base16
int decodeBase16(const std::string& input, std::string& output){
    int pending = -1;
    for (char c : input){
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int value = -1;
        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
        if (value < 0) return 2;
        if (pending < 0){
            pending = value;
        } else {
            output += static_cast<char>((pending << 4) | value);
            pending = -1;
        }
    }
    return pending < 0 ? 0 : 1;
}

// decode Base64 Full
// decode base64 full
// decode Base64 Full
std::string decodeBase64Full(const std::string& input){
    std::string data;
    for (char c : input){
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '=') continue;
        if (c == '-') data += '+';
        else if (c == '_') data += '/';
        else if (std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '/') data += c;
    }
    auto valueOf = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string result;
    size_t i = 0;
    for (; i + 3 < data.size(); i += 4){
        int a = valueOf(data[i]);
        int b = valueOf(data[i + 1]);
        int c = valueOf(data[i + 2]);
        int d = valueOf(data[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) continue;
        unsigned int value = (a << 18) | (b << 12) | (c << 6) | d;
        result += static_cast<char>((value >> 16) & 0xFF);
        result += static_cast<char>((value >> 8) & 0xFF);
        result += static_cast<char>(value & 0xFF);
    }
    size_t remaining = data.size() - i;
    if (remaining >= 2){
        int a = valueOf(data[i]);
        int b = valueOf(data[i + 1]);
        if (a >= 0 && b >= 0){
            result += static_cast<char>(((a << 2) | (b >> 4)) & 0xFF);
            if (remaining >= 3){
                int c = valueOf(data[i + 2]);
                if (c >= 0) result += static_cast<char>(((b << 4) | (c >> 2)) & 0xFF);
            }
        }
    }
    return result;
}

// encode Base64 Lines
// encode base64 lines
// encode Base64 Lines
std::string encodeBase64Lines(const std::string& input){
    std::string encoded = encodeBase64(input, false);
    if (encoded.empty()) return "";
    std::string result;
    for (size_t i = 0; i < encoded.size(); i += 76){
        result += encoded.substr(i, 76);
        result += '\n';
    }
    return result;
}
}

// fn B64encode
// fn b64encode
// fn B64encode
static minipy::value fnB64encode(minipy::cffi& a){ return a.mkstr(encodeBase64(a.arg_str(0), false)); }

// fn B64decode
// fn b64decode
// fn B64decode
static minipy::value fnB64decode(minipy::cffi& a){ return a.mkstr(decodeBase64(a.arg_str(0))); }

// fn Urlsafe Encode
// fn urlsafe encode
// fn Urlsafe Encode
static minipy::value fnUrlsafeEncode(minipy::cffi& a){ return a.mkstr(encodeBase64(a.arg_str(0), true)); }

// fn Urlsafe Decode
// fn urlsafe decode
// fn Urlsafe Decode
static minipy::value fnUrlsafeDecode(minipy::cffi& a){ return a.mkstr(decodeBase64(a.arg_str(0))); }

// fn B32encode
// fn b32encode
// fn B32encode
static minipy::value fnB32encode(minipy::cffi& a){ return a.mkstr(encodeBase32(a.arg_str(0))); }

// fn B32decode
// fn b32decode
// fn B32decode
static minipy::value fnB32decode(minipy::cffi& a){
    std::string result;
    if (!decodeBase32(a.arg_str(0), result)){
        a.vm->raiseError("ValueError", "Non-base32 digit found");
        return a.none();
    }
    return a.mkstr(result);
}

// fn B16encode
// fn b16encode
// fn B16encode
static minipy::value fnB16encode(minipy::cffi& a){ return a.mkstr(encodeBase16(a.arg_str(0))); }

// fn B16decode
// fn b16decode
// fn B16decode
static minipy::value fnB16decode(minipy::cffi& a){
    std::string result;
    int status = decodeBase16(a.arg_str(0), result);
    if (status == 1){
        a.vm->raiseError("ValueError", "Odd-length string");
        return a.none();
    }
    if (status == 2){
        a.vm->raiseError("ValueError", "Non-hexadecimal digit found");
        return a.none();
    }
    return a.mkstr(result);
}

// fn Encodebytes
// fn encodebytes
// fn Encodebytes
static minipy::value fnEncodebytes(minipy::cffi& a){ return a.mkstr(encodeBase64Lines(a.arg_str(0))); }

// fn Decodebytes
// fn decodebytes
// fn Decodebytes
static minipy::value fnDecodebytes(minipy::cffi& a){ return a.mkstr(decodeBase64Full(a.arg_str(0))); }

// cffi base64 lib
// cffi base64 lib
// cffi base64 lib
minipy::ffilib cffi_base64_lib(){
    return minipy::cffi_lib(
        "base64",
        {
            {"b64encode", (void*)fnB64encode},
            {"b64decode", (void*)fnB64decode},
            {"standard_b64encode", (void*)fnB64encode},
            {"standard_b64decode", (void*)fnB64decode},
            {"urlsafe_b64encode", (void*)fnUrlsafeEncode},
            {"urlsafe_b64decode", (void*)fnUrlsafeDecode},
            {"b32encode", (void*)fnB32encode},
            {"b32decode", (void*)fnB32decode},
            {"b16encode", (void*)fnB16encode},
            {"b16decode", (void*)fnB16decode},
            {"encodebytes", (void*)fnEncodebytes},
            {"decodebytes", (void*)fnDecodebytes}
        });
}


}
