#include "../cffi.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 读取整个文件 (音频头通常不大, 读前 64KB 足够)
// Read the file head (64KB is enough for audio headers)
// ᠠᠶᠠᠰᠤᠨ ᠦᠨ ᠲᠣᠯᠤᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
std::string readHead(const std::string& path){
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::string data;
    data.resize(65536);
    in.read(data.data(), static_cast<std::streamsize>(data.size()));
    data.resize(static_cast<size_t>(in.gcount()));
    return data;
}

std::string rawOf(const value& v){
    if ((v.type == "bytes" || v.type == "bytearray" || v.type == "str") && v.ptr)
        return *static_cast<std::string*>(v.ptr);
    return "";
}

uint32_t rd32(const std::string& h, size_t off){
    if (off + 4 > h.size()) return 0;
    return (uint32_t(static_cast<unsigned char>(h[off])) << 24) |
           (uint32_t(static_cast<unsigned char>(h[off+1])) << 16) |
           (uint32_t(static_cast<unsigned char>(h[off+2])) << 8) |
           uint32_t(static_cast<unsigned char>(h[off+3]));
}

uint32_t rd32le(const std::string& h, size_t off){
    if (off + 4 > h.size()) return 0;
    return (uint32_t(static_cast<unsigned char>(h[off+3])) << 24) |
           (uint32_t(static_cast<unsigned char>(h[off+2])) << 16) |
           (uint32_t(static_cast<unsigned char>(h[off+1])) << 8) |
           uint32_t(static_cast<unsigned char>(h[off]));
}

uint16_t rd16(const std::string& h, size_t off){
    if (off + 2 > h.size()) return 0;
    return static_cast<uint16_t>((uint16_t(static_cast<unsigned char>(h[off])) << 8) |
                                uint16_t(static_cast<unsigned char>(h[off+1])));
}

uint16_t rd16le(const std::string& h, size_t off){
    if (off + 2 > h.size()) return 0;
    return static_cast<uint16_t>((uint16_t(static_cast<unsigned char>(h[off+1])) << 8) |
                                uint16_t(static_cast<unsigned char>(h[off])));
}

// 80 位扩展浮点 → double
// 80-bit extended float to double
// 80 ᠪᠢᠲ᠋ ᠬᠥᠪᠥᠷᠭ᠎ᠠ ᠲᠣᠭ᠎ᠠ
double extended80(const std::string& h, size_t off){
    if (off + 10 > h.size()) return 0.0;
    uint16_t exponent = rd16(h, off);
    uint64_t mantissa = 0;
    for (int i = 2; i < 10; ++i)
        mantissa = (mantissa << 8) | static_cast<unsigned char>(h[off + i]);
    if (exponent == 0 && mantissa == 0) return 0.0;
    double sign = (exponent & 0x8000) ? -1.0 : 1.0;
    int exp = (exponent & 0x7FFF) - 16383;
    return sign * static_cast<double>(mantissa) *
           std::pow(2.0, static_cast<double>(exp) - 63.0);
}

// 结果构造
// Build result tuple
// ᠦᠷᠳᠦᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeHeader(cffi& a, const std::string& filetype, long long channels,
                 long long frames, long long rate, long long sampwidth){
    return a.vm->makeTuple({a.mkstr(filetype), a.mkint(channels),
                            a.mkint(frames), a.mkint(rate), a.mkint(sampwidth)});
}

// WAV 解析
// Parse WAVE
// WAVE ᠲᠠᠢᠯᠢᠬᠤ
value parseWav(cffi& a, const std::string& h){
    if (h.size() < 12 || h.compare(0, 4, "RIFF") != 0 ||
        h.compare(8, 4, "WAVE") != 0) return a.none();
    size_t off = 12;
    long long channels = 0, rate = 0, bits = 0, blockAlign = 0, dataSize = 0;
    while (off + 8 <= h.size()){
        std::string id = h.substr(off, 4);
        uint32_t size = rd32le(h, off + 4);
        size_t body = off + 8;
        if (id == "fmt "){
            channels = rd16le(h, body + 2);
            rate = rd32le(h, body + 4);
            blockAlign = rd16le(h, body + 12);
            bits = rd16le(h, body + 14);
        } else if (id == "data"){
            dataSize = static_cast<long long>(size);
            break;
        }
        off = body + size + (size & 1);
    }
    long long frames = blockAlign > 0 ? dataSize / blockAlign : 0;
    long long sampwidth = bits / 8;
    return makeHeader(a, "wav", channels, frames, rate, sampwidth);
}

// AU 解析
// Parse AU
// AU ᠲᠠᠢᠯᠢᠬᠤ
value parseAu(cffi& a, const std::string& h){
    if (h.size() < 24 || h.compare(0, 4, ".snd") != 0) return a.none();
    long long dataSize = static_cast<long long>(rd32(h, 8));
    long long encoding = static_cast<long long>(rd32(h, 12));
    long long rate = static_cast<long long>(rd32(h, 16));
    long long channels = static_cast<long long>(rd32(h, 20));
    long long sampwidth = 1;
    if (encoding == 3) sampwidth = 2;
    else if (encoding == 4) sampwidth = 3;
    else if (encoding == 5) sampwidth = 4;
    long long frames = sampwidth > 0 && channels > 0
                     ? dataSize / (sampwidth * channels) : 0;
    return makeHeader(a, "au", channels, frames, rate, sampwidth);
}

// AIFF 解析
// Parse AIFF
// AIFF ᠲᠠᠢᠯᠢᠬᠤ
value parseAiff(cffi& a, const std::string& h){
    if (h.size() < 12 || h.compare(0, 4, "FORM") != 0) return a.none();
    std::string form = h.substr(8, 4);
    if (form != "AIFF" && form != "AIFC") return a.none();
    size_t off = 12;
    long long channels = 0, frames = 0, bits = 0;
    double rate = 0.0;
    while (off + 8 <= h.size()){
        std::string id = h.substr(off, 4);
        uint32_t size = rd32(h, off + 4);
        size_t body = off + 8;
        if (id == "COMM"){
            channels = rd16(h, body);
            frames = static_cast<long long>(rd32(h, body + 2));
            bits = rd16(h, body + 6);
            rate = extended80(h, body + 8);
            break;
        }
        off = body + size + (size & 1);
    }
    long long sampwidth = bits / 8;
    return makeHeader(a, "aiff", channels, frames,
                      static_cast<long long>(rate), sampwidth);
}

// 8SVX 解析
// Parse 8SVX
// 8SVX ᠲᠠᠢᠯᠢᠬᠤ
value parse8svx(cffi& a, const std::string& h){
    if (h.size() < 12 || h.compare(0, 4, "FORM") != 0 ||
        h.compare(8, 4, "8SVX") != 0) return a.none();
    size_t off = 12;
    long long frames = 0, rate = 0;
    while (off + 8 <= h.size()){
        std::string id = h.substr(off, 4);
        uint32_t size = rd32(h, off + 4);
        size_t body = off + 8;
        if (id == "VHDR"){
            frames = static_cast<long long>(rd32(h, body));
            rate = static_cast<long long>(rd16(h, body + 12));
            break;
        }
        off = body + size + (size & 1);
    }
    return makeHeader(a, "8svx", 1, frames, rate, 1);
}

// sndhdr.what
// sndhdr.what
// sndhdr.what
static value fnWhat(cffi& a){
    value fileArg = a.arg(0);
    std::string h;
    if (a.argc() > 1 && a.arg(1).type != "none"){
        h = rawOf(a.arg(1));
    } else if (fileArg.type == "str" && fileArg.ptr){
        h = readHead(*static_cast<std::string*>(fileArg.ptr));
    } else if (fileArg.type == "bytes" && fileArg.ptr){
        h = *static_cast<std::string*>(fileArg.ptr);
    } else {
        value readMethod = a.vm->getAttribute(fileArg, "read");
        if (!a.vm->hasError()){
            value chunk = a.vm->callValue(readMethod, {a.mkint(65536)});
            if (!a.vm->hasError()) h = rawOf(chunk);
        }
        a.vm->clearErr();
    }
    value result = parseWav(a, h);
    if (result.type != "none") return result;
    result = parseAu(a, h);
    if (result.type != "none") return result;
    result = parseAiff(a, h);
    if (result.type != "none") return result;
    result = parse8svx(a, h);
    if (result.type != "none") return result;
    return a.none();
}

// sndhdr.tests 占位 (空列表)
// sndhdr.tests placeholder (empty list)
// sndhdr.tests
static value dataTests(cffi& a){
    return a.mklist({});
}

}

// cffi sndhdr lib
// cffi sndhdr lib
// cffi sndhdr lib
ffilib cffi_sndhdr_lib(){
    return cffi_lib(
        "sndhdr",
        {
            {"what", (void*)fnWhat}
        },
        {
            {"tests", (void*)dataTests}
        });
}

}
