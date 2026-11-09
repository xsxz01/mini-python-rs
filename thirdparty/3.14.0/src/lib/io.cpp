#include "../cffi.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace minipy {

namespace {

// StringIO 状态
// StringIO state
// StringIO ᠪᠠᠶᠢᠳᠠᠯ
struct StringIoState{
    std::string buffer;
    size_t pos = 0;
    bool closed = false;
};

// BytesIO 状态
// BytesIO state
// BytesIO ᠪᠠᠶᠢᠳᠠᠯ
struct BytesIoState{
    std::string buffer;
    size_t pos = 0;
    bool closed = false;
};

// StringIO 状态获取
// get StringIO state
// StringIO ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
StringIoState* strState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<StringIoState*>(inst->handle);
}

BytesIoState* bytesState(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<BytesIoState*>(inst->handle);
}

bool checkClosed(cffi& a, bool closed){
    if (closed){
        a.vm->raiseError("ValueError", "I/O operation on closed file.");
        return true;
    }
    return false;
}

}

// StringIO 构造
// StringIO constructor
// StringIO ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnStringIoCtor(cffi& a){
    auto* state = new StringIoState();
    if (a.argc() > 0 && a.arg(0).type == "str" && a.arg(0).ptr)
        state->buffer = *static_cast<std::string*>(a.arg(0).ptr);
    value obj = a.mkobject(state);
    if (obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["closed"] = a.mkbool(false);
    return obj;
}

static void strWriteAt(StringIoState* state, const std::string& text){
    if (state->pos < state->buffer.size()){
        size_t overwrite = std::min(text.size(), state->buffer.size() - state->pos);
        state->buffer.replace(state->pos, overwrite, text.substr(0, overwrite));
        if (overwrite < text.size()) state->buffer.append(text.substr(overwrite));
    } else {
        state->buffer.append(text);
    }
    state->pos += text.size();
}

static value fnStrWrite(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    std::string text = a.vm->toString(a.arg(0));
    strWriteAt(state, text);
    return a.mkint(static_cast<long long>(text.size()));
}

static value fnStrRead(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long count = -1;
    if (a.argc() > 0 && a.arg(0).type != "none") count = a.arg_int(0);
    std::string out;
    if (count < 0){
        out = state->buffer.substr(state->pos);
        state->pos = state->buffer.size();
    } else {
        size_t n = std::min<size_t>(static_cast<size_t>(count),
                                    state->buffer.size() - state->pos);
        out = state->buffer.substr(state->pos, n);
        state->pos += n;
    }
    return a.mkstr(out);
}

static value fnStrReadline(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    if (state->pos >= state->buffer.size()) return a.mkstr("");
    size_t nl = state->buffer.find('\n', state->pos);
    std::string out;
    if (nl == std::string::npos){
        out = state->buffer.substr(state->pos);
        state->pos = state->buffer.size();
    } else {
        out = state->buffer.substr(state->pos, nl - state->pos + 1);
        state->pos = nl + 1;
    }
    return a.mkstr(out);
}

static value fnStrGetvalue(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    return a.mkstr(state->buffer);
}

static value fnStrSeek(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.mkint(0);
    long long offset = a.arg_int(0);
    long long whence = a.argc() > 1 ? a.arg_int(1) : 0;
    long long base = 0;
    if (whence == 1) base = static_cast<long long>(state->pos);
    else if (whence == 2) base = static_cast<long long>(state->buffer.size());
    long long target = base + offset;
    if (target < 0) target = 0;
    state->pos = static_cast<size_t>(target);
    return a.mkint(target);
}

static value fnStrTell(cffi& a){
    auto* state = strState(a);
    if (!state) return a.mkint(0);
    return a.mkint(static_cast<long long>(state->pos));
}

static value fnStrClose(cffi& a){
    auto* state = strState(a);
    if (state) state->closed = true;
    a.setattr("closed", a.mkbool(true));
    return a.none();
}

static value fnStrFlush(cffi& a){
    return a.none();
}

static value fnStrTruncate(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long size = a.argc() > 0 && a.arg(0).type != "none"
                         ? a.arg_int(0)
                         : static_cast<long long>(state->pos);
    if (size < 0) size = 0;
    state->buffer.resize(static_cast<size_t>(size));
    if (state->pos > state->buffer.size()) state->pos = state->buffer.size();
    return a.mkint(size);
}

static value fnStrIter(cffi& a){
    return a.self();
}

static value fnStrReadlines(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long hint = -1;
    if (a.argc() > 0 && a.arg(0).type != "none") hint = a.arg_int(0);
    std::vector<value> lines;
    long long total = 0;
    while (state->pos < state->buffer.size()){
        size_t nl = state->buffer.find('\n', state->pos);
        std::string line;
        if (nl == std::string::npos){
            line = state->buffer.substr(state->pos);
            state->pos = state->buffer.size();
        } else {
            line = state->buffer.substr(state->pos, nl - state->pos + 1);
            state->pos = nl + 1;
        }
        lines.push_back(a.mkstr(line));
        total += static_cast<long long>(line.size());
        if (hint >= 0 && total >= hint) break;
    }
    return a.mklist(lines);
}

static value fnStrWritelines(cffi& a){
    auto* state = strState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    for (auto& item : items){
        strWriteAt(state, a.vm->toString(item));
    }
    return a.none();
}

static value fnStrBoolTrue(cffi& a){ return a.mkbool(true); }
static value fnStrBoolFalse(cffi& a){ return a.mkbool(false); }

static value fnStrNext(cffi& a){
    auto* state = strState(a);
    if (!state || state->pos >= state->buffer.size()){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    return fnStrReadline(a);
}

static value fnStrEnter(cffi& a){
    return a.self();
}

static value fnStrExit(cffi& a){
    fnStrClose(a);
    return a.mkbool(false);
}

// BytesIO 构造
// BytesIO constructor
// BytesIO ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnBytesIoCtor(cffi& a){
    auto* state = new BytesIoState();
    if (a.argc() > 0 && a.arg(0).ptr &&
        (a.arg(0).type == "bytes" || a.arg(0).type == "bytearray"))
        state->buffer = *static_cast<std::string*>(a.arg(0).ptr);
    value obj = a.mkobject(state);
    if (obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["closed"] = a.mkbool(false);
    return obj;
}

static void bytesWriteAt(BytesIoState* state, const std::string& text){
    if (state->pos < state->buffer.size()){
        size_t overwrite = std::min(text.size(), state->buffer.size() - state->pos);
        state->buffer.replace(state->pos, overwrite, text.substr(0, overwrite));
        if (overwrite < text.size()) state->buffer.append(text.substr(overwrite));
    } else {
        state->buffer.append(text);
    }
    state->pos += text.size();
}

static value fnBytesWrite(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    value input = a.arg(0);
    std::string text;
    if (input.ptr && (input.type == "bytes" || input.type == "bytearray" ||
                      input.type == "str"))
        text = *static_cast<std::string*>(input.ptr);
    bytesWriteAt(state, text);
    return a.mkint(static_cast<long long>(text.size()));
}

static value fnBytesRead(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long count = -1;
    if (a.argc() > 0 && a.arg(0).type != "none") count = a.arg_int(0);
    std::string out;
    if (count < 0){
        out = state->buffer.substr(state->pos);
        state->pos = state->buffer.size();
    } else {
        size_t n = std::min<size_t>(static_cast<size_t>(count),
                                    state->buffer.size() - state->pos);
        out = state->buffer.substr(state->pos, n);
        state->pos += n;
    }
    value v = a.mkstr(out);
    v.type = "bytes";
    return v;
}

static value fnBytesGetvalue(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    value v = a.mkstr(state->buffer);
    v.type = "bytes";
    return v;
}

static value fnBytesSeek(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.mkint(0);
    long long offset = a.arg_int(0);
    long long whence = a.argc() > 1 ? a.arg_int(1) : 0;
    long long base = 0;
    if (whence == 1) base = static_cast<long long>(state->pos);
    else if (whence == 2) base = static_cast<long long>(state->buffer.size());
    long long target = base + offset;
    if (target < 0) target = 0;
    state->pos = static_cast<size_t>(target);
    return a.mkint(target);
}

static value fnBytesTell(cffi& a){
    auto* state = bytesState(a);
    if (!state) return a.mkint(0);
    return a.mkint(static_cast<long long>(state->pos));
}

static value fnBytesClose(cffi& a){
    auto* state = bytesState(a);
    if (state) state->closed = true;
    a.setattr("closed", a.mkbool(true));
    return a.none();
}

static value fnBytesFlush(cffi& a){
    return a.none();
}

static value fnBytesReadline(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    std::string out;
    if (state->pos < state->buffer.size()){
        size_t nl = state->buffer.find('\n', state->pos);
        if (nl == std::string::npos){
            out = state->buffer.substr(state->pos);
            state->pos = state->buffer.size();
        } else {
            out = state->buffer.substr(state->pos, nl - state->pos + 1);
            state->pos = nl + 1;
        }
    }
    value v = a.mkstr(out);
    v.type = "bytes";
    return v;
}

static value fnBytesReadlines(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long hint = -1;
    if (a.argc() > 0 && a.arg(0).type != "none") hint = a.arg_int(0);
    std::vector<value> lines;
    long long total = 0;
    while (state->pos < state->buffer.size()){
        size_t nl = state->buffer.find('\n', state->pos);
        std::string line;
        if (nl == std::string::npos){
            line = state->buffer.substr(state->pos);
            state->pos = state->buffer.size();
        } else {
            line = state->buffer.substr(state->pos, nl - state->pos + 1);
            state->pos = nl + 1;
        }
        value v = a.mkstr(line);
        v.type = "bytes";
        lines.push_back(v);
        total += static_cast<long long>(line.size());
        if (hint >= 0 && total >= hint) break;
    }
    return a.mklist(lines);
}

static value fnBytesWritelines(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    std::vector<value> items;
    a.vm->materializeIterable(a.arg(0), items);
    for (auto& item : items){
        if (item.ptr && (item.type == "bytes" || item.type == "bytearray" || item.type == "str"))
            bytesWriteAt(state, *static_cast<std::string*>(item.ptr));
    }
    return a.none();
}

static value fnBytesTruncate(cffi& a){
    auto* state = bytesState(a);
    if (!state || checkClosed(a, state->closed)) return a.none();
    long long size = a.argc() > 0 && a.arg(0).type != "none"
                         ? a.arg_int(0)
                         : static_cast<long long>(state->pos);
    if (size < 0) size = 0;
    state->buffer.resize(static_cast<size_t>(size));
    if (state->pos > state->buffer.size()) state->pos = state->buffer.size();
    return a.mkint(size);
}

static value fnBytesIter(cffi& a){ return a.self(); }

static value fnBytesNext(cffi& a){
    auto* state = bytesState(a);
    if (!state || state->pos >= state->buffer.size()){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    return fnBytesReadline(a);
}

static value fnBytesEnter(cffi& a){
    return a.self();
}

static value fnBytesExit(cffi& a){
    fnBytesClose(a);
    return a.mkbool(false);
}

// ============================================================
// 通用 I/O 包装类
// Generic I/O wrapper classes
// ============================================================

struct IoWrapState{
    value reader;
    value writer;
    bool hasWriter = false;
    bool closed = false;
};

static IoWrapState* wrapStateOf(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<IoWrapState*>(inst->handle);
}

static value fnWrapCtorCommon(cffi& a, bool useWriter){
    auto* st = new IoWrapState();
    if (a.argc() > 0) st->reader = a.arg(0);
    if (useWriter && a.argc() > 1){
        st->writer = a.arg(1);
        st->hasWriter = true;
    }
    value obj = a.mkobject(st);
    if (obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["closed"] = a.mkbool(false);
        if (a.argc() > 0) inst->attributes["buffer"] = a.arg(0);
        value nm = a.attr(st->reader, "name");
        if (nm.type != "none") inst->attributes["name"] = nm;
        value md = a.attr(st->reader, "mode");
        if (md.type != "none") inst->attributes["mode"] = md;
    }
    return obj;
}

static value fnTextIOWrapperCtor(cffi& a){ return fnWrapCtorCommon(a, false); }
static value fnBufferedReaderCtor(cffi& a){ return fnWrapCtorCommon(a, false); }
static value fnBufferedWriterCtor(cffi& a){ return fnWrapCtorCommon(a, false); }
static value fnBufferedRandomCtor(cffi& a){ return fnWrapCtorCommon(a, false); }
static value fnBufferedRWPairCtor(cffi& a){ return fnWrapCtorCommon(a, true); }

static value convertTextResult(cffi& a, const std::string& name, value result);

static value wrapDelegate(cffi& a, const std::string& name){
    auto* st = wrapStateOf(a);
    if (!st || !st->reader.ptr) return a.none();
    if (st->closed){
        a.vm->raiseError("ValueError", "I/O operation on closed file.");
        return a.none();
    }
    std::vector<value> args;
    for (size_t i = 0; i < a.argc(); ++i) args.push_back(a.arg(i));
    value result = a.none();
    const value& target = (st->hasWriter && name == "write") ? st->writer : st->reader;
    if (!cffiCallMethod(*a.vm, target, name, args, result, a.kwargv)){
        if (!a.vm->hasError())
            a.vm->raiseError("UnsupportedOperation", name);
        return a.none();
    }
    return convertTextResult(a, name, result);
}

#define MP_IO_DELEGATE(FN, NAME) \
    static value FN(cffi& a){ return wrapDelegate(a, NAME); }

MP_IO_DELEGATE(fnWrapRead, "read")
MP_IO_DELEGATE(fnWrapRead1, "read1")
MP_IO_DELEGATE(fnWrapReadline, "readline")
MP_IO_DELEGATE(fnWrapReadlines, "readlines")
MP_IO_DELEGATE(fnWrapWrite, "write")
MP_IO_DELEGATE(fnWrapWritelines, "writelines")
MP_IO_DELEGATE(fnWrapFlush, "flush")
MP_IO_DELEGATE(fnWrapSeek, "seek")
MP_IO_DELEGATE(fnWrapTell, "tell")
MP_IO_DELEGATE(fnWrapTruncate, "truncate")
MP_IO_DELEGATE(fnWrapFileno, "fileno")

#undef MP_IO_DELEGATE

static value fnWrapReadable(cffi& a){
    auto* st = wrapStateOf(a);
    if (!st) return a.mkbool(false);
    value md = a.attr(st->reader, "mode");
    if (md.type == "str"){
        std::string m = a.vm->toString(md);
        return a.mkbool(m.find('r') != std::string::npos || m.find('+') != std::string::npos);
    }
    return a.mkbool(true);
}

static value fnWrapWritable(cffi& a){
    auto* st = wrapStateOf(a);
    if (!st) return a.mkbool(false);
    if (st->hasWriter) return a.mkbool(true);
    value md = a.attr(st->reader, "mode");
    if (md.type == "str"){
        std::string m = a.vm->toString(md);
        return a.mkbool(m.find('w') != std::string::npos || m.find('a') != std::string::npos ||
                        m.find('x') != std::string::npos || m.find('+') != std::string::npos);
    }
    return a.mkbool(true);
}

static value fnWrapSeekable(cffi& a){
    auto* st = wrapStateOf(a);
    return a.mkbool(st && st->reader.ptr);
}

static value fnWrapIsatty(cffi& a){
    return a.mkbool(false);
}

static value fnWrapDetach(cffi& a){
    auto* st = wrapStateOf(a);
    if (!st) return a.none();
    if (st->closed){
        a.vm->raiseError("ValueError", "I/O operation on closed file.");
        return a.none();
    }
    st->closed = true;
    a.setattr("closed", a.mkbool(true));
    return st->reader;
}

static value fnWrapClose(cffi& a){
    value self = a.self();
    auto* st = wrapStateOf(a);
    if (!st) return a.none();
    if (!st->closed){
        value r = a.none();
        if (st->reader.ptr) cffiCallMethod(*a.vm, st->reader, "close", {}, r);
        if (st->hasWriter && st->writer.ptr){
            value r2 = a.none();
            cffiCallMethod(*a.vm, st->writer, "close", {}, r2);
        }
        st->closed = true;
        if (self.ptr)
            static_cast<ffi_instance*>(self.ptr)->attributes["closed"] = a.mkbool(true);
    }
    return a.none();
}

static value fnWrapEnter(cffi& a){
    return a.self();
}

static value fnWrapExit(cffi& a){
    fnWrapClose(a);
    return a.mkbool(false);
}

static value fnWrapIter(cffi& a){
    return a.self();
}

static value fnWrapNext(cffi& a){
    auto* st = wrapStateOf(a);
    if (!st || !st->reader.ptr){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    std::vector<value> args;
    value result = a.none();
    if (cffiCallMethod(*a.vm, st->reader, "__next__", args, result, {})) return result;
    a.vm->clearErr();
    if (cffiCallMethod(*a.vm, st->reader, "readline", args, result, {})){
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
// IOBase / RawIOBase / BufferedIOBase / TextIOBase
// ============================================================

static value fnBaseCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    if (obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["closed"] = a.mkbool(false);
    return obj;
}

static value baseUnsupported(cffi& a, const std::string& name){
    a.vm->raiseError("UnsupportedOperation", name);
    return a.none();
}

static value fnBaseRead(cffi& a){ return baseUnsupported(a, "read"); }
static value fnBaseRead1(cffi& a){ return baseUnsupported(a, "read1"); }
static value fnBaseSeek(cffi& a){ return baseUnsupported(a, "seek"); }
static value fnBaseTell(cffi& a){ return baseUnsupported(a, "seek"); }
static value fnBaseTruncate(cffi& a){ return baseUnsupported(a, "truncate"); }
static value fnBaseFileno(cffi& a){ return baseUnsupported(a, "fileno"); }

static value fnNotImplemented(cffi& a){
    a.vm->raiseError("NotImplementedError", "");
    return a.none();
}

static value fnBufferedReadUnsupported(cffi& a){ return baseUnsupported(a, "read"); }
static value fnTextReadlineUnsupported(cffi& a){ return baseUnsupported(a, "readline"); }

static value fnBaseClose(cffi& a){
    value self = a.self();
    if (self.ptr)
        static_cast<ffi_instance*>(self.ptr)->attributes["closed"] = a.mkbool(true);
    return a.none();
}

static value fnBaseFlush(cffi& a){ return a.none(); }
static value fnBaseBoolFalse(cffi& a){ return a.mkbool(false); }
static value fnBaseEnter(cffi& a){ return a.self(); }
static value fnBaseExit(cffi& a){ fnBaseClose(a); return a.mkbool(false); }
static value fnBaseIter(cffi& a){ return a.self(); }

// TextIOWrapper: 二进制底层读出后转换为文本, 并按通用换行翻译
static std::string ioUniversalNewlines(const std::string& text){
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i){
        if (text[i] == '\r'){
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            out += text[i];
        }
    }
    return out;
}

static value convertTextResult(cffi& a, const std::string& name, value result){
    bool textClass = a.currentClass && a.currentClass->name == "TextIOWrapper";
    if (!textClass) return result;
    if ((name == "read" || name == "read1" || name == "readline") &&
        result.type == "bytes" && result.ptr){
        value out = a.mkstr(ioUniversalNewlines(*static_cast<std::string*>(result.ptr)));
        return out;
    }
    if (name == "readlines" && result.type == "list" && result.ptr){
        std::vector<value> items;
        a.vm->materializeIterable(result, items);
        std::vector<value> out;
        for (auto& item : items){
            if (item.type == "bytes" && item.ptr)
                out.push_back(a.mkstr(ioUniversalNewlines(*static_cast<std::string*>(item.ptr))));
            else
                out.push_back(item);
        }
        return a.mklist(out);
    }
    return result;
}

// FileIO: 基于内置 open 的文件对象
static value fnFileIoCtor(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "FileIO() missing required argument 'file' (pos 1)");
        return a.none();
    }
    std::string path = a.vm->toString(a.arg(0));
    std::string mode = "r";
    if (a.argc() > 1 && a.arg(1).type != "none") mode = a.vm->toString(a.arg(1));
    if (a.has_kwarg("mode") && a.kwarg("mode").type != "none")
        mode = a.vm->toString(a.kwarg("mode"));
    ffilib* builtins = a.vm->findCffiLibrary("builtins");
    value fileObj = a.none();
    if (builtins){
        std::vector<value> args;
        args.push_back(a.mkstr(path));
        args.push_back(a.mkstr(mode));
        std::map<std::string, value> kwargs;
        cffiCallFunction(*builtins, "open", *a.vm, args, fileObj, kwargs);
    }
    if (fileObj.type != "native_object" || !fileObj.ptr) return a.none();

    auto* st = new IoWrapState();
    st->reader = fileObj;
    value obj = a.mkobject(st);
    if (obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["closed"] = a.mkbool(false);
        inst->attributes["name"] = a.mkstr(path);
        inst->attributes["mode"] = a.mkstr(mode);
    }
    return obj;
}

// 常量
static value dataSeekSet(cffi& a){ return a.mkint(0); }
static value dataSeekCur(cffi& a){ return a.mkint(1); }
static value dataSeekEnd(cffi& a){ return a.mkint(2); }

// io.open 复用内置 open
static value fnIoOpen(cffi& a){
    ffilib* builtins = a.vm->findCffiLibrary("builtins");
    if (!builtins){
        a.vm->raiseError("RuntimeError", "builtins module not available");
        return a.none();
    }
    std::vector<value> args;
    for (size_t i = 0; i < a.argc(); ++i) args.push_back(a.arg(i));
    value result = a.none();
    cffiCallFunction(*builtins, "open", *a.vm, args, result, a.kwargv);
    return result;
}

// cffi io 库
// cffi io library
// cffi io ᠨᠣᠮ
ffilib cffi_io_lib(){
    ffi_class stringIoClass;
    stringIoClass.name = "StringIO";
    stringIoClass.constructor = (void*)fnStringIoCtor;
    stringIoClass.methods = {
        {"write", (void*)fnStrWrite},
        {"read", (void*)fnStrRead},
        {"readline", (void*)fnStrReadline},
        {"readlines", (void*)fnStrReadlines},
        {"writelines", (void*)fnStrWritelines},
        {"getvalue", (void*)fnStrGetvalue},
        {"seek", (void*)fnStrSeek},
        {"tell", (void*)fnStrTell},
        {"close", (void*)fnStrClose},
        {"flush", (void*)fnStrFlush},
        {"truncate", (void*)fnStrTruncate},
        {"readable", (void*)fnStrBoolTrue},
        {"writable", (void*)fnStrBoolTrue},
        {"seekable", (void*)fnStrBoolTrue},
        {"isatty", (void*)fnStrBoolFalse},
        {"__iter__", (void*)fnStrIter},
        {"__next__", (void*)fnStrNext},
        {"__enter__", (void*)fnStrEnter},
        {"__exit__", (void*)fnStrExit}
    };

    ffi_class bytesIoClass;
    bytesIoClass.name = "BytesIO";
    bytesIoClass.constructor = (void*)fnBytesIoCtor;
    bytesIoClass.methods = {
        {"write", (void*)fnBytesWrite},
        {"read", (void*)fnBytesRead},
        {"read1", (void*)fnBytesRead},
        {"readline", (void*)fnBytesReadline},
        {"readlines", (void*)fnBytesReadlines},
        {"writelines", (void*)fnBytesWritelines},
        {"getvalue", (void*)fnBytesGetvalue},
        {"seek", (void*)fnBytesSeek},
        {"tell", (void*)fnBytesTell},
        {"close", (void*)fnBytesClose},
        {"flush", (void*)fnBytesFlush},
        {"truncate", (void*)fnBytesTruncate},
        {"readable", (void*)fnStrBoolTrue},
        {"writable", (void*)fnStrBoolTrue},
        {"seekable", (void*)fnStrBoolTrue},
        {"isatty", (void*)fnStrBoolFalse},
        {"__iter__", (void*)fnBytesIter},
        {"__next__", (void*)fnBytesNext},
        {"__enter__", (void*)fnBytesEnter},
        {"__exit__", (void*)fnBytesExit}
    };

    std::map<std::string, void*> wrapMethods = {
        {"read", (void*)fnWrapRead},
        {"read1", (void*)fnWrapRead1},
        {"readline", (void*)fnWrapReadline},
        {"readlines", (void*)fnWrapReadlines},
        {"write", (void*)fnWrapWrite},
        {"writelines", (void*)fnWrapWritelines},
        {"flush", (void*)fnWrapFlush},
        {"seek", (void*)fnWrapSeek},
        {"tell", (void*)fnWrapTell},
        {"truncate", (void*)fnWrapTruncate},
        {"fileno", (void*)fnWrapFileno},
        {"readable", (void*)fnWrapReadable},
        {"writable", (void*)fnWrapWritable},
        {"seekable", (void*)fnWrapSeekable},
        {"isatty", (void*)fnWrapIsatty},
        {"detach", (void*)fnWrapDetach},
        {"close", (void*)fnWrapClose},
        {"__enter__", (void*)fnWrapEnter},
        {"__exit__", (void*)fnWrapExit},
        {"__iter__", (void*)fnWrapIter},
        {"__next__", (void*)fnWrapNext}
    };

    auto makeWrapClass = [&](const std::string& name, void* ctor){
        ffi_class cls;
        cls.name = name;
        cls.constructor = ctor;
        cls.methods = wrapMethods;
        return cls;
    };

    std::map<std::string, void*> baseMethods = {
        {"seek", (void*)fnBaseSeek},
        {"tell", (void*)fnBaseTell},
        {"truncate", (void*)fnBaseTruncate},
        {"fileno", (void*)fnBaseFileno},
        {"flush", (void*)fnBaseFlush},
        {"close", (void*)fnBaseClose},
        {"readable", (void*)fnBaseBoolFalse},
        {"writable", (void*)fnBaseBoolFalse},
        {"seekable", (void*)fnBaseBoolFalse},
        {"isatty", (void*)fnBaseBoolFalse},
        {"__enter__", (void*)fnBaseEnter},
        {"__exit__", (void*)fnBaseExit},
        {"__iter__", (void*)fnBaseIter}
    };

    auto makeBaseClass = [&](const std::string& name,
                             std::map<std::string, void*> extra){
        ffi_class cls;
        cls.name = name;
        cls.constructor = (void*)fnBaseCtor;
        cls.methods = baseMethods;
        for (auto& item : extra) cls.methods[item.first] = item.second;
        return cls;
    };

    return cffi_lib(
        "io",
        {
            {"open", (void*)fnIoOpen}
        },
        {
            {"SEEK_SET", (void*)dataSeekSet},
            {"SEEK_CUR", (void*)dataSeekCur},
            {"SEEK_END", (void*)dataSeekEnd}
        },
        {
            {"StringIO", stringIoClass},
            {"BytesIO", bytesIoClass},
            {"TextIOWrapper", makeWrapClass("TextIOWrapper", (void*)fnTextIOWrapperCtor)},
            {"BufferedReader", makeWrapClass("BufferedReader", (void*)fnBufferedReaderCtor)},
            {"BufferedWriter", makeWrapClass("BufferedWriter", (void*)fnBufferedWriterCtor)},
            {"BufferedRandom", makeWrapClass("BufferedRandom", (void*)fnBufferedRandomCtor)},
            {"BufferedRWPair", makeWrapClass("BufferedRWPair", (void*)fnBufferedRWPairCtor)},
            {"FileIO", makeWrapClass("FileIO", (void*)fnFileIoCtor)},
            {"IOBase", makeBaseClass("IOBase", {})},
            {"RawIOBase", makeBaseClass("RawIOBase", {
                {"read", (void*)fnNotImplemented},
                {"readinto", (void*)fnNotImplemented},
                {"write", (void*)fnNotImplemented},
                {"readline", (void*)fnNotImplemented},
                {"readlines", (void*)fnNotImplemented},
                {"__next__", (void*)fnNotImplemented}
            })},
            {"BufferedIOBase", makeBaseClass("BufferedIOBase", {
                {"read", (void*)fnBufferedReadUnsupported},
                {"read1", (void*)fnBaseRead1},
                {"readline", (void*)fnBufferedReadUnsupported},
                {"readlines", (void*)fnBufferedReadUnsupported},
                {"__next__", (void*)fnBufferedReadUnsupported}
            })},
            {"TextIOBase", makeBaseClass("TextIOBase", {
                {"read", (void*)fnBaseRead},
                {"readline", (void*)fnTextReadlineUnsupported},
                {"readlines", (void*)fnTextReadlineUnsupported},
                {"__next__", (void*)fnTextReadlineUnsupported}
            })},
            {"UnsupportedOperation", makeBaseClass("UnsupportedOperation", {})}
        });
}

}
