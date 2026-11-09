#include "../cffi.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 小端读取
// Little-endian readers
// ᠰᠢᠭᠤᠮ᠎ᠠ ᠤᠨ ᠲᠣᠭ᠎ᠠ
uint16_t rd16le(const std::string& d, size_t off){
    if (off + 2 > d.size()) return 0;
    return static_cast<uint16_t>((uint16_t(static_cast<unsigned char>(d[off+1])) << 8) |
                                uint16_t(static_cast<unsigned char>(d[off])));
}

uint32_t rd32le(const std::string& d, size_t off){
    if (off + 4 > d.size()) return 0;
    return (uint32_t(static_cast<unsigned char>(d[off+3])) << 24) |
           (uint32_t(static_cast<unsigned char>(d[off+2])) << 16) |
           (uint32_t(static_cast<unsigned char>(d[off+1])) << 8) |
           uint32_t(static_cast<unsigned char>(d[off]));
}

// 读取整个文件
// Read whole file
// ᠪᠦᠭᠦᠳᠡ ᠪᠢᠴᠢᠭ ᠬᠠᠯᠠᠬᠤ
std::string readAll(const std::string& path){
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::string data;
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return data;
}

// Wave_read 状态
// Wave_read state
// Wave_read ᠪᠠᠶᠢᠳᠠᠯ
struct WaveReadState{
    std::string data;
    size_t dataOffset = 0;
    size_t dataSize = 0;
    size_t pos = 0;
    long long channels = 0;
    long long sampwidth = 0;
    long long framerate = 0;
    long long nframes = 0;
    std::string comptype = "NONE";
    std::string compname = "not compressed";
    bool closed = false;
};

// Wave_write 状态
// Wave_write state
// Wave_write ᠪᠠᠶᠢᠳᠠᠯ
struct WaveWriteState{
    std::string filename;
    std::string buffer;
    long long channels = 0;
    long long sampwidth = 0;
    long long framerate = 0;
    long long nframes = 0;
    std::string comptype = "NONE";
    std::string compname = "not compressed";
    bool closed = false;
    bool written = false;
};

// 状态取用
// Fetch states
// ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
WaveReadState* readState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    return static_cast<WaveReadState*>(static_cast<ffi_instance*>(self.ptr)->handle);
}

WaveWriteState* writeState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    return static_cast<WaveWriteState*>(static_cast<ffi_instance*>(self.ptr)->handle);
}

const ffi_class* findClass(const ffilib* lib, const std::string& name){
    if (!lib) return nullptr;
    auto it = lib->classes.find(name);
    return it == lib->classes.end() ? nullptr : &it->second;
}

// 解析 WAV 头
// Parse WAV header
// WAV ᠲᠣᠯᠤᠭ᠎ᠠ ᠲᠠᠢᠯᠢᠬᠤ
bool parseWave(WaveReadState* st, std::string& err){
    const std::string& d = st->data;
    if (d.size() < 12 || d.compare(0, 4, "RIFF") != 0 ||
        d.compare(8, 4, "WAVE") != 0){
        err = "file does not start with RIFF id";
        return false;
    }
    size_t off = 12;
    bool haveFmt = false;
    while (off + 8 <= d.size()){
        std::string id = d.substr(off, 4);
        uint32_t size = rd32le(d, off + 4);
        size_t body = off + 8;
        if (id == "fmt "){
            if (size < 16){ err = "bad fmt chunk"; return false; }
            long long format = rd16le(d, body);
            if (format != 1){ err = "unknown format: " + std::to_string(format); return false; }
            st->channels = rd16le(d, body + 2);
            st->framerate = rd32le(d, body + 4);
            st->sampwidth = rd16le(d, body + 14) / 8;
            haveFmt = true;
        } else if (id == "data"){
            st->dataOffset = body;
            st->dataSize = static_cast<size_t>(size);
            if (st->dataOffset + st->dataSize > d.size())
                st->dataSize = d.size() - st->dataOffset;
            break;
        }
        off = body + size + (size & 1);
    }
    if (!haveFmt){ err = "fmt chunk and/or data chunk missing"; return false; }
    long long frameBytes = st->channels * st->sampwidth;
    st->nframes = frameBytes > 0 ? static_cast<long long>(st->dataSize / frameBytes) : 0;
    return true;
}

// Wave_read 构造
// Wave_read constructor
// Wave_read ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnReadCtor(cffi& a){
    std::string path = a.arg_str(0);
    auto* st = new WaveReadState();
    st->data = readAll(path);
    std::string err;
    if (!parseWave(st, err)){
        a.vm->raiseError("Error", err);
        return a.none();
    }
    value obj = a.vm->makeNativeObject(a.library,
                                       findClass(a.library, "Wave_read"), st);
    return obj;
}

// Wave_write 构造
// Wave_write constructor
// Wave_write ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnWriteCtor(cffi& a){
    auto* st = new WaveWriteState();
    st->filename = a.arg_str(0);
    value obj = a.vm->makeNativeObject(a.library,
                                       findClass(a.library, "Wave_write"), st);
    return obj;
}

// getnchannels
static value fnGetnchannels(cffi& a){
    auto* st = readState(a);
    return a.mkint(st ? st->channels : 0);
}

// getsampwidth
static value fnGetsampwidth(cffi& a){
    auto* st = readState(a);
    return a.mkint(st ? st->sampwidth : 0);
}

// getframerate
static value fnGetframerate(cffi& a){
    auto* st = readState(a);
    return a.mkint(st ? st->framerate : 0);
}

// getnframes
static value fnGetnframes(cffi& a){
    auto* st = readState(a);
    return a.mkint(st ? st->nframes : 0);
}

// getcomptype
static value fnGetcomptype(cffi& a){
    auto* st = readState(a);
    return a.mkstr(st ? st->comptype : "NONE");
}

// getcompname
static value fnGetcompname(cffi& a){
    auto* st = readState(a);
    return a.mkstr(st ? st->compname : "not compressed");
}

// getparams
static value fnGetparams(cffi& a){
    auto* st = readState(a);
    if (!st) return a.vm->makeTuple({});
    return a.vm->makeTuple({a.mkint(st->channels), a.mkint(st->sampwidth),
                            a.mkint(st->framerate), a.mkint(st->nframes),
                            a.mkstr(st->comptype), a.mkstr(st->compname)});
}

// tell
static value fnReadTell(cffi& a){
    auto* st = readState(a);
    if (!st) return a.mkint(0);
    long long frameBytes = st->channels * st->sampwidth;
    return a.mkint(frameBytes > 0 ? static_cast<long long>(st->pos / frameBytes) : 0);
}

// setpos
static value fnSetpos(cffi& a){
    auto* st = readState(a);
    if (!st) return a.none();
    long long frameBytes = st->channels * st->sampwidth;
    long long target = a.arg_int(0) * frameBytes;
    if (target < 0) target = 0;
    st->pos = static_cast<size_t>(target);
    return a.none();
}

// rewind
static value fnRewind(cffi& a){
    auto* st = readState(a);
    if (st) st->pos = 0;
    return a.none();
}

// readframes
static value fnReadframes(cffi& a){
    auto* st = readState(a);
    if (!st) return a.mkstr("");
    long long count = a.arg_int(0);
    if (count < 0) count = 0;
    long long frameBytes = st->channels * st->sampwidth;
    size_t remaining = st->pos < st->dataSize ? st->dataSize - st->pos : 0;
    size_t want = static_cast<size_t>(count) * static_cast<size_t>(frameBytes);
    if (want > remaining) want = remaining;
    std::string raw = st->data.substr(st->dataOffset + st->pos, want);
    st->pos += want;
    value v = a.mkstr(raw);
    v.type = "bytes";
    return v;
}

// getmarkers
static value fnGetmarkers(cffi& a){
    return a.none();
}

// close (read)
static value fnReadClose(cffi& a){
    auto* st = readState(a);
    if (st) st->closed = true;
    return a.none();
}

// Wave_read.__enter__
static value fnReadEnter(cffi& a){
    return a.self();
}

// Wave_read.__exit__
static value fnReadExit(cffi& a){
    auto* st = readState(a);
    if (st) st->closed = true;
    return a.mkbool(false);
}

// setnchannels
static value fnSetnchannels(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    st->channels = a.arg_int(0);
    return a.none();
}

// setsampwidth
static value fnSetsampwidth(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    long long width = a.arg_int(0);
    if (width < 1 || width > 4){
        a.vm->raiseError("Error", "bad sample width");
        return a.none();
    }
    st->sampwidth = width;
    return a.none();
}

// setframerate
static value fnSetframerate(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    st->framerate = a.arg_int(0);
    return a.none();
}

// setnframes
static value fnSetnframes(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    st->nframes = a.arg_int(0);
    return a.none();
}

// setcomptype
static value fnSetcomptype(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    st->comptype = a.arg_str(0);
    if (st->comptype != "NONE"){
        a.vm->raiseError("Error", "unknown compression type");
        return a.none();
    }
    st->compname = a.argc() > 1 ? a.arg_str(1) : "not compressed";
    return a.none();
}

// setparams
static value fnSetparams(cffi& a){
    value params = a.arg(0);
    std::vector<value>* fields = nullptr;
    if ((params.type == "tuple" || params.type == "list") && params.ptr)
        fields = static_cast<std::vector<value>*>(params.ptr);
    if (!fields || fields->size() < 6){
        a.vm->raiseError("Error", "bad params");
        return a.none();
    }
    auto* st = writeState(a);
    if (!st) return a.none();
    st->channels = a.vm->toInt((*fields)[0]);
    long long width = a.vm->toInt((*fields)[1]);
    if (width < 1 || width > 4){
        a.vm->raiseError("Error", "bad sample width");
        return a.none();
    }
    st->sampwidth = width;
    st->framerate = a.vm->toInt((*fields)[2]);
    st->nframes = a.vm->toInt((*fields)[3]);
    st->comptype = a.vm->toString((*fields)[4]);
    st->compname = a.vm->toString((*fields)[5]);
    if (st->comptype != "NONE"){
        a.vm->raiseError("Error", "unknown compression type");
        return a.none();
    }
    return a.none();
}

// tell (write)
static value fnWriteTell(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.mkint(0);
    long long frameBytes = st->channels * st->sampwidth;
    return a.mkint(frameBytes > 0
                   ? static_cast<long long>(st->buffer.size() / frameBytes) : 0);
}

// writeframesraw
static value fnWriteframesraw(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    value data = a.arg(0);
    if ((data.type == "bytes" || data.type == "bytearray" || data.type == "str") && data.ptr)
        st->buffer += *static_cast<std::string*>(data.ptr);
    return a.none();
}

// writeframes
static value fnWriteframes(cffi& a){
    auto* st = writeState(a);
    if (!st) return a.none();
    value data = a.arg(0);
    std::string raw;
    if ((data.type == "bytes" || data.type == "bytearray" || data.type == "str") && data.ptr)
        raw = *static_cast<std::string*>(data.ptr);
    long long frameBytes = st->channels * st->sampwidth;
    if (frameBytes <= 0){
        a.vm->raiseError("Error", "number of channels not specified");
        return a.none();
    }
    if (raw.size() % static_cast<size_t>(frameBytes) != 0){
        a.vm->raiseError("Error", "data chunk length not a whole number of frames");
        return a.none();
    }
    st->buffer += raw;
    return a.none();
}

// 写入完整 WAV 文件
// Write the complete WAV file
// ᠪᠦᠭᠦᠳᠡ WAV ᠪᠢᠴᠢᠭ ᠪᠢᠴᠢᠬᠦ
bool flushWave(cffi& a, WaveWriteState* st){
    if (st->channels <= 0 || st->sampwidth <= 0 || st->framerate <= 0){
        a.vm->raiseError("Error", "not all parameters set");
        return false;
    }
    long long frameBytes = st->channels * st->sampwidth;
    uint32_t dataSize = static_cast<uint32_t>(st->buffer.size());
    uint32_t nframes = static_cast<uint32_t>(frameBytes > 0
                          ? st->buffer.size() / static_cast<size_t>(frameBytes) : 0);
    std::ofstream out(st->filename, std::ios::binary);
    if (!out){
        a.vm->raiseError("Error", "cannot open output file");
        return false;
    }
    auto put32 = [&](uint32_t v){ out.put(static_cast<char>(v & 0xFF)); out.put(static_cast<char>((v >> 8) & 0xFF)); out.put(static_cast<char>((v >> 16) & 0xFF)); out.put(static_cast<char>((v >> 24) & 0xFF)); };
    auto put16 = [&](uint16_t v){ out.put(static_cast<char>(v & 0xFF)); out.put(static_cast<char>((v >> 8) & 0xFF)); };
    auto putTag = [&](const char* tag){ for (int i = 0; i < 4; ++i) out.put(tag[i]); };
    putTag("RIFF"); put32(dataSize + 36); putTag("WAVE");
    putTag("fmt "); put32(16);
    put16(1); put16(static_cast<uint16_t>(st->channels));
    put32(static_cast<uint32_t>(st->framerate));
    put32(static_cast<uint32_t>(st->framerate * frameBytes));
    put16(static_cast<uint16_t>(frameBytes));
    put16(static_cast<uint16_t>(st->sampwidth * 8));
    putTag("data"); put32(dataSize);
    out.write(st->buffer.data(), static_cast<std::streamsize>(st->buffer.size()));
    out.close();
    st->nframes = static_cast<long long>(nframes);
    st->written = true;
    return true;
}

// close (write)
static value fnWriteClose(cffi& a){
    auto* st = writeState(a);
    if (!st || st->closed) return a.none();
    if (!flushWave(a, st)) return a.none();
    st->closed = true;
    return a.none();
}

// Wave_write.__enter__
static value fnWriteEnter(cffi& a){
    return a.self();
}

// Wave_write.__exit__
static value fnWriteExit(cffi& a){
    return fnWriteClose(a);
}

// wave.open
// wave.open
// wave.open
static value fnOpen(cffi& a){
    std::string mode = a.argc() > 1 && a.arg(1).type != "none" ? a.arg_str(1) : "rb";
    if (mode.find('w') != std::string::npos){
        auto* st = new WaveWriteState();
        st->filename = a.arg_str(0);
        return a.vm->makeNativeObject(a.library,
                                      findClass(a.library, "Wave_write"), st);
    }
    auto* st = new WaveReadState();
    st->data = readAll(a.arg_str(0));
    std::string err;
    if (!parseWave(st, err)){
        a.vm->raiseError("Error", err);
        return a.none();
    }
    return a.vm->makeNativeObject(a.library,
                                  findClass(a.library, "Wave_read"), st);
}

}

// cffi wave lib
// cffi wave lib
// cffi wave lib
ffilib cffi_wave_lib(){
    ffi_class readClass;
    readClass.name = "Wave_read";
    readClass.constructor = (void*)fnReadCtor;
    readClass.methods["getnchannels"] = (void*)fnGetnchannels;
    readClass.methods["getsampwidth"] = (void*)fnGetsampwidth;
    readClass.methods["getframerate"] = (void*)fnGetframerate;
    readClass.methods["getnframes"] = (void*)fnGetnframes;
    readClass.methods["getcomptype"] = (void*)fnGetcomptype;
    readClass.methods["getcompname"] = (void*)fnGetcompname;
    readClass.methods["getparams"] = (void*)fnGetparams;
    readClass.methods["tell"] = (void*)fnReadTell;
    readClass.methods["setpos"] = (void*)fnSetpos;
    readClass.methods["rewind"] = (void*)fnRewind;
    readClass.methods["readframes"] = (void*)fnReadframes;
    readClass.methods["getmarkers"] = (void*)fnGetmarkers;
    readClass.methods["close"] = (void*)fnReadClose;
    readClass.methods["__enter__"] = (void*)fnReadEnter;
    readClass.methods["__exit__"] = (void*)fnReadExit;

    ffi_class writeClass;
    writeClass.name = "Wave_write";
    writeClass.constructor = (void*)fnWriteCtor;
    writeClass.methods["setnchannels"] = (void*)fnSetnchannels;
    writeClass.methods["setsampwidth"] = (void*)fnSetsampwidth;
    writeClass.methods["setframerate"] = (void*)fnSetframerate;
    writeClass.methods["setnframes"] = (void*)fnSetnframes;
    writeClass.methods["setcomptype"] = (void*)fnSetcomptype;
    writeClass.methods["setcompname"] = (void*)fnSetcomptype;
    writeClass.methods["setparams"] = (void*)fnSetparams;
    writeClass.methods["tell"] = (void*)fnWriteTell;
    writeClass.methods["writeframesraw"] = (void*)fnWriteframesraw;
    writeClass.methods["writeframes"] = (void*)fnWriteframes;
    writeClass.methods["close"] = (void*)fnWriteClose;
    writeClass.methods["__enter__"] = (void*)fnWriteEnter;
    writeClass.methods["__exit__"] = (void*)fnWriteExit;

    ffi_class errorClass;
    errorClass.name = "Error";

    return cffi_lib(
        "wave",
        {
            {"open", (void*)fnOpen}
        },
        {},
        {
            {"Wave_read", readClass},
            {"Wave_write", writeClass},
            {"Error", errorClass}
        });
}

}
