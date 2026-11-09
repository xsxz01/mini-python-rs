#include "../cffi.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 数组状态
// array state
// ᠠᠷᠠᠢ ᠪᠠᠶᠢᠳᠠᠯ
struct ArrayState{
    char typecode = 'i';
    bool isFloat = false;
    bool isUnicode = false;
    bool swapped = false;
    std::vector<long long> ints;
    std::vector<double> floats;
};

// 数组元素个数
// array element count
// ᠠᠷᠠᠢ ᠡᠯᠡᠮᠡᠨᠲ ᠲᠣᠭ᠎ᠠ
size_t arrayLength(const ArrayState& s){
    return s.isFloat ? s.floats.size() : s.ints.size();
}

// 类型码尺寸
// typecode size
// ᠲᠥᠷᠥᠯ ᠺᠣᠳ᠋ ᠬᠡᠮᠵᠢᠶ᠎ᠡ
size_t typecodeSize(char code){
    switch (code){
        case 'b': case 'B': return 1;
        case 'h': case 'H': return 2;
        case 'i': case 'I': return sizeof(int);
        case 'l': case 'L': return sizeof(long);
        case 'q': case 'Q': return 8;
        case 'f': return 4;
        case 'd': return 8;
        case 'u': return sizeof(wchar_t);
        case 'w': return 4;
        default: return 0;
    }
}

// 是否浮点类型
// is float typecode
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠨᠠᠮᠠᠭ᠌᠎ᠠ ᠲᠥᠷᠥᠯ
bool isFloatType(char code){
    return code == 'f' || code == 'd';
}

// 是否 unicode 类型
// is unicode typecode
// unicode ᠲᠥᠷᠥᠯ ᠮᠦ
bool isUnicodeType(char code){
    return code == 'u' || code == 'w';
}

// 数组状态获取
// get array state
// ᠠᠷᠠᠢ ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
ArrayState* arrayState(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        if (inst->handle) return static_cast<ArrayState*>(inst->handle);
    }
    return nullptr;
}

// 检查 typecode
// validate typecode
// typecode ᠰᠢᠯᠠᠭᠠᠬᠤ
bool validTypecode(char code){
    return typecodeSize(code) != 0;
}

// 写一个 UTF-8 码点
// append one UTF-8 codepoint
// UTF-8 ᠨᠢᠭᠡ ᠺᠣᠳ᠋ ᠪᠢᠴᠢᠬᠦ
void pushCodepoint(std::string& out, unsigned int cp){
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

// 解码 UTF-8 文本为码点
// decode UTF-8 text into codepoints
// UTF-8 ᠪᠢᠴᠢᠭ ᠢ ᠲᠠᠢᠯᠠᠬᠤ
bool decodeCodepoints(const std::string& text, std::vector<long long>& cps){
    size_t i = 0;
    while (i < text.size()){
        unsigned char c = static_cast<unsigned char>(text[i]);
        long long cp = 0;
        size_t extra = 0;
        if (c < 0x80){
            cp = c;
        } else if ((c >> 5) == 0x6){
            cp = c & 0x1F;
            extra = 1;
        } else if ((c >> 4) == 0xE){
            cp = c & 0x0F;
            extra = 2;
        } else if ((c >> 3) == 0x1E){
            cp = c & 0x07;
            extra = 3;
        } else {
            return false;
        }
        if (i + extra >= text.size()) return false;
        for (size_t k = 1; k <= extra; ++k){
            unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc >> 6) != 0x2) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += extra + 1;
        cps.push_back(cp);
    }
    return true;
}

// 字符串值转数组单元
// convert a string value into array units
// ᠦᠰᠦᠭ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠡᠯᠡᠮᠡᠨᠲ ᠪᠣᠯᠭᠠᠬᠤ
bool valueToUnits(cffi& a, const value& item, const ArrayState& s,
                  std::vector<long long>& units, bool singleOnly){
    if (item.type != "str"){
        std::string typeName = item.type == "none" ? "NoneType" : item.type;
        a.vm->raiseError("TypeError",
                         "array item must be a unicode character, not " + typeName);
        return false;
    }
    std::vector<long long> cps;
    if (!decodeCodepoints(a.vm->toString(item), cps)){
        a.vm->raiseError("TypeError", "array item must be a unicode character");
        return false;
    }
    for (long long cp : cps){
        if (s.typecode == 'w'){
            units.push_back(cp);
        } else if (cp > 0xFFFF){
            long long v = cp - 0x10000;
            units.push_back(0xD800 + (v >> 10));
            units.push_back(0xDC00 + (v & 0x3FF));
        } else {
            units.push_back(cp);
        }
    }
    if (singleOnly && units.size() != 1){
        a.vm->raiseError("TypeError",
                         "array item must be a unicode character, not a string of length " +
                             std::to_string(units.size()));
        return false;
    }
    return true;
}

// 数组单元转字符串值
// convert an array unit into a string value
// ᠡᠯᠡᠮᠡᠨᠲ ᠢ ᠦᠰᠦᠭ ᠪᠣᠯᠭᠠᠬᠤ
value unitToValue(cffi& a, long long unit){
    std::string text;
    pushCodepoint(text, static_cast<unsigned int>(unit));
    return a.mkstr(text);
}

// 读取 long double 通用的元素值
// read the numeric element at index
// ᠡᠯᠡᠮᠡᠨᠲ ᠬᠠᠯᠠᠬᠤ
value elementValue(cffi& a, const ArrayState& s, size_t index){
    if (s.isFloat) return a.mkfloat(s.floats[index]);
    if (s.isUnicode) return unitToValue(a, s.ints[index]);
    return a.mkint(s.ints[index]);
}

// 追加一个元素值
// append one value as element
// ᠨᠢᠭᠡ ᠡᠯᠡᠮᠡᠨᠲ ᠨᠡᠮᠡᠬᠦ
bool appendElement(cffi& a, ArrayState& s, const value& item){
    if (s.isFloat){
        s.floats.push_back(a.vm->toFloat(item));
        return true;
    }
    if (s.isUnicode){
        std::vector<long long> units;
        if (!valueToUnits(a, item, s, units, false)) return false;
        for (long long u : units) s.ints.push_back(u);
        return true;
    }
    s.ints.push_back(a.vm->toInt(item));
    return true;
}

// 反转整数的字节
// byte-swap an integer of given size
// ᠲᠣᠭ᠎ᠠ ᠶᠢᠨ ᠪᠠᠶᠢᠲ᠋ ᠰᠣᠯᠢᠬᠤ
long long swapInteger(long long v, size_t size){
    unsigned long long x = static_cast<unsigned long long>(v);
    unsigned long long r = 0;
    for (size_t i = 0; i < size; ++i){
        r = (r << 8) | (x & 0xFFull);
        x >>= 8;
    }
    return static_cast<long long>(r);
}

// 反转浮点数的字节
// byte-swap a floating point value
// ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ ᠲᠣᠭ᠎ᠠ ᠶᠢᠨ ᠪᠠᠶᠢᠲ᠋ ᠰᠣᠯᠢᠬᠤ
double swapFloat(double d, size_t size){
    if (size == 4){
        float f = static_cast<float>(d);
        unsigned char bytes[4];
        std::memcpy(bytes, &f, 4);
        unsigned char flipped[4];
        for (size_t i = 0; i < 4; ++i) flipped[i] = bytes[3 - i];
        float g = 0;
        std::memcpy(&g, flipped, 4);
        return static_cast<double>(g);
    }
    unsigned char bytes[8];
    std::memcpy(bytes, &d, 8);
    unsigned char flipped[8];
    for (size_t i = 0; i < 8; ++i) flipped[i] = bytes[7 - i];
    double g = 0;
    std::memcpy(&g, flipped, 8);
    return g;
}

// 写整数元素的本机字节
// write integer element bytes in native order
// ᠲᠣᠭ᠎ᠠ ᠡᠯᠡᠮᠡᠨᠲ ᠪᠠᠶᠢᠲ᠋ ᠪᠢᠴᠢᠬᠦ
void appendIntegerBytes(std::string& out, long long v, size_t size){
    unsigned long long x = static_cast<unsigned long long>(v);
    if constexpr (std::endian::native == std::endian::little){
        for (size_t i = 0; i < size; ++i)
            out.push_back(static_cast<char>((x >> (8 * i)) & 0xFF));
    } else {
        for (size_t i = 0; i < size; ++i)
            out.push_back(static_cast<char>((x >> (8 * (size - 1 - i))) & 0xFF));
    }
}

// 读整数元素的本机字节
// read integer element bytes in native order
// ᠲᠣᠭ᠎ᠠ ᠡᠯᠡᠮᠡᠨᠲ ᠪᠠᠶᠢᠲ᠋ ᠤᠨᠰᠢᠬᠤ
long long readIntegerBytes(const std::string& raw, size_t offset, size_t size){
    unsigned long long v = 0;
    if constexpr (std::endian::native == std::endian::little){
        for (size_t i = 0; i < size; ++i)
            v |= static_cast<unsigned long long>(
                     static_cast<unsigned char>(raw[offset + i])) << (8 * i);
    } else {
        for (size_t i = 0; i < size; ++i)
            v |= static_cast<unsigned long long>(
                     static_cast<unsigned char>(raw[offset + i])) << (8 * (size - 1 - i));
    }
    return static_cast<long long>(v);
}

// array 构造器
// array constructor
// array ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnArrayConstructor(cffi& a){
    std::string codeText = a.arg_str(0);
    if (codeText.size() != 1 || !validTypecode(codeText[0])){
        a.vm->raiseError("ValueError",
                         "bad typecode (must be b, B, u, h, H, i, I, l, L, q, Q, f or d)");
        return a.none();
    }
    char code = codeText[0];
    auto* state = new ArrayState();
    state->typecode = code;
    state->isFloat = isFloatType(code);
    state->isUnicode = isUnicodeType(code);
    value obj = a.mkobject(state);
    a.setattr(obj, "typecode", a.mkstr(codeText));
    a.setattr(obj, "itemsize", a.mkint(static_cast<long long>(typecodeSize(code))));
    if (a.argc() >= 2 && a.arg(1).type != "none"){
        if (state->isUnicode && a.arg(1).type == "str"){
            std::vector<long long> units;
            if (!valueToUnits(a, a.arg(1), *state, units, false)) return a.none();
            for (long long u : units) state->ints.push_back(u);
        } else {
            std::vector<value> items;
            a.vm->materializeIterable(a.arg(1), items);
            if (a.vm->hasError()) return a.none();
            for (auto& item : items){
                if (!appendElement(a, *state, item)) return a.none();
            }
        }
    }
    return obj;
}

// array.append
value fnArrayAppend(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    appendElement(a, *s, a.arg(0));
    return a.none();
}

// array.extend
value fnArrayExtend(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (a.vm->hasError()) return a.none();
    for (auto& item : items){
        if (!appendElement(a, *s, item)) return a.none();
    }
    return a.none();
}

// array.insert
value fnArrayInsert(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    long long index = a.arg_int(0);
    size_t len = arrayLength(*s);
    if (index < 0) index += static_cast<long long>(len);
    if (index < 0) index = 0;
    if (index > static_cast<long long>(len)) index = static_cast<long long>(len);
    if (s->isFloat){
        s->floats.insert(s->floats.begin() + index, a.vm->toFloat(a.arg(1)));
    } else if (s->isUnicode){
        std::vector<long long> units;
        if (!valueToUnits(a, a.arg(1), *s, units, true)) return a.none();
        s->ints.insert(s->ints.begin() + index, units[0]);
    } else {
        s->ints.insert(s->ints.begin() + index, a.vm->toInt(a.arg(1)));
    }
    return a.none();
}

// array.pop
value fnArrayPop(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    size_t len = arrayLength(*s);
    if (len == 0){
        a.vm->raiseError("IndexError", "pop from empty array");
        return a.none();
    }
    long long index = a.argc() >= 1 ? a.arg_int(0) : -1;
    if (index < 0) index += static_cast<long long>(len);
    if (index < 0 || index >= static_cast<long long>(len)){
        a.vm->raiseError("IndexError", "pop index out of range");
        return a.none();
    }
    if (s->isFloat){
        double v = s->floats[static_cast<size_t>(index)];
        s->floats.erase(s->floats.begin() + index);
        return a.mkfloat(v);
    }
    long long v = s->ints[static_cast<size_t>(index)];
    s->ints.erase(s->ints.begin() + index);
    if (s->isUnicode) return unitToValue(a, v);
    return a.mkint(v);
}

// 查找 unicode 元素下标
// find a unicode unit index
// unicode ᠡᠯᠡᠮᠡᠨᠲ ᠢᠯᠭᠠᠬᠤ
bool findUnicode(cffi& a, ArrayState& s, const value& item, size_t& found, bool& ok){
    std::vector<long long> units;
    if (!valueToUnits(a, item, s, units, true)) return false;
    found = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < s.ints.size(); ++i){
        if (s.ints[i] == units[0]){
            found = i;
            break;
        }
    }
    ok = found != std::numeric_limits<size_t>::max();
    return true;
}

// array.remove
value fnArrayRemove(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    if (s->isFloat){
        double target = a.vm->toFloat(a.arg(0));
        for (size_t i = 0; i < s->floats.size(); ++i){
            if (s->floats[i] == target){
                s->floats.erase(s->floats.begin() + i);
                return a.none();
            }
        }
    } else if (s->isUnicode){
        size_t found = 0;
        bool ok = false;
        if (!findUnicode(a, *s, a.arg(0), found, ok)) return a.none();
        if (ok){
            s->ints.erase(s->ints.begin() + static_cast<long long>(found));
            return a.none();
        }
    } else {
        long long target = a.vm->toInt(a.arg(0));
        for (size_t i = 0; i < s->ints.size(); ++i){
            if (s->ints[i] == target){
                s->ints.erase(s->ints.begin() + i);
                return a.none();
            }
        }
    }
    a.vm->raiseError("ValueError", "array.remove(x): x not in array");
    return a.none();
}

// array.index
value fnArrayIndex(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    if (s->isFloat){
        double target = a.vm->toFloat(a.arg(0));
        for (size_t i = 0; i < s->floats.size(); ++i)
            if (s->floats[i] == target) return a.mkint(static_cast<long long>(i));
    } else if (s->isUnicode){
        size_t found = 0;
        bool ok = false;
        if (!findUnicode(a, *s, a.arg(0), found, ok)) return a.none();
        if (ok) return a.mkint(static_cast<long long>(found));
    } else {
        long long target = a.vm->toInt(a.arg(0));
        for (size_t i = 0; i < s->ints.size(); ++i)
            if (s->ints[i] == target) return a.mkint(static_cast<long long>(i));
    }
    a.vm->raiseError("ValueError", "array.index(x): x not in array");
    return a.none();
}

// array.count
value fnArrayCount(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    long long n = 0;
    if (s->isFloat){
        double target = a.vm->toFloat(a.arg(0));
        for (double v : s->floats) if (v == target) ++n;
    } else if (s->isUnicode){
        std::vector<long long> units;
        if (!valueToUnits(a, a.arg(0), *s, units, true)) return a.none();
        for (long long v : s->ints) if (v == units[0]) ++n;
    } else {
        long long target = a.vm->toInt(a.arg(0));
        for (long long v : s->ints) if (v == target) ++n;
    }
    return a.mkint(n);
}

// array.reverse
value fnArrayReverse(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    if (s->isFloat) std::reverse(s->floats.begin(), s->floats.end());
    else std::reverse(s->ints.begin(), s->ints.end());
    return a.none();
}

// array.clear
value fnArrayClear(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    s->ints.clear();
    s->floats.clear();
    return a.none();
}

// array.byteswap
value fnArrayByteswap(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    size_t size = typecodeSize(s->typecode);
    if (size <= 1) return a.none();
    if (s->isFloat){
        for (double& d : s->floats) d = swapFloat(d, size);
    } else {
        for (long long& v : s->ints) v = swapInteger(v, size);
    }
    return a.none();
}

// array.tolist
value fnArrayToList(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    std::vector<value> items;
    if (s->isFloat){
        for (double v : s->floats) items.push_back(a.mkfloat(v));
    } else if (s->isUnicode){
        for (long long v : s->ints) items.push_back(unitToValue(a, v));
    } else {
        for (long long v : s->ints) items.push_back(a.mkint(v));
    }
    return a.mklist(items);
}

// array.fromlist
value fnArrayFromList(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    if (a.vm->hasError()) return a.none();
    for (auto& item : items){
        if (!appendElement(a, *s, item)) return a.none();
    }
    return a.none();
}

// array.tobytes
value fnArrayToBytes(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    size_t size = typecodeSize(s->typecode);
    std::string out;
    if (s->isFloat){
        for (double d : s->floats){
            if (s->typecode == 'f'){
                float f = static_cast<float>(d);
                unsigned char bytes[4];
                std::memcpy(bytes, &f, 4);
                for (size_t k = 0; k < 4; ++k)
                    out.push_back(static_cast<char>(bytes[k]));
            } else {
                unsigned char bytes[8];
                std::memcpy(bytes, &d, 8);
                for (size_t k = 0; k < 8; ++k)
                    out.push_back(static_cast<char>(bytes[k]));
            }
        }
    } else {
        for (long long v : s->ints) appendIntegerBytes(out, v, size);
    }
    value result = a.mkstr(out);
    result.type = "bytes";
    return result;
}

// array.frombytes
value fnArrayFromBytes(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    std::string raw;
    value arg = a.arg(0);
    if ((arg.type == "bytes" || arg.type == "bytearray" || arg.type == "str") && arg.ptr)
        raw = *static_cast<std::string*>(arg.ptr);
    size_t size = typecodeSize(s->typecode);
    if (size == 0 || raw.size() % size != 0){
        a.vm->raiseError("ValueError", "bytes length not a multiple of item size");
        return a.none();
    }
    for (size_t offset = 0; offset < raw.size(); offset += size){
        if (s->isFloat){
            if (size == 4){
                float f = 0;
                std::memcpy(&f, raw.data() + offset, 4);
                s->floats.push_back(static_cast<double>(f));
            } else {
                double d = 0;
                std::memcpy(&d, raw.data() + offset, 8);
                s->floats.push_back(d);
            }
        } else {
            s->ints.push_back(readIntegerBytes(raw, offset, size));
        }
    }
    return a.none();
}

// array.buffer_info
value fnArrayBufferInfo(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    const void* address = s->isFloat
        ? static_cast<const void*>(s->floats.data())
        : static_cast<const void*>(s->ints.data());
    std::vector<value> pair;
    pair.push_back(a.mkint(static_cast<long long>(reinterpret_cast<intptr_t>(address))));
    pair.push_back(a.mkint(static_cast<long long>(arrayLength(*s))));
    return a.vm->makeTuple(pair);
}

// array.__len__
value fnArrayLen(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.mkint(0);
    return a.mkint(static_cast<long long>(arrayLength(*s)));
}

// array.__getitem__
value fnArrayGetItem(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    size_t len = arrayLength(*s);
    long long index = a.arg_int(0);
    if (index < 0) index += static_cast<long long>(len);
    if (index < 0 || index >= static_cast<long long>(len)){
        a.vm->raiseError("IndexError", "array index out of range");
        return a.none();
    }
    return elementValue(a, *s, static_cast<size_t>(index));
}

// array.__setitem__
value fnArraySetItem(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.none();
    size_t len = arrayLength(*s);
    long long index = a.arg_int(0);
    if (index < 0) index += static_cast<long long>(len);
    if (index < 0 || index >= static_cast<long long>(len)){
        a.vm->raiseError("IndexError", "array assignment index out of range");
        return a.none();
    }
    if (s->isFloat){
        s->floats[static_cast<size_t>(index)] = a.vm->toFloat(a.arg(1));
    } else if (s->isUnicode){
        std::vector<long long> units;
        if (!valueToUnits(a, a.arg(1), *s, units, true)) return a.none();
        s->ints[static_cast<size_t>(index)] = units[0];
    } else {
        s->ints[static_cast<size_t>(index)] = a.vm->toInt(a.arg(1));
    }
    return a.none();
}

// array.__repr__
value fnArrayRepr(cffi& a){
    ArrayState* s = arrayState(a);
    if (!s) return a.mkstr("array()");
    size_t len = arrayLength(*s);
    std::string text = "array('";
    text.push_back(s->typecode);
    if (len == 0){
        text += "')";
        return a.mkstr(text);
    }
    if (s->isUnicode){
        std::string joined;
        for (long long v : s->ints) pushCodepoint(joined, static_cast<unsigned int>(v));
        text += "', ";
        text += pythonRepr(*a.vm, a.mkstr(joined));
        text += ")";
        return a.mkstr(text);
    }
    text += "', [";
    for (size_t i = 0; i < len; ++i){
        if (i) text += ", ";
        value item = elementValue(a, *s, i);
        text += pythonRepr(*a.vm, item);
    }
    text += "])";
    return a.mkstr(text);
}

}  // namespace

// cffi array lib
// cffi array library
// cffi array ᠨᠣᠮ
ffilib cffi_array_lib(){
    ffi_class arrayClass;
    arrayClass.name = "array";
    arrayClass.constructor = (void*)fnArrayConstructor;
    arrayClass.methods = {
        {"append", (void*)fnArrayAppend},
        {"extend", (void*)fnArrayExtend},
        {"insert", (void*)fnArrayInsert},
        {"pop", (void*)fnArrayPop},
        {"remove", (void*)fnArrayRemove},
        {"index", (void*)fnArrayIndex},
        {"count", (void*)fnArrayCount},
        {"reverse", (void*)fnArrayReverse},
        {"clear", (void*)fnArrayClear},
        {"byteswap", (void*)fnArrayByteswap},
        {"tolist", (void*)fnArrayToList},
        {"fromlist", (void*)fnArrayFromList},
        {"tobytes", (void*)fnArrayToBytes},
        {"tostring", (void*)fnArrayToBytes},
        {"frombytes", (void*)fnArrayFromBytes},
        {"fromstring", (void*)fnArrayFromBytes},
        {"buffer_info", (void*)fnArrayBufferInfo},
        {"__len__", (void*)fnArrayLen},
        {"__getitem__", (void*)fnArrayGetItem},
        {"__setitem__", (void*)fnArraySetItem},
        {"__repr__", (void*)fnArrayRepr}
    };

    return cffi_lib(
        "array",
        {},
        {},
        {{"array", arrayClass}});
}


}
