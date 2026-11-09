#include "../cffi.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 上下文数据
// Context data
// ᠬᠠᠮ ᠪᠠᠶᠢᠳᠠᠯ
struct ContextData{
    std::map<void*, value> vars;
};

// ContextVar 状态
// ContextVar state
// ContextVar ᠪᠠᠶᠢᠳᠠᠯ
struct VarState{
    std::string name;
    bool hasDefault = false;
    value defaultValue;
};

// Token 状态
// Token state
// Token ᠪᠠᠶᠢᠳᠠᠯ
struct TokenState{
    void* var = nullptr;
    bool hadValue = false;
    value oldValue;
    std::shared_ptr<ContextData> context;
    bool used = false;
};

// 每个 VM 的当前上下文
// Current context per VM
// ᠣᠳᠣᠮᠤ ᠬᠠᠮ ᠪᠠᠶᠢᠳᠠᠯ
std::map<vm*, std::shared_ptr<ContextData>>& currentContexts(){
    static std::map<vm*, std::shared_ptr<ContextData>> table;
    return table;
}

std::shared_ptr<ContextData> currentContext(vm* v){
    auto& table = currentContexts();
    auto it = table.find(v);
    if (it != table.end()) return it->second;
    auto ctx = std::make_shared<ContextData>();
    table[v] = ctx;
    return ctx;
}

// Token.MISSING 标记 (每个 VM 一个实例)
// Token.MISSING sentinel (one instance per VM)
// Token.MISSING ᠲᠡᠮᠳᠡᠭ
std::map<vm*, value>& missingSentinels(){
    static std::map<vm*, value> table;
    return table;
}

// 从 library 找类
// Find a class in the library
// ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠡᠴᠡ ᠲᠥᠷᠥᠯ ᠬᠠᠯᠠᠬᠤ
const ffi_class* findClass(const ffilib* lib, const std::string& name){
    if (!lib) return nullptr;
    auto it = lib->classes.find(name);
    return it == lib->classes.end() ? nullptr : &it->second;
}

// 取 Token.MISSING 标记对象
// Get the Token.MISSING sentinel
// Token.MISSING ᠲᠡᠮᠳᠡᠭ ᠬᠠᠯᠠᠬᠤ
value sentinelMissing(cffi& a){
    value& slot = missingSentinels()[a.vm];
    if (slot.type != "native_object" || !slot.ptr)
        slot = a.vm->makeNativeObject(a.library, findClass(a.library, "_TokenMissing"), nullptr);
    return slot;
}

// _TokenMissing.__str__ / __repr__
// _TokenMissing.__str__ / __repr__
static value fnMissingText(cffi& a){
    return a.mkstr("<Token.MISSING>");
}

// data Token.MISSING
// data Token.MISSING
static value dataMissing(cffi& a){
    return sentinelMissing(a);
}


// 取 VarState
// Get VarState from value
// VarState ᠬᠠᠯᠠᠬᠤ
VarState* varState(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    return static_cast<VarState*>(static_cast<ffi_instance*>(v.ptr)->handle);
}

// 取 ContextData
// Get ContextData from Context object
// ContextData ᠬᠠᠯᠠᠬᠤ
std::shared_ptr<ContextData> contextData(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* state = static_cast<std::shared_ptr<ContextData>*>(
        static_cast<ffi_instance*>(v.ptr)->handle);
    return state ? *state : nullptr;
}

// 生成 Token 对象
// Build a Token object
// Token ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeToken(cffi& a, TokenState* state, const value& var){
    value obj = a.vm->makeNativeObject(a.library, findClass(a.library, "Token"), state);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["var"] = var;
        inst->attributes["old_value"] = state->hadValue ? state->oldValue : sentinelMissing(a);
    }
    return obj;
}

// ContextVar 构造
// ContextVar constructor
// ContextVar ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnVarCtor(cffi& a){
    auto* state = new VarState();
    state->name = a.arg_str(0);
    if (a.has_kwarg("default")){
        state->hasDefault = true;
        state->defaultValue = a.kwarg("default");
    }
    value obj = a.mkobject(state);
    if (obj.type == "native_object" && obj.ptr)
        static_cast<ffi_instance*>(obj.ptr)->attributes["name"] = a.mkstr(state->name);
    return obj;
}

// ContextVar.get
// ContextVar.get
// ContextVar.get
static value fnVarGet(cffi& a){
    value var = a.self();
    VarState* state = varState(var);
    auto ctx = currentContext(a.vm);
    auto it = ctx->vars.find(state);
    if (it != ctx->vars.end()) return it->second;
    if (a.argc() > 0) return a.arg(0);
    if (state && state->hasDefault) return state->defaultValue;
    a.vm->raiseError("LookupError", state ? state->name : "<ContextVar>");
    return a.none();
}

// ContextVar.set
// ContextVar.set
// ContextVar.set
static value fnVarSet(cffi& a){
    value var = a.self();
    VarState* state = varState(var);
    auto ctx = currentContext(a.vm);
    auto* token = new TokenState();
    token->var = state;
    token->context = ctx;
    auto it = ctx->vars.find(state);
    if (it != ctx->vars.end()){
        token->hadValue = true;
        token->oldValue = it->second;
    }
    ctx->vars[state] = a.arg(0);
    return makeToken(a, token, var);
}

// ContextVar.reset
// ContextVar.reset
// ContextVar.reset
static value fnVarReset(cffi& a){
    value tokenValue = a.arg(0);
    if (tokenValue.type != "native_object" || !tokenValue.ptr){
        a.vm->raiseError("TypeError", "expected a Token");
        return a.none();
    }
    auto* token = static_cast<TokenState*>(static_cast<ffi_instance*>(tokenValue.ptr)->handle);
    if (!token) return a.none();
    if (token->used){
        a.vm->raiseError("RuntimeError", "Token has already been used once");
        return a.none();
    }
    auto ctx = currentContext(a.vm);
    if (token->context && ctx != token->context){
        a.vm->raiseError("ValueError",
                         "Token was created by a different Context");
        return a.none();
    }
    if (token->hadValue) ctx->vars[token->var] = token->oldValue;
    else ctx->vars.erase(token->var);
    token->used = true;
    return a.none();
}

// Context 构造
// Context constructor
// Context ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnContextCtor(cffi& a){
    auto* state = new std::shared_ptr<ContextData>(std::make_shared<ContextData>());
    value obj = a.mkobject(state);
    return obj;
}

// Context.__getitem__
// Context.__getitem__
// Context.__getitem__
static value fnContextGetitem(cffi& a){
    auto ctx = contextData(a.self());
    VarState* var = varState(a.arg(0));
    if (ctx && var){
        auto it = ctx->vars.find(var);
        if (it != ctx->vars.end()) return it->second;
    }
    a.vm->raiseError("LookupError", var ? var->name : "<ContextVar>");
    return a.none();
}

// Context.get
// Context.get
// Context.get
static value fnContextGet(cffi& a){
    auto ctx = contextData(a.self());
    VarState* var = varState(a.arg(0));
    if (ctx && var){
        auto it = ctx->vars.find(var);
        if (it != ctx->vars.end()) return it->second;
    }
    return a.argc() > 1 ? a.arg(1) : a.none();
}

// Context.__contains__
// Context.__contains__
// Context.__contains__
static value fnContextContains(cffi& a){
    auto ctx = contextData(a.self());
    VarState* var = varState(a.arg(0));
    return a.mkbool(ctx && var && ctx->vars.count(var) > 0);
}

// Context.__len__
// Context.__len__
// Context.__len__
static value fnContextLen(cffi& a){
    auto ctx = contextData(a.self());
    return a.mkint(ctx ? static_cast<long long>(ctx->vars.size()) : 0);
}

// Context.copy
// Context.copy
// Context.copy
static value fnContextCopy(cffi& a){
    auto ctx = contextData(a.self());
    auto* fresh = new std::shared_ptr<ContextData>(std::make_shared<ContextData>());
    if (ctx) (*fresh)->vars = ctx->vars;
    value obj = a.vm->makeNativeObject(a.library, findClass(a.library, "Context"), fresh);
    return obj;
}

// Context.run
// Context.run
// Context.run
static value fnContextRun(cffi& a){
    auto ctx = contextData(a.self());
    value func = a.arg(0);
    std::vector<value> args;
    for (size_t i = 1; i < a.argc(); ++i) args.push_back(a.arg(i));
    auto& table = currentContexts();
    auto old = table[a.vm];
    table[a.vm] = ctx ? ctx : std::make_shared<ContextData>();
    value result = a.vm->callValue(func, args);
    table[a.vm] = old ? old : std::make_shared<ContextData>();
    return result;
}

// copy_context
// copy_context
// copy_context
static value fnCopyContext(cffi& a){
    auto ctx = currentContext(a.vm);
    auto* fresh = new std::shared_ptr<ContextData>(std::make_shared<ContextData>());
    if (ctx) (*fresh)->vars = ctx->vars;
    return a.vm->makeNativeObject(a.library, findClass(a.library, "Context"), fresh);
}

}

// cffi contextvars lib
// cffi contextvars lib
// cffi contextvars lib
ffilib cffi_contextvars_lib(){
    ffi_class varClass;
    varClass.name = "ContextVar";
    varClass.constructor = (void*)fnVarCtor;
    varClass.methods["get"] = (void*)fnVarGet;
    varClass.methods["set"] = (void*)fnVarSet;
    varClass.methods["reset"] = (void*)fnVarReset;

    ffi_class tokenClass;
    tokenClass.name = "Token";
    tokenClass.data["MISSING"] = (void*)dataMissing;

    ffi_class missingClass;
    missingClass.name = "_TokenMissing";
    missingClass.methods["__str__"] = (void*)fnMissingText;
    missingClass.methods["__repr__"] = (void*)fnMissingText;

    ffi_class contextClass;
    contextClass.name = "Context";
    contextClass.constructor = (void*)fnContextCtor;
    contextClass.methods["__getitem__"] = (void*)fnContextGetitem;
    contextClass.methods["get"] = (void*)fnContextGet;
    contextClass.methods["__contains__"] = (void*)fnContextContains;
    contextClass.methods["__len__"] = (void*)fnContextLen;
    contextClass.methods["copy"] = (void*)fnContextCopy;
    contextClass.methods["run"] = (void*)fnContextRun;

    return cffi_lib(
        "contextvars",
        {
            {"copy_context", (void*)fnCopyContext}
        },
        {},
        {
            {"ContextVar", varClass},
            {"Context", contextClass},
            {"Token", tokenClass},
            {"_TokenMissing", missingClass}
        });
}

// 清理某个 VM 的 contextvars 状态
// Drop a VM's contextvars state
// contextvars ᠪᠠᠶᠢᠮᠠᠭ᠎ᠠ
void minipyCleanupContextvars(vm& v){
    currentContexts().erase(&v);
    missingSentinels().erase(&v);
}

}
