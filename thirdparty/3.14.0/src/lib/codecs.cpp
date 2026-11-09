#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace minipy {




// 构造 bytes 值
// make bytes value
// bytes ᠪᠠᠶᠢᠳᠠᠯ ᠪᠣᠯᠭᠠᠬᠤ
static value mkBytes(cffi& a, const std::string& raw){
    value v = a.mkstr(raw);
    v.type = "bytes";
    return v;
}

// 原始字节内容
// raw byte content
// ᠬᠠᠭᠤᠴᠢᠨ ᠪᠠᠶᠢᠲ᠋ ᠠᠭᠤᠯᠭ᠎ᠠ
static std::string rawOf(const value& v){
    if ((v.type == "bytes" || v.type == "bytearray" || v.type == "str") && v.ptr)
        return *static_cast<std::string*>(v.ptr);
    return "";
}

// 追加 UTF-8 码点
// append UTF-8 codepoint
// UTF-8 ᠺᠣᠳ᠋ ᠨᠠᠮᠠᠭ᠌᠎ᠠ
static void appendCodepoint(std::string& out, unsigned int cp){
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

// 读取 UTF-8 码点
// read UTF-8 codepoint
// UTF-8 ᠺᠣᠳ᠋ ᠤᠨᠰᠢᠬᠤ
static bool readCodepoint(const std::string& text, size_t& pos, unsigned int& cp){
    if (pos >= text.size()) return false;
    unsigned char first = static_cast<unsigned char>(text[pos]);
    if (first < 0x80){
        cp = first;
        ++pos;
        return true;
    }
    int extra = 0;
    unsigned int value = 0;
    if ((first & 0xE0) == 0xC0){ extra = 1; value = first & 0x1F; }
    else if ((first & 0xF0) == 0xE0){ extra = 2; value = first & 0x0F; }
    else if ((first & 0xF8) == 0xF0){ extra = 3; value = first & 0x07; }
    else return false;
    if (pos + static_cast<size_t>(extra) >= text.size()) return false;
    for (int i = 1; i <= extra; ++i){
        unsigned char c = static_cast<unsigned char>(text[pos + i]);
        if ((c & 0xC0) != 0x80) return false;
        value = (value << 6) | (c & 0x3F);
    }
    cp = value;
    pos += static_cast<size_t>(extra + 1);
    return true;
}

// 规范化编码名
// normalize encoding name
// ᠬᠡᠯᠪᠡᠷᠢ ᠨᠡᠷᠡ ᠵᠢᠭᠳᠡᠭᠦᠯᠬᠦ
static std::string normalizeEncoding(const std::string& encoding){
    std::string out;
    for (char c : encoding){
        unsigned char u = static_cast<unsigned char>(c);
        if (u == '_' || u == ' ' || u == '-') continue;
        out.push_back(static_cast<char>(std::tolower(u)));
    }
    if (out == "utf8" || out == "u8") return "utf-8";
    if (out == "latin1" || out == "iso88591" || out == "cp1252") return "latin-1";
    if (out == "usascii" || out == "646") return "ascii";
    if (out == "rot13" || out == "rot_13") return "rot13";
    if (out == "hexcodec" || out == "hex") return "hex";
    if (out == "base64codec" || out == "base64") return "base64";
    return out;
}

// 编码为字节
// encode to bytes
// ᠪᠠᠶᠢᠲ᠋ ᠪᠣᠯᠭᠠᠨ ᠺᠣᠳ᠋ᠯᠦᠯ
static bool codecEncode(const std::string& text, const std::string& encoding,
                        std::string& out, std::string& error){
    std::string enc = normalizeEncoding(encoding);
    if (enc == "utf-8"){
        out = text;
        return true;
    }
    if (enc == "ascii"){
        for (char c : text){
            if (static_cast<unsigned char>(c) >= 128){
                error = "'ascii' codec can't encode character: ordinal not in range(128)";
                return false;
            }
        }
        out = text;
        return true;
    }
    if (enc == "latin-1"){
        size_t pos = 0;
        while (pos < text.size()){
            unsigned int cp = 0;
            if (!readCodepoint(text, pos, cp)){
                error = "'latin-1' codec can't encode: invalid UTF-8 input";
                return false;
            }
            if (cp > 255){
                error = "'latin-1' codec can't encode character";
                return false;
            }
            out.push_back(static_cast<char>(cp));
        }
        return true;
    }
    if (enc == "rot13"){
        out = text;
        for (auto& c : out){
            unsigned char u = static_cast<unsigned char>(c);
            if (u >= 'a' && u <= 'z') c = static_cast<char>('a' + (u - 'a' + 13) % 26);
            else if (u >= 'A' && u <= 'Z') c = static_cast<char>('A' + (u - 'A' + 13) % 26);
        }
        return true;
    }
    if (enc == "hex"){
        char buf[4];
        for (unsigned char c : text){
            std::snprintf(buf, sizeof(buf), "%02x", c);
            out += buf;
        }
        return true;
    }
    if (enc == "base64"){
        const char* alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t i = 0;
        while (i + 2 < text.size()){
            unsigned int n = (static_cast<unsigned char>(text[i]) << 16) |
                             (static_cast<unsigned char>(text[i + 1]) << 8) |
                              static_cast<unsigned char>(text[i + 2]);
            out.push_back(alphabet[(n >> 18) & 0x3F]);
            out.push_back(alphabet[(n >> 12) & 0x3F]);
            out.push_back(alphabet[(n >> 6) & 0x3F]);
            out.push_back(alphabet[n & 0x3F]);
            i += 3;
        }
        if (i < text.size()){
            unsigned int n = static_cast<unsigned char>(text[i]) << 16;
            bool two = (i + 1 < text.size());
            if (two) n |= static_cast<unsigned char>(text[i + 1]) << 8;
            out.push_back(alphabet[(n >> 18) & 0x3F]);
            out.push_back(alphabet[(n >> 12) & 0x3F]);
            out.push_back(two ? alphabet[(n >> 6) & 0x3F] : '=');
            out.push_back('=');
        }
        out.push_back('\n');
        return true;
    }
    error = "unknown encoding: " + encoding;
    return false;
}

// 字节解码
// decode bytes
// ᠪᠠᠶᠢᠲ᠋ ᠲᠠᠢᠯᠠᠬᠤ
static bool codecDecode(cffi& a, const std::string& raw, const std::string& encoding,
                        std::string& out, bool& isBytes, std::string& error){
    std::string enc = normalizeEncoding(encoding);
    isBytes = false;
    if (enc == "utf-8"){
        out = raw;
        return true;
    }
    if (enc == "ascii"){
        for (char c : raw){
            if (static_cast<unsigned char>(c) >= 128){
                error = "'ascii' codec can't decode: ordinal not in range(128)";
                return false;
            }
        }
        out = raw;
        return true;
    }
    if (enc == "latin-1"){
        for (unsigned char c : raw) appendCodepoint(out, c);
        return true;
    }
    if (enc == "rot13"){
        out = raw;
        for (auto& c : out){
            unsigned char u = static_cast<unsigned char>(c);
            if (u >= 'a' && u <= 'z') c = static_cast<char>('a' + (u - 'a' + 13) % 26);
            else if (u >= 'A' && u <= 'Z') c = static_cast<char>('A' + (u - 'A' + 13) % 26);
        }
        return true;
    }
    if (enc == "hex"){
        if (raw.size() % 2 != 0){
            error = "Odd-length string";
            return false;
        }
        auto hexVal = [](char c) -> int {
            unsigned char u = static_cast<unsigned char>(c);
            if (u >= '0' && u <= '9') return u - '0';
            if (u >= 'a' && u <= 'f') return u - 'a' + 10;
            if (u >= 'A' && u <= 'F') return u - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < raw.size(); i += 2){
            int hi = hexVal(raw[i]);
            int lo = hexVal(raw[i + 1]);
            if (hi < 0 || lo < 0){
                error = "Non-hexadecimal digit found";
                return false;
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
        }
        isBytes = true;
        return true;
    }
    if (enc == "base64"){
        const std::string alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string data;
        for (char c : raw) if (!std::isspace(static_cast<unsigned char>(c))) data.push_back(c);
        size_t i = 0;
        while (i < data.size()){
            unsigned int n = 0;
            int count = 0;
            for (; count < 4 && i < data.size() && data[i] != '='; ++count, ++i){
                size_t idx = alphabet.find(data[i]);
                if (idx == std::string::npos){
                    error = "invalid base64 data";
                    return false;
                }
                n |= static_cast<unsigned int>(idx) << (18 - 6 * count);
            }
            if (count >= 2) out.push_back(static_cast<char>((n >> 16) & 0xFF));
            if (count >= 3) out.push_back(static_cast<char>((n >> 8) & 0xFF));
            if (count >= 4) out.push_back(static_cast<char>(n & 0xFF));
            while (i < data.size() && data[i] == '=') ++i;
        }
        isBytes = true;
        return true;
    }
    error = "unknown encoding: " + encoding;
    return false;
}

// 解析编码参数
// parse encoding argument
// ᠬᠡᠯᠪᠡᠷᠢ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠰᠢᠨᠠᠯᠠᠬᠤ
static std::string encodingArg(cffi& a, size_t index, const std::string& fallback){
    if (a.has_kwarg("encoding")) return a.kwarg_str("encoding", fallback);
    if (a.argc() > index && a.arg(index).type == "str") return a.arg_str(index);
    return fallback;
}

// 类型名
// type name
// ᠲᠥᠷᠥᠯ ᠦᠨ ᠨᠡᠷᠡ
static std::string typeNameOf(const value& v){
    if (v.type == "none") return "NoneType";
    return v.type;
}

// 是否字节串
// whether value is bytes-like
// ᠪᠠᠶᠢᠲ᠋ ᠮᠦ
static bool isBytesLike(const value& v){
    return (v.type == "bytes" || v.type == "bytearray") && v.ptr;
}

// 编码输入类型检查
// check encoder input type
// ᠺᠣᠳ᠋ᠯᠦᠯ ᠦᠨ ᠣᠷᠣᠯᠲᠠ ᠰᠢᠯᠠᠭᠠᠬᠤ
static bool checkEncodeInput(cffi& a, const value& obj, const std::string& enc){
    std::string tname = typeNameOf(obj);
    if (enc == "utf-8" || enc == "ascii" || enc == "latin-1"){
        if (obj.type == "str") return true;
        std::string codecName =
            enc == "utf-8" ? "utf_8_encode()" : (enc == "ascii" ? "ascii_encode()" : "latin_1_encode()");
        a.vm->raiseError("TypeError", codecName + " argument 1 must be str, not " + tname);
        return false;
    }
    if (enc == "rot13"){
        if (obj.type == "str") return true;
        a.vm->raiseError("TypeError",
                         "descriptor 'translate' for 'str' objects doesn't apply to a '" +
                             tname + "' object");
        return false;
    }
    if (enc == "hex" || enc == "base64"){
        if (isBytesLike(obj)) return true;
        a.vm->raiseError("TypeError",
                         "a bytes-like object is required, not '" + tname + "'");
        return false;
    }
    return true;
}

// 解码输入类型检查
// check decoder input type
// ᠲᠠᠢᠯᠠᠯᠲᠠ ᠶᠢᠨ ᠣᠷᠣᠯᠲᠠ ᠰᠢᠯᠠᠭᠠᠬᠤ
static bool checkDecodeInput(cffi& a, const value& obj, const std::string& enc){
    std::string tname = typeNameOf(obj);
    if (enc == "utf-8" || enc == "ascii" || enc == "latin-1"){
        if (isBytesLike(obj)) return true;
        a.vm->raiseError("TypeError",
                         "a bytes-like object is required, not '" + tname + "'");
        return false;
    }
    if (enc == "rot13"){
        if (obj.type == "str") return true;
        a.vm->raiseError("TypeError",
                         "descriptor 'translate' for 'str' objects doesn't apply to a '" +
                             tname + "' object");
        return false;
    }
    if (enc == "hex") return true;
    if (enc == "base64"){
        if (isBytesLike(obj)) return true;
        a.vm->raiseError("TypeError", "expected bytes-like object, not " + tname);
        return false;
    }
    return true;
}

// fn Encode
// fn encode
// fn encode
static value fnEncode(cffi& a){
    value obj = a.arg(0);
    std::string encoding = encodingArg(a, 1, "utf-8");
    std::string enc = normalizeEncoding(encoding);
    if (!checkEncodeInput(a, obj, enc)) return a.none();
    std::string text = rawOf(obj);
    std::string out, error;
    if (!codecEncode(text, encoding, out, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    if (enc == "rot13") return a.mkstr(out);
    return mkBytes(a, out);
}

// fn Decode
// fn decode
// fn decode
static value fnDecode(cffi& a){
    value obj = a.arg(0);
    std::string encoding = encodingArg(a, 1, "utf-8");
    std::string enc = normalizeEncoding(encoding);
    if (!checkDecodeInput(a, obj, enc)) return a.none();
    std::string raw = rawOf(obj);
    std::string out, error;
    bool isBytes = false;
    if (!codecDecode(a, raw, encoding, out, isBytes, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    if (isBytes) return mkBytes(a, out);
    return a.mkstr(out);
}

// fn Iter Encode
// fn iter encode
// fn iter encode
static value fnIterEncode(cffi& a){
    value encoded = fnEncode(a);
    if (a.vm->hasError()) return a.none();
    std::vector<value> items;
    items.push_back(encoded);
    value list = a.mklist(items);
    return list;
}

// fn Iter Decode
// fn iter decode
// fn iter decode
static value fnIterDecode(cffi& a){
    value decoded = fnDecode(a);
    if (a.vm->hasError()) return a.none();
    std::vector<value> items;
    items.push_back(decoded);
    return a.mklist(items);
}

// fn Lookup
// fn lookup
// fn lookup
static value fnLookup(cffi& a){
    std::string encoding = a.argc() >= 1 ? rawOf(a.arg(0)) : "utf-8";
    std::string enc = normalizeEncoding(encoding);
    const char* known[] = {"utf-8", "ascii", "latin-1", "rot13", "hex", "base64"};
    bool found = false;
    for (const char* name : known) if (enc == name) found = true;
    if (!found){
        a.vm->raiseError("LookupError", "unknown encoding: " + encoding);
        return a.none();
    }
    auto it = a.library->classes.find("CodecInfo");
    if (it == a.library->classes.end()) return a.none();
    value obj = a.vm->makeNativeObject(a.library, &it->second, new std::string(enc));
    std::string display = enc;
    if (enc == "latin-1") display = "iso8859-1";
    else if (enc == "rot13") display = "rot-13";
    a.setattr(obj, "name", a.mkstr(display));
    return obj;
}

// CodecInfo 构造器
// CodecInfo constructor
// CodecInfo ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnCodecInfoConstructor(cffi& a){
    value obj = a.mkobject(new std::string(a.argc() >= 1 ? rawOf(a.arg(0)) : "utf-8"));
    if (a.argc() >= 1) a.setattr(obj, "name", a.mkstr(rawOf(a.arg(0))));
    return obj;
}

// CodecInfo 状态名
// codec info name
// codec ᠨᠡᠷᠡ
static std::string codecObjectName(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        if (inst->handle) return *static_cast<std::string*>(inst->handle);
    }
    return "utf-8";
}

// Codec.encode
// Codec.encode
// Codec.encode
static value fnCodecObjectEncode(cffi& a){
    std::string encoding = codecObjectName(a);
    value obj = a.arg(0);
    if (!checkEncodeInput(a, obj, encoding)) return a.none();
    std::string text = rawOf(obj);
    std::string out, error;
    if (!codecEncode(text, encoding, out, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    if (normalizeEncoding(encoding) == "rot13") return a.mkstr(out);
    return mkBytes(a, out);
}

// Codec.decode
// Codec.decode
// Codec.decode
static value fnCodecObjectDecode(cffi& a){
    std::string encoding = codecObjectName(a);
    value obj = a.arg(0);
    if (!checkDecodeInput(a, obj, encoding)) return a.none();
    std::string raw = rawOf(obj);
    std::string out, error;
    bool isBytes = false;
    if (!codecDecode(a, raw, encoding, out, isBytes, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    if (isBytes) return mkBytes(a, out);
    return a.mkstr(out);
}

// fn Register
// fn register
// fn register
static value fnRegister(cffi& a){
    (void)a;
    return a.none();
}

// fn Lookup Error
// fn lookup error
// fn lookup error
static value fnLookupError(cffi& a){
    std::string name = a.argc() >= 1 ? rawOf(a.arg(0)) : "";
    a.vm->raiseError("LookupError", "unknown error handler name '" + name + "'");
    return a.none();
}

// data BOM Utf8
// data bom utf8
// data BOM Utf8
static value dataBomUtf8(cffi& a){ return mkBytes(a, "\xef\xbb\xbf"); }

// data BOM Utf16 Le
// data bom utf16 le
// data BOM Utf16 Le
static value dataBomUtf16Le(cffi& a){ return mkBytes(a, "\xff\xfe"); }

// data BOM Utf16 Be
// data bom utf16 be
// data BOM Utf16 Be
static value dataBomUtf16Be(cffi& a){ return mkBytes(a, "\xfe\xff"); }

// data BOM Utf32 Le
// data bom utf32 le
// data BOM Utf32 Le
static value dataBomUtf32Le(cffi& a){ return mkBytes(a, "\xff\xfe\x00\x00"); }

// data BOM Utf32 Be
// data bom utf32 be
// data BOM Utf32 Be
static value dataBomUtf32Be(cffi& a){ return mkBytes(a, "\x00\x00\xfe\xff"); }

// cffi codecs lib
// cffi codecs library
// cffi codecs ᠨᠣᠮ
ffilib cffi_codecs_lib(){
    ffi_class errorClass;
    errorClass.name = "Error";

    ffi_class infoClass;
    infoClass.name = "CodecInfo";
    infoClass.constructor = (void*)fnCodecInfoConstructor;
    infoClass.methods = {
        {"encode", (void*)fnCodecObjectEncode},
        {"decode", (void*)fnCodecObjectDecode}
    };

    ffi_class codecClass;
    codecClass.name = "Codec";
    codecClass.constructor = (void*)fnCodecInfoConstructor;
    codecClass.methods = {
        {"encode", (void*)fnCodecObjectEncode},
        {"decode", (void*)fnCodecObjectDecode}
    };

    return cffi_lib(
        "codecs",
        {
            {"encode", (void*)fnEncode},
            {"decode", (void*)fnDecode},
            {"lookup", (void*)fnLookup},
            {"iterencode", (void*)fnIterEncode},
            {"iterdecode", (void*)fnIterDecode},
            {"register", (void*)fnRegister},
            {"lookup_error", (void*)fnLookupError}
        },
        {
            {"BOM_UTF8", (void*)dataBomUtf8},
            {"BOM_UTF16_LE", (void*)dataBomUtf16Le},
            {"BOM_UTF16_BE", (void*)dataBomUtf16Be},
            {"BOM_UTF32_LE", (void*)dataBomUtf32Le},
            {"BOM_UTF32_BE", (void*)dataBomUtf32Be}
        },
        {
            {"Error", errorClass},
            {"CodecInfo", infoClass},
            {"Codec", codecClass}
        });
}


}
