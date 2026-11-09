#include "../cffi.hpp"

#include <cstdio>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 警告过滤器
// Warning filter entry
// ᠰᠡᠷᠡᠭᠡᠯ ᠦᠨ ᠰᠢᠯᠵᠢᠭᠦᠷᠢᠶ᠎ᠡ
struct Filter{
    std::string action;
    std::string message;
    std::string category;
    std::string module;
    long long lineno = 0;
};

// catch_warnings 状态
// catch_warnings state
// catch_warnings ᠪᠠᠶᠢᠳᠠᠯ
struct CatchState{
    std::vector<Filter> saved;
    bool record = false;
    bool active = false;
};

// 每个 VM 的 warnings 状态
// Per-VM warnings state
// warnings ᠪᠠᠶᠢᠳᠠᠯ
struct WarnState{
    std::vector<Filter> filters;
    std::set<std::string> onceSeen;
    std::set<std::string> defaultSeen;
    std::set<std::string> moduleSeen;
    value recorder;
};

std::map<vm*, WarnState>& states(){
    static std::map<vm*, WarnState> table;
    return table;
}

WarnState& stateOf(vm* v){
    auto it = states().find(v);
    if (it != states().end()) return it->second;
    WarnState fresh;
    Filter def;
    def.action = "default";
    fresh.filters.push_back(def);
    return states()[v] = fresh;
}

// 取警告类别名
// Get warning category name
// ᠰᠡᠷᠡᠭᠡᠯ ᠦᠨ ᠲᠥᠷᠥᠯ ᠨᠡᠷᠡ
std::string categoryName(vm& v, const value& category){
    if (category.type == "native_object" && category.ptr){
        auto* inst = static_cast<ffi_instance*>(category.ptr);
        if (!inst->typeName.empty()) return inst->typeName;
        if (inst->klass) return inst->klass->name;
    }
    if (category.type == "class" && category.ptr){
        auto* klass = static_cast<class_t*>(category.ptr);
        return klass ? klass->name : "Warning";
    }
    std::string name = v.toString(category);
    if (name.size() > 2 && name.front() == '<' && name.back() == '>')
        name = name.substr(1, name.size() - 2);
    if (name.find("'") != std::string::npos){
        size_t a = name.find('\'');
        size_t b = name.find('\'', a + 1);
        if (a != std::string::npos && b != std::string::npos && b > a + 1)
            return name.substr(a + 1, b - a - 1);
    }
    return name.empty() ? "Warning" : name;
}

// 类别是否匹配过滤器
// Whether a category matches a filter category
// ᠲᠥᠷᠥᠯ ᠲᠣᠬᠢᠷᠠᠯᠳᠤ
bool categoryMatches(const std::string& filter, const std::string& actual){
    if (filter.empty() || filter == "Warning")
        return actual == "Warning" ||
               (actual.size() > 7 && actual.compare(actual.size() - 7, 7, "Warning") == 0);
    if (filter == "Exception" || filter == "BaseException") return true;
    return filter == actual;
}

// 正则/字面量匹配
// Regex-or-literal match
// ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠤᠯ
bool textMatches(const std::string& pattern, const std::string& text){
    if (pattern.empty()) return true;
    try{
        std::regex rx(pattern);
        return std::regex_search(text, rx);
    } catch (...){}
    return text.find(pattern) != std::string::npos;
}

// 读取类别参数 (缺省 UserWarning)
// Read the category argument (default UserWarning)
// ᠲᠥᠷᠥᠯ ᠬᠠᠯᠠᠬᠤ
value defaultCategory(cffi& a){
    value result = a.none();
    if (a.library && cffiCallData(*a.library, "UserWarning", *a.vm, result))
        return result;
    return a.vm->makeDynamicObject("UserWarning", nullptr, nullptr, nullptr);
}

// 创建一条警告记录对象
// Build a WarningMessage object
// ᠰᠡᠷᠡᠭᠡᠯ ᠦᠨ ᠮᠡᠳᠦᠭᠡᠯ
value makeRecord(vm& v, const ffilib* library, const ffi_class* klass,
                 const std::string& message, const value& category,
                 const std::string& filename, long long lineno){
    value rec = v.makeNativeObject(library, klass, nullptr);
    if (rec.type == "native_object" && rec.ptr){
        auto* inst = static_cast<ffi_instance*>(rec.ptr);
        inst->attributes["message"] = v.makeString(message);
        inst->attributes["category"] = category;
        inst->attributes["filename"] = v.makeString(filename);
        inst->attributes["lineno"] = v.makeInt(lineno);
    }
    return rec;
}

// 输出警告到 stderr
// Print a warning to stderr
// ᠰᠡᠷᠡᠭᠡᠯ ᠬᠡᠪᠯᠡᠬᠦ
void printWarning(const std::string& message, const std::string& category,
                  const std::string& filename, long long lineno){
    std::string line = filename + ":" + std::to_string(lineno) + ": " +
                       category + ": " + message + "\n";
    std::fwrite(line.data(), 1, line.size(), stderr);
}

// 执行 warn_explicit 的核心逻辑
// Core logic of warn_explicit
// warn_explicit ᠶᠢᠨ ᠭᠣᠯ ᠯᠣᠭᠢᠬᠣ
value warnCore(cffi& a, const std::string& message, const value& category,
               const std::string& filename, long long lineno){
    WarnState& st = stateOf(a.vm);
    std::string catName = categoryName(*a.vm, category);
    Filter* matched = nullptr;
    for (auto& filter : st.filters){
        if (!categoryMatches(filter.category, catName)) continue;
        if (!textMatches(filter.message, message)) continue;
        if (!filter.module.empty() && !textMatches(filter.module, filename)) continue;
        if (filter.lineno != 0 && filter.lineno != lineno) continue;
        matched = &filter;
        break;
    }
    std::string action = matched ? matched->action : "default";

    if (action == "error"){
        a.vm->raiseError(catName, message);
        return a.none();
    }
    if (action == "ignore") return a.none();

    if (action == "once"){
        std::string key = catName + "\x01" + message;
        if (st.onceSeen.count(key)) return a.none();
        st.onceSeen.insert(key);
    } else if (action == "module"){
        std::string key = filename + "\x01" + catName;
        if (st.moduleSeen.count(key)) return a.none();
        st.moduleSeen.insert(key);
    } else if (action == "default"){
        std::string key = catName + "\x01" + message + "\x01" +
                          std::to_string(lineno) + "\x01" + filename;
        if (st.defaultSeen.count(key)) return a.none();
        st.defaultSeen.insert(key);
    }

    if (st.recorder.type == "list" && st.recorder.ptr){
        value rec = makeRecord(*a.vm, a.library, nullptr, message, category,
                               filename, lineno);
        auto* items = static_cast<std::vector<value>*>(st.recorder.ptr);
        items->push_back(rec);
        return a.none();
    }

    printWarning(message, catName, filename, lineno);
    return a.none();
}

// warnings.warn
// warnings.warn
// warnings.warn
static value fnWarn(cffi& a){
    std::string message = a.arg_str(0);
    value category = a.argc() > 1 && a.arg(1).type != "none"
                   ? a.arg(1) : defaultCategory(a);
    long long lineno = a.argc() > 2 && a.arg(2).type != "none" ? a.arg_int(2) : 1;
    return warnCore(a, message, category, "<unknown>", lineno);
}

// warnings.warn_explicit
// warnings.warn_explicit
// warnings.warn_explicit
static value fnWarnExplicit(cffi& a){
    std::string message = a.arg_str(0);
    value category = a.argc() > 1 && a.arg(1).type != "none"
                   ? a.arg(1) : defaultCategory(a);
    std::string filename = a.arg_str(2);
    long long lineno = a.argc() > 3 ? a.arg_int(3) : 1;
    return warnCore(a, message, category, filename, lineno);
}

// warnings.showwarning
// warnings.showwarning
// warnings.showwarning
static value fnShowwarning(cffi& a){
    std::string message = a.arg_str(0);
    std::string filename = a.argc() > 2 ? a.arg_str(2) : "<unknown>";
    long long lineno = a.argc() > 3 ? a.arg_int(3) : 0;
    std::string category = categoryName(*a.vm, a.arg(1));
    printWarning(message, category, filename, lineno);
    return a.none();
}

// warnings.formatwarning
// warnings.formatwarning
// warnings.formatwarning
static value fnFormatwarning(cffi& a){
    std::string message = a.arg_str(0);
    std::string filename = a.argc() > 2 ? a.arg_str(2) : "<unknown>";
    long long lineno = a.argc() > 3 ? a.arg_int(3) : 0;
    std::string category = categoryName(*a.vm, a.arg(1));
    return a.mkstr(filename + ":" + std::to_string(lineno) + ": " +
                   category + ": " + message + "\n");
}

// 过滤器校验
// Validate a filter action
// ᠰᠢᠯᠵᠢᠭᠦᠷᠢᠶ᠎ᠡ ᠰᠢᠯᠭᠠᠬᠤ
bool validAction(const std::string& action){
    return action == "error" || action == "ignore" || action == "always" ||
           action == "all" || action == "default" || action == "module" ||
           action == "once";
}

// warnings.filterwarnings
// warnings.filterwarnings
// warnings.filterwarnings
static value fnFilterwarnings(cffi& a){
    Filter filter;
    filter.action = a.arg_str(0);
    if (!validAction(filter.action)){
        a.vm->raiseError("ValueError", "invalid action: '" + filter.action + "'");
        return a.none();
    }
    if (filter.action == "all") filter.action = "always";
    if (a.argc() > 1 && a.arg(1).type != "none") filter.message = a.arg_str(1);
    if (a.argc() > 2 && a.arg(2).type != "none")
        filter.category = categoryName(*a.vm, a.arg(2));
    if (a.argc() > 3 && a.arg(3).type != "none") filter.module = a.arg_str(3);
    if (a.argc() > 4) filter.lineno = a.arg_int(4);
    bool append = a.argc() > 5 ? a.arg_bool(5) : false;
    WarnState& st = stateOf(a.vm);
    if (append) st.filters.push_back(filter);
    else st.filters.insert(st.filters.begin(), filter);
    return a.none();
}

// warnings.simplefilter
// warnings.simplefilter
// warnings.simplefilter
static value fnSimplefilter(cffi& a){
    Filter filter;
    filter.action = a.arg_str(0);
    if (!validAction(filter.action)){
        a.vm->raiseError("ValueError", "invalid action: '" + filter.action + "'");
        return a.none();
    }
    if (filter.action == "all") filter.action = "always";
    if (a.argc() > 1 && a.arg(1).type != "none")
        filter.category = categoryName(*a.vm, a.arg(1));
    if (a.argc() > 2) filter.lineno = a.arg_int(2);
    bool append = a.argc() > 3 ? a.arg_bool(3) : false;
    WarnState& st = stateOf(a.vm);
    if (append) st.filters.push_back(filter);
    else st.filters.insert(st.filters.begin(), filter);
    return a.none();
}

// warnings.resetwarnings
// warnings.resetwarnings
// warnings.resetwarnings
static value fnResetwarnings(cffi& a){
    WarnState& st = stateOf(a.vm);
    st.filters.clear();
    Filter def;
    def.action = "default";
    st.filters.push_back(def);
    st.onceSeen.clear();
    st.defaultSeen.clear();
    st.moduleSeen.clear();
    return a.none();
}

// catch_warnings 构造
// catch_warnings constructor
// catch_warnings ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnCatchCtor(cffi& a){
    auto* state = new CatchState();
    state->record = a.has_kwarg("record") ? a.kwarg_bool("record", false)
                                         : (a.argc() > 0 ? a.arg_bool(0) : false);
    value obj = a.mkobject(state);
    return obj;
}

// catch_warnings.__enter__
// catch_warnings.__enter__
// catch_warnings.__enter__
static value fnCatchEnter(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.none();
    auto* state = static_cast<CatchState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!state) return a.none();
    WarnState& st = stateOf(a.vm);
    state->saved = st.filters;
    state->active = true;
    if (state->record){
        st.recorder = a.vm->makeList({});
        return st.recorder;
    }
    return a.none();
}

// catch_warnings.__exit__
// catch_warnings.__exit__
// catch_warnings.__exit__
static value fnCatchExit(cffi& a){
    value self = a.self();
    if (self.type != "native_object" || !self.ptr) return a.mkbool(false);
    auto* state = static_cast<CatchState*>(static_cast<ffi_instance*>(self.ptr)->handle);
    if (!state) return a.mkbool(false);
    WarnState& st = stateOf(a.vm);
    st.filters = state->saved;
    st.recorder = a.none();
    state->active = false;
    return a.mkbool(false);
}

// 生成动态类别对象
// Build a dynamic category object
// ᠳ᠋ᠶᠨᠠᠮᠢᠭ ᠲᠥᠷᠥᠯ
static value categoryData(cffi& a, const std::string& name){
    value v = a.vm->makeDynamicObject(name, nullptr, nullptr, nullptr);
    if (v.type == "native_object" && v.ptr)
        static_cast<ffi_instance*>(v.ptr)->isTypeObject = true;
    return v;
}

static value dataWarning(cffi& a){ return categoryData(a, "Warning"); }
static value dataUserWarning(cffi& a){ return categoryData(a, "UserWarning"); }
static value dataDeprecationWarning(cffi& a){ return categoryData(a, "DeprecationWarning"); }
static value dataSyntaxWarning(cffi& a){ return categoryData(a, "SyntaxWarning"); }
static value dataRuntimeWarning(cffi& a){ return categoryData(a, "RuntimeWarning"); }
static value dataFutureWarning(cffi& a){ return categoryData(a, "FutureWarning"); }
static value dataPendingDeprecationWarning(cffi& a){ return categoryData(a, "PendingDeprecationWarning"); }
static value dataImportWarning(cffi& a){ return categoryData(a, "ImportWarning"); }
static value dataUnicodeWarning(cffi& a){ return categoryData(a, "UnicodeWarning"); }
static value dataBytesWarning(cffi& a){ return categoryData(a, "BytesWarning"); }
static value dataResourceWarning(cffi& a){ return categoryData(a, "ResourceWarning"); }
static value dataEncodingWarning(cffi& a){ return categoryData(a, "EncodingWarning"); }

}

// cffi warnings lib
// cffi warnings lib
// cffi warnings lib
ffilib cffi_warnings_lib(){
    ffi_class catchClass;
    catchClass.name = "catch_warnings";
    catchClass.constructor = (void*)fnCatchCtor;
    catchClass.methods["__enter__"] = (void*)fnCatchEnter;
    catchClass.methods["__exit__"] = (void*)fnCatchExit;

    return cffi_lib(
        "warnings",
        {
            {"warn", (void*)fnWarn},
            {"warn_explicit", (void*)fnWarnExplicit},
            {"showwarning", (void*)fnShowwarning},
            {"formatwarning", (void*)fnFormatwarning},
            {"filterwarnings", (void*)fnFilterwarnings},
            {"simplefilter", (void*)fnSimplefilter},
            {"resetwarnings", (void*)fnResetwarnings}
        },
        {
            {"Warning", (void*)dataWarning},
            {"UserWarning", (void*)dataUserWarning},
            {"DeprecationWarning", (void*)dataDeprecationWarning},
            {"SyntaxWarning", (void*)dataSyntaxWarning},
            {"RuntimeWarning", (void*)dataRuntimeWarning},
            {"FutureWarning", (void*)dataFutureWarning},
            {"PendingDeprecationWarning", (void*)dataPendingDeprecationWarning},
            {"ImportWarning", (void*)dataImportWarning},
            {"UnicodeWarning", (void*)dataUnicodeWarning},
            {"BytesWarning", (void*)dataBytesWarning},
            {"ResourceWarning", (void*)dataResourceWarning},
            {"EncodingWarning", (void*)dataEncodingWarning}
        },
        {
            {"catch_warnings", catchClass}
        });
}

// 清理某个 VM 的 warnings 状态
// Drop a VM's warnings state
// warnings ᠪᠠᠶᠢᠮᠠᠭ᠎ᠠ
void minipyCleanupWarnings(vm& v){
    states().erase(&v);
}

}
