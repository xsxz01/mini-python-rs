#include "../cffi.hpp"
#include "../platform.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace minipy {

namespace {

namespace fs = std::filesystem;

// UTF-8 <-> filesystem path
std::string u8Text(const fs::path& p){
    std::u8string u8 = p.generic_u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

fs::path fsPathFromUtf8(const std::string& text){
    std::u8string u8;
    u8.assign(reinterpret_cast<const char8_t*>(text.data()), text.size());
    return fs::path(u8);
}

std::atomic<unsigned long long> g_counter{0};

std::string randomToken(){
    static std::mt19937_64 rng(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::string out;
    out.push_back(static_cast<char>('a' + static_cast<int>(rng() % 26)));
    for (int i = 0; i < 7; ++i)
        out.push_back(static_cast<char>('0' + static_cast<int>(rng() % 10)));
    out += std::to_string(g_counter.fetch_add(1));
    return out;
}

std::string defaultDir(){
    const char* names[] = {"TMPDIR", "TEMP", "TMP"};
    for (const char* name : names){
        const char* v = std::getenv(name);
        if (v && v[0]) return v;
    }
    std::string dir = mpTempDirectory();
    if (!dir.empty()) return dir;
    return ".";
}

std::string joinPath(const std::string& dir, const std::string& name){
    if (dir.empty()) return name;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + name;
    return dir + std::string(1, mpPathSeparator()) + name;
}

std::string optString(cffi& a, const char* kw, const std::string& def){
    if (a.has_kwarg(kw) && a.kwarg(kw).type == "str")
        return a.vm->toString(a.kwarg(kw));
    return def;
}

bool optBool(cffi& a, const char* kw, bool def){
    if (a.has_kwarg(kw) && a.kwarg(kw).type != "none")
        return a.vm->truthy(a.kwarg(kw));
    return def;
}

std::string tempName(const std::string& suffix, const std::string& prefix,
                     const std::string& dir){
    return joinPath(dir, prefix + randomToken() + suffix);
}

int createExclusive(const std::string& path){
#if defined(_WIN32)
    return _open(path.c_str(), _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                 _S_IREAD | _S_IWRITE);
#else
    return open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
#endif
}

void closeFd(int fd){
#if defined(_WIN32)
    _close(fd);
#else
    ::close(fd);
#endif
}

// mkstemp
static value fnMkstemp(cffi& a){
    std::string suffix = optString(a, "suffix", "");
    std::string prefix = optString(a, "prefix", "tmp");
    std::string dir = optString(a, "dir", "");
    if (dir.empty()) dir = defaultDir();
    std::error_code ec;
    fs::create_directories(fsPathFromUtf8(dir), ec);
    for (int attempt = 0; attempt < 100; ++attempt){
        std::string name = tempName(suffix, prefix, dir);
        int fd = createExclusive(name);
        if (fd >= 0){
            std::vector<value> pair;
            pair.push_back(a.mkint(fd));
            pair.push_back(a.mkstr(name));
            return a.vm->makeTuple(pair);
        }
    }
    a.vm->raiseError("FileExistsError", "No usable temporary file name found");
    return a.none();
}

// mkdtemp
static value fnMkdtemp(cffi& a){
    std::string suffix = optString(a, "suffix", "");
    std::string prefix = optString(a, "prefix", "tmp");
    std::string dir = optString(a, "dir", "");
    if (dir.empty()) dir = defaultDir();
    std::error_code ec;
    fs::create_directories(fsPathFromUtf8(dir), ec);
    for (int attempt = 0; attempt < 100; ++attempt){
        std::string name = tempName(suffix, prefix, dir);
        if (fs::create_directory(fsPathFromUtf8(name), ec) && !ec)
            return a.mkstr(name);
        ec.clear();
    }
    a.vm->raiseError("FileExistsError", "No usable temporary directory name found");
    return a.none();
}

// mktemp (deprecated): 只生成名字, 不创建文件
static value fnMktemp(cffi& a){
    std::string suffix = optString(a, "suffix", "");
    std::string prefix = optString(a, "prefix", "tmp");
    std::string dir = optString(a, "dir", "");
    if (dir.empty()) dir = defaultDir();
    for (int attempt = 0; attempt < 100; ++attempt){
        std::string name = tempName(suffix, prefix, dir);
        if (!fs::exists(fsPathFromUtf8(name))) return a.mkstr(name);
    }
    return a.mkstr(tempName(suffix, prefix, dir));
}

// gettempdir / gettempdirb
static value fnGettempdir(cffi& a){ return a.mkstr(defaultDir()); }

static value fnGettempdirb(cffi& a){
    value v = a.mkstr(defaultDir());
    v.type = "bytes";
    return v;
}

// ============================================================
// NamedTemporaryFile / TemporaryFile / SpooledTemporaryFile
// ============================================================

struct TempFileState{
    std::string path;
    std::string mode;
    bool deleteOnClose = true;
    bool closed = false;
    value file;
};

static TempFileState* tempFileState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<TempFileState*>(inst->handle);
}

static value openFileValue(cffi& a, const std::string& path, const std::string& mode){
    ffilib* builtins = a.vm->findCffiLibrary("builtins");
    value fileObj = a.none();
    if (!builtins) return fileObj;
    std::vector<value> args;
    args.push_back(a.mkstr(path));
    args.push_back(a.mkstr(mode));
    std::map<std::string, value> kwargs;
    cffiCallFunction(*builtins, "open", *a.vm, args, fileObj, kwargs);
    return fileObj;
}

static value makeTempFile(cffi& a, bool alwaysDelete){
    std::string suffix = optString(a, "suffix", "");
    std::string prefix = optString(a, "prefix", "tmp");
    std::string dir = optString(a, "dir", "");
    std::string mode = optString(a, "mode", "w+b");
    if (dir.empty()) dir = defaultDir();
    std::error_code ec;
    fs::create_directories(fsPathFromUtf8(dir), ec);

    auto* st = new TempFileState();
    st->mode = mode;
    st->deleteOnClose = alwaysDelete ? true : optBool(a, "delete", true);
    int fd = -1;
    for (int attempt = 0; attempt < 100; ++attempt){
        std::string name = tempName(suffix, prefix, dir);
        fd = createExclusive(name);
        if (fd >= 0){
            st->path = name;
            break;
        }
    }
    if (fd < 0){
        delete st;
        a.vm->raiseError("FileExistsError", "No usable temporary file name found");
        return a.none();
    }
    closeFd(fd);
    st->file = openFileValue(a, st->path, mode);
    if (st->file.type != "native_object" || !st->file.ptr){
        std::string failedPath = st->path;
        delete st;
        if (!a.vm->hasError())
            a.vm->raiseError("OSError", "cannot open temporary file '" + failedPath + "'");
        return a.none();
    }
    value obj = a.mkobject(st);
    if (obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["name"] = a.mkstr(st->path);
        inst->attributes["mode"] = a.mkstr(mode);
        inst->attributes["closed"] = a.mkbool(false);
        inst->attributes["delete"] = a.mkbool(st->deleteOnClose);
    }
    return obj;
}

static value fnNamedTempCtor(cffi& a){ return makeTempFile(a, false); }

static value fnTempFileCtor(cffi& a){ return makeTempFile(a, true); }

static value fnSpooledCtor(cffi& a){ return makeTempFile(a, true); }

static value tempDelegate(cffi& a, const std::string& name){
    auto* st = tempFileState(a);
    if (!st || !st->file.ptr) return a.none();
    if (st->closed){
        a.vm->raiseError("ValueError", "I/O operation on closed file");
        return a.none();
    }
    std::vector<value> args;
    for (size_t i = 0; i < a.argc(); ++i) args.push_back(a.arg(i));
    value result = a.none();
    if (!cffiCallMethod(*a.vm, st->file, name, args, result, a.kwargv)){
        if (!a.vm->hasError()) a.vm->raiseError("UnsupportedOperation", name);
        return a.none();
    }
    return result;
}

#define MP_TEMP_DELEGATE(FN, NAME) \
    static value FN(cffi& a){ return tempDelegate(a, NAME); }

MP_TEMP_DELEGATE(fnTempRead, "read")
MP_TEMP_DELEGATE(fnTempRead1, "read1")
MP_TEMP_DELEGATE(fnTempReadline, "readline")
MP_TEMP_DELEGATE(fnTempReadlines, "readlines")
MP_TEMP_DELEGATE(fnTempWrite, "write")
MP_TEMP_DELEGATE(fnTempWritelines, "writelines")
MP_TEMP_DELEGATE(fnTempFlush, "flush")
MP_TEMP_DELEGATE(fnTempSeek, "seek")
MP_TEMP_DELEGATE(fnTempTell, "tell")
MP_TEMP_DELEGATE(fnTempTruncate, "truncate")

#undef MP_TEMP_DELEGATE

static value fnTempClose(cffi& a){
    auto* st = tempFileState(a);
    if (!st) return a.none();
    value self = a.self();
    if (!st->closed){
        value r = a.none();
        if (st->file.ptr) cffiCallMethod(*a.vm, st->file, "close", {}, r);
        st->closed = true;
        if (self.ptr)
            static_cast<ffi_instance*>(self.ptr)->attributes["closed"] = a.mkbool(true);
        if (st->deleteOnClose){
            std::error_code ec;
            fs::remove(fsPathFromUtf8(st->path), ec);
        }
    }
    return a.none();
}

static value fnTempEnter(cffi& a){ return a.self(); }

static value fnTempExit(cffi& a){
    fnTempClose(a);
    return a.mkbool(false);
}

static value fnTempIter(cffi& a){ return a.self(); }

static value fnTempNext(cffi& a){
    auto* st = tempFileState(a);
    if (!st || !st->file.ptr){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    std::vector<value> args;
    value result = a.none();
    if (cffiCallMethod(*a.vm, st->file, "__next__", args, result, {})) return result;
    a.vm->clearErr();
    if (cffiCallMethod(*a.vm, st->file, "readline", args, result, {})){
        if (result.type == "str" && result.ptr &&
            static_cast<std::string*>(result.ptr)->empty()){
            a.vm->raiseError("StopIteration", "");
        }
        return result;
    }
    a.vm->clearErr();
    a.vm->raiseError("StopIteration", "");
    return a.none();
}

// ============================================================
// TemporaryDirectory
// ============================================================

struct TempDirState{
    std::string path;
    bool deleted = false;
};

static TempDirState* tempDirState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<TempDirState*>(inst->handle);
}

static value fnTemporaryDirectoryCtor(cffi& a){
    std::string suffix = optString(a, "suffix", "");
    std::string prefix = optString(a, "prefix", "tmp");
    std::string dir = optString(a, "dir", "");
    if (dir.empty()) dir = defaultDir();
    std::error_code ec;
    fs::create_directories(fsPathFromUtf8(dir), ec);
    std::string name;
    bool created = false;
    for (int attempt = 0; attempt < 100; ++attempt){
        name = tempName(suffix, prefix, dir);
        if (fs::create_directory(fsPathFromUtf8(name), ec) && !ec){
            created = true;
            break;
        }
        ec.clear();
    }
    if (!created){
        a.vm->raiseError("FileExistsError", "No usable temporary directory name found");
        return a.none();
    }
    auto* st = new TempDirState();
    st->path = name;
    value obj = a.mkobject(st);
    if (obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["name"] = a.mkstr(name);
    }
    return obj;
}

static value fnTempDirCleanup(cffi& a){
    auto* st = tempDirState(a);
    if (!st || st->deleted) return a.none();
    std::error_code ec;
    fs::remove_all(fsPathFromUtf8(st->path), ec);
    st->deleted = true;
    return a.none();
}

static value fnTempDirEnter(cffi& a){
    auto* st = tempDirState(a);
    if (!st) return a.none();
    return a.mkstr(st->path);
}

static value fnTempDirExit(cffi& a){
    fnTempDirCleanup(a);
    return a.mkbool(false);
}

static value fnTempDirFspath(cffi& a){
    auto* st = tempDirState(a);
    if (!st) return a.none();
    return a.mkstr(st->path);
}

static value fnTempDirStr(cffi& a){
    auto* st = tempDirState(a);
    if (!st) return a.mkstr("");
    return a.mkstr(st->path);
}

}

ffilib cffi_tempfile_lib(){
    std::map<std::string, void*> functions = {
        {"mkstemp", (void*)fnMkstemp},
        {"mkdtemp", (void*)fnMkdtemp},
        {"mktemp", (void*)fnMktemp},
        {"gettempdir", (void*)fnGettempdir},
        {"gettempdirb", (void*)fnGettempdirb}
    };

    std::map<std::string, void*> tempMethods = {
        {"read", (void*)fnTempRead},
        {"read1", (void*)fnTempRead1},
        {"readline", (void*)fnTempReadline},
        {"readlines", (void*)fnTempReadlines},
        {"write", (void*)fnTempWrite},
        {"writelines", (void*)fnTempWritelines},
        {"flush", (void*)fnTempFlush},
        {"seek", (void*)fnTempSeek},
        {"tell", (void*)fnTempTell},
        {"truncate", (void*)fnTempTruncate},
        {"close", (void*)fnTempClose},
        {"__enter__", (void*)fnTempEnter},
        {"__exit__", (void*)fnTempExit},
        {"__iter__", (void*)fnTempIter},
        {"__next__", (void*)fnTempNext}
    };

    ffi_class namedTempClass;
    namedTempClass.name = "NamedTemporaryFile";
    namedTempClass.constructor = (void*)fnNamedTempCtor;
    namedTempClass.methods = tempMethods;

    ffi_class tempFileClass;
    tempFileClass.name = "TemporaryFile";
    tempFileClass.constructor = (void*)fnTempFileCtor;
    tempFileClass.methods = tempMethods;

    ffi_class spooledClass;
    spooledClass.name = "SpooledTemporaryFile";
    spooledClass.constructor = (void*)fnSpooledCtor;
    spooledClass.methods = tempMethods;

    ffi_class tempDirClass;
    tempDirClass.name = "TemporaryDirectory";
    tempDirClass.constructor = (void*)fnTemporaryDirectoryCtor;
    tempDirClass.methods = {
        {"cleanup", (void*)fnTempDirCleanup},
        {"__enter__", (void*)fnTempDirEnter},
        {"__exit__", (void*)fnTempDirExit},
        {"__fspath__", (void*)fnTempDirFspath},
        {"__str__", (void*)fnTempDirStr}
    };

    return cffi_lib(
        "tempfile",
        functions,
        {},
        {
            {"NamedTemporaryFile", namedTempClass},
            {"TemporaryFile", tempFileClass},
            {"SpooledTemporaryFile", spooledClass},
            {"TemporaryDirectory", tempDirClass}
        });
}

}
