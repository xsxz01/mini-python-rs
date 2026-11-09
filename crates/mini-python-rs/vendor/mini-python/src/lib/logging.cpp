#include "../cffi.hpp"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 级别常量
// Level constants
// ᠲᠥᠪᠰᠢᠨ ᠦᠨ ᠲᠣᠭᠲᠠᠮᠠᠯ
constexpr int LVL_DEBUG = 10;
constexpr int LVL_INFO = 20;
constexpr int LVL_WARNING = 30;
constexpr int LVL_ERROR = 40;
constexpr int LVL_CRITICAL = 50;

// logger 状态 (name → 级别/处理器/父链)
// Logger state (name → level/handlers/parent chain)
// logger ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct LoggerState{
    std::string name;
    int level = 0;
    std::vector<value> handlers;
    value pyHandlers; // 共享的 Python 列表 / shared Python list / ᠬᠠᠮᠲᠤ ᠶᠢᠨ ᠵᠠᠭᠰᠠᠯᠲᠠ
    LoggerState* parent = nullptr;
    bool propagate = true;
    value pyObject; // 对应的 Python 侧实例 / Python-side instance / Python ᠦᠨᠢᠰ
};

// 每个 vm 的 logging 全局状态
// Per-vm logging global state
// vm ᠪᠤᠷᠢ ᠶᠢᠨ logging ᠪᠠᠶᠢᠳᠠᠯ
struct LoggingState{
    LoggerState* root = nullptr;
    std::map<std::string, LoggerState*> loggers;
};

std::map<vm*, LoggingState*>& registry(){
    static std::map<vm*, LoggingState*> table;
    return table;
}

// 取得指定 vm 的 logging 状态 (惰性创建)
// Get the logging state for a vm (lazy create)
// vm ᠦᠨ logging ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠪᠬᠤ
LoggingState* stateFor(vm& v){
    LoggingState*& st = registry()[&v];
    if (!st) st = new LoggingState();
    return st;
}

// 取得/创建指定名字的 logger 状态
// Get or create logger state by name
// ᠨᠡᠷ᠎ᠡ ᠪᠠᠷ logger ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠪᠬᠤ
LoggerState* loggerFor(vm& v, const std::string& name){
    LoggingState& st = *stateFor(v);
    if (!st.root){
        st.root = new LoggerState();
        st.root->name = "root";
        st.root->level = LVL_WARNING;
        st.root->pyHandlers = v.makeList({});
    }
    if (name.empty() || name == "root") return st.root;
    auto it = st.loggers.find(name);
    if (it != st.loggers.end()) return it->second;
    auto* created = new LoggerState();
    created->name = name;
    created->pyHandlers = v.makeList({});
    // 父 logger: 取最长前缀, 否则 root
    // Parent logger: longest prefix, else root
    // ᠡᠴᠡᠭᠡ logger: ᠬᠠᠮᠤᠭ ᠦᠨ ᠤᠷᠲᠤ ᠦᠰᠦᠭ
    size_t dot = name.rfind('.');
    std::string parentName = (dot == std::string::npos) ? "root" : name.substr(0, dot);
    created->parent = loggerFor(v, parentName);
    st.loggers[name] = created;
    return created;
}

// 级别号转名字
// Level number to name
// ᠲᠥᠪᠰᠢᠨ ᠦᠨ ᠲᠣᠭ᠎ᠠ → ᠨᠡᠷ᠎ᠡ
std::string levelName(int level){
    switch (level){
        case LVL_DEBUG: return "DEBUG";
        case LVL_INFO: return "INFO";
        case LVL_WARNING: return "WARNING";
        case LVL_ERROR: return "ERROR";
        case LVL_CRITICAL: return "CRITICAL";
        case 0: return "NOTSET";
        default: return "Level " + std::to_string(level);
    }
}

// 名字/数字转级别号
// Name or number to level number
// ᠨᠡᠷ᠎ᠡ/ᠲᠣᠭ᠎ᠠ → ᠲᠥᠪᠰᠢᠨ
int levelFrom(cffi& a, const value& v){
    if (v.type == "int") return static_cast<int>(a.vm->toInt(v));
    std::string text = a.vm->toString(v);
    if (text == "DEBUG") return LVL_DEBUG;
    if (text == "INFO") return LVL_INFO;
    if (text == "WARNING" || text == "WARN") return LVL_WARNING;
    if (text == "ERROR") return LVL_ERROR;
    if (text == "CRITICAL" || text == "FATAL") return LVL_CRITICAL;
    if (text == "NOTSET") return 0;
    try { return std::stoi(text); } catch (...) { return 0; }
}

// %-式消息格式化 (支持 %s %d %i %f %x %r %%, %(name)s)
// %-style message formatting (%s %d %i %f %x %r %%, %(name)s)
// %-ᠬᠡᠯᠪᠡᠷᠢ ᠶᠢᠨ ᠮᠡᠰᠰᠡᠵᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
std::string percentFormat(cffi& a, const std::string& fmt,
                          const std::vector<value>& args, size_t& consumed){
    std::string out;
    size_t argIndex = 0;
    for (size_t i = 0; i < fmt.size(); ++i){
        char c = fmt[i];
        if (c != '%'){ out += c; continue; }
        if (i + 1 >= fmt.size()){ out += '%'; continue; }
        char n = fmt[i + 1];
        if (n == '%'){ out += '%'; ++i; continue; }
        if (n == '('){
            size_t close = fmt.find(')', i + 2);
            if (close == std::string::npos){ out += c; continue; }
            out += fmt.substr(i, close + 2 - i);
            i = close + 1;
            continue;
        }
        // 跳过 flags/width/precision
        // Skip flags/width/precision
        // flags/width/precision ᠢ ᠠᠯᠬᠤᠬᠤ
        size_t j = i + 1;
        while (j < fmt.size() && (fmt[j] == '-' || fmt[j] == '+' || fmt[j] == ' ' ||
                                  fmt[j] == '0' || fmt[j] == '#')) j++;
        while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) j++;
        if (j < fmt.size() && fmt[j] == '.'){
            j++;
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) j++;
        }
        if (j >= fmt.size()){ out += c; continue; }
        char conv = fmt[j];
        value v = argIndex < args.size() ? args[argIndex] : a.none();
        ++argIndex;
        std::string spec = "%" + fmt.substr(i + 1, j - i - 1) + conv;
        char buf[512];
        switch (conv){
            case 's': out += a.vm->toString(v); break;
            case 'r': out += pythonRepr(*a.vm, v); break;
            case 'd': case 'i': case 'u': {
                std::snprintf(buf, sizeof(buf), spec.c_str(),
                              static_cast<int>(a.vm->toInt(v)));
                out += buf; break;
            }
            case 'x': {
                std::snprintf(buf, sizeof(buf), spec.c_str(),
                              static_cast<unsigned int>(a.vm->toInt(v)));
                out += buf; break;
            }
            case 'f': case 'e': case 'g': {
                std::snprintf(buf, sizeof(buf), spec.c_str(), a.vm->toFloat(v));
                out += buf; break;
            }
            default:
                out += a.vm->toString(v);
                break;
        }
        i = j;
    }
    consumed = argIndex;
    return out;
}

// 从 self 实例取得 handler 内部状态
// Get handler internal state from self instance
// self ᠡᠴᠡ handler ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠪᠬᠤ
int handlerLevel(cffi& a, const value& self){
    value lvl = a.attr(self, "level");
    if (lvl.type == "int") return static_cast<int>(a.vm->toInt(lvl));
    return 0;
}

// 组装 LogRecord (以 dict 表示, 键同 CPython)
// Build a LogRecord (as dict, keys mirror CPython)
// LogRecord ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeRecord(cffi& a, const std::string& loggerName, int level,
                 const std::string& msg, const std::vector<value>& extraArgs){
    size_t consumed = 0;
    std::string message = extraArgs.empty() ? msg : percentFormat(a, msg, extraArgs, consumed);
    double created = static_cast<double>(std::time(nullptr));
    std::vector<std::pair<value, value>> items;
    auto put = [&](const char* key, const value& v){
        items.emplace_back(a.mkstr(key), v);
    };
    put("name", a.mkstr(loggerName));
    put("levelno", a.mkint(level));
    put("levelname", a.mkstr(levelName(level)));
    put("msg", a.mkstr(msg));
    put("message", a.mkstr(message));
    std::vector<value> argItems(extraArgs.begin(), extraArgs.end());
    value argTuple = a.vm->makeTuple(argItems);
    put("args", argTuple);
    put("created", a.mkfloat(created));
    put("msecs", a.mkfloat(0));
    put("module", a.mkstr(""));
    put("funcName", a.mkstr(""));
    put("lineno", a.mkint(0));
    put("pathname", a.mkstr(""));
    put("filename", a.mkstr(""));
    put("process", a.mkint(0));
    put("thread", a.mkint(0));
    put("asctime", a.mkstr(""));
    return a.mkdict(items);
}

// dict 记录取值
// Read a key from the record dict
// dict ᠡᠴᠡ ᠲᠤᠰᠢᠶᠠᠷ ᠢ ᠠᠪᠬᠤ
std::string recordStr(cffi& a, const value& record, const char* key){
    if (record.type != "dict" || !record.ptr) return "";
    auto* items = static_cast<dict_t*>(record.ptr);
    for (auto& kv : *items){
        if (kv.first.type == "str" && a.vm->toString(kv.first) == key){
            return a.vm->toString(kv.second);
        }
    }
    return "";
}

// Formatter.format 的 %(key)type 替换
// %(key)type substitution for Formatter.format
// Formatter.format ᠤᠨ %(key) ᠠᠯᠺᠠᠳᠠᠯ
std::string formatterSubstitute(cffi& a, const std::string& fmt, const value& record){
    std::string out;
    for (size_t i = 0; i < fmt.size(); ++i){
        char c = fmt[i];
        if (c != '%'){
            if (c == '%'){}
            out += c;
            continue;
        }
        if (i + 1 < fmt.size() && fmt[i + 1] == '%'){ out += '%'; ++i; continue; }
        if (i + 1 >= fmt.size() || fmt[i + 1] != '('){ out += c; continue; }
        size_t close = fmt.find(')', i + 2);
        if (close == std::string::npos){ out += c; continue; }
        std::string key = fmt.substr(i + 2, close - i - 2);
        size_t j = close + 1;
        while (j < fmt.size() && (fmt[j] == '-' || fmt[j] == '+' || fmt[j] == ' ' ||
                                  fmt[j] == '0' || fmt[j] == '#')) j++;
        while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) j++;
        if (j < fmt.size() && fmt[j] == '.'){
            j++;
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) j++;
        }
        char conv = j < fmt.size() ? fmt[j] : 's';
        std::string raw = recordStr(a, record, key.c_str());
        char buf[512];
        if (conv == 'd' || conv == 'i'){
            std::string spec = "%" + fmt.substr(close + 1, j - close) ;
            spec.back() = 'd';
            std::snprintf(buf, sizeof(buf), spec.c_str(),
                          static_cast<int>(std::atoll(raw.c_str())));
            out += buf;
        } else if (conv == 'f'){
            std::string spec = "%" + fmt.substr(close + 1, j - close);
            spec.back() = 'f';
            std::snprintf(buf, sizeof(buf), spec.c_str(), std::atof(raw.c_str()));
            out += buf;
        } else {
            out += raw;
        }
        i = j;
    }
    return out;
}

// 生成 asctime 时间串 (CPython: "2003-07-08 00:49:23,123")
// Build asctime string ("2003-07-08 00:49:23,123")
// asctime ᠴᠠᠭ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
std::string makeAsctime(const std::string& datefmt){
    std::time_t now = std::time(nullptr);
    std::tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char buf[64];
    std::string fmt = datefmt.empty() ? "%Y-%m-%d %H:%M:%S" : datefmt;
    std::strftime(buf, sizeof(buf), fmt.c_str(), &tmv);
    std::string out = buf;
    if (datefmt.empty()) out += ",000";
    return out;
}

// handler 输出一条记录 (按 stream 类型分发)
// Emit one record through the handler (dispatch by stream type)
// handler ᠠ᠋ᠴᠠ ᠪᠢᠴᠢᠭ ᠢ ᠭᠠᠷᠤᠯᠭᠠᠬᠤ
void handlerEmit(cffi& a, const value& handler, const std::string& text){
    std::string target = a.vm->toString(a.attr(handler, "_stream"));
    if (target == "stdout"){
        std::cout << text << "\n";
        std::cout.flush();
    } else if (target.rfind("file:", 0) == 0){
        std::string path = target.substr(5);
        FILE* fp = std::fopen(path.c_str(), "a");
        if (fp){
            std::fwrite(text.data(), 1, text.size(), fp);
            std::fwrite("\n", 1, 1, fp);
            std::fclose(fp);
        }
    } else {
        std::cerr << text << "\n";
        std::cerr.flush();
    }
}

// 处理一条记录: formatter → emit
// Handle one record: formatter → emit
// ᠪᠢᠴᠢᠭ ᠢ ᠪᠣᠯᠪᠠᠰᠤᠷᠠᠭᠤᠯᠬᠤ
void handlerHandle(cffi& a, const value& handler, const value& record){
    int hlevel = handlerLevel(a, handler);
    int rlevel = std::atoi(recordStr(a, record, "levelno").c_str());
    if (rlevel < hlevel) return;
    value formatter = a.attr(handler, "formatter");
    std::string text;
    bool formatted = false;
    if (formatter.type == "native_object" && formatter.ptr){
        value result;
        if (cffiCallMethod(*a.vm, formatter, "format", {record}, result)){
            text = a.vm->toString(result);
            formatted = true;
        }
    }
    if (!formatted) text = recordStr(a, record, "message");
    handlerEmit(a, handler, text);
}

// 沿父链分发记录 (propagate)
// Dispatch record along the parent chain (propagate)
// ᠡᠴᠡᠭᠡ ᠰᠡᠯᠭᠡ ᠪᠡᠷ ᠪᠢᠴᠢᠭ ᠢ ᠳᠡᠯᠭᠡᠷᠡᠭᠦᠯᠬᠦ
void callHandlers(cffi& a, LoggerState* logger, const value& record){
    int found = 0;
    for (LoggerState* cur = logger; cur; cur = cur->parent){
        for (auto& h : cur->handlers){
            handlerHandle(a, h, record);
            found++;
        }
        if (!cur->propagate) break;
    }
    if (found == 0){
        // last resort: 直接输出 message 到 stderr (WARNING+)
        // Last resort: print bare message to stderr (WARNING+)
        // ᠰᠡᠭᠦᠯ ᠦᠨ ᠠᠷᠭ᠎ᠠ: stderr ᠲᠤ ᠭᠠᠷᠤᠯᠭ᠎ᠠ
        int rlevel = std::atoi(recordStr(a, record, "levelno").c_str());
        if (rlevel >= LVL_WARNING){
            std::cerr << recordStr(a, record, "message") << "\n";
            std::cerr.flush();
        }
    }
}

// logger 有效级别
// Effective level of a logger
// logger ᠦᠨ ᠦᠨᠡᠨ ᠲᠥᠪᠰᠢᠨ
int effectiveLevel(LoggerState* logger){
    for (LoggerState* cur = logger; cur; cur = cur->parent){
        if (cur->level != 0) return cur->level;
    }
    return LVL_WARNING;
}

// 从实例取得 LoggerState
// Get LoggerState from an instance
// ᠶᠢᠨᠢᠰ ᠡᠴᠡ LoggerState ᠢ ᠠᠪᠬᠤ
LoggerState* stateOf(cffi& a, const value& self){
    value nameVal = a.attr(self, "name");
    return loggerFor(*a.vm, a.vm->toString(nameVal));
}

// 通用 log 实现
// Common log implementation
// ᠨᠡᠶᠢᠲᠡ ᠶᠢᠨ log ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value doLog(cffi& a, LoggerState* logger, int level, const std::string& msg,
            size_t extraStart){
    if (level < effectiveLevel(logger)) return a.none();
    std::vector<value> extras;
    for (size_t i = extraStart; i < a.argc(); ++i) extras.push_back(a.arg(i));
    value record = makeRecord(a, logger->name, level, msg, extras);
    callHandlers(a, logger, record);
    return a.none();
}

// ---- Logger 方法 / Logger methods / Logger ᠠᠷᠭ᠎ᠠ ----

value loggerCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    std::string name = a.argc() > 0 ? a.arg_str(0) : "root";
    LoggerState* st = loggerFor(*a.vm, name);
    inst->attributes["name"] = a.mkstr(name);
    inst->attributes["level"] = a.mkint(st->level);
    inst->attributes["propagate"] = a.mkbool(true);
    inst->attributes["handlers"] = st->pyHandlers;
    return obj;
}

value loggerSetLevel(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    st->level = levelFrom(a, a.arg(0));
    a.setattr("level", a.arg(0));
    return a.none();
}

value loggerGetEffectiveLevel(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    return a.mkint(effectiveLevel(st));
}

value loggerIsEnabledFor(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    int level = levelFrom(a, a.arg(0));
    return a.mkbool(level >= effectiveLevel(st));
}

value loggerAddHandler(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    st->handlers.push_back(a.arg(0));
    if (st->pyHandlers.type == "list" && st->pyHandlers.ptr){
        auto* items = static_cast<std::vector<value>*>(st->pyHandlers.ptr);
        items->push_back(a.arg(0));
    }
    return a.none();
}

value loggerRemoveHandler(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    value target = a.arg(0);
    for (auto it = st->handlers.begin(); it != st->handlers.end(); ++it){
        if (a.vm->valuesEqual(*it, target)){ st->handlers.erase(it); break; }
    }
    if (st->pyHandlers.type == "list" && st->pyHandlers.ptr){
        auto* items = static_cast<std::vector<value>*>(st->pyHandlers.ptr);
        for (auto it = items->begin(); it != items->end(); ++it){
            if (a.vm->valuesEqual(*it, target)){ items->erase(it); break; }
        }
    }
    return a.none();
}

value loggerHasHandlers(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    for (LoggerState* cur = st; cur; cur = cur->parent){
        if (!cur->handlers.empty()) return a.mkbool(true);
        if (!cur->propagate) break;
    }
    return a.mkbool(false);
}

value loggerLogCommon(cffi& a, int fixedLevel, bool levelArg){
    LoggerState* st = stateOf(a, a.self());
    int level = fixedLevel;
    size_t msgIndex = 0;
    if (levelArg){
        level = levelFrom(a, a.arg(0));
        msgIndex = 1;
    }
    std::string msg = a.arg_str(msgIndex);
    return doLog(a, st, level, msg, msgIndex + 1);
}

value loggerDebug(cffi& a){ return loggerLogCommon(a, LVL_DEBUG, false); }
value loggerInfo(cffi& a){ return loggerLogCommon(a, LVL_INFO, false); }
value loggerWarning(cffi& a){ return loggerLogCommon(a, LVL_WARNING, false); }
value loggerError(cffi& a){ return loggerLogCommon(a, LVL_ERROR, false); }
value loggerCritical(cffi& a){ return loggerLogCommon(a, LVL_CRITICAL, false); }
value loggerLog(cffi& a){ return loggerLogCommon(a, 0, true); }
value loggerException(cffi& a){ return loggerLogCommon(a, LVL_ERROR, false); }

value loggerRepr(cffi& a){
    LoggerState* st = stateOf(a, a.self());
    std::string out = "<Logger " + st->name + " (" + levelName(effectiveLevel(st)) + ")>";
    return a.mkstr(out);
}

// ---- Handler 方法 / Handler methods / Handler ᠠᠷᠭ᠎ᠠ ----

value handlerCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["level"] = a.mkint(0);
    inst->attributes["formatter"] = a.none();
    inst->attributes["_stream"] = a.mkstr("stderr");
    return obj;
}

value streamHandlerCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    std::string stream = "stderr";
    if (a.has_kwarg("stream")){
        value s = a.kwarg("stream");
        stream = a.vm->toString(s);
        if (stream.find("stderr") != std::string::npos) stream = "stderr";
        else if (stream.find("stdout") != std::string::npos) stream = "stdout";
    } else if (a.argc() > 0){
        std::string s = a.arg_str(0);
        if (s.find("stdout") != std::string::npos) stream = "stdout";
    }
    inst->attributes["level"] = a.mkint(0);
    inst->attributes["formatter"] = a.none();
    inst->attributes["_stream"] = a.mkstr(stream);
    return obj;
}

value fileHandlerCtor(cffi& a){
    std::string filename = a.arg_str(0);
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["level"] = a.mkint(0);
    inst->attributes["formatter"] = a.none();
    inst->attributes["_stream"] = a.mkstr("file:" + filename);
    inst->attributes["filename"] = a.mkstr(filename);
    inst->attributes["mode"] = a.argc() > 1 ? a.arg(1) : a.mkstr("a");
    return obj;
}

value handlerSetLevel(cffi& a){
    a.setattr("level", a.mkint(levelFrom(a, a.arg(0))));
    return a.none();
}

value handlerSetFormatter(cffi& a){
    a.setattr("formatter", a.arg(0));
    return a.none();
}

value handlerHandleMethod(cffi& a){
    handlerHandle(a, a.self(), a.arg(0));
    return a.none();
}

value handlerEmitMethod(cffi& a){
    value record = a.arg(0);
    value formatter = a.attr("formatter");
    std::string text;
    if (formatter.type == "native_object" && formatter.ptr){
        value result;
        if (cffiCallMethod(*a.vm, formatter, "format", {record}, result)){
            text = a.vm->toString(result);
        }
    }
    if (text.empty()) text = recordStr(a, record, "message");
    handlerEmit(a, a.self(), text);
    return a.none();
}

value handlerFlush(cffi& a){ return a.none(); }

value handlerClose(cffi& a){ return a.none(); }

// ---- Formatter / Formatter / Formatter ----

value formatterCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    value fmt = a.argc() > 0 ? a.arg(0) : (a.has_kwarg("fmt") ? a.kwarg("fmt") : a.none());
    inst->attributes["_fmt"] = fmt;
    value datefmt = a.argc() > 1 ? a.arg(1)
                    : (a.has_kwarg("datefmt") ? a.kwarg("datefmt") : a.none());
    inst->attributes["_datefmt"] = datefmt;
    return obj;
}

value formatterFormat(cffi& a){
    value record = a.arg(0);
    value fmtVal = a.attr("_fmt");
    std::string fmt = fmtVal.type == "none" || fmtVal.type == ""
        ? "%(message)s" : a.vm->toString(fmtVal);
    // 先算 asctime (惰性, CPython 行为)
    // Compute asctime lazily (CPython behavior)
    // asctime ᠢ ᠲᠣᠳᠣᠷᠬᠠᠶᠢᠯᠠᠬᠤ
    if (fmt.find("%(asctime)") != std::string::npos){
        value datefmt = a.attr("_datefmt");
        std::string df = datefmt.type == "none" ? "" : a.vm->toString(datefmt);
        std::string asc = makeAsctime(df);
        if (record.type == "dict" && record.ptr){
            auto* items = static_cast<dict_t*>(record.ptr);
            for (auto& kv : *items){
                if (kv.first.type == "str" && a.vm->toString(kv.first) == "asctime"){
                    kv.second = a.mkstr(asc);
                }
            }
        }
    }
    return a.mkstr(formatterSubstitute(a, fmt, record));
}

value formatterFormatTime(cffi& a){
    value datefmt = a.attr("_datefmt");
    std::string df = datefmt.type == "none" ? "" : a.vm->toString(datefmt);
    return a.mkstr(makeAsctime(df));
}

// ---- 模块级函数 / module-level functions / ᠮᠣᠳᠤᠯᠢ ᠠᠷᠭ᠎ᠠ ----

value fnGetLogger(cffi& a){
    std::string name = a.argc() > 0 && a.arg(0).type != "none" ? a.arg_str(0) : "root";
    loggerFor(*a.vm, name);
    // 返回一个新的 Logger 实例包装
    // Return a new Logger instance wrapper
    // Logger ᠶᠢᠨᠢᠰ ᠢ ᠪᠤᠴᠠᠶ᠎ᠠ
    auto classIt = a.library->classes.find("Logger");
    value inst = a.vm->makeNativeObject(a.library, &classIt->second, nullptr);
    auto* ffiInst = static_cast<ffi_instance*>(inst.ptr);
    ffiInst->attributes["name"] = a.mkstr(name);
    LoggerState* st = loggerFor(*a.vm, name);
    ffiInst->attributes["level"] = a.mkint(st->level);
    ffiInst->attributes["propagate"] = a.mkbool(true);
    ffiInst->attributes["handlers"] = st->pyHandlers;
    return inst;
}

value fnRoot(cffi& a);

// 模块级 log 便捷函数
// Module-level log shortcuts
// ᠮᠣᠳᠤᠯᠢ ᠲᠣᠪᠴᠢᠶ᠎ᠠ
value moduleLogCommon(cffi& a, int fixedLevel, bool levelArg){
    LoggingState& st = *stateFor(*a.vm);
    LoggerState* root = loggerFor(*a.vm, "root");
    // CPython: 无 handler 时隐式 basicConfig
    // CPython: implicit basicConfig when no handlers
    // CPython: handler ᠦᠭᠡᠢ ᠪᠣᠯ basicConfig
    if (st.root->handlers.empty()){
        // 构造默认 StreamHandler + Formatter
        // Build default StreamHandler + Formatter
        // ᠠᠩᠬᠠᠷ StreamHandler + Formatter
        auto& classes = a.library->classes;
        value handler = a.vm->makeNativeObject(a.library, &classes.at("StreamHandler"), nullptr);
        auto* hi = static_cast<ffi_instance*>(handler.ptr);
        hi->attributes["level"] = a.mkint(0);
        value formatter = a.vm->makeNativeObject(a.library, &classes.at("Formatter"), nullptr);
        auto* fi = static_cast<ffi_instance*>(formatter.ptr);
        fi->attributes["_fmt"] = a.mkstr("%(levelname)s:%(name)s:%(message)s");
        fi->attributes["_datefmt"] = a.none();
        hi->attributes["formatter"] = formatter;
        hi->attributes["_stream"] = a.mkstr("stderr");
        st.root->handlers.push_back(handler);
    }
    int level = fixedLevel;
    size_t msgIndex = 0;
    if (levelArg){ level = levelFrom(a, a.arg(0)); msgIndex = 1; }
    std::string msg = a.arg_str(msgIndex);
    std::vector<value> extras;
    for (size_t i = msgIndex + 1; i < a.argc(); ++i) extras.push_back(a.arg(i));
    if (level < effectiveLevel(root)) return a.none();
    value record = makeRecord(a, "root", level, msg, extras);
    callHandlers(a, root, record);
    return a.none();
}

value fnDebug(cffi& a){ return moduleLogCommon(a, LVL_DEBUG, false); }
value fnInfo(cffi& a){ return moduleLogCommon(a, LVL_INFO, false); }
value fnWarning(cffi& a){ return moduleLogCommon(a, LVL_WARNING, false); }
value fnError(cffi& a){ return moduleLogCommon(a, LVL_ERROR, false); }
value fnCritical(cffi& a){ return moduleLogCommon(a, LVL_CRITICAL, false); }
value fnLog(cffi& a){ return moduleLogCommon(a, 0, true); }
value fnException(cffi& a){ return moduleLogCommon(a, LVL_ERROR, false); }

// basicConfig(**kwargs): filename/format/level/datefmt/force
// basicConfig(**kwargs): filename/format/level/datefmt/force
// basicConfig(**kwargs)
value fnBasicConfig(cffi& a){
    LoggingState& st = *stateFor(*a.vm);
    LoggerState* root = loggerFor(*a.vm, "root");
    bool force = a.kwarg_bool("force", false);
    if (!st.root->handlers.empty() && !force) return a.none();
    if (force) st.root->handlers.clear();

    auto& classes = a.library->classes;
    bool useFile = a.has_kwarg("filename");
    value handler = a.vm->makeNativeObject(
        a.library, &classes.at(useFile ? "FileHandler" : "StreamHandler"), nullptr);
    auto* hi = static_cast<ffi_instance*>(handler.ptr);
    hi->attributes["level"] = a.mkint(0);
    if (useFile){
        std::string fn = a.kwarg_str("filename", "");
        hi->attributes["_stream"] = a.mkstr("file:" + fn);
        hi->attributes["filename"] = a.mkstr(fn);
        hi->attributes["mode"] = a.mkstr("a");
    } else {
        hi->attributes["_stream"] = a.mkstr("stderr");
    }
    std::string fmt = a.kwarg_str("format", "%(levelname)s:%(name)s:%(message)s");
    value formatter = a.vm->makeNativeObject(a.library, &classes.at("Formatter"), nullptr);
    auto* fi = static_cast<ffi_instance*>(formatter.ptr);
    fi->attributes["_fmt"] = a.mkstr(fmt);
    if (a.has_kwarg("datefmt")) fi->attributes["_datefmt"] = a.kwarg("datefmt");
    else fi->attributes["_datefmt"] = a.none();
    hi->attributes["formatter"] = formatter;
    st.root->handlers.push_back(handler);

    if (a.has_kwarg("level")){
        root->level = levelFrom(a, a.kwarg("level"));
    }
    return a.none();
}

value fnGetLevelName(cffi& a){
    value v = a.arg(0);
    if (v.type == "int") return a.mkstr(levelName(static_cast<int>(a.vm->toInt(v))));
    return a.mkstr("Level " + a.vm->toString(v));
}

value fnAddLevelName(cffi& a){ return a.none(); }

value fnDisable(cffi& a){ return a.none(); }

value fnShutdown(cffi& a){
    LoggingState& st = *stateFor(*a.vm);
    if (st.root) st.root->handlers.clear();
    return a.none();
}

value fnMakeLogRecord(cffi& a){
    return makeRecord(a, "", 0, "", {});
}

value dataRoot(cffi& a){
    loggerFor(*a.vm, "root");
    auto classIt = a.library->classes.find("Logger");
    value inst = a.vm->makeNativeObject(a.library, &classIt->second, nullptr);
    auto* ffiInst = static_cast<ffi_instance*>(inst.ptr);
    ffiInst->attributes["name"] = a.mkstr("root");
    ffiInst->attributes["level"] = a.mkint(loggerFor(*a.vm, "root")->level);
    ffiInst->attributes["propagate"] = a.mkbool(true);
    return inst;
}

value dataBASIC_FORMAT(cffi& a){
    return a.mkstr("%(levelname)s:%(name)s:%(message)s");
}

} // namespace

// 清理指定 vm 的 logging 状态
// Drop a vm's logging state
// logging ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠷᠢᠯᠭᠠᠬᠤ
void minipyCleanupLogging(vm& v){
    auto it = registry().find(&v);
    if (it == registry().end()) return;
    LoggingState* st = it->second;
    if (st){
        for (auto& kv : st->loggers) delete kv.second;
        delete st->root;
    }
    registry().erase(it);
}

// cffi logging lib
// cffi logging lib
// cffi logging lib
ffilib cffi_logging_lib(){
    ffi_class logger = cffi_class(
        "Logger", (void*)loggerCtor,
        {
            {"setLevel", (void*)loggerSetLevel},
            {"getEffectiveLevel", (void*)loggerGetEffectiveLevel},
            {"isEnabledFor", (void*)loggerIsEnabledFor},
            {"addHandler", (void*)loggerAddHandler},
            {"removeHandler", (void*)loggerRemoveHandler},
            {"hasHandlers", (void*)loggerHasHandlers},
            {"debug", (void*)loggerDebug},
            {"info", (void*)loggerInfo},
            {"warning", (void*)loggerWarning},
            {"warn", (void*)loggerWarning},
            {"error", (void*)loggerError},
            {"critical", (void*)loggerCritical},
            {"fatal", (void*)loggerCritical},
            {"log", (void*)loggerLog},
            {"exception", (void*)loggerException},
            {"__repr__", (void*)loggerRepr}
        });

    ffi_class handler = cffi_class(
        "Handler", (void*)handlerCtor,
        {
            {"setLevel", (void*)handlerSetLevel},
            {"setFormatter", (void*)handlerSetFormatter},
            {"handle", (void*)handlerHandleMethod},
            {"emit", (void*)handlerEmitMethod},
            {"flush", (void*)handlerFlush},
            {"close", (void*)handlerClose}
        });

    ffi_class streamHandler = cffi_class(
        "StreamHandler", (void*)streamHandlerCtor,
        {
            {"setLevel", (void*)handlerSetLevel},
            {"setFormatter", (void*)handlerSetFormatter},
            {"handle", (void*)handlerHandleMethod},
            {"emit", (void*)handlerEmitMethod},
            {"flush", (void*)handlerFlush},
            {"close", (void*)handlerClose}
        });

    ffi_class fileHandler = cffi_class(
        "FileHandler", (void*)fileHandlerCtor,
        {
            {"setLevel", (void*)handlerSetLevel},
            {"setFormatter", (void*)handlerSetFormatter},
            {"handle", (void*)handlerHandleMethod},
            {"emit", (void*)handlerEmitMethod},
            {"flush", (void*)handlerFlush},
            {"close", (void*)handlerClose}
        });

    ffi_class formatter = cffi_class(
        "Formatter", (void*)formatterCtor,
        {
            {"format", (void*)formatterFormat},
            {"formatTime", (void*)formatterFormatTime}
        });

    return cffi_lib(
        "logging",
        {
            {"getLogger", (void*)fnGetLogger},
            {"basicConfig", (void*)fnBasicConfig},
            {"debug", (void*)fnDebug},
            {"info", (void*)fnInfo},
            {"warning", (void*)fnWarning},
            {"warn", (void*)fnWarning},
            {"error", (void*)fnError},
            {"critical", (void*)fnCritical},
            {"fatal", (void*)fnCritical},
            {"log", (void*)fnLog},
            {"exception", (void*)fnException},
            {"getLevelName", (void*)fnGetLevelName},
            {"addLevelName", (void*)fnAddLevelName},
            {"disable", (void*)fnDisable},
            {"shutdown", (void*)fnShutdown},
            {"makeLogRecord", (void*)fnMakeLogRecord}
        },
        {
            {"DEBUG", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_DEBUG); }},
            {"INFO", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_INFO); }},
            {"WARNING", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_WARNING); }},
            {"WARN", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_WARNING); }},
            {"ERROR", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_ERROR); }},
            {"CRITICAL", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_CRITICAL); }},
            {"FATAL", (void*)+[] (cffi& a) -> value { return a.mkint(LVL_CRITICAL); }},
            {"NOTSET", (void*)+[] (cffi& a) -> value { return a.mkint(0); }},
            {"BASIC_FORMAT", (void*)dataBASIC_FORMAT},
            {"root", (void*)dataRoot}
        },
        {
            {"Logger", logger},
            {"Handler", handler},
            {"StreamHandler", streamHandler},
            {"FileHandler", fileHandler},
            {"Formatter", formatter}
        });
}

} // namespace minipy
