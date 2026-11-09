#include "../cffi.hpp"
#include "../platform.hpp"

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace minipy {


bool ctypesCall(minipy::vm& vm, minipy::ffi_instance& self, const std::vector<minipy::value>& args,
                const std::map<std::string, minipy::value>& kwargs, minipy::value& result);
bool ctypesGet(minipy::vm& vm, void* handle, const std::string& name, minipy::value& result);


enum{
    ctArgWord = 0,   // 原样 64 位: 指针 / long long / 浮点
    ctArgS8 = 1,     // 有符号 8 位
    ctArgU8 = 2,     // 无符号 8 位
    ctArgS16 = 3,    // 有符号 16 位
    ctArgU16 = 4,    // 无符号 16 位
    ctArgS32 = 5,    // 有符号 32 位
    ctArgU32 = 6     // 无符号 32 位
};

// 按 ctypes 类型名确定整数宽度种类
// Map a ctypes type name to an integer width kind
// ctypes ᠲᠥᠷᠥᠯ ᠦᠨ ᠨᠡᠷᠡ ᠪᠡᠷ ᠴᠠᠭᠠᠨ ᠤ ᠣᠷᠭᠠᠨ ᠴᠣᠮ ᠢ ᠲᠣᠭᠲᠠᠭᠠᠬᠤ
static int ctypesArgKind(const std::string& typeName){
    static const bool longIs32 = sizeof(long) == 4;
    if (typeName == "c_int" || typeName == "c_int32") return ctArgS32;
    if (typeName == "c_uint" || typeName == "c_uint32") return ctArgU32;
    if (typeName == "c_short") return ctArgS16;
    if (typeName == "c_ushort") return ctArgU16;
    if (typeName == "c_byte" || typeName == "c_char") return ctArgS8;
    if (typeName == "c_ubyte") return ctArgU8;
    if (typeName == "c_wchar") return ctArgU16;
    if (typeName == "c_bool") return ctArgU8;
    if (typeName == "c_long") return longIs32 ? ctArgS32 : ctArgWord;
    if (typeName == "c_ulong") return longIs32 ? ctArgU32 : ctArgWord;
    return ctArgWord;
}

// 参数在栈上占用的字节数: 仅 32 位 x86 需要区分 (64 位平台栈槽固定 8 字节)
// Bytes an argument occupies on the stack; only x86-32 needs this (64-bit slots are always 8)
// ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠳᠤ ᠡᠵᠡᠯᠡᠬᠦ ᠪᠠᠢᠲᠤ
static int ctypesArgSize(const std::string& typeName){
#if defined(_M_IX86) || defined(__i386__)
    if (typeName == "c_longlong" || typeName == "c_ulonglong" ||
        typeName == "c_int64" || typeName == "c_uint64" || typeName == "c_double")
        return 8;
    if (typeName == "c_size_t" || typeName == "c_ssize_t")
        return static_cast<int>(sizeof(size_t));
    if (typeName == "c_long" || typeName == "c_ulong")
        return static_cast<int>(sizeof(long));
    if (typeName == "c_void_p" || typeName == "c_char_p" || typeName == "c_wchar_p")
        return static_cast<int>(sizeof(void*));
    return 4;
#else
    (void)typeName;
    return 8;
#endif
}

// 按宽度种类把原始整数规范化 (截断到低位并做符号/零扩展)
// Normalize a raw integer by width kind (truncate to low bits, then sign/zero extend)
// ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠴᠠᠭᠠᠨ ᠢ ᠬᠡᠪᠲᠡᠭᠡᠬᠦ
static long long ctypesNormalizeArg(long long raw, int kind){
    switch (kind){
    case ctArgS8: return static_cast<long long>(static_cast<signed char>(raw));
    case ctArgU8: return static_cast<long long>(static_cast<unsigned char>(raw));
    case ctArgS16: return static_cast<long long>(static_cast<short>(raw));
    case ctArgU16: return static_cast<long long>(static_cast<unsigned short>(raw));
    case ctArgS32: return static_cast<long long>(static_cast<int>(raw));
    case ctArgU32: return static_cast<long long>(static_cast<unsigned int>(raw));
    default: return raw;
    }
}

// 已加载共享库缓存, 同一名称只加载一次
// Cache of loaded shared libraries so each name is loaded once
// ᠲᠠᠲᠠᠭᠰᠠᠨ ᠰᠠᠩ ᠤᠨ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ
std::map<std::string, mp_module>& moduleCache(){
    static std::map<std::string, mp_module> cache;
    return cache;
}

// ends With Char
// ends with char
// ends With Char
bool endsWithChar(const std::string& text, char c){
    return !text.empty() && text.back() == c;
}


// 加载 模块
// load module
// ᠠᠶᠢᠯᠳᠠᠬᠤ ᠦᠪᠲ᠋ᠠᠢ
mp_module loadModule(const std::string& name, std::string& error){
    auto it = moduleCache().find(name);
    if (it != moduleCache().end()) return it->second;

// 按平台候选名依次尝试, Windows 自动补 .dll, 类 Unix 补 .so / .dylib
// Try platform candidates in order: .dll on Windows, .so or .dylib on Unix
// ᠲᠠᠯᠠᠪᠠᠢ ᠶᠢᠨ ᠨᠡᠷᠡ ᠦᠨ ᠵᠢᠭᠰᠠᠭᠠᠯ ᠢ ᠳᠠᠷᠠᠭ᠎ᠠ ᠪᠡᠷ ᠰᠢᠯᠭᠠᠨ᠎ᠠ
    mp_module module = nullptr;
    std::string lastError;
    for (const auto& candidate : mpSharedLibraryCandidates(name)){
        module = mpLoadLibrary(candidate);
        if (module) break;
        lastError = mpLibraryError();
    }
    if (!module){
        error = "Could not find module '" + name + "'";
        if (!lastError.empty()) error += ": " + lastError;
        return nullptr;
    }
    moduleCache()[name] = module;
    return module;
}

// 构造 Library 对象
// make library object
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Library ᠣᠪᠶᠡᠭᠲ
minipy::value makeLibraryObject(minipy::vm& vm, const std::string& dllName){
    auto* info = new CtypesHandle();
    info->kind = ctypes_library;
    info->dllName = dllName;
    return vm.makeDynamicObject("CDLL", info, ctypesGet, nullptr);
}


// ctypes 获取
// ctypes get
// ctypes ᠬᠠᠯᠠᠬᠤ
bool ctypesGet(minipy::vm& vm, void* handle, const std::string& name, minipy::value& result){
    auto* info = static_cast<CtypesHandle*>(handle);
    if (info == nullptr) return false;
    if (info->kind == ctypes_loader){
        std::string error;
        if (loadModule(name, error) == nullptr){
            vm.raiseError("OSError", error);
            return true;
        }
        result = makeLibraryObject(vm, name);
        return true;
    }
    if (info->kind == ctypes_library){
        std::string error;
        mp_module module = loadModule(info->dllName, error);
        if (module == nullptr){
            vm.raiseError("OSError", error);
            return true;
        }
        void* proc = mpGetProcAddress(module, name.c_str());
        if (proc == nullptr) return false;
        auto* fn = new CtypesHandle();
        fn->kind = ctypes_function;
        fn->dllName = info->dllName;
        fn->procName = name;
        fn->procAddress = proc;
        result = vm.makeDynamicObject("CFunction", fn, nullptr, ctypesCall);
        return true;
    }
    return false;
}



using fn0 = long long (*)();
using fn1 = long long (*)(void*);
using fn2 = long long (*)(void*, void*);
using fn3 = long long (*)(void*, void*, void*);
using fn4 = long long (*)(void*, void*, void*, void*);
using fn5 = long long (*)(void*, void*, void*, void*, void*);
using fn6 = long long (*)(void*, void*, void*, void*, void*, void*);
using fn7 = long long (*)(void*, void*, void*, void*, void*, void*, void*);
using fn8 = long long (*)(void*, void*, void*, void*, void*, void*, void*, void*);
using fn9 = long long (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*);
using fn10 = long long (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
using fn11 = long long (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
using fn12 = long long (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);

using fnd10 = double (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);


#define CT_SIG(r, a0, a1, a2, a3) r (*)(a0, a1, a2, a3)
using fi0  = CT_SIG(long long, void*, void*, void*, void*);
using fi1  = CT_SIG(long long, double, void*, void*, void*);
using fi2  = CT_SIG(long long, void*, double, void*, void*);
using fi3  = CT_SIG(long long, double, double, void*, void*);
using fi4  = CT_SIG(long long, void*, void*, double, void*);
using fi5  = CT_SIG(long long, double, void*, double, void*);
using fi6  = CT_SIG(long long, void*, double, double, void*);
using fi7  = CT_SIG(long long, double, double, double, void*);
using fi8  = CT_SIG(long long, void*, void*, void*, double);
using fi9  = CT_SIG(long long, double, void*, void*, double);
using fi10 = CT_SIG(long long, void*, double, void*, double);
using fi11 = CT_SIG(long long, double, double, void*, double);
using fi12 = CT_SIG(long long, void*, void*, double, double);
using fi13 = CT_SIG(long long, double, void*, double, double);
using fi14 = CT_SIG(long long, void*, double, double, double);
using fi15 = CT_SIG(long long, double, double, double, double);
using fd0  = CT_SIG(double, void*, void*, void*, void*);
using fd1  = CT_SIG(double, double, void*, void*, void*);
using fd2  = CT_SIG(double, void*, double, void*, void*);
using fd3  = CT_SIG(double, double, double, void*, void*);
using fd4  = CT_SIG(double, void*, void*, double, void*);
using fd5  = CT_SIG(double, double, void*, double, void*);
using fd6  = CT_SIG(double, void*, double, double, void*);
using fd7  = CT_SIG(double, double, double, double, void*);
using fd8  = CT_SIG(double, void*, void*, void*, double);
using fd9  = CT_SIG(double, double, void*, void*, double);
using fd10 = CT_SIG(double, void*, double, void*, double);
using fd11 = CT_SIG(double, double, double, void*, double);
using fd12 = CT_SIG(double, void*, void*, double, double);
using fd13 = CT_SIG(double, double, void*, double, double);
using fd14 = CT_SIG(double, void*, double, double, double);
using fd15 = CT_SIG(double, double, double, double, double);
#undef CT_SIG


bool ctypesCall(minipy::vm& vm, minipy::ffi_instance& self, const std::vector<minipy::value>& args,
                const std::map<std::string, minipy::value>& kwargs, minipy::value& result){
    auto* info = static_cast<CtypesHandle*>(self.handle);
    if (info == nullptr || info->kind != ctypes_function || info->procAddress == nullptr){
        return false;
    }
    if (!kwargs.empty()){
        vm.raiseError("TypeError", info->procName + "() takes no keyword arguments");
        return true;
    }
    if (args.size() > 20){
        vm.raiseError("TypeError", info->procName + "() takes at most 20 arguments (" +
                                   std::to_string(args.size()) + " given)");
        return true;
    }

    bool returnsDouble = false;
    // 声明的返回宽度: 未声明时按 64 位处理, 与既有行为保持一致
    // Declared return width: defaults to 64-bit when unspecified, preserving existing behavior
    // ᠲᠣᠭᠲᠠᠭᠠᠭᠳᠠᠭᠰᠠᠨ ᠪᠤᠴᠠᠭᠤᠯᠤᠭ᠎ᠠ ᠤᠨ ᠣᠷᠭᠠᠨ
    int returnKind = ctArgWord;
    auto restypeIt = self.attributes.find("restype");
    if (restypeIt != self.attributes.end() &&
        restypeIt->second.type == "native_object" && restypeIt->second.ptr != nullptr){
        auto* marker = static_cast<minipy::ffi_instance*>(restypeIt->second.ptr);
        if (marker->typeName == "c_double") returnsDouble = true;
        else returnKind = ctypesArgKind(marker->typeName);
    }

    bool wide = endsWithChar(info->procName, 'W');
    const size_t count = args.size();
    void* raw[20] = {nullptr, nullptr, nullptr, nullptr, nullptr,
                     nullptr, nullptr, nullptr, nullptr, nullptr,
                     nullptr, nullptr, nullptr, nullptr, nullptr,
                     nullptr, nullptr, nullptr, nullptr, nullptr};
    double dbl[4] = {0.0, 0.0, 0.0, 0.0};
    int pattern = 0;
    std::vector<std::wstring> wideStrings(count);
    std::vector<std::string> narrowStrings(count);
    for (size_t i = 0; i < count; ++i){
        const minipy::value& arg = args[i];
        if (arg.type == "none") continue;
        if (arg.type == "float"){
            if (i >= 4){
                vm.raiseError("ArgumentError", "argument " + std::to_string(i + 1) +
                                               ": floating-point arguments are only supported "
                                               "for the first 4 parameters");
                return true;
            }
            pattern |= (1 << static_cast<int>(i));
            dbl[i] = vm.toFloat(arg);
            continue;
        }
        if (arg.type == "bool" || arg.type == "int"){
            raw[i] = reinterpret_cast<void*>(static_cast<long long>(vm.toInt(arg)));
            continue;
        }
        if (arg.type == "str"){
            if (wide){
                wideStrings[i] = utf8ToWide(vm.toString(arg));
                raw[i] = const_cast<wchar_t*>(wideStrings[i].c_str());
            } else {
                narrowStrings[i] = vm.toString(arg);
                raw[i] = const_cast<char*>(narrowStrings[i].c_str());
            }
            continue;
        }
        if (arg.type == "native_object" && arg.ptr != nullptr){
            auto* inst = static_cast<minipy::ffi_instance*>(arg.ptr);
            // 回调对象传给 C 时必须使用跳板地址, 而不是内部的封装结构
            // A callback object must hand its trampoline address to C, not the wrapper
            // ᠪᠤᠴᠠᠭᠤᠯᠬᠤ ᠣᠪᠶᠡᠭᠲ ᠤᠨ ᠬᠠᠶᠢᠭ᠎ᠠ
            if (inst->typeName == "Callback" && inst->handle != nullptr){
                raw[i] = static_cast<CtypesCBWrap*>(inst->handle)->trampoline;
            } else {
                raw[i] = inst->handle;
            }
            continue;
        }
        std::string typeName = arg.type == "none" ? "NoneType" : arg.type;
        vm.raiseError("ArgumentError", "argument " + std::to_string(i + 1) +
                                       ": Don't know how to convert parameter " +
                                       std::to_string(i + 1) + " (type '" + typeName + "')");
        return true;
    }
    void* proc = info->procAddress;
    if (pattern != 0){

        if (count > 4){
            vm.raiseError("ArgumentError", info->procName + "(): floating-point parameters after "
                                           "the first 4 arguments are not supported");
            return true;
        }
        void* r0 = raw[0]; void* r1 = raw[1]; void* r2 = raw[2]; void* r3 = raw[3];
        double d0 = dbl[0]; double d1 = dbl[1]; double d2 = dbl[2]; double d3 = dbl[3];
        if (returnsDouble){
            double dret = 0.0;
            switch (pattern){
#define CT_CASE_D(p, a0, a1, a2, a3) case p: dret = reinterpret_cast<fd##p>(proc)(a0, a1, a2, a3); break;
                CT_CASE_D(0, r0, r1, r2, r3)
                CT_CASE_D(1, d0, r1, r2, r3)
                CT_CASE_D(2, r0, d1, r2, r3)
                CT_CASE_D(3, d0, d1, r2, r3)
                CT_CASE_D(4, r0, r1, d2, r3)
                CT_CASE_D(5, d0, r1, d2, r3)
                CT_CASE_D(6, r0, d1, d2, r3)
                CT_CASE_D(7, d0, d1, d2, r3)
                CT_CASE_D(8, r0, r1, r2, d3)
                CT_CASE_D(9, d0, r1, r2, d3)
                CT_CASE_D(10, r0, d1, r2, d3)
                CT_CASE_D(11, d0, d1, r2, d3)
                CT_CASE_D(12, r0, r1, d2, d3)
                CT_CASE_D(13, d0, r1, d2, d3)
                CT_CASE_D(14, r0, d1, d2, d3)
                CT_CASE_D(15, d0, d1, d2, d3)
#undef CT_CASE_D
            }
            result = vm.makeFloat(dret);
        } else {
            long long ret = 0;
            switch (pattern){
#define CT_CASE_I(p, a0, a1, a2, a3) case p: ret = reinterpret_cast<fi##p>(proc)(a0, a1, a2, a3); break;
                CT_CASE_I(0, r0, r1, r2, r3)
                CT_CASE_I(1, d0, r1, r2, r3)
                CT_CASE_I(2, r0, d1, r2, r3)
                CT_CASE_I(3, d0, d1, r2, r3)
                CT_CASE_I(4, r0, r1, d2, r3)
                CT_CASE_I(5, d0, r1, d2, r3)
                CT_CASE_I(6, r0, d1, d2, r3)
                CT_CASE_I(7, d0, d1, d2, r3)
                CT_CASE_I(8, r0, r1, r2, d3)
                CT_CASE_I(9, d0, r1, r2, d3)
                CT_CASE_I(10, r0, d1, r2, d3)
                CT_CASE_I(11, d0, d1, r2, d3)
                CT_CASE_I(12, r0, r1, d2, d3)
                CT_CASE_I(13, d0, r1, d2, d3)
                CT_CASE_I(14, r0, d1, d2, d3)
                CT_CASE_I(15, d0, d1, d2, d3)
#undef CT_CASE_I
            }
            ret = ctypesNormalizeArg(ret, returnKind);
            result = vm.makeInt(ret);
        }
        return true;
    }

    if (returnsDouble){
        double dret = reinterpret_cast<fnd10>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4],
                                                    raw[5], raw[6], raw[7], raw[8], raw[9]);
        result = vm.makeFloat(dret);
        return true;
    }


    long long ret = 0;
    switch (count){
        case 0: ret = reinterpret_cast<fn0>(proc)(); break;
        case 1: ret = reinterpret_cast<fn1>(proc)(raw[0]); break;
        case 2: ret = reinterpret_cast<fn2>(proc)(raw[0], raw[1]); break;
        case 3: ret = reinterpret_cast<fn3>(proc)(raw[0], raw[1], raw[2]); break;
        case 4: ret = reinterpret_cast<fn4>(proc)(raw[0], raw[1], raw[2], raw[3]); break;
        case 5: ret = reinterpret_cast<fn5>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4]); break;
        case 6: ret = reinterpret_cast<fn6>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]); break;
        case 7: ret = reinterpret_cast<fn7>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]); break;
        case 8: ret = reinterpret_cast<fn8>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]); break;
        case 9: ret = reinterpret_cast<fn9>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8]); break;
        case 10: ret = reinterpret_cast<fn10>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9]); break;
        case 11: ret = reinterpret_cast<fn11>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9], raw[10]); break;
        case 12: ret = reinterpret_cast<fn12>(proc)(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], raw[8], raw[9], raw[10], raw[11]); break;
        default: break;
    }
    ret = ctypesNormalizeArg(ret, returnKind);
    result = vm.makeInt(ret);
    return true;
}


// 构造 Loader
// make loader
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Loader
minipy::value makeLoader(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    auto* info = new CtypesHandle();
    info->kind = ctypes_loader;
    return vm.makeDynamicObject("LibraryLoader", info, ctypesGet, nullptr);
}

// data Windll
// data windll
// data Windll
static minipy::value dataWindll(minipy::cffi& a){ return makeLoader(a); }

// data Cdll
// data cdll
// data Cdll
static minipy::value dataCdll(minipy::cffi& a){ return makeLoader(a); }

// data Oledll
// data oledll
// data Oledll
static minipy::value dataOledll(minipy::cffi& a){ return makeLoader(a); }

// 加载 Library
// load library
// ᠠᠶᠢᠯᠳᠠᠬᠤ Library
minipy::value loadLibrary(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    if (a.argc() == 0){
        vm.raiseError("TypeError", "LoadLibrary() missing required argument: 'name'");
        return a.none();
    }
    std::string name = a.arg_str(0);
    std::string error;
    if (loadModule(name, error) == nullptr){
        vm.raiseError("OSError", error);
        return a.none();
    }
    return makeLibraryObject(vm, name);
}

// fn Win D L L
// fn win d l l
// fn Win D L L
static minipy::value fnWinDLL(minipy::cffi& a){ return loadLibrary(a); }

// fn C D L L
// fn c d l l
// fn C D L L
static minipy::value fnCDLL(minipy::cffi& a){ return loadLibrary(a); }

// fn 加载 Library
// fn load library
// fn ᠠᠶᠢᠯᠳᠠᠬᠤ Library
static minipy::value fnLoadLibrary(minipy::cffi& a){ return loadLibrary(a); }




static bool typeMarkerCall(minipy::vm& vm, minipy::ffi_instance& self,
                           const std::vector<minipy::value>& args,
                           const std::map<std::string, minipy::value>& kwargs,
                           minipy::value& result){
    if (args.empty()){
        result = vm.makeInt(0);
    } else {
        result = args[0];
    }
    return true;
}


// 构造 类型 Marker
// make type marker
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠲᠥᠷᠥᠯ Marker
minipy::value makeTypeMarker(minipy::cffi& a, const std::string& name){
    return a.vm->makeDynamicObject(name, nullptr, nullptr, typeMarkerCall);
}

// data C Double
// data c double
// data C Double
static minipy::value dataCDouble(minipy::cffi& a){ return makeTypeMarker(a, "c_double"); }

// data C 整数
// data c int
// data C ᠳ᠋ᠠᠩ ᠲᠣ᠎
static minipy::value dataCInt(minipy::cffi& a){ return makeTypeMarker(a, "c_int"); }

// data C Void P
// data c void p
// data C Void P
static minipy::value dataCVoidP(minipy::cffi& a){ return makeTypeMarker(a, "c_void_p"); }

// data C Char P
// data c char p
// data C Char P
static minipy::value dataCCharP(minipy::cffi& a){ return makeTypeMarker(a, "c_char_p"); }

// data C Wchar P
// data c wchar p
// data C Wchar P
static minipy::value dataCWcharP(minipy::cffi& a){ return makeTypeMarker(a, "c_wchar_p"); }

// 生成整数/布尔类型标记的 data 入口
// Produce an integer/bool type-marker data entry
// ᠳ᠋ᠠᠩ ᠲᠣ᠎ ᠮᠠᠷᠬᠢᠷ ᠤᠨ ᠭᠠᠷᠠᠯᠭ᠎ᠠ
#define MP_CTYPES_MARKER(fnName, typeName) \
    static minipy::value fnName(minipy::cffi& a){ return makeTypeMarker(a, typeName); }
MP_CTYPES_MARKER(dataCUInt, "c_uint")
MP_CTYPES_MARKER(dataCLong, "c_long")
MP_CTYPES_MARKER(dataCULong, "c_ulong")
MP_CTYPES_MARKER(dataCLongLong, "c_longlong")
MP_CTYPES_MARKER(dataCULongLong, "c_ulonglong")
MP_CTYPES_MARKER(dataCInt64, "c_int64")
MP_CTYPES_MARKER(dataCUInt64, "c_uint64")
MP_CTYPES_MARKER(dataCSsize, "c_ssize_t")
MP_CTYPES_MARKER(dataCSize, "c_size_t")
MP_CTYPES_MARKER(dataCShort, "c_short")
MP_CTYPES_MARKER(dataCUShort, "c_ushort")
MP_CTYPES_MARKER(dataCByte, "c_byte")
MP_CTYPES_MARKER(dataCUByte, "c_ubyte")
MP_CTYPES_MARKER(dataCChar, "c_char")
MP_CTYPES_MARKER(dataCWchar, "c_wchar")
MP_CTYPES_MARKER(dataCBool, "c_bool")
#undef MP_CTYPES_MARKER


// fn C Integer
// fn c integer
// fn C Integer
static minipy::value fnCInteger(minipy::cffi& a){
    return a.argc() > 0 ? a.mkint(a.arg_int(0)) : a.mkint(0);
}

// fn C 浮点
// fn c float
// fn C ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ
static minipy::value fnCFloat(minipy::cffi& a){
    return a.argc() > 0 ? a.mkfloat(a.arg_float(0)) : a.mkfloat(0.0);
}

// fn C 布尔
// fn c bool
// fn C ᠪᠤᠯ
static minipy::value fnCBool(minipy::cffi& a){
    return a.mkbool(a.argc() > 0 && a.arg_bool(0));
}

// fn C Pointer
// fn c pointer
// fn C Pointer
static minipy::value fnCPointer(minipy::cffi& a){
    if (a.argc() == 0) return a.none();
    auto value = a.arg(0);
    if (value.type == "str") return value;
    if (value.type == "none") return value;
    return a.mkint(a.arg_int(0));
}

// fn 获取 Last 错误
// fn get last error
// fn ᠬᠠᠯᠠᠬᠤ Last ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
static minipy::value fnGetLastError(minipy::cffi& a){
    return a.mkint(static_cast<long long>(mpLastError()));
}

// fn 集合 Last 错误
// fn set last error
// fn ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ Last ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
static minipy::value fnSetLastError(minipy::cffi& a){
    mpSetLastError(static_cast<unsigned long>(a.argc() > 0 ? a.arg_int(0) : 0));
    return a.none();
}







// fn Byref
// fn byref
// fn Byref
static minipy::value fnByref(minipy::cffi& a){
    if (a.argc() == 0) return a.none();
    minipy::value arg = a.arg(0);
    if (arg.type == "native_object" && arg.ptr){
        auto* inst = static_cast<minipy::ffi_instance*>(arg.ptr);
        if (inst->typeName == "Structure" && inst->handle){

            auto* info = static_cast<CtypesStructInfo*>(inst->handle);
            return a.mkint(reinterpret_cast<long long>(info->buffer.data()));
        }
        if (inst->handle){
            return a.mkint(reinterpret_cast<long long>(inst->handle));
        }
    }
    if (arg.type == "int") return arg;
    return a.none();
}





// Wintypes Info
// wintypes info
// Wintypes Info
struct WintypesInfo{
    minipy::vm* vm = nullptr;
};

// wintypes 获取
// wintypes get
// wintypes ᠬᠠᠯᠠᠬᠤ
static bool wintypesGet(minipy::vm& vm, void* handle, const std::string& name, minipy::value& result){


    result = vm.makeDynamicObject(name, nullptr, nullptr, typeMarkerCall);
    return true;
}

// data Wintypes
// data wintypes
// data Wintypes
static minipy::value dataWintypes(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    auto* info = new WintypesInfo();
    info->vm = &vm;
    return vm.makeDynamicObject("wintypes", info, wintypesGet, nullptr);
}




// ctypes Struct 获取
// ctypes struct get
// ctypes Struct ᠬᠠᠯᠠᠬᠤ
bool ctypesStructGet(minipy::vm& vm, void* handle, const std::string& name, minipy::value& result){
    auto* info = static_cast<CtypesStructInfo*>(handle);
    if (!info) return false;

    for (auto& f : info->fields){
        if (f.name == name){

            if (info->buffer.size() < f.offset + f.size) return false;
            const unsigned char* p = info->buffer.data() + f.offset;
            if (f.isPointer || f.size == 8){
                long long val = 0;
                if (sizeof(void*) == 8){
                    val = *reinterpret_cast<const long long*>(p);
                } else {
                    val = *reinterpret_cast<const int*>(p);
                }
                result = vm.makeInt(val);
            } else if (f.size == 4){
                int val = *reinterpret_cast<const int*>(p);
                result = vm.makeInt(val);
            } else if (f.size == 2){
                short val = *reinterpret_cast<const short*>(p);
                result = vm.makeInt(val);
            } else if (f.size == 1){
                unsigned char val = *p;
                result = vm.makeInt(val);
            } else {
                result = vm.makeInt(0);
            }
            return true;
        }
    }
    return false;
}





// compute Struct Layout
// compute struct layout
// compute Struct Layout
static void computeStructLayout(CtypesStructInfo* info){
    size_t cursor = 0;
    size_t maxAlign = 1;
    for (auto& f : info->fields){
        size_t align = f.align > 0 ? f.align : 1;

        size_t misaligned = cursor % align;
        if (misaligned > 0) cursor += (align - misaligned);
        f.offset = cursor;
        cursor += f.size;
        if (align > maxAlign) maxAlign = align;
    }

    size_t tailMisaligned = cursor % maxAlign;
    if (tailMisaligned > 0) cursor += (maxAlign - tailMisaligned);
    info->totalSize = cursor;
    info->align = maxAlign;
    info->buffer.assign(cursor, 0);
}



// fn Structure
// fn structure
// fn Structure
static minipy::value fnStructure(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    auto* info = new CtypesStructInfo();

    info->totalSize = 4096;
    info->align = 8;
    info->buffer.assign(4096, 0);
    info->buffer.reserve(8192);


    minipy::value result = vm.makeDynamicObject("Structure", info, ctypesStructGet, nullptr);

    static_cast<minipy::ffi_instance*>(result.ptr)->typeName = "Structure";
    return result;
}







// ctypes Unicode 缓冲区
// ctypes unicode buffer
// ctypes Unicode ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
struct CtypesUnicodeBuffer{
    std::vector<uint16_t> buffer;
};

// ctypes 字符串 缓冲区
// ctypes string buffer
// ctypes ᠦᠰᠦᠭ ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
struct CtypesStringBuffer{
    std::vector<uint8_t> buffer;
};

// unicode 缓冲区 获取
// unicode buffer get
// unicode ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
static bool unicodeBufferGet(minipy::vm& vm, void* h, const std::string& name, minipy::value& out){
    auto* ub = static_cast<CtypesUnicodeBuffer*>(h);
    if (name == "address" || name == "_buffer"){
        out = vm.makeInt(reinterpret_cast<long long>(ub->buffer.data()));
        return true;
    }
    if (name == "value" || name == "contents"){

        std::wstring w;
        for (uint16_t c : ub->buffer){
            if (c == 0) break;
            w.push_back(static_cast<wchar_t>(c));
        }

        std::string text;
        text.reserve(w.size());
        for (wchar_t wc : w){
            if (wc < 0x80) text.push_back(static_cast<char>(wc));
            else {

                uint32_t cp = static_cast<uint32_t>(wc);
                if (cp < 0x800){
                    text.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else {
                    text.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    text.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
            }
        }
        out = vm.makeString(text);
        return true;
    }
    return false;
}

// 字符串 缓冲区 获取
// string buffer get
// ᠦᠰᠦᠭ ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
static bool stringBufferGet(minipy::vm& vm, void* h, const std::string& name, minipy::value& out){
    auto* sb = static_cast<CtypesStringBuffer*>(h);
    if (name == "address" || name == "_buffer"){
        out = vm.makeInt(reinterpret_cast<long long>(sb->buffer.data()));
        return true;
    }
    if (name == "value" || name == "contents"){
        std::string text;
        for (uint8_t c : sb->buffer){
            if (c == 0) break;
            text.push_back(static_cast<char>(c));
        }
        out = vm.makeString(text);
        return true;
    }
    return false;
}

// fn 创建 Unicode 缓冲区
// fn create unicode buffer
// fn ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Unicode ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
static minipy::value fnCreateUnicodeBuffer(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    size_t count = 0;
    std::string init;
    if (a.argc() >= 1){
        minipy::value first = a.arg(0);
        if (first.type == "int"){
            count = static_cast<size_t>(vm.toInt(first));
        } else if (first.type == "str"){
            init = vm.toString(first);
            count = init.size() + 1;
        }
    }
    if (count == 0) count = 1;
    auto* ub = new CtypesUnicodeBuffer();
    ub->buffer.assign(count, 0);
    if (!init.empty()){
        size_t i = 0;
        for (char c : init){
            if (i < ub->buffer.size() - 1) ub->buffer[i++] = static_cast<uint16_t>(static_cast<unsigned char>(c));
        }
        ub->buffer[i] = 0;
    }
    minipy::value r = vm.makeDynamicObject("UnicodeBuffer", ub, unicodeBufferGet, nullptr);
    return r;
}

// fn 创建 字符串 缓冲区
// fn create string buffer
// fn ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠦᠰᠦᠭ ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
static minipy::value fnCreateStringBuffer(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    size_t count = 0;
    std::string init;
    if (a.argc() >= 1){
        minipy::value first = a.arg(0);
        if (first.type == "int") count = static_cast<size_t>(vm.toInt(first));
        else if (first.type == "str"){ init = vm.toString(first); count = init.size() + 1; }
    }
    if (count == 0) count = 1;
    auto* sb = new CtypesStringBuffer();
    sb->buffer.assign(count, 0);
    if (!init.empty()){
        size_t i = 0;
        for (char c : init){
            if (i < sb->buffer.size() - 1) sb->buffer[i++] = static_cast<uint8_t>(static_cast<unsigned char>(c));
        }
        sb->buffer[i] = 0;
    }
    minipy::value r = vm.makeDynamicObject("StringBuffer", sb, stringBufferGet, nullptr);
    return r;
}


// fn Addressof
// fn addressof
// fn Addressof
static minipy::value fnAddressof(minipy::cffi& a){
    if (a.argc() == 0) return a.none();
    minipy::vm& vm = *a.vm;
    minipy::value arg = a.arg(0);
    if (arg.type == "native_object" && arg.ptr){
        auto* inst = static_cast<minipy::ffi_instance*>(arg.ptr);
        if (inst->handle == nullptr) return a.none();
        if (inst->typeName == "Structure"){
            auto* info = static_cast<CtypesStructInfo*>(inst->handle);
            return vm.makeInt(reinterpret_cast<long long>(info->buffer.data()));
        }
        if (inst->typeName == "UnicodeBuffer"){
            auto* ub = static_cast<CtypesUnicodeBuffer*>(inst->handle);
            return vm.makeInt(reinterpret_cast<long long>(ub->buffer.data()));
        }
        if (inst->typeName == "StringBuffer"){
            auto* sb = static_cast<CtypesStringBuffer*>(inst->handle);
            return vm.makeInt(reinterpret_cast<long long>(sb->buffer.data()));
        }
        return vm.makeInt(reinterpret_cast<long long>(inst->handle));
    }
    if (arg.type == "int") return arg;
    return a.none();
}



// ctypes C Func 类型 Stub
// ctypes c func type stub
// ctypes C Func ᠲᠥᠷᠥᠯ Stub
struct CtypesCFuncTypeStub;
static bool cfuncTypeStubCall(minipy::vm& vm, minipy::ffi_instance& self,
                              const std::vector<minipy::value>& args,
                              const std::map<std::string, minipy::value>& kwargs,
                              minipy::value& result);



// ctypes Callback Context
// ctypes callback context
// ctypes Callback Context
struct CtypesCallbackContext{
    // 回调参数槽, 各平台跳板按自身 ABI 写入, 最多 16 个整数/指针参数
    // Argument slots filled by the platform trampoline according to its ABI, up to 16
    // ᠴᠠᠭᠠᠨ ᠤ ᠵᠠᠪᠰᠠᠷ ᠤᠨ ᠲᠥᠯᠦᠪ
    long long savedArgs[16] = {0};
    // 每个参数的真实宽度种类: ABI 只保证窄于 64 位的槽位低字节有效, 高位可能是垃圾
    // Width kind per argument: the ABI only guarantees low bytes of narrow slots, high bits may be garbage
    // ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠴᠠᠭᠠᠨ ᠤ ᠣᠷᠭᠠᠨ ᠴᠣᠮ
    int argKinds[16] = {0};
    // 每个参数占用的栈字节数, x86-32 的 long long 占 8 字节 (两个栈槽)
    // Stack bytes per argument; long long occupies 8 bytes (two slots) on x86-32
    // ᠴᠠᠭᠠᠨ ᠤ ᠡᠵᠡᠯᠡᠬᠦ ᠪᠠᠢᠲᠤ ᠬᠡᠮᠵᠢᠶ᠎ᠡ
    int argSizes[16] = {0};
    minipy::vm* vm = nullptr;
    minipy::function_t* function = nullptr;
    std::string pythonFuncName;
    void* trampolineCode = nullptr;
    size_t trampolineSize = 0;
    int argCount = 0;
};


// ctypes C Func 类型 Stub
// ctypes c func type stub
// ctypes C Func ᠲᠥᠷᠥᠯ Stub
struct CtypesCFuncTypeStub{
    minipy::vm* vm = nullptr;
    std::vector<std::string> argTypes;
};



// ctypes Callback Dispatcher
// ctypes callback dispatcher
// ctypes Callback Dispatcher
extern "C" long long ctypesCallbackDispatcher(CtypesCallbackContext* ctx){
    if (!ctx || !ctx->vm || !ctx->function) return 0;
    minipy::vm& vm = *ctx->vm;

    minipy::vm::ErrorSnapshot saved = vm.saveErrorState();
    std::vector<minipy::value> args;
    int count = ctx->argCount;
    if (count > 16) count = 16;
    if (count < 0) count = 0;
    for (int i = 0; i < count; ++i)
        args.push_back(vm.makeInt(ctypesNormalizeArg(ctx->savedArgs[i], ctx->argKinds[i])));
    minipy::value result = vm.invoke(ctx->function, args);
    long long ret = 0;
    if (!vm.hasError()) ret = vm.toInt(result);
    else {

        std::fprintf(stderr, "[ctypes.callback %s] exception: %s\n",
                     ctx->pythonFuncName.c_str(), vm.getLastErr().c_str());
        ret = 0;
    }
    vm.restoreErrorState(saved);
    return ret;
}





#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
// 写入一条 4 字节指令
// Emit one 4-byte instruction
// ᠳᠥᠷᠪᠡᠨ ᠪᠠᠢᠲᠤ ᠵᠠᠠᠪᠤᠷ ᠢ ᠪᠢᠴᠢᠬᠦ
static void emitCode32(unsigned char*& p, unsigned int ins){
    std::memcpy(p, &ins, 4);
    p += 4;
}
#endif

#if defined(__x86_64__) || defined(_M_X64)
// 写入 64 位绝对地址立即数
// Emit an absolute 64-bit address immediate
// ᠠᠪᠰᠣᠯᠢᠤᠲ 64 ᠪᠠᠢᠲᠤ ᠬᠠᠶᠢᠭ᠎ᠠ
static void emitImm64(unsigned char*& p, const void* value){
    std::memcpy(p, &value, 8);
    p += 8;
}
#endif

#if defined(_M_IX86) || defined(__i386__)
// 写入 32 位绝对地址立即数
// Emit an absolute 32-bit address immediate
// ᠠᠪᠰᠣᠯᠢᠤᠲ 32 ᠪᠠᠢᠲᠤ ᠬᠠᠶᠢᠭ᠎ᠠ
static void emitImm32(unsigned char*& p, unsigned int value){
    std::memcpy(p, &value, 4);
    p += 4;
}
#endif

#if defined(__arm__) || defined(_M_ARM)
// ARMv7: 把 32 位地址装入寄存器 (MOVW + MOVT)
// ARMv7: load a 32-bit address into a register (MOVW + MOVT)
// ARMv7: 32 ᠪᠠᠢᠲᠤ ᠬᠠᠶᠢᠭ᠎ᠠ ᠢ ᠰᠠᠨᠠᠭᠤᠯᠤᠭ᠎ᠠ ᠳᠤ ᠬᠡᠪᠲᠡᠭᠡᠬᠦ
static void emitArmMovImm(unsigned char*& p, unsigned int rd, const void* value){
    unsigned int v = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(value));
    emitCode32(p, 0xE3000000u | (((v >> 12) & 0xFu) << 16) | (rd << 12) | (v & 0xFFFu));
    unsigned int hi = (v >> 16) & 0xFFFFu;
    emitCode32(p, 0xE3400000u | ((hi >> 12) << 16) | (rd << 12) | (hi & 0xFFFu));
}
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
// AArch64: 把 64 位地址装入寄存器 (MOVZ + 3×MOVK)
// AArch64: load a 64-bit address into a register (MOVZ + 3x MOVK)
// AArch64: 64 ᠪᠠᠢᠲᠤ ᠬᠠᠶᠢᠭ᠎ᠠ ᠢ ᠰᠠᠨᠠᠭᠤᠯᠤᠭ᠎ᠠ ᠳᠤ ᠬᠡᠪᠲᠡᠭᠡᠬᠦ
static void emitA64MovImm(unsigned char*& p, unsigned int rd, const void* value){
    unsigned long long v = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(value));
    emitCode32(p, 0xD2800000u | ((static_cast<unsigned int>(v & 0xFFFFu)) << 5) | rd);
    emitCode32(p, 0xF2800000u | (1u << 21) | ((static_cast<unsigned int>((v >> 16) & 0xFFFFu)) << 5) | rd);
    emitCode32(p, 0xF2800000u | (2u << 21) | ((static_cast<unsigned int>((v >> 32) & 0xFFFFu)) << 5) | rd);
    emitCode32(p, 0xF2800000u | (3u << 21) | ((static_cast<unsigned int>((v >> 48) & 0xFFFFu)) << 5) | rd);
}
#endif

// 构造 Trampoline: 按平台/架构生成符合本机 ABI 的跳板机器码
// Make trampoline: emit ABI-correct machine code for the current platform/arch
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Trampoline
static void* makeTrampoline(CtypesCallbackContext* ctx, const void* dispatcherFunc){
// WASM 无 JIT 可执行内存, 回调降级为不可用
// WASM has no JIT executable memory, callbacks degrade to unavailable
// WASM ᠳᠤ JIT ᠭᠦᠢᠴᠡᠳᠬᠡᠬᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠦᠭᠡᠢ
#if defined(MINIPY_OS_WASM)
    (void)ctx;
    (void)dispatcherFunc;
    return nullptr;
#else
    size_t size = 4096;
    void* code = mpAllocateExecutable(size);
    if (!code) return nullptr;
    unsigned char* p = static_cast<unsigned char*>(code);

#if (defined(__x86_64__) || defined(_M_X64)) && defined(MINIPY_OS_WINDOWS)
    // Windows x64 (Microsoft ABI): rcx/rdx/r8/r9, 32 字节 shadow space, rax 返回
    // Windows x64 (Microsoft ABI): rcx/rdx/r8/r9, 32-byte shadow space, return in rax
    // Windows x64 (Microsoft ABI): rcx/rdx/r8/r9, 32 ᠪᠠᠢᠲᠤ shadow space
    *p++ = 0x49; *p++ = 0xBA;
    emitImm64(p, ctx);
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x4A; *p++ = 0x00;
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x52; *p++ = 0x08;
    *p++ = 0x4D; *p++ = 0x89; *p++ = 0x42; *p++ = 0x10;
    *p++ = 0x4D; *p++ = 0x89; *p++ = 0x4A; *p++ = 0x18;
    for (int i = 0; i < 8; ++i){
        *p++ = 0x48; *p++ = 0x8B; *p++ = 0x44; *p++ = 0x24;
        *p++ = static_cast<unsigned char>(40 + i * 8);
        *p++ = 0x49; *p++ = 0x89; *p++ = 0x42;
        *p++ = static_cast<unsigned char>(32 + i * 8);
    }
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28;
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0xD1;
    *p++ = 0x48; *p++ = 0xB8;
    emitImm64(p, dispatcherFunc);
    *p++ = 0xFF; *p++ = 0xD0;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
    *p++ = 0xC3;
#elif defined(__x86_64__) || defined(_M_X64)
    // SysV x64 (Linux/macOS/Android/HarmonyOS): rdi/rsi/rdx/rcx/r8/r9, 无 shadow space
    // SysV x64 (Linux/macOS/Android/HarmonyOS): rdi/rsi/rdx/rcx/r8/r9, no shadow space
    // SysV x64: rdi/rsi/rdx/rcx/r8/r9
    *p++ = 0x49; *p++ = 0xBA;
    emitImm64(p, ctx);
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x7A; *p++ = 0x00;
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x72; *p++ = 0x08;
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x52; *p++ = 0x10;
    *p++ = 0x49; *p++ = 0x89; *p++ = 0x4A; *p++ = 0x18;
    *p++ = 0x4D; *p++ = 0x89; *p++ = 0x42; *p++ = 0x20;
    *p++ = 0x4D; *p++ = 0x89; *p++ = 0x4A; *p++ = 0x28;
    for (int i = 0; i < 8; ++i){
        *p++ = 0x48; *p++ = 0x8B; *p++ = 0x44; *p++ = 0x24;
        *p++ = static_cast<unsigned char>(8 + i * 8);
        *p++ = 0x49; *p++ = 0x89; *p++ = 0x42;
        *p++ = static_cast<unsigned char>(48 + i * 8);
    }
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x08;
    *p++ = 0x4C; *p++ = 0x89; *p++ = 0xD7;
    *p++ = 0x48; *p++ = 0xB8;
    emitImm64(p, dispatcherFunc);
    *p++ = 0xFF; *p++ = 0xD0;
    *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x08;
    *p++ = 0xC3;
#elif defined(__aarch64__) || defined(_M_ARM64)
    // AArch64 (iOS/macOS/Android/HarmonyOS/Linux/Windows ARM64): x0-x7, 栈参数, x0 返回
    // AArch64 (iOS/macOS/Android/HarmonyOS/Linux/Windows ARM64): x0-x7, stack args, return in x0
    // AArch64: x0-x7, ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠴᠠᠭᠠᠨ
    for (int i = 0; i < 8; ++i){
        emitCode32(p, 0xF9000000u | (static_cast<unsigned int>(i) << 10) |
                          (9u << 5) | static_cast<unsigned int>(i));
    }
    emitCode32(p, 0xAA0903E0u);
    for (int i = 0; i < 8; ++i){
        emitCode32(p, 0xF9400000u | (static_cast<unsigned int>(i) << 10) | (31u << 5) | 10u);
        emitCode32(p, 0xF9000000u | (static_cast<unsigned int>(8 + i) << 10) | (9u << 5) | 10u);
    }
    emitA64MovImm(p, 16u, dispatcherFunc);
    emitCode32(p, 0xD63F0200u);
    emitCode32(p, 0xD65F03C0u);
#elif defined(__arm__) || defined(_M_ARM)
    // ARMv7 A32 (Android/iOS 32 位): r0-r3 + 栈参数, r0 返回
    // ARMv7 A32 (Android/iOS 32-bit): r0-r3 plus stack args, return in r0
    // ARMv7 A32: r0-r3, ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨ ᠴᠠᠭᠠᠨ
    emitArmMovImm(p, 12, ctx);
    for (int i = 0; i < 4; ++i){
        emitCode32(p, 0xE58C0000u | (static_cast<unsigned int>(i) << 12) |
                          static_cast<unsigned int>(i * 8));
    }
    // AAPCS: 第 5 个起的参数紧跟 sp 起始, 偏移从 0 开始
    // AAPCS: the 5th argument onward starts right at sp, offset begins at 0
    // AAPCS: ᠲᠠᠪᠤᠳᠤᠭᠠᠷ ᠡᠴᠡ ᠡᠬᠢᠯᠡᠵᠦ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠳᠤ
    for (int i = 4; i < 12; ++i){
        emitCode32(p, 0xE59D0000u | static_cast<unsigned int>((i - 4) * 4));
        emitCode32(p, 0xE58C0000u | static_cast<unsigned int>(i * 8));
    }
    emitCode32(p, 0xE92D4010u);
    emitArmMovImm(p, 0, dispatcherFunc);
    emitCode32(p, 0xE12FFF30u);
    emitCode32(p, 0xE8BD8010u);
#elif defined(_M_IX86) || defined(__i386__)
    // x86 32 位 (Windows/Linux): 全部参数在栈上, eax 返回
    // x86 32-bit (Windows/Linux): all arguments on the stack, return in eax
    // x86 32 ᠪᠠᠢᠲᠤ: ᠪᠦᠬᠦᠨ ᠴᠠᠭᠠᠨ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠳᠤ
    *p++ = 0xB8;
    emitImm32(p, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(ctx)));
    // 读栈槽低 32 位到 edx: disp8 / disp32 两种寻址
    // Load the low 32 bits of a stack slot into edx: disp8 or disp32 addressing
    // ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠡᠴᠡ 32 ᠪᠠᠢᠲᠤ ᠢ ᠳᠠᠷᠤᠬᠤ
    auto emitLoadStack32 = [&](int disp){
        if (disp <= 127){
            *p++ = 0x8B; *p++ = 0x54; *p++ = 0x24;
            *p++ = static_cast<unsigned char>(disp);
        } else {
            *p++ = 0x8B; *p++ = 0x94; *p++ = 0x24;
            emitImm32(p, static_cast<unsigned int>(disp));
        }
    };
    int stackOffset = 4;   // 返回地址之后就是第一个参数
    int argCount = ctx->argCount;
    if (argCount > 16) argCount = 16;
    if (argCount < 0) argCount = 0;
    for (int i = 0; i < argCount; ++i){
        int size = ctx->argSizes[i];
        if (size != 8) size = 4;
        // 参数在栈上按声明顺序紧密排列, 8 字节参数占连续两个 4 字节槽
        // Arguments are packed in declaration order; an 8-byte argument spans two 4-byte slots
        // ᠴᠠᠭᠠᠨ ᠪᠡᠷ ᠲᠡᠭᠰᠢ ᠪᠠᠢᠷᠠᠯᠠᠬᠤ
        emitLoadStack32(stackOffset);
        *p++ = 0x89; *p++ = 0x50;
        *p++ = static_cast<unsigned char>(i * 8);
        if (size == 8){
            emitLoadStack32(stackOffset + 4);
            *p++ = 0x89; *p++ = 0x50;
            *p++ = static_cast<unsigned char>(i * 8 + 4);
        }
        stackOffset += size;
    }
    // 先留 8 字节保证调用点 16 字节对齐, 再压 ctx: ctx 必须停在 [esp+4]
    // Reserve 8 bytes for 16-byte call alignment first, then push ctx so it lands at [esp+4]
    // ᠡᠬᠢᠯᠡᠳ 8 ᠪᠠᠢᠲᠤ ᠰᠠᠨᠠᠭᠤᠯᠭ᠎ᠠ ᠳᠤ ᠪᠠᠢᠭᠤᠯᠠᠭ᠎ᠠ
    *p++ = 0x83; *p++ = 0xEC; *p++ = 0x08;
    *p++ = 0x50;
    *p++ = 0xB8;
    emitImm32(p, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(dispatcherFunc)));
    *p++ = 0xFF; *p++ = 0xD0;
    *p++ = 0x83; *p++ = 0xC4; *p++ = 0x0C;
    *p++ = 0xC3;
#else
    // 未知架构: 明确降级为不可用
    // Unknown architecture: explicitly degrade to unavailable
    // ᠮᠡᠳᠡᠭᠡᠭᠳᠡᠭᠰᠡᠨ ᠦᠭᠡᠢ ᠠᠷᠬᠢᠲᠡᠬᠲᠦᠷ
    mpFreeExecutable(code, size);
    return nullptr;
#endif

    ctx->trampolineCode = code;
    ctx->trampolineSize = size;
    mpFlushInstructionCache(code, size);
    return code;
#endif
}


// fn C F U N C T Y P E
// fn c f u n c t y p e
// fn C F U N C T Y P E
static minipy::value fnCFUNCTYPE(minipy::cffi& a){
    minipy::vm& vm = *a.vm;
    auto* info = new CtypesCFuncTypeStub();
    info->vm = &vm;
    for (size_t i = 0; i < a.argc(); ++i){
        minipy::value arg = a.arg(i);
        if (arg.type == "native_object" && arg.ptr){
            info->argTypes.push_back(static_cast<minipy::ffi_instance*>(arg.ptr)->typeName);
        } else {
            info->argTypes.push_back("int");
        }
    }
    minipy::value result = vm.makeDynamicObject("CFuncType", info, nullptr, cfuncTypeStubCall);
    return result;
}


static bool cfuncTypeStubCall(minipy::vm& vm, minipy::ffi_instance& self,
                              const std::vector<minipy::value>& args,
                              const std::map<std::string, minipy::value>& kwargs,
                              minipy::value& result){
    auto* stub = static_cast<CtypesCFuncTypeStub*>(self.handle);
    if (!stub){ result = vm.makeNone(); return true; }
    if (args.empty()){
        vm.raiseError("TypeError", "CFuncType() missing required argument");
        result = vm.makeNone();
        return true;
    }
    const minipy::value& funcVal = args[0];
    if (funcVal.type != "function" || !funcVal.ptr){
        vm.raiseError("TypeError", "CFuncType() argument must be a callable");
        result = vm.makeNone();
        return true;
    }
    auto* func = static_cast<minipy::function_t*>(funcVal.ptr);
    auto* ctx = new CtypesCallbackContext();
    ctx->vm = &vm;
    ctx->function = func;
    ctx->pythonFuncName = func->name;
    ctx->argCount = static_cast<int>(stub->argTypes.size()) - 1;
    // 记录每个参数的宽度, 供调度器按类型截断/扩展栈槽中的高位垃圾
    // Record each parameter's width so the dispatcher can trim garbage high bits of stack slots
    // ᠲᠤᠰ ᠪᠤᠷᠢ ᠴᠠᠭᠠᠨ ᠤ ᠣᠷᠭᠠᠨ ᠢ ᠲᠣᠭᠲᠠᠭᠠᠬᠤ
    for (int i = 0; i < ctx->argCount && i < 16; ++i){
        const std::string& typeName = stub->argTypes[static_cast<size_t>(i) + 1];
        ctx->argKinds[i] = ctypesArgKind(typeName);
        ctx->argSizes[i] = ctypesArgSize(typeName);
    }
    void* dispatcherAddr = reinterpret_cast<void*>(ctypesCallbackDispatcher);
    void* tramp = makeTrampoline(ctx, dispatcherAddr);
    if (!tramp){
        vm.raiseError("RuntimeError", "Failed to allocate trampoline memory");
        result = vm.makeNone();
        return true;
    }
    auto* cbWrap = new CtypesCBWrap();
    cbWrap->context = ctx;
    cbWrap->funcName = func->name;
    cbWrap->trampoline = tramp;
    result = vm.makeDynamicObject("Callback", cbWrap, nullptr, nullptr);

    static_cast<minipy::ffi_instance*>(result.ptr)->typeName = "Callback";
    return true;
}

// cffi ctypes lib
// cffi ctypes lib
// cffi ctypes lib
minipy::ffilib cffi_ctypes_lib(){

    minipy::ffi_class structureClass;
    structureClass.name = "Structure";
    structureClass.constructor = (void*)fnStructure;
    return minipy::cffi_lib(
        "ctypes",
        {
            {"WinDLL", (void*)fnWinDLL},
            {"CDLL", (void*)fnCDLL},
            {"LoadLibrary", (void*)fnLoadLibrary},
            {"Structure", (void*)fnStructure},
            {"CFUNCTYPE", (void*)fnCFUNCTYPE},
            {"c_int", (void*)fnCInteger},
            {"c_uint", (void*)fnCInteger},
            {"c_long", (void*)fnCInteger},
            {"c_ulong", (void*)fnCInteger},
            {"c_longlong", (void*)fnCInteger},
            {"c_ulonglong", (void*)fnCInteger},
            {"c_int64", (void*)fnCInteger},
            {"c_uint64", (void*)fnCInteger},
            {"c_ssize_t", (void*)fnCInteger},
            {"c_size_t", (void*)fnCInteger},
            {"c_short", (void*)fnCInteger},
            {"c_ushort", (void*)fnCInteger},
            {"c_byte", (void*)fnCInteger},
            {"c_ubyte", (void*)fnCInteger},
            {"c_char", (void*)fnCInteger},
            {"c_wchar", (void*)fnCInteger},
            {"c_float", (void*)fnCFloat},
            {"c_double", (void*)fnCFloat},
            {"c_bool", (void*)fnCBool},
            {"c_char_p", (void*)fnCPointer},
            {"c_wchar_p", (void*)fnCPointer},
            {"c_void_p", (void*)fnCPointer},
            {"byref", (void*)fnByref},
            {"addressof", (void*)fnAddressof},
            {"create_unicode_buffer", (void*)fnCreateUnicodeBuffer},
            {"create_string_buffer", (void*)fnCreateStringBuffer},
            {"get_last_error", (void*)fnGetLastError},
            {"set_last_error", (void*)fnSetLastError}
        },
        {
            {"windll", (void*)dataWindll},
            {"cdll", (void*)dataCdll},
            {"oledll", (void*)dataOledll},
            {"wintypes", (void*)dataWintypes},
            {"c_double", (void*)dataCDouble},
            {"c_int", (void*)dataCInt},
            {"c_void_p", (void*)dataCVoidP},
            {"c_char_p", (void*)dataCCharP},
            {"c_wchar_p", (void*)dataCWcharP},
            {"c_uint", (void*)dataCUInt},
            {"c_long", (void*)dataCLong},
            {"c_ulong", (void*)dataCULong},
            {"c_longlong", (void*)dataCLongLong},
            {"c_ulonglong", (void*)dataCULongLong},
            {"c_int64", (void*)dataCInt64},
            {"c_uint64", (void*)dataCUInt64},
            {"c_ssize_t", (void*)dataCSsize},
            {"c_size_t", (void*)dataCSize},
            {"c_short", (void*)dataCShort},
            {"c_ushort", (void*)dataCUShort},
            {"c_byte", (void*)dataCByte},
            {"c_ubyte", (void*)dataCUByte},
            {"c_char", (void*)dataCChar},
            {"c_wchar", (void*)dataCWchar},
            {"c_bool", (void*)dataCBool}
        },
        {
            {"Structure", structureClass}
        });
}


}
