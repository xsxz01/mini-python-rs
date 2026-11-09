#include "../cffi.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// Profile 内部状态
// Profile internal state
// Profile ᠦᠨ ᠳᠣᠲᠤᠭᠠᠳᠤ ᠪᠠᠶᠢᠳᠠᠯ
struct ProfileState{
    double totalTime = 0.0;
    long long callCount = 0;
    bool running = false;
};

ProfileState* stateOf(const value& self){
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<ProfileState*>(inst->handle);
}

// 打印统计表 (格式仿 CPython)
// Print the stats table (CPython-style)
// ᠲᠣᠬᠢᠶ᠎ᠠ ᠶᠢᠨ ᠬᠤᠪᠢᠶ᠎ᠠ ᠢ ᠬᠡᠪᠯᠡᠬᠦ
void printStats(cffi& a, ProfileState* st){
    char buf[256];
    std::snprintf(buf, sizeof(buf), "         %lld function calls in %.3f seconds\n",
                  st->callCount > 0 ? st->callCount : 1, st->totalTime);
    std::cout << "\n" << buf << "\n";
    std::cout << "   Ordered by: standard name\n\n";
    std::cout << "   ncalls  tottime  percall  cumtime  percall filename:lineno(function)\n";
    char line[256];
    std::snprintf(line, sizeof(line), "%8lld %8.3f %8.3f %8.3f %8.3f %s\n",
                  st->callCount > 0 ? st->callCount : 1,
                  st->totalTime, st->totalTime,
                  st->totalTime, st->totalTime, "<string>:1(<module>)");
    std::cout << line << "\n";
    std::cout.flush();
}

// Profile 构造器
// Profile constructor
// Profile ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value profileCtor(cffi& a){
    return a.mkobject(new ProfileState());
}

// Profile.enable
// Profile.enable
// Profile.enable
value profileEnable(cffi& a){
    stateOf(a.self())->running = true;
    return a.none();
}

// Profile.disable
// Profile.disable
// Profile.disable
value profileDisable(cffi& a){
    stateOf(a.self())->running = false;
    return a.none();
}

// Profile.run(stmt)
// Profile.run(stmt)
// Profile.run(stmt)
value profileRun(cffi& a){
    ProfileState* st = stateOf(a.self());
    std::string stmt = a.arg_str(0);
    auto start = std::chrono::steady_clock::now();
    a.vm->execSource(stmt);
    auto end = std::chrono::steady_clock::now();
    st->totalTime += std::chrono::duration<double>(end - start).count();
    st->callCount += 1;
    return a.self();
}

// Profile.runctx(stmt, globals, locals)
// Profile.runctx(stmt, globals, locals)
// Profile.runctx(stmt, globals, locals)
value profileRunctx(cffi& a){
    return profileRun(a);
}

// Profile.runcall(func, *args)
// Profile.runcall(func, *args)
// Profile.runcall(func, *args)
value profileRuncall(cffi& a){
    ProfileState* st = stateOf(a.self());
    std::vector<value> innerArgs;
    for (size_t i = 1; i < a.argc(); ++i) innerArgs.push_back(a.arg(i));
    auto start = std::chrono::steady_clock::now();
    value result = a.vm->callValue(a.arg(0), innerArgs);
    auto end = std::chrono::steady_clock::now();
    st->totalTime += std::chrono::duration<double>(end - start).count();
    st->callCount += 1;
    return result;
}

// Profile.create_stats
// Profile.create_stats
// Profile.create_stats
value profileCreateStats(cffi& a){
    return a.none();
}

// Profile.snapshot_stats
// Profile.snapshot_stats
// Profile.snapshot_stats
value profileSnapshotStats(cffi& a){
    return a.none();
}

// Profile.print_stats
// Profile.print_stats
// Profile.print_stats
value profilePrintStats(cffi& a){
    printStats(a, stateOf(a.self()));
    return a.none();
}

// Profile.dump_stats(filename)
// Profile.dump_stats(filename)
// Profile.dump_stats(filename)
value profileDumpStats(cffi& a){
    ProfileState* st = stateOf(a.self());
    std::string filename = a.arg_str(0);
    std::ofstream out(filename, std::ios::binary);
    if (out){
        out << "MiniPython profile stats\n";
        out << "calls " << st->callCount << "\n";
        out << "time " << st->totalTime << "\n";
    }
    return a.none();
}

// fn run(statement, filename=None)
// fn run(statement, filename=None)
// fn run(statement, filename=None)
value fnRun(cffi& a){
    std::string stmt = a.arg_str(0);
    ProfileState st;
    auto start = std::chrono::steady_clock::now();
    a.vm->execSource(stmt);
    auto end = std::chrono::steady_clock::now();
    st.totalTime = std::chrono::duration<double>(end - start).count();
    st.callCount = 1;
    if (a.argc() > 1 && a.arg(1).type != "none"){
        std::string filename = a.arg_str(1);
        std::ofstream out(filename, std::ios::binary);
        if (out){
            out << "MiniPython profile stats\n";
            out << "calls " << st.callCount << "\n";
            out << "time " << st.totalTime << "\n";
        }
    } else {
        printStats(a, &st);
    }
    return a.none();
}

// fn runctx(statement, globals, locals, filename=None)
// fn runctx(statement, globals, locals, filename=None)
// fn runctx(statement, globals, locals, filename=None)
value fnRunctx(cffi& a){
    std::string stmt = a.arg_str(0);
    std::string filename;
    if (a.has_kwarg("filename")) filename = a.kwarg_str("filename", "");
    ProfileState st;
    auto start = std::chrono::steady_clock::now();
    a.vm->execSource(stmt);
    auto end = std::chrono::steady_clock::now();
    st.totalTime = std::chrono::duration<double>(end - start).count();
    st.callCount = 1;
    if (!filename.empty()){
        std::ofstream out(filename, std::ios::binary);
        if (out){
            out << "MiniPython profile stats\n";
            out << "calls 1\n";
            out << "time " << st.totalTime << "\n";
        }
    } else {
        printStats(a, &st);
    }
    return a.none();
}

} // namespace

// cffi profile lib
// cffi profile lib
// cffi profile lib
ffilib cffi_profile_lib(){
    ffi_class profile = cffi_class(
        "Profile", (void*)profileCtor,
        {
            {"enable", (void*)profileEnable},
            {"disable", (void*)profileDisable},
            {"run", (void*)profileRun},
            {"runctx", (void*)profileRunctx},
            {"runcall", (void*)profileRuncall},
            {"create_stats", (void*)profileCreateStats},
            {"snapshot_stats", (void*)profileSnapshotStats},
            {"print_stats", (void*)profilePrintStats},
            {"dump_stats", (void*)profileDumpStats}
        });

    return cffi_lib(
        "profile",
        {
            {"run", (void*)fnRun},
            {"runctx", (void*)fnRunctx}
        },
        {},
        {
            {"Profile", profile}
        });
}

} // namespace minipy
