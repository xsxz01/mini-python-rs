#include "../cffi.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <map>
#include <string>
#include <vector>

namespace minipy {











bool ctypesCall(minipy::vm& vm, minipy::ffi_instance& self, const std::vector<minipy::value>& args,
                const std::map<std::string, minipy::value>& kwargs, minipy::value& result);
bool ctypesGet(minipy::vm& vm, void* handle, const std::string& name, minipy::value& result);

std::map<std::string, HMODULE>& moduleCache(){
    static std::map<std::string, HMODULE> cache;
    return cache;
}

// ends With Char
// ends with char
// ends With Char
bool endsWithChar(const std::string& text, char c){
    return !text.empty() && text.back() == c;
}


// utf8 转 Wide
// utf8 to wide
// utf8 ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Wide
std::wstring utf8ToWide(const std::string& text){
    std::wstring result;
    size_t i = 0;
    while (i < text.size()){
        unsigned char c = static_cast<unsigned char>(text[i]);
        long long code = 0;
        size_t extra = 0;
        if (c < 0x80){ code = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0){ code = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0){ code = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0){ code = c & 0x07; extra = 3; }
        else { i++; continue; }
        bool valid = (i + extra < text.size());
        for (size_t k = 1; valid && k <= extra; ++k){
            unsigned char next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0) != 0x80) valid = false;
            else code = (code << 6) | (next & 0x3F);
        }
        i += extra + 1;
        if (!valid) continue;
        if (code >= 0x10000){
            long long offset = code - 0x10000;
            result.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
            result.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));
        } else if (code > 0){
            result.push_back(static_cast<wchar_t>(code));
        }
    }
    return result;
}

// 加载 模块
// load module
// ᠠᠶᠢᠯᠳᠠᠬᠤ ᠦᠪᠲ᠋ᠠᠢ
HMODULE loadModule(const std::string& name, std::string& error){
    auto it = moduleCache().find(name);
    if (it != moduleCache().end()) return it->second;
    std::string full = name;
    if (full.find('.') == std::string::npos) full += ".dll";
    HMODULE module = LoadLibraryW(utf8ToWide(full).c_str());
    if (module == nullptr){
        error = "Could not find module '" + name + "'";
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
        HMODULE module = loadModule(info->dllName, error);
        if (module == nullptr){
            vm.raiseError("OSError", error);
            return true;
        }
        void* proc = reinterpret_cast<void*>(GetProcAddress(module, name.c_str()));
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
    auto restypeIt = self.attributes.find("restype");
    if (restypeIt != self.attributes.end() &&
        restypeIt->second.type == "native_object" && restypeIt->second.ptr != nullptr){
        auto* marker = static_cast<minipy::ffi_instance*>(restypeIt->second.ptr);
        if (marker->typeName == "c_double") returnsDouble = true;
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
            raw[i] = static_cast<minipy::ffi_instance*>(arg.ptr)->handle;
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
    return a.mkint(static_cast<long long>(GetLastError()));
}

// fn 集合 Last 错误
// fn set last error
// fn ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ Last ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
static minipy::value fnSetLastError(minipy::cffi& a){
    SetLastError(static_cast<DWORD>(a.argc() > 0 ? a.arg_int(0) : 0));
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
    long long savedArgs[4] = {0, 0, 0, 0};
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
    if (count > 4) count = 4;
    for (int i = 0; i < count; ++i) args.push_back(vm.makeInt(ctx->savedArgs[i]));
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





// 构造 Trampoline
// make trampoline
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Trampoline
static void* makeTrampoline(CtypesCallbackContext* ctx, const void* dispatcherFunc){
    SIZE_T size = 4096;
    void* code = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!code) return nullptr;
    unsigned char* p = static_cast<unsigned char*>(code);

    p[0] = 0x48; p[1] = 0xB8;
    memcpy(p + 2, &ctx, 8);
    p += 10;

    p[0] = 0x48; p[1] = 0x89; p[2] = 0x08;
    p += 3;

    p[0] = 0x48; p[1] = 0x89; p[2] = 0x50; p[3] = 0x08;
    p += 4;

    p[0] = 0x4C; p[1] = 0x89; p[2] = 0x40; p[3] = 0x10;
    p += 4;

    p[0] = 0x4C; p[1] = 0x89; p[2] = 0x48; p[3] = 0x18;
    p += 4;

    p[0] = 0x48; p[1] = 0x83; p[2] = 0xEC; p[3] = 0x08;
    p += 4;

    p[0] = 0x48; p[1] = 0x89; p[2] = 0xC1;
    p += 3;

    p[0] = 0x48; p[1] = 0xB8;
    memcpy(p + 2, &dispatcherFunc, 8);
    p += 10;

    p[0] = 0xFF; p[1] = 0xD0;
    p += 2;

    p[0] = 0x48; p[1] = 0x83; p[2] = 0xC4; p[3] = 0x08;
    p += 4;

    p[0] = 0xC3;
    ctx->trampolineCode = code;
    ctx->trampolineSize = 4096;
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
    return code;
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
            {"c_wchar_p", (void*)dataCWcharP}
        },
        {
            {"Structure", structureClass}
        });
}


}
