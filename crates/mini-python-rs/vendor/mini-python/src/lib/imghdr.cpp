#include "../cffi.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 读取文件头部
// Read a file header
// ᠲᠣᠯᠣᠭ᠎ᠠ ᠦᠨ ᠲᠣᠯᠤᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
std::string readHeader(const std::string& path, size_t limit = 8192){
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::string data;
    data.resize(limit);
    in.read(data.data(), static_cast<std::streamsize>(limit));
    data.resize(static_cast<size_t>(in.gcount()));
    return data;
}

// 原始内容
// Raw content of a value
// ᠪᠠᠶᠢᠲ᠋ ᠠᠭᠤᠯᠭ᠎ᠠ
std::string rawOf(const value& v){
    if ((v.type == "bytes" || v.type == "bytearray" || v.type == "str") && v.ptr)
        return *static_cast<std::string*>(v.ptr);
    return "";
}

bool startsWith(const std::string& h, const char* prefix){
    size_t n = std::strlen(prefix);
    return h.size() >= n && h.compare(0, n, prefix) == 0;
}

// 取测试头
// Resolve the header argument
// ᠲᠣᠯᠣᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
std::string resolveHeader(cffi& a){
    value fileArg = a.arg(0);
    if (a.argc() > 1 && a.arg(1).type != "none") return rawOf(a.arg(1));
    if (fileArg.type == "str" && fileArg.ptr)
        return readHeader(*static_cast<std::string*>(fileArg.ptr));
    if (fileArg.type == "bytes" && fileArg.ptr)
        return *static_cast<std::string*>(fileArg.ptr);
    // 文件对象: 调用 read(32)
    value readMethod = a.vm->getAttribute(fileArg, "read");
    if (!a.vm->hasError()){
        value chunk = a.vm->callValue(readMethod, {a.mkint(8192)});
        if (!a.vm->hasError()) return rawOf(chunk);
    }
    a.vm->clearErr();
    if (fileArg.type == "str" && fileArg.ptr)
        return readHeader(*static_cast<std::string*>(fileArg.ptr));
    return "";
}

// 各类别测试
// Per-format tests
// ᠲᠥᠷᠥᠯ ᠪᠦᠷᠢ ᠶᠢᠨ ᠰᠢᠯᠭᠠᠯ
bool testJpeg(const std::string& h){
    return h.size() > 10 && (h.compare(6, 4, "JFIF") == 0 ||
                             h.compare(6, 4, "Exif") == 0);
}

bool testPng(const std::string& h){
    return h.size() >= 8 && h.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0;
}

bool testGif(const std::string& h){
    return h.size() >= 6 && (h.compare(0, 6, "GIF87a") == 0 ||
                             h.compare(0, 6, "GIF89a") == 0);
}

bool testTiff(const std::string& h){
    return h.size() >= 2 && (h.compare(0, 2, "MM") == 0 ||
                             h.compare(0, 2, "II") == 0);
}

bool testRgb(const std::string& h){
    if (h.size() < 2) return false;
    return (h[0] == '\x01' && (h[1] == '\xda' || h[1] == '\x01' ||
                               h[1] == '\xc0' || h[1] == '\xc4'));
}

bool testPbm(const std::string& h){
    return startsWith(h, "P1") || startsWith(h, "P4");
}

bool testPgm(const std::string& h){
    return startsWith(h, "P2") || startsWith(h, "P5");
}

bool testPpm(const std::string& h){
    return startsWith(h, "P3") || startsWith(h, "P6");
}

bool testPnm(const std::string& h){
    return testPbm(h) || testPgm(h) || testPpm(h);
}

bool testRas(const std::string& h){
    return h.size() >= 4 && h.compare(0, 4, "\x59\xa6\x6a\x95") == 0;
}

bool testXbm(const std::string& h){
    return startsWith(h, "#define ");
}

bool testBmp(const std::string& h){
    return h.size() >= 2 && h.compare(0, 2, "BM") == 0;
}

bool testWebp(const std::string& h){
    return h.size() >= 12 && h.compare(0, 4, "RIFF") == 0 &&
           h.compare(8, 4, "WEBP") == 0;
}

bool testExr(const std::string& h){
    return h.size() >= 4 && h.compare(0, 4, "\x76\x2f\x31\x01") == 0;
}

// 识别
// Identify
// ᠲᠠᠨᠢᠬᠤ
std::string identify(const std::string& h){
    if (testJpeg(h)) return "jpeg";
    if (testPng(h)) return "png";
    if (testGif(h)) return "gif";
    if (testTiff(h)) return "tiff";
    if (testRgb(h)) return "rgb";
    if (testPbm(h)) return "pbm";
    if (testPgm(h)) return "pgm";
    if (testPpm(h)) return "ppm";
    if (testPnm(h)) return "pnm";
    if (testRas(h)) return "rast";
    if (testXbm(h)) return "xbm";
    if (testBmp(h)) return "bmp";
    if (testWebp(h)) return "webp";
    if (testExr(h)) return "exr";
    return "";
}

}

// imghdr.what
// imghdr.what
// imghdr.what
static value fnWhat(cffi& a){
    std::string type = identify(resolveHeader(a));
    if (type.empty()) return a.none();
    return a.mkstr(type);
}

// 各格式测试函数
// Format test functions
// ᠲᠥᠷᠥᠯ ᠪᠦᠷᠢ ᠶᠢᠨ ᠰᠢᠯᠭᠠᠯ ᠦᠨ ᠬᠡᠷᠡᠭᠰᠡᠯ
static value fnTestJpeg(cffi& a){ return a.mkbool(testJpeg(rawOf(a.arg(0)))); }
static value fnTestPng(cffi& a){ return a.mkbool(testPng(rawOf(a.arg(0)))); }
static value fnTestGif(cffi& a){ return a.mkbool(testGif(rawOf(a.arg(0)))); }
static value fnTestTiff(cffi& a){ return a.mkbool(testTiff(rawOf(a.arg(0)))); }
static value fnTestRgb(cffi& a){ return a.mkbool(testRgb(rawOf(a.arg(0)))); }
static value fnTestPbm(cffi& a){ return a.mkbool(testPbm(rawOf(a.arg(0)))); }
static value fnTestPgm(cffi& a){ return a.mkbool(testPgm(rawOf(a.arg(0)))); }
static value fnTestPpm(cffi& a){ return a.mkbool(testPpm(rawOf(a.arg(0)))); }
static value fnTestPnm(cffi& a){ return a.mkbool(testPnm(rawOf(a.arg(0)))); }
static value fnTestRas(cffi& a){ return a.mkbool(testRas(rawOf(a.arg(0)))); }
static value fnTestXbm(cffi& a){ return a.mkbool(testXbm(rawOf(a.arg(0)))); }
static value fnTestBmp(cffi& a){ return a.mkbool(testBmp(rawOf(a.arg(0)))); }
static value fnTestWebp(cffi& a){ return a.mkbool(testWebp(rawOf(a.arg(0)))); }
static value fnTestExr(cffi& a){ return a.mkbool(testExr(rawOf(a.arg(0)))); }

// cffi imghdr lib
// cffi imghdr lib
// cffi imghdr lib
ffilib cffi_imghdr_lib(){
    return cffi_lib(
        "imghdr",
        {
            {"what", (void*)fnWhat},
            {"test_jpeg", (void*)fnTestJpeg},
            {"test_png", (void*)fnTestPng},
            {"test_gif", (void*)fnTestGif},
            {"test_tiff", (void*)fnTestTiff},
            {"test_rgb", (void*)fnTestRgb},
            {"test_pbm", (void*)fnTestPbm},
            {"test_pgm", (void*)fnTestPgm},
            {"test_ppm", (void*)fnTestPpm},
            {"test_pnm", (void*)fnTestPnm},
            {"test_rast", (void*)fnTestRas},
            {"test_xbm", (void*)fnTestXbm},
            {"test_bmp", (void*)fnTestBmp},
            {"test_webp", (void*)fnTestWebp},
            {"test_exr", (void*)fnTestExr}
        });
}

}
