#include "../cffi.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace minipy {




// 结构字段
// struct field
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠲᠠᠯᠠ
struct StructField{
    char code = 0;
    long long repeat = 1;
    size_t elemSize = 1;
    size_t offset = 0;
    size_t align = 1;
};

// 结构布局
// struct layout
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠵᠢᠷᠤᠭ
struct StructSpec{
    bool bigEndian = false;
    bool native = true;
    std::vector<StructField> fields;
    size_t size = 0;
    std::string format;
};

// 结构对象状态
// struct object state
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠪᠠᠶᠢᠳᠠᠯ
struct StructState{
    StructSpec spec;
};

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

// 标准数值尺寸
// standard numeric size
// ᠰᠲᠠᠨᠳᠠᠷᠲ ᠲᠣᠭ᠎ᠠ ᠬᠡᠮᠵᠢᠶ᠎ᠡ
static size_t standardSize(char code){
    switch (code){
        case 'b': case 'B': case '?': case 'c': return 1;
        case 'h': case 'H': case 'e': return 2;
        case 'i': case 'I': case 'l': case 'L': case 'f': return 4;
        case 'q': case 'Q': case 'd': return 8;
        default: return 0;
    }
}

// 原生数值尺寸
// native numeric size
// ᠨᠠᠲᠢᠪ ᠲᠣᠭ᠎ᠠ ᠬᠡᠮᠵᠢᠶ᠎ᠡ
static size_t nativeSize(char code){
    switch (code){
        case 'b': case 'B': case '?': case 'c': return 1;
        case 'h': case 'H': case 'e': return 2;
        case 'i': case 'I': return sizeof(int);
        case 'l': case 'L': return sizeof(long);
        case 'q': case 'Q': return sizeof(long long);
        case 'f': return sizeof(float);
        case 'd': return sizeof(double);
        case 'n': case 'N': case 'P': return sizeof(void*);
        default: return 0;
    }
}

// 数值对齐
// numeric alignment
// ᠲᠣᠭ᠎ᠠ ᠵᠢᠷᠤᠭᠯᠠᠯ
static size_t nativeAlign(char code){
    switch (code){
        case 'b': case 'B': case '?': case 'c': return 1;
        case 'h': case 'H': case 'e': return alignof(short);
        case 'i': case 'I': return alignof(int);
        case 'l': case 'L': return alignof(long);
        case 'q': case 'Q': return alignof(long long);
        case 'f': return alignof(float);
        case 'd': return alignof(double);
        case 'n': case 'N': case 'P': return alignof(void*);
        default: return 1;
    }
}

// 解析结构格式
// parse struct format
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠬᠡᠯᠪᠡᠷᠢ ᠰᠢᠨᠠᠯᠠᠬᠤ
static bool parseStructFormat(cffi& a, const std::string& fmt, bool validate,
                              StructSpec& spec, std::string& error){
    spec = StructSpec();
    spec.format = fmt;
    bool byteOrderSet = false;
    size_t pos = 0;
    size_t offset = 0;
    size_t maxAlign = 1;

    while (pos < fmt.size()){
        char c = fmt[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            ++pos;
            continue;
        }
        if (c == '@' || c == '=' || c == '<' || c == '>' || c == '!'){
            if (byteOrderSet && validate){
                error = "repeat count given without format specifier";
                return false;
            }
            byteOrderSet = true;
            spec.native = (c == '@');
            spec.bigEndian = (c == '>') || (c == '!') ||
                             (spec.native && std::endian::native == std::endian::big);
            ++pos;
            continue;
        }
        long long count = 0;
        bool hasCount = false;
        while (pos < fmt.size() && fmt[pos] >= '0' && fmt[pos] <= '9'){
            hasCount = true;
            count = count * 10 + (fmt[pos] - '0');
            ++pos;
        }
        if (pos >= fmt.size()){
            if (validate){
                error = "repeat count given without format specifier";
                return false;
            }
            break;
        }
        char code = fmt[pos++];
        size_t elemSize = spec.native ? nativeSize(code) : standardSize(code);
        size_t align = 1;
        if (code == 'x'){
            elemSize = 1;
            align = 1;
            if (!hasCount) count = 1;
            StructField field;
            field.code = code;
            field.repeat = count;
            field.elemSize = 1;
            field.align = 1;
            field.offset = offset;
            offset += static_cast<size_t>(count);
            spec.fields.push_back(field);
            continue;
        }
        if (code == 's'){
            elemSize = hasCount ? static_cast<size_t>(count) : 1;
            align = 1;
            if (spec.native) offset = (offset + 1 - 1);
            StructField field;
            field.code = code;
            field.repeat = 1;
            field.elemSize = elemSize;
            field.align = 1;
            field.offset = offset;
            offset += elemSize;
            spec.fields.push_back(field);
            continue;
        }
        if (code == 'c'){
            elemSize = 1;
            align = 1;
            if (!hasCount) count = 1;
            StructField field;
            field.code = code;
            field.repeat = count;
            field.elemSize = 1;
            field.align = 1;
            field.offset = offset;
            offset += static_cast<size_t>(count);
            spec.fields.push_back(field);
            continue;
        }
        if (elemSize == 0){
            error = "bad char in struct format";
            return false;
        }
        if (spec.native && (code == 'P' || code == 'n' || code == 'N')){
            align = nativeAlign(code);
        }
        if (!spec.native && (code == 'P' || code == 'n' || code == 'N')){
            error = "bad char in struct format";
            return false;
        }
        if (!hasCount) count = 1;
        if (count < 0) count = 0;
        if (spec.native){
            size_t aligned = (offset + align - 1) / align * align;
            offset = aligned;
            if (align > maxAlign) maxAlign = align;
        }
        StructField field;
        field.code = code;
        field.repeat = count;
        field.elemSize = elemSize;
        field.align = spec.native ? align : 1;
        field.offset = offset;
        offset += elemSize * static_cast<size_t>(count);
        spec.fields.push_back(field);
    }
    if (spec.native) offset = (offset + maxAlign - 1) / maxAlign * maxAlign;
    spec.size = offset;
    return true;
}

// 写入整数
// write integer
// ᠲᠣᠭ᠎ᠠ ᠪᠢᠴᠢᠬᠦ
static void writeInt(std::string& out, size_t offset, size_t size, long long value,
                     bool bigEndian){
    union {
        unsigned long long u;
        long long s;
    } bits;
    bits.s = value;
    for (size_t i = 0; i < size; ++i){
        size_t index = bigEndian ? (size - 1 - i) : i;
        unsigned char byte = static_cast<unsigned char>((bits.u >> (index * 8)) & 0xFF);
        out[offset + i] = static_cast<char>(byte);
    }
}

// 读取整数
// read integer
// ᠲᠣᠭ᠎ᠠ ᠤᠨᠰᠢᠬᠤ
static long long readInt(const std::string& raw, size_t offset, size_t size,
                         bool bigEndian, bool isSigned){
    unsigned long long u = 0;
    for (size_t i = 0; i < size; ++i){
        size_t index = bigEndian ? (size - 1 - i) : i;
        u |= static_cast<unsigned long long>(
                 static_cast<unsigned char>(raw[offset + i])) << (index * 8);
    }
    if (isSigned && size < 8){
        unsigned long long signBit = 1ULL << (size * 8 - 1);
        if (u & signBit)
            u |= ~((signBit << 1) - 1);
    }
    return static_cast<long long>(u);
}

// 浮点转半精度
// float to half
// ᠨᠠᠮᠠᠭ᠌᠎ᠠ ᠬᠠᠭᠠᠰ ᠪᠣᠯᠭᠠᠬᠤ
static uint16_t floatToHalf(float value){
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFF;
    if (exponent >= 31){
        return static_cast<uint16_t>(sign | 0x7C00);
    }
    if (exponent <= 0){
        if (exponent < -10) return static_cast<uint16_t>(sign);
        mantissa = (mantissa | 0x800000) >> (1 - exponent);
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) |
                                 (mantissa >> 13));
}

// 半精度转浮点
// half to float
// ᠬᠠᠭᠠᠰ ᠨᠠᠮᠠᠭ᠌᠎ᠠ ᠪᠣᠯᠭᠠᠬᠤ
static float halfToFloat(uint16_t value){
    uint32_t sign = static_cast<uint32_t>(value & 0x8000) << 16;
    uint32_t exponent = (value >> 10) & 0x1F;
    uint32_t mantissa = value & 0x3FF;
    uint32_t bits;
    if (exponent == 0){
        if (mantissa == 0){
            bits = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x400) == 0){
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x3FF;
            bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31){
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// 打包结构
// pack struct
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠪᠣᠳᠤᠬᠤ
static bool packStruct(cffi& a, const StructSpec& spec, const std::vector<value>& args,
                       size_t argStart, std::string& out, std::string& error){
    out.assign(spec.size, '\0');
    size_t argIndex = argStart;
    for (auto& field : spec.fields){
        if (field.code == 'x'){
            continue;
        }
        if (field.code == 's'){
            if (argIndex >= args.size()){
                error = "pack requires more arguments";
                return false;
            }
            std::string text = rawOf(args[argIndex++]);
            size_t copyLen = std::min(text.size(), field.elemSize);
            out.replace(field.offset, copyLen, text, 0, copyLen);
            continue;
        }
        for (long long r = 0; r < field.repeat; ++r){
            if (argIndex >= args.size()){
                error = "pack requires more arguments";
                return false;
            }
            size_t offset = field.offset + static_cast<size_t>(r) * field.elemSize;
            const value& item = args[argIndex++];
            if (field.code == 'c'){
                std::string text = rawOf(item);
                if (text.size() != 1){
                    error = "char format requires a bytes object of length 1";
                    return false;
                }
                out[offset] = text[0];
                continue;
            }
            if (field.code == '?'){
                writeInt(out, offset, 1, a.vm->truthy(item) ? 1 : 0, spec.bigEndian);
                continue;
            }
            if (field.code == 'e'){
                uint16_t half = floatToHalf(static_cast<float>(a.vm->toFloat(item)));
                unsigned char bytes[2];
                if (spec.bigEndian){
                    bytes[0] = static_cast<unsigned char>(half >> 8);
                    bytes[1] = static_cast<unsigned char>(half & 0xFF);
                } else {
                    bytes[0] = static_cast<unsigned char>(half & 0xFF);
                    bytes[1] = static_cast<unsigned char>(half >> 8);
                }
                out[offset] = static_cast<char>(bytes[0]);
                out[offset + 1] = static_cast<char>(bytes[1]);
                continue;
            }
            if (field.code == 'f'){
                float f = static_cast<float>(a.vm->toFloat(item));
                unsigned char bytes[4];
                std::memcpy(bytes, &f, 4);
                for (size_t k = 0; k < 4; ++k)
                    out[offset + (spec.bigEndian ? 3 - k : k)] = static_cast<char>(bytes[k]);
                continue;
            }
            if (field.code == 'd'){
                double d = a.vm->toFloat(item);
                unsigned char bytes[8];
                std::memcpy(bytes, &d, 8);
                for (size_t k = 0; k < 8; ++k)
                    out[offset + (spec.bigEndian ? 7 - k : k)] = static_cast<char>(bytes[k]);
                continue;
            }
            bool isSigned = (field.code == 'b' || field.code == 'h' ||
                             field.code == 'i' || field.code == 'l' ||
                             field.code == 'q' || field.code == 'n');
            long long value = a.vm->toInt(item);
            writeInt(out, offset, field.elemSize, value, spec.bigEndian);
            (void)isSigned;
        }
    }
    return true;
}

// 解包结构
// unpack struct
// ᠪᠣᠳᠢ ᠨᠣᠮ ᠨᠡᠶᠢᠯᠡᠬᠦ
static bool unpackStruct(cffi& a, const StructSpec& spec, const std::string& raw,
                         size_t baseOffset, std::vector<value>& results,
                         std::string& error){
    for (auto& field : spec.fields){
        if (field.code == 'x') continue;
        if (field.code == 's'){
            size_t offset = baseOffset + field.offset;
            results.push_back(mkBytes(a, raw.substr(offset, field.elemSize)));
            continue;
        }
        for (long long r = 0; r < field.repeat; ++r){
            size_t offset = baseOffset + field.offset + static_cast<size_t>(r) * field.elemSize;
            if (field.code == 'c'){
                results.push_back(mkBytes(a, raw.substr(offset, 1)));
                continue;
            }
            if (field.code == '?'){
                results.push_back(a.mkbool(readInt(raw, offset, 1, spec.bigEndian, false) != 0));
                continue;
            }
            if (field.code == 'e'){
                uint16_t half;
                if (spec.bigEndian){
                    half = static_cast<uint16_t>(
                        (static_cast<unsigned char>(raw[offset]) << 8) |
                         static_cast<unsigned char>(raw[offset + 1]));
                } else {
                    half = static_cast<uint16_t>(
                         static_cast<unsigned char>(raw[offset]) |
                        (static_cast<unsigned char>(raw[offset + 1]) << 8));
                }
                results.push_back(a.mkfloat(static_cast<double>(halfToFloat(half))));
                continue;
            }
            if (field.code == 'f'){
                unsigned char bytes[4];
                for (size_t k = 0; k < 4; ++k)
                    bytes[k] = static_cast<unsigned char>(raw[offset + (spec.bigEndian ? 3 - k : k)]);
                float f;
                std::memcpy(&f, bytes, 4);
                results.push_back(a.mkfloat(static_cast<double>(f)));
                continue;
            }
            if (field.code == 'd'){
                unsigned char bytes[8];
                for (size_t k = 0; k < 8; ++k)
                    bytes[k] = static_cast<unsigned char>(raw[offset + (spec.bigEndian ? 7 - k : k)]);
                double d;
                std::memcpy(&d, bytes, 8);
                results.push_back(a.mkfloat(d));
                continue;
            }
            bool isSigned = (field.code == 'b' || field.code == 'h' ||
                             field.code == 'i' || field.code == 'l' ||
                             field.code == 'q' || field.code == 'n');
            results.push_back(a.mkint(readInt(raw, offset, field.elemSize,
                                               spec.bigEndian, isSigned)));
        }
    }
    return true;
}

// 取值检查长度
// check raw length
// ᠤᠷᠲᠤ ᠰᠢᠯᠠᠭᠠᠬᠤ
static bool checkRawLength(cffi& a, const std::string& raw, size_t need,
                           size_t base, std::string& error){
    if (base > raw.size() || need > raw.size() - base){
        error = "unpack requires a buffer of " + std::to_string(need) + " bytes";
        return false;
    }
    return true;
}

// fn Calc Size
// fn calc size
// fn calc size
static value fnCalcSize(cffi& a){
    StructSpec spec;
    std::string error;
    if (!parseStructFormat(a, rawOf(a.arg(0)), true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    return a.mkint(static_cast<long long>(spec.size));
}

// fn Pack
// fn pack
// fn pack
static value fnPack(cffi& a){
    StructSpec spec;
    std::string error;
    if (!parseStructFormat(a, rawOf(a.arg(0)), true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::vector<value> rest;
    for (size_t i = 1; i < a.argc(); ++i) rest.push_back(a.arg(i));
    std::string out;
    if (!packStruct(a, spec, rest, 0, out, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    return mkBytes(a, out);
}

// fn Unpack
// fn unpack
// fn unpack
static value fnUnpack(cffi& a){
    StructSpec spec;
    std::string error;
    if (!parseStructFormat(a, rawOf(a.arg(0)), true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::string raw = rawOf(a.arg(1));
    if (!checkRawLength(a, raw, spec.size, 0, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::vector<value> results;
    unpackStruct(a, spec, raw, 0, results, error);
    return a.vm->makeTuple(results);
}

// fn Pack Into
// fn pack into
// fn pack into
static value fnPackInto(cffi& a){
    StructSpec spec;
    std::string error;
    if (!parseStructFormat(a, rawOf(a.arg(0)), true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    value buffer = a.arg(1);
    long long offset = a.arg_int(2);
    if (buffer.type != "bytearray" && buffer.type != "list"){
        a.vm->raiseError("error", "pack_into requires a writable buffer");
        return a.none();
    }
    std::vector<value> rest;
    for (size_t i = 3; i < a.argc(); ++i) rest.push_back(a.arg(i));
    std::string packed;
    if (!packStruct(a, spec, rest, 0, packed, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    if (buffer.type == "bytearray"){
        auto* raw = static_cast<std::string*>(buffer.ptr);
        if (offset < 0) offset += static_cast<long long>(raw->size());
        if (offset < 0 || static_cast<size_t>(offset) + packed.size() > raw->size()){
            a.vm->raiseError("error", "pack_into requires a buffer of at least " +
                                      std::to_string(offset + packed.size()) + " bytes");
            return a.none();
        }
        raw->replace(static_cast<size_t>(offset), packed.size(), packed);
    } else {
        auto* items = static_cast<std::vector<value>*>(buffer.ptr);
        if (offset < 0) offset += static_cast<long long>(items->size());
        for (size_t i = 0; i < packed.size(); ++i){
            size_t index = static_cast<size_t>(offset) + i;
            if (index >= items->size()) break;
            (*items)[index] = a.mkint(static_cast<long long>(
                static_cast<unsigned char>(packed[i])));
        }
    }
    return a.none();
}

// fn Unpack From
// fn unpack from
// fn unpack from
static value fnUnpackFrom(cffi& a){
    StructSpec spec;
    std::string error;
    if (!parseStructFormat(a, rawOf(a.arg(0)), true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::string raw = rawOf(a.arg(1));
    long long offset = a.argc() >= 3 ? a.arg_int(2) : 0;
    if (offset < 0) offset += static_cast<long long>(raw.size());
    if (offset < 0){
        a.vm->raiseError("error", "offset must be non-negative");
        return a.none();
    }
    if (!checkRawLength(a, raw, spec.size, static_cast<size_t>(offset), error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::vector<value> results;
    unpackStruct(a, spec, raw, static_cast<size_t>(offset), results, error);
    return a.vm->makeTuple(results);
}

// Struct 构造器
// Struct constructor
// Struct ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnStructConstructor(cffi& a){
    StructSpec spec;
    std::string error;
    std::string format = rawOf(a.arg(0));
    if (!parseStructFormat(a, format, true, spec, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    value obj = a.mkobject(new StructState{spec});
    a.setattr(obj, "format", a.mkstr(format));
    a.setattr(obj, "size", a.mkint(static_cast<long long>(spec.size)));
    return obj;
}

// Struct 状态
// Struct state
// Struct ᠪᠠᠶᠢᠳᠠᠯ
static StructState* structState(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        if (inst->handle) return static_cast<StructState*>(inst->handle);
    }
    return nullptr;
}

// Struct.pack
// Struct.pack
// Struct.pack
static value fnStructPack(cffi& a){
    StructState* state = structState(a);
    if (!state) return a.none();
    std::vector<value> args;
    for (size_t i = 0; i < a.argc(); ++i) args.push_back(a.arg(i));
    std::string out, error;
    if (!packStruct(a, state->spec, args, 0, out, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    return mkBytes(a, out);
}

// Struct.unpack
// Struct.unpack
// Struct.unpack
static value fnStructUnpack(cffi& a){
    StructState* state = structState(a);
    if (!state) return a.none();
    std::string raw = rawOf(a.arg(0));
    std::string error;
    if (!checkRawLength(a, raw, state->spec.size, 0, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    std::vector<value> results;
    unpackStruct(a, state->spec, raw, 0, results, error);
    return a.vm->makeTuple(results);
}

// Struct.pack_into
// Struct.pack_into
// Struct.pack_into
static value fnStructPackInto(cffi& a){
    StructState* state = structState(a);
    if (!state) return a.none();
    value buffer = a.arg(0);
    long long offset = a.arg_int(1);
    std::vector<value> rest;
    for (size_t i = 2; i < a.argc(); ++i) rest.push_back(a.arg(i));
    std::string packed, error;
    if (!packStruct(a, state->spec, rest, 0, packed, error)){
        a.vm->raiseError("error", error);
        return a.none();
    }
    if (buffer.type == "bytearray"){
        auto* raw = static_cast<std::string*>(buffer.ptr);
        if (offset < 0) offset += static_cast<long long>(raw->size());
        if (offset < 0 || static_cast<size_t>(offset) + packed.size() > raw->size()){
            a.vm->raiseError("error", "pack_into requires a buffer of at least " +
                                      std::to_string(offset + packed.size()) + " bytes");
            return a.none();
        }
        raw->replace(static_cast<size_t>(offset), packed.size(), packed);
    }
    return a.none();
}

// Struct.unpack_from
// Struct.unpack_from
// Struct.unpack_from
static value fnStructUnpackFrom(cffi& a){
    StructState* state = structState(a);
    if (!state) return a.none();
    std::string raw = rawOf(a.arg(0));
    long long offset = a.argc() >= 2 ? a.arg_int(1) : 0;
    if (offset < 0) offset += static_cast<long long>(raw.size());
    std::string error;
    if (offset < 0 || !checkRawLength(a, raw, state->spec.size,
                                     static_cast<size_t>(offset), error)){
        a.vm->raiseError("error", error.empty() ? "offset must be non-negative" : error);
        return a.none();
    }
    std::vector<value> results;
    unpackStruct(a, state->spec, raw, static_cast<size_t>(offset), results, error);
    return a.vm->makeTuple(results);
}

// cffi struct lib
// cffi struct library
// cffi struct ᠨᠣᠮ
ffilib cffi_struct_lib(){
    ffi_class errorClass;
    errorClass.name = "error";
    ffi_class structClass;
    structClass.name = "Struct";
    structClass.constructor = (void*)fnStructConstructor;
    structClass.methods = {
        {"pack", (void*)fnStructPack},
        {"unpack", (void*)fnStructUnpack},
        {"pack_into", (void*)fnStructPackInto},
        {"unpack_from", (void*)fnStructUnpackFrom}
    };

    return cffi_lib(
        "struct",
        {
            {"calcsize", (void*)fnCalcSize},
            {"pack", (void*)fnPack},
            {"unpack", (void*)fnUnpack},
            {"pack_into", (void*)fnPackInto},
            {"unpack_from", (void*)fnUnpackFrom}
        },
        {},
        {
            {"error", errorClass},
            {"Struct", structClass}
        });
}


}
