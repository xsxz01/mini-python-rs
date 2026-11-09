#include "../cffi.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 全局随机源
// Global random engine
// ᠨᠢᠭᠡᠨᠳᠦ ᠰᠠᠨᠠᠮᠠᠯ ᠤᠨ ᠡᠬᠢ
std::mt19937_64& randomEngine(){
    static std::mt19937_64 engine(std::random_device{}());
    return engine;
}

// 生成随机字节串
// Generate random bytes
// ᠰᠠᠨᠠᠮᠠᠯ ᠪᠠᠶᠢᠲ᠋ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
std::string randomBytes(size_t count){
    std::string out;
    out.reserve(count);
    while (out.size() < count){
        uint64_t value = randomEngine()();
        for (int i = 0; i < 8 && out.size() < count; ++i){
            out.push_back(static_cast<char>(value & 0xFF));
            value >>= 8;
        }
    }
    return out;
}

// 构造 bytes 值 (底层复用字符串存储)
// Build a bytes value (backed by a string)
// bytes ᠭᠠᠷᠴᠠᠭ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeBytes(cffi& a, const std::string& raw){
    value result = a.mkstr(raw);
    result.type = "bytes";
    return result;
}

// URL 安全 Base64 编码
// URL-safe Base64 encoding without padding
// URL ᠠᠰᠠᠭᠤᠯᠳᠠ ᠦᠭᠡᠢ Base64 ᠴᠢᠨᠠᠭᠠᠯ
std::string urlSafeBase64(const std::string& raw){
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    size_t i = 0;
    while (i + 3 <= raw.size()){
        unsigned int value = (static_cast<unsigned char>(raw[i]) << 16) |
                             (static_cast<unsigned char>(raw[i + 1]) << 8) |
                             static_cast<unsigned char>(raw[i + 2]);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
        out.push_back(alphabet[value & 0x3F]);
        i += 3;
    }
    size_t remaining = raw.size() - i;
    if (remaining == 1){
        unsigned int value = static_cast<unsigned char>(raw[i]) << 16;
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
    } else if (remaining == 2){
        unsigned int value = (static_cast<unsigned char>(raw[i]) << 16) |
                             (static_cast<unsigned char>(raw[i + 1]) << 8);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
    }
    return out;
}

// 读取字节数参数
// Read the nbytes argument
// nbytes ᠬᠡᠮᠵᠢᠶ᠎ᠡ ᠢ ᠦᠨᠰᠢᠬᠦ
bool readByteCount(cffi& a, size_t& count){
    count = 32;
    if (a.argc() >= 1 && a.arg(0).type != "none"){
        long long value = a.arg_int(0);
        if (value < 0){
            a.vm->raiseError("ValueError", "number of bytes must be non-negative");
            return false;
        }
        count = static_cast<size_t>(value);
    }
    return true;
}
}

// fn Choice
// fn choice
// fn Choice
static value fnChoice(cffi& a){
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (a.vm->hasError()) return a.none();
    if (items.empty()){
        a.vm->raiseError("IndexError", "Cannot choose from an empty sequence");
        return a.none();
    }
    size_t index = static_cast<size_t>(randomEngine()() % items.size());
    return items[index];
}

// fn Randbelow
// fn randbelow
// fn Randbelow
static value fnRandbelow(cffi& a){
    long long bound = a.arg_int(0);
    if (bound <= 0){
        a.vm->raiseError("ValueError", "Upper bound must be positive");
        return a.none();
    }
    uint64_t value = randomEngine()() % static_cast<uint64_t>(bound);
    return a.mkint(static_cast<long long>(value));
}

// fn Randbits
// fn randbits
// fn Randbits
static value fnRandbits(cffi& a){
    long long count = a.arg_int(0);
    if (count < 0){
        a.vm->raiseError("ValueError", "number of bits must be non-negative");
        return a.none();
    }
    if (count > 63){
        a.vm->raiseError("ValueError", "number of bits must be at most 63");
        return a.none();
    }
    if (count == 0) return a.mkint(0);
    uint64_t mask = (1ULL << count) - 1ULL;
    return a.mkint(static_cast<long long>(randomEngine()() & mask));
}

// fn Token Bytes
// fn token_bytes
// fn Token Bytes
static value fnTokenBytes(cffi& a){
    size_t count = 0;
    if (!readByteCount(a, count)) return a.none();
    return makeBytes(a, randomBytes(count));
}

// fn Token Hex
// fn token_hex
// fn Token Hex
static value fnTokenHex(cffi& a){
    size_t count = 0;
    if (!readByteCount(a, count)) return a.none();
    std::string raw = randomBytes(count);
    std::string out;
    out.reserve(raw.size() * 2);
    for (unsigned char c : raw){
        char buffer[4];
        std::snprintf(buffer, sizeof(buffer), "%02x", c);
        out += buffer;
    }
    return a.mkstr(out);
}

// fn Token Urlsafe
// fn token_urlsafe
// fn Token Urlsafe
static value fnTokenUrlsafe(cffi& a){
    size_t count = 0;
    if (!readByteCount(a, count)) return a.none();
    return a.mkstr(urlSafeBase64(randomBytes(count)));
}

// fn Compare Digest
// fn compare_digest
// fn Compare Digest
static value fnCompareDigest(cffi& a){
    value left = a.arg(0), right = a.arg(1);
    if (left.type != "str" && left.type != "bytes"){
        a.vm->raiseError("TypeError", "unsupported operand types");
        return a.none();
    }
    if (right.type != "str" && right.type != "bytes"){
        a.vm->raiseError("TypeError", "unsupported operand types");
        return a.none();
    }
    std::string leftText = a.vm->toString(left);
    std::string rightText = a.vm->toString(right);
    size_t length = leftText.size() > rightText.size() ? leftText.size() : rightText.size();
    unsigned int diff = leftText.size() ^ rightText.size();
    for (size_t i = 0; i < length; ++i){
        unsigned char lc = i < leftText.size() ? static_cast<unsigned char>(leftText[i]) : 0;
        unsigned char rc = i < rightText.size() ? static_cast<unsigned char>(rightText[i]) : 0;
        diff |= static_cast<unsigned int>(lc ^ rc);
    }
    return a.mkbool(diff == 0);
}

// data Default Entropy
// data default entropy
// data Default Entropy
static value dataDefaultEntropy(cffi& a){ return a.mkint(32); }

// cffi secrets lib
// cffi secrets lib
// cffi secrets lib
ffilib cffi_secrets_lib(){
    return cffi_lib(
        "secrets",
        {
            {"choice", (void*)fnChoice},
            {"randbelow", (void*)fnRandbelow},
            {"randbits", (void*)fnRandbits},
            {"token_bytes", (void*)fnTokenBytes},
            {"token_hex", (void*)fnTokenHex},
            {"token_urlsafe", (void*)fnTokenUrlsafe},
            {"compare_digest", (void*)fnCompareDigest}
        },
        {
            {"DEFAULT_ENTROPY", (void*)dataDefaultEntropy}
        });
}

}
