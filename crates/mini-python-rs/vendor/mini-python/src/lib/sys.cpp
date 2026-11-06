#include "../cffi.hpp"

#include <cstdlib>
#include <vector>

namespace minipy {




using namespace minipy;

// data Version
// data version
// data Version
static value dataVersion(cffi& a){ return a.mkstr("1.0.1 MiniPython (ᠮᠢᠨᠢ ᠫᠢᠲ᠋ᠢᠣᠨ Portable Python<3.14.0>)"); }

// data Version Info
// data version info
// data Version Info
static value dataVersionInfo(cffi& a){
    return a.mklist({a.mkint(3), a.mkint(14), a.mkint(0), a.mkstr("final"), a.mkint(0)});
}

// data Platform
// data platform
// data Platform
static value dataPlatform(cffi& a){
#if defined(_WIN32)
    return a.mkstr("win32");
#elif defined(__APPLE__)
    return a.mkstr("darwin");
#else
    return a.mkstr("linux");
#endif
}

// data Maxsize
// data maxsize
// data Maxsize
static value dataMaxsize(cffi& a){ return a.mkint(9223372036854775807LL); }

// data Byteorder
// data byteorder
// data Byteorder
static value dataByteorder(cffi& a){ return a.mkstr("little"); }

// data Argv
// data argv
// data Argv
static value dataArgv(cffi& a){ return a.mklist({a.mkstr("test.py")}); }

// data Path
// data path
// data Path
static value dataPath(cffi& a){ return a.mklist({a.mkstr(".")}); }

// data Executable
// data executable
// data Executable
static value dataExecutable(cffi& a){ return a.mkstr("MiniPython Copyright (c) 2027, RySoftware ᠷᠢᠤ ᠶᠦᠨ ᠰᠢᠷᠡᠭᠡᠨ\nAll Rights Reserved."); }

// data Copyright
// data copyright
// data Copyright
static value dataCopyright(cffi& a){ return a.mkstr("MiniPython Copyright (c) 2027, RySoftware ᠷᠢᠤ ᠶᠦᠨ ᠰᠢᠷᠡᠭᠡᠨ\nAll Rights Reserved."); }

// data Hexversion
// data hexversion
// data Hexversion
static value dataHexversion(cffi& a){ return a.mkint(0x030E00F0); }

namespace {

long long recursionLimit = 1000;
}

// fn Getrecursionlimit
// fn getrecursionlimit
// fn Getrecursionlimit
static value fnGetrecursionlimit(cffi& a){
    return a.mkint(recursionLimit);
}

// fn Setrecursionlimit
// fn setrecursionlimit
// fn Setrecursionlimit
static value fnSetrecursionlimit(cffi& a){
    long long value = a.arg_int(0);
    if (value <= 0){
        a.vm->raiseError("ValueError", "recursion limit must be positive");
        return a.none();
    }
    recursionLimit = value;
    return a.none();
}

// fn Getdefaultencoding
// fn getdefaultencoding
// fn Getdefaultencoding
static value fnGetdefaultencoding(cffi& a){
    return a.mkstr("utf-8");
}

// fn Intern
// fn intern
// fn Intern
static value fnIntern(cffi& a){
    value value = a.arg(0);
    if (value.type != "str"){
        a.vm->raiseError("TypeError", "intern() argument must be str, not " +
                                          a.vm->pythonTypeName(value));
        return a.none();
    }
    return value;
}

// fn Exit
// fn exit
// fn Exit
static value fnExit(cffi& a){
    int code = (a.argc() > 0) ? static_cast<int>(a.arg_int(0)) : 0;
    std::exit(code);
    return a.none();
}

// fn Getsizeof
// fn getsizeof
// fn Getsizeof
static value fnGetsizeof(cffi& a){
    value value = a.arg(0);
    if (value.type == "str"){
        return a.mkint(49 + static_cast<long long>(a.vm->toString(value).size()));
    }
    if (value.type == "list"){
        return a.mkint(56 + 8 * static_cast<long long>(
                                   static_cast<std::vector<minipy::value>*>(value.ptr)->size()));
    }
    if (value.type == "float") return a.mkint(24);
    return a.mkint(28);
}

// cffi sys lib
// cffi sys lib
// cffi sys lib
ffilib cffi_sys_lib(){
    return cffi_lib(
        "sys",
        {
            {"exit", (void*)fnExit},
            {"getsizeof", (void*)fnGetsizeof},
            {"getrecursionlimit", (void*)fnGetrecursionlimit},
            {"setrecursionlimit", (void*)fnSetrecursionlimit},
            {"getdefaultencoding", (void*)fnGetdefaultencoding},
            {"intern", (void*)fnIntern}
        },
        {
            {"version", (void*)dataVersion},
            {"version_info", (void*)dataVersionInfo},
            {"platform", (void*)dataPlatform},
            {"maxsize", (void*)dataMaxsize},
            {"argv", (void*)dataArgv},
            {"path", (void*)dataPath},
            {"executable", (void*)dataExecutable},
            {"byteorder", (void*)dataByteorder},
            {"copyright", (void*)dataCopyright},
            {"hexversion", (void*)dataHexversion}
        });
}


}
