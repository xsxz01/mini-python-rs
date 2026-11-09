#include "vmware.hpp"
#include "platform.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "cffi.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include <functional>
#include <complex>

namespace {
    using namespace minipy;

// 实例
// instance
// ᠶᠢᠨᠢᠰ
struct instance{
    class_t* klass = nullptr;
    dict_t* attributes = nullptr;

    value ownedAttributes;

    value container;
    ~instance(){  }
};

// 字典 Backing
// dict backing
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ Backing
dict_t* dictBacking(const value& v){
    if (v.type == "dict" && v.ptr) return static_cast<dict_t*>(v.ptr);
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->container.type == "dict" && inst->container.ptr)
            return static_cast<dict_t*>(inst->container.ptr);
    }
    return nullptr;
}

// 列表 Backing
// list backing
// ᠲᠠᠯᠠᠭ᠌ Backing
std::vector<value>* listBacking(const value& v){
    if (v.type == "list" && v.ptr)
        return static_cast<std::vector<value>*>(v.ptr);
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->container.type == "list" && inst->container.ptr)
            return static_cast<std::vector<value>*>(inst->container.ptr);
    }
    return nullptr;
}

// sequence Backing
// sequence backing
// sequence Backing
std::vector<value>* sequenceBacking(const value& v){
    if ((v.type == "list" || v.type == "tuple") && v.ptr)
        return static_cast<std::vector<value>*>(v.ptr);
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->container.type == "list" && inst->container.ptr)
            return static_cast<std::vector<value>*>(inst->container.ptr);
    }
    return nullptr;
}

// 查找 类 属性 属于 Chain
// find class attr in chain
// ᠬᠠᠯᠠᠬᠤ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠰᠢᠨᠡ ᠳᠣᠲᠣᠭ᠌ Chain
value* findClassAttrInChain(class_t* klass, const std::string& name){
    while (klass){
        if (klass->attributes){
            auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            auto it = attrs->find(name);
            if (it != attrs->end()) return &it->second;
        }
        klass = klass->baseClass;
    }
    return nullptr;
}

// 是否 容器 方法 标识符
// is container method name
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠦᠮᠢᠰᠢ ᠠᠷᠭᠠ ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
bool isContainerMethodName(const std::string& kind, const std::string& name){
    if (kind == "dict"){
        return name == "keys" || name == "values" || name == "items" ||
               name == "get" || name == "setdefault" || name == "pop" ||
               name == "popitem" || name == "update" || name == "clear" ||
               name == "copy" || name == "__len__" || name == "__getitem__" ||
               name == "__setitem__" || name == "__delitem__" ||
               name == "__contains__" || name == "__init__";
    }
    if (kind == "list"){
        return name == "append" || name == "extend" || name == "pop" ||
               name == "insert" || name == "index" || name == "count" ||
               name == "remove" || name == "clear" || name == "copy" ||
               name == "sort" || name == "reverse" || name == "__len__" ||
               name == "__getitem__" || name == "__setitem__" ||
               name == "__delitem__" || name == "__contains__" ||
               name == "__init__";
    }
    return false;
}

void fillContainer(vm& vm, dict_t* dictItems, std::vector<value>* listItems,
                   const std::vector<value>& args,
                   const std::map<std::string, value>& kwargs){
    if (dictItems){
        if (args.size() > 1){
            vm.raiseError("TypeError", "dict expected at most 1 argument, got " +
                                       std::to_string(args.size()));
            return;
        }
        if (!args.empty() && args[0].type != "none"){
            dict_t* source = dictBacking(args[0]);
            if (source){
                for (auto& item : *source){
                    std::string error = vm.dictSet(value("dict", dictItems, &vm), item.first, item.second);
                    if (!error.empty()){
                        vm.raiseError("TypeError", error);
                        return;
                    }
                }
            } else {
                std::vector<value> entries;
                vm.materializeIterable(args[0], entries);
                for (size_t i = 0; i < entries.size(); ++i){
                    std::vector<value>* pair = sequenceBacking(entries[i]);
                    if (!pair || pair->size() != 2){
                        vm.raiseError("ValueError", "dictionary update sequence element #" +
                                                    std::to_string(i) + " has length " +
                                                    std::to_string(pair ? pair->size() : 0) +
                                                    "; 2 is required");
                        return;
                    }
                    std::string error = vm.dictSet(value("dict", dictItems, &vm),
                                                   (*pair)[0], (*pair)[1]);
                    if (!error.empty()){
                        vm.raiseError("TypeError", error);
                        return;
                    }
                }
            }
        }
        for (auto& entry : kwargs){
            std::string error = vm.dictSet(value("dict", dictItems, &vm),
                                           vm.makeString(entry.first), entry.second);
            if (!error.empty()){
                vm.raiseError("TypeError", error);
                return;
            }
        }
        return;
    }
    if (listItems){
        if (args.size() > 1){
            vm.raiseError("TypeError", "list expected at most 1 argument, got " +
                                       std::to_string(args.size()));
            return;
        }
        if (!args.empty() && args[0].type != "none"){
            std::vector<value> extra;
            vm.materializeIterable(args[0], extra);
            listItems->insert(listItems->end(), extra.begin(), extra.end());
        }
    }
}

// 闭包 句柄
// closure handle
// ᠬᠠᠭᠳᠠᠯᠲᠠ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct closure_handle{
    function_t* fn;
    std::map<std::string, value> captures;
    std::vector<std::string> freeVars;
};

// 是否 Plain Ident
// is plain ident
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Plain Ident
static bool isPlainIdent(const std::string& s){
    if (s.empty()) return false;
    if (std::isdigit((unsigned char)s[0])) return false;
    for (char c : s){
        if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    }
    return true;
}

// 扫描 Idents
// scan idents
// ᠶᠢᠨᠳᠡᠬᠡ Idents
static void scanIdents(const std::string& text, std::vector<std::string>& out){
    size_t i = 0;
    while (i < text.size()){
        unsigned char c = (unsigned char)text[i];
        if (std::isalpha(c) || c == '_' || c >= 0x80){
            size_t j = i + 1;
            while (j < text.size()){
                unsigned char d = (unsigned char)text[j];
                if (std::isalnum(d) || d == '_' || d >= 0x80) ++j;
                else break;
            }
            out.push_back(text.substr(i, j - i));
            i = j;
        } else {
            ++i;
        }
    }
}


static std::vector<std::string> computeClosureFreeVars(function_t* fn,
                                                       const std::set<std::string>& enclosingNames){
    std::set<std::string> locals(fn->argNames.begin(), fn->argNames.end());
    std::set<std::string> globalsDecl(fn->globalNames.begin(), fn->globalNames.end());
    for (auto& ins : fn->body){
        if (ins.operation == eop_assign && !ins.operands.empty() && isPlainIdent(ins.operands[0]))
            locals.insert(ins.operands[0]);
        else if (ins.operation == eop_for && !ins.operands.empty() && isPlainIdent(ins.operands[0]))
            locals.insert(ins.operands[0]);
        else if (ins.operation == eop_function && ins.operands.size() >= 2 && isPlainIdent(ins.operands[1]))
            locals.insert(ins.operands[1]);
    }
    std::vector<std::string> ordered;
    std::set<std::string> seen;
    auto consider = [&](const std::string& ident){
        if (locals.count(ident) || globalsDecl.count(ident)) return;
        if (ident == "True" || ident == "False" || ident == "None" ||
            ident == "and" || ident == "or" || ident == "not" ||
            ident == "in" || ident == "is" || ident == "if" || ident == "else") return;
        if (!enclosingNames.count(ident)) return;
        if (seen.insert(ident).second) ordered.push_back(ident);
    };
    std::vector<std::string> idents;
    auto scanText = [&](const std::string& text){
        idents.clear();
        scanIdents(text, idents);
        for (auto& ident : idents) consider(ident);
    };
    for (auto& ins : fn->body){
        for (auto& op : ins.operands) scanText(op);
        for (auto& kw : ins.kwargs) scanText(kw.second);
        for (auto& unpack : ins.argsUnpack) scanText(unpack);
        for (auto& unpack : ins.kwargsUnpack) scanText(unpack);
    }
    for (auto& nonlocalName : fn->nonlocalNames){
        if (enclosingNames.count(nonlocalName) && seen.insert(nonlocalName).second)
            ordered.push_back(nonlocalName);
    }
    return ordered;
}

// 单元 句柄
// cell handle
// ᠤᠷᠭᠤ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct cell_handle{
    std::string name;
    closure_handle* owner = nullptr;
};

// 单元 状态 获取
// cell state get
// ᠤᠷᠭᠤ ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
static bool cellStateGet(vm& vm, void* handle, const std::string& name, value& result){
    auto* cell = static_cast<cell_handle*>(handle);
    if (name == "cell_contents"){
        auto it = cell->owner->captures.find(cell->name);
        result = (it != cell->owner->captures.end()) ? it->second : vm.makeNone();
        return true;
    }
    return false;
}

// 代码 状态 获取
// code state get
// ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
static bool codeStateGet(vm& vm, void* handle, const std::string& name, value& result){
    auto* ch = static_cast<closure_handle*>(handle);
    function_t* fn = ch->fn;
    if (name == "co_name" || name == "co_qualname"){
        result = vm.makeString(fn->name);
        return true;
    }
    if (name == "co_filename"){
        result = vm.makeString("<main>");
        return true;
    }
    if (name == "co_firstlineno"){
        result = vm.makeInt(1);
        return true;
    }
    if (name == "co_flags"){
        result = vm.makeInt(0);
        return true;
    }
    if (name == "co_argcount" || name == "co_kwonlyargcount" || name == "co_nlocals" ||
        name == "co_posonlyargcount"){
        long long positional = 0, kwOnly = 0;
        for (size_t i = 0; i < fn->argNames.size(); ++i){
            if ((int)i == fn->variadicArg || (int)i == fn->keywordArg) continue;
            if (fn->keywordOnlyStart >= 0 && (long long)i >= fn->keywordOnlyStart) ++kwOnly;
            else ++positional;
        }
        if (name == "co_argcount") result = vm.makeInt(positional);
        else if (name == "co_kwonlyargcount") result = vm.makeInt(kwOnly);
        else if (name == "co_posonlyargcount") result = vm.makeInt(fn->positionalOnlyCount);
        else result = vm.makeInt((long long)fn->argNames.size());
        return true;
    }
    if (name == "co_stacksize"){
        result = vm.makeInt(1);
        return true;
    }
    if (name == "co_varnames"){
        std::vector<value> names;
        for (auto& argName : fn->argNames) names.push_back(vm.makeString(argName));
        result = vm.makeTuple(names);
        return true;
    }
    if (name == "co_freevars"){
        std::vector<value> names;
        for (auto& captured : ch->freeVars) names.push_back(vm.makeString(captured));
        result = vm.makeTuple(names);
        return true;
    }
    if (name == "co_cellvars"){
        result = vm.makeTuple({});
        return true;
    }
    return false;
}

// 闭包 状态 获取
// closure state get
// ᠬᠠᠭᠳᠠᠯᠲᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
static bool closureStateGet(vm& vm, void* handle, const std::string& name, value& result){
    auto* ch = static_cast<closure_handle*>(handle);
    if (name == "__code__"){
        result = vm.makeDynamicObject("code", ch, codeStateGet, nullptr);
        return true;
    }
    if (name == "__closure__"){
        std::vector<value> cells;
        for (auto& captured : ch->freeVars){
            if (ch->captures.find(captured) == ch->captures.end()) continue;
            auto* cell = new cell_handle{captured, ch};
            cells.push_back(vm.makeDynamicObject("cell", cell, cellStateGet, nullptr));
        }
        result = cells.empty() ? vm.makeNone() : vm.makeTuple(cells);
        return true;
    }
    if (name == "__name__" || name == "__qualname__"){
        result = vm.makeString(ch->fn->name);
        return true;
    }
    if (name == "__module__"){
        result = vm.makeString("__main__");
        return true;
    }
    if (name == "__doc__" || name == "__defaults__" || name == "__kwdefaults__"){
        result = vm.makeNone();
        return true;
    }
    if (name == "__annotations__"){
        result = vm.makeDict({});
        return true;
    }
    if (name == "__globals__"){
        result = vm.makeDict({});
        return true;
    }
    return false;
}

// 原生 bound 方法
// native bound method
// ᠨᠠᠲᠢᠪ bound ᠠᠷᠭᠠ
struct native_bound_method{
    value self;
    std::string methodName;
};

// 超类 对象
// super object
// ᠪᠡᠶᠡᠨ ᠣᠪᠶᠡᠭᠲ
struct super_object{
    value self;
    class_t* startClass = nullptr;
};

// bound 函数 对象
// bound function object
// bound ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ ᠣᠪᠶᠡᠭᠲ
struct bound_function_object{
    function_t* fn = nullptr;
    value self;
};

// 方法 Context Guard
// method context guard
// ᠠᠷᠭᠠ Context Guard
struct MethodContextGuard{
    vm& m_vm;
    bool active = false;

// 方法 Context Guard
// method context guard
// ᠠᠷᠭᠠ Context Guard
    explicit MethodContextGuard(vm& v) : m_vm(v) {}
    ~MethodContextGuard(){
        if (active) m_vm.popMethodContext();
    }
};

// 解析 超类 Member
// resolve super member
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠡᠶᠡᠨ Member
static value resolveSuperMember(vm& vm, super_object* so, const std::string& member){
    if (!so || !so->startClass){
        vm.raiseError("AttributeError", "'super' object has no attribute '" + member + "'");
        return vm.makeNone();
    }

    function_t* method = nullptr;
    int guard = 0;
    for (class_t* current = so->startClass->baseClass; current != nullptr && guard < 1000;
         current = current->baseClass, ++guard){
        for (auto& m : current->methods){
            if (m.name == member){
                method = &m;
                break;
            }
        }
        if (method) break;
    }
    if (method){
        return vm.makeBoundFunction(method, so->self);
    }

    guard = 0;
    for (class_t* current = so->startClass; current != nullptr && guard < 1000;
         current = current->baseClass, ++guard){
        if (!current->builtinBase.empty()){
            if (isContainerMethodName(current->builtinBase, member)){
                auto* bound = new native_bound_method();
                bound->self = so->self;
                bound->methodName = member;
                return {"native_method", bound};
            }
            break;
        }
    }
    vm.raiseError("AttributeError", "'super' object has no attribute '" + member + "'");
    return vm.makeNone();
}




// 值 拥有 描述器 方法
// value has descriptor method
// ᠭᠠᠷᠴᠠᠭ ᠲᠡᠢ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠠᠷᠭᠠ
bool valueHasDescriptorMethod(const value& v, const std::string& methodName){
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->klass) return inst->klass->methods.end() !=
            std::find_if(inst->klass->methods.begin(), inst->klass->methods.end(),
                         [&](const function_t& m){ return m.name == methodName; });
    } else if (v.type == "native_object" && v.ptr){
        auto* inst = static_cast<ffi_instance*>(v.ptr);
        if (inst->klass){
            auto it = inst->klass->methods.find(methodName);
            if (it != inst->klass->methods.end() && it->second != nullptr) return true;
        }
    }
    return false;
}


value invokeDescriptorMethod(vm& vm, const value& desc,
                               const std::string& methodName,
                               const std::vector<value>& methodArgs){
    if (desc.type == "object" && desc.ptr){
        auto* inst = static_cast<instance*>(desc.ptr);
        if (inst->klass){
            function_t* method = vm.findMethod(inst->klass, methodName);
            if (method){
                value selfCopy = desc;
                return vm.invoke(method, methodArgs, &selfCopy);
            }
        }
    } else if (desc.type == "native_object" && desc.ptr){
        value result = vm.makeNone();
        if (cffiCallMethod(vm, desc, methodName, methodArgs, result)) return result;
    }
    return vm.makeNone();
}



// 异常 父类
// exception parent
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠡᠪᠦᠭ
const char* exceptionParent(const std::string& type){

    if (type == "GeneratorExit" || type == "KeyboardInterrupt" ||
        type == "SystemExit") return "BaseException";
    if (type == "BaseException") return "";
    if (type == "Exception") return "BaseException";

    if (type == "StopIteration" || type == "StopAsyncIteration" ||
        type == "ArithmeticError" || type == "AssertionError" ||
        type == "AttributeError" || type == "BufferError" || type == "EOFError" ||
        type == "ImportError" || type == "LookupError" || type == "MemoryError" ||
        type == "NameError" || type == "OSError" || type == "ReferenceError" ||
        type == "RuntimeError" || type == "SyntaxError" || type == "SystemError" ||
        type == "TypeError" || type == "ValueError" || type == "Warning" ||
        type == "BaseExceptionGroup" || type == "ExceptionGroup" ||
        type == "PythonFinalizationError") return "Exception";

    if (type == "IOError" || type == "EnvironmentError" || type == "WindowsError"){
        return "OSError";
    }
    if (type == "ZeroDivisionError" || type == "OverflowError" ||
        type == "FloatingPointError") return "ArithmeticError";
    if (type == "ModuleNotFoundError") return "ImportError";
    if (type == "Error") return "ValueError";
    if (type == "NoSectionError" || type == "DuplicateSectionError" ||
        type == "NoOptionError" || type == "InterpolationError" ||
        type == "MissingSectionHeaderError" || type == "ParsingError" ||
        type == "InterpolationMissingOptionError" || type == "InterpolationSyntaxError")
        return "Error";
    if (type == "TOMLDecodeError") return "ValueError";
    if (type == "IndexError" || type == "KeyError") return "LookupError";
    if (type == "UnboundLocalError") return "NameError";
    if (type == "BlockingIOError" || type == "ChildProcessError" ||
        type == "ConnectionError" || type == "BrokenPipeError" ||
        type == "FileExistsError" || type == "FileNotFoundError" ||
        type == "InterruptedError" || type == "IsADirectoryError" ||
        type == "NotADirectoryError" || type == "PermissionError" ||
        type == "ProcessLookupError" || type == "TimeoutError") return "OSError";
    if (type == "ConnectionAbortedError" || type == "ConnectionRefusedError" ||
        type == "ConnectionResetError") return "ConnectionError";
    if (type == "NotImplementedError" || type == "RecursionError") return "RuntimeError";
    if (type == "IndentationError") return "SyntaxError";
    if (type == "TabError") return "IndentationError";
    if (type == "UnicodeError") return "ValueError";
    if (type == "UnicodeDecodeError" || type == "UnicodeEncodeError" ||
        type == "UnicodeTranslateError") return "UnicodeError";
    if (type.size() > 7 && type.compare(type.size() - 7, 7, "Warning") == 0) return "Warning";
    if (type == "CookieError" || type == "Empty" || type == "Full") return "Exception";
    return "";
}

// 处理器 匹配
// handler matches
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤᠮᠤᠨ ᠲᠣᠬᠢᠷᠠᠭᠤᠯᠳᠠᠭ᠎ᠠ
bool handlerMatches(const std::string& handlerType, const std::string& excType){
    if (handlerType.empty()) return true;
    std::string shortHandler = handlerType;
    size_t dot = shortHandler.rfind('.');
    if (dot != std::string::npos) shortHandler = shortHandler.substr(dot + 1);
    std::string current = excType;
    while (!current.empty()){
        if (current == handlerType || current == shortHandler) return true;
        current = exceptionParent(current);
    }
    return false;
}

// 已经 执行 Msg
// already executing msg
// ᠶᠡᠨᠳᠡ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ Msg
std::string alreadyExecutingMsg(const GeneratorState* gs){
    return gs->asyncGen ? "async generator already executing"
         : (gs->isCoroutine ? "coroutine already executing"
                            : "generator already executing");
}

// 裁剪 字符串
// trim string
// ᠬᠠᠯᠠᠬᠤ ᠦᠰᠦᠭ
std::string trimString(const std::string& text){
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(begin, end - begin);
}

// 裁剪 Left 字符串
// trim left string
// ᠬᠠᠯᠠᠬᠤ Left ᠦᠰᠦᠭ
std::string trimLeftString(const std::string& text){
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) begin++;
    return text.substr(begin);
}

// 裁剪 Right 字符串
// trim right string
// ᠬᠠᠯᠠᠬᠤ Right ᠦᠰᠦᠭ
std::string trimRightString(const std::string& text){
    size_t end = text.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(0, end);
}

// 提取 Trailing Marker
// take trailing marker
// ᠬᠠᠯᠠᠬᠤ Trailing Marker
span_t takeTrailingMarker(std::string& text){
    span_t span;
    if (text.size() >= 3 && text.back() == '\x02'){
        size_t start = text.rfind('\x01');
        if (start != std::string::npos){
            std::string body = text.substr(start + 1, text.size() - start - 2);
            std::string header = body;
            std::string partsText;
            size_t bar = body.find('|');
            if (bar != std::string::npos){
                header = body.substr(0, bar);
                partsText = body.substr(bar + 1);
            }
            int values[5] = {0, 0, 0, 0, 0};
            int count = 0;
            size_t p = 0;
            while (p <= header.size() && count < 5){
                size_t sep = header.find('_', p);
                std::string piece = header.substr(p, (sep == std::string::npos ? header.size() : sep) - p);
                if (!piece.empty()){
                    try { values[count] = std::stoi(piece); } catch (...){}
                }
                count++;
                if (sep == std::string::npos) break;
                p = sep + 1;
            }
            if (count == 5 && values[1] > 0 && values[2] >= values[1]){
                span.line = values[0];
                span.startCol = values[1];
                span.endCol = values[2];
                span.anchorStart = values[3];
                span.anchorEnd = values[4];

                size_t q = 0;
                while (q < partsText.size()){
                    size_t sep = partsText.find('_', q);
                    std::string piece = partsText.substr(q, (sep == std::string::npos ? partsText.size() : sep) - q);
                    size_t colon = piece.find(':');
                    if (colon != std::string::npos){
                        try {
                            int a = std::stoi(piece.substr(0, colon));
                            int b = std::stoi(piece.substr(colon + 1));
                            if (a > 0 && b >= a) span.parts.push_back({a, b});
                        } catch (...){}
                    }
                    if (sep == std::string::npos) break;
                    q = sep + 1;
                }
            }
            text.erase(start);
        }
    }
    return span;
}

// 合并 源码范围
// combine span
// ᠨᠢᠯᠡᠨ᠎ᠠ ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
span_t combineSpan(const span_t& left, const span_t& right){
    span_t span;
    if (!left.valid()) return right;
    if (!right.valid()) return left;
    span.line = left.line;
    span.startCol = left.startCol;
    span.endCol = (right.line == left.line) ? right.endCol : left.endCol;
    span.anchorStart = left.anchorStart;
    span.anchorEnd = left.anchorEnd;
    return span;
}

// 拆分 表达式 Items
// split expression items
// ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ ᠢᠯᠡᠳᠬᠡᠯᠳᠡ Items
std::vector<eval_item_t> splitExprItems(const std::string& expr){
    std::vector<eval_item_t> items;
    std::string current;
    int depth = 0;
    for (size_t i = 0; i < expr.size(); ++i){
        char c = expr[i];
        if (c == '(' || c == '[' || c == '{'){
            depth++;
        } else if (c == ')' || c == ']' || c == '}'){
            if (depth > 0) depth--;
        }
        if (c == ';' && depth == 0){
            eval_item_t item;
            item.span = takeTrailingMarker(current);
            item.text = current;
            items.push_back(item);
            current.clear();
        } else {
            current += c;
        }
    }
    eval_item_t item;
    item.span = takeTrailingMarker(current);
    item.text = current;
    items.push_back(item);
    return items;
}

// 拆分 Top Level
// split top level
// ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ Top Level
std::vector<std::string> splitTopLevel(const std::string& text, char delimiter){
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (char c : text){
        if (c == '(' || c == '[' || c == '{'){
            depth++;
        } else if (c == ')' || c == ']' || c == '}'){
            if (depth > 0) depth--;
        }
        if (c == delimiter && depth == 0){
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

// 是否 数字 Literal
// is number literal
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠲᠣ᠎ Literal
bool isNumberLiteral(const std::string& text){
    if (text.empty()) return false;
    size_t i = 0;
    if (text[0] == '-' || text[0] == '+') i++;
    if (i >= text.size()) return false;
    bool hasDigit = false;
    bool hasDot = false;
    for (; i < text.size(); ++i){
        char c = text[i];
        if (std::isdigit(static_cast<unsigned char>(c))){
            hasDigit = true;
        } else if (c == '.' && !hasDot){
            hasDot = true;
        } else {
            return false;
        }
    }
    return hasDigit;
}

// op 优先级
// op precedence
// op ᠠᠨᠲᠠᠷᠲᠠ
int opPrecedence(const std::string& op){
    if (op == "or") return 1;
    if (op == "and") return 2;
    if (op == "|") return 3;
    if (op == "^") return 4;
    if (op == "&") return 5;
    if (op == "==" || op == "!=" || op == "<" || op == ">" ||
        op == "<=" || op == ">=" ||
        op == "in" || op == "not in" || op == "is" || op == "is not") return 6;
    if (op == "<<" || op == ">>") return 7;
    if (op == "+" || op == "-") return 8;
    if (op == "*" || op == "/" || op == "//" || op == "%") return 9;
    if (op == "**") return 10;
    return 0;
}

// 是否 Binary Op
// is binary op
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Binary Op
bool isBinaryOp(const std::string& op){
    return opPrecedence(op) > 0;
}

// 浮点 转 字符串
// float to string
// ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ ᠦᠰᠦᠭ
std::string floatToString(double v){
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";

    char buffer[64];
    std::string text;
    for (int precision = 1; precision <= 17; ++precision){
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, v);
        text = buffer;
        if (std::strtod(buffer, nullptr) == v) break;
    }
    bool negative = false;
    if (!text.empty() && text[0] == '-'){ negative = true; text.erase(0, 1); }
    if (text.find("inf") != std::string::npos) return negative ? "-inf" : "inf";
    if (text.find("nan") != std::string::npos) return "nan";

    std::string digits;
    int exp10 = 0;
    size_t epos = text.find_first_of("eE");
    if (epos != std::string::npos){
        std::string mant = text.substr(0, epos);
        int e = std::atoi(text.c_str() + epos + 1);
        size_t dot = mant.find('.');
        int dotPos = dot == std::string::npos ? static_cast<int>(mant.size())
                                              : static_cast<int>(dot);
        digits = mant;
        if (dot != std::string::npos) digits.erase(dot, 1);
        exp10 = e + dotPos - 1;
        while (!digits.empty() && digits.front() == '0') digits.erase(0, 1);
        while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    } else {
        size_t dot = text.find('.');
        if (dot == std::string::npos){
            digits = text;
            exp10 = static_cast<int>(digits.size()) - 1;
        } else {
            std::string intPart = text.substr(0, dot);
            std::string fracPart = text.substr(dot + 1);
            if (intPart == "0"){
                size_t firstSig = fracPart.find_first_not_of('0');
                if (firstSig == std::string::npos){
                    digits = "0";
                    exp10 = 0;
                } else {
                    digits = fracPart.substr(firstSig);
                    exp10 = -static_cast<int>(firstSig) - 1;
                }
            } else {
                digits = intPart + fracPart;
                exp10 = static_cast<int>(intPart.size()) - 1;
                while (!digits.empty() && digits.front() == '0') digits.erase(0, 1);
            }
        }
    }
    if (digits.empty()) digits = "0";
    std::string out;
    if (exp10 < -4 || exp10 >= 16){
        out += digits[0];
        if (digits.size() > 1){
            out += ".";
            out += digits.substr(1);
        }
        int e = exp10;
        out += "e";
        out += (e < 0) ? "-" : "+";
        e = std::abs(e);
        if (e < 10) out += "0";
        out += std::to_string(e);
    } else if (exp10 >= 0){
        if (static_cast<int>(digits.size()) <= exp10 + 1){
            out = digits + std::string(static_cast<size_t>(exp10 + 1) - digits.size(), '0');
            out += ".0";
        } else {
            out = digits.substr(0, static_cast<size_t>(exp10 + 1)) + "." +
                  digits.substr(static_cast<size_t>(exp10 + 1));
        }
    } else {
        out = "0." + std::string(static_cast<size_t>(-exp10 - 1), '0') + digits;
    }
    if (negative) out = "-" + out;
    return out;
}

// 是否为复数 value (字面量或 complex() 构造的原生对象)
// Whether a value is a complex number (literal or complex() native object)
// ᠰᠢᠯᠢ ᠲᠣ᠎ value ᠮᠥᠨ ᠦᠦ
bool isComplexValue(const value& v){
    if (v.type != "native_object" || !v.ptr) return false;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (inst->typeName == "complex") return true;
    return inst->klass && inst->klass->name == "complex";
}

// 读取复数的实部与虚部 (非复数返回 false)
// Read real and imaginary parts of a complex value (false if not complex)
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠪᠣᠳᠢᠲ ᠪᠠ ᠬᠤᠶᠠᠳᠤᠭᠠᠷ ᠬᠡᠰᠡᠭ ᠢ ᠤᠩᠰᠢᠨ᠎ᠠ
bool complexParts(const value& v, double& real, double& imag){
    if (!isComplexValue(v)) return false;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    real = 0.0;
    imag = 0.0;
    auto readPart = [](const value& part) -> double {
        if (!part.ptr) return 0.0;
        if (part.type == "float") return *static_cast<double*>(part.ptr);
        if (part.type == "int" || part.type == "bool")
            return static_cast<double>(*static_cast<long long*>(part.ptr));
        return 0.0;
    };
    auto reIt = inst->attributes.find("real");
    if (reIt != inst->attributes.end()) real = readPart(reIt->second);
    auto imIt = inst->attributes.find("imag");
    if (imIt != inst->attributes.end()) imag = readPart(imIt->second);
    return true;
}

// 复数单个部分的文本 (整数浮点去掉 .0 后缀, 与 CPython 一致)
// Text of one complex part (drop .0 for integral floats, matching CPython)
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠨᠢᠭᠡ ᠬᠡᠰᠡᠭ ᠦᠨ ᠲ᠋ᠧᠺᠰᠲ
std::string complexPartToString(double v){
    std::string text = floatToString(v);
    if (text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0)
        text.erase(text.size() - 2);
    return text;
}

// 复数的 repr 文本 (2j / -2j / (1+2j) / (1-2j))
// repr text of a complex value (2j / -2j / (1+2j) / (1-2j))
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ repr ᠲ᠋ᠧᠺᠰᠲ
std::string complexToString(double real, double imag){
    if (real == 0.0 && !std::signbit(real)){
        return complexPartToString(imag) + "j";
    }
    std::string reText = complexPartToString(real);
    std::string imText = complexPartToString(std::fabs(imag));
    return "(" + reText + (std::signbit(imag) ? "-" : "+") + imText + "j)";
}

// 重复 字符串
// repeat string
// ᠳᠠᠨᠳᠠᠭᠤᠯᠬᠤ ᠦᠰᠦᠭ
std::string repeatString(const std::string& text, long long count){
    if (count <= 0 || text.empty()) return "";
    std::string result;
    result.reserve(text.size() * static_cast<size_t>(count));
    for (long long i = 0; i < count; ++i){
        result += text;
    }
    return result;
}

// 内联 索引
// inline index
// ᠳᠣᠣᠷᠠ ᠢᠨᠳᠡᠭᠡᠰ
size_t inlineIndex(const std::string& text, size_t fallback){
    if (text.empty()) return fallback;
    try {
        long long parsed = std::stoll(text);
        if (parsed < 0) return fallback;
        return static_cast<size_t>(parsed);
    } catch (...){
        return fallback;
    }
}

// suggestion Substitution Cost
// suggestion substitution cost
// suggestion Substitution Cost
inline int suggestionSubstitutionCost(char a, char b){
    if ((a & 31) != (b & 31)) return 2;
    if (a == b) return 0;
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
    if (a == b) return 1;
    return 2;
}

// suggestion 距离
// suggestion distance
// suggestion ᠶᠢᠷᠲᠡᠭᠡᠷ
long long suggestionDistance(const std::string& aText, const std::string& bText, long long maxCost){
    size_t aSize = aText.size();
    size_t bSize = bText.size();
    size_t aBegin = 0;
    size_t bBegin = 0;
    while (aSize && bSize && aText[aBegin] == bText[bBegin]){
        aBegin++;
        aSize--;
        bBegin++;
        bSize--;
    }
    while (aSize && bSize && aText[aBegin + aSize - 1] == bText[bBegin + bSize - 1]){
        aSize--;
        bSize--;
    }
    if (aSize == 0 || bSize == 0){
        return static_cast<long long>(aSize + bSize) * 2;
    }
    if (aSize > 40 || bSize > 40){
        return maxCost + 1;
    }
    const char* a = aText.data() + aBegin;
    const char* b = bText.data() + bBegin;
    if (bSize < aSize){
        std::swap(a, b);
        std::swap(aSize, bSize);
    }
    if (static_cast<long long>((bSize - aSize) * 2) > maxCost){
        return maxCost + 1;
    }
    std::vector<size_t> buffer(aSize);
    size_t tmp = 2;
    for (size_t i = 0; i < aSize; ++i){
        buffer[i] = tmp;
        tmp += 2;
    }
    size_t result = 0;
    for (size_t bIndex = 0; bIndex < bSize; ++bIndex){
        char code = b[bIndex];
        size_t distance = result = bIndex * 2;
        size_t minimum = static_cast<size_t>(-1);
        for (size_t index = 0; index < aSize; ++index){
            size_t substitute = distance +
                static_cast<size_t>(suggestionSubstitutionCost(code, a[index]));
            distance = buffer[index];
            size_t insertDelete = std::min(result, distance) + 2;
            result = std::min(insertDelete, substitute);
            buffer[index] = result;
            if (result < minimum){
                minimum = result;
            }
        }
        if (static_cast<long long>(minimum) > maxCost){
            return maxCost + 1;
        }
    }
    return static_cast<long long>(result);
}

// 挑选 Suggestion
// pick suggestion
// ᠰᠣᠩᠭᠣᠨ᠎ᠠ Suggestion
std::string pickSuggestion(const std::string& name, const std::vector<std::string>& candidates){
    std::string best;
    long long bestDistance = -1;
    for (const auto& item : candidates){
        if (item == name) continue;
        long long maxDistance =
            static_cast<long long>((name.size() + item.size() + 3) * 2 / 6);
        if (bestDistance >= 0){
            long long cap = bestDistance - 1;
            if (cap < maxDistance) maxDistance = cap;
        }
        if (maxDistance < 0) continue;
        long long current = suggestionDistance(name, item, maxDistance);
        if (current > maxDistance) continue;
        if (bestDistance < 0 || current < bestDistance){
            best = item;
            bestDistance = current;
        }
    }
    return best;
}

// builtin 异常 Names
// builtin exception names
// builtin ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ Names
const std::vector<std::string>& builtinExceptionNames(){
    static const std::vector<std::string> names = {
        "BaseException", "BaseExceptionGroup", "Exception", "ExceptionGroup",
        "ArithmeticError", "AssertionError", "AttributeError", "BlockingIOError",
        "BrokenPipeError", "BufferError", "BytesWarning", "ChildProcessError",
        "ConnectionAbortedError", "ConnectionError", "ConnectionRefusedError",
        "ConnectionResetError", "DeprecationWarning", "EOFError", "EncodingWarning",
        "EnvironmentError", "FileExistsError", "FileNotFoundError",
        "FloatingPointError", "FutureWarning", "GeneratorExit", "IOError",
        "ImportError", "ImportWarning", "IndentationError", "IndexError",
        "InterruptedError", "IsADirectoryError", "KeyError", "KeyboardInterrupt",
        "LookupError", "MemoryError", "ModuleNotFoundError", "NameError",
        "NotADirectoryError", "NotImplementedError", "OSError", "OverflowError",
        "PendingDeprecationWarning", "PermissionError", "ProcessLookupError",
        "PythonFinalizationError", "RecursionError", "ReferenceError",
        "ResourceWarning", "RuntimeError", "RuntimeWarning", "StopAsyncIteration",
        "StopIteration", "SyntaxError", "SyntaxWarning", "SystemError",
        "SystemExit", "TabError", "TimeoutError", "TypeError",
        "UnboundLocalError", "UnicodeDecodeError", "UnicodeEncodeError",
        "UnicodeError", "UnicodeTranslateError", "UnicodeWarning", "UserWarning",
        "ValueError", "Warning", "WindowsError", "ZeroDivisionError"
    };
    return names;
}

// edit 距离
// edit distance
// edit ᠶᠢᠷᠲᠡᠭᠡᠷ
size_t editDistance(const std::string& a, const std::string& b){
    std::vector<size_t> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = j;
    for (size_t i = 1; i <= a.size(); ++i){
        current[0] = i;
        for (size_t j = 1; j <= b.size(); ++j){
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});
        }
        previous.swap(current);
    }
    return previous[b.size()];
}

// 建议 异常 标识符
// suggest exception name
// ᠶᠡᠨᠳᠡᠯ ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
std::string suggestExceptionName(const std::string& name){
    std::string best;
    double bestScore = 0.6;
    for (const auto& candidate : builtinExceptionNames()){
        size_t longest = std::max(name.size(), candidate.size());
        if (longest == 0) continue;
        double score = 1.0 - static_cast<double>(editDistance(name, candidate)) /
                                static_cast<double>(longest);
        if (score > bestScore){
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}
}

// 追加 UTF-8 码点
// Append a UTF-8 codepoint
// UTF-8 ᠺᠣᠳ᠋ ᠨᠠᠮᠠᠭ᠌᠎ᠠ
static void appendUtf8Codepoint(std::string& out, unsigned int cp){
    if (cp < 0x80){
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800){
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000){
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// 字节串 repr 文本
// Byte-string repr text
// ᠪᠠᠶᠢᠲ᠋ ᠦᠰᠦᠭ repr ᠪᠢᠴᠢᠭ
static std::string bytesReprText(const std::string& raw, bool isBytearray){
    std::string result = isBytearray ? "bytearray(b'" : "b'";
    for (unsigned char ch : raw){
        if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else if (ch == '\t') result += "\\t";
        else if (ch == '\\') result += "\\\\";
        else if (ch == '\'') result += "\\'";
        else if (ch < 32 || ch > 126){
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02x", ch);
            result += buf;
        } else {
            result.push_back(static_cast<char>(ch));
        }
    }
    result += isBytearray ? "')" : "'";
    return result;
}

std::string minipy::pythonRepr(vm& vm, const value& v){
    if (v.ptr != nullptr && (v.type == "bytes" || v.type == "bytearray")){
        return bytesReprText(*static_cast<std::string*>(v.ptr), v.type == "bytearray");
    }
    if (v.ptr != nullptr && v.type == "str"){
        std::string text = *static_cast<std::string*>(v.ptr);
        bool hasSingle = text.find('\'') != std::string::npos;
        bool hasDouble = text.find('"') != std::string::npos;
        char quote = (hasSingle && !hasDouble) ? '"' : '\'';
        std::string result(1, quote);
        size_t i = 0;
        while (i < text.size()){
            char c = text[i];
            if (c == '\\'){ result += "\\\\"; ++i; continue; }
            if (c == quote){ result += std::string("\\") + quote; ++i; continue; }
            unsigned char u = static_cast<unsigned char>(c);
            if (c == '\n'){ result += "\\n"; ++i; continue; }
            if (c == '\r'){ result += "\\r"; ++i; continue; }
            if (c == '\t'){ result += "\\t"; ++i; continue; }
            if (u < 0x20 || u == 0x7F){
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\x%02x", u);
                result += buf;
                ++i;
                continue;
            }
            if (u >= 0xE0 && u < 0xF0 && i + 2 < text.size() &&
                (static_cast<unsigned char>(text[i + 1]) & 0xC0) == 0x80 &&
                (static_cast<unsigned char>(text[i + 2]) & 0xC0) == 0x80){
                unsigned int cp = ((u & 0x0F) << 12) |
                                  ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                                  (static_cast<unsigned char>(text[i + 2]) & 0x3F);
                if (cp >= 0xD800 && cp <= 0xDFFF){
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", cp);
                    result += buf;
                    i += 3;
                    continue;
                }
            }
            result += c;
            ++i;
        }
        result += quote;
        return result;
    }
    if (v.ptr != nullptr && v.type == "list"){
        auto* list = static_cast<std::vector<value>*>(v.ptr);
        std::string result = "[";
        for (size_t i = 0; i < list->size(); ++i){
            if (i > 0) result += ", ";
            result += pythonRepr(vm, (*list)[i]);
        }
        return result + "]";
    }
    if (v.ptr != nullptr && v.type == "dict"){
        auto* items = static_cast<dict_t*>(v.ptr);
        std::string result = "{";
        for (size_t i = 0; i < items->size(); ++i){
            if (i > 0) result += ", ";
            result += pythonRepr(vm, (*items)[i].first);
            result += ": ";
            result += pythonRepr(vm, (*items)[i].second);
        }
        return result + "}";
    }
    if (v.ptr != nullptr && v.type == "tuple"){
        auto* items = static_cast<std::vector<value>*>(v.ptr);
        std::string result = "(";
        for (size_t i = 0; i < items->size(); ++i){
            if (i > 0) result += ", ";
            result += pythonRepr(vm, (*items)[i]);
        }
        if (items->size() == 1) result += ",";
        return result + ")";
    }
    if (v.ptr != nullptr &&
        (v.type == "dict_keys" || v.type == "dict_values" ||
         v.type == "dict_items")){
        auto* view = static_cast<dict_view*>(v.ptr);
        std::string result = v.type + "([";
        auto* items = static_cast<dict_t*>(view->dict.ptr);
        for (size_t i = 0; i < items->size(); ++i){
            if (i > 0) result += ", ";
            if (view->kind == 0){
                result += pythonRepr(vm, (*items)[i].first);
            } else if (view->kind == 1){
                result += pythonRepr(vm, (*items)[i].second);
            } else {
                result += "(" + pythonRepr(vm, (*items)[i].first) + ", " +
                          pythonRepr(vm, (*items)[i].second) + ")";
            }
        }
        return result + "])";
    }
    if (v.ptr != nullptr && v.type == "native_object"){
        auto* inst = static_cast<ffi_instance*>(v.ptr);
        if (inst){
            if (inst->isTypeObject) return "<class '" + inst->typeName + "'>";
            if (inst->klass && inst->klass->methods.count("__repr__")){
                bool hadError = vm.hasError();
                value result;
                if (cffiCallMethod(vm, v, "__repr__", {}, result) && !vm.hasError() &&
                    result.type == "str" && result.ptr){
                    return *static_cast<std::string*>(result.ptr);
                }
                if (!hadError && vm.hasError()) vm.clearErr();
            }
        }
    }
    return vm.toString(v);
}

vm::vm(){
    scopes.push_back({});
    lastCallResult = {"none", nullptr};
    loadlib(cffi_builtins_lib());
}

std::string vm::getLastErr() const{
    return lastErr;
}

bool vm::hasError() const{
    return failed;
}

void vm::clearErr(){
    failed = false;
    lastErr.clear();
    pendingExceptionType.clear();
    pendingExceptionMsg.clear();
    pendingExceptionValue = makeNone();
}


const std::string& vm::exceptionType() const{
    return pendingExceptionType;
}

vm::ErrorSnapshot vm::saveErrorState() const{
    ErrorSnapshot snapshot;
    snapshot.failed = failed;
    snapshot.lastErr = lastErr;
    snapshot.caret = pendingCaret;
    return snapshot;
}

void vm::restoreErrorState(const ErrorSnapshot& snapshot){
    failed = snapshot.failed;
    lastErr = snapshot.lastErr;
    pendingCaret = snapshot.caret;
}

std::string vm::currentFileName() const{
    if (currentScript && !currentScript->name.empty()) return currentScript->name;
    if (!script.name.empty()) return script.name;
    return "<main>";
}

std::string vm::pythonTypeName(const value& v) const{
    if (v.type == "none") return "NoneType";
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst && inst->klass) return inst->klass->name;
    }
    if (v.type == "native_object" && v.ptr){
        auto* obj = static_cast<ffi_instance*>(v.ptr);
        if (obj && obj->klass) return obj->klass->name;
    }
    return v.type;
}


std::string vm::suggestName(const std::string& name) const{
    if (name.empty()) return "";
    std::vector<std::string> candidates;
    std::vector<std::string> seen;
    auto add = [&](const std::string& item){
        if (item.empty()) return;
        for (const auto& exist : seen){
            if (exist == item) return;
        }
        seen.push_back(item);
        candidates.push_back(item);
    };

    for (size_t i = scopes.size(); i-- > 0;){
        for (const auto& entry : scopes[i]) add(entry.first);
    }

    for (const auto& function : script.functions) add(function.name);
    for (const auto& klass : script.classes) add(klass.name);

    for (auto* library : cffiLibraries){
        if (!library || library->name != "builtins") continue;
        for (const auto& entry : library->functions) add(entry.first);
        for (const auto& entry : library->data) add(entry.first);
        for (const auto& entry : library->classes) add(entry.first);
    }
    add("None");
    add("True");
    add("False");
    return pickSuggestion(name, candidates);
}

void vm::raiseError(const std::string& type, const std::string& message,

                           const value& v){
    if (failed) return;
    failed = true;
    pendingExceptionType = type;
    pendingExceptionMsg = message;
    pendingExceptionValue = v;

    if (!frames.empty() && pendingCaret.valid()){
        frames.back().caret = pendingCaret;
    }

    std::string result = "Traceback (most recent call last):\n";
    std::vector<std::string> blocks;
    for (const auto& frame : frames){
        std::string block = "  File \"" + frame.file + "\", line " +
                            std::to_string(frame.line > 0 ? frame.line : 1) +
                            ", in " + frame.function + "\n";

        if (frame.sourceScript && frame.line > 0){
            const std::string& source = frame.sourceScript->source;
            if (!source.empty()){
                size_t begin = 0;
                int current = 1;
                while (begin <= source.size() && current < frame.line){
                    size_t next = source.find('\n', begin);
                    if (next == std::string::npos) { begin = source.size(); break; }
                    begin = next + 1;
                    current++;
                }
                size_t end = source.find('\n', begin);
                if (end == std::string::npos) end = source.size();
                std::string text = source.substr(begin, end - begin);
                if (!text.empty() && text.back() == '\r') text.pop_back();
                size_t first = text.find_first_not_of(" \t");
                if (first != std::string::npos){
                    block += "    " + text.substr(first) + "\n";
                    const span_t& caret = frame.caret;
                    if (caret.valid() && caret.line == frame.line &&
                        caret.startCol > static_cast<int>(first)){
                        int offset = caret.startCol - static_cast<int>(first) - 1;
                        block += "    " + std::string(static_cast<size_t>(offset), ' ');
                        for (int col = caret.startCol; col <= caret.endCol; ++col){
                            bool anchor = caret.hasAnchor() &&
                                          col >= caret.anchorStart && col <= caret.anchorEnd;
                            block += anchor ? '^' : '~';
                        }
                        block += "\n";
                    }
                }
            }
        }
        blocks.push_back(block);
    }

    for (size_t i = 0; i < blocks.size(); ){
        size_t j = i;
        while (j + 1 < blocks.size() && blocks[j + 1] == blocks[i]) j++;
        size_t repeat = j - i + 1;
        if (repeat > 3){
            result += blocks[i] + blocks[i + 1] + blocks[i + 2];
            size_t more = repeat - 3;
            result += "  [Previous line repeated " + std::to_string(more) +
                      (more == 1 ? " more time]\n" : " more times]\n");
        } else {
            for (size_t k = i; k <= j; ++k) result += blocks[k];
        }
        i = j + 1;
    }
    if (message.empty()){
        result += type;
    } else {
        result += type + ": " + message;
    }
    lastErr = result;
}





value::value(std::string t, void* v, vm* o)

// 类型
// type
// ᠲᠥᠷᠥᠯ
    : type(std::move(t)), ptr(v), owner(o){
    if (owner) owner->retainValue(ptr);
}

value::value(const value& other)

// 类型
// type
// ᠲᠥᠷᠥᠯ
    : type(other.type), ptr(other.ptr), owner(other.owner){
    if (owner) owner->retainValue(ptr);
}

value::value(value&& other) noexcept

// 类型
// type
// ᠲᠥᠷᠥᠯ
    : type(std::move(other.type)), ptr(other.ptr), owner(other.owner){
    other.type = "none";
    other.ptr = nullptr;
    other.owner = nullptr;
}

value& value::operator=(const value& other){
    if (this == &other) return *this;

    if (other.owner) other.owner->retainValue(other.ptr);
    void* oldValue = ptr;
    vm* oldOwner = owner;
    type = other.type;
    ptr = other.ptr;
    owner = other.owner;
    if (oldOwner) oldOwner->releaseValue(oldValue);
    return *this;
}

value& value::operator=(value&& other) noexcept{
    if (this == &other) return *this;
    void* oldValue = ptr;
    vm* oldOwner = owner;
    type = std::move(other.type);
    ptr = other.ptr;
    owner = other.owner;
    other.type = "none";
    other.ptr = nullptr;
    other.owner = nullptr;
    if (oldOwner) oldOwner->releaseValue(oldValue);
    return *this;
}

value::~value(){
    if (owner) owner->releaseValue(ptr);
}

vm::~vm(){

    shuttingDown = true;

    scopes.clear();
    callResults.clear();
    lastCallResult = value();
    pendingExceptionValue = value();
    globalNameStack.clear();

    std::vector<void*> remaining;
    remaining.reserve(heapObjects.size());
    for (auto& item : heapObjects){
        remaining.push_back(item.first);
    }
    for (void* pointer : remaining){
        destroyHeapObject(pointer);
    }
    heapObjects.clear();
    refCounts.clear();
    for (auto* library : cffiLibraries){
        delete library;
    }
    cffiLibraries.clear();
}

void* vm::keep(int kind, void* pointer){
    if (pointer == nullptr) return pointer;
    heapObjects[pointer] = heap_value{kind, pointer};

    refCounts[pointer] = 0;
    return pointer;
}

void vm::retainValue(void* pointer){
    if (shuttingDown || pointer == nullptr) return;
    auto it = refCounts.find(pointer);
    if (it != refCounts.end()) ++it->second;
}

void vm::releaseValue(void* pointer){
    if (shuttingDown || pointer == nullptr) return;
    auto it = refCounts.find(pointer);
    if (it == refCounts.end()) return;
    if (--it->second <= 0){
        destroyHeapObject(pointer);
    }
}

void vm::destroyHeapObject(void* pointer){
    auto objIt = heapObjects.find(pointer);
    if (objIt == heapObjects.end()) return;
    int kind = objIt->second.kind;

    heapObjects.erase(objIt);
    refCounts.erase(pointer);
    switch (kind){
        case vk_int:
        case vk_bool:
            delete static_cast<long long*>(pointer);
            break;
        case vk_float:
            delete static_cast<double*>(pointer);
            break;
        case vk_str:
            delete static_cast<std::string*>(pointer);
            break;
        case vk_list:
        case vk_tuple:
            delete static_cast<std::vector<value>*>(pointer);
            break;
        case vk_dict:
            delete static_cast<dict_t*>(pointer);
            break;
        case vk_dict_view:
            delete static_cast<dict_view*>(pointer);
            break;
        case vk_range:
            delete static_cast<range_t*>(pointer);
            break;
        case vk_object:
            delete static_cast<instance*>(pointer);
            break;
        case vk_native_object:
            delete static_cast<ffi_instance*>(pointer);
            break;
        case vk_exception:
            delete static_cast<ExceptionPayload*>(pointer);
            break;
        case vk_class:{
            auto* k = static_cast<class_t*>(pointer);
            if (k->attributes){
                delete static_cast<std::map<std::string, value>*>(k->attributes);
                k->attributes = nullptr;
            }
            delete k;
            break;
        }
        case vk_super:
            delete static_cast<super_object*>(pointer);
            break;
        case vk_bound_function:
            delete static_cast<bound_function_object*>(pointer);
            break;
        default:
            break;
    }
}

long long vm::heapObjectCount() const{
    return static_cast<long long>(heapObjects.size());
}

std::vector<value> vm::heapSnapshot(){
    std::vector<value> out;
    out.reserve(heapObjects.size());
    for (const auto& entry : heapObjects){
        const char* typeName = nullptr;
        switch (entry.second.kind){
            case vk_int: typeName = "int"; break;
            case vk_float: typeName = "float"; break;
            case vk_bool: typeName = "bool"; break;
            case vk_str: typeName = "str"; break;
            case vk_list: typeName = "list"; break;
            case vk_tuple: typeName = "tuple"; break;
            case vk_dict: typeName = "dict"; break;
            case vk_dict_view: typeName = "dict_view"; break;
            case vk_range: typeName = "range"; break;
            case vk_object: typeName = "object"; break;
            case vk_native_object: typeName = "native_object"; break;
            case vk_exception: typeName = "exception"; break;
            case vk_class: typeName = "class"; break;
            case vk_super: typeName = "super"; break;
            case vk_bound_function: typeName = "bound_function"; break;
            default: break;
        }
        if (typeName) out.push_back(value(typeName, entry.second.pointer, this));
    }
    return out;
}

value vm::makeInt(long long v){
    return {"int", keep(vk_int, new long long(v)), this};
}

value vm::makeFloat(double v){
    return {"float", keep(vk_float, new double(v)), this};
}

value vm::makeBool(bool v){
    return {"bool", keep(vk_bool, new long long(v ? 1 : 0)), this};
}

value vm::makeString(const std::string& v){
    return {"str", keep(vk_str, new std::string(v)), this};
}

value vm::makeNone(){
    return {"none", nullptr};
}

value vm::makeList(const std::vector<value>& elements){
    return {"list", keep(vk_list, new std::vector<value>(elements)), this};
}

value vm::makeDict(const std::vector<std::pair<value, value>>& items){
    auto* d = new dict_t();
    d->reserve(items.size());
    for (auto& kv : items){
        d->push_back(kv);
    }
    dictRebuildBuckets(*d);
    return {"dict", keep(vk_dict, d), this};
}






// hash str
// hash str
// hash str
static long long hash_str(const std::string& s){

    long long x = 0;
    for (unsigned char c : s){
        x = (1000003LL * x) ^ c;
    }
    x ^= (long long)s.size();
    if (x == -1) x = -2;
    return x;
}

long long vm::hashKey(const value& key){
    if (key.type == "int"){
        long long v = *static_cast<long long*>(key.ptr);
        if (v == -1) return -2;
        return v;
    }
    if (key.type == "bool"){
        long long v = *static_cast<long long*>(key.ptr);
        return v ? 1 : 0;
    }
    if (key.type == "float"){
        double d = *static_cast<double*>(key.ptr);

        if (d == 0.0) return 0;

        if (d != d) return 0;

        long long h = 0;
        double f = d;
        if (f < 0) { f = -f; h = 1; }
        while (f >= 2147483648.0){ f /= 2; h = (h << 1) | (h >> 31); }
        while (f < 1) { f *= 2; h = (h << 1) | (h >> 31); }
        h ^= (long long)f;
        if (d != (double)(long long)d && d < 0) h = -h;
        return h;
    }
    if (key.type == "str"){
        return hash_str(*static_cast<std::string*>(key.ptr));
    }
    if (key.type == "tuple"){
        long long x = 0x99999993LL;
        auto* elems = static_cast<std::vector<value>*>(key.ptr);
        for (auto& e : *elems){
            x = ((x ^ hashKey(e)) * 1000003LL) & 0x7FFFFFFFFFFFLL;
        }
        x ^= (long long)elems->size();
        if (x == -1) x = -2;
        return x;
    }
    if (key.type == "object" && key.ptr){
        auto* inst = static_cast<instance*>(key.ptr);
        if (inst->klass){
            function_t* hashMethod = findMethod(inst->klass, "__hash__");
            if (hashMethod){
                auto snap = saveErrorState();
                value selfCopy = key;
                value r = invoke(hashMethod, {}, &selfCopy);
                if (!hasError()){
                    restoreErrorState(snap);
                    if (r.type == "int" || r.type == "bool") return toInt(r);
                }
                restoreErrorState(snap);
            }
        }

        long long addr = (long long)(uintptr_t)inst;
        return addr ^ (addr >> 16);
    }
    if (key.type == "native_object" && key.ptr){
        auto* inst = static_cast<ffi_instance*>(key.ptr);
        if (inst && inst->klass && inst->klass->methods.count("__hash__")){
            value result;
            bool hadError = hasError();
            if (cffiCallMethod(*this, key, "__hash__", {}, result) && !hasError() &&
                (result.type == "int" || result.type == "bool"))
                return toInt(result);
            if (!hadError && hasError()) clearPendingExceptionState();
        }
        long long addr = (long long)(uintptr_t)key.ptr;
        return addr ^ (addr >> 16);
    }
    return 0;
}

value vm::makeTuple(const std::vector<value>& elements){
    return {"tuple", keep(vk_tuple, new std::vector<value>(elements)), this};
}

value vm::makeDictView(const value& dict, int kind){
    auto* view = new dict_view();
    view->dict = dict;
    view->kind = kind;
    const char* typeName = kind == 0 ? "dict_keys" : (kind == 1 ? "dict_values" : "dict_items");
    return {typeName, keep(vk_dict_view, view), this};
}

value vm::makeRange(long long start, long long stop, long long step){
    auto* range = new range_t();
    range->start = start;
    range->stop = stop;
    range->step = step;
    return {"range", keep(vk_range, range), this};
}

bool vm::hashable(const value& key){
    if (key.type == "list" || key.type == "dict" ||
        key.type == "dict_keys" || key.type == "dict_values" ||
        key.type == "dict_items") return false;
    if (key.type == "tuple" && key.ptr != nullptr){

        for (auto& item : *static_cast<std::vector<value>*>(key.ptr)){
            if (!hashable(item)) return false;
        }
    }

    if (key.type == "object" && key.ptr){
        auto* inst = static_cast<instance*>(key.ptr);

        if ((inst->container.type == "dict" || inst->container.type == "list") &&
            !(inst->klass && findMethod(inst->klass, "__hash__"))){
            return false;
        }
        if (inst->klass){

            auto* klassAttrs = inst->klass->attributes ?
                static_cast<std::map<std::string, value>*>(inst->klass->attributes) : nullptr;
            if (klassAttrs){
                auto it = klassAttrs->find("__hash__");
                if (it != klassAttrs->end() && it->second.type == "none") return false;
            }


        }
        return true;
    }
    return true;
}

void vm::dictRebuildBuckets(dict_t& d){
    d.buckets.clear();
    for (size_t i = 0; i < d.size(); ++i){
        long long h = hashKey(d[i].first);
        d.buckets[h].push_back(i);
    }
}

value* vm::dictFind(const value& dict, const value& key){
    auto* items = dictBacking(dict);
    if (items == nullptr) return nullptr;
    long long h = hashKey(key);
    auto bucketIt = items->buckets.find(h);
    if (bucketIt == items->buckets.end()) return nullptr;
    for (size_t idx : bucketIt->second){
        if (idx >= items->size()) continue;
        if (valuesEqual((*items)[idx].first, key)) return &(*items)[idx].second;
    }
    return nullptr;
}

std::string vm::dictSet(const value& dict, const value& key, const value& v){
    if (!hashable(key)) return "cannot use '" + pythonTypeName(key) +
                                   "' as a dict key (unhashable type: '" +
                                   pythonTypeName(key) + "')";
    auto* items = dictBacking(dict);
    if (items == nullptr) return "not a dict";
    long long h = hashKey(key);
    auto bucketIt = items->buckets.find(h);
    if (bucketIt != items->buckets.end()){
        for (size_t idx : bucketIt->second){
            if (idx >= items->size()) continue;
            if (valuesEqual((*items)[idx].first, key)){
                (*items)[idx].second = v;
                return "";
            }
        }
    }

    items->push_back({key, v});
    items->buckets[h].push_back(items->size() - 1);
    return "";
}

bool vm::dictRemove(const value& dict, const value& key){
    auto* items = dictBacking(dict);
    if (items == nullptr) return false;
    long long h = hashKey(key);
    auto bucketIt = items->buckets.find(h);
    if (bucketIt == items->buckets.end()) return false;
    for (size_t& idxRef : bucketIt->second){
        if (idxRef >= items->size()) continue;
        if (valuesEqual((*items)[idxRef].first, key)){
            size_t removed = idxRef;
            items->erase(items->begin() + removed);

            dictRebuildBuckets(*items);
            return true;
        }
    }
    return false;
}

value vm::makeObject(class_t* klass){
    auto* inst = new instance();
    inst->klass = klass;
    inst->attributes = new dict_t();

    inst->ownedAttributes = value("dict", keep(vk_dict, inst->attributes), this);

    std::string builtinBase = builtinBaseName(klass);
    if (builtinBase == "dict"){
        inst->container = makeDict();
    } else if (builtinBase == "list"){
        inst->container = makeList({});
    }
    return {"object", keep(vk_object, inst), this};
}

value vm::callValue(const value& callee, const std::vector<value>& args){
    return invokeAny(callee, args);
}

value vm::containerOf(const value& v) const {
    if (v.type == "dict" || v.type == "list" || v.type == "tuple") return v;
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->container.type != "none") return inst->container;
    }
    return value();
}

bool vm::isSubclassOfBuiltin(const value& sub, const std::string& builtinName){
    class_t* klass = nullptr;
    if (sub.type == "class" && sub.ptr) klass = static_cast<class_t*>(sub.ptr);
    else if (sub.type == "object" && sub.ptr) klass = static_cast<instance*>(sub.ptr)->klass;
    if (!klass) return false;
    return builtinBaseName(klass) == builtinName;
}

value vm::makeNativeObject(const ffilib* library, const ffi_class* klass, void* handle){
    auto* instance = new ffi_instance();
    instance->library = library;
    instance->klass = klass;
    instance->handle = handle;
    return {"native_object", keep(vk_native_object, instance), this};
}

// 构造一个复数 value (real + imag*1j), 复用 builtins 的 complex 类
// Build a complex value (real + imag*1j), reusing the builtins complex class
// ᠰᠢᠯᠢ ᠲᠣ᠎ value ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value vm::makeComplex(double real, double imag){
    const ffilib* builtinsLib = nullptr;
    const ffi_class* complexClass = nullptr;
    for (auto* lib : cffiLibraries){
        if (lib && lib->name == "builtins"){
            auto it = lib->classes.find("complex");
            if (it != lib->classes.end()){
                builtinsLib = lib;
                complexClass = &it->second;
            }
            break;
        }
    }
    value obj = makeNativeObject(builtinsLib, complexClass, nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    if (inst){
        if (inst->typeName.empty()) inst->typeName = "complex";
        inst->attributes["real"] = makeFloat(real);
        inst->attributes["imag"] = makeFloat(imag);
    }
    return obj;
}

value vm::makeDynamicObject(const std::string& typeName, void* handle,

                                     dynamic_get getter, dynamic_call caller){
    auto* instance = new ffi_instance();
    instance->handle = handle;
    instance->typeName = typeName;
    instance->dynamicGet = getter;
    instance->dynamicCall = caller;
    return {"native_object", keep(vk_native_object, instance), this};
}

bool vm::truthy(const value& v){
    if (v.ptr == nullptr) return false;
    if (v.type == "bool" || v.type == "int"){
        return *static_cast<long long*>(v.ptr) != 0;
    }
    if (v.type == "float"){
        return *static_cast<double*>(v.ptr) != 0.0;
    }
    if (v.type == "str"){
        return !static_cast<std::string*>(v.ptr)->empty();
    }
    if (v.type == "list" || v.type == "tuple"){
        return !static_cast<std::vector<value>*>(v.ptr)->empty();
    }
    if (v.type == "dict"){
        return !static_cast<dict_t*>(v.ptr)->empty();
    }
    if (v.type == "dict_keys" || v.type == "dict_values" ||
        v.type == "dict_items"){
        return !static_cast<dict_t*>(static_cast<dict_view*>(v.ptr)->dict.ptr)->empty();
    }
    if (v.type == "range"){
        return static_cast<range_t*>(v.ptr)->count() > 0;
    }

    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->klass){

            function_t* boolMethod = findMethod(inst->klass, "__bool__");
            if (boolMethod){
                auto snap = saveErrorState();
                value selfCopy = v;
                value r = invoke(boolMethod, {}, &selfCopy);
                if (!hasError()){
                    restoreErrorState(snap);
                    return truthy(r);
                }
                restoreErrorState(snap);
            }

            function_t* lenMethod = findMethod(inst->klass, "__len__");
            if (lenMethod){
                auto snap = saveErrorState();
                value selfCopy = v;
                value r = invoke(lenMethod, {}, &selfCopy);
                if (!hasError()){
                    restoreErrorState(snap);
                    if (r.type == "int" || r.type == "bool") return toInt(r) != 0;
                    return truthy(r);
                }
                restoreErrorState(snap);
            }
        }

        if (inst->container.type == "dict")
            return !static_cast<dict_t*>(inst->container.ptr)->empty();
        if (inst->container.type == "list")
            return !static_cast<std::vector<value>*>(inst->container.ptr)->empty();
        return true;
    }
    if (v.type == "native_object" && v.ptr){
        auto* inst = static_cast<ffi_instance*>(v.ptr);
        if (inst && inst->klass && inst->klass->methods.count("__bool__")){
            value result;
            bool hadError = hasError();
            if (cffiCallMethod(*this, v, "__bool__", {}, result) && !hasError())
                return truthy(result);
            if (!hadError && hasError()) clearErr();
        }
        if (inst && inst->klass && inst->klass->methods.count("__len__")){
            value result;
            bool hadError = hasError();
            if (cffiCallMethod(*this, v, "__len__", {}, result) && !hasError())
                return toInt(result) != 0;
            if (!hadError && hasError()) clearErr();
        }

// 复数零值 (0j) 为假
// zero complex (0j) is falsy
// ᠰᠢᠯᠢ ᠲᠣ᠎ 0j ᠦᠭᠡᠢ
        double cRe, cIm;
        if (complexParts(v, cRe, cIm)) return cRe != 0.0 || cIm != 0.0;
        return true;
    }
    return false;
}

std::string vm::toString(const value& v) {
    if (v.ptr == nullptr) return "None";
    if (v.type == "bool"){
        return *static_cast<long long*>(v.ptr) != 0 ? "True" : "False";
    }
    if (v.type == "int"){
        return std::to_string(*static_cast<long long*>(v.ptr));
    }
    if (v.type == "float"){
        return floatToString(*static_cast<double*>(v.ptr));
    }
    if (v.type == "str"){
        return *static_cast<std::string*>(v.ptr);
    }
    if (v.type == "bytes" || v.type == "bytearray"){
        return bytesReprText(*static_cast<std::string*>(v.ptr), v.type == "bytearray");
    }
    if (v.type == "range"){
        auto* range = static_cast<range_t*>(v.ptr);
        std::string text = "range(" + std::to_string(range->start) + ", " +
                           std::to_string(range->stop);
        if (range->step != 1) text += ", " + std::to_string(range->step);
        return text + ")";
    }
    if (v.type == "tuple" || v.type == "dict_keys" ||
        v.type == "dict_values" || v.type == "dict_items"){
        return pythonRepr(*this, v);
    }
    if (v.type == "list"){
        auto* list = static_cast<std::vector<value>*>(v.ptr);
        std::string result = "[";
        for (size_t i = 0; i < list->size(); ++i){
            if (i > 0) result += ", ";
            result += pythonRepr(*this, (*list)[i]);
        }
        result += "]";
        return result;
    }
    if (v.type == "dict"){
        auto* items = static_cast<dict_t*>(v.ptr);
        std::string result = "{";
        for (size_t i = 0; i < items->size(); ++i){
            if (i > 0) result += ", ";
            result += pythonRepr(*this, (*items)[i].first);
            result += ": ";
            result += pythonRepr(*this, (*items)[i].second);
        }
        result += "}";
        return result;
    }
    if (v.type == "object"){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst && inst->klass){

            function_t* strMethod = findMethod(inst->klass, "__str__");
            if (strMethod){
                value selfCopy = v;
                value r = invoke(strMethod, {}, &selfCopy, &script, inst->klass->name + ".__str__");
                if (!hasError() && r.type == "str" && r.ptr){
                    return *static_cast<std::string*>(r.ptr);
                }
                clearPendingExceptionState();
            }
            function_t* reprMethod = findMethod(inst->klass, "__repr__");
            if (reprMethod){
                value selfCopy = v;
                value r = invoke(reprMethod, {}, &selfCopy, &script, inst->klass->name + ".__repr__");
                if (!hasError() && r.type == "str" && r.ptr){
                    return *static_cast<std::string*>(r.ptr);
                }
                clearPendingExceptionState();
            }

            if (inst->container.type == "dict" || inst->container.type == "list")
                return toString(inst->container);
            char address[32];
            std::snprintf(address, sizeof(address), "0x%012llx",
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(inst)));
            return "<__main__." + inst->klass->name + " object at " + address + ">";
        }
        return "<object>";
    }
    if (v.type == "native_object"){
        auto* instance = static_cast<ffi_instance*>(v.ptr);
        if (instance && instance->typeName == "generator"){
            auto* gs = static_cast<GeneratorState*>(instance->handle);
            std::string name = gs && gs->fn ? gs->fn->name : "<genexpr>";
            char address[32];
            std::snprintf(address, sizeof(address), "0x%012llx",
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gs)));
            return "<generator object " + name + " at " + address + ">";
        }
        if (instance && instance->typeName == "coroutine"){
            auto* gs = static_cast<GeneratorState*>(instance->handle);
            std::string name = gs && gs->fn ? gs->fn->name : "<coro>";
            char address[32];
            std::snprintf(address, sizeof(address), "0x%012llx",
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gs)));
            return "<coroutine object " + name + " at " + address + ">";
        }
        if (instance && instance->typeName == "async_generator"){
            auto* gs = static_cast<GeneratorState*>(instance->handle);
            std::string name = gs && gs->fn ? gs->fn->name : "<agen>";
            char address[32];
            std::snprintf(address, sizeof(address), "0x%012llx",
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(gs)));
            return "<async_generator object " + name + " at " + address + ">";
        }
        if (instance && instance->typeName == "exception" && instance->handle){

            auto* payload = static_cast<ExceptionPayload*>(instance->handle);
            if (payload->type == "KeyError" && payload->value.type != "none" && payload->value.ptr){
                return pythonRepr(*this, payload->value);
            }
            return payload->msg;
        }
        if (instance && instance->isTypeObject){
            return "<class '" + instance->typeName + "'>";
        }

// 复数按 CPython 格式打印 (2j / (1+2j))
// print complex in CPython format (2j / (1+2j))
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢ CPython ᠬᠡᠯᠪᠡᠷᠢ ᠪᠡᠷ ᠬᠡᠪᠡᠷᠢᠶᠡᠨ᠎ᠠ
        double cRe, cIm;
        if (complexParts(v, cRe, cIm)){
            return complexToString(cRe, cIm);
        }
        if (instance && instance->klass){
            auto strIt = instance->klass->methods.find("__str__");
            if (strIt != instance->klass->methods.end() && strIt->second){
                value strResult;
                if (cffiCallMethod(*this, v, "__str__", {}, strResult) &&
                    !hasError() && strResult.type == "str" && strResult.ptr){
                    return *static_cast<std::string*>(strResult.ptr);
                }
                clearPendingExceptionState();
            }
            auto reprIt = instance->klass->methods.find("__repr__");
            if (reprIt != instance->klass->methods.end() && reprIt->second){
                value reprResult;
                if (cffiCallMethod(*this, v, "__repr__", {}, reprResult) &&
                    !hasError() && reprResult.type == "str" && reprResult.ptr){
                    return *static_cast<std::string*>(reprResult.ptr);
                }
            }
        }
        if (instance && !instance->typeName.empty()){
            return "<" + instance->typeName + " object>";
        }
        if (instance && instance->klass){
            return "<" + instance->klass->name + " object>";
        }
        return "<native object>";
    }
    return "<?>";
}

long long vm::toInt(const value& v) const {
    if (v.ptr == nullptr) return 0;
    if (v.type == "int" || v.type == "bool"){
        return *static_cast<long long*>(v.ptr);
    }
    if (v.type == "float"){
        return static_cast<long long>(*static_cast<double*>(v.ptr));
    }
    if (v.type == "str"){
        return std::atoll(static_cast<std::string*>(v.ptr)->c_str());
    }
    return 0;
}

double vm::toFloat(const value& v) const {
    if (v.ptr == nullptr) return 0.0;
    if (v.type == "int" || v.type == "bool"){
        return static_cast<double>(*static_cast<long long*>(v.ptr));
    }
    if (v.type == "float"){
        return *static_cast<double*>(v.ptr);
    }
    if (v.type == "str"){
        return std::atof(static_cast<std::string*>(v.ptr)->c_str());
    }
    return 0.0;
}

bool vm::valuesEqual(const value& left, const value& right) const {
    if (left.ptr == nullptr || right.ptr == nullptr){
        return left.ptr == nullptr && right.ptr == nullptr;
    }
    if (left.type == "object" || right.type == "object"){

        auto tryEq = [&](const value& obj, const value& other) -> bool {
            if (obj.type != "object" || !obj.ptr) return false;
            auto* inst = static_cast<instance*>(obj.ptr);
            if (!inst->klass) return false;
            auto* self = const_cast<vm*>(this);
            function_t* eq = self->findMethod(inst->klass, "__eq__");
            if (!eq) return false;
            value selfCopy = obj;
            value result = self->invoke(eq, {other}, &selfCopy);
            if (self->hasError()) { self->clearPendingExceptionState(); return false; }
            return self->truthy(result);
        };
        if (tryEq(left, right)) return true;
        if (tryEq(right, left)) return true;

        dict_t* leftDict = dictBacking(left);
        dict_t* rightDict = dictBacking(right);
        if (leftDict && rightDict){
            if (leftDict->size() != rightDict->size()) return false;
            for (auto& entry : *leftDict){
                value* other = const_cast<vm*>(this)->dictFind(right, entry.first);
                if (!other || !valuesEqual(entry.second, *other)) return false;
            }
            return true;
        }
        auto listLike = [](const value& v) {
            if (v.type == "list") return true;
            if (v.type == "object" && v.ptr)
                return static_cast<instance*>(v.ptr)->container.type == "list";
            return false;
        };
        if (listLike(left) && listLike(right)){
            auto* a = sequenceBacking(left);
            auto* b = sequenceBacking(right);
            if (a->size() != b->size()) return false;
            for (size_t i = 0; i < a->size(); ++i){
                if (!valuesEqual((*a)[i], (*b)[i])) return false;
            }
            return true;
        }

        return left.ptr == right.ptr;
    }
    if (left.type == "native_object" || right.type == "native_object"){

// native 对象的 __eq__ 优先于指针比较
// native object __eq__ takes priority over pointer comparison
// ᠨᠠᠲᠢᠪ ᠣᠪᠶᠧᠺᠲ __eq__ ᠨᠢ ᠰᠢᠯᠢ ᠪᠠᠷ ᠬᠠᠪᠢᠴᠢᠨ᠎ᠠ ᠡᠴᠡ ᠡᠮᠦᠨ᠎ᠡ ᠪᠠᠶᠢᠨ᠎ᠠ
        auto tryNativeEq = [&](const value& obj, const value& other) -> std::pair<bool, bool>{
            if (obj.type != "native_object" || !obj.ptr) return {false, false};
            auto* ninst = static_cast<ffi_instance*>(obj.ptr);
            if (!ninst->klass) return {false, false};
            if (ninst->klass->methods.find("__eq__") == ninst->klass->methods.end())
                return {false, false};
            auto* self = const_cast<vm*>(this);
            value result;
            if (!cffiCallMethod(*self, obj, "__eq__", {other}, result)){
                if (self->hasError()) self->clearPendingExceptionState();
                return {false, false};
            }
            return {true, self->truthy(result)};
        };
        auto [handled1, res1] = tryNativeEq(left, right);
        if (handled1) return res1;
        auto [handled2, res2] = tryNativeEq(right, left);
        if (handled2) return res2;

// 复数按数值比较 ((1+2j) == (1+2j) 成立)
// compare complex numerically ((1+2j) == (1+2j) holds)
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢ ᠲᠣᠭᠠᠨ ᠪᠠᠷ ᠬᠠᠪᠢᠴᠢᠨ᠎ᠠ
        double lcRe, lcIm, rcRe, rcIm;
        bool leftCplx = complexParts(left, lcRe, lcIm);
        bool rightCplx = complexParts(right, rcRe, rcIm);
        if (leftCplx || rightCplx){
            bool leftNum = (left.type == "int" || left.type == "float" || left.type == "bool");
            bool rightNum = (right.type == "int" || right.type == "float" || right.type == "bool");
            if ((leftCplx || leftNum) && (rightCplx || rightNum)){
                double a = leftCplx ? lcRe : toFloat(left);
                double b = leftCplx ? lcIm : 0.0;
                double c = rightCplx ? rcRe : toFloat(right);
                double d = rightCplx ? rcIm : 0.0;
                return a == c && b == d;
            }
            return false;
        }
        return left.ptr == right.ptr;
    }
    if (left.type == "list" && right.type == "list"){
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        if (a->size() != b->size()) return false;
        for (size_t i = 0; i < a->size(); ++i){
            if (!valuesEqual((*a)[i], (*b)[i])) return false;
        }
        return true;
    }
    if (left.type == "tuple" && right.type == "tuple"){
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        if (a->size() != b->size()) return false;
        for (size_t i = 0; i < a->size(); ++i){
            if (!valuesEqual((*a)[i], (*b)[i])) return false;
        }
        return true;
    }
    if ((left.type == "dict_keys" || left.type == "dict_values" || left.type == "dict_items") &&
        left.type == right.type){
        auto* a = static_cast<dict_view*>(left.ptr);
        auto* b = static_cast<dict_view*>(right.ptr);
        auto* aItems = static_cast<dict_t*>(a->dict.ptr);
        auto* bItems = static_cast<dict_t*>(b->dict.ptr);
        if (aItems->size() != bItems->size()) return false;
        for (size_t i = 0; i < aItems->size(); ++i){
            if (a->kind == 0){
                if (!valuesEqual((*aItems)[i].first, (*bItems)[i].first)) return false;
            } else if (a->kind == 1){
                if (!valuesEqual((*aItems)[i].second, (*bItems)[i].second)) return false;
            } else {
                if (!valuesEqual((*aItems)[i].first, (*bItems)[i].first) ||
                    !valuesEqual((*aItems)[i].second, (*bItems)[i].second)) return false;
            }
        }
        return true;
    }
    if (left.type == "dict" && right.type == "dict"){
        auto* a = static_cast<dict_t*>(left.ptr);
        auto* b = static_cast<dict_t*>(right.ptr);
        if (a->size() != b->size()) return false;
        for (auto& item : *a){
            bool found = false;
            for (auto& other : *b){
                if (valuesEqual(item.first, other.first)){
                    if (!valuesEqual(item.second, other.second)) return false;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }
    if (left.type == "range" && right.type == "range"){
        auto* a = static_cast<range_t*>(left.ptr);
        auto* b = static_cast<range_t*>(right.ptr);
        long long aCount = a->count();
        long long bCount = b->count();
        if (aCount != bCount) return false;
        if (aCount == 0) return true;
        if (a->start != b->start) return false;
        return aCount == 1 || a->step == b->step;
    }
    if (left.type == "str" && right.type == "str"){
        if (!left.ptr || !right.ptr) return left.ptr == right.ptr;
        return *static_cast<std::string*>(left.ptr) == *static_cast<std::string*>(right.ptr);
    }
    if ((left.type == "bytes" || left.type == "bytearray") &&
        (right.type == "bytes" || right.type == "bytearray")){
        if (!left.ptr || !right.ptr) return left.ptr == right.ptr;
        return *static_cast<std::string*>(left.ptr) == *static_cast<std::string*>(right.ptr);
    }
    bool leftNumber = (left.type == "int" || left.type == "float" || left.type == "bool");
    bool rightNumber = (right.type == "int" || right.type == "float" || right.type == "bool");
    if (leftNumber && rightNumber){
        return toFloat(left) == toFloat(right);
    }
    return left.ptr == right.ptr;
}


value* vm::findVariablePtr(const std::string& name){

    value* captureHit = nullptr;
    auto isCapture = [&](size_t idx){
        return std::find(captureScopeIndices.begin(), captureScopeIndices.end(), idx) !=
               captureScopeIndices.end();
    };
    for (size_t i = scopes.size(); i-- > 0;){
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()){
            if (isCapture(i)){
                if (!captureHit) captureHit = &it->second;
                continue;
            }
            return &it->second;
        }
    }
    return captureHit;
}

value vm::findVariable(const std::string& name){
    value* found = findVariablePtr(name);
    if (found) return *found;

    size_t firstDot = name.find('.');
    if (firstDot == std::string::npos) return makeNone();
    std::string libraryName = name.substr(0, firstDot);
    std::string rest = name.substr(firstDot + 1);
    for (auto* library : cffiLibraries){
        if (!library || library->name != libraryName) continue;
        size_t secondDot = rest.find('.');
        if (secondDot == std::string::npos){
            value result = makeNone();
            if (cffiCallData(*library, rest, *this, result)) return result;
        } else {
            std::string className = rest.substr(0, secondDot);
            std::string dataName = rest.substr(secondDot + 1);
            auto it = library->classes.find(className);
            if (it != library->classes.end()){
                value result = makeNone();
                if (cffiCallClassData(*library, it->second, dataName, *this, result)) return result;
            }
        }
    }

    for (auto* library : cffiLibraries){
        if (!library) continue;
        if (name.size() > library->name.size() + 1 &&
            name.compare(0, library->name.size(), library->name) == 0 &&
            name[library->name.size()] == '.'){
            std::string dataName = name.substr(library->name.size() + 1);
            if (dataName.find('.') == std::string::npos){
                value result = makeNone();
                if (cffiCallData(*library, dataName, *this, result)) return result;
            }
        }
    }
    return makeNone();
}

void vm::setVariable(const std::string& name, const value& v){
    if (scopes.empty()) scopes.push_back({});

    if (!globalNameStack.empty() && globalNameStack.back() &&
        std::find(globalNameStack.back()->begin(), globalNameStack.back()->end(), name) !=
        globalNameStack.back()->end()){
        scopes.front()[name] = v;
        return;
    }

    if (!nonlocalNameStack.empty() && nonlocalNameStack.back() &&
        std::find(nonlocalNameStack.back()->begin(), nonlocalNameStack.back()->end(), name) !=
        nonlocalNameStack.back()->end()){
        auto isCapture = [&](size_t idx){
            return std::find(captureScopeIndices.begin(), captureScopeIndices.end(), idx) !=
                   captureScopeIndices.end();
        };
        if (scopes.size() >= 2){
            for (size_t i = scopes.size() - 1; i-- > 0;){
                if (isCapture(i)) continue;
                if (scopes[i].find(name) != scopes[i].end()){
                    scopes[i][name] = v;
                    return;
                }
            }
            for (size_t i = scopes.size() - 1; i-- > 0;){
                if (!isCapture(i)) continue;
                if (scopes[i].find(name) != scopes[i].end()){
                    scopes[i][name] = v;
                    return;
                }
            }
        }
        scopes.front()[name] = v;
        return;
    }
    scopes.back()[name] = v;
}

void vm::removeVariable(const std::string& name){
    if (scopes.empty()) return;

    if (!globalNameStack.empty() && globalNameStack.back() &&
        std::find(globalNameStack.back()->begin(), globalNameStack.back()->end(), name) !=
        globalNameStack.back()->end()){
        auto it = scopes.front().find(name);
        if (it != scopes.front().end()) scopes.front().erase(it);
        return;
    }
    auto it = scopes.back().find(name);
    if (it != scopes.back().end()){
        scopes.back().erase(it);
    } else {
        raiseError("UnboundLocalError", "cannot delete local variable '" + name + "'");
    }
}


value vm::indexValue(const value& obj, const value& key){
    if (obj.type == "class" && obj.ptr){
        auto* klass = static_cast<class_t*>(obj.ptr);
        if (klass->builtinBase.rfind("enum:", 0) == 0 && key.type == "str"){
            const std::string& memberName = *static_cast<std::string*>(key.ptr);
            if (klass->attributes){
                auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
                auto it = attrs->find(memberName);
                if (it != attrs->end() && it->second.type == "native_object")
                    return it->second;
            }
            raiseError("KeyError", "'" + memberName + "'");
            return makeNone();
        }
    }
    // 自定义 __getitem__ 优先
    // Custom __getitem__ takes precedence
    if (obj.type == "object" && obj.ptr){
        auto* inst = static_cast<instance*>(obj.ptr);
        if (inst->klass){
            function_t* getitem = findMethod(inst->klass, "__getitem__");
            if (getitem){
                auto snap = saveErrorState();
                value selfCopy = obj;
                value result = invoke(getitem, {key}, &selfCopy);
                if (!hasError()) return result;
                restoreErrorState(snap);
            }
        }
        if (inst->container.type == "dict" || inst->container.type == "list"){
            return indexValue(inst->container, key);
        }
    }

    if (dict_t* entries = dictBacking(obj)){
        if (!hashable(key)){
            raiseError("TypeError", "unhashable type: '" + pythonTypeName(key) + "'");
            return makeNone();
        }
        value* found = dictFind(obj, key);
        if (!found){
            raiseError("KeyError", pythonRepr(*this, key));
            return makeNone();
        }
        return *found;
    }

    if (obj.type == "list" || obj.type == "tuple"){
        auto* items = static_cast<std::vector<value>*>(obj.ptr);
        if (key.type != "int" && key.type != "bool"){
            raiseError("TypeError", obj.type + " indices must be integers, not " +
                                    pythonTypeName(key));
            return makeNone();
        }
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(items->size());
        if (idx < 0 || idx >= static_cast<long long>(items->size())){
            raiseError("IndexError", obj.type + " index out of range");
            return makeNone();
        }
        return (*items)[static_cast<size_t>(idx)];
    }

    if (obj.type == "str"){
        if (key.type != "int" && key.type != "bool"){
            raiseError("TypeError", "string indices must be integers, not '" +
                                    pythonTypeName(key) + "'");
            return makeNone();
        }
        auto* text = static_cast<std::string*>(obj.ptr);
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(text->size());
        if (idx < 0 || idx >= static_cast<long long>(text->size())){
            raiseError("IndexError", "string index out of range");
            return makeNone();
        }
        return makeString(std::string(1, (*text)[static_cast<size_t>(idx)]));
    }

    if (obj.type == "bytes" || obj.type == "bytearray"){
        if (key.type != "int" && key.type != "bool"){
            raiseError("TypeError", "byte indices must be integers or slices, not '" +
                                    pythonTypeName(key) + "'");
            return makeNone();
        }
        auto* raw = static_cast<std::string*>(obj.ptr);
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(raw->size());
        if (idx < 0 || idx >= static_cast<long long>(raw->size())){
            raiseError("IndexError", "index out of range");
            return makeNone();
        }
        return makeInt(static_cast<unsigned char>((*raw)[static_cast<size_t>(idx)]));
    }

    if (obj.type == "range"){
        auto* range = static_cast<range_t*>(obj.ptr);
        long long idx = toInt(key);
        long long total = range->count();
        if (idx < 0) idx += total;
        if (idx < 0 || idx >= total){
            raiseError("IndexError", "range object index out of range");
            return makeNone();
        }
        return makeInt(range->at(idx));
    }

    raiseError("TypeError", "'" + pythonTypeName(obj) + "' object is not subscriptable");
    return makeNone();
}

std::string vm::indexAssign(const value& obj, const value& key, const value& v){
    if (obj.type == "native_object" && obj.ptr){
        auto* ninst = static_cast<ffi_instance*>(obj.ptr);
        if (ninst->klass && ninst->klass->methods.count("__setitem__")){
            value r;
            cffiCallMethod(*this, obj, "__setitem__", {key, v}, r);
            return "";
        }
        return "'" + pythonTypeName(obj) + "' object does not support item assignment";
    }
    if (obj.type == "object" && obj.ptr){
        auto* inst = static_cast<instance*>(obj.ptr);
        if (inst->klass){
            function_t* setitem = findMethod(inst->klass, "__setitem__");
            if (setitem){
                value selfCopy = obj;
                (void)invoke(setitem, {key, v}, &selfCopy);
                if (!hasError()) return "";
                clearPendingExceptionState();
            }
        }
        if (inst->container.type == "dict" || inst->container.type == "list"){
            return indexAssign(inst->container, key, v);
        }
        return "'" + pythonTypeName(obj) + "' object does not support item assignment";
    }

    if (dict_t* entries = dictBacking(obj)){
        return dictSet(obj, key, v);
    }

    if (obj.type == "bytearray"){
        if (key.type != "int" && key.type != "bool"){
            return "bytearray indices must be integers or slices, not '" +
                   pythonTypeName(key) + "'";
        }
        auto* raw = static_cast<std::string*>(obj.ptr);
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(raw->size());
        if (idx < 0 || idx >= static_cast<long long>(raw->size()))
            return "bytearray index out of range";
        long long byte = toInt(v);
        if (byte < 0 || byte > 255)
            return "byte must be in range(0, 256)";
        (*raw)[static_cast<size_t>(idx)] = static_cast<char>(byte);
        return "";
    }
    if (obj.type == "bytes"){
        return "'bytes' object does not support item assignment";
    }

    if (obj.type == "list"){
        auto* items = static_cast<std::vector<value>*>(obj.ptr);
        if (key.type != "int" && key.type != "bool"){
            return "list indices must be integers, not " + pythonTypeName(key);
        }
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(items->size());
        if (idx < 0 || idx >= static_cast<long long>(items->size())){
            return "list assignment index out of range";
        }
        (*items)[static_cast<size_t>(idx)] = v;
        return "";
    }

    return "'" + pythonTypeName(obj) + "' object does not support item assignment";
}

std::string vm::indexDelete(const value& obj, const value& key){
    if (obj.type == "native_object" && obj.ptr){
        auto* ninst = static_cast<ffi_instance*>(obj.ptr);
        if (ninst->klass && ninst->klass->methods.count("__delitem__")){
            value r;
            cffiCallMethod(*this, obj, "__delitem__", {key}, r);
            return "";
        }
        return "'" + pythonTypeName(obj) + "' object doesn't support item deletion";
    }
    if (obj.type == "object" && obj.ptr){
        auto* inst = static_cast<instance*>(obj.ptr);
        if (inst->klass){
            function_t* delitem = findMethod(inst->klass, "__delitem__");
            if (delitem){
                value selfCopy = obj;
                (void)invoke(delitem, {key}, &selfCopy);
                if (!hasError()) return "";
                clearPendingExceptionState();
            }
        }
        if (inst->container.type == "dict" || inst->container.type == "list"){
            return indexDelete(inst->container, key);
        }
        return "'" + pythonTypeName(obj) + "' object doesn't support item deletion";
    }

    if (dict_t* entries = dictBacking(obj)){
        if (!dictRemove(obj, key)) return pythonRepr(*this, key);
        return "";
    }

    if (obj.type == "list"){
        auto* items = static_cast<std::vector<value>*>(obj.ptr);
        long long idx = toInt(key);
        if (idx < 0) idx += static_cast<long long>(items->size());
        if (idx < 0 || idx >= static_cast<long long>(items->size())){
            return "list assignment index out of range";
        }
        items->erase(items->begin() + static_cast<std::ptrdiff_t>(idx));
        return "";
    }

    return "'" + pythonTypeName(obj) + "' object doesn't support item deletion";
}

std::vector<std::string> vm::collectScopeNames() const{
    std::vector<std::string> names;
    std::set<std::string> seen;
    for (size_t i = scopes.size(); i-- > 0;){
        for (const auto& entry : scopes[i]){
            if (seen.insert(entry.first).second) names.push_back(entry.first);
        }
    }
    return names;
}


void vm::assignTarget(const std::string& target, const value& v){

    if (!target.empty() && target.back() == ']'){
        int depth = 0;
        size_t open = std::string::npos;
        for (size_t i = target.size(); i > 0; --i){
            char c = target[i - 1];
            if (c == ']'){
                depth++;
            } else if (c == '['){
                depth--;
                if (depth == 0){ open = i - 1; break; }
            }
        }
        if (open != std::string::npos && open > 0){
            std::string baseText = trimString(target.substr(0, open));
            std::string indexText = target.substr(open + 1, target.size() - open - 2);
            value base = evalExpr(baseText);
            if (failed) return;
            if (isSliceText(indexText)){
                if (base.type == "bytearray" && base.ptr){
                    auto* raw = static_cast<std::string*>(base.ptr);
                    long long bs = 0, be = 0, bst = 1;
                    if (!sliceBounds(indexText, static_cast<long long>(raw->size()), bs, be, bst)) return;
                    std::vector<value> vals;
                    if (v.type == "bytes" || v.type == "bytearray"){
                        auto* src = static_cast<std::string*>(v.ptr);
                        for (unsigned char ch : *src)
                            vals.push_back(makeInt(static_cast<long long>(ch)));
                    } else {
                        materializeIterable(v, vals);
                    }
                    if (failed) return;
                    std::string repl;
                    for (auto& item : vals){
                        long long byte = toInt(item);
                        if (byte < 0 || byte > 255){
                            raiseError("ValueError", "byte must be in range(0, 256)");
                            return;
                        }
                        repl.push_back(static_cast<char>(byte));
                    }
                    if (bst == 1){
                        raw->erase(raw->begin() + bs, raw->begin() + be);
                        raw->insert(raw->begin() + bs, repl.begin(), repl.end());
                    } else {
                        long long count = (bst > 0)
                            ? (be > bs ? (be - bs + bst - 1) / bst : 0)
                            : (bs > be ? (bs - be - bst - 1) / (-bst) : 0);
                        if (static_cast<long long>(repl.size()) != count){
                            raiseError("ValueError", "attempt to assign bytes of size " +
                                                     std::to_string(repl.size()) +
                                                     " to extended slice of size " +
                                                     std::to_string(count));
                            return;
                        }
                        size_t k = 0;
                        for (long long idx = bs; (bst > 0) ? idx < be : idx > be; idx += bst)
                            (*raw)[static_cast<size_t>(idx)] = repl[k++];
                    }
                    return;
                }
                std::vector<value>* lst = nullptr;
                if (base.type == "list" && base.ptr){
                    lst = static_cast<std::vector<value>*>(base.ptr);
                } else if (base.type == "object" && base.ptr){
                    auto* inst = static_cast<instance*>(base.ptr);
                    if (inst->container.type == "list" && inst->container.ptr)
                        lst = static_cast<std::vector<value>*>(inst->container.ptr);
                }
                if (!lst){
                    raiseError("TypeError", "'" + pythonTypeName(base) +
                                            "' object does not support slice assignment");
                    return;
                }
                long long s = 0, e = 0, st = 1;
                if (!sliceBounds(indexText, static_cast<long long>(lst->size()), s, e, st)) return;
                std::vector<value> vals;
                materializeIterable(v, vals);
                if (failed) return;
                if (st == 1){
                    lst->erase(lst->begin() + s, lst->begin() + e);
                    lst->insert(lst->begin() + s, vals.begin(), vals.end());
                } else {
                    long long count = (st > 0) ? (e > s ? (e - s + st - 1) / st : 0)
                                               : (s > e ? (s - e - st - 1) / (-st) : 0);
                    if (static_cast<long long>(vals.size()) != count){
                        raiseError("ValueError", "attempt to assign sequence of size " +
                                                 std::to_string(vals.size()) +
                                                 " to extended slice of size " +
                                                 std::to_string(count));
                        return;
                    }
                    size_t k = 0;
                    for (long long idx = s; (st > 0) ? idx < e : idx > e; idx += st){
                        (*lst)[static_cast<size_t>(idx)] = vals[k++];
                    }
                }
                return;
            }
            value key = evalExpr(indexText);
            if (failed) return;

            if (base.type == "object" && base.ptr){
                auto* inst = static_cast<instance*>(base.ptr);
                function_t* setitem = findMethod(inst->klass, "__setitem__");
                if (setitem){
                    value selfCopy = base;
                    (void)invoke(setitem, {key, v}, &selfCopy);
                    if (!hasError()) return;
                    clearPendingExceptionState();
                }

                if (dict_t* items = dictBacking(base)){
                    std::string error = dictSet(base, key, v);
                    if (!error.empty()){
                        raiseError("TypeError", error);
                    }
                    return;
                }
                if (inst->container.type == "list"){
                    auto* lst = static_cast<std::vector<value>*>(inst->container.ptr);
                    if (key.type != "int" && key.type != "bool"){
                        raiseError("TypeError", "list indices must be integers or slices, not " +
                                                pythonTypeName(key));
                        return;
                    }
                    long long idx = toInt(key);
                    if (idx < 0) idx += static_cast<long long>(lst->size());
                    if (idx < 0 || idx >= static_cast<long long>(lst->size())){
                        raiseError("IndexError", "list assignment index out of range");
                        return;
                    }
                    (*lst)[static_cast<size_t>(idx)] = v;
                    return;
                }
                raiseError("TypeError", "'" + pythonTypeName(base) +
                                        "' object does not support item assignment");
                return;
            }
            if (base.type == "native_object" && base.ptr){
                auto* ninst = static_cast<ffi_instance*>(base.ptr);
                if (ninst->klass && ninst->klass->methods.count("__setitem__")){
                    value r;
                    cffiCallMethod(*this, base, "__setitem__", {key, v}, r);
                    return;
                }
                raiseError("TypeError", "'" + pythonTypeName(base) +
                                        "' object does not support item assignment");
                return;
            }
            if (base.type == "list" && base.ptr){
                auto* lst = static_cast<std::vector<value>*>(base.ptr);
                int idx = toInt(key);
                if (idx < 0) idx += static_cast<int>(lst->size());
                if (idx < 0 || idx >= static_cast<int>(lst->size())){
                    raiseError("IndexError", "list assignment index out of range");
                    return;
                }
                (*lst)[idx] = v;
                return;
            }
            if (base.type == "bytearray" && base.ptr){
                if (key.type != "int" && key.type != "bool"){
                    raiseError("TypeError", "bytearray indices must be integers or slices, not " +
                                            pythonTypeName(key));
                    return;
                }
                auto* raw = static_cast<std::string*>(base.ptr);
                long long idx = toInt(key);
                if (idx < 0) idx += static_cast<long long>(raw->size());
                if (idx < 0 || idx >= static_cast<long long>(raw->size())){
                    raiseError("IndexError", "bytearray index out of range");
                    return;
                }
                long long byte = toInt(v);
                if (byte < 0 || byte > 255){
                    raiseError("ValueError", "byte must be in range(0, 256)");
                    return;
                }
                (*raw)[static_cast<size_t>(idx)] = static_cast<char>(byte);
                return;
            }
            if (base.type == "bytes"){
                raiseError("TypeError", "'bytes' object does not support item assignment");
                return;
            }
            if (base.type != "dict"){
                raiseError("TypeError", "'" + pythonTypeName(base) +
                                        "' object does not support item assignment");
                return;
            }
            std::string error = dictSet(base, key, v);
            if (!error.empty()){
                raiseError("TypeError", error);
            }
            return;
        }
    }
    size_t dot = target.rfind('.');
    if (dot == std::string::npos){
        setVariable(target, v);
        return;
    }
    std::string baseName = target.substr(0, dot);
    std::string member = target.substr(dot + 1);
    value* base = findVariablePtr(baseName);
    if (base && base->type == "object" && base->ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
        struct instance* instance = static_cast<struct instance*>(base->ptr);

        if (instance->klass){
            function_t* setattrMethod = findMethod(instance->klass, "__setattr__");
            if (setattrMethod){
                auto snap = saveErrorState();
                value selfCopy = *base;
                value nameStr = makeString(member);
                value r = invoke(setattrMethod, {nameStr, v}, &selfCopy);
                if (!hasError()){
                    restoreErrorState(snap);
                    return;
                }
                restoreErrorState(snap);
            }
        }

        if (instance->klass && instance->klass->attributes){
            auto* klassAttrs = static_cast<std::map<std::string, value>*>(instance->klass->attributes);
            auto clsAttrIt = klassAttrs->find(member);
            if (clsAttrIt != klassAttrs->end()){
                const value& clsVal = clsAttrIt->second;
                if (valueHasDescriptorMethod(clsVal, "__set__")){

                    saveErrorState();
                    value descResult = invokeDescriptorMethod(*this, clsVal, "__set__",
                                                              {*base, v});
                    if (!hasError()) return;
                    restoreErrorState(saveErrorState());
                }
                if (valueHasDescriptorMethod(clsVal, "__delete__") &&
                    !valueHasDescriptorMethod(clsVal, "__set__")){

                    raiseError("AttributeError", "can't set attribute");
                    return;
                }
            }
        }
        {
            value attrDict("dict", instance->attributes, this);
            dictSet(attrDict, makeString(member), v);
        }
        return;
    }
    if (base && base->type == "class" && base->ptr){

        auto* klass = static_cast<class_t*>(base->ptr);
        if (klass->attributes){
            auto* klassAttrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            auto clsAttrIt = klassAttrs->find(member);
            if (clsAttrIt != klassAttrs->end()){
                const value& clsVal = clsAttrIt->second;
                if (valueHasDescriptorMethod(clsVal, "__set__")){

                    saveErrorState();
                    value descResult = invokeDescriptorMethod(*this, clsVal, "__set__",
                                                              {*base, v});
                    if (!hasError()) return;
                    restoreErrorState(saveErrorState());
                }
            }
        }
        if (!klass->attributes) klass->attributes = new std::map<std::string, value>();
        auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
        (*attrs)[member] = v;
        return;
    }
    if (base && base->type == "native_object" && base->ptr){
        auto* instance = static_cast<ffi_instance*>(base->ptr);
        instance->attributes[member] = v;
        if (instance->typeName == "Structure"){
            auto* info = static_cast<CtypesStructInfo*>(instance->handle);
            if (info){

                size_t fldOffset = info->cursor;
                size_t fldSize = 8;
                bool foundInFields = false;
                std::string fldTypeName;
                for (auto& f : info->fields){
                    if (f.name == member){
                        fldOffset = f.offset;
                        fldSize = f.size;
                        fldTypeName = f.typeName;
                        foundInFields = true;
                        break;
                    }
                }
                if (!foundInFields){


                    static const char* ptrNames[] = {
                        "lpfnWndProc","hInstance","hIcon","hCursor","hbrBackground",
                        "lpszMenuName","lpszClassName","hwnd","hWnd","hDC","hBitmap",
                        "lParam","wParam","lpParam","lpszText","lpData","lpBits",
                        "lpParamPrevWndFunc","lpPrevWndFunc"
                    };
                    static const char* int4Names[] = {
                        "style","cbClsExtra","cbWndExtra","message","msg","time",
                        "iWidth","iHeight","nHeight","nWidth","wMaxWidth","wMaxHeight",
                        "clrText","clrBackground","iMode","iPlanes","iPixelBits"
                    };
                    static const char* int8Names[] = {"long long","c_longlong","c_ulonglong"};
                    size_t fldAlign = 8;
                    bool isPtr = false, isInt4 = false, isInt8 = false;
                    for (auto* n : ptrNames) if (member == n){ isPtr = true; break; }
                    for (auto* n : int4Names) if (member == n){ isInt4 = true; break; }
                    for (auto* n : int8Names) if (member == n){ isInt8 = true; break; }
                    if (isPtr){ fldSize = fldAlign = 8; }                     else if (isInt4){ fldSize = fldAlign = 4; }                     else if (isInt8){ fldSize = fldAlign = 8; }                     else if (v.type == "native_object" && v.ptr){
                        fldSize = fldAlign = 8;
                    } else if (v.type == "int"){
                        long long ll = toInt(v);
                        if (ll >= INT_MIN && ll <= INT_MAX){ fldSize = fldAlign = 4; }                         else { fldSize = fldAlign = 8; }
                    }
                    size_t mis = info->cursor % fldAlign;
                    if (mis > 0) info->cursor += (fldAlign - mis);
                    fldOffset = info->cursor;
                }
                size_t written = 0;
                if (v.type == "int"){
                    long long ll = toInt(v);
                    size_t sz = std::min(fldSize, info->buffer.size() - fldOffset);
                    memcpy(info->buffer.data() + fldOffset, &ll, sz);
                    written = sz;
                } else if (v.type == "native_object" && v.ptr){
                    auto* sub = static_cast<ffi_instance*>(v.ptr);
                    void* h = nullptr;
                    if (sub->typeName == "CFunction" && sub->handle){
                        auto* ch = static_cast<CtypesHandle*>(sub->handle);
                        h = ch->procAddress;
                    } else if (sub->typeName == "Callback" && sub->handle){
                        auto* cb = static_cast<CtypesCBWrap*>(sub->handle);
                        h = cb->trampoline;
                    } else {
                        h = sub->handle ? sub->handle : sub;
                    }
                    uint64_t ptr = reinterpret_cast<uint64_t>(h);
                    size_t sz = std::min(fldSize, info->buffer.size() - fldOffset);
                    memcpy(info->buffer.data() + fldOffset, &ptr, sz);
                    written = sz;
                } else if (v.type == "none"){
                    uint64_t zero = 0;
                    size_t sz = std::min(fldSize, info->buffer.size() - fldOffset);
                    memcpy(info->buffer.data() + fldOffset, &zero, sz);
                    written = sz;
                } else if (v.type == "str"){
                    auto* strPtr = static_cast<std::string*>(v.ptr);
                    const std::string& s = *strPtr;

                    bool isWide = false;
                    if (foundInFields){
                        static const char* wideNames[] = {
                            "c_wchar_p","LPWSTR","LPCWSTR","wchar_t*","c_wstring",
                            "PWSTR","PCWSTR","BSTR"
                        };
                        for (auto* wn : wideNames){
                            if (fldTypeName == wn || fldTypeName.find(wn) != std::string::npos){
                                isWide = true;
                                break;
                            }
                        }
                    }
                    uint64_t strAddr = reinterpret_cast<uint64_t>(info->buffer.data() + info->buffer.size());
                    if (isWide){
                        std::wstring ws = utf8ToWide(s);

                        for (wchar_t wc : ws){
                            unsigned char lo = static_cast<unsigned char>(wc & 0xFF);
                            unsigned char hi = static_cast<unsigned char>((wc >> 8) & 0xFF);
                            info->buffer.push_back(lo);
                            info->buffer.push_back(hi);
                        }

                        info->buffer.push_back(0);
                        info->buffer.push_back(0);
                    } else {
                        info->buffer.insert(info->buffer.end(), s.begin(), s.end());
                        info->buffer.push_back(0);
                    }
                    size_t sz = std::min(fldSize, info->buffer.size() - fldOffset);
                    memcpy(info->buffer.data() + fldOffset, &strAddr, sz);
                    written = sz;
                } else if (v.type == "bytes"){

                    auto* vec = static_cast<std::vector<unsigned char>*>(v.ptr);
                    uint64_t addr = reinterpret_cast<uint64_t>(info->buffer.data() + info->buffer.size());
                    info->buffer.insert(info->buffer.end(), vec->begin(), vec->end());
                    info->buffer.push_back(0);
                    size_t sz = std::min(fldSize, info->buffer.size() - fldOffset);
                    memcpy(info->buffer.data() + fldOffset, &addr, sz);
                    written = sz;
                }

                if (!foundInFields && written > 0){
                    info->cursor = fldOffset + written;
                }
            }
        }
        return;
    }
    setVariable(target, v);
}


void vm::unpackValues(const value& v, const std::vector<std::string>& targets){
    std::vector<value> items;
    materializeIterable(v, items);
    if (failed) return;
    int starIndex = -1;
    for (size_t i = 0; i < targets.size(); ++i){
        if (!targets[i].empty() && targets[i][0] == '*'){ starIndex = static_cast<int>(i); break; }
    }
    if (starIndex >= 0){
        int fixed = static_cast<int>(targets.size()) - 1;
        if (static_cast<int>(items.size()) < fixed){
            raiseError("ValueError", "not enough values to unpack (expected at least " +
                                     std::to_string(fixed) + ", got " +
                                     std::to_string(items.size()) + ")");
            return;
        }
        int post = fixed - starIndex;
        for (int i = 0; i < starIndex; ++i){
            unpackTarget(targets[static_cast<size_t>(i)], items[static_cast<size_t>(i)]);
            if (failed) return;
        }
        std::vector<value> starItems(items.begin() + starIndex,
                                       items.end() - post);
        std::string starName = targets[static_cast<size_t>(starIndex)].substr(1);
        unpackTarget(starName, makeList(starItems));
        if (failed) return;
        for (int i = 0; i < post; ++i){
            unpackTarget(targets[static_cast<size_t>(starIndex + 1 + i)],
                         items[items.size() - static_cast<size_t>(post) + static_cast<size_t>(i)]);
            if (failed) return;
        }
        return;
    }
    if (items.size() != targets.size()){
        if (items.size() < targets.size()){
            raiseError("ValueError", "not enough values to unpack (expected " +
                                     std::to_string(targets.size()) + ", got " +
                                     std::to_string(items.size()) + ")");
        } else {
            raiseError("ValueError", "too many values to unpack (expected " +
                                     std::to_string(targets.size()) + ")");
        }
        return;
    }
    for (size_t i = 0; i < targets.size(); ++i){
        unpackTarget(targets[i], items[i]);
        if (failed) return;
    }
}


void vm::unpackTarget(const std::string& target, const value& v){
    std::string t = trimString(target);
    if (t.empty()) return;
    if (t.size() >= 2 && t.front() == '(' && t.back() == ')'){

        std::vector<std::string> parts;
        std::string cur;
        int depth = 0;
        for (size_t i = 1; i + 1 < t.size(); ++i){
            char c = t[i];
            if (c == '(' || c == '[' || c == '{'){
                depth++;
                cur += c;
            } else if (c == ')' || c == ']' || c == '}'){
                depth--;
                cur += c;
            } else if (c == ',' && depth == 0){
                parts.push_back(trimString(cur));
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) parts.push_back(trimString(cur));
        unpackValues(v, parts);
        return;
    }
    assignTarget(t, v);
}

bool vm::isSliceText(const std::string& text){
    int depth = 0;
    for (size_t i = 0; i < text.size(); ++i){
        char c = text[i];
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
        else if (c == ':' && depth == 0) return true;
    }
    return false;
}

bool vm::sliceBounds(const std::string& text, long long length,

                           long long& start, long long& stop, long long& step){
    std::vector<std::string> parts;
    std::string cur;
    int depth = 0;
    for (size_t i = 0; i < text.size(); ++i){
        char c = text[i];
        if (c == '(' || c == '[' || c == '{'){
            depth++;
            cur += c;
        } else if (c == ')' || c == ']' || c == '}'){
            depth--;
            cur += c;
        } else if (c == ':' && depth == 0){
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    auto evalPart = [&](size_t idx, bool& has, long long& out){
        has = false;
        if (idx >= parts.size()) return true;
        std::string t = parts[idx];
        if (t.find_first_not_of(" \t") == std::string::npos) return true;
        has = true;
        value val = evalExpr(t);
        if (failed) return false;
        if (val.type != "int" && val.type != "bool"){
            raiseError("TypeError", "slice indices must be integers or None or have an __index__ method");
            return false;
        }
        out = toInt(val);
        return true;
    };
    long long startVal = 0, stopVal = 0, stepVal = 1;
    bool hasStart = false, hasStop = false, hasStep = false;
    if (!evalPart(0, hasStart, startVal)) return false;
    if (!evalPart(1, hasStop, stopVal)) return false;
    if (!evalPart(2, hasStep, stepVal)) return false;
    if (!hasStep) stepVal = 1;
    if (stepVal == 0){
        raiseError("ValueError", "slice step cannot be zero");
        return false;
    }
    step = stepVal;
    if (step > 0){
        if (!hasStart) start = 0;
        else {
            start = startVal;
            if (start < 0) start += length;
            if (start < 0) start = 0;
            if (start > length) start = length;
        }
        if (!hasStop) stop = length;
        else {
            stop = stopVal;
            if (stop < 0) stop += length;
            if (stop < 0) stop = 0;
            if (stop > length) stop = length;
        }
    } else {
        if (!hasStart) start = length - 1;
        else {
            start = startVal;
            if (start < 0) start += length;
            if (start < 0) start = -1;
            if (start >= length) start = length - 1;
        }
        if (!hasStop) stop = -1;
        else {
            stop = stopVal;
            if (stop < 0) stop += length;
            if (stop < 0) stop = -1;
            if (stop >= length) stop = length - 1;
        }
    }
    return true;
}


value vm::evalExpr(std::string expr){
    if (!expr.empty() &&
        (std::isspace(static_cast<unsigned char>(expr.front())) ||
         std::isspace(static_cast<unsigned char>(expr.back())))){
        expr = trimString(expr);
    }
    if (expr.empty()) return makeNone();

    bool simpleName = expr[0] != '\0' && !std::isdigit(static_cast<unsigned char>(expr[0]));
    if (simpleName){
        for (char c : expr){
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')){
                simpleName = false;
                break;
            }
        }
    }
    if (simpleName){
        value* found = findVariablePtr(expr);
        if (found) return *found;
    }
    std::vector<eval_item_t> items = splitExprItems(expr);
    size_t index = 0;
    span_t span;
    return evalItems(items, index, 0, &span);
}

value vm::evalItems(const std::vector<eval_item_t>& items, size_t& index, int minPrec,

                             span_t* outSpan){
    span_t leftSpan;
    value left = evalUnaryItem(items, index, &leftSpan);
    while (index < items.size()){
        const std::string& op = items[index].text;
        if (!isBinaryOp(op)) break;
        int prec = opPrecedence(op);
        if (prec < minPrec) break;
        span_t opSpan = items[index].span;
        index++;
        int nextMin = (op == "**") ? prec : prec + 1;
        span_t rightSpan;
        value right = evalItems(items, index, nextMin, &rightSpan);
        span_t binSpan = combineSpan(leftSpan, rightSpan);
        if (opSpan.valid() && binSpan.valid() && opSpan.line == binSpan.line){
            binSpan.anchorStart = opSpan.startCol;
            binSpan.anchorEnd = opSpan.endCol;
        }

        if (op == "<" || op == ">" || op == "<=" || op == ">=" ||
            op == "in" || op == "not in"){
            binSpan.anchorStart = binSpan.startCol;
            binSpan.anchorEnd = binSpan.endCol;
        }
        pendingCaret = binSpan;
        left = applyBinary(op, left, right);
        leftSpan = binSpan;
        if (failed) break;
        pendingCaret = span_t{};
    }
    if (outSpan) *outSpan = leftSpan;
    return left;
}

value vm::evalUnaryItem(const std::vector<eval_item_t>& items, size_t& index,

                                 span_t* outSpan){
    if (index >= items.size()){
        if (outSpan) *outSpan = span_t{};
        return makeNone();
    }
    const std::string& item = items[index].text;
    if (item == "not" || item == "-" || item == "+" || item == "~"){
        span_t opSpan = items[index].span;
        index++;
        span_t operandSpan;
        value operand = evalUnaryItem(items, index, &operandSpan);
        span_t unarySpan = combineSpan(opSpan, operandSpan);
        unarySpan.anchorStart = unarySpan.startCol;
        unarySpan.anchorEnd = unarySpan.endCol;
        pendingCaret = unarySpan;
        value result = applyUnary(item, operand);
        if (outSpan) *outSpan = unarySpan;
        if (failed) return result;
        pendingCaret = span_t{};
        return result;
    }
    span_t itemSpan = items[index].span;
    index++;
    if (outSpan) *outSpan = itemSpan;
    return evalOperand(item, itemSpan);
}

value vm::evalOperand(const std::string& item, const span_t& span){
    std::string text = trimString(item);
    if (text.empty()) return makeNone();
    if (text == "True") return makeBool(true);
    if (text == "False") return makeBool(false);
    if (text == "None") return makeNone();

    size_t p = 0;
    value v;
    std::string name;
    bool defined = true;
    if (text[0] == '('){

        int depth = 0;
        size_t i = 0;
        bool matched = false;
        for (; i < text.size(); ++i){
            if (text[i] == '('){
                depth++;
            } else if (text[i] == ')'){
                depth--;
                if (depth == 0){ i++; matched = true; break; }
            }
        }
        if (!matched) return makeNone();
        std::string inner = trimString(text.substr(1, i - 2));

        int commaDepth = 0;
        bool hasComma = false;
        for (char c : inner){
            if (c == '(' || c == '[' || c == '{') commaDepth++;
            else if (c == ')' || c == ']' || c == '}'){ if (commaDepth > 0) commaDepth--; }             else if (c == ',' && commaDepth == 0) hasComma = true;
        }
        if (hasComma || inner.empty()){
            std::vector<value> elements;
            for (auto& part : splitTopLevel(inner, ',')){
                if (trimString(part).empty()) continue;
                elements.push_back(evalExpr(part));
                if (failed) return makeNone();
            }
            v = makeTuple(elements);
        } else {
            v = evalExpr(inner);
        }
        p = i;
    } else if (text[0] == '['){

        int depth = 0;
        size_t i = 0;
        bool matched = false;
        for (; i < text.size(); ++i){
            if (text[i] == '['){
                depth++;
            } else if (text[i] == ']'){
                depth--;
                if (depth == 0){ i++; matched = true; break; }
            }
        }
        if (!matched) return makeNone();
        std::vector<value> elements;
        std::string inner = trimString(text.substr(1, i - 2));
        if (!inner.empty()){
            for (auto& part : splitTopLevel(inner, ',')){
                elements.push_back(evalExpr(part));
            }
        }
        v = makeList(elements);
        p = i;
    } else if (text[0] == '{'){

        int depth = 0;
        size_t i = 0;
        bool matched = false;
        for (; i < text.size(); ++i){
            if (text[i] == '{'){
                depth++;
            } else if (text[i] == '}'){
                depth--;
                if (depth == 0){ i++; matched = true; break; }
            }
        }
        if (!matched) return makeNone();
        dict_t items;
        std::string inner = trimString(text.substr(1, i - 2));
        if (!inner.empty()){
            for (auto& pairText : splitTopLevel(inner, ',')){

                int depth2 = 0;
                size_t colonPos = std::string::npos;
                for (size_t k = 0; k < pairText.size(); ++k){
                    char cc = pairText[k];
                    if (cc == '(' || cc == '[' || cc == '{') depth2++;
                    else if (cc == ')' || cc == ']' || cc == '}'){ if (depth2 > 0) depth2--; }                     else if (cc == ':' && depth2 == 0){ colonPos = k; break; }
                }
                if (pairText.size() >= 2 && pairText[0] == '*' && pairText[1] == '*'){

                    value source = evalExpr(pairText.substr(2));
                    if (failed) return makeNone();
                    dict_t* src = dictBacking(source);
                    if (!src){
                        raiseError("TypeError", "'" + pythonTypeName(source) +
                                                "' object is not a mapping");
                        return makeNone();
                    }
                    for (auto& entry : *src){
                        bool replaced = false;
                        for (auto& existing : items){
                            if (valuesEqual(existing.first, entry.first)){
                                existing.second = entry.second;
                                replaced = true;
                                break;
                            }
                        }
                        if (!replaced) items.push_back(entry);
                    }
                    continue;
                }
                if (colonPos == std::string::npos) continue;
                value key = evalExpr(pairText.substr(0, colonPos));
                if (failed) return makeNone();
                value item = evalExpr(pairText.substr(colonPos + 1));
                if (failed) return makeNone();
                if (!hashable(key)){
                    raiseError("TypeError", "cannot use '" + pythonTypeName(key) +
                                            "' as a dict key (unhashable type: '" +
                                            pythonTypeName(key) + "')");
                    return makeNone();
                }

                bool replaced = false;
                for (auto& existing : items){
                    if (valuesEqual(existing.first, key)){
                        existing.second = item;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced) items.push_back({key, item});
            }
        }
        v = makeDict(items);
        p = i;
    } else if (text.size() > 2 && text[0] == 'b' && text[1] == '#'){
        size_t q = 2;
        while (q < text.size() && std::isdigit(static_cast<unsigned char>(text[q]))) q++;
        long long index = -1;
        if (q > 2){
            try { index = std::stoll(text.substr(2, q - 2)); } catch (...){}
        }
        const script_t* activeScript = currentScript ? currentScript : &script;
        if (index >= 0 && index < static_cast<long long>(activeScript->StringLibrary.size())){
            v = makeString(activeScript->StringLibrary[static_cast<size_t>(index)]);
            v.type = "bytes";
        } else {
            v = makeNone();
        }
        p = q;
    } else if (text[0] == '#'){

        size_t q = 1;
        while (q < text.size() && std::isdigit(static_cast<unsigned char>(text[q]))) q++;
        long long index = -1;
        if (q > 1){
            try { index = std::stoll(text.substr(1, q - 1)); } catch (...){}
        }
        const script_t* activeScript = currentScript ? currentScript : &script;
        if (index >= 0 && index < static_cast<long long>(activeScript->StringLibrary.size())){
            v = makeString(activeScript->StringLibrary[static_cast<size_t>(index)]);
        } else {
            v = makeNone();
        }
        p = q;
    } else if (text.compare(0, 5, "@fret") == 0){

        size_t q = 5;
        while (q < text.size() && std::isdigit(static_cast<unsigned char>(text[q]))) q++;
        if (q > 5){
            long long id = 0;
            try { id = std::stoll(text.substr(5, q - 5)); } catch (...){}
            auto it = callResults.find(id);
            v = (it != callResults.end()) ? it->second : makeNone();
        } else {
            v = lastCallResult;
        }
        p = q;
    } else {

        size_t r = 0;
        if (r < text.size() && (text[r] == '-' || text[r] == '+')) r++;
        bool digits = false;
        bool dot = false;
        bool hasExp = false;
        bool expSign = false;
        bool isHex = (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'));
        bool isBin = (text.size() >= 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B'));
        size_t startScan = 0;
        if (isHex || isBin){ startScan = 2; digits = true; }
        while (startScan < text.size()){
            char c = text[startScan];
            if (isHex && std::isxdigit(static_cast<unsigned char>(c))){
                digits = true;
                startScan++;
            } else if (isBin && (c == '0' || c == '1')){
                digits = true;
                startScan++;
            } else break;
        }
        bool complexLiteral = false;
        if (isHex){
            r = text.size();
        } else if (isBin){
            if (startScan == text.size() && startScan > 2) r = text.size();
            else { r = 0; digits = false; }
        } else {
            while (r < text.size()){
                char c = text[r];
                if (std::isdigit(static_cast<unsigned char>(c))){
                    digits = true;
                    r++;
                } else if (c == '.' && !dot && !hasExp){
                    dot = true;
                    r++;
                } else if ((c == 'e' || c == 'E') && !hasExp && digits){
                    hasExp = true;
                    r++;
                } else if ((c == '+' || c == '-') && hasExp && !expSign && r > 0){
                    expSign = true;
                    r++;
                } else {
                    break;
                }
            }

// 复数虚部后缀: 数值部分紧跟 j/J 时按复数字面量处理
// complex imaginary suffix: treat as complex literal when digits are followed by j/J
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠬᠤᠶᠠᠳᠤᠭᠠᠷ ᠬᠡᠰᠡᠭ ᠦᠨ ᠳᠠᠭᠠᠪᠤᠷᠢ
            if (digits && r < text.size() && (text[r] == 'j' || text[r] == 'J')){
                complexLiteral = true;
                r++;
            }
        }
        bool numericPrefix = digits &&
            (r >= text.size() || text[r] == '[' || text[r] == '.' || text[r] == '(');
        if (numericPrefix){
            std::string atom = text.substr(0, complexLiteral ? r - 1 : r);
            if (complexLiteral){
                double imagVal = 0.0;
                try { imagVal = std::stod(atom); } catch (...) { imagVal = 0.0; }
                v = makeComplex(0.0, imagVal);
            } else {
                bool isFloat = (dot || hasExp);
                if (isFloat){
                    v = makeFloat(std::stod(atom));
                } else {

                    long long intVal = 0;
                    try {
                        int base = 10;
                        std::string digitsPart = atom;
                        if (atom.size() >= 2 && atom[0] == '0' &&
                            (atom[1] == 'x' || atom[1] == 'X')){
                            base = 16;
                        } else if (atom.size() >= 2 && atom[0] == '0' &&
                                   (atom[1] == 'b' || atom[1] == 'B')){
                            base = 2;
                            digitsPart = atom.substr(2);
                        }
                        size_t idx = 0;
                        intVal = std::stoll(digitsPart, &idx, base);
                    } catch (...) {
                        intVal = 0;
                    }
                    v = makeInt(intVal);
                }
            }
            p = r;
        } else {

            while (p < text.size() && text[p] != '[' && text[p] != '.' && text[p] != '('){
                name += text[p];
                p++;
            }
            if (name.empty()) return makeNone();
            if (name == "None" || name == "True" || name == "False"){
                v = name == "None" ? makeNone() : makeBool(name == "True");
            } else {
                value* variablePtr = findVariablePtr(name);
                if (variablePtr){
                    v = *variablePtr;
                } else {
                    v = findVariable(name);
                    if (!failed && v.type == "none"){
                        bool isLibrary = false;
                        for (auto* library : cffiLibraries){
                            if (library && library->name == name) isLibrary = true;
                        }
                        for (auto& library : scriptLibraries){
                            if (library.first == name) isLibrary = true;
                        }

                        function_t* func = findFunction(name);
                        class_t* cls = findClass(name);
                        if (func){
                            value fv;
                            fv.type = "function"; fv.ptr = func;
                            setVariable(name, fv);
                            v = fv;
                            defined = true;
                        } else if (cls){
                            value cv;
                            cv.type = "class"; cv.ptr = cls;
                            setVariable(name, cv);
                            v = cv;
                            defined = true;
                        } else if (!isLibrary){

                            bool foundBuiltin = false;
                            for (auto* library : cffiLibraries){
                                if (!library || library->name != "builtins") continue;
                                auto fnIt = library->functions.find(name);
                                if (fnIt != library->functions.end() && fnIt->second != nullptr){


                                    ffi_instance* ni = new ffi_instance();
                                    ni->library = library;
                                    ni->klass = nullptr;
                                    ni->typeName = name;
                                    ni->handle = fnIt->second;
                                    value callable("native_object",
                                                     keep(vk_native_object, ni), this);
                                    v = callable;
                                    defined = true;
                                    foundBuiltin = true;
                                    break;
                                }
                                auto clsIt = library->classes.find(name);
                                if (clsIt != library->classes.end()){

                                    ffi_instance* ni = new ffi_instance();
                                    ni->library = library;
                                    ni->klass = &clsIt->second;
                                    value instance("native_object",
                                                     keep(vk_native_object, ni), this);
                                    v = instance;
                                    defined = true;
                                    foundBuiltin = true;
                                    break;
                                }
                                auto dataIt = library->data.find(name);
                                if (dataIt != library->data.end()){
                                    value bnResult = makeNone();
                                    if (cffiCallData(*library, name, *this, bnResult)){
                                        v = bnResult;
                                        defined = true;
                                        foundBuiltin = true;
                                        break;
                                    }
                                }
                            }
                            if (!foundBuiltin){
                                // 内置异常类型可作一等值 (如 assertRaises(ValueError, f))
                                // Builtin exception types as first-class values
                                // builtin ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠲᠥᠷᠥᠯ ᠦᠳ ᠢ ᠦᠨ᠎ᠡ ᠪᠣᠯᠭᠠᠬᠤ
                                const std::vector<std::string>& excNames = builtinExceptionNames();
                                if (std::find(excNames.begin(), excNames.end(), name) != excNames.end()){
                                    ffi_instance* ni = new ffi_instance();
                                    ni->library = nullptr;
                                    ni->klass = nullptr;
                                    ni->typeName = name;
                                    ni->handle = nullptr;
                                    value excType("native_object",
                                                     keep(vk_native_object, ni), this);
                                    v = excType;
                                    defined = true;
                                    foundBuiltin = true;
                                }
                            }
                            if (!foundBuiltin){
                                pendingCaret = span;
                                pendingCaret.anchorStart = span.startCol;
                                pendingCaret.anchorEnd = span.endCol;
                                std::string message = "name '" + name + "' is not defined";
                                std::string suggestion = suggestName(name);
                                if (!suggestion.empty()){
                                    message += ". Did you mean: '" + suggestion + "'?";
                                }
                                raiseError("NameError", message);
                                return makeNone();
                            }
                        }
                        defined = false;
                    }
                }
            }
        }
    }
    size_t partIndex = 0;

    auto setIndexCaret = [&](size_t k){
        span_t caret = span;
        if (k < span.parts.size()){
            int partStart = span.parts[k].first;
            int partEnd = span.parts[k].second;
            caret.startCol = span.startCol;
            caret.endCol = partEnd;
            caret.anchorStart = partStart;
            caret.anchorEnd = partEnd;
        } else {
            caret.anchorStart = caret.startCol;
            caret.anchorEnd = caret.endCol;
        }
        pendingCaret = caret;
    };
    while (p < text.size()){
        if (text[p] == '['){
            size_t close = p + 1;
            int depth = 1;
            while (close < text.size()){
                if (text[close] == '['){
                    depth++;
                } else if (text[close] == ']'){
                    depth--;
                    if (depth == 0) break;
                }
                close++;
            }
            if (close >= text.size()) break;
            std::string indexExpr = text.substr(p + 1, close - p - 1);

            if (v.type == "object" && v.ptr){
                value indexValue = evalExpr(indexExpr);
                if (failed) return makeNone();
                auto* inst = static_cast<instance*>(v.ptr);
                function_t* getitem = findMethod(inst->klass, "__getitem__");
                if (getitem){
                    value selfCopy = v;
                    value r = invoke(getitem, {indexValue}, &selfCopy);
                    if (!hasError()){
                        v = r;
                        partIndex++;
                        p = close + 1;
                        continue;
                    }
                    clearPendingExceptionState();
                }

                if (dict_t* items = dictBacking(v)){
                    if (!hashable(indexValue)){
                        setIndexCaret(partIndex);
                        raiseError("TypeError", "cannot use '" + pythonTypeName(indexValue) +
                                                "' as a dict key (unhashable type: '" +
                                                pythonTypeName(indexValue) + "')");
                        return makeNone();
                    }
                    value* found = dictFind(v, indexValue);
                    if (found == nullptr){

                        function_t* missing = findMethod(inst->klass, "__missing__");
                        if (missing){
                            value selfCopy = v;
                            value r = invoke(missing, {indexValue}, &selfCopy);
                            if (hasError()) return makeNone();
                            v = r;
                            partIndex++;
                            p = close + 1;
                            continue;
                        }
                        setIndexCaret(partIndex);
                        raiseError("KeyError", pythonRepr(*this, indexValue));
                        return makeNone();
                    }
                    v = *found;
                    partIndex++;
                    p = close + 1;
                    continue;
                }
                if (inst->container.type == "list"){
                    if (indexValue.type != "int" && indexValue.type != "bool"){
                        setIndexCaret(partIndex);
                        raiseError("TypeError", "list indices must be integers or slices, not " +
                                                pythonTypeName(indexValue));
                        return makeNone();
                    }
                    auto* items = static_cast<std::vector<value>*>(inst->container.ptr);
                    long long index = toInt(indexValue);
                    if (index < 0) index += static_cast<long long>(items->size());
                    if (index >= 0 && index < static_cast<long long>(items->size())){
                        v = (*items)[static_cast<size_t>(index)];
                        partIndex++;
                        p = close + 1;
                        continue;
                    }
                    setIndexCaret(partIndex);
                    raiseError("IndexError", "list index out of range");
                    return makeNone();
                }

                setIndexCaret(partIndex);
                raiseError("TypeError", "'" + pythonTypeName(v) + "' object is not subscriptable");
                return makeNone();
            }

            if (v.type == "native_object" && v.ptr){
                auto* ninst = static_cast<ffi_instance*>(v.ptr);
                if (ninst->klass && ninst->klass->methods.count("__getitem__")){
                    value indexValue = evalExpr(indexExpr);
                    if (failed) return makeNone();
                    value r;
                    if (cffiCallMethod(*this, v, "__getitem__", {indexValue}, r)){
                        if (hasError()) return makeNone();
                        v = r;
                        partIndex++;
                        p = close + 1;
                        continue;
                    }
                }
            }
            if (v.type == "class" && v.ptr){
                auto* klass = static_cast<class_t*>(v.ptr);
                if (klass->builtinBase.rfind("enum:", 0) == 0){
                    value enumKey = evalExpr(indexExpr);
                    if (failed) return makeNone();
                    if (enumKey.type == "str"){
                        const std::string& memberName = *static_cast<std::string*>(enumKey.ptr);
                        if (klass->attributes){
                            auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
                            auto it = attrs->find(memberName);
                            if (it != attrs->end() && it->second.type == "native_object"){
                                v = it->second;
                                partIndex++;
                                p = close + 1;
                                continue;
                            }
                        }
                        setIndexCaret(partIndex);
                        raiseError("KeyError", "'" + memberName + "'");
                        return makeNone();
                    }
                    setIndexCaret(partIndex);
                    raiseError("TypeError", "enum keys must be strings");
                    return makeNone();
                }
            }
            if (v.type != "list" && v.type != "str" &&
                v.type != "dict" && v.type != "tuple" &&
                v.type != "range" && v.type != "bytes" &&
                v.type != "bytearray"){
                setIndexCaret(partIndex);
                raiseError("TypeError", "'" + pythonTypeName(v) + "' object is not subscriptable");
                return makeNone();
            }

            if (isSliceText(indexExpr) &&
                (v.type == "list" || v.type == "tuple" ||
                 v.type == "str" || v.type == "range" ||
                 v.type == "bytes" || v.type == "bytearray")){
                long long length = 0;
                if (v.type == "list" || v.type == "tuple"){
                    length = static_cast<long long>(
                        static_cast<std::vector<value>*>(v.ptr)->size());
                } else if (v.type == "str"){
                    length = static_cast<long long>(static_cast<std::string*>(v.ptr)->size());
                } else if (v.type == "bytes" || v.type == "bytearray"){
                    length = static_cast<long long>(static_cast<std::string*>(v.ptr)->size());
                } else {
                    length = static_cast<range_t*>(v.ptr)->count();
                }
                long long s = 0, e = 0, st = 1;
                if (!sliceBounds(indexExpr, length, s, e, st)) return makeNone();
                if (v.type == "list" || v.type == "tuple"){
                    auto* items = static_cast<std::vector<value>*>(v.ptr);
                    std::vector<value> out;
                    for (long long k = s; (st > 0) ? k < e : k > e; k += st){
                        out.push_back((*items)[static_cast<size_t>(k)]);
                    }
                    v = (v.type == "list") ? makeList(out) : makeTuple(out);
                } else if (v.type == "str"){
                    auto* str = static_cast<std::string*>(v.ptr);
                    std::string out;
                    for (long long k = s; (st > 0) ? k < e : k > e; k += st){
                        out.push_back((*str)[static_cast<size_t>(k)]);
                    }
                    v = makeString(out);
                } else if (v.type == "bytes" || v.type == "bytearray"){
                    auto* raw = static_cast<std::string*>(v.ptr);
                    std::string out;
                    for (long long k = s; (st > 0) ? k < e : k > e; k += st){
                        out.push_back((*raw)[static_cast<size_t>(k)]);
                    }
                    value sliced = makeString(out);
                    sliced.type = v.type;
                    v = sliced;
                } else {
                    auto* range = static_cast<range_t*>(v.ptr);
                    long long count = (st > 0) ? (e > s ? (e - s + st - 1) / st : 0)
                                               : (s > e ? (s - e - st - 1) / (-st) : 0);
                    long long newStart = range->at(s);
                    long long newStep = range->step * st;
                    v = makeRange(newStart, newStart + count * newStep, newStep);
                }
                partIndex++;
                p = close + 1;
                continue;
            }
            value indexValue = evalExpr(indexExpr);
            if (failed) return makeNone();
            if (v.type == "dict"){
                if (!hashable(indexValue)){
                    setIndexCaret(partIndex);
                    raiseError("TypeError", "cannot use '" + pythonTypeName(indexValue) +
                                            "' as a dict key (unhashable type: '" +
                                            pythonTypeName(indexValue) + "')");
                    return makeNone();
                }
                value* found = dictFind(v, indexValue);
                if (found == nullptr){
                    setIndexCaret(partIndex);
                    raiseError("KeyError", pythonRepr(*this, indexValue));
                    return makeNone();
                }
                v = *found;
                partIndex++;
                p = close + 1;
                continue;
            }
            if (indexValue.type != "int" && indexValue.type != "bool"){
                setIndexCaret(partIndex);
                if (v.type == "list"){
                    raiseError("TypeError", "list indices must be integers or slices, not " +
                                                pythonTypeName(indexValue));
                } else if (v.type == "tuple"){
                    raiseError("TypeError", "tuple indices must be integers or slices, not " +
                                                pythonTypeName(indexValue));
                } else if (v.type == "range"){
                    raiseError("TypeError", "range indices must be integers or slices, not " +
                                                pythonTypeName(indexValue));
                } else if (v.type == "bytes" || v.type == "bytearray"){
                    raiseError("TypeError", "byte indices must be integers or slices, not " +
                                                pythonTypeName(indexValue));
                } else {
                    raiseError("TypeError", "string indices must be integers, not '" +
                                                pythonTypeName(indexValue) + "'");
                }
                return makeNone();
            }
            long long index = toInt(indexValue);
            if (v.type == "list" || v.type == "tuple"){
                auto* list = static_cast<std::vector<value>*>(v.ptr);
                if (index < 0) index += static_cast<long long>(list->size());
                if (index >= 0 && index < static_cast<long long>(list->size())){
                    v = (*list)[static_cast<size_t>(index)];
                } else {
                    setIndexCaret(partIndex);
                    raiseError("IndexError", v.type == "tuple" ? "tuple index out of range"
                                                                   : "list index out of range");
                    return makeNone();
                }
            } else if (v.type == "range"){
                auto* range = static_cast<range_t*>(v.ptr);
                long long count = range->count();
                if (index < 0) index += count;
                if (index >= 0 && index < count){
                    v = makeInt(range->at(index));
                } else {
                    setIndexCaret(partIndex);
                    raiseError("IndexError", "range object index out of range");
                    return makeNone();
                }
            } else if (v.type == "bytes" || v.type == "bytearray"){
                auto* raw = static_cast<std::string*>(v.ptr);
                if (index < 0) index += static_cast<long long>(raw->size());
                if (index >= 0 && index < static_cast<long long>(raw->size())){
                    v = makeInt(static_cast<unsigned char>((*raw)[static_cast<size_t>(index)]));
                } else {
                    setIndexCaret(partIndex);
                    raiseError("IndexError", "index out of range");
                    return makeNone();
                }
            } else if (v.type == "native_object" && v.ptr){
                std::vector<value> callArgs;
                callArgs.push_back(indexValue);
                value result;
                if (cffiCallMethod(*this, v, "__getitem__", callArgs, result)){
                    v = result;
                } else {
                    setIndexCaret(partIndex);
                    raiseError("TypeError", "'" + pythonTypeName(v) + "' object is not subscriptable");
                    return makeNone();
                }
            } else {
                auto* str = static_cast<std::string*>(v.ptr);
                if (index < 0) index += static_cast<long long>(str->size());
                if (index >= 0 && index < static_cast<long long>(str->size())){
                    v = makeString(std::string(1, (*str)[static_cast<size_t>(index)]));
                } else {
                    setIndexCaret(partIndex);
                    raiseError("IndexError", "string index out of range");
                    return makeNone();
                }
            }
            partIndex++;
            p = close + 1;
        } else if (text[p] == '.'){
            p++;
            std::string member;
            while (p < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[p])) || text[p] == '_')){
                member += text[p];
                p++;
            }
            std::string fullName = name + "." + member;
            if (v.type == "object" && v.ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
                struct instance* instance = static_cast<struct instance*>(v.ptr);

                if (member == "__dict__"){
                    value d("dict", instance->attributes, this);
                    v = d;
                } else {
                    value attrDict("dict", instance->attributes, this);
                    value keyStr = makeString(member);
                    value* foundAttr = dictFind(attrDict, keyStr);


                    bool resolved = false;
                    value* clsAttrPtr = instance->klass ?
                        findClassAttrInChain(instance->klass, member) : nullptr;
                    if (instance->klass && clsAttrPtr){

                        {
                            const value& clsVal = *clsAttrPtr;
                            bool hasSetOrDel = valueHasDescriptorMethod(clsVal, "__set__") ||
                                               valueHasDescriptorMethod(clsVal, "__delete__");
                            bool hasGet = valueHasDescriptorMethod(clsVal, "__get__");
                            if (hasSetOrDel && hasGet){

                                value owner("class", instance->klass);
                                value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                                          {v, owner});
                                if (!hasError()){
                                    v = descResult;
                                    resolved = true;
                                } else {
                                    return makeNone();
                                }
                            }
                        }
                    }
                    if (!resolved && foundAttr){
                        v = *foundAttr;
                        resolved = true;
                    }
                    if (!resolved && instance->klass){

                        if (clsAttrPtr){
                            const value& clsVal = *clsAttrPtr;
                            if (valueHasDescriptorMethod(clsVal, "__get__")){

                                value owner("class", instance->klass);
                                value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                                          {v, owner});
                                if (!hasError()){
                                    v = descResult;
                                    resolved = true;
                                } else {
                                    return makeNone();
                                }
                            } else {
                                v = clsVal;
                                resolved = true;
                            }
                        }
                        if (!resolved){

                            function_t* method = findMethod(instance->klass, member);
                            if (method){
                                v.type = "function";
                                v.ptr = method;
                                resolved = true;
                            }
                        }
                        if (!resolved){

                            if (!instance->container.type.empty() &&
                                instance->container.type != "none" &&
                                isContainerMethodName(instance->container.type, member)){
                                auto* bound = new native_bound_method();
                                bound->self = v;
                                bound->methodName = member;
                                v.type = "native_method";
                                v.ptr = bound;
                                resolved = true;
                            }
                        }
                        if (!resolved){

                            function_t* getattrMethod = findMethod(instance->klass, "__getattr__");
                            if (getattrMethod){
                                value selfCopy = v;
                                value memberName = makeString(member);
                                value r = invoke(getattrMethod, {memberName}, &selfCopy);
                                if (!hasError()){
                                    v = r;
                                } else {
                                    return makeNone();
                                }
                            } else {

                                std::string typeName = instance->klass ? instance->klass->name : "object";
                                raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + member + "'");
                                return makeNone();
                            }
                        }
                    }
                    }
                } else if (v.type == "native_object" && v.ptr){
                    auto* instance = static_cast<ffi_instance*>(v.ptr);
                    auto it = instance->attributes.find(member);
                    if (it != instance->attributes.end()){
                        v = it->second;
                    } else {
                        bool resolved = false;

                        if (instance->klass){
                            auto methodIt = instance->klass->methods.find(member);
                            if (methodIt != instance->klass->methods.end() && methodIt->second != nullptr){


                                auto* bound = new native_bound_method();
                                bound->self = v;
                                bound->methodName = member;
                                v.type = "native_method";
                                v.ptr = bound;
                                resolved = true;
                            }
                        }
                        if (!resolved && member == "fromkeys" && instance->typeName == "dict"){
                            auto* bound = new native_bound_method();
                            bound->self = makeDict({});
                            bound->methodName = "fromkeys";
                            v.type = "native_method";
                            v.ptr = bound;
                            resolved = true;
                        }
                        if (!resolved && instance->dynamicGet){
                            value dynamicValue = makeNone();
                            resolved = instance->dynamicGet(*this, instance->handle, member, dynamicValue);
                            if (resolved) v = dynamicValue;
                        }
                        if (!resolved && member == "__name__"){
                            std::string typeName = !instance->typeName.empty()
                                ? instance->typeName
                                : (instance->klass ? instance->klass->name : "object");
                            v = makeString(typeName);
                            resolved = true;
                        }
                        if (!resolved){
                            v = findVariable(fullName);
                            if (!failed && v.type == "none"){
                                std::string typeName = !instance->typeName.empty() ? instance->typeName
                                                  : (instance->klass ? instance->klass->name : "object");
                                raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + member + "'");
                                return makeNone();
                            }
                        }
                    }
                } else if (v.type == "super" && v.ptr){
                    v = resolveSuperMember(*this, static_cast<super_object*>(v.ptr), member);
                    if (hasError()) return makeNone();
                } else if (v.type == "function" && v.ptr){
                    auto* fn = static_cast<function_t*>(v.ptr);
                    if (member == "__name__"){
                        v = makeString(fn->name);
                    } else if (member == "__doc__"){
                        v = makeNone();
                    } else {
                        raiseError("AttributeError", "'function' object has no attribute '" + member + "'");
                        return makeNone();
                    }
                } else if (v.type == "class" && v.ptr){
                    auto* klass = static_cast<class_t*>(v.ptr);

                    value* classAttr = findClassAttrInChain(klass, member);
                    bool resolved = false;
                    if (classAttr){

                        if (valueHasDescriptorMethod(*classAttr, "__get__")){
                            value noneInst = makeNone();
                            value descResult = invokeDescriptorMethod(*this, *classAttr, "__get__",
                                                                      {noneInst, v});
                            if (!hasError()){
                                v = descResult;
                                resolved = true;
                            } else {
                                return makeNone();
                            }
                        } else {
                            v = *classAttr;
                            resolved = true;
                        }
                    }
                    if (!resolved){
                        function_t* method = findMethod(klass, member);
                        if (method){
                            v.type = "function";
                            v.ptr = method;
                            resolved = true;
                        }
                    }
                    if (!resolved && member == "__name__"){
                        v = makeString(klass->name);
                        resolved = true;
                    }
                    if (!resolved){
                        raiseError("AttributeError", "'" + klass->name + "' object has no attribute '" + member + "'");
                        return makeNone();
                    }
                } else {
                std::string baseType = pythonTypeName(v);
                bool found = false;

                if (v.type == "none" && !name.empty()){
                    for (auto* library : cffiLibraries){
                        if (!library || library->name != name) continue;
                        value dataValue = makeNone();
                        if (cffiCallData(*library, member, *this, dataValue)){
                            if (failed) return makeNone();
                            v = dataValue;
                            found = true;
                        } else if (!failed){
                            auto funcIt = library->functions.find(member);
                            if (funcIt != library->functions.end() && funcIt->second != nullptr){
                                auto* ni = new ffi_instance();
                                ni->library = library;
                                ni->klass = nullptr;
                                ni->typeName = member;
                                ni->handle = funcIt->second;
                                v = value("native_object", keep(vk_native_object, ni), this);
                                found = true;
                            } else {
                                auto classIt = library->classes.find(member);
                                if (classIt != library->classes.end()){
                                    v = makeNativeObject(library, &classIt->second, nullptr);
                                    found = true;
                                }
                            }
                        }
                        break;
                    }
                }
                if (!found && !name.empty()){
                    value fallback = findVariable(fullName);
                    if (fallback.type != "none"){
                        v = fallback;
                        found = true;
                    }
                }

                if (!found && v.type == "str" && member == "__name__"){
                    v = makeString(toString(v));
                    found = true;
                }
                if (!found && v.type == "native_object" && v.ptr && member == "__name__"){
                    auto* inst = static_cast<ffi_instance*>(v.ptr);
                    std::string typeName = !inst->typeName.empty()
                        ? inst->typeName
                        : (inst->klass ? inst->klass->name : "object");
                    v = makeString(typeName);
                    found = true;
                }
                if (!found && !failed && defined){
                    pendingCaret = span;
                    pendingCaret.anchorStart = span.startCol;
                    pendingCaret.anchorEnd = span.endCol;
                    raiseError("AttributeError", "'" + baseType + "' object has no attribute '" + member + "'");
                    return makeNone();
                }
                }
            name = fullName;
            partIndex++;
        } else {
            break;
        }
    }
    return v;
}
value vm::applyUnary(const std::string& op, const value& v){
    if (op == "not"){
        return makeBool(!truthy(v));
    }
    auto tryScriptMagic = [&](const std::string& methodName) -> std::pair<bool, value> {
        if (v.type == "native_object" && v.ptr){
            auto* ninst = static_cast<ffi_instance*>(v.ptr);
            if (!ninst->klass) return {false, makeNone()};
            if (ninst->klass->methods.find(methodName) == ninst->klass->methods.end())
                return {false, makeNone()};
            value result;
            cffiCallMethod(*this, v, methodName, {}, result);
            return {true, result};
        }
        if (v.type != "object" || !v.ptr) return {false, makeNone()};
        auto* inst = static_cast<instance*>(v.ptr);
        if (!inst->klass) return {false, makeNone()};
        function_t* method = findMethod(inst->klass, methodName);
        if (!method) return {false, makeNone()};
        value selfCopy = v;
        value result = invoke(method, {}, &selfCopy);
        if (hasError()) return {true, result};
        return {true, result};
    };
    bool isNumber = (v.type == "int" || v.type == "float" || v.type == "bool");
    if (op == "+"){
        if (!isNumber){

// 一元 + 作用于复数时原样返回
// unary + on a complex returns it unchanged
// ᠨᠢᠭᠡᠳᠦᠭᠡᠷ + ᠰᠢᠯᠢ ᠲᠣ᠎ ᠳᠤ ᠦᠢᠯᠡᠳᠬᠦ
            double cRe, cIm;
            if (complexParts(v, cRe, cIm)) return v;
            auto [ok, r] = tryScriptMagic("__pos__");
            if (ok) return r;
            raiseError("TypeError", "bad operand type for unary +: '" + pythonTypeName(v) + "'");
            return makeNone();
        }
        return v;
    }
    if (op == "-"){
        if (v.type == "float"){
            return makeFloat(-toFloat(v));
        }
        if (v.type == "int" || v.type == "bool"){
            return makeInt(-toInt(v));
        }

// 复数取负: 实部虚部同时取负
// complex negation: negate both real and imaginary parts
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢ ᠰᠦᠯᠢᠵᠢᠨ᠎ᠡ
        {
            double cRe, cIm;
            if (complexParts(v, cRe, cIm)) return makeComplex(-cRe, -cIm);
        }
        auto [ok, r] = tryScriptMagic("__neg__");
        if (ok) return r;
        raiseError("TypeError", "bad operand type for unary -: '" + pythonTypeName(v) + "'");
        return makeNone();
    }
    if (op == "~"){
        if (v.type == "int" || v.type == "bool"){
            return makeInt(~toInt(v));
        }
        auto [ok, r] = tryScriptMagic("__invert__");
        if (ok) return r;
        raiseError("TypeError", "bad operand type for unary ~: '" + pythonTypeName(v) + "'");
        return makeNone();
    }

    return v;
}
value vm::applyFormat(const value& v, const std::string& spec){

    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst && inst->klass){
            function_t* fmtMethod = findMethod(inst->klass, "__format__");
            if (fmtMethod){
                value selfCopy = v;
                value specArg = makeString(spec);
                value result = invoke(fmtMethod, {specArg}, &selfCopy);
                if (hasError()) return makeNone();
                return result;
            }

            if (spec.empty()){

                function_t* strMethod = findMethod(inst->klass, "__str__");
                if (strMethod){
                    value selfCopy = v;
                    value result = invoke(strMethod, {}, &selfCopy);
                    if (hasError()) return makeNone();
                    return result;
                }

                function_t* reprMethod = findMethod(inst->klass, "__repr__");
                if (reprMethod){
                    value selfCopy = v;
                    value result = invoke(reprMethod, {}, &selfCopy);
                    if (hasError()) return makeNone();
                    return result;
                }

                return makeString("<" + pythonTypeName(v) + " object>");
            }

            raiseError("TypeError", "unsupported format character");
            return makeNone();
        }
    }
    if (v.type == "str" || v.type == "int" || v.type == "float" || v.type == "bool"){
        if (spec.empty()) return makeString(toString(v));

        size_t p = 0;
        char fill = ' ';
        char align = 0;
        auto isAlign = [](char c){ return c == '<' || c == '>' || c == '^' || c == '='; };
        if (spec.size() >= 2 && isAlign(spec[1])){
            fill = spec[0];
            align = spec[1];
            p = 2;
        } else if (!spec.empty() && isAlign(spec[0])){
            align = spec[0];
            p = 1;
        }
        char signCh = 0;
        if (p < spec.size() && (spec[p] == '+' || spec[p] == '-' || spec[p] == ' ')){
            signCh = spec[p++];
        }
        bool alternate = false;
        if (p < spec.size() && spec[p] == '#'){ alternate = true; p++; }
        if (p < spec.size() && spec[p] == '0'){
            p++;
            if (!align){ align = '='; fill = '0'; }
        }
        int width = 0;
        while (p < spec.size() && std::isdigit(static_cast<unsigned char>(spec[p]))) width = width * 10 + (spec[p++] - '0');
        bool grouping = false;
        if (p < spec.size() && spec[p] == ','){ grouping = true; p++; }
        int precision = -1;
        if (p < spec.size() && spec[p] == '.'){
            p++;
            precision = 0;
            while (p < spec.size() && std::isdigit(static_cast<unsigned char>(spec[p]))) precision = precision * 10 + (spec[p++] - '0');
        }
        char type = 0;
        if (p < spec.size()) type = spec[p++];
        if (p < spec.size()){
            raiseError("ValueError", "Invalid format specifier '" + spec + "'");
            return makeNone();
        }
        std::string body;
        bool numeric = (v.type != "str");
        if (!numeric){
            if (type && type != 's'){
                raiseError("ValueError", "Unknown format code '" + std::string(1, type) +
                                         "' for object of type 'str'");
                return makeNone();
            }
            body = toString(v);
            if (precision >= 0 && static_cast<int>(body.size()) > precision){
                body = body.substr(0, static_cast<size_t>(precision));
            }
            if (!align) align = '<';
        } else {
            bool isFloat = (v.type == "float");
            char effectiveType = type;
            if (!effectiveType) effectiveType = isFloat ? 'g' : 'd';
            if (!isFloat && (effectiveType == 'f' || effectiveType == 'e' || effectiveType == 'g' || effectiveType == '%')){
                isFloat = true;
            }
            if (!isFloat && (effectiveType == 'd' || effectiveType == 'n' ||
                             effectiveType == 'b' || effectiveType == 'o' ||
                             effectiveType == 'x' || effectiveType == 'X')){
                long long iv = toInt(v);
                if (effectiveType == 'b' || effectiveType == 'o' || effectiveType == 'x' || effectiveType == 'X'){
                    std::string digits;
                    bool neg = iv < 0;
                    unsigned long long base = (effectiveType == 'b') ? 2ULL : (effectiveType == 'o') ? 8ULL : 16ULL;
                    unsigned long long uv = (iv < 0) ? static_cast<unsigned long long>(-(iv + 1)) + 1ULL
                                                     : static_cast<unsigned long long>(iv);
                    if (uv == 0) digits = "0";
                    while (uv){ unsigned long long d = uv % base; digits.push_back(static_cast<char>(d < 10 ? '0' + d : (effectiveType == 'X' ? 'A' : 'a') + static_cast<char>(d - 10))); uv /= base; }
                    std::reverse(digits.begin(), digits.end());
                    if (alternate) digits = (effectiveType == 'b') ? "0b" + digits : (effectiveType == 'o') ? "0o" + digits : (effectiveType == 'X') ? "0X" + digits : "0x" + digits;
                    body = (neg ? "-" : "") + digits;
                } else {
                    body = std::to_string(iv < 0 ? -iv : iv);
                    if (grouping){
                        for (int k = static_cast<int>(body.size()) - 3; k > 0; k -= 3) body.insert(static_cast<size_t>(k), ",");
                    }
                    if (iv < 0) body = "-" + body;
                }
            } else {
                double dv = toFloat(v);
                if (effectiveType == '%'){
                    dv *= 100.0;
                }
                std::ostringstream out;
                if (effectiveType == 'f'){
                    out << std::fixed << std::setprecision(precision >= 0 ? precision : 6) << dv;
                    body = out.str();
                    if (alternate && precision == 0 && body.find('.') == std::string::npos) body += ".";
                } else if (effectiveType == 'e' || effectiveType == 'E'){
                    out << std::scientific << std::setprecision(precision >= 0 ? precision : 6) << dv;
                    body = out.str();
                    if (effectiveType == 'E'){
                        for (auto& ch : body) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    }
                } else if (effectiveType == 'g' || effectiveType == 'G'){
                    if (precision >= 0){
                        out << std::setprecision(precision) << dv;
                    } else {
                        out << std::setprecision(15) << dv;
                    }
                    body = out.str();
                    if (effectiveType == 'G'){
                        for (auto& ch : body) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    }
                } else if (effectiveType == '%'){
                    out << std::fixed << std::setprecision(precision >= 0 ? precision : 6) << dv;
                    body = out.str();
                } else {
                    raiseError("ValueError", "Unknown format code '" + std::string(1, effectiveType) +
                                             "' for object of type 'float'");
                    return makeNone();
                }
                if (effectiveType == '%') body += "%";
            }
            if (signCh && !body.empty() && body[0] != '-'){
                if (signCh == '+') body = "+" + body;
                else if (signCh == ' ') body = " " + body;
            }
            if (!align) align = '>';
        }

        if (width > static_cast<int>(body.size())){
            int pad = width - static_cast<int>(body.size());
            if (align == '<'){
                body += std::string(static_cast<size_t>(pad), fill);
            } else if (align == '^'){
                int left = pad / 2;
                body = std::string(static_cast<size_t>(left), fill) + body +
                       std::string(static_cast<size_t>(pad - left), fill);
            } else if (align == '=' && numeric){

                std::string prefix;
                if (!body.empty() && (body[0] == '-' || body[0] == '+' || body[0] == ' ')){
                    prefix = body.substr(0, 1);
                    body = body.substr(1);
                }
                body = prefix + std::string(static_cast<size_t>(pad), fill) + body;
            } else {
                body = std::string(static_cast<size_t>(pad), fill) + body;
            }
        }
        return makeString(body);
    }

    return makeString(toString(v));
}

value vm::percentFormat(const value& format, const value& arg){
    std::string fmt = toString(format);
    std::vector<value> values;
    dict_t* mapping = dictBacking(arg);
    if (!mapping){
        if (arg.type == "tuple" && arg.ptr)
            values = *static_cast<std::vector<value>*>(arg.ptr);
        else
            values.push_back(arg);
    }
    std::string out;
    size_t i = 0;
    size_t valueIndex = 0;
    while (i < fmt.size()){
        if (fmt[i] != '%'){ out += fmt[i++]; continue; }
        i++;
        if (i >= fmt.size()) break;
        if (fmt[i] == '%'){ out += '%'; i++; continue; }
        std::string key;
        bool hasKey = false;
        if (fmt[i] == '('){
            size_t close = fmt.find(')', i + 1);
            if (close == std::string::npos){
                raiseError("ValueError", "incomplete format key");
                return makeNone();
            }
            key = fmt.substr(i + 1, close - i - 1);
            hasKey = true;
            i = close + 1;
        }
        std::string flags;
        while (i < fmt.size() && std::strchr("-+ #0", fmt[i])){
            flags += fmt[i++];
        }
        std::string width;
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i])))
            width += fmt[i++];
        std::string precision;
        bool hasPrecision = false;
        if (i < fmt.size() && fmt[i] == '.'){
            hasPrecision = true;
            precision += '.';
            i++;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i])))
                precision += fmt[i++];
            if (precision == ".") precision += "0";
        }

        while (i < fmt.size() && std::strchr("hlL", fmt[i])) i++;
        if (i >= fmt.size()){
            raiseError("ValueError", "incomplete format");
            return makeNone();
        }
        char conversion = fmt[i++];
        value v;
        if (hasKey){
            if (!mapping){
                raiseError("TypeError", "format requires a mapping");
                return makeNone();
            }
            value* found = dictFind(arg, makeString(key));
            if (!found){
                raiseError("KeyError", pythonRepr(*this, makeString(key)));
                return makeNone();
            }
            v = *found;
        } else {
            if (valueIndex >= values.size()){
                raiseError("TypeError", "not enough arguments for format string");
                return makeNone();
            }
            v = values[valueIndex++];
        }
        bool leftAlign = flags.find('-') != std::string::npos;
        std::string body;
        if (conversion == 's' || conversion == 'r' || conversion == 'a'){
            body = (conversion == 's') ? toString(v) : pythonRepr(*this, v);
            if (hasPrecision){
                int limit = std::atoi(precision.c_str() + 1);
                if (limit >= 0 && static_cast<int>(body.size()) > limit)
                    body = body.substr(0, static_cast<size_t>(limit));
            }
        } else if (conversion == 'c'){
            if (v.type == "str"){
                std::string text = toString(v);
                body = text.empty() ? "" : text.substr(0, 1);
            } else {
                body = std::string(1, static_cast<char>(toInt(v)));
            }
        } else {
            char conv = conversion;
            if (conv == 'i' || conv == 'u') conv = 'd';
            if (!std::strchr("doxXfFeEgG", conv)){
                raiseError("ValueError", "unsupported format character '" +
                                         std::string(1, conversion) + "'");
                return makeNone();
            }
            std::string cformat = "%" + flags + width + (hasPrecision ? precision : "") +
                                  std::string(1, conv);
            char buffer[256];
            if (conv == 'd' || conv == 'o' || conv == 'x' || conv == 'X'){
                std::snprintf(buffer, sizeof(buffer), cformat.c_str(), toInt(v));
            } else {
                std::snprintf(buffer, sizeof(buffer), cformat.c_str(), toFloat(v));
            }
            body = buffer;
        }
        if (static_cast<int>(body.size()) < std::atoi(width.c_str())){
            int pad = std::atoi(width.c_str()) - static_cast<int>(body.size());
            if (leftAlign) body += std::string(static_cast<size_t>(pad), ' ');
            else body = std::string(static_cast<size_t>(pad), ' ') + body;
        }
        out += body;
    }
    if (!mapping && arg.type == "tuple" && valueIndex < values.size()){
        raiseError("TypeError", "not all arguments converted during string formatting");
        return makeNone();
    }
    return makeString(out);
}
function_t* vm::findObjectMethod(const value& v, const std::string& methodName){
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst && inst->klass) return findMethod(inst->klass, methodName);
    }
    if (v.type == "class" && v.ptr){
        auto* klass = static_cast<class_t*>(v.ptr);
        if (klass) return findMethod(klass, methodName);
    }
    return nullptr;
}
std::vector<std::string> vm::collectDirNames(const value& v){
    std::vector<std::string> names;
    auto addKlassNames = [&](class_t* klass){
        if (!klass) return;
        for (auto& method : klass->methods){
            names.push_back(method.name);
        }
        if (klass->attributes){
            auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            for (auto& kv : *attrs){
                names.push_back(kv.first);
            }
        }
    };
    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst){
            if (inst->attributes){
                for (auto& kv : *inst->attributes){
                    if (kv.first.type == "str" && kv.first.ptr){
                        names.push_back(*static_cast<std::string*>(kv.first.ptr));
                    }
                }
            }
            addKlassNames(inst->klass);
        }
    } else if (v.type == "class" && v.ptr){
        auto* klass = static_cast<class_t*>(v.ptr);
        addKlassNames(klass);
    }
    return names;
}
value vm::createDynamicClass(const std::string& name,
                                      const std::vector<value>& bases,
                                      const value& namespaceDict){

    auto* klass = new class_t();
    klass->name = name;

    if (!bases.empty()){
        if (bases[0].type == "class" && bases[0].ptr){
            auto* baseKlass = static_cast<class_t*>(bases[0].ptr);
            klass->baseName = baseKlass->name;
            klass->baseClass = baseKlass;
            klass->builtinBase = builtinBaseName(baseKlass);
        } else if (bases[0].type == "native_object" && bases[0].ptr){
            auto* bi = static_cast<ffi_instance*>(bases[0].ptr);
            if (!bi->klass && (bi->typeName == "dict" || bi->typeName == "list")){
                klass->baseName = bi->typeName;
                klass->builtinBase = bi->typeName;
            }
        }
    }

    klass->attributes = new std::map<std::string, value>();
    auto* classAttrs = static_cast<std::map<std::string, value>*>(klass->attributes);

    if (namespaceDict.type == "dict" && namespaceDict.ptr){
        auto* dict = static_cast<dict_t*>(namespaceDict.ptr);
        for (auto& kv : *dict){
            std::string keyName;
            if (kv.first.type == "str" && kv.first.ptr){
                keyName = *static_cast<std::string*>(kv.first.ptr);
            } else {
                continue;
            }

            if (kv.second.type == "function" && kv.second.ptr){
                function_t* fn = static_cast<function_t*>(kv.second.ptr);
                function_t newMethod = *fn;
                newMethod.name = keyName;
                klass->methods.push_back(newMethod);
            } else {

                (*classAttrs)[keyName] = kv.second;
            }
        }
    }

    applyInitSubclass(klass, {});

    return {"class", keep(vk_class, klass), this};
}


static std::pair<bool, value> tryMagicInplaceOp(
    vm& vm, const std::string& op,
    const value& left, const value& right){
    auto inplaceName = [](const std::string& o) -> std::string {
        if (o == "+") return "__iadd__";
        if (o == "-") return "__isub__";
        if (o == "*") return "__imul__";
        if (o == "/") return "__itruediv__";
        if (o == "//") return "__ifloordiv__";
        if (o == "%") return "__imod__";
        if (o == "**") return "__ipow__";
        if (o == "|") return "__ior__";
        if (o == "&") return "__iand__";
        if (o == "^") return "__ixor__";
        if (o == "<<") return "__ilshift__";
        if (o == ">>") return "__irshift__";
        return "";
    };
    std::string magic = inplaceName(op);
    if (magic.empty()) return {false, vm.makeNone()};

    if (left.type == "object" && left.ptr){
        auto* inst = static_cast<instance*>(left.ptr);
        if (inst->klass){
            function_t* method = vm.findMethod(inst->klass, magic);
            if (method){
                auto snap = vm.saveErrorState();
                value selfCopy = left;
                value result = vm.invoke(method, {right}, &selfCopy);
                if (!vm.hasError()){
                    vm.restoreErrorState(snap);
                    return {true, result};
                }

                vm.restoreErrorState(snap);
            }
        }
    }

    return {false, vm.makeNone()};
}


static std::pair<bool, value> tryMagicBinaryOp(
    vm& vm, const std::string& op,
    const value& left, const value& right){
    auto magicName = [](const std::string& o, bool reversed) -> std::string {
        if (reversed){
            if (o == "+") return "__radd__";
            if (o == "-") return "__rsub__";
            if (o == "*") return "__rmul__";
            if (o == "/") return "__rtruediv__";
            if (o == "//") return "__rfloordiv__";
            if (o == "%") return "__rmod__";
            if (o == "**") return "__rpow__";
            if (o == "&") return "__rand__";
            if (o == "|") return "__ror__";
            if (o == "^") return "__rxor__";
            if (o == "<<") return "__rlshift__";
            if (o == ">>") return "__rrshift__";
            return "";
        } else {
            if (o == "+") return "__add__";
            if (o == "-") return "__sub__";
            if (o == "*") return "__mul__";
            if (o == "/") return "__truediv__";
            if (o == "//") return "__floordiv__";
            if (o == "%") return "__mod__";
            if (o == "**") return "__pow__";
            if (o == "&") return "__and__";
            if (o == "|") return "__or__";
            if (o == "^") return "__xor__";
            if (o == "<<") return "__lshift__";
            if (o == ">>") return "__rshift__";
            if (o == "==") return "__eq__";
            if (o == "!=") return "__ne__";
            if (o == "<") return "__lt__";
            if (o == "<=") return "__le__";
            if (o == ">") return "__gt__";
            if (o == ">=") return "__ge__";
            return "";
        }
    };

    if (op == "in" || op == "not in"){
        return {false, vm.makeNone()};
    }
    std::string primaryName = magicName(op, false);
    std::string reversedName = magicName(op, true);
    auto tryCall = [&](const value& obj, const std::string& methodName,
                       const value& other) -> std::pair<bool, value> {
        if (methodName.empty()) return {false, vm.makeNone()};
        if (obj.type == "native_object" && obj.ptr){
            auto* ninst = static_cast<ffi_instance*>(obj.ptr);
            if (!ninst->klass) return {false, vm.makeNone()};
            if (ninst->klass->methods.find(methodName) == ninst->klass->methods.end())
                return {false, vm.makeNone()};
            value result;
            if (cffiCallMethod(vm, obj, methodName, {other}, result))
                return {true, result};
            if (vm.hasError()) return {true, result};
            return {true, result};
        }
        if (obj.type != "object" || !obj.ptr) return {false, vm.makeNone()};
        auto* inst = static_cast<instance*>(obj.ptr);
        if (!inst->klass) return {false, vm.makeNone()};
        function_t* method = vm.findMethod(inst->klass, methodName);
        if (!method) return {false, vm.makeNone()};

        value selfCopy = obj;
        value result = vm.invoke(method, {other}, &selfCopy);
        if (vm.hasError()){

            return {true, result};
        }
        return {true, result};
    };
    auto [ok1, res1] = tryCall(left, primaryName, right);
    if (ok1) return {true, res1};
    auto [ok2, res2] = tryCall(right, reversedName, left);
    if (ok2) return {true, res2};
    return {false, vm.makeNone()};
}
value vm::applyBinary(const std::string& op, const value& left, const value& right){

    auto [magicHandled, magicResult] = tryMagicBinaryOp(*this, op, left, right);
    if (magicHandled) return magicResult;

    if (op == "and"){

        return truthy(left) ? right : left;
    }
    if (op == "or"){
        return truthy(left) ? left : right;
    }

    if (op == "==" || op == "!="){
        bool equal = valuesEqual(left, right);
        return makeBool(op == "==" ? equal : !equal);
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">="){
        bool result = false;
        bool bothNumber = (left.type == "int" || left.type == "float" || left.type == "bool") &&
                          (right.type == "int" || right.type == "float" || right.type == "bool");
        if ((left.type == "bytes" || left.type == "bytearray") &&
            (right.type == "bytes" || right.type == "bytearray") &&
            left.ptr && right.ptr){
            std::string a = *static_cast<std::string*>(left.ptr);
            std::string b = *static_cast<std::string*>(right.ptr);
            if (op == "<") result = (a < b);
            else if (op == ">") result = (a > b);
            else if (op == "<=") result = (a <= b);
            else if (op == ">=") result = (a >= b);
        } else if (left.type == "str" && right.type == "str"){
            std::string a = toString(left);
            std::string b = toString(right);
            if (op == "<") result = (a < b);
            else if (op == ">") result = (a > b);
            else if (op == "<=") result = (a <= b);
            else if (op == ">=") result = (a >= b);
        } else if (bothNumber){
            double a = toFloat(left);
            double b = toFloat(right);
            if (op == "<") result = (a < b);
            else if (op == ">") result = (a > b);
            else if (op == "<=") result = (a <= b);
            else if (op == ">=") result = (a >= b);
        } else {
            raiseError("TypeError", "'" + op + "' not supported between instances of '" +
                                    pythonTypeName(left) + "' and '" + pythonTypeName(right) + "'");
            return makeNone();
        }
        return makeBool(result);
    }

    if (op == "is" || op == "is not"){
        bool same = (left.ptr == right.ptr);
        return makeBool(op == "is" ? same : !same);
    }

    if (op == "in" || op == "not in"){
        bool found = false;
        if ((right.type == "list" || right.type == "tuple") && right.ptr){
            auto* list = static_cast<std::vector<value>*>(right.ptr);
            for (auto& element : *list){
                if (valuesEqual(left, element)){
                    found = true;
                    break;
                }
            }
        } else if ((right.type == "dict_keys" || right.type == "dict_values" ||
                    right.type == "dict_items") && right.ptr){
            std::vector<value> contents;
            materializeIterable(right, contents);
            for (auto& element : contents){
                if (valuesEqual(left, element)){
                    found = true;
                    break;
                }
            }
        } else if (right.type == "dict" && right.ptr){
            if (!hashable(left)){
                raiseError("TypeError", "cannot use '" + pythonTypeName(left) +
                                        "' as a dict key (unhashable type: '" +
                                        pythonTypeName(left) + "')");
                return makeNone();
            }
            auto* items = static_cast<dict_t*>(right.ptr);
            for (auto& item : *items){
                if (valuesEqual(left, item.first)){
                    found = true;
                    break;
                }
            }
        } else if ((right.type == "bytes" || right.type == "bytearray") && right.ptr){
            std::string haystack = *static_cast<std::string*>(right.ptr);
            std::string needle;
            if (left.type == "bytes" || left.type == "bytearray"){
                needle = *static_cast<std::string*>(left.ptr);
            } else if (left.type == "int" || left.type == "bool"){
                needle.push_back(static_cast<char>(toInt(left) & 0xFF));
            } else {
                raiseError("TypeError", "a bytes-like object is required, not '" +
                                        pythonTypeName(left) + "'");
                return makeNone();
            }
            found = (haystack.find(needle) != std::string::npos);
        } else if (right.type == "str" && right.ptr){
            std::string haystack = *static_cast<std::string*>(right.ptr);
            std::string needle = toString(left);
            found = (haystack.find(needle) != std::string::npos);
        } else if (right.type == "range" && right.ptr){
            auto* range = static_cast<range_t*>(right.ptr);
            if (left.type == "int" || left.type == "bool"){
                long long value = toInt(left);
                if (range->step > 0){
                    if (value >= range->start && value < range->stop){
                        found = ((value - range->start) % range->step) == 0;
                    }
                } else if (range->step < 0){
                    if (value <= range->start && value > range->stop){
                        found = ((range->start - value) % (-range->step)) == 0;
                    }
                }
            }
        } else if (right.type == "native_object" && right.ptr){
            auto* ninst = static_cast<ffi_instance*>(right.ptr);
            if (ninst->klass && ninst->klass->methods.count("__contains__")){
                value r;
                if (cffiCallMethod(*this, right, "__contains__", {left}, r) && !hasError()){
                    bool result = truthy(r);
                    return makeBool(op == "in" ? result : !result);
                }
                if (hasError()) return makeNone();
            }
            raiseError("TypeError", "argument of type '" + pythonTypeName(right) +
                                    "' is not a container or iterable");
            return makeNone();
        } else if (right.type == "object" && right.ptr){

            auto* inst = static_cast<instance*>(right.ptr);
            function_t* contains = findMethod(inst->klass, "__contains__");
            if (contains){
                value selfCopy = right;
                value r = invoke(contains, {left}, &selfCopy);
                if (!hasError()){
                    bool result = truthy(r);
                    return makeBool(op == "in" ? result : !result);
                }
                clearPendingExceptionState();
            }

            if (dict_t* items = dictBacking(right)){
                bool foundKey = false;
                for (auto& item : *items){
                    if (valuesEqual(left, item.first)){
                        foundKey = true;
                        break;
                    }
                }
                return makeBool(op == "in" ? foundKey : !foundKey);
            }
            if (auto* items = sequenceBacking(right)){
                bool foundElement = false;
                for (auto& element : *items){
                    if (valuesEqual(left, element)){
                        foundElement = true;
                        break;
                    }
                }
                return makeBool(op == "in" ? foundElement : !foundElement);
            }
            raiseError("TypeError", "argument of type '" + pythonTypeName(right) +
                                    "' is not a container or iterable");
            return makeNone();
        } else if (right.type == "class" && right.ptr){
            auto* klass = static_cast<class_t*>(right.ptr);
            if (klass->builtinBase.rfind("enum:", 0) == 0 && klass->attributes){
                auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
                auto it = attrs->find("_member_names_");
                if (it != attrs->end() && it->second.type == "list" && it->second.ptr){
                    for (auto& nameVal : *static_cast<std::vector<value>*>(it->second.ptr)){
                        if (nameVal.type != "str") continue;
                        auto mit = attrs->find(*static_cast<std::string*>(nameVal.ptr));
                        if (mit != attrs->end() && valuesEqual(left, mit->second)){
                            found = true;
                            break;
                        }
                    }
                }
                return makeBool(op == "in" ? found : !found);
            }
            raiseError("TypeError", "argument of type '" + pythonTypeName(right) +
                                    "' is not a container or iterable");
            return makeNone();
        } else {
            raiseError("TypeError", "argument of type '" + pythonTypeName(right) +
                                    "' is not a container or iterable");
            return makeNone();
        }
        return makeBool(op == "in" ? found : !found);
    }

    if (op == "+" && left.type == "list" && right.type == "list"){
        std::vector<value> result;
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        result.insert(result.end(), a->begin(), a->end());
        result.insert(result.end(), b->begin(), b->end());
        return makeList(result);
    }
    if (op == "*" && left.type == "list" && (right.type == "int" || right.type == "bool")){
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        std::vector<value> result;
        long long count = toInt(right);
        for (long long i = 0; i < count; ++i) result.insert(result.end(), a->begin(), a->end());
        return makeList(result);
    }
    if (op == "*" && right.type == "list" && (left.type == "int" || left.type == "bool")){
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        std::vector<value> result;
        long long count = toInt(left);
        for (long long i = 0; i < count; ++i) result.insert(result.end(), b->begin(), b->end());
        return makeList(result);
    }
    if (op == "+" && left.type == "tuple" && right.type == "tuple"){
        std::vector<value> result;
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        result.insert(result.end(), a->begin(), a->end());
        result.insert(result.end(), b->begin(), b->end());
        return makeTuple(result);
    }
    if (op == "*" && left.type == "tuple" && (right.type == "int" || right.type == "bool")){
        auto* a = static_cast<std::vector<value>*>(left.ptr);
        std::vector<value> result;
        long long count = toInt(right);
        for (long long i = 0; i < count; ++i) result.insert(result.end(), a->begin(), a->end());
        return makeTuple(result);
    }
    if (op == "*" && right.type == "tuple" && (left.type == "int" || left.type == "bool")){
        auto* b = static_cast<std::vector<value>*>(right.ptr);
        std::vector<value> result;
        long long count = toInt(left);
        for (long long i = 0; i < count; ++i) result.insert(result.end(), b->begin(), b->end());
        return makeTuple(result);
    }

    if (left.type == "str" && op == "+" && right.type != "str"){
        raiseError("TypeError", "can only concatenate str (not \"" +
                                pythonTypeName(right) + "\") to str");
        return makeNone();
    }
    if (left.type == "list" && op == "+" && right.type != "list"){
        raiseError("TypeError", "can only concatenate list (not \"" +
                                pythonTypeName(right) + "\") to list");
        return makeNone();
    }
    if (left.type == "tuple" && op == "+" && right.type != "tuple"){
        raiseError("TypeError", "can only concatenate tuple (not \"" +
                                pythonTypeName(right) + "\") to tuple");
        return makeNone();
    }
    if ((left.type == "bytes" || left.type == "bytearray") ||
        (right.type == "bytes" || right.type == "bytearray")){
        auto makeByteResult = [&](const std::string& raw, bool isBA){
            value r = makeString(raw);
            r.type = isBA ? "bytearray" : "bytes";
            return r;
        };
        if (op == "+"){
            bool leftByte = (left.type == "bytes" || left.type == "bytearray") && left.ptr;
            bool rightByte = (right.type == "bytes" || right.type == "bytearray") && right.ptr;
            if (leftByte && rightByte &&
                (left.type == "bytearray" || right.type == "bytes")){
                return makeByteResult(*static_cast<std::string*>(left.ptr) +
                                      *static_cast<std::string*>(right.ptr),
                                      left.type == "bytearray");
            }
            raiseError("TypeError", "can't concat " + pythonTypeName(right) +
                                    " to " + (left.type == "bytearray" ? "bytearray" : "bytes"));
            return makeNone();
        }
        if (op == "*"){
            if ((left.type == "bytes" || left.type == "bytearray") && left.ptr &&
                (right.type == "int" || right.type == "bool")){
                return makeByteResult(repeatString(*static_cast<std::string*>(left.ptr), toInt(right)),
                                      left.type == "bytearray");
            }
            if ((right.type == "bytes" || right.type == "bytearray") && right.ptr &&
                (left.type == "int" || left.type == "bool")){
                return makeByteResult(repeatString(*static_cast<std::string*>(right.ptr), toInt(left)),
                                      right.type == "bytearray");
            }
            raiseError("TypeError", "can't multiply sequence by non-int of type '" +
                                    pythonTypeName(left.type == "int" || left.type == "bool" ? right : left) + "'");
            return makeNone();
        }
        raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                                pythonTypeName(left) + "' and '" + pythonTypeName(right) + "'");
        return makeNone();
    }

    if (left.type == "str" || right.type == "str"){
        if (op == "%" && left.type == "str"){
            return percentFormat(left, right);
        }
        if (op == "+" && left.type == "str" && right.type == "str"){
            return makeString(toString(left) + toString(right));
        }
        if (op == "*" && left.type == "str" && (right.type == "int" || right.type == "bool")){
            return makeString(repeatString(toString(left), toInt(right)));
        }
        if (op == "*" && right.type == "str" && (left.type == "int" || left.type == "bool")){
            return makeString(repeatString(toString(right), toInt(left)));
        }
        if (op == "*" && left.type == "str"){
            raiseError("TypeError", "can't multiply sequence by non-int of type '" +
                                    pythonTypeName(right) + "'");
            return makeNone();
        }
        if (op == "*" && right.type == "str"){
            raiseError("TypeError", "can't multiply sequence by non-int of type '" +
                                    pythonTypeName(left) + "'");
            return makeNone();
        }
        raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                                pythonTypeName(left) + "' and '" + pythonTypeName(right) + "'");
        return makeNone();
    }

    if (op == "|" && dictBacking(left) && dictBacking(right)){
        dict_t result = *dictBacking(left);
        for (auto& item : *dictBacking(right)){
            bool replaced = false;
            for (auto& existing : result){
                if (valuesEqual(existing.first, item.first)){
                    existing.second = item.second;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) result.push_back(item);
        }
        return makeDict(result);
    }

// 复数参与的四则运算: 另一侧为 int/float/bool 时先提升为复数
// arithmetic with complex: promote int/float/bool operand to complex first
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠣᠷᠣᠯᠴᠠᠭᠰᠠᠨ ᠳᠥᠷᠪᠡᠨ ᠦᠢᠯᠡᠳᠦᠯ
    {
        double lcRe, lcIm, rcRe, rcIm;
        bool leftCplx = complexParts(left, lcRe, lcIm);
        bool rightCplx = complexParts(right, rcRe, rcIm);
        if (leftCplx || rightCplx){
            bool leftPlainNum = (left.type == "int" || left.type == "float" || left.type == "bool");
            bool rightPlainNum = (right.type == "int" || right.type == "float" || right.type == "bool");
            if ((leftCplx || leftPlainNum) && (rightCplx || rightPlainNum)){
                double a = leftCplx ? lcRe : toFloat(left);
                double b = leftCplx ? lcIm : 0.0;
                double c = rightCplx ? rcRe : toFloat(right);
                double d = rightCplx ? rcIm : 0.0;
                if (op == "+") return makeComplex(a + c, b + d);
                if (op == "-") return makeComplex(a - c, b - d);
                if (op == "*") return makeComplex(a * c - b * d, a * d + b * c);

// 复数除法: 与 CPython 相同的 Smith 算法, 保持符号零与舍入一致
// complex division: same Smith algorithm as CPython, keeping signed zero and rounding
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠬᠤᠪᠢᠶᠠᠯᠭ᠎ᠠ
                auto cquot = [&](double ar, double ai, double br, double bi,
                                 double& outRe, double& outIm) -> bool {
                    if (br == 0.0 && bi == 0.0) return false;
                    if (std::fabs(br) >= std::fabs(bi)){
                        double ratio = bi / br;
                        double denom = br + bi * ratio;
                        outRe = (ar + ai * ratio) / denom;
                        outIm = (ai - ar * ratio) / denom;
                    } else {
                        double ratio = br / bi;
                        double denom = br * ratio + bi;
                        outRe = (ar * ratio + ai) / denom;
                        outIm = (ai * ratio - ar) / denom;
                    }
                    return true;
                };
                if (op == "/"){
                    double outRe, outIm;
                    if (!cquot(a, b, c, d, outRe, outIm)){
                        raiseError("ZeroDivisionError", "complex division by zero");
                        return makeNone();
                    }
                    return makeComplex(outRe, outIm);
                }
                if (op == "**"){

// 复数幂: 整数小指数重复相乘, 其余走极坐标公式, 与 CPython 一致
// complex power: repeated multiplication for small integer exponents, polar form otherwise
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠵᠡᠷᠭᠡ
                    bool intExp = (d == 0.0 && c == std::floor(c) && std::fabs(c) <= 100.0);
                    bool baseZero = (a == 0.0 && b == 0.0);
                    if (baseZero && (c < 0.0 || d != 0.0)){
                        raiseError("ZeroDivisionError", "zero to a negative or complex power");
                        return makeNone();
                    }
                    if (intExp){
                        long long n = static_cast<long long>(c);
                        unsigned long long e = (n < 0) ? static_cast<unsigned long long>(-n)
                                                       : static_cast<unsigned long long>(n);
                        double rRe = 1.0, rIm = 0.0;
                        double fRe = a, fIm = b;
                        while (e){
                            if (e & 1){
                                double tRe = rRe * fRe - rIm * fIm;
                                rIm = rRe * fIm + rIm * fRe;
                                rRe = tRe;
                            }
                            double tRe = fRe * fRe - fIm * fIm;
                            fIm = fRe * fIm + fIm * fRe;
                            fRe = tRe;
                            e >>= 1;
                        }
                        if (n < 0){
                            double outRe, outIm;
                            if (!cquot(1.0, 0.0, rRe, rIm, outRe, outIm)){
                                raiseError("ZeroDivisionError", "complex division by zero");
                                return makeNone();
                            }
                            rRe = outRe;
                            rIm = outIm;
                        }
                        return makeComplex(rRe, rIm);
                    }
                    if (baseZero) return makeComplex(0.0, 0.0);
                    std::complex<double> power = std::pow(std::complex<double>(a, b),
                                                          std::complex<double>(c, d));
                    return makeComplex(power.real(), power.imag());
                }
            }
            raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                                    pythonTypeName(left) + "' and '" + pythonTypeName(right) + "'");
            return makeNone();
        }
    }

    bool leftNumber = (left.type == "int" || left.type == "float" || left.type == "bool");
    bool rightNumber = (right.type == "int" || right.type == "float" || right.type == "bool");
    if (!leftNumber || !rightNumber){
        raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                                pythonTypeName(left) + "' and '" + pythonTypeName(right) + "'");
        return makeNone();
    }

    bool useFloat = (left.type == "float" || right.type == "float");
    if (op == "|"){
        if (useFloat){
            raiseError("TypeError", "unsupported operand type(s) for |: '" +
                                    pythonTypeName(left) + "' and '" +
                                    pythonTypeName(right) + "'");
            return makeNone();
        }
        return makeInt(toInt(left) | toInt(right));
    }
    if (op == "&"){
        if (useFloat){
            raiseError("TypeError", "unsupported operand type(s) for &: '" +
                                    pythonTypeName(left) + "' and '" +
                                    pythonTypeName(right) + "'");
            return makeNone();
        }
        return makeInt(toInt(left) & toInt(right));
    }
    if (op == "^"){
        if (useFloat){
            raiseError("TypeError", "unsupported operand type(s) for ^: '" +
                                    pythonTypeName(left) + "' and '" +
                                    pythonTypeName(right) + "'");
            return makeNone();
        }
        return makeInt(toInt(left) ^ toInt(right));
    }
    if (op == "<<" || op == ">>"){
        if (useFloat){
            raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                                    pythonTypeName(left) + "' and '" +
                                    pythonTypeName(right) + "'");
            return makeNone();
        }
        long long shift = toInt(right);
        if (shift < 0){
            raiseError("ValueError", "negative shift count");
            return makeNone();
        }
        if (op == "<<") return makeInt(toInt(left) << shift);
        return makeInt(toInt(left) >> shift);
    }
    if (op == "/"){
        double divisor = toFloat(right);
        if (divisor == 0.0){
            raiseError("ZeroDivisionError", "division by zero");
            return makeNone();
        }
        return makeFloat(toFloat(left) / divisor);
    }
    if (op == "//" || op == "%"){
        if (useFloat){
            double a = toFloat(left);
            double b = toFloat(right);
            if (b == 0.0){
                raiseError("ZeroDivisionError", "division by zero");
                return makeNone();
            }
            if (op == "//") return makeFloat(std::floor(a / b));
            return makeFloat(a - std::floor(a / b) * b);
        }
        long long a = toInt(left);
        long long b = toInt(right);
        if (b == 0){
            raiseError("ZeroDivisionError", "division by zero");
            return makeNone();
        }
        if (op == "//"){
            long long q = a / b;
            long long r = a % b;
            if (r != 0 && ((r > 0) != (b > 0))) q--;
            return makeInt(q);
        }
        long long r = a % b;
        if (r != 0 && ((r < 0) != (b < 0))) r += b;
        return makeInt(r);
    }
    if (op == "**"){
        if (toFloat(left) == 0.0 && toFloat(right) < 0.0){
            raiseError("ZeroDivisionError", "zero to a negative power");
            return makeNone();
        }
        double result = std::pow(toFloat(left), toFloat(right));
        if (left.type != "float" && right.type != "float" && toFloat(right) >= 0){
            return makeInt(static_cast<long long>(result));
        }
        return makeFloat(result);
    }
    if (op == "+"){
        return useFloat ? makeFloat(toFloat(left) + toFloat(right))
                        : makeInt(toInt(left) + toInt(right));
    }
    if (op == "-"){
        return useFloat ? makeFloat(toFloat(left) - toFloat(right))
                        : makeInt(toInt(left) - toInt(right));
    }
    if (op == "*"){
        return useFloat ? makeFloat(toFloat(left) * toFloat(right))
                        : makeInt(toInt(left) * toInt(right));
    }
    return makeNone();
}

function_t* vm::findFunction(const std::string& name){
    const script_t* active = currentScript ? currentScript : &script;
    for (auto it = active->functions.rbegin(); it != active->functions.rend(); ++it){
        if (it->name == name) return const_cast<function_t*>(&*it);
    }
    return nullptr;
}
class_t* vm::findClass(const std::string& name){
    const script_t* active = currentScript ? currentScript : &script;
    for (auto& klass : active->classes){
        if (klass.name == name) return const_cast<class_t*>(&klass);
    }
    return nullptr;
}

// 判断指针是否落在某个函数向量内 (用于确定函数所属脚本)
// Check whether a pointer lies inside a function vector (to find its owning script)
static bool pointerInFunctionVector(const function_t* fn, const std::vector<function_t>& vec){
    if (!fn || vec.empty()) return false;
    const function_t* begin = vec.data();
    const function_t* end = begin + vec.size();
    std::less<const function_t*> less;
    return !less(fn, begin) && less(fn, end);
}

const script_t* vm::findFunctionOwner(const function_t* fn) const {
    if (!fn) return nullptr;
    auto owns = [fn](const script_t& s){
        if (pointerInFunctionVector(fn, s.functions)) return true;
        for (auto& klass : s.classes){
            if (pointerInFunctionVector(fn, klass.methods)) return true;
        }
        return false;
    };
    if (owns(script)) return &script;
    for (auto& lib : scriptLibraries){
        if (owns(lib.second)) return &lib.second;
    }
    return nullptr;
}
function_t* vm::findMethod(class_t* klass, const std::string& name){
    function_t* out = nullptr;
    findMethodOwner(klass, name, out);
    return out;
}
class_t* vm::findMethodOwner(class_t* klass, const std::string& name, function_t*& out){
    out = nullptr;

    int guard = 0;
    for (class_t* current = klass; current != nullptr && guard < 1000; current = current->baseClass, ++guard){
        for (auto& method : current->methods){
            if (method.name == name){
                out = &method;
                return current;
            }
        }
    }
    return nullptr;
}
std::string vm::builtinBaseName(class_t* klass){
    int guard = 0;
    for (class_t* current = klass; current != nullptr && guard < 1000; current = current->baseClass, ++guard){
        if (!current->builtinBase.empty()) return current->builtinBase;
    }
    return "";
}
void vm::resolveClassBase(class_t* klass){
    if (!klass) return;
    klass->baseClass = nullptr;
    klass->builtinBase.clear();

    std::string firstBase;
    {
        int depth = 0;
        size_t begin = 0;
        size_t end = klass->baseName.size();
        for (size_t i = 0; i <= end; ++i){
            char c = (i < end) ? klass->baseName[i] : ',';
            if (c == '(' || c == '[' || c == '{') depth++;
            else if (c == ')' || c == ']' || c == '}') depth--;
            else if (c == ',' && depth == 0){
                firstBase = klass->baseName.substr(begin, i - begin);
                break;
            }
        }

        size_t a = 0, b = firstBase.size();
        while (a < b && std::isspace(static_cast<unsigned char>(firstBase[a]))) a++;
        while (b > a && std::isspace(static_cast<unsigned char>(firstBase[b - 1]))) b--;
        firstBase = firstBase.substr(a, b - a);
    }
    if (firstBase.empty()) return;
    auto snap = saveErrorState();
    value base = evalExpr(firstBase);
    if (failed){
        restoreErrorState(snap);
        return;
    }
    if (base.type == "class" && base.ptr){
        class_t* baseKlass = static_cast<class_t*>(base.ptr);
        if (baseKlass != klass) klass->baseClass = baseKlass;
    } else if (base.type == "native_object" && base.ptr){
        auto* inst = static_cast<ffi_instance*>(base.ptr);
        if (!inst->klass && inst->typeName != "object"){
            if (inst->typeName == "dict" || inst->typeName == "list"){
                klass->builtinBase = inst->typeName;
            }
        } else if (inst->klass && inst->klass->name == "object"){

        } else if (inst->klass){
            const std::string& bn = inst->klass->name;
            if (bn == "Enum" || bn == "IntEnum" || bn == "StrEnum" ||
                bn == "Flag" || bn == "IntFlag"){
                klass->builtinBase = "enum:" + bn;
            }
        }
    }
}
void vm::pushMethodContext(class_t* definingClass, const value& self){
    method_context ctx;
    ctx.definingClass = definingClass;
    ctx.self = self;
    methodContexts.push_back(ctx);
}
void vm::popMethodContext(){
    if (!methodContexts.empty()) methodContexts.pop_back();
}
value vm::makeSuper(const value* klassArg, const value* objArg){
    if (!klassArg && !objArg){

        if (methodContexts.empty()){
            raiseError("RuntimeError", "super(): no arguments");
            return makeNone();
        }
        auto* handle = new super_object();
        handle->self = methodContexts.back().self;
        handle->startClass = methodContexts.back().definingClass;
        return {"super", keep(vk_super, handle), this};
    }
    if (!klassArg || !objArg){
        raiseError("RuntimeError", "super(): no arguments");
        return makeNone();
    }
    if (klassArg->type != "class" || klassArg->ptr == nullptr){
        raiseError("TypeError", "super() argument 1 must be a class");
        return makeNone();
    }
    auto* handle = new super_object();
    handle->self = *objArg;
    handle->startClass = static_cast<class_t*>(klassArg->ptr);
    return {"super", keep(vk_super, handle), this};
}
value vm::makeBoundFunction(function_t* fn, const value& self){
    auto* handle = new bound_function_object();
    handle->fn = fn;
    handle->self = self;
    return {"bound_function", keep(vk_bound_function, handle), this};
}
void vm::applyInitSubclass(class_t* klass, const std::map<std::string, value>& kwargs){
    if (!klass) return;
    if (klass->baseClass){
        function_t* initSub = findMethod(klass->baseClass, "__init_subclass__");
        if (initSub){

            value classValue;
            classValue.type = "class";
            classValue.ptr = klass;
            invoke(initSub, {classValue}, nullptr, &script,
                   klass->baseClass->name + ".__init_subclass__", kwargs);
            return;
        }
    }
    if (!kwargs.empty()){
        raiseError("TypeError", klass->name +
                                ".__init_subclass__() takes no keyword arguments");
    }
}


namespace {
    using namespace minipy;

// match node
// match node
// match node
struct match_node{
    char kind = 'W';
    std::string text;
    std::vector<match_node> children;
    std::vector<std::pair<std::string, match_node>> attrs;
};

// match Decode Payload
// match decode payload
// match Decode Payload
std::string matchDecodePayload(const std::string& ir, size_t& p){
    size_t start = p;
    while (p < ir.size() && ir[p] >= '0' && ir[p] <= '9') ++p;
    size_t len = 0;
    if (p > start) len = static_cast<size_t>(std::stoul(ir.substr(start, p - start)));
    if (p < ir.size() && ir[p] == ':') ++p;
    std::string out = ir.substr(p, len);
    p += len;
    return out;
}

// match 解析 节点
// match parse node
// match ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠲᠥᠪ
match_node matchParseNode(const std::string& ir, size_t& p){
    match_node node;
    if (p >= ir.size()) return node;
    node.kind = ir[p++];
    switch (node.kind){
        case 'W':
            break;
        case 'C': case 'L': case 'V': case 'T': case 'R':
            node.text = matchDecodePayload(ir, p);
            break;
        case 'A':
            node.text = matchDecodePayload(ir, p);
            node.children.push_back(matchParseNode(ir, p));
            break;
        case 'O':
            node.children.push_back(matchParseNode(ir, p));
            node.children.push_back(matchParseNode(ir, p));
            break;
        case 'S': case 'M': {
            std::string countText = matchDecodePayload(ir, p);
            int count = countText.empty() ? 0 : std::stoi(countText);
            for (int i = 0; i < count; ++i) node.children.push_back(matchParseNode(ir, p));
            break;
        }
        case 'E':
            node.text = matchDecodePayload(ir, p);
            node.children.push_back(matchParseNode(ir, p));
            break;
        case 'K': {
            node.text = matchDecodePayload(ir, p);
            std::string nposText = matchDecodePayload(ir, p);
            int npos = nposText.empty() ? 0 : std::stoi(nposText);
            for (int i = 0; i < npos; ++i) node.children.push_back(matchParseNode(ir, p));
            std::string nkwText = matchDecodePayload(ir, p);
            int nkw = nkwText.empty() ? 0 : std::stoi(nkwText);
            for (int i = 0; i < nkw; ++i){
                std::string attr = matchDecodePayload(ir, p);
                node.attrs.push_back({attr, matchParseNode(ir, p)});
            }
            break;
        }
        default:
            break;
    }
    return node;
}
}
bool vm::matchPattern(const value& subject, const std::string& ir,
                             std::vector<std::pair<std::string, value>>& captures){
    size_t cursor = 0;
    match_node root = matchParseNode(ir, cursor);
    auto bindName = [&](const std::string& name, const value& v) -> bool {
        if (name == "_") return true;
        for (auto& existing : captures){
            if (existing.first == name){
                return valuesEqual(existing.second, v);
            }
        }
        captures.push_back({name, v});
        return true;
    };
    std::function<bool(const value&, const match_node&,
                       std::vector<std::pair<std::string, value>>&)> matchNode;
    matchNode = [&](const value& v, const match_node& node,
                    std::vector<std::pair<std::string, value>>& caps) -> bool {
        switch (node.kind){
            case 'W':
                return true;
            case 'C':
            case 'T':
                return bindName(node.text, v);
            case 'L':
            case 'V': {
                value expected = evalExpr(node.text);
                if (failed) return false;
                return valuesEqual(v, expected);
            }
            case 'A': {
                if (node.children.empty()) return false;
                if (!matchNode(v, node.children[0], caps)) return false;
                return bindName(node.text, v);
            }
            case 'O': {
                if (node.children.size() < 2) return false;
                auto saved = caps;
                if (matchNode(v, node.children[0], caps)) return true;
                caps = saved;
                return matchNode(v, node.children[1], caps);
            }
            case 'S': {
                auto* seq = sequenceBacking(v);
                if (!seq) return false;
                int starPos = -1;
                for (size_t idx = 0; idx < node.children.size(); ++idx){
                    if (node.children[idx].kind == 'T'){
                        starPos = static_cast<int>(idx);
                        break;
                    }
                }
                if (starPos < 0){
                    if (seq->size() != node.children.size()) return false;
                    for (size_t idx = 0; idx < node.children.size(); ++idx){
                        if (!matchNode((*seq)[idx], node.children[idx], caps)) return false;
                    }
                    return true;
                }
                size_t fixed = node.children.size() - 1;
                if (seq->size() < fixed) return false;
                size_t prefixCount = static_cast<size_t>(starPos);
                size_t suffixCount = node.children.size() - static_cast<size_t>(starPos) - 1;
                for (size_t idx = 0; idx < prefixCount; ++idx){
                    if (!matchNode((*seq)[idx], node.children[idx], caps)) return false;
                }
                for (size_t idx = 0; idx < suffixCount; ++idx){
                    size_t subjectIdx = seq->size() - suffixCount + idx;
                    if (!matchNode((*seq)[subjectIdx], node.children[static_cast<size_t>(starPos) + 1 + idx], caps))
                        return false;
                }
                std::vector<value> middle(seq->begin() + starPos,
                                            seq->begin() + static_cast<long long>(seq->size() - suffixCount));
                return bindName(node.children[static_cast<size_t>(starPos)].text, makeList(middle));
            }
            case 'M': {
                dict_t* backing = dictBacking(v);
                if (!backing) return false;
                std::vector<value> consumedKeys;
                std::vector<const match_node*> restNodes;
                for (auto& entry : node.children){
                    if (entry.kind == 'R'){
                        restNodes.push_back(&entry);
                        continue;
                    }
                    if (entry.kind != 'E' || entry.children.empty()) return false;
                    value key = evalExpr(entry.text);
                    if (failed) return false;
                    value* found = dictFind(v, key);
                    if (!found) return false;
                    consumedKeys.push_back(key);
                    if (!matchNode(*found, entry.children[0], caps)) return false;
                }
                for (auto* restNode : restNodes){
                    std::vector<std::pair<value, value>> restItems;
                    for (auto& item : *backing){
                        bool consumed = false;
                        for (auto& key : consumedKeys){
                            if (valuesEqual(key, item.first)){ consumed = true; break; }
                        }
                        if (!consumed) restItems.push_back(item);
                    }
                    if (!bindName(restNode->text, makeDict(restItems))) return false;
                }
                return true;
            }
            case 'K': {
                value cls = evalExpr(node.text);
                if (failed) return false;
                class_t* ck = nullptr;
                std::string clsName;
                if (cls.type == "class" && cls.ptr){
                    ck = static_cast<class_t*>(cls.ptr);
                    clsName = ck->name;
                } else if (cls.type == "native_object" && cls.ptr){
                    auto* native = static_cast<ffi_instance*>(cls.ptr);
                    if (native->klass) clsName = native->klass->name;
                    else clsName = native->typeName;
                } else {
                    return false;
                }
                if (!isInstance(v, cls)) return false;
                size_t positionalCount = node.children.size();
                if (positionalCount > 0){
                    value matchArgs = makeNone();
                    bool hasMatchArgs = false;
                    for (class_t* current = ck; current != nullptr; current = current->baseClass){
                        if (!current->attributes) continue;
                        auto* attrs = static_cast<std::map<std::string, value>*>(current->attributes);
                        auto it = attrs->find("__match_args__");
                        if (it != attrs->end()){
                            matchArgs = it->second;
                            hasMatchArgs = true;
                            break;
                        }
                    }
                    if (!hasMatchArgs){
                        raiseError("TypeError", clsName +
                                   ".__match_args__ is not a tuple of strings (got NoneType)");
                        return false;
                    }
                    auto* matchArgsSeq = sequenceBacking(matchArgs);
                    if (!matchArgsSeq){
                        raiseError("TypeError", clsName +
                                   ".__match_args__ is not a tuple of strings (got " +
                                   pythonTypeName(matchArgs) + ")");
                        return false;
                    }
                    for (auto& item : *matchArgsSeq){
                        if (item.type != "str"){
                            raiseError("TypeError", clsName +
                                       ".__match_args__ is not a tuple of strings (got " +
                                       pythonTypeName(matchArgs) + ")");
                            return false;
                        }
                    }
                    if (positionalCount > matchArgsSeq->size()){
                        std::size_t accepted = matchArgsSeq->size();
                        raiseError("TypeError", clsName + "() accepts " + std::to_string(accepted) +
                                   " positional sub-pattern" + (accepted == 1 ? "" : "s") +
                                   " (" + std::to_string(positionalCount) + " given)");
                        return false;
                    }
                    for (size_t i = 0; i < positionalCount; ++i){
                        std::string attr = *static_cast<std::string*>((*matchArgsSeq)[i].ptr);
                        if (!hasAttribute(v, attr)) return false;
                        value attrValue = getAttribute(v, attr);
                        if (failed) return false;
                        if (!matchNode(attrValue, node.children[i], caps)) return false;
                    }
                }
                for (auto& attrEntry : node.attrs){
                    if (!hasAttribute(v, attrEntry.first)) return false;
                    value attrValue = getAttribute(v, attrEntry.first);
                    if (failed) return false;
                    if (!matchNode(attrValue, attrEntry.second, caps)) return false;
                }
                return true;
            }
            default:
                return false;
        }
    };
    return matchNode(subject, root, captures);
}
bool vm::isInstance(const value& obj, const value& target){

    if (target.type == "tuple" && target.ptr){
        auto* tp = static_cast<std::vector<value>*>(target.ptr);
        for (auto& item : *tp){
            if (isInstance(obj, item)) return true;
        }
        return false;
    }

    if (obj.type == "object" && obj.ptr && target.type == "class" && target.ptr){
        auto* inst = static_cast<instance*>(obj.ptr);
        auto* tk = static_cast<class_t*>(target.ptr);
        if (!inst->klass || !tk) return false;
        if (tk->name == "object") return true;
        int guard = 0;
        for (class_t* current = inst->klass; current != nullptr && guard < 1000;
             current = current->baseClass, ++guard){
            if (current == tk) return true;
        }
    }

    if (obj.type == "native_object" && obj.ptr && target.type == "native_object" && target.ptr){
        auto* ni = static_cast<ffi_instance*>(obj.ptr);
        auto* tk = static_cast<ffi_instance*>(target.ptr);
        if (ni->klass && tk->klass && ni->klass == tk->klass) return true;
    }

    if (target.type == "native_object" && target.ptr){
        auto* tk = static_cast<ffi_instance*>(target.ptr);
        if (!tk->klass){
            if (tk->typeName == "object"){
                if (obj.type == "object") return true;
            } else if (tk->typeName == "dict" || tk->typeName == "list"){
                if (obj.type == "object" && obj.ptr){
                    auto* inst = static_cast<instance*>(obj.ptr);
                    if (builtinBaseName(inst->klass) == tk->typeName) return true;
                }
            }
        }
    }

    if (target.type == "native_object" && target.ptr){
        auto* tk = static_cast<ffi_instance*>(target.ptr);
        if (tk->klass){
            const std::string& abc = tk->klass->name;
            if (abc == "Number" || abc == "Complex" || abc == "Real" ||
                abc == "Rational" || abc == "Integral"){
                bool intLike = (obj.type == "int" || obj.type == "bool");
                bool floatLike = (obj.type == "float");
                bool complexLike = isComplexValue(obj);
                bool fraction = false;
                if (obj.type == "native_object" && obj.ptr){
                    auto* oi = static_cast<ffi_instance*>(obj.ptr);
                    if (oi->klass && oi->klass->name == "Fraction") fraction = true;
                }
                if (abc == "Number" || abc == "Complex")
                    return intLike || floatLike || complexLike || fraction;
                if (abc == "Real") return intLike || floatLike || fraction;
                if (abc == "Rational") return intLike || fraction;
                return intLike;
            }
        }
    }

    const char* targetName = nullptr;
    if (target.type == "class" && target.ptr){
        targetName = static_cast<class_t*>(target.ptr)->name.c_str();
    } else if (target.type == "native_object" && target.ptr){
        auto* tk = static_cast<ffi_instance*>(target.ptr);
        if (tk->klass) targetName = tk->klass->name.c_str();
        else if (!tk->typeName.empty()) targetName = tk->typeName.c_str();
    }
    if (targetName){
        static const char* scalarMap[][2] = {
            {"int", "int"},
            {"float", "float"},
            {"str", "str"},
            {"bool", "bool"},
            {"list", "list"},
            {"tuple", "tuple"},
            {"dict", "dict"},
            {"object", nullptr},
        };
        for (auto& pair : scalarMap){
            if (strcmp(targetName, pair[0]) == 0){
                if (pair[1] == nullptr) return true;
                if (obj.type == pair[1]) return true;

                if (strcmp(targetName, "int") == 0 && obj.type == "bool") return true;
                break;
            }
        }
    }
    return false;
}
bool vm::truthy(const value& v) const {
    if (v.type == "none") return false;
    if (v.type == "bool") return *static_cast<bool*>(v.ptr) != false;
    if (v.type == "int") return *static_cast<long long*>(v.ptr) != 0;
    if (v.type == "float") return *static_cast<double*>(v.ptr) != 0.0;
    if (v.type == "str") return !static_cast<std::string*>(v.ptr)->empty();
    if (v.type == "bytes" || v.type == "bytearray")
        return !static_cast<std::string*>(v.ptr)->empty();
    if (v.type == "list") return !static_cast<std::vector<value>*>(v.ptr)->empty();
    if (v.type == "tuple") return !static_cast<std::vector<value>*>(v.ptr)->empty();
    if (v.type == "dict") return !static_cast<dict_t*>(v.ptr)->empty();
    if (v.type == "native_object" && v.ptr){
        auto* inst = static_cast<ffi_instance*>(v.ptr);
        if (inst && inst->klass && inst->klass->methods.count("__bool__")){
            auto* self = const_cast<vm*>(this);
            value result;
            if (cffiCallMethod(*self, v, "__bool__", {}, result) && !self->hasError())
                return self->truthy(result);
            if (self->hasError()) self->clearPendingExceptionState();
        }
        if (inst && inst->klass && inst->klass->methods.count("__len__")){
            auto* self = const_cast<vm*>(this);
            value result;
            if (cffiCallMethod(*self, v, "__len__", {}, result) && !self->hasError())
                return self->toInt(result) != 0;
            if (self->hasError()) self->clearPendingExceptionState();
        }
    }
    return true;
}




value vm::getAttribute(const value& obj, const std::string& name){

    if (obj.type == "super" && obj.ptr){
        return resolveSuperMember(*this, static_cast<super_object*>(obj.ptr), name);
    }

    if (obj.type == "object" && obj.ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
        struct instance* instance = static_cast<struct instance*>(obj.ptr);

        if (name == "__class__"){
            if (instance->klass) return value("class", instance->klass);
            return makeNone();
        }

        if (name == "__dict__"){
            value d("dict", instance->attributes, this);
            return d;
        }
        value attrDict("dict", instance->attributes, this);
        value keyStr = makeString(name);
        value* foundAttr = dictFind(attrDict, keyStr);
        bool resolved = false;

        value* clsAttrPtr = instance->klass ? findClassAttrInChain(instance->klass, name) : nullptr;
        if (clsAttrPtr){
            const value& clsVal = *clsAttrPtr;
            bool hasSetOrDel = valueHasDescriptorMethod(clsVal, "__set__") ||
                               valueHasDescriptorMethod(clsVal, "__delete__");
            bool hasGet = valueHasDescriptorMethod(clsVal, "__get__");
            if (hasSetOrDel && hasGet){
                value owner("class", instance->klass);
                value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                          {obj, owner});
                if (!hasError()) return descResult;
                return makeNone();
            }
        }
        if (foundAttr) return *foundAttr;

        if (instance->klass){
            if (clsAttrPtr){
                const value& clsVal = *clsAttrPtr;
                if (valueHasDescriptorMethod(clsVal, "__get__")){
                    value owner("class", instance->klass);
                    value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                              {obj, owner});
                    if (!hasError()) return descResult;
                    return makeNone();
                } else {
                    return clsVal;
                }
            }

            function_t* method = findMethod(instance->klass, name);
            if (method){
                value r;
                r.type = "function";
                r.ptr = method;
                return r;
            }

            if ((instance->container.type == "dict" || instance->container.type == "list") &&
                isContainerMethodName(instance->container.type, name)){
                auto* bound = new native_bound_method();
                bound->self = obj;
                bound->methodName = name;
                value r;
                r.type = "native_method";
                r.ptr = bound;
                return r;
            }

            function_t* getattrMethod = findMethod(instance->klass, "__getattr__");
            if (getattrMethod){
                value selfCopy = obj;
                value memberName = makeString(name);
                value r = invoke(getattrMethod, {memberName}, &selfCopy);
                if (!hasError()) return r;
                return makeNone();
            }
            raiseError("AttributeError", "'" + instance->klass->name + "' object has no attribute '" + name + "'");
            return makeNone();
        }
        raiseError("AttributeError", "'object' object has no attribute '" + name + "'");
        return makeNone();
    }

    if (obj.type == "native_object" && obj.ptr){
        auto* instance = static_cast<ffi_instance*>(obj.ptr);
        auto it = instance->attributes.find(name);
        if (it != instance->attributes.end()) return it->second;

        if (name == "__name__" && !instance->typeName.empty()){
            return makeString(instance->typeName);
        }

        if (instance->klass){
            auto methodIt = instance->klass->methods.find(name);
            if (methodIt != instance->klass->methods.end() && methodIt->second != nullptr){
                auto* bound = new native_bound_method();
                bound->self = obj;
                bound->methodName = name;
                value r;
                r.type = "native_method";
                r.ptr = bound;
                return r;
            }
        }

        if (instance->dynamicGet){
            value dynamicValue = makeNone();
            if (instance->dynamicGet(*this, instance->handle, name, dynamicValue)) return dynamicValue;
        }

        for (auto* lib : cffiLibraries){
            if (instance->library == lib){
                auto funcIt = lib->functions.find(name);
                if (funcIt != lib->functions.end() && funcIt->second != nullptr){

                    value r = makeNativeObject(lib, nullptr, funcIt->second);
                    // 记录函数所属的库: invoke() 只有在 inst->library 非空时才会走原生调用分支
                    // (lib->functions[func].__call__(...) 这条路径依赖这里补齐 library)
                    // Remember the owning library: invoke() only takes the native path when
                    // inst->library is set (the lib->functions[func].__call__(...) path).
                    static_cast<ffi_instance*>(r.ptr)->library = lib;
                    return r;
                }
                auto dataIt = lib->data.find(name);
                if (dataIt != lib->data.end()){
                    value r = makeNone();
                    if (cffiCallData(*lib, name, *this, r)) return r;
                }
                auto classIt = lib->classes.find(name);
                if (classIt != lib->classes.end()){
                    return makeNativeObject(lib, &classIt->second, nullptr);
                }
                break;
            }
        }
        std::string typeName = !instance->typeName.empty() ? instance->typeName
                          : (instance->klass ? instance->klass->name : "object");

        if (name == "fromkeys" && typeName == "dict"){
            auto* bound = new native_bound_method();
            bound->self = makeDict({});
            bound->methodName = "fromkeys";
            value r;
            r.type = "native_method";
            r.ptr = bound;
            return r;
        }
        if (name == "__class__"){

            for (auto* lib : cffiLibraries){
                if (!lib || lib->name != "builtins") continue;
                auto fnIt = lib->functions.find(typeName);
                if (fnIt != lib->functions.end() && fnIt->second != nullptr)
                    return makeNativeObject(lib, nullptr, fnIt->second);
                break;
            }
        }
        raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + name + "'");
        return makeNone();
    }

    if (obj.type == "class" && obj.ptr){
        auto* klass = static_cast<class_t*>(obj.ptr);
        if (name == "__class__"){

            for (auto* lib : cffiLibraries){
                if (!lib || lib->name != "builtins") continue;
                auto it = lib->functions.find("type");
                if (it != lib->functions.end() && it->second != nullptr)
                    return makeNativeObject(lib, nullptr, it->second);
                break;
            }
            return obj;
        }
        value* classAttr = findClassAttrInChain(klass, name);
        if (classAttr){
            if (valueHasDescriptorMethod(*classAttr, "__get__")){
                value noneInst = makeNone();
                value descResult = invokeDescriptorMethod(*this, *classAttr, "__get__",
                                                          {noneInst, obj});
                if (!hasError()) return descResult;
                return makeNone();
            }
            return *classAttr;
        }
        function_t* method = findMethod(klass, name);
        if (method){
            value r;
            r.type = "function";
            r.ptr = method;
            return r;
        }
        raiseError("AttributeError", "type object '" + klass->name + "' has no attribute '" + name + "'");
        return makeNone();
    }

    if (obj.type == "function" && obj.ptr){
        auto* fn = static_cast<function_t*>(obj.ptr);
        if (name == "__name__" || name == "__qualname__") return makeString(fn->name);
        if (name == "__module__") return makeString("__main__");
        if (name == "__doc__" || name == "__defaults__" || name == "__kwdefaults__") return makeNone();
        if (name == "__annotations__" || name == "__globals__") return makeDict({});
        if (name == "__code__"){

            auto* ch = new closure_handle();
            ch->fn = fn;
            return makeDynamicObject("code", ch, codeStateGet, nullptr);
        }
        if (name == "__closure__") return makeNone();
        raiseError("AttributeError", "'function' object has no attribute '" + name + "'");
        return makeNone();
    }


    if (obj.type == "str" && name == "__name__"){
        return makeString(toString(obj));
    }
    raiseError("AttributeError", "'" + pythonTypeName(obj) + "' object has no attribute '" + name + "'");
    return makeNone();
}
void vm::setAttribute(const value& obj, const std::string& name, const value& v){

    if (obj.type == "object" && obj.ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
        struct instance* instance = static_cast<struct instance*>(obj.ptr);

        if (instance->klass){
            function_t* setattrMethod = findMethod(instance->klass, "__setattr__");
            if (setattrMethod){
                auto snap = saveErrorState();
                value selfCopy = obj;
                value nameStr = makeString(name);
                value r = invoke(setattrMethod, {nameStr, v}, &selfCopy);
                if (!hasError()){
                    restoreErrorState(snap);
                    return;
                }
                restoreErrorState(snap);
            }
        }

        if (instance->klass && instance->klass->attributes){
            auto* klassAttrs = static_cast<std::map<std::string, value>*>(instance->klass->attributes);
            auto clsAttrIt = klassAttrs->find(name);
            if (clsAttrIt != klassAttrs->end()){
                const value& clsVal = clsAttrIt->second;
                if (valueHasDescriptorMethod(clsVal, "__set__")){
                    auto snap = saveErrorState();
                    value descResult = invokeDescriptorMethod(*this, clsVal, "__set__",
                                                              {obj, v});
                    if (!hasError()) return;
                    restoreErrorState(saveErrorState());
                }
                if (valueHasDescriptorMethod(clsVal, "__delete__") &&
                    !valueHasDescriptorMethod(clsVal, "__set__")){
                    raiseError("AttributeError", "can't set attribute");
                    return;
                }
            }
        }
        value attrDict("dict", instance->attributes, this);
        dictSet(attrDict, makeString(name), v);
        return;
    }

    if (obj.type == "class" && obj.ptr){
        auto* klass = static_cast<class_t*>(obj.ptr);
        if (klass->attributes){
            auto* klassAttrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            auto clsAttrIt = klassAttrs->find(name);
            if (clsAttrIt != klassAttrs->end()){
                const value& clsVal = clsAttrIt->second;
                if (valueHasDescriptorMethod(clsVal, "__set__")){
                    auto snap = saveErrorState();
                    value descResult = invokeDescriptorMethod(*this, clsVal, "__set__",
                                                              {obj, v});
                    if (!hasError()) return;
                    restoreErrorState(saveErrorState());
                }
            }
        }
        if (!klass->attributes) klass->attributes = new std::map<std::string, value>();
        auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
        (*attrs)[name] = v;
        return;
    }

    if (obj.type == "native_object" && obj.ptr){
        auto* instance = static_cast<ffi_instance*>(obj.ptr);
        instance->attributes[name] = v;
        return;
    }
    raiseError("AttributeError", "'" + pythonTypeName(obj) + "' object has no attribute '" + name + "'");
}
void vm::deleteAttribute(const value& obj, const std::string& name){

    if (obj.type == "object" && obj.ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
        struct instance* instance = static_cast<struct instance*>(obj.ptr);
        if (instance && instance->klass){
            function_t* delattrMethod = findMethod(instance->klass, "__delattr__");
            if (delattrMethod){
                value selfCopy = obj;
                value nameStr = makeString(name);
                value r = invoke(delattrMethod, {nameStr}, &selfCopy);
                if (!hasError()) return;
                return;
            }
        }

        value attrDict("dict", instance->attributes, this);
        value keyStr = makeString(name);
        if (!dictRemove(attrDict, keyStr)){
            raiseError("AttributeError", "'" + instance->klass->name + "' object has no attribute '" + name + "'");
        }
        return;
    }

    if (obj.type == "class" && obj.ptr){
        auto* klass = static_cast<class_t*>(obj.ptr);
        if (klass->attributes){
            auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            auto it = attrs->find(name);
            if (it != attrs->end()){
                attrs->erase(it);
                return;
            }
        }
        raiseError("AttributeError", "type object '" + klass->name + "' has no attribute '" + name + "'");
        return;
    }

    if (obj.type == "native_object" && obj.ptr){
        auto* instance = static_cast<ffi_instance*>(obj.ptr);
        auto it = instance->attributes.find(name);
        if (it != instance->attributes.end()){
            instance->attributes.erase(it);
            return;
        }
        std::string typeName = !instance->typeName.empty() ? instance->typeName
                          : (instance->klass ? instance->klass->name : "object");
        raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + name + "'");
        return;
    }
    raiseError("AttributeError", "'" + pythonTypeName(obj) + "' object has no attribute '" + name + "'");
}
bool vm::hasAttribute(const value& obj, const std::string& name){
    auto snap = saveErrorState();
    value result = getAttribute(obj, name);
    if (hasError()){
        restoreErrorState(snap);
        return false;
    }
    restoreErrorState(snap);
    return true;
}
void vm::loadlib(const ffilib& library){
    cffiLibraries.push_back(new ffilib(library));
}

ffilib* vm::findCffiLibrary(const std::string& name){
    for (auto* lib : cffiLibraries){
        if (lib && lib->name == name) return lib;
    }
    return nullptr;
}

void vm::usestdlib(){
    loadlib(cffi_math_lib());
    loadlib(cffi_random_lib());
    loadlib(cffi_time_lib());
    loadlib(cffi_sys_lib());
    loadlib(cffi_os_lib());
    loadlib(cffi_os_path_lib());
    loadlib(cffi_string_lib());
    loadlib(cffi_json_lib());
    loadlib(cffi_re_lib());
    loadlib(cffi_itertools_lib());
    loadlib(cffi_statistics_lib());
    loadlib(cffi_base64_lib());
    loadlib(cffi_bisect_lib());
    loadlib(cffi_heapq_lib());
    loadlib(cffi_copy_lib());
    loadlib(cffi_operator_lib());
    loadlib(cffi_ctypes_lib());
    loadlib(cffi_fnmatch_lib());
    loadlib(cffi_colorsys_lib());
    loadlib(cffi_html_lib());
    loadlib(cffi_urllib_parse_lib());
    loadlib(cffi_calendar_lib());
    loadlib(cffi_textwrap_lib());
    loadlib(cffi_graphlib_lib());
    loadlib(cffi_secrets_lib());
    loadlib(cffi_reprlib_lib());
    loadlib(cffi_difflib_lib());
    loadlib(cffi_getopt_lib());
    loadlib(cffi_binascii_lib());
    loadlib(cffi_struct_lib());
    loadlib(cffi_array_lib());
    loadlib(cffi_codecs_lib());
    loadlib(cffi_pprint_lib());
    loadlib(cffi_csv_lib());
    loadlib(cffi_configparser_lib());
    loadlib(cffi_tomllib_lib());
    loadlib(cffi_io_lib());
    loadlib(cffi_collections_lib());
    loadlib(cffi_types_lib());
    loadlib(cffi_enum_lib());
    loadlib(cffi_numbers_lib());
    loadlib(cffi_cmath_lib());
    loadlib(cffi_fractions_lib());
    loadlib(cffi_pathlib_lib());
    loadlib(cffi_glob_lib());
    loadlib(cffi_filecmp_lib());
    loadlib(cffi_shutil_lib());
    loadlib(cffi_tempfile_lib());
    loadlib(cffi_atexit_lib());
    loadlib(cffi_gc_lib());
    loadlib(cffi_warnings_lib());
    loadlib(cffi_sched_lib());
    loadlib(cffi_queue_lib());
    loadlib(cffi_contextvars_lib());
    loadlib(cffi_timeit_lib());
    loadlib(cffi_uuid_lib());
    loadlib(cffi_http_cookies_lib());
    loadlib(cffi_imghdr_lib());
    loadlib(cffi_sndhdr_lib());
    loadlib(cffi_wave_lib());
    loadlib(cffi_argparse_lib());
    loadlib(cffi_optparse_lib());
    loadlib(cffi_logging_lib());
    loadlib(cffi_traceback_lib());
    loadlib(cffi_doctest_lib());
    loadlib(cffi_unittest_lib());
    loadlib(cffi_profile_lib());
    loadlib(cffi_pickle_lib());
    loadlib(cffi_marshal_lib());
}
void vm::loadpy(const script_t& libraryScript){
    std::string name = libraryScript.name;
    if (name.empty()){
        name = "lib_" + std::to_string(scriptLibraries.size());
    }
    addScriptLibrary(name, libraryScript);
}
void vm::loadpy(const std::string& name, const script_t& libraryScript){
    addScriptLibrary(name, libraryScript);
}
void vm::addScriptLibrary(const std::string& name, const script_t& libraryScript){
    for (auto& item : scriptLibraries){
        if (item.first == name){
            item.second = libraryScript;
            return;
        }
    }
    scriptLibraries.push_back({name, libraryScript});
}
const script_t* vm::findScriptLibrary(const std::string& name) const {
    for (auto& item : scriptLibraries){
        if (item.first == name) return &item.second;
    }
    return nullptr;
}

void vm::executeScriptModule(const std::string& name){
    if (executedScriptModules.count(name)) return;
    for (auto& lib : scriptLibraries){
        if (lib.first == name){
            executedScriptModules.insert(name);

            const script_t* prevScriptPtr = currentScript;
            currentScript = &lib.second;

            frames.push_back({currentFileName(), "<module>", 0, &lib.second});

            bool returned = false;
            value returnValue = makeNone();
            executeBody(lib.second.mainFunction.body, returned, returnValue);

            frames.pop_back();
            currentScript = prevScriptPtr;
            return;
        }
    }
}

bool minipy::generatorStateGet(vm& vm, void* handle, const std::string& name, value& result);
bool minipy::generatorStateCall(vm& vm, ffi_instance& self,
                        const std::vector<value>& args,
                        const std::map<std::string, value>& kwargs,
                        value& result);
value vm::invoke(function_t* function, const std::vector<value>& args, value* selfValue,
                          const script_t* sourceScript, const std::string& displayName,
                          const std::map<std::string, value>& kwargs){
    if (!function) return makeNone();
    if (failed) return makeNone();
    if (recursionDepth >= 999){

        pendingCaret = span_t();
        raiseError("RecursionError", "maximum recursion depth exceeded");
        return makeNone();
    }

    MethodContextGuard methodGuard(*this);
    if (selfValue){
        class_t* startKlass = nullptr;
        if (selfValue->type == "object" && selfValue->ptr)
            startKlass = static_cast<instance*>(selfValue->ptr)->klass;
        else if (selfValue->type == "class" && selfValue->ptr)
            startKlass = static_cast<class_t*>(selfValue->ptr);
        class_t* owner = nullptr;
        int guardCount = 0;
        for (class_t* k = startKlass; k != nullptr && guardCount < 1000;
             k = k->baseClass, ++guardCount){
            for (auto& m : k->methods){
                if (&m == function){
                    owner = k;
                    break;
                }
            }
            if (owner) break;
        }
        if (owner){
            pushMethodContext(owner, *selfValue);
            methodGuard.active = true;
        }
    }

    std::string callName = displayName.empty() ? function->name : displayName;
    bool hasSelf = selfValue != nullptr;
    int variadicArg = function->variadicArg;
    int keywordArg = function->keywordArg;

    std::vector<std::string> allParams;
    std::vector<int> paramOrigIndex;
    for (size_t i = 0; i < function->argNames.size(); ++i){
        if (hasSelf && function->argNames[i] == "self") continue;
        allParams.push_back(function->argNames[i]);
        paramOrigIndex.push_back(static_cast<int>(i));
    }

    auto remapIdx = [&](int idx) -> int {
        if (idx < 0) return -1;
        int adjusted = idx;
        if (hasSelf && function->argNames[idx] == "self") return -1;
        for (size_t i = 0; i < paramOrigIndex.size(); ++i){
            if (paramOrigIndex[i] == idx) return static_cast<int>(i);
        }
        return -1;
    };
    int vIdx = remapIdx(variadicArg);
    int kIdx = remapIdx(keywordArg);



    size_t given = args.size();
    size_t nParams = allParams.size();
    size_t maxPositional = (vIdx >= 0) ? static_cast<size_t>(vIdx) : nParams;

    if (given > maxPositional && vIdx < 0){

        size_t selfCount = hasSelf ? 1 : 0;
        size_t takes = maxPositional + selfCount;
        size_t wasGiven = given + selfCount;
        raiseError("TypeError", callName + "() takes " + std::to_string(takes) +
                   (takes == 1 ? " positional argument but " : " positional arguments but ") +
                   std::to_string(wasGiven) + (wasGiven == 1 ? " was given" : " were given"));
        return makeNone();
    }

    std::vector<value> positionalValues(nParams);
    std::vector<bool> filled(nParams, false);
    std::vector<value> variadicExtra;
    for (size_t i = 0; i < maxPositional && i < given; ++i){
        positionalValues[i] = args[i];
        filled[i] = true;
    }
    if (given > maxPositional){
        for (size_t i = maxPositional; i < given; ++i){
            variadicExtra.push_back(args[i]);
        }
    }

    int posOnly = 0;
    for (int origIndex : paramOrigIndex){
        if (origIndex < function->positionalOnlyCount) posOnly++;
    }
    if (posOnly > 0){
        std::vector<std::string> badNames;
        for (auto& entry : kwargs){
            int foundIdx = -1;
            for (size_t i = 0; i < allParams.size(); ++i){
                if (allParams[i] == entry.first){ foundIdx = static_cast<int>(i); break; }
            }
            if (foundIdx >= 0 && foundIdx < posOnly){
                badNames.push_back(entry.first);
            }
        }
        if (!badNames.empty()){
            std::string names;
            for (size_t i = 0; i < badNames.size(); ++i){
                if (i > 0) names += ", ";
                names += badNames[i];
            }
            raiseError("TypeError", callName +
                       "() got some positional-only arguments passed as keyword arguments: '" +
                       names + "'");
            return makeNone();
        }
    }

    std::map<std::string, value> keywordExtra;
    for (auto& entry : kwargs){

        int foundIdx = -1;
        for (size_t i = 0; i < allParams.size(); ++i){
            if (allParams[i] == entry.first){ foundIdx = static_cast<int>(i); break; }
        }
        if (foundIdx >= 0){

            if (filled[foundIdx]){
                raiseError("TypeError", callName + "() got multiple values for argument '" +
                                        entry.first + "'");
                return makeNone();
            }


            positionalValues[foundIdx] = entry.second;
            filled[foundIdx] = true;
        } else {

            if (kIdx >= 0){
                keywordExtra[entry.first] = entry.second;
            } else {
                raiseError("TypeError", callName + "() got an unexpected keyword argument '" +
                                        entry.first + "'");
                return makeNone();
            }
        }
    }

    if (!function->defaultArgs.empty()){
        if (!function->defaultCache){
            function->defaultCache = new std::map<int, value>();
        }
        auto* cache = static_cast<std::map<int, value>*>(function->defaultCache);
        for (auto& def : function->defaultArgs){
            int origIndex = def.first;
            int remapped = -1;
            for (size_t j = 0; j < paramOrigIndex.size(); ++j){
                if (paramOrigIndex[j] == origIndex){ remapped = static_cast<int>(j); break; }
            }
            if (remapped < 0) continue;
            if (filled[remapped]) continue;
            value v;
            auto it = cache->find(origIndex);
            if (it != cache->end()){
                v = it->second;
            } else {
                v = evalExpr(def.second);
                if (failed){
                    methodGuard.active = false;
                    return makeNone();
                }
                (*cache)[origIndex] = v;
            }
            positionalValues[remapped] = v;
            filled[remapped] = true;
        }
    }





    int firstSpecial = static_cast<int>(nParams);
    if (vIdx >= 0) firstSpecial = std::min(firstSpecial, vIdx);
    if (kIdx >= 0) firstSpecial = std::min(firstSpecial, kIdx);
    size_t requiredEnd = static_cast<size_t>(firstSpecial);
    std::vector<std::string> missingNames;
    for (size_t i = 0; i < requiredEnd; ++i){
        if (!filled[i]) missingNames.push_back(allParams[i]);
    }
    if (!missingNames.empty()){
        size_t missing = missingNames.size();
        std::string names;
        for (size_t i = 0; i < missingNames.size(); ++i){
            if (i > 0) names += (missing > 2) ? ", " : " and ";
            names += "'" + missingNames[i] + "'";
        }
        if (missing > 2){
            size_t pos = names.rfind(", ");
            if (pos != std::string::npos) names = names.substr(0, pos) + ", and " + names.substr(pos + 2);
        }
        raiseError("TypeError", callName + "() missing " + std::to_string(missing) +
                   (missing == 1 ? " required positional argument: " : " required positional arguments: ") +
                   names);
        return makeNone();
    }

    value variadicList = makeTuple(variadicExtra);
    value keywordDict = makeDict();
    for (auto& e : keywordExtra){
        dictSet(keywordDict, makeString(e.first), e.second);
    }
    // 函数所属脚本优先于调用点传入的脚本;
    // 否则通过 from ... import 暴露出去的模块函数会丢失自己的模块上下文
    // The owner script of the callee wins over the caller's script; otherwise
    // module functions exported via `from ... import` lose their module context.
    const script_t* ownerScript = findFunctionOwner(function);
    const script_t* effectiveSource = ownerScript ? ownerScript : sourceScript;
    const script_t* previousScript = currentScript;
    if (effectiveSource) currentScript = effectiveSource;
    std::string frameFile = (effectiveSource && !effectiveSource->name.empty())
                                ? effectiveSource->name
                                : currentFileName();
    int functionLine = function->body.empty() ? 0 : function->body[0].line;
    frames.push_back({frameFile, function->name, functionLine, effectiveSource});
    recursionDepth++;
    globalNameStack.push_back(&function->globalNames);
    nonlocalNameStack.push_back(&function->nonlocalNames);

    auto savedCallResults = callResults;
    callResults.clear();
    scopes.push_back({});
    for (size_t i = 0; i < function->argNames.size(); ++i){
        const std::string& name = function->argNames[i];
        value value = makeNone();
        if (selfValue && name == "self"){
            value = *selfValue;
        } else if (static_cast<int>(i) == variadicArg){
            value = variadicList;
        } else if (static_cast<int>(i) == keywordArg){
            value = keywordDict;
        } else {

            int remapped = -1;
            for (size_t j = 0; j < paramOrigIndex.size(); ++j){
                if (paramOrigIndex[j] == static_cast<int>(i)){
                    remapped = static_cast<int>(j);
                    break;
                }
            }
            if (remapped >= 0 && remapped < static_cast<int>(positionalValues.size())){
                value = positionalValues[remapped];
            }
        }
        scopes.back()[name] = value;
    }

    if (function->isGenerator || function->isCoroutine){

        auto* gs = new GeneratorState();
        gs->fn = function;
        gs->localScope = std::move(scopes.back());
        gs->pc = 0;
        gs->status = GeneratorState::CREATED;
        gs->isCoroutine = function->isCoroutine;
        gs->asyncGen = function->isCoroutine && function->isGenerator;
        gs->globalNames = &function->globalNames;
        gs->savedScopeDepth = scopes.size();
        scopes.pop_back();
        frames.pop_back();
        recursionDepth--;
        globalNameStack.pop_back();
        nonlocalNameStack.pop_back();
        currentScript = previousScript;
        callResults = std::move(savedCallResults);
        const char* typeName = gs->asyncGen ? "async_generator"
                              : (function->isCoroutine ? "coroutine" : "generator");
        return makeDynamicObject(typeName, gs, generatorStateGet, generatorStateCall);
    }
    bool returned = false;
    value returnValue = makeNone();
    executeBody(function->body, returned, returnValue);
    scopes.pop_back();
    frames.pop_back();
    recursionDepth--;
    globalNameStack.pop_back();
    nonlocalNameStack.pop_back();
    currentScript = previousScript;
    callResults = std::move(savedCallResults);
    return returnValue;
}





bool minipy::generatorStateGet(vm& vm, void* handle, const std::string& name, value& result){
    auto* gs = static_cast<GeneratorState*>(handle);
    if (!gs) return false;
    if (name == "__next__" || name == "send" || name == "throw" || name == "close" ||
        name == "__iter__" || name == "__await__" || name == "__aiter__" ||
        name == "__anext__" || name == "asend" || name == "athrow" || name == "aclose"){
        result = vm.makeDynamicObject("generator_method", gs, generatorStateGet, generatorStateCall);
        auto* inst = static_cast<ffi_instance*>(result.ptr);
        if (inst) inst->attributes["method_name"] = vm.makeString(name);
        return true;
    }
    return false;
}

bool minipy::generatorStateCall(vm& vm, ffi_instance& self,
                        const std::vector<value>& args,
                        const std::map<std::string, value>& kwargs,
                        value& result){
    auto* gs = static_cast<GeneratorState*>(self.handle);
    if (!gs) return false;
    std::string methodName;
    auto mnIt = self.attributes.find("method_name");
    if (mnIt != self.attributes.end() && mnIt->second.type == "str"){
        methodName = *static_cast<std::string*>(mnIt->second.ptr);
    } else {
        methodName = "__next__";
    }
    bool asyncCompletion = false;
    auto anextIt = self.attributes.find("async_anext");
    if (anextIt != self.attributes.end() && vm.truthy(anextIt->second)){
        asyncCompletion = true;
    } else if (gs->asyncGen &&
               (methodName == "asend" || methodName == "athrow" || methodName == "aclose" ||
                methodName == "send" || methodName == "__anext__" || methodName == "__next__")){


        asyncCompletion = true;
    }

    if (methodName == "__iter__" || methodName == "__await__" || methodName == "__aiter__"){
        const char* typeName = gs->asyncGen ? "async_generator"
                              : (gs->isCoroutine ? "coroutine" : "generator");
        result = vm.makeDynamicObject(typeName, gs, generatorStateGet, generatorStateCall);
        return true;
    }

    if (methodName == "close" || methodName == "aclose"){
        result = vm.makeNone();
        if (gs->status == GeneratorState::CREATED){
            gs->status = GeneratorState::CLOSED;
            return true;
        }
        if (gs->status == GeneratorState::CLOSED) return true;
        if (gs->running){
            vm.raiseError("ValueError", alreadyExecutingMsg(gs));
            return true;
        }
        gs->closing = true;



        bool atDelegation = false;
        if (gs->pc < gs->fn->body.size()){
            unsigned int op = gs->fn->body[gs->pc].operation;
            atDelegation = (op == eop_yield_from || op == eop_await);
        }
        if (!atDelegation){
            vm.pendingExceptionType = "GeneratorExit";
            vm.pendingExceptionMsg.clear();
            vm.pendingExceptionValue = vm.makeNone();
            vm.failed = true;
        }
        value out;
        vm.generatorResumeBody(gs, out, asyncCompletion);
        return true;
    }

    if (methodName == "throw" || methodName == "athrow"){
        std::string excType, excMsg;
        value excValue = vm.makeNone();
        if (!args.empty() && args[0].type == "native_object"){
            auto* instance = static_cast<ffi_instance*>(args[0].ptr);
            if (instance && instance->typeName == "exception" && instance->handle){
                auto* payload = static_cast<ExceptionPayload*>(instance->handle);
                excType = payload->type;
                excMsg = payload->msg;
                excValue = payload->value;
            }
        } else if (!args.empty() && args[0].type == "str"){
            std::string text = *static_cast<std::string*>(args[0].ptr);
            const std::vector<std::string>& names = builtinExceptionNames();
            if (std::find(names.begin(), names.end(), text) != names.end()){
                excType = text;
            }
        }
        if (excType.empty()){
            vm.raiseError("TypeError", "exceptions must derive from BaseException");
            return true;
        }
        if (gs->status == GeneratorState::CREATED || gs->status == GeneratorState::CLOSED){


            vm.raiseError(excType, excMsg, excValue);
            return true;
        }
        if (gs->running){
            vm.raiseError("ValueError", alreadyExecutingMsg(gs));
            return true;
        }

        bool atDelegation = false;
        if (gs->pc < gs->fn->body.size()){
            unsigned int op = gs->fn->body[gs->pc].operation;
            atDelegation = (op == eop_yield_from || op == eop_await);
        }
        if (atDelegation){
            gs->hasPendingThrow = true;
            gs->pendingThrowType = excType;
            gs->pendingThrowMsg = excMsg;
            gs->pendingThrowValue = excValue;
        } else {
            vm.pendingExceptionType = excType;
            vm.pendingExceptionMsg = excMsg;
            vm.pendingExceptionValue = excValue;
            vm.failed = true;
        }
        value out;
        vm.generatorResumeBody(gs, out, asyncCompletion);
        if (!vm.failed) result = out;
        return true;
    }

    if (methodName == "__anext__"){
        if (gs->status == GeneratorState::CLOSED){
            vm.raiseError("StopAsyncIteration", "");
            return true;
        }
        if (gs->running){
            vm.raiseError("ValueError", alreadyExecutingMsg(gs));
            return true;
        }
        result = vm.makeDynamicObject("coroutine", gs, generatorStateGet, generatorStateCall);
        auto* wrapper = static_cast<ffi_instance*>(result.ptr);
        wrapper->attributes["async_anext"] = vm.makeBool(true);
        wrapper->attributes["method_name"] = vm.makeString("__next__");
        return true;
    }

    value sendValue = vm.makeNone();
    if (methodName == "send" || methodName == "asend"){
        sendValue = args.empty() ? vm.makeNone() : args[0];
    }
    if (gs->status == GeneratorState::CLOSED){
        if (asyncCompletion){
            vm.raiseError("StopAsyncIteration", "");
        } else if (gs->isCoroutine && !gs->asyncGen){
            vm.raiseError("RuntimeError", "cannot reuse already awaited coroutine");
        } else {
            vm.raiseError("StopIteration", "");
        }
        return true;
    }
    if (gs->status == GeneratorState::CREATED &&
        (methodName == "send" || methodName == "asend") &&
        !(sendValue.type == "none" && sendValue.ptr == nullptr)){
        vm.raiseError("TypeError", "can't send non-None value to a just-started generator");
        return true;
    }
    if (gs->running){
        vm.raiseError("ValueError", alreadyExecutingMsg(gs));
        return true;
    }
    gs->pendingSend = sendValue;
    gs->hasPendingSend = true;
    vm.generatorResumeBody(gs, result, asyncCompletion);
    return true;
}

static bool exceptionPayloadGet(vm& vm, void* handle, const std::string& name,
                                value& result){
    auto* payload = static_cast<ExceptionPayload*>(handle);
    if (!payload) return false;
    if (name == "v"){
        result = payload->value;
        return true;
    }
    if (name == "args"){
        if (payload->value.type != "none"){
            result = vm.makeTuple({payload->value});
        } else if (!payload->msg.empty()){
            result = vm.makeTuple({vm.makeString(payload->msg)});
        } else {
            result = vm.makeTuple({});
        }
        return true;
    }
    return false;
}
static bool exceptionPayloadCall(vm&, ffi_instance&,
                                 const std::vector<value>&,
                                 const std::map<std::string, value>&,
                                 value&){
    return false;
}

// generator 状态 Of
// generator state of
// generator ᠪᠠᠶᠢᠳᠠᠯ Of
static GeneratorState* generatorStateOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* instance = static_cast<ffi_instance*>(v.ptr);
    if (instance->typeName != "generator" && instance->typeName != "coroutine" &&
        instance->typeName != "async_generator"){
        return nullptr;
    }
    return static_cast<GeneratorState*>(instance->handle);
}

// Deleg Result
// deleg result
// Deleg Result
enum DelegResult { DL_YIELD = 0, DL_DONE = 1, DL_ERROR = 2 };

value vm::makeExceptionObject(const std::string& type, const std::string& msg,
                                       const value& v){
    auto* payload = new ExceptionPayload();
    payload->type = type;
    payload->msg = msg;
    payload->value = v;

    value result = makeDynamicObject("exception", payload,
                                       exceptionPayloadGet, exceptionPayloadCall);
    if (result.type == "native_object" && result.ptr){
        static_cast<ffi_instance*>(result.ptr)->ownedValue =
            value("exception", keep(vk_exception, payload), this);
    }
    return result;
}


value vm::getBuiltinTypeObj(const std::string& typeName){
    auto& cache = this->builtinTypeCache;
    auto it = cache.find(typeName);
    if (it != cache.end()) return it->second;
    for (auto* lib : cffiLibraries){
        if (!lib || lib->name != "builtins") continue;
        auto fnIt = lib->functions.find(typeName);
        if (fnIt != lib->functions.end() && fnIt->second != nullptr){
            auto* ni = new ffi_instance();
            ni->library = lib;
            ni->klass = nullptr;
            ni->typeName = typeName;
            ni->handle = fnIt->second;
            ni->isTypeObject = true;
            value obj("native_object", keep(vk_native_object, ni), this);
            cache[typeName] = obj;
            return obj;
        }
        break;
    }
    static const std::set<std::string> kPureTypeNames = {
        "NoneType", "ellipsis", "NotImplementedType", "type", "object",
        "function", "generator", "coroutine", "async_generator", "method",
        "builtin_function_or_method", "module", "code", "cell", "UnionType",
        "slice", "property", "staticmethod", "classmethod", "memoryview",
        "frame", "traceback", "mappingproxy"
    };
    if (kPureTypeNames.count(typeName)){
        auto* ni = new ffi_instance();
        ni->klass = nullptr;
        ni->typeName = typeName;
        ni->isTypeObject = true;
        value obj("native_object", keep(vk_native_object, ni), this);
        cache[typeName] = obj;
        return obj;
    }
    return makeNone();
}
class_t* vm::getClassOfScriptObject(const value& obj){
    if (obj.type == "object" && obj.ptr){
        auto* inst = static_cast<instance*>(obj.ptr);
        return inst->klass;
    }
    return nullptr;
}

value vm::adoptClass(class_t* klass){
    initializeClass(*klass);
    return value("class", keep(vk_class, klass), this);
}

// 复数访问公共包装 (cmath 等模块使用)
// public complex accessors (used by cmath etc.)
// ᠰᠢᠯᠢ ᠲᠣ᠎ ᠶᠢᠨ ᠨᠠᠢᠳᠠᠪᠤᠷᠢ ᠰᠢᠯᠲᠡ
bool minipy::isComplexValueEx(const minipy::value& v){
    return isComplexValue(v);
}

bool minipy::complexPartsEx(const minipy::value& v, double& real, double& imag){
    return complexParts(v, real, imag);
}

bool minipy::closureFunctionCopy(const minipy::value& v, minipy::function_t& out){
    if (v.type != "native_object" || v.ptr == nullptr) return false;
    auto* inst = static_cast<minipy::ffi_instance*>(v.ptr);
    if (!inst || inst->typeName != "closure" || inst->handle == nullptr) return false;
    auto* ch = static_cast<closure_handle*>(inst->handle);
    if (!ch || !ch->fn) return false;
    out = *ch->fn;
    return true;
}

void vm::clearPendingExceptionState(){
    failed = false;
    lastErr.clear();
    pendingExceptionType.clear();
    pendingExceptionMsg.clear();
    pendingExceptionValue = makeNone();
}


void vm::generatorResumeBody(GeneratorState* gs, value& out, bool asyncCompletion){
    out = makeNone();
    if (gs->running){
        raiseError("ValueError", alreadyExecutingMsg(gs));
        return;
    }
    gs->running = true;
    // 恢复生成器/协程时, 切回其函数所属脚本, 保证模块内名称解析正确
    // When resuming, switch back to the script that owns the generator function.
    struct ScriptRestoreGuard{
        const script_t*& slot;
        const script_t* saved;
        ~ScriptRestoreGuard(){ slot = saved; }
    } scriptGuard{currentScript, currentScript};
    {
        const script_t* ownerScript = gs->fn ? findFunctionOwner(gs->fn) : nullptr;
        if (ownerScript) currentScript = ownerScript;
    }
    scopes.push_back(std::move(gs->localScope));
    globalNameStack.push_back(gs->globalNames);
    int functionLine = gs->fn && !gs->fn->body.empty() ? gs->fn->body[0].line : 0;
    std::string fileName = currentScript ? currentScript->name : "";
    frames.push_back({fileName, gs->fn ? gs->fn->name : "generator", functionLine, currentScript});
    recursionDepth++;

    auto savedCallResults = callResults;
    callResults.clear();



    if (gs->hasPendingSend && !gs->pendingTarget.empty()){
        scopes.back()[gs->pendingTarget] = gs->pendingSend;
        gs->hasPendingSend = false;
        gs->pendingTarget.clear();
    }
    bool returned = false;
    value returnValue = makeNone();
    bool paused = false;
    size_t pausePc = 0;
    executeBody(gs->fn->body, returned, returnValue, gs->pc, &paused, &pausePc, gs);

    for (auto& kv : gs->forStates) kv.second.target = nullptr;
    gs->localScope = std::move(scopes.back());
    scopes.pop_back();
    frames.pop_back();
    recursionDepth--;
    globalNameStack.pop_back();
    callResults = std::move(savedCallResults);
    gs->running = false;
    if (paused){

        unsigned int op = pausePc < gs->fn->body.size() ? gs->fn->body[pausePc].operation : 0;
        gs->pc = (op == eop_yield_from || op == eop_await || op == eop_for) ? pausePc : pausePc + 1;
        if (gs->closing){

            gs->status = GeneratorState::CLOSED;
            gs->closing = false;
            raiseError("RuntimeError", "generator ignored GeneratorExit");
            return;
        }
        gs->status = GeneratorState::SUSPENDED;
        out = returnValue;
        return;
    }
    gs->status = GeneratorState::CLOSED;
    bool wasClosing = gs->closing;
    gs->closing = false;
    if (!failed){
        if (wasClosing){


            return;
        }

        raiseError(asyncCompletion ? "StopAsyncIteration" : "StopIteration", "", returnValue);
        return;
    }

    if (pendingExceptionType == "GeneratorExit"){

        clearPendingExceptionState();
        return;
    }
    if (pendingExceptionType == "StopIteration"){

        failed = false;
        raiseError("RuntimeError", "generator raised StopIteration");
    }
}

int vm::delegNextValue(DelegState& ds, int action, const value& payload, value& out){
    out = makeNone();
    if (ds.mode == 1){

        auto* instance = static_cast<ffi_instance*>(ds.sub.ptr);
        if (!instance){
            raiseError("TypeError", "delegation target is not a generator");
            return DL_ERROR;
        }
        bool asyncCompletion = false;
        auto anextIt = instance->attributes.find("async_anext");
        if (anextIt != instance->attributes.end() && truthy(anextIt->second)){
            asyncCompletion = true;
        }
        std::string completion = asyncCompletion ? "StopAsyncIteration" : "StopIteration";
        const char* methodName = "__next__";
        std::vector<value> callArgs;
        if (action == 1){
            methodName = "send";
            callArgs.push_back(payload);
        } else if (action == 2){
            methodName = "throw";
            callArgs.push_back(payload);
        } else if (action == 3){
            methodName = "close";
        }
        instance->attributes["method_name"] = makeString(methodName);
        value v;
        std::map<std::string, value> noKwargs;
        generatorStateCall(*this, *instance, callArgs, noKwargs, v);
        if (failed){
            if (action != 3 && pendingExceptionType == completion){
                ds.result = pendingExceptionValue;
                clearPendingExceptionState();
                return DL_DONE;
            }
            return DL_ERROR;
        }
        if (action == 3){

            raiseError("GeneratorExit", "");
            return DL_ERROR;
        }
        out = v;
        return DL_YIELD;
    }
    if (ds.mode == 2){

        if (ds.index < ds.items.size()){
            out = ds.items[ds.index++];
            return DL_YIELD;
        }
        return DL_DONE;
    }
    if (ds.mode == 3){

        if (ds.index < ds.range.count()){
            out = makeInt(ds.range.at(ds.index++));
            return DL_YIELD;
        }
        return DL_DONE;
    }
    return DL_DONE;
}

value vm::callBuiltin(const std::string& name, const std::vector<value>& args,
                               bool& handled, const std::map<std::string, value>& kwargs){
    for (auto* library : cffiLibraries){


        if (!library || library->name != "builtins") continue;
        value result = makeNone();
        if (cffiCallFunction(*library, name, *this, args, result, kwargs)){
            handled = true;
            return result;
        }
    }
    handled = false;
    return makeNone();
}

value vm::callBuiltinMethod(const std::string& method, value& target,
                                     const std::vector<value>& args, bool& handled,
                                     const std::map<std::string, value>& kwargs){
    handled = true;
    if ((target.type == "bytes" || target.type == "bytearray") && target.ptr){
        std::string raw = *static_cast<std::string*>(target.ptr);
        bool isBA = (target.type == "bytearray");
        auto mk = [&](const std::string& text) -> value {
            value r = makeString(text);
            r.type = isBA ? "bytearray" : "bytes";
            return r;
        };
        auto bytesOf = [](const value& item) -> bool {
            return item.type == "bytes" || item.type == "bytearray";
        };
        if (method == "decode"){
            std::string encoding = args.empty() ? "utf-8" : toString(args[0]);
            std::string enc;
            for (char c : encoding)
                enc.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (enc == "utf-8" || enc == "utf8" || enc == "u8") return makeString(raw);
            if (enc == "ascii" || enc == "us-ascii"){
                for (size_t i = 0; i < raw.size(); ++i){
                    unsigned char ch = static_cast<unsigned char>(raw[i]);
                    if (ch >= 128){
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\x%02x", ch);
                        raiseError("UnicodeDecodeError",
                                   "'ascii' codec can't decode byte " + std::string(buf) +
                                   " in position " + std::to_string(i) +
                                   ": ordinal not in range(128)");
                        return makeNone();
                    }
                }
                return makeString(raw);
            }
            if (enc == "latin-1" || enc == "latin1" || enc == "iso-8859-1"){
                std::string out;
                for (unsigned char ch : raw) appendUtf8Codepoint(out, ch);
                return makeString(out);
            }
            raiseError("LookupError", "unknown encoding: " + encoding);
            return makeNone();
        }
        if (method == "hex"){
            std::string out;
            char buf[4];
            for (unsigned char ch : raw){
                std::snprintf(buf, sizeof(buf), "%02x", ch);
                out += buf;
            }
            return makeString(out);
        }
        if (method == "upper" || method == "lower"){
            std::string out = raw;
            for (auto& c : out){
                unsigned char ch = static_cast<unsigned char>(c);
                if (ch < 128){
                    c = static_cast<char>(method == "upper" ? std::toupper(ch)
                                                             : std::tolower(ch));
                }
            }
            return mk(out);
        }
        if (method == "strip" || method == "lstrip" || method == "rstrip"){
            std::string chars = " \t\n\r\v\f";
            if (!args.empty() && args[0].type != "none" && bytesOf(args[0]))
                chars = *static_cast<std::string*>(args[0].ptr);
            auto inset = [&](char c){ return chars.find(c) != std::string::npos; };
            size_t b = 0, e = raw.size();
            if (method != "rstrip") while (b < e && inset(raw[b])) ++b;
            if (method != "lstrip") while (e > b && inset(raw[e - 1])) --e;
            return mk(raw.substr(b, e - b));
        }
        if (method == "split" || method == "rsplit"){
            long long maxsplit = -1;
            if (args.size() >= 2) maxsplit = toInt(args[1]);
            std::vector<value> parts;
            bool hasSep = !args.empty() && args[0].type != "none" && bytesOf(args[0]);
            if (!hasSep){
                std::string sepChars = " \t\n\r\v\f";
                std::vector<std::string> tmp;
                if (maxsplit == 0){
                    size_t b = 0;
                    while (b < raw.size() && sepChars.find(raw[b]) != std::string::npos) ++b;
                    size_t e = raw.size();
                    while (e > b && sepChars.find(raw[e - 1]) != std::string::npos) --e;
                    if (b < e) tmp.push_back(raw.substr(b, e - b));
                } else if (method == "split"){
                    size_t i = 0;
                    long long splits = 0;
                    while (i < raw.size()){
                        while (i < raw.size() && sepChars.find(raw[i]) != std::string::npos) ++i;
                        if (i >= raw.size()) break;
                        if (maxsplit >= 0 && splits >= maxsplit){
                            tmp.push_back(raw.substr(i));
                            break;
                        }
                        size_t j = i;
                        while (j < raw.size() && sepChars.find(raw[j]) == std::string::npos) ++j;
                        tmp.push_back(raw.substr(i, j - i));
                        splits++;
                        i = j;
                    }
                } else {
                    long long i = static_cast<long long>(raw.size()) - 1;
                    long long end = i + 1;
                    long long splits = 0;
                    while (i >= 0){
                        while (i >= 0 && sepChars.find(raw[static_cast<size_t>(i)]) != std::string::npos) --i;
                        if (i < 0) break;
                        if (maxsplit >= 0 && splits >= maxsplit){
                            tmp.push_back(raw.substr(0, static_cast<size_t>(i + 1)));
                            break;
                        }
                        long long j = i;
                        while (j >= 0 && sepChars.find(raw[static_cast<size_t>(j)]) == std::string::npos) --j;
                        tmp.push_back(raw.substr(static_cast<size_t>(j + 1),
                                                 static_cast<size_t>(end - j - 1)));
                        splits++;
                        end = j + 1;
                        i = j;
                    }
                    std::reverse(tmp.begin(), tmp.end());
                }
                for (auto& piece : tmp) parts.push_back(mk(piece));
            } else {
                std::string sep = *static_cast<std::string*>(args[0].ptr);
                if (sep.empty()){
                    raiseError("ValueError", "empty separator");
                    return makeNone();
                }
                if (method == "split"){
                    size_t pos = 0;
                    long long splits = 0;
                    while (true){
                        if (maxsplit >= 0 && splits >= maxsplit){
                            parts.push_back(mk(raw.substr(pos)));
                            break;
                        }
                        size_t hit = raw.find(sep, pos);
                        if (hit == std::string::npos){
                            parts.push_back(mk(raw.substr(pos)));
                            break;
                        }
                        parts.push_back(mk(raw.substr(pos, hit - pos)));
                        pos = hit + sep.size();
                        splits++;
                    }
                } else {
                    std::vector<std::string> tmp;
                    size_t end = raw.size();
                    long long splits = 0;
                    while (true){
                        if (maxsplit >= 0 && splits >= maxsplit){
                            tmp.push_back(raw.substr(0, end));
                            break;
                        }
                        size_t hit = end == 0 ? std::string::npos : raw.rfind(sep, end - 1);
                        if (hit == std::string::npos || hit + sep.size() > end){
                            tmp.push_back(raw.substr(0, end));
                            break;
                        }
                        tmp.push_back(raw.substr(hit + sep.size(), end - hit - sep.size()));
                        end = hit;
                        splits++;
                    }
                    std::reverse(tmp.begin(), tmp.end());
                    for (auto& piece : tmp) parts.push_back(mk(piece));
                }
            }
            return makeList(parts);
        }
        if (method == "join"){
            std::vector<value> items;
            if (!args.empty()) materializeIterable(args[0], items);
            if (hasError()) return makeNone();
            std::string out;
            for (size_t i = 0; i < items.size(); ++i){
                if (!bytesOf(items[i])){
                    raiseError("TypeError", "sequence item " + std::to_string(i) +
                                            ": expected a bytes-like object, " +
                                            pythonTypeName(items[i]) + " found");
                    return makeNone();
                }
                if (i > 0) out += raw;
                out += *static_cast<std::string*>(items[i].ptr);
            }
            return mk(out);
        }
        if (method == "replace"){
            if (args.size() < 2){
                raiseError("TypeError", "replace() takes at least 2 arguments (" +
                                        std::to_string(args.size()) + " given)");
                return makeNone();
            }
            if (!bytesOf(args[0]) || !bytesOf(args[1])){
                raiseError("TypeError", "replace() argument must be a bytes-like object");
                return makeNone();
            }
            std::string oldS = *static_cast<std::string*>(args[0].ptr);
            std::string newS = *static_cast<std::string*>(args[1].ptr);
            long long count = -1;
            if (args.size() >= 3) count = toInt(args[2]);
            std::string out;
            if (oldS.empty()){
                long long done = 0;
                auto canDo = [&](){ return count < 0 || done < count; };
                if (canDo()){ out += newS; done++; }
                for (size_t i = 0; i < raw.size(); ++i){
                    out.push_back(raw[i]);
                    if (canDo()){ out += newS; done++; }
                }
                return mk(out);
            }
            size_t pos = 0;
            long long done = 0;
            while (true){
                size_t hit = raw.find(oldS, pos);
                if (hit == std::string::npos || (count >= 0 && done >= count)){
                    out += raw.substr(pos);
                    break;
                }
                out += raw.substr(pos, hit - pos);
                out += newS;
                pos = hit + oldS.size();
                done++;
            }
            return mk(out);
        }
        if (method == "find" || method == "rfind" ||
            method == "index" || method == "rindex" || method == "count"){
            if (args.empty()){
                raiseError("TypeError", method + "() takes at least 1 argument (0 given)");
                return makeNone();
            }
            if (!bytesOf(args[0])){
                raiseError("TypeError", "a bytes-like object is required, not '" +
                                        pythonTypeName(args[0]) + "'");
                return makeNone();
            }
            std::string needle = *static_cast<std::string*>(args[0].ptr);
            long long start = 0, end = static_cast<long long>(raw.size());
            if (args.size() >= 2) start = toInt(args[1]);
            if (args.size() >= 3) end = toInt(args[2]);
            if (start < 0) start += static_cast<long long>(raw.size());
            if (end < 0) end += static_cast<long long>(raw.size());
            if (start < 0) start = 0;
            if (end > static_cast<long long>(raw.size())) end = static_cast<long long>(raw.size());
            std::string region = (start < end)
                ? raw.substr(static_cast<size_t>(start), static_cast<size_t>(end - start))
                : std::string();
            if (method == "count"){
                long long n = 0;
                if (needle.empty()){
                    n = static_cast<long long>(region.size()) + 1;
                } else {
                    size_t pos = 0;
                    while ((pos = region.find(needle, pos)) != std::string::npos){
                        n++;
                        pos += needle.size();
                    }
                }
                return makeInt(n);
            }
            size_t hit = (method == "rfind" || method == "rindex")
                ? region.rfind(needle) : region.find(needle);
            if (hit == std::string::npos){
                if (method == "index" || method == "rindex"){
                    raiseError("ValueError", "subsection not found");
                    return makeNone();
                }
                return makeInt(-1);
            }
            return makeInt(start + static_cast<long long>(hit));
        }
        if (method == "startswith" || method == "endswith"){
            if (args.empty()){
                raiseError("TypeError", method + "() takes at least 1 argument (0 given)");
                return makeNone();
            }
            long long start = 0, end = static_cast<long long>(raw.size());
            if (args.size() >= 2) start = toInt(args[1]);
            if (args.size() >= 3) end = toInt(args[2]);
            if (start < 0) start += static_cast<long long>(raw.size());
            if (end < 0) end += static_cast<long long>(raw.size());
            if (start < 0) start = 0;
            if (end > static_cast<long long>(raw.size())) end = static_cast<long long>(raw.size());
            std::string region = (start < end)
                ? raw.substr(static_cast<size_t>(start), static_cast<size_t>(end - start))
                : std::string();
            bool ok = false;
            auto testOne = [&](const value& affix) -> bool {
                if (!bytesOf(affix)){
                    raiseError("TypeError", "startswith/endswith first arg must be bytes or "
                                            "a tuple of bytes, not '" +
                                            pythonTypeName(affix) + "'");
                    return false;
                }
                std::string s = *static_cast<std::string*>(affix.ptr);
                if (s.size() > region.size()) return false;
                if (method == "startswith")
                    return region.compare(0, s.size(), s) == 0;
                return region.compare(region.size() - s.size(), s.size(), s) == 0;
            };
            if (args[0].type == "tuple" || args[0].type == "list"){
                auto* items = static_cast<std::vector<value>*>(args[0].ptr);
                for (auto& item : *items){
                    if (testOne(item)){ ok = true; break; }
                }
            } else {
                ok = testOne(args[0]);
            }
            if (hasError()) return makeNone();
            return makeBool(ok);
        }
        if (method == "removeprefix" || method == "removesuffix"){
            if (args.empty() || !bytesOf(args[0])){
                raiseError("TypeError", method + "() argument must be a bytes-like object");
                return makeNone();
            }
            std::string affix = *static_cast<std::string*>(args[0].ptr);
            if (method == "removeprefix"){
                if (raw.compare(0, affix.size(), affix) == 0)
                    return mk(raw.substr(affix.size()));
                return mk(raw);
            }
            if (affix.size() <= raw.size() &&
                raw.compare(raw.size() - affix.size(), affix.size(), affix) == 0)
                return mk(raw.substr(0, raw.size() - affix.size()));
            return mk(raw);
        }
        if (method == "isalpha" || method == "isdigit" ||
            method == "isspace" || method == "isupper" ||
            method == "islower" || method == "isascii"){
            bool result = !raw.empty();
            bool hasCased = false;
            for (unsigned char ch : raw){
                if (method == "isascii"){
                    if (ch >= 128){ result = false; break; }
                    continue;
                }
                if (method == "isspace"){ result = result && std::isspace(ch) != 0; }
                else if (method == "isdigit"){ result = result && std::isdigit(ch) != 0; }
                else if (method == "isalpha"){ result = result && std::isalpha(ch) != 0; }
                else if (method == "isupper"){
                    if (std::isalpha(ch)) hasCased = true;
                    if (std::islower(ch)) result = false;
                } else if (method == "islower"){
                    if (std::isalpha(ch)) hasCased = true;
                    if (std::isupper(ch)) result = false;
                }
            }
            if ((method == "isupper" || method == "islower") && !hasCased) result = false;
            return makeBool(result);
        }
        if (method == "copy"){
            return mk(raw);
        }
        handled = false;
        return makeNone();
    }

    if (target.type == "str" && target.ptr){
        std::string text = *static_cast<std::string*>(target.ptr);
        if (method == "upper"){
            for (auto& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return makeString(text);
        }
        if (method == "lower"){
            for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return makeString(text);
        }
        if (method == "strip") return makeString(trimString(text));
        if (method == "lstrip") return makeString(trimLeftString(text));
        if (method == "rstrip") return makeString(trimRightString(text));
        if (method == "capitalize"){
            if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return makeString(text);
        }
        if (method == "title"){
            bool newWord = true;
            for (auto& c : text){
                if (std::isalpha(static_cast<unsigned char>(c))){
                    c = static_cast<char>(newWord ? std::toupper(static_cast<unsigned char>(c))
                                                  : std::tolower(static_cast<unsigned char>(c)));
                    newWord = false;
                } else {
                    newWord = true;
                }
            }
            return makeString(text);
        }
        if (method == "swapcase"){
            for (auto& c : text){
                if (std::islower(static_cast<unsigned char>(c))) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                else if (std::isupper(static_cast<unsigned char>(c))) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return makeString(text);
        }
        if (method == "casefold" || method == "lower"){
            for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return makeString(text);
        }
        if (method == "splitlines"){
            bool keepends = !args.empty() && truthy(args[0]);
            std::vector<value> parts;
            size_t i = 0;
            while (i < text.size()){
                size_t begin = i;
                while (i < text.size() && text[i] != '\n' && text[i] != '\r') i++;
                size_t endContent = i;
                if (i < text.size()){
                    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') i += 2;
                    else i += 1;
                }
                std::string line = text.substr(begin, (keepends ? i : endContent) - begin);
                parts.push_back(makeString(line));
            }
            return makeList(parts);
        }
        if (method == "zfill"){
            if (args.empty()) return makeString(text);
            long long width = toInt(args[0]);
            if (width <= static_cast<long long>(text.size())) return makeString(text);
            std::string sign;
            std::string body = text;
            if (!body.empty() && (body[0] == '+' || body[0] == '-')){
                sign = body.substr(0, 1);
                body = body.substr(1);
            }
            std::string pad(static_cast<size_t>(width) - text.size(), '0');
            return makeString(sign + pad + body);
        }
        if (method == "center" || method == "ljust" || method == "rjust"){
            if (args.empty()) return makeString(text);
            long long width = toInt(args[0]);
            std::string fill = " ";
            if (args.size() >= 2) fill = toString(args[1]);
            if (fill.empty()) fill = " ";
            if (width <= static_cast<long long>(text.size())) return makeString(text);
            long long total = width - static_cast<long long>(text.size());
            if (method == "ljust"){
                std::string pad;
                while (static_cast<long long>(pad.size()) < total) pad += fill;
                pad.resize(static_cast<size_t>(total));
                return makeString(text + pad);
            }
            if (method == "rjust"){
                std::string pad;
                while (static_cast<long long>(pad.size()) < total) pad += fill;
                pad.resize(static_cast<size_t>(total));
                return makeString(pad + text);
            }

            long long left = total / 2;
            long long right = total - left;
            std::string leftPad, rightPad;
            while (static_cast<long long>(leftPad.size()) < left) leftPad += fill;
            while (static_cast<long long>(rightPad.size()) < right) rightPad += fill;
            leftPad.resize(static_cast<size_t>(left));
            rightPad.resize(static_cast<size_t>(right));
            return makeString(leftPad + text + rightPad);
        }
        if (method == "format"){
            std::string result;
            size_t autoIndex = 0;
            for (size_t i = 0; i < text.size(); ++i){
                if (text[i] == '{'){
                    if (i + 1 < text.size() && text[i + 1] == '{'){ result += '{'; i++; continue; }
                    size_t close = text.find('}', i + 1);
                    if (close == std::string::npos){ result += text.substr(i); break; }
                    std::string field = text.substr(i + 1, close - i - 1);
                    std::string spec;
                    char conv = 0;
                    size_t colon = field.find(':');
                    size_t bang = field.find('!');
                    size_t cut = std::min(colon == std::string::npos ? field.size() : colon,
                                          bang == std::string::npos ? field.size() : bang);
                    std::string name = field.substr(0, cut);
                    if (bang != std::string::npos && bang + 1 < field.size()) conv = field[bang + 1];
                    if (colon != std::string::npos) spec = field.substr(colon + 1);
                    value v;
                    if (name.empty()){
                        if (autoIndex < args.size()) v = args[autoIndex];
                        autoIndex++;
                    } else if (std::isdigit(static_cast<unsigned char>(name[0]))){
                        size_t idx = static_cast<size_t>(std::stoll(name));
                        if (idx < args.size()) v = args[idx];
                    } else {
                        auto it = kwargs.find(name);
                        if (it != kwargs.end()) v = it->second;
                    }
                    if (v.type == "none" && name.empty() && autoIndex > args.size()){
                        raiseError("IndexError", "Replacement index " + std::to_string(autoIndex - 1) +
                                                 " out of range for positional args tuple");
                        return makeNone();
                    }
                    std::string out;
                    if (conv == 'r'){
                        out = pythonRepr(*this, v);
                        if (!spec.empty()) out = toString(applyFormat(makeString(out), spec));
                    } else {
                        out = toString(applyFormat(v, spec));
                    }
                    result += out;
                    i = close;
                    continue;
                }
                if (text[i] == '}' && i + 1 < text.size() && text[i + 1] == '}'){
                    result += '}';
                    i++;
                    continue;
                }
                result += text[i];
            }
            return makeString(result);
        }
        if (method == "split" || method == "rsplit"){
            std::vector<value> parts;
            long long maxsplit = -1;
            if (!args.empty()){
                auto kwIt = kwargs.find("maxsplit");
                if (args.size() >= 2) maxsplit = toInt(args[1]);
                else if (kwIt != kwargs.end()) maxsplit = toInt(kwIt->second);
            } else {
                auto kwIt = kwargs.find("maxsplit");
                if (kwIt != kwargs.end()) maxsplit = toInt(kwIt->second);
            }
            bool plainSep = args.empty() || args[0].type == "none";
            if (plainSep){
                size_t i = 0;
                while (i < text.size()){
                    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) i++;
                    if (i >= text.size()) break;
                    if (maxsplit >= 0 && static_cast<long long>(parts.size()) >= maxsplit){

                        size_t end = text.size();
                        while (end > i && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
                        parts.push_back(makeString(text.substr(i, end - i)));
                        break;
                    }
                    size_t begin = i;
                    while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) i++;
                    parts.push_back(makeString(text.substr(begin, i - begin)));
                }
            } else {
                std::string separator = toString(args[0]);
                if (separator.empty()){
                    raiseError("ValueError", "empty separator");
                    return makeNone();
                }
                if (method == "split"){
                    size_t begin = 0;
                    while (true){
                        if (maxsplit >= 0 && static_cast<long long>(parts.size()) >= maxsplit){
                            parts.push_back(makeString(text.substr(begin)));
                            break;
                        }
                        size_t found = text.find(separator, begin);
                        if (found == std::string::npos){
                            parts.push_back(makeString(text.substr(begin)));
                            break;
                        }
                        parts.push_back(makeString(text.substr(begin, found - begin)));
                        begin = found + separator.size();
                    }
                } else {
                    size_t end = text.size();
                    std::vector<value> reversed;
                    while (true){
                        if (maxsplit >= 0 && static_cast<long long>(parts.size()) >= maxsplit){
                            reversed.push_back(makeString(text.substr(0, end)));
                            break;
                        }
                        size_t found = (end == 0) ? std::string::npos
                                                  : text.rfind(separator, end - 1);
                        if (found == std::string::npos || found + separator.size() > end){
                            reversed.push_back(makeString(text.substr(0, end)));
                            break;
                        }
                        reversed.push_back(makeString(text.substr(found + separator.size(),
                                                                 end - found - separator.size())));
                        end = found;
                    }
                    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it)
                        parts.push_back(*it);
                }
            }
            return makeList(parts);
        }
        if (method == "join"){
            if (args.empty() || args[0].type != "list") return makeString("");
            auto* list = static_cast<std::vector<value>*>(args[0].ptr);
            std::string result;
            for (size_t i = 0; i < list->size(); ++i){
                if ((*list)[i].type != "str"){
                    raiseError("TypeError", "sequence item " + std::to_string(i) +
                                                ": expected str instance, " +
                                                pythonTypeName((*list)[i]) + " found");
                    return makeNone();
                }
                if (i > 0) result += text;
                result += toString((*list)[i]);
            }
            return makeString(result);
        }
        if (method == "replace"){
            if (args.size() < 2) return makeString(text);
            std::string from = toString(args[0]);
            std::string to = toString(args[1]);
            if (from.empty()) return makeString(text);
            long long limit = (args.size() >= 3) ? toInt(args[2]) : -1;
            size_t begin = 0;
            long long done = 0;
            while (limit < 0 || done < limit){
                size_t found = text.find(from, begin);
                if (found == std::string::npos) break;
                text.replace(found, from.size(), to);
                begin = found + to.size();
                done++;
            }
            return makeString(text);
        }
        if (method == "startswith" || method == "endswith"){
            if (args.empty()) return makeBool(false);
            long long length = static_cast<long long>(text.size());
            long long begin = 0;
            long long end = length;
            if (args.size() >= 2){
                begin = toInt(args[1]);
                if (begin < 0) begin += length;
                if (begin < 0) begin = 0;
                if (begin > length) begin = length;
            }
            if (args.size() >= 3){
                end = toInt(args[2]);
                if (end < 0) end += length;
                if (end < 0) end = 0;
                if (end > length) end = length;
            }
            if (end < begin) end = begin;
            auto matches = [&](const std::string& affix){
                long long affixLen = static_cast<long long>(affix.size());
                if (affixLen > end - begin) return false;
                long long at = (method == "startswith") ? begin : end - affixLen;
                return text.compare(static_cast<size_t>(at), affix.size(), affix) == 0;
            };
            std::vector<std::string> candidates;
            if (args[0].type == "tuple" && args[0].ptr){
                for (auto& item : *static_cast<std::vector<value>*>(args[0].ptr))
                    candidates.push_back(toString(item));
            } else {
                candidates.push_back(toString(args[0]));
            }
            for (auto& candidate : candidates){
                if (matches(candidate)) return makeBool(true);
            }
            return makeBool(false);
        }
        if (method == "find" || method == "rfind" || method == "index" || method == "rindex"){
            if (args.empty()) return makeInt(-1);
            std::string needle = toString(args[0]);
            long long length = static_cast<long long>(text.size());
            long long begin = 0;
            long long end = length;
            if (args.size() >= 2){
                begin = toInt(args[1]);
                if (begin < 0) begin += length;
                if (begin < 0) begin = 0;
                if (begin > length) begin = length;
            }
            if (args.size() >= 3){
                end = toInt(args[2]);
                if (end < 0) end += length;
                if (end < 0) end = 0;
                if (end > length) end = length;
            }
            if (end < begin) end = begin;
            long long found = -1;
            if (!needle.empty()){
                size_t at = (method == "rfind" || method == "rindex")
                                ? text.rfind(needle, static_cast<size_t>(end))
                                : text.find(needle, static_cast<size_t>(begin));
                if (at != std::string::npos && static_cast<long long>(at) <= end &&
                    static_cast<long long>(at) + static_cast<long long>(needle.size()) <= end){
                    found = static_cast<long long>(at);
                }
            } else if (begin <= end){
                found = (method == "rfind" || method == "rindex") ? end : begin;
            }
            if (found < 0 && (method == "index" || method == "rindex")){
                raiseError("ValueError", "substring not found");
                return makeNone();
            }
            return makeInt(found);
        }
        if (method == "count"){
            if (args.empty()) return makeInt(0);
            std::string needle = toString(args[0]);
            long long length = static_cast<long long>(text.size());
            long long begin = 0;
            long long end = length;
            if (args.size() >= 2){
                begin = toInt(args[1]);
                if (begin < 0) begin += length;
                if (begin < 0) begin = 0;
                if (begin > length) begin = length;
            }
            if (args.size() >= 3){
                end = toInt(args[2]);
                if (end < 0) end += length;
                if (end < 0) end = 0;
                if (end > length) end = length;
            }
            if (end < begin) end = begin;
            long long count = 0;
            if (needle.empty()){
                count = end - begin + 1;
            } else {
                size_t beginAt = static_cast<size_t>(begin);
                while (beginAt + needle.size() <= static_cast<size_t>(end)){
                    size_t found = text.find(needle, beginAt);
                    if (found == std::string::npos || found + needle.size() > static_cast<size_t>(end))
                        break;
                    count++;
                    beginAt = found + needle.size();
                }
            }
            return makeInt(count);
        }
        if (method == "isdigit" || method == "isalpha" || method == "isspace"){
            if (text.empty()) return makeBool(false);
            for (char c : text){
                bool ok = (method == "isdigit")
                              ? std::isdigit(static_cast<unsigned char>(c))
                              : (method == "isalpha")
                                    ? std::isalpha(static_cast<unsigned char>(c))
                                    : std::isspace(static_cast<unsigned char>(c));
                if (!ok) return makeBool(false);
            }
            return makeBool(true);
        }
        if (method == "isdigit"){
            return makeBool(false);
        }
    }
    if (auto* list = listBacking(target)){
        if (method == "append"){
            if (!args.empty()) list->push_back(args[0]);
            return makeNone();
        }
        if (method == "extend"){
            if (!args.empty()){
                std::vector<value> extra;
                materializeIterable(args[0], extra);
                for (auto& item : extra) list->push_back(item);
            }
            return makeNone();
        }
        if (method == "pop"){
            if (list->empty()){
                raiseError("IndexError", "pop from empty list");
                return makeNone();
            }
            long long index = (args.empty()) ? static_cast<long long>(list->size()) - 1
                                             : toInt(args[0]);
            if (index < 0) index += static_cast<long long>(list->size());
            if (index < 0 || index >= static_cast<long long>(list->size())){
                raiseError("IndexError", "pop index out of range");
                return makeNone();
            }
            value value = (*list)[static_cast<size_t>(index)];
            list->erase(list->begin() + index);
            return value;
        }
        if (method == "insert"){
            if (args.size() >= 2){
                long long index = toInt(args[0]);
                if (index < 0) index = 0;
                if (index > static_cast<long long>(list->size())){
                    index = static_cast<long long>(list->size());
                }
                list->insert(list->begin() + index, args[1]);
            }
            return makeNone();
        }
        if (method == "index"){
            if (args.empty()) return makeInt(-1);
            for (size_t i = 0; i < list->size(); ++i){
                if (valuesEqual(args[0], (*list)[i])) return makeInt(static_cast<long long>(i));
            }
            raiseError("ValueError", "list.index(x): x not in list");
            return makeNone();
        }
        if (method == "count"){
            if (args.empty()) return makeInt(0);
            long long count = 0;
            for (auto& item : *list){
                if (valuesEqual(args[0], item)) count++;
            }
            return makeInt(count);
        }
        if (method == "remove"){
            if (!args.empty()){
                bool removed = false;
                for (size_t i = 0; i < list->size(); ++i){
                    if (valuesEqual(args[0], (*list)[i])){
                        list->erase(list->begin() + i);
                        removed = true;
                        break;
                    }
                }
                if (!removed){
                    raiseError("ValueError", "list.remove(x): x not in list");
                    return makeNone();
                }
            }
            return makeNone();
        }
        if (method == "reverse"){
            std::reverse(list->begin(), list->end());
            return makeNone();
        }
        if (method == "sort"){
            value keyFunc;
            if (!args.empty()) keyFunc = args[0];
            auto keyIt = kwargs.find("key");
            if (keyIt != kwargs.end()) keyFunc = keyIt->second;
            bool reverse = false;
            auto revIt = kwargs.find("reverse");
            if (revIt != kwargs.end()) reverse = truthy(revIt->second);
            std::vector<value> keyed(list->size());
            for (size_t i = 0; i < list->size(); ++i){
                keyed[i] = (keyFunc.type == "none" || keyFunc.type.empty() || !keyFunc.ptr)
                               ? (*list)[i]

// 调用 值
// call value
// ᠢᠳᠡᠬᠡᠯᠳᠡ ᠭᠠᠷᠴᠠᠭ
                               : callValue(keyFunc, {(*list)[i]});
                if (failed) return makeNone();
            }
            std::vector<size_t> order(list->size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            auto rawLess = [this](const value& a, const value& b){
                if (a.type == "str" && b.type == "str") return toString(a) < toString(b);
                if ((a.type == "int" || a.type == "float" || a.type == "bool") &&
                    (b.type == "int" || b.type == "float" || b.type == "bool"))
                    return toFloat(a) < toFloat(b);
                if (a.type == "list" || a.type == "tuple"){
                    auto* la = static_cast<std::vector<value>*>(a.ptr);
                    auto* lb = static_cast<std::vector<value>*>(b.ptr);
                    for (size_t k = 0; k < la->size() && k < lb->size(); ++k){
                        if (valuesEqual((*la)[k], (*lb)[k])) continue;
                        if ((*la)[k].type == "str" && (*lb)[k].type == "str")
                            return toString((*la)[k]) < toString((*lb)[k]);
                        return toFloat((*la)[k]) < toFloat((*lb)[k]);
                    }
                    return la->size() < lb->size();
                }
                return toFloat(a) < toFloat(b);
            };
            auto less = [&keyed, &rawLess, reverse](size_t ia, size_t ib){
                return reverse ? rawLess(keyed[ib], keyed[ia]) : rawLess(keyed[ia], keyed[ib]);
            };
            std::stable_sort(order.begin(), order.end(), less);
            std::vector<value> sorted;
            sorted.reserve(list->size());
            for (size_t idx : order) sorted.push_back((*list)[idx]);
            *list = std::move(sorted);
            return makeNone();
        }
        if (method == "clear"){
            list->clear();
            return makeNone();
        }
        if (method == "copy"){
            return makeList(*list);
        }

        if (method == "__init__"){
            fillContainer(*this, nullptr, list, args, kwargs);
            return makeNone();
        }
        if (method == "__len__"){
            return makeInt(static_cast<long long>(list->size()));
        }
        if (method == "__contains__"){
            if (args.empty()) return makeBool(false);
            for (auto& item : *list){
                if (valuesEqual(args[0], item)) return makeBool(true);
            }
            return makeBool(false);
        }
        if (method == "__getitem__"){
            if (args.empty()) return makeNone();
            long long index = toInt(args[0]);
            if (index < 0) index += static_cast<long long>(list->size());
            if (index < 0 || index >= static_cast<long long>(list->size())){
                raiseError("IndexError", "list index out of range");
                return makeNone();
            }
            return (*list)[static_cast<size_t>(index)];
        }
        if (method == "__setitem__"){
            if (args.size() < 2) return makeNone();
            long long index = toInt(args[0]);
            if (index < 0) index += static_cast<long long>(list->size());
            if (index < 0 || index >= static_cast<long long>(list->size())){
                raiseError("IndexError", "list assignment index out of range");
                return makeNone();
            }
            (*list)[static_cast<size_t>(index)] = args[1];
            return makeNone();
        }
        if (method == "__delitem__"){
            if (args.empty()) return makeNone();
            long long index = toInt(args[0]);
            if (index < 0) index += static_cast<long long>(list->size());
            if (index < 0 || index >= static_cast<long long>(list->size())){
                raiseError("IndexError", "list assignment index out of range");
                return makeNone();
            }
            list->erase(list->begin() + index);
            return makeNone();
        }
    }
    if (target.type == "tuple" && target.ptr){
        auto* list = static_cast<std::vector<value>*>(target.ptr);
        if (method == "count"){
            if (args.empty()) return makeInt(0);
            long long count = 0;
            for (auto& item : *list){
                if (valuesEqual(args[0], item)) count++;
            }
            return makeInt(count);
        }
        if (method == "index"){
            if (args.empty()) return makeInt(-1);
            for (size_t i = 0; i < list->size(); ++i){
                if (valuesEqual(args[0], (*list)[i])) return makeInt(static_cast<long long>(i));
            }
            raiseError("ValueError", "tuple.index(x): x not in tuple");
            return makeNone();
        }
    }
    if (auto* items = dictBacking(target)){

        value alias = target;
        if (target.type == "object") alias = value("dict", items, this);
        if (method == "fromkeys"){
            std::vector<std::pair<value, value>> entries;
            if (!args.empty()){
                std::vector<value> keys;
                materializeIterable(args[0], keys);
                if (failed) return makeNone();
                value value = (args.size() >= 2) ? args[1] : makeNone();
                for (auto& key : keys){
                    bool exists = false;
                    for (auto& entry : entries)
                        if (valuesEqual(entry.first, key)){ exists = true; break; }
                    if (!exists) entries.push_back({key, value});
                }
            }
            return makeDict(entries);
        }
        if (method == "keys"){
            return makeDictView(alias, 0);
        }
        if (method == "values"){
            return makeDictView(alias, 1);
        }
        if (method == "items"){
            return makeDictView(alias, 2);
        }
        if (method == "get"){
            if (args.empty()){
                raiseError("TypeError", "get expected at least 1 argument, got 0");
                return makeNone();
            }
            if (!hashable(args[0])){
                raiseError("TypeError", "cannot use '" + pythonTypeName(args[0]) +
                                        "' as a dict key (unhashable type: '" +
                                        pythonTypeName(args[0]) + "')");
                return makeNone();
            }
            value* found = dictFind(target, args[0]);
            if (found) return *found;
            return args.size() > 1 ? args[1] : makeNone();
        }
        if (method == "setdefault"){
            if (args.empty()){
                raiseError("TypeError", "setdefault expected at least 1 argument, got 0");
                return makeNone();
            }
            if (!hashable(args[0])){
                raiseError("TypeError", "cannot use '" + pythonTypeName(args[0]) +
                                        "' as a dict key (unhashable type: '" +
                                        pythonTypeName(args[0]) + "')");
                return makeNone();
            }
            value* found = dictFind(target, args[0]);
            if (found) return *found;
            value defaultValue = args.size() > 1 ? args[1] : makeNone();
            std::string error = dictSet(target, args[0], defaultValue);
            if (!error.empty()){
                raiseError("TypeError", error);
                return makeNone();
            }
            return defaultValue;
        }
        if (method == "pop"){
            if (args.empty()){
                raiseError("TypeError", "pop expected at least 1 argument, got 0");
                return makeNone();
            }
            value* found = dictFind(target, args[0]);
            if (found){
                value value = *found;
                dictRemove(target, args[0]);
                return value;
            }
            if (args.size() > 1) return args[1];
            raiseError("KeyError", pythonRepr(*this, args[0]));
            return makeNone();
        }
        if (method == "popitem"){
            if (items->empty()){
                raiseError("KeyError", "'popitem(): dictionary is empty'");
                return makeNone();
            }
            auto last = items->back();
            items->pop_back();

// 构造 元组
// make tuple
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠬᠣᠶᠠᠳᠠᠯ ᠲᠠᠯᠠᠭ᠌
            return makeTuple({last.first, last.second});
        }
        if (method == "update"){

            for (auto& entry : kwargs){
                std::string error = dictSet(target, makeString(entry.first), entry.second);
                if (!error.empty()){
                    raiseError("TypeError", error);
                    return makeNone();
                }
            }
            if (args.empty()) return makeNone();
            if (dict_t* source = dictBacking(args[0])){
                for (auto& item : *source){
                    std::string error = dictSet(target, item.first, item.second);
                    if (!error.empty()){
                        raiseError("TypeError", error);
                        return makeNone();
                    }
                }
                return makeNone();
            }
            std::vector<value> pairs;
            materializeIterable(args[0], pairs);
            for (size_t i = 0; i < pairs.size(); ++i){
                std::vector<value>* entry = nullptr;
                if ((pairs[i].type == "list" || pairs[i].type == "tuple") && pairs[i].ptr){
                    entry = static_cast<std::vector<value>*>(pairs[i].ptr);
                }
                if (entry == nullptr){
                    raiseError("TypeError", "cannot convert dictionary update sequence element #" +
                                            std::to_string(i) + " to a sequence");
                    return makeNone();
                }
                if (entry->size() != 2){
                    raiseError("ValueError", "dictionary update sequence element #" +
                                             std::to_string(i) + " has length " +
                                             std::to_string(entry->size()) + "; 2 is required");
                    return makeNone();
                }
                std::string error = dictSet(target, (*entry)[0], (*entry)[1]);
                if (!error.empty()){
                    raiseError("TypeError", error);
                    return makeNone();
                }
            }
            return makeNone();
        }
        if (method == "clear"){
            items->clear();
            return makeNone();
        }
        if (method == "copy"){
            return makeDict(*items);
        }

        if (method == "__init__"){
            fillContainer(*this, items, nullptr, args, kwargs);
            return makeNone();
        }
        if (method == "__len__"){
            return makeInt(static_cast<long long>(items->size()));
        }
        if (method == "__contains__"){
            if (args.empty()) return makeBool(false);
            if (!hashable(args[0])) return makeBool(false);
            return makeBool(dictFind(target, args[0]) != nullptr);
        }
        if (method == "__getitem__"){
            if (args.empty()) return makeNone();
            value* found = dictFind(target, args[0]);
            if (found) return *found;
            raiseError("KeyError", pythonRepr(*this, args[0]));
            return makeNone();
        }
        if (method == "__setitem__"){
            if (args.size() < 2) return makeNone();
            std::string error = dictSet(target, args[0], args[1]);
            if (!error.empty()) raiseError("TypeError", error);
            return makeNone();
        }
        if (method == "__delitem__"){
            if (args.empty()) return makeNone();
            if (!dictRemove(target, args[0])){
                raiseError("KeyError", pythonRepr(*this, args[0]));
            }
            return makeNone();
        }
    }
    if (target.type == "int" || target.type == "bool"){
        long long value = toInt(target);
        if (method == "bit_length"){
            unsigned long long magnitude = value < 0 ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                                                     : static_cast<unsigned long long>(value);
            long long bits = 0;
            while (magnitude){ bits++; magnitude >>= 1; }
            return makeInt(bits);
        }
        if (method == "conjugate" || method == "__int__" || method == "__index__" ||
            method == "__pos__"){
            return makeInt(value);
        }
        if (method == "__abs__") return makeInt(value < 0 ? -value : value);
        if (method == "__neg__") return makeInt(-value);
        if (method == "__bool__") return makeBool(value != 0);
        if (method == "__float__") return makeFloat(static_cast<double>(value));
        if (method == "__hash__") return makeInt(static_cast<long long>(
            std::hash<long long>{}(value)));
    }
    if (target.type == "float" && target.ptr){
        double value = toFloat(target);
        if (method == "conjugate" || method == "__float__") return makeFloat(value);
        if (method == "__abs__") return makeFloat(std::fabs(value));
        if (method == "__neg__") return makeFloat(-value);
        if (method == "__bool__") return makeBool(value != 0.0);
        if (method == "is_integer") return makeBool(std::isfinite(value) && std::floor(value) == value);
        if (method == "__hash__") return makeInt(static_cast<long long>(std::hash<double>{}(value)));
    }
    handled = false;
    return makeNone();
}


void vm::materializeIterable(const value& v, std::vector<value>& items){
    if (v.ptr == nullptr) return;
    if (v.type == "class"){
        auto* klass = static_cast<class_t*>(v.ptr);
        if (klass->builtinBase.rfind("enum:", 0) == 0 && klass->attributes){
            auto* attrs = static_cast<std::map<std::string, value>*>(klass->attributes);
            auto it = attrs->find("_member_names_");
            if (it != attrs->end() && it->second.type == "list" && it->second.ptr){
                for (auto& nameVal : *static_cast<std::vector<value>*>(it->second.ptr)){
                    if (nameVal.type != "str") continue;
                    auto mit = attrs->find(*static_cast<std::string*>(nameVal.ptr));
                    if (mit != attrs->end() && mit->second.type == "native_object")
                        items.push_back(mit->second);
                }
            }
        }
        return;
    }
    if (v.type == "list" || v.type == "tuple"){
        items = *static_cast<std::vector<value>*>(v.ptr);
        return;
    }
    if (v.type == "range"){
        auto* range = static_cast<range_t*>(v.ptr);
        long long count = range->count();
        for (long long i = 0; i < count; ++i) items.push_back(makeInt(range->at(i)));
        return;
    }
    if (v.type == "dict_keys" || v.type == "dict_values" ||
        v.type == "dict_items"){
        auto* view = static_cast<dict_view*>(v.ptr);
        auto* entries = static_cast<dict_t*>(view->dict.ptr);
        for (auto& entry : *entries){
            if (view->kind == 0) items.push_back(entry.first);
            else if (view->kind == 1) items.push_back(entry.second);
            else items.push_back(makeTuple({entry.first, entry.second}));
        }
        return;
    }
    if (v.type == "dict"){
        for (auto& item : *static_cast<dict_t*>(v.ptr)) items.push_back(item.first);
        return;
    }

    if (v.type == "object" && v.ptr){
        auto* inst = static_cast<instance*>(v.ptr);
        if (inst->container.type == "dict"){
            for (auto& item : *static_cast<dict_t*>(inst->container.ptr))
                items.push_back(item.first);
            return;
        }
        if (inst->container.type == "list"){
            items = *static_cast<std::vector<value>*>(inst->container.ptr);
            return;
        }
    }
    if (v.type == "str"){
        for (char c : toString(v)) items.push_back(makeString(std::string(1, c)));
        return;
    }
    if (v.type == "bytes" || v.type == "bytearray"){
        auto* raw = static_cast<std::string*>(v.ptr);
        for (unsigned char ch : *raw)
            items.push_back(makeInt(static_cast<long long>(ch)));
        return;
    }
    if (v.type == "int"){
        long long count = toInt(v);
        for (long long i = 0; i < count; ++i) items.push_back(makeInt(i));
        return;
    }

    if (v.type == "native_object" && v.ptr){
        auto* inst = static_cast<ffi_instance*>(v.ptr);
        if (inst && (inst->typeName == "generator" || inst->typeName == "generator_method")){
            DelegState tmp;
            tmp.mode = 1;
            tmp.sub = v;
            value item;
            while (true){
                int r = delegNextValue(tmp, 0, makeNone(), item);
                if (r == DL_YIELD){
                    items.push_back(item);
                    continue;
                }

                return;
            }
        }
        if (inst && inst->klass &&
            inst->klass->methods.find("__iter__") != inst->klass->methods.end() &&
            inst->klass->methods.find("__next__") == inst->klass->methods.end()){
            value itResult;
            if (cffiCallMethod(*this, v, "__iter__", {}, itResult) && !hasError()){
                materializeIterable(itResult, items);
            }
            if (hasError()) clearPendingExceptionState();
            return;
        }
        if (inst && inst->klass &&
            inst->klass->methods.find("__next__") != inst->klass->methods.end()){
            value iterObj = v;
            if (inst->klass->methods.find("__iter__") != inst->klass->methods.end()){
                value itResult;
                if (cffiCallMethod(*this, v, "__iter__", {}, itResult) &&
                    !hasError() && itResult.type == "native_object" && itResult.ptr)
                    iterObj = itResult;
                if (hasError()) clearPendingExceptionState();
            }
            while (true){
                value item;
                if (!cffiCallMethod(*this, iterObj, "__next__", {}, item)) break;
                if (hasError()){
                    if (exceptionType() == "StopIteration"){
                        clearPendingExceptionState();
                        return;
                    }
                    return;
                }
                items.push_back(item);
            }
            return;
        }
    }

    class_t* klass = nullptr;
    if (v.type == "object" && v.ptr){
        klass = static_cast<instance*>(v.ptr)->klass;
    } else if (v.type == "class" && v.ptr){
        klass = static_cast<class_t*>(v.ptr);
    }
    if (klass){
        function_t* iterMethod = findMethod(klass, "__iter__");
        if (iterMethod){
            value selfCopy = v;
            value iterObj = invoke(iterMethod, {}, &selfCopy);
            if (hasError()) return;

            function_t* nextMethod = nullptr;
            if (iterObj.type == "object" && iterObj.ptr){
                nextMethod = findMethod(static_cast<instance*>(iterObj.ptr)->klass, "__next__");
            }
            if (!nextMethod){

                if (v.type == "object" && v.ptr){
                    nextMethod = findMethod(static_cast<instance*>(v.ptr)->klass, "__next__");
                }
            }
            if (nextMethod){
                while (true){
                    value nextSelf = (iterObj.type == "object" && iterObj.ptr) ? iterObj : v;
                    value r = invoke(nextMethod, {}, &nextSelf);
                    if (hasError()){
                        if (exceptionType() == "StopIteration"){
                            clearPendingExceptionState();
                            return;
                        }
                        return;
                    }
                    items.push_back(r);
                }
            }
        }
    }
}


value vm::invokeAny(value callee, std::vector<value> args){
    if (failed) return makeNone();
    const script_t* activeScript = currentScript ? currentScript : &script;
    if (callee.type == "function" && callee.ptr){
        auto* func = static_cast<function_t*>(callee.ptr);
        return invoke(func, args, nullptr, activeScript, func->name);
    }
    if (callee.type == "native_method" && callee.ptr){

        auto* bound = static_cast<native_bound_method*>(callee.ptr);
        value result = makeNone();

        if (bound->self.type == "dict" || bound->self.type == "list"){
            bool handled = false;
            value selfCopy = bound->self;
            value methodResult = callBuiltinMethod(bound->methodName, selfCopy, args, handled);
            if (handled) return methodResult;
        }
        if (bound->self.type == "object" && bound->self.ptr){
            auto* inst = static_cast<instance*>(bound->self.ptr);
            if (inst->container.type == "dict" || inst->container.type == "list"){
                bool handled = false;
                value methodResult = callBuiltinMethod(bound->methodName, bound->self, args, handled);
                if (handled) return methodResult;
            }
        }
        cffiCallMethod(*this, bound->self, bound->methodName, args, result);
        return result;
    }
    if (callee.type == "bound_function" && callee.ptr){
        auto* bf = static_cast<bound_function_object*>(callee.ptr);
        if (!bf->fn) return makeNone();
        value selfCopy = bf->self;
        return invoke(bf->fn, args, &selfCopy);
    }
    if (callee.type == "native_object" && callee.ptr){
        auto* inst = static_cast<ffi_instance*>(callee.ptr);

        if (inst->typeName == "closure"){
            auto* ch = static_cast<closure_handle*>(inst->handle);
            if (!ch || !ch->fn){
                raiseError("RuntimeError", "closure handle invalid");
                return makeNone();
            }
            size_t capIdx = scopes.size();
            scopes.push_back(ch->captures);
            captureScopeIndices.push_back(capIdx);
            value result = invoke(ch->fn, args, nullptr, activeScript, ch->fn->name);
            captureScopeIndices.pop_back();
            ch->captures = scopes.back();
            scopes.pop_back();
            return result;
        }

        if (!inst->klass && inst->handle && inst->library){
            auto fn = reinterpret_cast<cffi_native_function>(inst->handle);
            cffi callCtx;
            callCtx.vm = this;
            callCtx.library = inst->library;
            callCtx.argv = args;
            // 记录被调用的槽位原值, 供 C-API 的共用入口精确反查回调
            // Remember the raw slot value so the C-API bridge can resolve the callback
            callCtx.nativeEntry = inst->handle;
            value result = fn(callCtx);
            return result;
        }


        if (inst->klass && inst->library){
            value instance = makeNone();
            cffiCallConstructor(*inst->library, *inst->klass, *this, args, instance);
            return instance;
        }
        if (inst->dynamicCall){
            value result = makeNone();
            inst->dynamicCall(*this, *inst, args, {}, result);
            return result;
        }
    }
    if (callee.type == "object" && callee.ptr){
        auto* inst = static_cast<instance*>(callee.ptr);
        function_t* call = findMethod(inst->klass, "__call__");
        if (call){
            value selfCopy = callee;
            return invoke(call, args, &selfCopy);
        }
    }
    if (callee.type == "class" && callee.ptr){
        auto* klass = static_cast<class_t*>(callee.ptr);

        if (klass->builtinBase.rfind("enum:", 0) == 0){
            return enumClassCall(*this, *klass, args);
        }

        value instan;

        function_t* newMethod = findMethod(klass, "__new__");
        if (newMethod){

            value clsSelf = callee;
            std::vector<value> newArgs = {callee};
            newArgs.insert(newArgs.end(), args.begin(), args.end());
            instan = invoke(newMethod, newArgs, &clsSelf, &script, klass->name + ".__new__");
        } else {

            instan = makeObject(klass);
        }
        if (hasError()) return makeNone();

        bool isOurInstance = false;
        if (instan.type == "object" && instan.ptr){
            auto* inst = static_cast<struct instance*>(instan.ptr);
            if (inst->klass == klass){
                isOurInstance = true;
            } else if (inst->klass){


                isOurInstance = true;
            }
        }

        if (isOurInstance){
            function_t* init = findMethod(klass, "__init__");

            if (!init && instan.type == "object" && instan.ptr){
                auto* objInst = static_cast<struct instance*>(instan.ptr);
                if (objInst->container.type == "dict"){
                    fillContainer(*this, static_cast<dict_t*>(objInst->container.ptr),
                                  nullptr, args, {});
                    if (hasError()) return makeNone();
                } else if (objInst->container.type == "list"){
                    fillContainer(*this, nullptr,
                                  static_cast<std::vector<value>*>(objInst->container.ptr),
                                  args, {});
                    if (hasError()) return makeNone();
                }
            }
            if (init){
                value selfCopy = instan;
                invoke(init, args, &selfCopy, &script, klass->name + ".__init__");
            }
        }
        return instan;
    }
    raiseError("TypeError", "'" + pythonTypeName(callee) + "' object is not callable");
    return makeNone();
}

value vm::callFunc(std::string funcName, std::vector<value> args,

                            const std::map<std::string, value>& kwargs){
    if (failed) return makeNone();

    // 被调函数/类所属的脚本 = 当前正在执行的脚本 (模块函数调用时不能退回主脚本)
    // The callee's owning script is the currently executing one; do not fall
    // back to the main script when we are inside an imported module.
    const script_t* callScript = currentScript ? currentScript : &script;

    bool isSimpleName = !funcName.empty() && funcName[0] != '\0' &&
        (std::isalpha(static_cast<unsigned char>(funcName[0])) || funcName[0] == '_');
    bool hasDot = funcName.find('.') != std::string::npos;
    if (isSimpleName && !hasDot){
        for (char c : funcName){
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')){
                isSimpleName = false;
                break;
            }
        }
    }
    if (!isSimpleName && !hasDot){

        value calleeValue = evalExpr(funcName);
        if (failed) return makeNone();
        return invokeAny(calleeValue, args);
    }


    if (funcName == "__delitem__"){
        value container = args.empty() ? makeNone() : args[0];
        value key = args.size() > 1 ? args[1] : makeNone();
        if (container.type == "dict" && container.ptr){
            if (!hashable(key)){
                raiseError("TypeError", "cannot use '" + pythonTypeName(key) +
                                        "' as a dict key (unhashable type: '" +
                                        pythonTypeName(key) + "')");
                return makeNone();
            }
            if (!dictRemove(container, key)){
                raiseError("KeyError", pythonRepr(*this, key));
                return makeNone();
            }
            return makeNone();
        }
        if (container.type == "native_object" && container.ptr){
            auto* ninst = static_cast<ffi_instance*>(container.ptr);
            if (ninst->klass && ninst->klass->methods.count("__delitem__")){
                value r;
                cffiCallMethod(*this, container, "__delitem__", {key}, r);
                return makeNone();
            }
            raiseError("TypeError", "'" + pythonTypeName(container) +
                                    "' object doesn't support item deletion");
            return makeNone();
        }
        if (container.type == "list" && container.ptr){
            auto* list = static_cast<std::vector<value>*>(container.ptr);
            long long index = toInt(key);
            if (index < 0) index += static_cast<long long>(list->size());
            if (index < 0 || index >= static_cast<long long>(list->size())){
                raiseError("IndexError", "list assignment index out of range");
                return makeNone();
            }
            list->erase(list->begin() + index);
            return makeNone();
        }
        raiseError("TypeError", "'" + pythonTypeName(container) +
                                "' object doesn't support item deletion");
        return makeNone();
    }

    auto setCalleeCaret = [this](){
        if (lastCalleeSpan.valid()){
            pendingCaret = lastCalleeSpan;
            pendingCaret.anchorStart = lastCalleeSpan.startCol;
            pendingCaret.anchorEnd = lastCalleeSpan.endCol;
        }
    };

    if (funcName == "len" && args.size() == 1){
        value& arg = args[0];
        if (arg.type == "object" && arg.ptr){
            auto* inst = static_cast<instance*>(arg.ptr);
            function_t* len = findMethod(inst->klass, "__len__");
            if (len){
                value selfCopy = arg;
                value r = invoke(len, {}, &selfCopy);
                if (!hasError()) return r;
                clearPendingExceptionState();
            }
        }
    }
    bool builtinHandled = false;
    value builtinResult = callBuiltin(funcName, args, builtinHandled, kwargs);
    if (builtinHandled) return builtinResult;

    size_t dot = funcName.rfind('.');
    if (dot != std::string::npos){
        std::string baseName = funcName.substr(0, dot);
        std::string member = funcName.substr(dot + 1);
        const script_t* library = findScriptLibrary(baseName);
        if (library){
            for (auto& function : library->functions){
                if (function.name == member){
                    return invoke(const_cast<function_t*>(&function), args, nullptr, library,
                                  function.name, kwargs);
                }
            }
            for (auto& klass : library->classes){
                if (klass.name == member){
                    value instance;
                    if (klass.baseName.find("Structure") != std::string::npos){
                        auto* info = new CtypesStructInfo();
                        info->totalSize = 4096;
                        info->align = 8;
                        info->buffer.assign(4096, 0);
                        info->buffer.reserve(8192);
                        instance = makeDynamicObject("Structure", info, ctypesStructGet, nullptr);
                        static_cast<ffi_instance*>(instance.ptr)->typeName = "Structure";
                        return instance;
                    }
                    instance = makeObject(const_cast<class_t*>(&klass));
                    function_t* init = findMethod(const_cast<class_t*>(&klass), "__init__");
                    if (init){
                        invoke(init, args, &instance, library, klass.name + ".__init__", kwargs);
                    } else if (!args.empty()){
                        raiseError("TypeError", klass.name + "() takes no arguments");
                    }
                    return instance;
                }
            }
            setCalleeCaret();
            raiseError("AttributeError", "module '" + baseName + "' has no attribute '" + member + "'");
            return makeNone();
        }

        for (auto* cffiLibrary : cffiLibraries){
            if (!cffiLibrary || cffiLibrary->name != baseName) continue;
            value result = makeNone();
            if (cffiCallFunction(*cffiLibrary, member, *this, args, result, kwargs)) return result;
            auto classIt = cffiLibrary->classes.find(member);
            if (classIt != cffiLibrary->classes.end()){
                value instance = makeNone();
                cffiCallConstructor(*cffiLibrary, classIt->second, *this, args, instance, kwargs);
                return instance;
            }
            setCalleeCaret();
            raiseError("AttributeError", "module '" + baseName + "' has no attribute '" + member + "'");
            return makeNone();
        }

        value target = evalExpr(baseName);
        if (failed) return makeNone();
        if (target.type == "super" && target.ptr){
            value memberValue = resolveSuperMember(*this, static_cast<super_object*>(target.ptr), member);
            if (hasError()) return makeNone();
            if (memberValue.type == "native_method" && memberValue.ptr){
                auto* bound = static_cast<native_bound_method*>(memberValue.ptr);
                bool handled = false;
                if (bound->self.type == "object"){
                    value r = callBuiltinMethod(bound->methodName, bound->self, args, handled, kwargs);
                    if (handled) return r;
                }
                value result = makeNone();
                cffiCallMethod(*this, bound->self, bound->methodName, args, result, kwargs);
                return result;
            }
            if (memberValue.type == "bound_function" && memberValue.ptr){
                auto* bf = static_cast<bound_function_object*>(memberValue.ptr);
                value selfCopy = bf->self;
                return invoke(bf->fn, args, &selfCopy, &script, member, kwargs);
            }
            return invokeAny(memberValue, args);
        }
        if (target.type == "native_object" && target.ptr){
            auto* instance = static_cast<ffi_instance*>(target.ptr);

            if (member == "fromkeys" && instance->typeName == "dict"){
                if (args.empty()){
                    raiseError("TypeError", "fromkeys() takes at least 1 argument (0 given)");
                    return makeNone();
                }
                std::vector<value> keys;
                materializeIterable(args[0], keys);
                if (failed) return makeNone();
                value fill = (args.size() >= 2) ? args[1] : makeNone();
                auto* result = new dict_t();
                for (auto& key : keys){
                    bool exists = false;
                    for (auto& entry : *result)
                        if (valuesEqual(entry.first, key)){ exists = true; break; }
                    if (!exists) result->push_back({key, fill});
                }
                return value("dict", keep(vk_dict, result), this);
            }
            if (!instance->klass && instance->library){
                value memberValue = getAttribute(target, member);
                if (failed) return makeNone();
                return invokeAny(memberValue, args);
            }
            value result = makeNone();
            if (cffiCallMethod(*this, target, member, args, result, kwargs)) return result;

            if (instance->dynamicGet){
                value memberValue = makeNone();
                if (instance->dynamicGet(*this, instance->handle, member, memberValue) &&
                    !failed && memberValue.type == "native_object" && memberValue.ptr){
                    auto* callable = static_cast<ffi_instance*>(memberValue.ptr);
                    if (callable->dynamicCall){
                        value dynamicResult = makeNone();
                        if (callable->dynamicCall(*this, *callable, args, kwargs, dynamicResult)){
                            return dynamicResult;
                        }
                        return makeNone();
                    }
                }
            }
            if (failed) return makeNone();
            std::string typeName = !instance->typeName.empty() ? instance->typeName
                                        : (instance->klass ? instance->klass->name : "object");
            setCalleeCaret();
            raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + member + "'");
            return makeNone();
        }
        if (target.type == "object" && target.ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
            struct instance* instance = static_cast<struct instance*>(target.ptr);

            value* clsAttr = instance->klass ?
                findClassAttrInChain(instance->klass, member) : nullptr;
            if (clsAttr && valueHasDescriptorMethod(*clsAttr, "__get__")){
                value owner("class", instance->klass);
                value descResult = invokeDescriptorMethod(*this, *clsAttr, "__get__",
                                                          {target, owner});
                if (hasError()) return makeNone();
                return invokeAny(descResult, args);
            }
            function_t* method = findMethod(instance->klass, member);
            if (method){



                if (!method->decorators.empty()){

                    value wrappedMethod;
                    wrappedMethod.type = "function";
                    wrappedMethod.ptr = method;

                    value current = wrappedMethod;
                    for (auto it = method->decorators.rbegin(); it != method->decorators.rend(); ++it){
                        const std::string& decExpr = *it;
                        value decorator = evalExpr(decExpr);
                        if (failed) return makeNone();

                        current = invokeAny(decorator, {current});
                        if (hasError()) return makeNone();
                    }



                    if (current.type == "function" && current.ptr){
                        auto* decMethod = static_cast<function_t*>(current.ptr);
                        return invoke(decMethod, args, &target, &script,
                                      instance->klass->name + "." + decMethod->name, kwargs);
                    }

                    return invokeAny(current, args);
                }
                std::string typeName = instance->klass ? instance->klass->name : "object";
                return invoke(method, args, &target, &script, typeName + "." + method->name, kwargs);
            }

            if (instance->container.type == "dict" || instance->container.type == "list"){
                bool methodHandled = false;
                value methodResult = callBuiltinMethod(member, target, args, methodHandled, kwargs);
                if (methodHandled) return methodResult;
            }
            std::string typeName = instance->klass ? instance->klass->name : "object";
            setCalleeCaret();
            raiseError("AttributeError", "'" + typeName + "' object has no attribute '" + member + "'");
            return makeNone();
        }
        if (target.type == "class" && target.ptr){
            auto* klass = static_cast<class_t*>(target.ptr);
            auto* klassAttrs = klass->attributes ?
                static_cast<std::map<std::string, value>*>(klass->attributes) : nullptr;
            bool resolved = false;

            if (klassAttrs){
                auto it = klassAttrs->find(member);
                if (it != klassAttrs->end()){
                    const value& clsVal = it->second;

                    bool hasSetOrDel = valueHasDescriptorMethod(clsVal, "__set__") ||
                                       valueHasDescriptorMethod(clsVal, "__delete__");
                    bool hasGet = valueHasDescriptorMethod(clsVal, "__get__");
                    if (hasSetOrDel && hasGet){

                        value noneInst = makeNone();
                        value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                                  {noneInst, target});
                        if (!hasError()){
                            return invokeAny(descResult, args);
                        } else {
                            return makeNone();
                        }
                    } else if (hasGet){



                        value noneInst = makeNone();
                        value descResult = invokeDescriptorMethod(*this, clsVal, "__get__",
                                                                  {noneInst, target});
                        if (!hasError()){
                            return invokeAny(descResult, args);
                        } else {
                            return makeNone();
                        }
                    } else {

                        return invokeAny(clsVal, args);
                    }
                }
            }

            function_t* method = findMethod(klass, member);
            if (method){
                if (!method->decorators.empty()){

                    value wrappedMethod;
                    wrappedMethod.type = "function";
                    wrappedMethod.ptr = method;
                    value current = wrappedMethod;
                    for (auto it2 = method->decorators.rbegin(); it2 != method->decorators.rend(); ++it2){
                        const std::string& decExpr = *it2;
                        value decorator = evalExpr(decExpr);
                        if (failed) return makeNone();
                        current = invokeAny(decorator, {current});
                        if (hasError()) return makeNone();
                    }

                    return invokeAny(current, args);
                }

                return invoke(method, args, nullptr, &script,
                              klass->name + "." + method->name, kwargs);
            }

            setCalleeCaret();
            raiseError("AttributeError", "'" + klass->name + "' object has no attribute '" + member + "'");
            return makeNone();
        }
        bool methodHandled = false;
        value methodResult = callBuiltinMethod(member, target, args, methodHandled, kwargs);
        if (methodHandled) return methodResult;
        setCalleeCaret();
        raiseError("AttributeError", "'" + pythonTypeName(target) +
                   "' object has no attribute '" + member + "'");
        return makeNone();
    }

    for (auto* cffiLibrary : cffiLibraries){
        if (!cffiLibrary) continue;
        auto classIt = cffiLibrary->classes.find(funcName);
        if (classIt != cffiLibrary->classes.end()){
            value instance = makeNone();
            cffiCallConstructor(*cffiLibrary, classIt->second, *this, args, instance, kwargs);
            return instance;
        }
    }

    class_t* klass = findClass(funcName);
    if (klass){

        if (klass->builtinBase.rfind("enum:", 0) == 0){
            return enumClassCall(*this, *klass, args);
        }

        if (klass->baseName.find("Structure") != std::string::npos){
            auto* info = new CtypesStructInfo();
            info->totalSize = 4096;
            info->align = 8;
            info->buffer.assign(4096, 0);
            info->buffer.reserve(8192);

            static const char* ptrNames[] = {
                "lpfnWndProc","hInstance","hIcon","hCursor","hbrBackground",
                "lpszMenuName","lpszClassName","hwnd","hWnd","hDC","hBitmap",
                "lParam","wParam","lpParam","lpszText","lpData","lpBits","lpPrevWndFunc",
                "hParent","hprc","hScrDC","hMemDC","hBitmapOld","hRgn","hPalette",
                "hpalette","hBitmapOld","hOldObj",

                "HWND","HINSTANCE","HCURSOR","HICON","HBRUSH","HDC","HBITMAP",
                "HANDLE","HMODULE","HRGN","HPEN","HFONT","HMENU","HPALETTE","HKEY",
                "LPSTR","LPWSTR","LPCSTR","LPCWSTR","LPSYSTEMTIME","LPVOID","LPDWORD",
                "PVOID","PHANDLE","POINTER","BYREF","c_void_p","c_char_p","c_wchar_p",
                "WPARAM","LPARAM","LRESULT","WNDPROC","FARPROC","PROC",
                "PSID","PTOKEN_USER","PROCESS_INFORMATION*"
            };
            static const char* int4Names[] = {
                "style","cbClsExtra","cbWndExtra","message","msg","time",
                "iWidth","iHeight","nHeight","nWidth","wMaxWidth","wMaxHeight",
                "clrText","clrBackground","iMode","iPlanes","iPixelBits",
                "size","count","offset","access","share","create","inherit","timeout",
                "UINT","DWORD","ULONG","ULONG_PTR","unsigned int","unsigned long",
                "INT","LONG","int","long","c_int","c_uint","c_long","c_ulong",
                "USHORT","WORD","SHORT","c_short","c_ushort","UCHAR","CHAR","BYTE",
                "c_byte","c_ubyte","c_char","BOOL","BOOLEAN","ULONG_PTR","SIZE_T",
                "wintypes::DWORD","wintypes::UINT","wintypes::WORD","wintypes::ULONG",
                "UINT","UINT_PTR","ULONGLONG"
            };
            static const char* int8Names[] = {
                "long long","c_longlong","c_ulonglong","LONGLONG","ULONGLONG",
                "INT64","UINT64","LARGE_INTEGER"
            };
            auto sizeFromTypeName = [&](const std::string& tn) -> std::pair<size_t, size_t> {
                std::string n = tn;

                size_t dot = n.rfind('.');
                if (dot != std::string::npos) n = n.substr(dot + 1);
                for (auto* p : ptrNames) if (n == p) return {8, 8};
                for (auto* p : int4Names) if (n == p) return {4, 4};
                for (auto* p : int8Names) if (n == p) return {8, 8};

                return {8, 8};
            };
            if (!klass->fieldsText.empty()){
                value fieldsVal = evalExpr(klass->fieldsText);
                if (!failed && fieldsVal.type == "list" && fieldsVal.ptr){
                    auto* list = static_cast<std::vector<value>*>(fieldsVal.ptr);
                    size_t cursor = 0;
                    size_t maxAlign = 1;
                    for (auto& field : *list){
                        if ((field.type == "list" || field.type == "tuple") && field.ptr){
                            auto* tup = static_cast<std::vector<value>*>(field.ptr);
                            if (tup->size() >= 2 && (*tup)[0].type == "str"){
                                std::string fname = *static_cast<std::string*>((*tup)[0].ptr);
                                std::string tname;
                                value& tval = (*tup)[1];
                                if (tval.type == "str") tname = *static_cast<std::string*>(tval.ptr);
                                else if (tval.type == "native_object" && tval.ptr){
                                    auto* ti = static_cast<ffi_instance*>(tval.ptr);
                                    tname = !ti->typeName.empty() ? ti->typeName : "unknown";
                                }
                                auto [sz, al] = sizeFromTypeName(tname);
                                if (al == 0) al = 1;
                                size_t mis = cursor % al;
                                if (mis > 0) cursor += (al - mis);
                                CtypesFieldDesc fd;
                                fd.name = fname;
                                fd.offset = cursor;
                                fd.size = sz;
                                fd.align = al;
                                fd.isPointer = (sz == 8);
                                fd.typeName = tname;
                                info->fields.push_back(fd);
                                info->fieldNames.push_back(fname);
                                cursor += sz;
                                if (al > maxAlign) maxAlign = al;
                            }
                        }
                    }
                    size_t mis = cursor % maxAlign;
                    if (mis > 0) cursor += (maxAlign - mis);
                    info->totalSize = cursor;
                    info->align = maxAlign;
                    info->cursor = cursor;
                }
            }
            value instance = makeDynamicObject("Structure", info, ctypesStructGet, nullptr);
            static_cast<ffi_instance*>(instance.ptr)->typeName = "Structure";
            return instance;
        }
        value instance;

        function_t* newMethod = findMethod(klass, "__new__");
        if (newMethod){
            value clsSelf;
            clsSelf.type = "class";
            clsSelf.ptr = klass;
            std::vector<value> newArgs = {clsSelf};
            newArgs.insert(newArgs.end(), args.begin(), args.end());
            instance = invoke(newMethod, newArgs, &clsSelf, callScript, klass->name + ".__new__", kwargs);
        } else {

            instance = makeObject(klass);
        }
        if (!hasError() && instance.type == "object" && instance.ptr){
            auto* inst = static_cast<struct instance*>(instance.ptr);
            if (inst->klass == klass){
                function_t* init = findMethod(klass, "__init__");
                if (init){
                    invoke(init, args, &instance, callScript, klass->name + ".__init__", kwargs);
                } else if (inst->container.type == "dict"){
                    fillContainer(*this, static_cast<dict_t*>(inst->container.ptr),
                                  nullptr, args, kwargs);
                } else if (inst->container.type == "list"){
                    fillContainer(*this, nullptr,
                                  static_cast<std::vector<value>*>(inst->container.ptr),
                                  args, kwargs);
                } else if (!args.empty() || !kwargs.empty()){
                    raiseError("TypeError", klass->name + "() takes no arguments");
                }
            }
        }
        return instance;
    }

    if (funcName.find('.') == std::string::npos){
        value* variablePtr = findVariablePtr(funcName);
        if (variablePtr && variablePtr->type == "native_object" && variablePtr->ptr){
            auto* instance = static_cast<ffi_instance*>(variablePtr->ptr);

            if (instance->typeName == "closure"){
                auto* ch = static_cast<closure_handle*>(instance->handle);
                if (!ch || !ch->fn){
                    raiseError("RuntimeError", "closure handle invalid");
                    return makeNone();
                }
                size_t capIdx = scopes.size();
                scopes.push_back(ch->captures);
                captureScopeIndices.push_back(capIdx);
                value result = invoke(ch->fn, args, nullptr, callScript, ch->fn->name, kwargs);
                captureScopeIndices.pop_back();
                ch->captures = scopes.back();
                scopes.pop_back();
                return result;
            }

            if (!instance->klass && instance->handle && instance->library){
                auto fn = reinterpret_cast<cffi_native_function>(instance->handle);
                cffi callCtx;
                callCtx.vm = this;
                callCtx.library = instance->library;
                callCtx.argv = args;
                callCtx.kwargv = kwargs;
                callCtx.nativeEntry = instance->handle;
                return fn(callCtx);
            }
            if (instance->dynamicCall){
                value result = makeNone();
                instance->dynamicCall(*this, *instance, args, kwargs, result);
                return result;
            }
        }

        if (variablePtr && variablePtr->type == "object" && variablePtr->ptr){

// 实例
// instance
// ᠶᠢᠨᠢᠰ
            struct instance* instance = static_cast<struct instance*>(variablePtr->ptr);
            function_t* call = findMethod(instance->klass, "__call__");
            if (call){
                value selfCopy = *variablePtr;
                return invoke(call, args, &selfCopy);
            }
        }

        if (variablePtr && variablePtr->type == "function" && variablePtr->ptr){
            auto* cachedFunc = static_cast<function_t*>(variablePtr->ptr);
            return invoke(cachedFunc, args, nullptr, callScript, cachedFunc->name, kwargs);
        }

        if (variablePtr && variablePtr->type == "class" && variablePtr->ptr){
            auto* klass = static_cast<class_t*>(variablePtr->ptr);
            if (klass->builtinBase.rfind("enum:", 0) == 0){
                return enumClassCall(*this, *klass, args);
            }
            value instance;

            function_t* newMethod = findMethod(klass, "__new__");
            if (newMethod){
                value klassData;
                klassData.type = "class"; klassData.ptr = klass;
                value r = invoke(newMethod, args, nullptr, callScript, klass->name + ".__new__", kwargs);
                if (!hasError() && r.type == "object" && r.ptr){
                    instance = r;
                } else if (!hasError()){

                    return r;
                } else {
                    clearPendingExceptionState();
                    instance = makeObject(klass);
                }
            } else {
                instance = makeObject(klass);
            }

            if (instance.ptr){
                function_t* init = findMethod(klass, "__init__");
                if (init){
                    value selfCopy = instance;
                    invoke(init, args, &selfCopy, callScript, klass->name + ".__init__", kwargs);
                } else if (!args.empty()){
                    raiseError("TypeError", klass->name + "() takes no arguments");
                }
            }
            return instance;
        } else if (variablePtr && variablePtr->type == "native_object" && variablePtr->ptr){
            auto* ni = static_cast<ffi_instance*>(variablePtr->ptr);
            if (ni->klass && ni->library){
                value instance = makeNone();
                cffiCallConstructor(*ni->library, *ni->klass, *this, args, instance, kwargs);
                return instance;
            }
            return invokeAny(*variablePtr, args);
        } else if (variablePtr){
            raiseError("TypeError", "'" + pythonTypeName(*variablePtr) + "' object is not callable");
            return makeNone();
        }
    }

    function_t* function = findFunction(funcName);
    if (!function){

        const std::vector<std::string>& excNames = builtinExceptionNames();
        if (std::find(excNames.begin(), excNames.end(), funcName) != excNames.end()){
            std::string msg;
            value value = makeNone();
            if (!args.empty()){
                value = args[0];
                msg = toString(value);
            }
            return makeExceptionObject(funcName, msg, value);
        }

        if (lastCalleeSpan.valid()){
            setCalleeCaret();
        } else if (!frames.empty() && frames.back().caret.valid()){
            span_t caret = frames.back().caret;
            caret.anchorStart = caret.startCol;
            caret.endCol = caret.startCol + static_cast<int>(funcName.size()) - 1;
            caret.anchorEnd = caret.endCol;
            pendingCaret = caret;
        }
        std::string message = "name '" + funcName + "' is not defined";
        std::string suggestion = suggestName(funcName);
        if (!suggestion.empty()){
            message += ". Did you mean: '" + suggestion + "'?";
        }
        raiseError("NameError", message);
        return makeNone();
    }
    return invoke(function, args, nullptr, callScript, funcName, kwargs);
}



// pyd 读取 文件 字节串
// pyd read file bytes
// pyd ᠪᠡᠷᠢᠵᠡ ᠪᠢᠴᠢᠭ ᠪᠢᠲ᠋ᠢ ᠦᠰᠦᠭ
static bool pydReadFileBytes(const std::string& path, std::vector<unsigned char>& out){
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    out.clear();
    unsigned char buffer[4096];
    size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), fp)) > 0){
        out.insert(out.end(), buffer, buffer + n);
    }
    std::fclose(fp);
    return !out.empty();
}



// pyd Locate 文件
// pyd locate file
// pyd Locate ᠪᠢᠴᠢᠭ
static bool pydLocateFile(const std::string& moduleName, std::string& foundPath){
    const char separator = mpPathSeparator();
    std::string relative = moduleName;
    for (auto& c : relative){
        if (c == '.') c = separator;
    }

// 搜索顺序: 当前工作目录优先, 其次可执行文件所在目录, 与 CPython 一致
// Search order: current working directory first, then the executable directory, like CPython
// ᠬᠠᠢᠬᠤ ᠵᠠᠷᠴᠢᠮ: ᠡᠬᠢᠨ᠎ᠡ ᠠᠵᠢᠯᠤᠨ ᠦ ᠭᠠᠵᠠᠷ
    std::vector<std::string> dirs;
    dirs.push_back("");
    std::string exeDir = mpExecutableDirectory();
    if (!exeDir.empty()) dirs.push_back(exeDir);

// 平台扩展模块后缀, Windows 为 pyd, 类 Unix 为共享库
// Platform extension suffixes: pyd on Windows, shared objects elsewhere
// ᠲᠠᠯᠠᠪᠠᠢ ᠶᠢᠨ ᠨᠡᠮᠡᠯᠲᠡ ᠬᠡᠰᠡᠭ
#if defined(MINIPY_OS_WINDOWS)
    std::vector<std::string> suffixes = {".pyd"};
#elif defined(MINIPY_OS_MACOS) || defined(MINIPY_OS_IOS)
    std::vector<std::string> suffixes = {".dylib", ".so"};
#else
    std::vector<std::string> suffixes = {".so"};
#endif

    std::string baseName = relative;
    size_t cut = baseName.find_last_of("/\\");
    if (cut != std::string::npos) baseName = baseName.substr(cut + 1);

    for (const auto& dir : dirs){
        std::string prefixPath = dir.empty() ? std::string() : (dir + separator);

// 先找完全同名的文件, 保持原有优先级
// First look for an exact-name file, keeping the original priority
// ᠡᠬᠢᠨ᠎ᠡ ᠳᠠᠭᠠᠷᠢᠯᠳᠤᠨ᠎ᠠ ᠰᠠᠪᠠᠷᠢ
        for (const auto& suffix : suffixes){
            std::string exact = prefixPath + relative + suffix;
            FILE* fp = std::fopen(exact.c_str(), "rb");
            if (fp){
                std::fclose(fp);
                foundPath = exact;
                return true;
            }
        }

// 再按 模块名.标签.后缀 的形式匹配带 ABI 标签的扩展模块
// Then match ABI-tagged modules of the form name.tag.suffix
// ᠳᠠᠷᠠᠭ᠎ᠠ ᠨᠡᠷᠡ.ᠲᠡᠮᠳᠡᠭ.ᠨᠡᠮᠡᠯᠲᠡ ᠬᠡᠰᠡᠭ ᠢ ᠬᠠᠢᠨ᠎ᠠ
        std::vector<mpDirEntry> entries = mpListDirectory(dir.empty() ? std::string(".") : dir);
        for (const auto& entry : entries){
            std::string fileName = entry.name;
            if (fileName.size() <= baseName.size() + 1) continue;
            if (fileName.compare(0, baseName.size(), baseName) != 0) continue;
            if (fileName[baseName.size()] != '.') continue;
            for (const auto& suffix : suffixes){
                if (fileName.size() >= suffix.size() &&
                    fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) == 0){
                    foundPath = prefixPath + fileName;
                    return true;
                }
            }
        }
    }
    return false;
}


void vm::executeBody(std::vector<execute_t>& body, bool& returned, value& returnValue,

                             size_t startPc, bool* pauseOut, size_t* pausePcOut,

                             GeneratorState* gs){
    size_t i = startPc;
    bool jumpedToBranch = false;
    std::vector<ForState> localForStates(body.size());
    std::vector<ActiveTry> localTryStack;
    std::vector<PendingFinish> localPendingFinishes;

    auto forState = [&](size_t idx) -> ForState& {
        return gs ? gs->forStates[idx] : localForStates[idx];
    };
    std::vector<ActiveTry>& activeTryStack = gs ? gs->tryStack : localTryStack;
    std::vector<PendingFinish>& pendingFinishes = gs ? gs->pendingFinishes : localPendingFinishes;

    auto startFinish = [&](int kind, size_t jumpTarget) -> bool {
        while (!activeTryStack.empty()){
            ActiveTry& top = activeTryStack.back();
            size_t fi = top.finallyIndex;
            size_t ei = top.endIndex;
            if (ei <= i){
                activeTryStack.pop_back();
                continue;
            }
            if (fi != ei && fi <= i){
                activeTryStack.pop_back();
                continue;
            }
            if (fi == ei || fi == (size_t)(-1)){
                activeTryStack.pop_back();
                continue;
            }
            if (kind == 1 && jumpTarget < ei){
                return false;
            }
            PendingFinish pf;
            pf.kind = kind;
            pf.jumpTarget = jumpTarget;
            pf.returnValue = returnValue;
            pf.finallyIdx = fi;
            pf.endJmpIdx = ei - 1;
            pendingFinishes.push_back(pf);
            activeTryStack.pop_back();
            i = fi;
            return true;
        }
        return false;
    };

    auto dropPendingInCurrentFinally = [&]() {
        while (!pendingFinishes.empty() &&
               i >= pendingFinishes.back().finallyIdx &&
               i <= pendingFinishes.back().endJmpIdx){
            pendingFinishes.pop_back();
        }
    };
    while (true){
    top_of_loop:

        while (!activeTryStack.empty() && activeTryStack.back().endIndex < i){
            if (activeTryStack.back().handledExc && !handledExcStack.empty()){
                handledExcStack.pop_back();
            }
            activeTryStack.pop_back();
        }
        if (i >= body.size()) break;

        if (failed){

            dropPendingInCurrentFinally();
            bool caught = false;
            bool runFinally = false;
            while (!activeTryStack.empty()){
                ActiveTry top = activeTryStack.back();
                activeTryStack.pop_back();

                if (!top.inHandler){

                    for (size_t h = 0; h < top.types.size(); ++h){
                        if (handlerMatches(top.types[h], pendingExceptionType)){

                            failed = false;
                            lastErr.clear();

                            if (!top.vars[h].empty()){
                                value excObj = makeExceptionObject(pendingExceptionType,
                                                                     pendingExceptionMsg,
                                                                     pendingExceptionValue);
                                assignTarget(top.vars[h], excObj);
                            }
                            ExceptionPayload ep;
                            ep.type = pendingExceptionType;
                            ep.msg = pendingExceptionMsg;
                            ep.value = pendingExceptionValue;
                            handledExcStack.push_back(ep);
                            top.handledExc = true;
                            pendingExceptionType.clear();
                            pendingExceptionMsg.clear();
                            pendingExceptionValue = makeNone();
                            top.inHandler = true;
                            activeTryStack.push_back(top);
                            i = top.handlerIndices[h];
                            caught = true;
                            break;
                        }
                    }
                }
                if (caught) break;

                if (top.finallyIndex != top.endIndex && top.finallyIndex != (size_t)(-1)){
                    PendingFinish pf;
                    pf.kind = 2;
                    pf.excType = pendingExceptionType;
                    pf.excMsg = pendingExceptionMsg;
                    pf.excValue = pendingExceptionValue;
                    pf.finallyIdx = top.finallyIndex;
                    pf.endJmpIdx = top.endIndex - 1;
                    pendingFinishes.push_back(pf);
                    failed = false;
                    pendingExceptionType.clear();
                    pendingExceptionMsg.clear();
                    i = top.finallyIndex;
                    runFinally = true;
                    break;
                }

            }
            if (caught) continue;
            if (runFinally) continue;

            return;
        }
        execute_t& ins = body[i];
        if (!frames.empty()){
            if (ins.line > 0) frames.back().line = ins.line;
            frames.back().caret = ins.span;
        }
        pendingCaret = ins.span;
        if (ins.operation == eop_jmp){

            if (ins.breakFor != (size_t)(-1)){
                forState(ins.breakFor).done = true;
            }
            if (ins.jumpTarget == (size_t)(-1)){
                ins.jumpTarget = inlineIndex(ins.operands.empty() ? "" : ins.operands[0], i + 1);
            }
            size_t target = ins.jumpTarget;

            if (!pendingFinishes.empty() && pendingFinishes.back().endJmpIdx == i){
                PendingFinish pf = pendingFinishes.back();
                pendingFinishes.pop_back();
                if (pf.kind == 0){
                    returnValue = pf.returnValue;
                    if (startFinish(0, 0)) continue;
                    returned = true;
                    return;
                } else if (pf.kind == 1){
                    if (startFinish(1, pf.jumpTarget)) continue;
                    i = pf.jumpTarget;
                    continue;
                } else {


                    if (failed){
                        pendingExceptionType = pf.excType;
                        pendingExceptionMsg = pf.excMsg;
                        pendingExceptionValue = pf.excValue;
                    }

                    continue;
                }
            }

            if (startFinish(1, target)) continue;
            i = target;
            continue;
        } else if (ins.operation == eop_call){
            std::string callee = ins.operands.empty() ? "" : ins.operands[0];
            std::vector<value> args;
            for (size_t j = 1; j < ins.operands.size(); ++j){
                if (j < ins.operandSpans.size() && ins.operandSpans[j].valid()){
                    pendingCaret = ins.operandSpans[j];
                } else {
                    pendingCaret = ins.span;
                }
                args.push_back(evalExpr(ins.operands[j]));
                if (failed) break;
            }

            if (!failed){
                for (auto& unpack : ins.argsUnpack){
                    value u = evalExpr(unpack);
                    if (failed) break;
                    if ((u.type == "list" || u.type == "tuple") && u.ptr){
                        auto* lst = static_cast<std::vector<value>*>(u.ptr);
                        for (auto& item : *lst) args.push_back(item);
                    } else {
                        raiseError("TypeError", "can only use * on list/tuple, got '" +
                                pythonTypeName(u) + "'");
                        break;
                    }
                }
            }
            std::map<std::string, value> kwargs;
            for (auto& entry : ins.kwargs){
                if (failed) break;
                pendingCaret = ins.span;
                value kwValue = evalExpr(entry.second);
                if (!failed) kwargs[entry.first] = kwValue;
            }

            if (!failed){
                for (auto& unpack : ins.kwargsUnpack){
                    value u = evalExpr(unpack);
                    if (failed) break;
                    if (u.type == "dict" && u.ptr){
                        auto* d = static_cast<dict_t*>(u.ptr);
                        for (auto& kv : *d){
                            if (kv.first.type == "str" && kv.first.ptr){
                                kwargs[*static_cast<std::string*>(kv.first.ptr)] = kv.second;
                            }
                        }
                    } else {
                        raiseError("TypeError", "can only use ** on dict, got '" +
                                pythonTypeName(u) + "'");
                        break;
                    }
                }
            }
            if (!failed){
                if (ins.hidePropagationCaret){

                    pendingCaret = span_t();
                    if (!frames.empty()) frames.back().caret = span_t();
                } else {
                    pendingCaret = ins.span;
                    if (!frames.empty()) frames.back().caret = ins.span;
                }
                if (!ins.operandSpans.empty()) lastCalleeSpan = ins.operandSpans[0];
                value result = callFunc(callee, args, kwargs);
                lastCallResult = result;
                if (ins.callId != 0) callResults[ins.callId] = result;
            }
        } else if (ins.operation == eop_function){
            std::string uniqueName = ins.operands.empty() ? "" : ins.operands[0];
            std::string visibleName = (ins.operands.size() > 1) ? ins.operands[1] : uniqueName;
            function_t* fn = findFunction(uniqueName);
            if (!fn){
                raiseError("NameError", "function '" + uniqueName + "' not found for closure creation");
            } else {
                auto* ch = new closure_handle();
                ch->fn = fn;
                for (auto& s : scopes){
                    for (auto& kv : s){
                        ch->captures[kv.first] = kv.second;
                    }
                }

                std::set<std::string> enclosingNames;
                for (size_t si = 1; si < scopes.size(); ++si){
                    for (auto& kv : scopes[si]) enclosingNames.insert(kv.first);
                }
                ch->freeVars = computeClosureFreeVars(fn, enclosingNames);
                value closure = makeDynamicObject("closure", ch, closureStateGet, nullptr);
                setVariable(visibleName, closure);
            }
        } else if (ins.operation == eop_return){
            if (!ins.operands.empty()){
                returnValue = evalExpr(ins.operands[0]);
            } else {
                returnValue = makeNone();
            }
            if (failed){

                continue;
            }

            dropPendingInCurrentFinally();

            if (startFinish(0, 0)) continue;
            returned = true;
            return;
        } else if (ins.operation == eop_yield){


            value yielded = (ins.operands.size() > 1 && !ins.operands[1].empty())
                                  ? evalExpr(ins.operands[1]) : makeNone();
            if (failed) continue;
            if (pauseOut){

                if (gs) gs->pendingTarget = ins.operands.empty() ? "" : ins.operands[0];
                *pauseOut = true;
                *pausePcOut = i;
                returnValue = yielded;
                return;
            }

            returnValue = yielded;
            returned = true;
            return;
        } else if (ins.operation == eop_yield_from || ins.operation == eop_await){

            if (!gs || !pauseOut){

                if (ins.operands.size() > 1 && !ins.operands[1].empty()){
                    evalExpr(ins.operands[1]);
                }
                i++;
                continue;
            }
            DelegState& ds = gs->delegStates[i];
            if (!ds.active){

                value sub = (ins.operands.size() > 1 && !ins.operands[1].empty())
                                  ? evalExpr(ins.operands[1]) : makeNone();
                if (failed) continue;
                ds.target = ins.operands.empty() ? "" : ins.operands[0];
                ds.index = 0;
                ds.result = makeNone();
                if (ins.operation == eop_await){

                    auto* inst = sub.type == "native_object" && sub.ptr
                                     ? static_cast<ffi_instance*>(sub.ptr) : nullptr;
                    bool coroutineOk = inst && inst->typeName == "coroutine" &&
                                       generatorStateOf(sub) != nullptr;
                    if (!coroutineOk){
                        raiseError("TypeError",
                                   "object " + pythonTypeName(sub) +
                                   " can't be used in 'await' expression");
                        continue;
                    }
                }
                if (generatorStateOf(sub) != nullptr){
                    ds.mode = 1;
                    ds.sub = sub;
                } else if (sub.type == "list" || sub.type == "tuple" || sub.type == "dict" ||
                           sub.type == "dict_keys" || sub.type == "dict_values" ||
                           sub.type == "dict_items" || sub.type == "str"){
                    ds.mode = 2;
                    materializeIterable(sub, ds.items);
                    if (failed) continue;
                } else if (sub.type == "range" && sub.ptr != nullptr){
                    ds.mode = 3;
                    ds.range = *static_cast<range_t*>(sub.ptr);
                } else {
                    raiseError("TypeError",
                               "'" + pythonTypeName(sub) + "' object is not iterable");
                    continue;
                }
                ds.active = true;
            }

            int action = 0;
            value payload = makeNone();
            if (gs->hasPendingThrow){
                action = 2;
                payload = makeExceptionObject(gs->pendingThrowType, gs->pendingThrowMsg,
                                              gs->pendingThrowValue);
                gs->hasPendingThrow = false;
            } else if (gs->closing){
                action = 3;
                payload = makeExceptionObject("GeneratorExit", "", makeNone());
            } else if (gs->hasPendingSend &&
                       !(gs->pendingSend.type == "none" && gs->pendingSend.ptr == nullptr)){
                action = 1;
                payload = gs->pendingSend;
            }
            gs->hasPendingSend = false;
            value v;
            int r = delegNextValue(ds, action, payload, v);
            if (r == DL_YIELD){


                if (gs) gs->pendingTarget.clear();
                *pauseOut = true;
                *pausePcOut = i;
                returnValue = v;
                return;
            }
            if (r == DL_DONE){

                std::string target = ds.target;
                value ret = ds.result;
                ds = DelegState();
                if (!target.empty()) assignTarget(target, ret);
                if (failed) continue;
                i++;
                continue;
            }

            continue;
        } else if (ins.operation == eop_raise){
            pendingCaret = span_t();
            if (ins.operands.empty()){

                if (!handledExcStack.empty()){
                    ExceptionPayload& ep = handledExcStack.back();
                    pendingCaret = span_t();
                    raiseError(ep.type, ep.msg, ep.value);
                } else {
                    raiseError("RuntimeError", "No active exception to reraise");
                }
            } else if (ins.operands[0].empty()){

                if (ins.operands.size() >= 2){
                    evalExpr(ins.operands[1]);
                }
                if (!failed){
                    pendingCaret = span_t();
                    raiseError("TypeError", "exceptions must derive from BaseException");
                }
            } else {
                std::string typeName = ins.operands[0];
                bool known = false;
                for (const auto& exceptionName : builtinExceptionNames()){
                    if (exceptionName == typeName){
                        known = true;
                        break;
                    }
                }
                if (!known){

                    value raised = evalExpr(ins.operands[0]);
                    if (failed){

                        continue;
                    }
                    auto* inst = raised.type == "native_object" && raised.ptr
                                     ? static_cast<ffi_instance*>(raised.ptr) : nullptr;
                    if (inst && inst->typeName == "exception" && inst->handle){
                        auto* payload = static_cast<ExceptionPayload*>(inst->handle);
                        pendingCaret = span_t();
                        raiseError(payload->type, payload->msg, payload->value);
                    } else {
                        if (ins.operands.size() >= 2){
                            evalExpr(ins.operands[1]);
                        }
                        if (!failed){
                            pendingCaret = span_t();
                            raiseError("TypeError",
                                       "exceptions must derive from BaseException");
                        }
                    }
                } else {
                    std::vector<std::string> argsText;
                    for (size_t k = 1; k < ins.operands.size(); ++k){
                        argsText.push_back(ins.operands[k]);
                    }
                    std::vector<value> argValues;
                    for (const auto& argText : argsText){
                        if (failed) break;
                        argValues.push_back(evalExpr(argText));
                    }
                    if (!failed){
                        std::string message;
                        value origValue = makeNone();
                        if (argValues.size() == 1){
                            origValue = argValues[0];
                            message = toString(argValues[0]);
                        } else if (argValues.size() > 1){
                            message = "(";
                            for (size_t k = 0; k < argValues.size(); ++k){
                                if (k > 0) message += ", ";
                                message += pythonRepr(*this, argValues[k]);
                            }
                            message += ")";
                        }
                        pendingCaret = span_t();
                        raiseError(typeName, message, origValue);
                    }
                }
            }
        } else if (ins.operation == eop_assign){
            if (ins.operands.size() >= 2 && ins.operands[1] == "list"){
                std::vector<value> elements;
                for (size_t j = 2; j < ins.operands.size(); ++j){
                    elements.push_back(evalExpr(ins.operands[j]));
                }
                assignTarget(ins.operands[0], makeList(elements));
            } else if (ins.operands.size() >= 3){
                assignTarget(ins.operands[0], evalExpr(ins.operands[2]));
            } else if (ins.operands.size() == 2){
                assignTarget(ins.operands[0], evalExpr(ins.operands[1]));
            }
        } else if (ins.operation == eop_unpack){

            if (ins.operands.size() >= 2){
                value source = evalExpr(ins.operands[0]);
                if (failed) goto top_of_loop;
                std::vector<std::string> targets(ins.operands.begin() + 1, ins.operands.end());
                unpackValues(source, targets);
                if (failed) goto top_of_loop;
            }
        } else if (ins.operation == eop_expr){

            if (!ins.operands.empty()){
                evalExpr(ins.operands[0]);
                if (failed) goto top_of_loop;
            }
        } else if (ins.operation == eop_delslice){

            if (ins.operands.size() >= 2){
                value base = evalExpr(ins.operands[0]);
                if (failed) goto top_of_loop;
                std::vector<value>* lst = nullptr;
                if (base.type == "list" && base.ptr){
                    lst = static_cast<std::vector<value>*>(base.ptr);
                } else if (base.type == "object" && base.ptr){
                    auto* inst = static_cast<instance*>(base.ptr);
                    if (inst->container.type == "list" && inst->container.ptr)
                        lst = static_cast<std::vector<value>*>(inst->container.ptr);
                }
                if (!lst){
                    raiseError("TypeError", "'" + pythonTypeName(base) +
                                            "' object does not support item deletion");
                    return;
                }
                long long s = 0, e = 0, st = 1;
                if (!sliceBounds(ins.operands[1], static_cast<long long>(lst->size()), s, e, st))
                    return;
                if (st == 1){
                    lst->erase(lst->begin() + s, lst->begin() + e);
                } else {
                    std::vector<value> kept;
                    kept.reserve(lst->size());
                    for (long long idx = 0; idx < static_cast<long long>(lst->size()); ++idx)
                        kept.push_back((*lst)[static_cast<size_t>(idx)]);
                    std::vector<bool> remove(lst->size(), false);
                    for (long long idx = s; (st > 0) ? idx < e : idx > e; idx += st)
                        remove[static_cast<size_t>(idx)] = true;
                    lst->clear();
                    for (size_t idx = 0; idx < kept.size(); ++idx)
                        if (!remove[idx]) lst->push_back(kept[idx]);
                }
            }
        } else if (ins.operation == eop_augassign){

            if (ins.operands.size() >= 3){
                std::string varName = ins.operands[0];
                std::string op = ins.operands[1];
                std::string rhsText = ins.operands[2];
                value left = evalExpr(varName);
                value right = evalExpr(rhsText);
                if (failed) goto top_of_loop;

                auto [inplaceHandled, inplaceResult] = tryMagicInplaceOp(*this, op, left, right);
                if (inplaceHandled){
                    assignTarget(varName, inplaceResult);
                } else {

                    value result = applyBinary(op, left, right);
                    if (failed) goto top_of_loop;
                    assignTarget(varName, result);
                }
            }
        } else if (ins.operation == eop_delattr){

            if (ins.operands.size() >= 2){
                std::string baseName = ins.operands[0];
                std::string member = ins.operands[1];
                value base = evalExpr(baseName);
                if (failed) goto top_of_loop;
                std::string attrName = member;
                bool delattrDone = false;
                if (base.type == "object" && base.ptr){
                    auto* inst = static_cast<instance*>(base.ptr);

                    if (inst->klass){
                        function_t* delattrMethod = findMethod(inst->klass, "__delattr__");
                        if (delattrMethod){
                            auto snap = saveErrorState();
                            value selfCopy = base;
                            value nameStr = makeString(attrName);
                            (void)invoke(delattrMethod, {nameStr}, &selfCopy);
                            if (!hasError()) delattrDone = true;
                            else restoreErrorState(snap);
                        }
                    }

                    if (!delattrDone){
                        value attrDict("dict", inst->attributes, this);
                        if (dictRemove(attrDict, makeString(attrName))){
                            delattrDone = true;
                        }
                    }

                    if (!delattrDone && inst->klass && inst->klass->attributes){
                        auto* klassAttrs = static_cast<std::map<std::string, value>*>(inst->klass->attributes);
                        auto clsIt = klassAttrs->find(attrName);
                        if (clsIt != klassAttrs->end()){
                            klassAttrs->erase(clsIt);
                            delattrDone = true;
                        }
                    }
                    if (!delattrDone)
                        raiseError("AttributeError", "'" + (inst->klass ? inst->klass->name : "object") +
                                                    "' object has no attribute '" + attrName + "'");
                } else if (base.type == "native_object" && base.ptr){
                    auto* ninst = static_cast<ffi_instance*>(base.ptr);
                    auto it = ninst->attributes.find(attrName);
                    if (it != ninst->attributes.end()){
                        ninst->attributes.erase(it);
                        delattrDone = true;
                    } else {
                        raiseError("AttributeError", "'" + (ninst->klass ? ninst->klass->name : ninst->typeName) +
                                                    "' object has no attribute '" + attrName + "'");
                    }
                } else if (base.type == "class" && base.ptr){

                    auto* klass = static_cast<class_t*>(base.ptr);
                    if (klass->attributes){
                        auto* klassAttrs = static_cast<std::map<std::string, value>*>(klass->attributes);
                        auto it = klassAttrs->find(attrName);
                        if (it != klassAttrs->end()){
                            klassAttrs->erase(it);
                            delattrDone = true;
                        }
                    }
                    if (!delattrDone)
                        raiseError("AttributeError", "type object '" + klass->name +
                                                    "' has no attribute '" + attrName + "'");
                } else {
                    raiseError("AttributeError", "'" + pythonTypeName(base) +
                                                "' object has no attribute '" + attrName + "'");
                }
                if (failed) goto top_of_loop;
            }
        } else if (ins.operation == eop_delvar){

            if (ins.operands.size() >= 1){
                std::string varName = ins.operands[0];
                removeVariable(varName);
            }
        } else if (ins.operation == eop_for){
            const std::string& var = ins.operands.empty() ? "" : ins.operands[0];
            ForState& state = forState(i);

            if (state.done) state = ForState();
            if (!state.initialized){
                state.initialized = true;
                state.bodyStart = inlineIndex(ins.operands.size() > 2 ? ins.operands[2] : "", i + 1);
                state.endIndex = inlineIndex(ins.operands.size() > 3 ? ins.operands[3] : "", body.size());
                value iterable = evalExpr(ins.operands.size() > 1 ? ins.operands[1] : "");
                if (failed) continue;
                bool isAsyncFor = ins.operands.size() > 4 && ins.operands[4] == "async";
                if (isAsyncFor){

                    auto* inst = iterable.type == "native_object" && iterable.ptr
                                     ? static_cast<ffi_instance*>(iterable.ptr) : nullptr;
                    if (!inst || inst->typeName != "async_generator" ||
                        !generatorStateOf(iterable)){
                        raiseError("TypeError",
                                   "'async for' requires an object with __aiter__ method, got " +
                                   pythonTypeName(iterable));
                        continue;
                    }
                    state.mode = 3;
                    state.genObj = iterable;
                } else if (iterable.type == "native_object" && iterable.ptr){
                    auto* inst = static_cast<ffi_instance*>(iterable.ptr);
                    GeneratorState* subGs = generatorStateOf(iterable);
                    if (subGs && inst->typeName == "generator"){

                        state.mode = 2;
                        state.genObj = iterable;
                    } else if (subGs && inst->typeName == "async_generator"){
                        raiseError("TypeError", "'async_generator' object is not iterable");
                        continue;
                    } else if (subGs && inst->typeName == "coroutine"){
                        raiseError("TypeError", "'coroutine' object is not iterable");
                        continue;
                    } else {
                        state.mode = 0;
                        materializeIterable(iterable, state.items);
                        if (failed) continue;
                    }
                } else if (iterable.type == "range" && iterable.ptr != nullptr){

                    auto* range = static_cast<range_t*>(iterable.ptr);
                    state.mode = 1;
                    state.rangeStart = range->start;
                    state.rangeStep = range->step;
                    state.rangeCount = range->count();
                } else if (iterable.type == "int" || iterable.type == "bool"){

                    state.mode = 1;
                    state.rangeStart = 0;
                    state.rangeStep = 1;
                    state.rangeCount = toInt(iterable);
                } else {
                    state.mode = 0;
                    materializeIterable(iterable, state.items);
                    if (failed) continue;
                }


                if (state.endIndex == state.bodyStart + 1 && state.bodyStart < body.size() &&
                    body[state.bodyStart].operation == eop_jmp){
                    execute_t& tail = body[state.bodyStart];
                    if (tail.jumpTarget == (size_t)(-1)){
                        tail.jumpTarget = inlineIndex(tail.operands.empty() ? "" : tail.operands[0],
                                                      (size_t)(-1));
                    }
                    if (tail.jumpTarget == i){
                        bool hasLast = false;
                        value last = makeNone();
                        if (state.mode == 1){
                            if (state.rangeCount > 0){
                                last = makeInt(state.rangeStart + (state.rangeCount - 1) * state.rangeStep);
                                hasLast = true;
                            }
                        } else if (!state.items.empty()){
                            last = state.items.back();
                            hasLast = true;
                        }
                        if (hasLast){
                            auto& scope = scopes.back();
                            auto it = scope.find(var);
                            if (it == scope.end()) it = scope.emplace(var, value()).first;
                            it->second = last;
                        }
                        state.done = true;
                        i = state.endIndex;
                        continue;
                    }
                }
            }

            if (state.target == nullptr){
                auto& scope = scopes.back();
                auto it = scope.find(var);
                if (it == scope.end()) it = scope.emplace(var, value()).first;
                state.target = &it->second;
            }
            if (state.mode == 2){

                DelegState tmp;
                tmp.mode = 1;
                tmp.sub = state.genObj;
                value v;
                int r = delegNextValue(tmp, 0, makeNone(), v);
                if (r == DL_YIELD){
                    *state.target = v;
                    state.index++;
                    i = state.bodyStart;
                    continue;
                }
                if (r == DL_DONE){
                    state.done = true;
                    i = state.endIndex;
                    continue;
                }
                continue;
            }
            if (state.mode == 3){

                if (state.pendingReady){
                    state.pendingReady = false;
                    *state.target = state.pendingValue;
                    state.index++;
                    i = state.bodyStart;
                    continue;
                }

                auto* inst = static_cast<ffi_instance*>(state.genObj.ptr);
                value methodObj;
                value wrapper;
                if (inst && inst->dynamicGet(*this, inst->handle, "__anext__", methodObj) &&
                    methodObj.type == "native_object"){
                    auto* methodInst = static_cast<ffi_instance*>(methodObj.ptr);
                    std::map<std::string, value> noKwargs;
                    methodInst->dynamicCall(*this, *methodInst, {}, noKwargs, wrapper);
                }
                if (failed) continue;
                if (wrapper.type != "native_object"){
                    raiseError("TypeError",
                               "'async for' requires an object with __aiter__ method");
                    continue;
                }
                DelegState tmp;
                tmp.mode = 1;
                tmp.sub = wrapper;
                value v;
                int r = delegNextValue(tmp, 0, makeNone(), v);
                if (r == DL_YIELD){

                    state.pendingValue = v;
                    state.pendingReady = true;
                    returnValue = v;
                    if (gs) gs->pendingTarget.clear();
                    *pauseOut = true;
                    *pausePcOut = i;
                    return;
                }
                if (r == DL_DONE){

                    state.done = true;
                    i = state.endIndex;
                    continue;
                }
                continue;
            }
            if (state.mode == 1){
                if (state.index < state.rangeCount){
                    *state.target = makeInt(state.rangeStart + state.index * state.rangeStep);
                    state.index++;
                    i = state.bodyStart;
                    continue;
                }
            } else if (state.index < static_cast<long long>(state.items.size())){
                *state.target = state.items[static_cast<size_t>(state.index)];
                state.index++;
                i = state.bodyStart;
                continue;
            }
            state.done = true;
            i = state.endIndex;
            continue;
        } else if (ins.operation == eop_try){

            if (ins.operands.size() >= 3){
                ActiveTry at;
                at.tryIndex = i;
                size_t nExcept = static_cast<size_t>(std::stoll(ins.operands[0]));
                at.finallyIndex = static_cast<size_t>(std::stoll(ins.operands[1]));
                at.endIndex = static_cast<size_t>(std::stoll(ins.operands[2]));
                at.afterIndex = at.finallyIndex != at.endIndex ? at.finallyIndex : at.endIndex;
                for (size_t h = 0; h < nExcept && h + 3 < ins.operands.size(); ++h){
                    at.handlerIndices.push_back(
                        static_cast<size_t>(std::stoll(ins.operands[h + 3])));
                }


                for (size_t hIdx : at.handlerIndices){
                    if (hIdx < body.size() && body[hIdx].operation == eop_except){
                        if (!body[hIdx].operands.empty()) at.types.push_back(body[hIdx].operands[0]);
                        if (body[hIdx].operands.size() >= 2) at.vars.push_back(body[hIdx].operands[1]);
                        else at.vars.push_back("");
                    } else {
                        at.types.push_back("");
                        at.vars.push_back("");
                    }
                }
                activeTryStack.push_back(std::move(at));
            }
            i++;
            continue;
        } else if (ins.operation == eop_except || ins.operation == eop_finally){

            if (ins.operation == eop_finally){
                value excType = makeNone();
                value excValue = makeNone();
                value excTb = makeNone();

                if (!pendingFinishes.empty() && pendingFinishes.back().kind == 2){
                    const auto& pf = pendingFinishes.back();
                    excType = makeString(pf.excType);
                    excValue = makeExceptionObject(pf.excType, pf.excMsg, pf.excValue);
                }

                setVariable("$exc_type", excType);
                setVariable("$exc_value", excValue);
                setVariable("$exc_tb", excTb);
            }


            i++;
            continue;
        } else if (ins.operation == eop_suppress_if_truthy){


            if (!ins.operands.empty()){
                value result = evalExpr(ins.operands[0]);
                if (truthy(result) && failed){

                    clearPendingExceptionState();
                }
            }
            i++;
            continue;
        } else if (ins.operation == eop_import){

            if (ins.operands.size() >= 2){
                const std::string& moduleName = ins.operands[0];
                const std::string& alias = ins.operands[1];
                bool found = false;
                bool isScriptLib = false;
                for (auto* lib : cffiLibraries){
                    if (lib && lib->name == moduleName){ found = true; break; }
                }
                if (!found){
                    for (auto& lib : scriptLibraries){
                        if (lib.first == moduleName){ found = true; isScriptLib = true; break; }
                    }
                }
                if (!found){

                    std::string pydPath;
                    std::vector<unsigned char> pydBytes;
                    if (pydLocateFile(moduleName, pydPath) && pydReadFileBytes(pydPath, pydBytes)){
                        loadpyd(moduleName, pydBytes);
                        if (!failed){
                            for (auto* lib : cffiLibraries){
                                if (lib && lib->name == moduleName){ found = true; break; }
                            }
                        }
                    }
                }
                if (!found && !failed){
                    raiseError("ModuleNotFoundError", "No module named '" + moduleName + "'");
                }

                if (isScriptLib && !failed){
                    executeScriptModule(moduleName);
                }

                if (alias != moduleName && !failed){
                    for (auto* lib : cffiLibraries){
                        if (lib && lib->name == moduleName){
                            ffi_instance* moduleObj = new ffi_instance();
                            moduleObj->library = lib;
                            moduleObj->klass = nullptr;
                            moduleObj->typeName = moduleName;
                            setVariable(alias, value("native_object",
                                                     keep(vk_native_object, moduleObj), this));
                            break;
                        }
                    }
                }

            }
            i++;
            continue;
        } else if (ins.operation == eop_from_import){

            if (ins.operands.size() >= 3){
                const std::string& moduleName = ins.operands[0];
                const std::string& memberName = ins.operands[1];
                const std::string& alias = ins.operands[2];
                value memberVal = makeNone();
                bool foundModule = false;
                bool isScriptLib = false;

                for (auto* lib : cffiLibraries){
                    if (lib && lib->name == moduleName){ foundModule = true; break; }
                }
                if (!foundModule){
                    for (auto& lib : scriptLibraries){
                        if (lib.first == moduleName){ foundModule = true; isScriptLib = true; break; }
                    }
                }
                if (!foundModule){
                    std::string pydPath;
                    std::vector<unsigned char> pydBytes;
                    if (pydLocateFile(moduleName, pydPath) && pydReadFileBytes(pydPath, pydBytes)){
                        loadpyd(moduleName, pydBytes);
                    }
                }

                if (isScriptLib && !failed){
                    executeScriptModule(moduleName);
                }

                foundModule = false;
                for (auto* lib : cffiLibraries){
                    if (lib && lib->name == moduleName){
                        foundModule = true;

                        if (cffiCallData(*lib, memberName, *this, memberVal)){
                            if (!failed) setVariable(alias, memberVal);
                            break;
                        }

                        auto funcIt = lib->functions.find(memberName);
                        if (funcIt != lib->functions.end()){
                            ffi_instance* fnObj = new ffi_instance();
                            fnObj->library = lib;
                            fnObj->klass = nullptr;
                            fnObj->typeName = memberName;
                            fnObj->handle = funcIt->second;
                            setVariable(alias, value("native_object",
                                                        keep(vk_native_object, fnObj), this));
                            break;
                        }

                        auto classIt = lib->classes.find(memberName);
                        if (classIt != lib->classes.end()){
                            ffi_instance* classObj = new ffi_instance();
                            classObj->library = lib;
                            classObj->klass = &classIt->second;
                            setVariable(alias, value("native_object",
                                                     keep(vk_native_object, classObj), this));
                            break;
                        }
                        break;
                    }
                }
                if (!foundModule){
                    for (auto& lib : scriptLibraries){
                        if (lib.first == moduleName){
                            foundModule = true;

                            function_t* fn = nullptr;
                            for (auto& f : lib.second.functions){
                                if (f.name == memberName){ fn = &f; break; }
                            }
                            if (fn){
                                value fnVal;
                                fnVal.type = "function";
                                fnVal.ptr = fn;
                                setVariable(alias, fnVal);
                                break;
                            }

                            class_t* cls = nullptr;
                            for (auto& c : lib.second.classes){
                                if (c.name == memberName){ cls = &c; break; }
                            }
                            if (cls){
                                initializeClass(*cls);
                                if (failed) break;
                                value clsVal;
                                clsVal.type = "class";
                                clsVal.ptr = cls;
                                setVariable(alias, clsVal);
                                break;
                            }

                            for (auto& v : lib.second.variables){
                                if (v.name == memberName){
                                    setVariable(alias, makeNone());
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    if (!foundModule && !failed){
                        raiseError("ModuleNotFoundError", "No module named '" + moduleName + "'");
                    }
                }
            }
            i++;
            continue;
        } else if (ins.operation == eop_match){
            if (ins.operands.size() < 3){
                i++;
                continue;
            }
            value subject = evalExpr(ins.operands[0]);
            if (failed){
                i++;
                continue;
            }
            std::vector<std::pair<std::string, value>> captures;
            bool ok = matchPattern(subject, ins.operands[1], captures);
            if (failed){
                i++;
                continue;
            }
            if (ok){
                for (auto& capture : captures){
                    assignTarget(capture.first, capture.second);
                    if (failed) break;
                }
            }
            assignTarget(ins.operands[2], makeBool(ok));
            if (failed){
                i++;
                continue;
            }
            i++;
            continue;
        } else if (ins.operation == eop_class){

            if (!ins.operands.empty()){
                size_t index = static_cast<size_t>(std::strtoull(ins.operands[0].c_str(), nullptr, 10));
                const script_t* activeScript = currentScript ? currentScript : &script;
                if (index < activeScript->classes.size()){
                    class_t& klass = const_cast<class_t&>(activeScript->classes[index]);
                    initializeClass(klass);
                    if (failed) goto top_of_loop;
                    if (ins.operands.size() >= 2){

                        value classVal;
                        classVal.type = "class";
                        classVal.ptr = &klass;
                        setVariable(ins.operands[1], classVal);
                    }
                }
            }
            i++;
            continue;
        } else if (ins.operation == eop_if){
            value condVal = evalExpr(ins.operands[0]);
            bool cond = truthy(condVal);
            size_t target = inlineIndex(ins.operands[cond ? 1 : 2], i + 1);
            jumpedToBranch = (!cond && target < body.size() &&
                              (body[target].operation == eop_elif ||
                               body[target].operation == eop_else));
            i = target;
            continue;
        } else if (ins.operation == eop_loop_if){

            value condVal = evalExpr(ins.operands[0]);
            bool cond = truthy(condVal);
            size_t target = inlineIndex(ins.operands[cond ? 1 : 2], i + 1);
            i = target;
            continue;
        } else if (ins.operation == eop_elif){
            if (jumpedToBranch){
                jumpedToBranch = false;
                if (truthy(evalExpr(ins.operands[0]))){
                    i++;
                    continue;
                }
                size_t target = inlineIndex(ins.operands[1], i + 1);
                jumpedToBranch = (target < body.size() &&
                                  (body[target].operation == eop_elif ||
                                   body[target].operation == eop_else));
                i = target;
                continue;
            }

            i = inlineIndex(ins.operands[1], i + 1);
            continue;
        } else if (ins.operation == eop_else){
            size_t target = inlineIndex(ins.operands[0], i + 1);
            if (jumpedToBranch){
                jumpedToBranch = false;
                i++;
                continue;
            }

            i = target;
            continue;
        } else if (ins.operation == eop_inc || ins.operation == eop_dec){
            if (!ins.operands.empty()){
                value* variable = findVariablePtr(ins.operands[0]);
                if (variable){
                    long long delta = (ins.operation == eop_inc) ? 1 : -1;
                    if (variable->type == "float"){
                        *static_cast<double*>(variable->ptr) += static_cast<double>(delta);
                    } else {
                        *static_cast<long long*>(variable->ptr) += delta;
                    }
                }
            }
        }
        i++;
    }
}

bool vm::initializeClass(class_t& klass){

    if (classInitializing.count(&klass)) return true;
    classInitializing.insert(&klass);

// Init Guard
// init guard
// Init Guard
    struct InitGuard{
        std::set<class_t*>& set;
        class_t* klass;
        ~InitGuard(){ set.erase(klass); }
    } initGuard{classInitializing, &klass};

        if (!klass.attributes){
            klass.attributes = new std::map<std::string, value>();
        }

        resolveClassBase(&klass);


        std::map<std::string, value> initScope;
        scopes.push_back(initScope);
        std::set<std::string> methodNames;

        for (auto& method : klass.methods){
            value fnVal;
            fnVal.type = "function";
            fnVal.ptr = const_cast<function_t*>(&method);
            scopes.back()[method.name] = fnVal;
            methodNames.insert(method.name);
        }

        if (!klass.initBody.empty()){
            bool dummyReturned = false;
            value dummyRet = makeNone();
            executeBody(klass.initBody, dummyReturned, dummyRet);
        }

        auto* attrs = static_cast<std::map<std::string, value>*>(klass.attributes);
        for (auto& kv : scopes.back()){
            if (methodNames.count(kv.first) == 0){
                (*attrs)[kv.first] = kv.second;
            }
        }


        {
            auto* cattrs = static_cast<std::map<std::string, value>*>(klass.attributes);
            for (size_t mi = 0; mi < klass.methods.size(); ++mi){
                auto& method = klass.methods[mi];
                if (method.decorators.empty()) continue;
                function_t* methodCopy = new function_t(method);
                value wrapped;
                wrapped.type = "function";
                wrapped.ptr = methodCopy;
                value current = wrapped;
                for (auto it = method.decorators.rbegin();
                     it != method.decorators.rend(); ++it){
                    const std::string& decExpr = *it;
                    value decorator = makeNone();
                    bool found = false;
                    bool decoratorIsClass = false;

                    decorator = evalExpr(decExpr);
                    if (!failed){
                        found = true;
                    } else {
                        clearPendingExceptionState();
                        failed = false;
                    }

                    if (!found){
                        for (auto* library : cffiLibraries){
                            if (!library) continue;
                            auto classIt = library->classes.find(decExpr);
                            if (classIt != library->classes.end()){
                                std::vector<value> ctorArgs = {current};
                                value instance = makeNone();
                                cffiCallConstructor(*library, classIt->second,
                                                   *this, ctorArgs, instance);
                                if (hasError()) break;
                                current = instance;
                                found = true;
                                decoratorIsClass = true;
                                break;
                            }
                        }
                    }
                    if (!found){
                        raiseError("NameError", "name '" + decExpr + "' is not defined");
                        break;
                    }
                    if (!decoratorIsClass){
                        current = invokeAny(decorator, {current});
                        if (hasError()) break;
                    }
                }
                if (!hasError()){

                    scopes.back()[method.name] = current;
                    if (cattrs) (*cattrs)[method.name] = current;
                    method.decorators.clear();
                }
            }
        }

        for (auto& kv : scopes.back()){
            if (methodNames.count(kv.first) == 0){
                (*attrs)[kv.first] = kv.second;
            }
        }

        if (klass.builtinBase.rfind("enum:", 0) == 0){
            applyEnumTransform(*this, klass);
        }

        {
            std::map<std::string, value> subKwargs;
            bool keywordError = false;
            for (auto& entry : klass.classKwargs){
                value value = evalExpr(entry.second);
                if (failed){
                    clearPendingExceptionState();
                    failed = false;
                    keywordError = true;
                    break;
                }
                subKwargs[entry.first] = value;
            }
            if (!keywordError){
                applyInitSubclass(&klass, subKwargs);
                if (hasError()){
                    scopes.pop_back();
                    return false;
                }
            }
        }



        if (!klass.metaclassExpr.empty()){

            value metaclass = evalExpr(klass.metaclassExpr);
            if (failed){

                clearPendingExceptionState();
                failed = false;
            }
            if (!failed && metaclass.type != "none"){

                std::vector<value> basesList;
                if (!klass.baseName.empty()){

                    value baseVal = evalExpr(klass.baseName);
                    if (!failed){
                        basesList.push_back(baseVal);
                    } else {
                        clearPendingExceptionState();
                        failed = false;
                    }
                }


                value ns = makeDict();

                if (attrs){
                    for (auto& kv : *attrs){
                        value keyStr = makeString(kv.first);
                        dictSet(ns, keyStr, kv.second);
                    }
                }

                for (auto& m : klass.methods){
                    value fnVal;
                    fnVal.type = "function";
                    fnVal.ptr = const_cast<function_t*>(&m);
                    value keyStr = makeString(m.name);
                    dictSet(ns, keyStr, fnVal);
                }

                dictSet(ns, makeString("__name__"), makeString(klass.name));
                dictSet(ns, makeString("__qualname__"), makeString(klass.name));

                value nameArg = makeString(klass.name);
                value basesArg = makeTuple(basesList);
                std::vector<value> callArgs = {nameArg, basesArg, ns};



                value newKlass = makeNone();
                if (metaclass.type == "class" && metaclass.ptr){
                    auto* mcKlass = static_cast<class_t*>(metaclass.ptr);
                    function_t* newMethod = findMethod(mcKlass, "__new__");
                    if (newMethod){
                        value mcsSelf = metaclass;
                        std::vector<value> newArgs = {metaclass, nameArg, basesArg, ns};
                        newKlass = invoke(newMethod, newArgs, &mcsSelf, &script, mcKlass->name + ".__new__");
                    }

                } else {

                    newKlass = invokeAny(metaclass, callArgs);
                }
                if (!hasError() && newKlass.type == "class" && newKlass.ptr){


                    auto* newKlassPtr = static_cast<class_t*>(newKlass.ptr);






                    klass.methods = newKlassPtr->methods;
                    klass.variables = newKlassPtr->variables;
                    if (newKlassPtr->attributes){

                        auto* newAttrs = static_cast<std::map<std::string, value>*>(newKlassPtr->attributes);
                        if (!klass.attributes){
                            klass.attributes = new std::map<std::string, value>();
                        }
                        auto* klassAttrs = static_cast<std::map<std::string, value>*>(klass.attributes);
                        for (auto& kv : *newAttrs){
                            (*klassAttrs)[kv.first] = kv.second;
                        }
                        delete newKlassPtr->attributes;
                        newKlassPtr->attributes = nullptr;
                    }


                    auto* finalAttrs = static_cast<std::map<std::string, value>*>(klass.attributes);
                    std::set<std::string> newMethodNames;
                    for (auto& m : klass.methods) newMethodNames.insert(m.name);
                    for (auto it = finalAttrs->begin(); it != finalAttrs->end(); ){
                        if (newMethodNames.count(it->first) && it->second.type == "function")
                            it = finalAttrs->erase(it);
                        else ++it;
                    }
                } else if (!hasError()){

                } else {

                    clearPendingExceptionState();
                    failed = false;
                }

                resolveClassBase(&klass);
            }
        }
    scopes.pop_back();
    return true;
}

void* vm::run(){
    scopes.clear();
    scopes.push_back({});
    lastCallResult = makeNone();
    callResults.clear();
    scopes[0]["__name__"] = makeString("__main__");

    static const char* builtinTypeNames[] = {
        "int", "float", "bool", "str", "list", "tuple", "dict",
        "range", "type", "object", "function", "generator", "NoneType"
    };
    for (auto* tn : builtinTypeNames){
        value obj = getBuiltinTypeObj(tn);
        if (obj.type != "none") scopes[0][tn] = obj;
    }
    currentScript = &script;
    failed = false;
    lastErr.clear();
    recursionDepth = 0;
    frames.clear();
    frames.push_back({currentFileName(), "<module>", 0, &script});
    classInitializing.clear();
    bool returned = false;
    value returnValue = makeNone();
    executeBody(script.mainFunction.body, returned, returnValue);
    frames.clear();
    return nullptr;
}

script_t vm::compile(const std::string& source){
    std::string text = source;

    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF){
        text.erase(0, 3);
    }
    script_t compiled = minipyParseTokens(minipyGetTokens(text));
    compiled.source = text;
    return compiled;
}

bool vm::execSource(const std::string& source){
    script_t nested = compile(source);
    const script_t* previousScript = currentScript;
    currentScript = &nested;
    frames.push_back({currentFileName(), "<module>", 0, &nested});
    bool returned = false;
    value returnValue = makeNone();
    executeBody(nested.mainFunction.body, returned, returnValue);
    frames.pop_back();
    currentScript = previousScript;
    return !failed;
}





namespace {

// Byte Writer
// byte writer
// Byte Writer
class ByteWriter{
public:
    std::vector<unsigned char> data;

// 追加 字节串
// append bytes
// ᠲᠡᠨᠳᠡᠭᠡ ᠨᠡᠮᠡᠵᠡ ᠪᠢᠲ᠋ᠢ ᠦᠰᠦᠭ
    void appendBytes(const void* p, size_t n){
        const unsigned char* b = static_cast<const unsigned char*>(p);
        data.insert(data.end(), b, b + n);
    }

// u8
// u8
// u8
    void u8(uint8_t v){ appendBytes(&v, 1); }

// u16
// u16
// u16
    void u16(uint16_t v){ appendBytes(&v, 2); }

// u32
// u32
// u32
    void u32(uint32_t v){ appendBytes(&v, 4); }

// s32
// s32
// s32
    void s32(int32_t v){ appendBytes(&v, 4); }

// u64
// u64
// u64
    void u64(uint64_t v){ appendBytes(&v, 8); }

// s64
// s64
// s64
    void s64(int64_t v){ appendBytes(&v, 8); }

// sz
// sz
// sz
    void sz(double v){ appendBytes(&v, 8); }

// str
// str
// str
    void str(const std::string& s){
        u32(static_cast<uint32_t>(s.size()));
        if (!s.empty()) appendBytes(s.data(), s.size());
    }

// magic
// magic
// magic
    void magic(const char* m){ appendBytes(m, 4); }
};

// 写入 源码范围
// write span
// ᠣᠷᠣᠭᠤᠯᠬᠤ ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
void writeSpan(ByteWriter& w, const span_t& sp){
    w.s32(sp.line);
    w.s32(sp.startCol);
    w.s32(sp.endCol);
    w.s32(sp.anchorStart);
    w.s32(sp.anchorEnd);
    w.u32(static_cast<uint32_t>(sp.parts.size()));
    for (auto& p : sp.parts){
        w.s32(p.first);
        w.s32(p.second);
    }
}

// 写入 Execute
// write execute
// ᠣᠷᠣᠭᠤᠯᠬᠤ Execute
void writeExecute(ByteWriter& w, const execute_t& e){
    w.u32(e.operation);
    w.u32(static_cast<uint32_t>(e.operands.size()));
    for (auto& s : e.operands) w.str(s);
    w.u32(static_cast<uint32_t>(e.kwargs.size()));
    for (auto& kv : e.kwargs){
        w.str(kv.first);
        w.str(kv.second);
    }
    w.u32(static_cast<uint32_t>(e.argsUnpack.size()));
    for (auto& s : e.argsUnpack) w.str(s);
    w.u32(static_cast<uint32_t>(e.kwargsUnpack.size()));
    for (auto& s : e.kwargsUnpack) w.str(s);
    w.s32(e.line);
    writeSpan(w, e.span);
    w.u32(static_cast<uint32_t>(e.operandSpans.size()));
    for (auto& sp : e.operandSpans) writeSpan(w, sp);
    w.u8(e.hidePropagationCaret ? 1 : 0);
    w.s64(e.callId);

}

// 写入 函数
// write function
// ᠣᠷᠣᠭᠤᠯᠬᠤ ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ
void writeFunction(ByteWriter& w, const function_t& f, int32_t parentIdx){
    w.str(f.name);
    w.str(f.returnType);
    w.u32(static_cast<uint32_t>(f.body.size()));
    for (auto& e : f.body) writeExecute(w, e);
    w.u32(static_cast<uint32_t>(f.argNames.size()));
    for (auto& s : f.argNames) w.str(s);
    w.s32(parentIdx);
    w.u32(static_cast<uint32_t>(f.globalNames.size()));
    for (auto& s : f.globalNames) w.str(s);
    w.u8(f.isGenerator ? 1 : 0);
    w.u8(f.isCoroutine ? 1 : 0);
    w.s32(f.variadicArg);
    w.s32(f.keywordArg);
    w.u32(static_cast<uint32_t>(f.capturedNames.size()));
    for (auto& s : f.capturedNames) w.str(s);
    w.s32(f.positionalOnlyCount);
    w.u32(static_cast<uint32_t>(f.decorators.size()));
    for (auto& s : f.decorators) w.str(s);
    w.u32(static_cast<uint32_t>(f.defaultArgs.size()));
    for (auto& d : f.defaultArgs){
        w.u32(static_cast<uint32_t>(d.first));
        w.str(d.second);
    }
    w.s32(f.keywordOnlyStart);
    w.u32(static_cast<uint32_t>(f.nonlocalNames.size()));
    for (auto& s : f.nonlocalNames) w.str(s);
}

// 写入 类
// write class
// ᠣᠷᠣᠭᠤᠯᠬᠤ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
void writeClass(ByteWriter& w, const class_t& c){
    w.str(c.name);
    w.str(c.baseName);
    w.str(c.fieldsText);
    w.u32(static_cast<uint32_t>(c.methods.size()));
    for (auto& m : c.methods) writeFunction(w, m, -1);
    w.u32(static_cast<uint32_t>(c.variables.size()));
    for (auto& v : c.variables){
        w.str(v.name);
        w.str(v.type);

        w.u32(0);
    }
    w.u32(static_cast<uint32_t>(c.initBody.size()));
    for (auto& e : c.initBody) writeExecute(w, e);
}

// 写入 Variables
// write variables
// ᠣᠷᠣᠭᠤᠯᠬᠤ Variables
void writeVariables(ByteWriter& w, const std::vector<variable_t>& vars){

    std::map<const variable_t*, int32_t> idxMap;
    for (size_t i = 0; i < vars.size(); ++i){
        idxMap[&vars[i]] = static_cast<int32_t>(i);
    }
    w.u32(static_cast<uint32_t>(vars.size()));
    for (auto& v : vars){
        w.str(v.name);
        w.str(v.type);

        w.u32(static_cast<uint32_t>(v.children.size()));
        for (auto* child : v.children){
            auto it = idxMap.find(child);
            w.s32(it == idxMap.end() ? -1 : it->second);
        }
    }
}
}










namespace {

static const uint8_t kMpycObfKey[16] = {
    0xC3, 0x5F, 0x8A, 0x21, 0x7D, 0x92, 0x0B, 0xE4,
    0x16, 0x68, 0x37, 0xAF, 0xB4, 0x5D, 0xCE, 0x90
};

// obfuscate Payload
// obfuscate payload
// obfuscate Payload
static void obfuscatePayload(std::vector<unsigned char>& data){
    if (data.size() <= 8) return;
    size_t pos = 8;
    uint32_t idx = 0;
    for (; pos < data.size(); ++pos, ++idx){
        uint8_t k = kMpycObfKey[idx % 16];
        k ^= static_cast<uint8_t>((idx >> 4) & 0xFF);
        k ^= static_cast<uint8_t>((idx >> 12) & 0xFF);
        k ^= static_cast<uint8_t>((idx * 31u + 17u) & 0xFF);
        data[pos] ^= k;
        uint8_t rot = static_cast<uint8_t>(idx & 7);
        if (rot){
            uint8_t b = data[pos];
            data[pos] = static_cast<unsigned char>((b >> rot) | (b << (8 - rot)));
        }
    }
}

// deobfuscate Payload
// deobfuscate payload
// deobfuscate Payload
static void deobfuscatePayload(std::vector<unsigned char>& data){
    if (data.size() <= 8) return;
    size_t pos = 8;
    uint32_t idx = 0;
    for (; pos < data.size(); ++pos, ++idx){
        uint8_t rot = static_cast<uint8_t>(idx & 7);
        if (rot){
            uint8_t b = data[pos];
            data[pos] = static_cast<unsigned char>((b << rot) | (b >> (8 - rot)));
        }
        uint8_t k = kMpycObfKey[idx % 16];
        k ^= static_cast<uint8_t>((idx >> 4) & 0xFF);
        k ^= static_cast<uint8_t>((idx >> 12) & 0xFF);
        k ^= static_cast<uint8_t>((idx * 31u + 17u) & 0xFF);
        data[pos] ^= k;
    }
}
}

// serialize 脚本
// serialize script
// serialize ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ
static std::vector<unsigned char> serializeScript(const script_t& script){
    ByteWriter w;
    w.magic("MPYC");
    w.u32(1);
    w.str(script.name);
    w.str(script.source);

    writeFunction(w, script.mainFunction, -1);

    w.u32(static_cast<uint32_t>(script.StringLibrary.size()));
    for (auto& s : script.StringLibrary) w.str(s);

    w.u32(static_cast<uint32_t>(script.functions.size()));
    for (auto& f : script.functions) writeFunction(w, f, -1);

    w.u32(static_cast<uint32_t>(script.classes.size()));
    for (auto& c : script.classes) writeClass(w, c);

    writeVariables(w, script.variables);

    obfuscatePayload(w.data);
    return w.data;
}







namespace {

// Byte Reader
// byte reader
// Byte Reader
class ByteReader{
public:
    const unsigned char* data;
    size_t size;
    size_t pos;
    ByteReader(const unsigned char* d, size_t s) : data(d), size(s), pos(0) {}

// ok
// ok
// ok
    bool ok() const { return pos <= size; }

// skip
// skip
// skip
    void skip(size_t n){ pos += n; }

// u8
// u8
// u8
    bool u8(uint8_t& v){
        if (pos + 1 > size) return false;
        v = data[pos]; pos += 1; return true;
    }

// u16
// u16
// u16
    bool u16(uint16_t& v){
        if (pos + 2 > size) return false;
        std::memcpy(&v, data + pos, 2); pos += 2; return true;
    }

// u32
// u32
// u32
    bool u32(uint32_t& v){
        if (pos + 4 > size) return false;
        std::memcpy(&v, data + pos, 4); pos += 4; return true;
    }

// s32
// s32
// s32
    bool s32(int32_t& v){
        if (pos + 4 > size) return false;
        std::memcpy(&v, data + pos, 4); pos += 4; return true;
    }

// u64
// u64
// u64
    bool u64(uint64_t& v){
        if (pos + 8 > size) return false;
        std::memcpy(&v, data + pos, 8); pos += 8; return true;
    }

// s64
// s64
// s64
    bool s64(int64_t& v){
        if (pos + 8 > size) return false;
        std::memcpy(&v, data + pos, 8); pos += 8; return true;
    }

// sz
// sz
// sz
    bool sz(double& v){
        if (pos + 8 > size) return false;
        std::memcpy(&v, data + pos, 8); pos += 8; return true;
    }

// str
// str
// str
    bool str(std::string& out){
        uint32_t n;
        if (!u32(n)) return false;
        if (pos + n > size) return false;
        out.assign(reinterpret_cast<const char*>(data + pos), n);
        pos += n;
        return true;
    }

// 检查 Magic
// check magic
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Magic
    bool checkMagic(const char* m){
        if (pos + 4 > size) return false;
        for (int i = 0; i < 4; ++i) if (data[pos + i] != static_cast<unsigned char>(m[i])) return false;
        pos += 4; return true;
    }
};

// 读取 源码范围
// read span
// ᠪᠡᠷᠢᠵᠡ ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
bool readSpan(ByteReader& r, span_t& sp){
    if (!r.s32(sp.line)) return false;
    if (!r.s32(sp.startCol)) return false;
    if (!r.s32(sp.endCol)) return false;
    if (!r.s32(sp.anchorStart)) return false;
    if (!r.s32(sp.anchorEnd)) return false;
    uint32_t n;
    if (!r.u32(n)) return false;
    sp.parts.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        int32_t a, b;
        if (!r.s32(a) || !r.s32(b)) return false;
        sp.parts[i] = {a, b};
    }
    return true;
}

// 读取 Execute
// read execute
// ᠪᠡᠷᠢᠵᠡ Execute
bool readExecute(ByteReader& r, execute_t& e){
    uint32_t n;
    if (!r.u32(e.operation)) return false;
    if (!r.u32(n)) return false;
    e.operands.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(e.operands[i])) return false;
    if (!r.u32(n)) return false;
    e.kwargs.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        if (!r.str(e.kwargs[i].first)) return false;
        if (!r.str(e.kwargs[i].second)) return false;
    }
    if (!r.u32(n)) return false;
    e.argsUnpack.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(e.argsUnpack[i])) return false;
    if (!r.u32(n)) return false;
    e.kwargsUnpack.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(e.kwargsUnpack[i])) return false;
    if (!r.s32(e.line)) return false;
    if (!readSpan(r, e.span)) return false;
    if (!r.u32(n)) return false;
    e.operandSpans.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!readSpan(r, e.operandSpans[i])) return false;
    uint8_t hc;
    if (!r.u8(hc)) return false;
    e.hidePropagationCaret = hc != 0;
    if (!r.s64(e.callId)) return false;

    e.jumpTarget = (size_t)(-1);
    e.breakFor = (size_t)(-1);
    return true;
}

// 读取 函数
// read function
// ᠪᠡᠷᠢᠵᠡ ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ
bool readFunction(ByteReader& r, function_t& f, int32_t& parentIdxOut){
    if (!r.str(f.name)) return false;
    if (!r.str(f.returnType)) return false;
    uint32_t n;
    if (!r.u32(n)) return false;
    f.body.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!readExecute(r, f.body[i])) return false;
    if (!r.u32(n)) return false;
    f.argNames.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(f.argNames[i])) return false;
    if (!r.s32(parentIdxOut)) return false;
    f.parentClass = nullptr;
    if (!r.u32(n)) return false;
    f.globalNames.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(f.globalNames[i])) return false;
    uint8_t g, c;
    if (!r.u8(g) || !r.u8(c)) return false;
    f.isGenerator = g != 0;
    f.isCoroutine = c != 0;
    if (!r.s32(f.variadicArg)) return false;
    if (!r.s32(f.keywordArg)) return false;
    if (!r.u32(n)) return false;
    f.capturedNames.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(f.capturedNames[i])) return false;
    if (!r.s32(f.positionalOnlyCount)) return false;
    if (!r.u32(n)) return false;
    f.decorators.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(f.decorators[i])) return false;
    if (!r.u32(n)) return false;
    f.defaultArgs.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        uint32_t idx = 0;
        if (!r.u32(idx)) return false;
        f.defaultArgs[i].first = static_cast<int>(idx);
        if (!r.str(f.defaultArgs[i].second)) return false;
    }
    if (!r.s32(f.keywordOnlyStart)) return false;
    if (!r.u32(n)) return false;
    f.nonlocalNames.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(f.nonlocalNames[i])) return false;
    return true;
}

// 读取 类
// read class
// ᠪᠡᠷᠢᠵᠡ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
bool readClass(ByteReader& r, class_t& c){
    if (!r.str(c.name)) return false;
    if (!r.str(c.baseName)) return false;
    if (!r.str(c.fieldsText)) return false;
    uint32_t n;
    if (!r.u32(n)) return false;
    c.methods.resize(n);
    int32_t dummy;
    for (uint32_t i = 0; i < n; ++i) if (!readFunction(r, c.methods[i], dummy)) return false;

    for (auto& m : c.methods) m.parentClass = &c;
    if (!r.u32(n)) return false;
    c.variables.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        if (!r.str(c.variables[i].name)) return false;
        if (!r.str(c.variables[i].type)) return false;
        uint32_t nc;
        if (!r.u32(nc)) return false;
        c.variables[i].children.resize(nc);
        for (uint32_t j = 0; j < nc; ++j){
            int32_t idx;
            if (!r.s32(idx)) return false;
            c.variables[i].children[j] = nullptr;
        }
    }
    if (!r.u32(n)) return false;
    c.initBody.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!readExecute(r, c.initBody[i])) return false;
    return true;
}

// 读取 Variables
// read variables
// ᠪᠡᠷᠢᠵᠡ Variables
bool readVariables(ByteReader& r, std::vector<variable_t>& vars){
    uint32_t n;
    if (!r.u32(n)) return false;
    vars.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        if (!r.str(vars[i].name)) return false;
        if (!r.str(vars[i].type)) return false;
        vars[i].value = nullptr;
        uint32_t nc;
        if (!r.u32(nc)) return false;
        vars[i].children.resize(nc);
        for (uint32_t j = 0; j < nc; ++j){
            int32_t idx;
            if (!r.s32(idx)) return false;
            vars[i].children[j] = (idx >= 0 && idx < static_cast<int32_t>(n)) ? &vars[static_cast<size_t>(idx)] : nullptr;
        }
    }
    return true;
}
}

// deserialize 脚本
// deserialize script
// deserialize ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ
static bool deserializeScript(const std::vector<unsigned char>& bytes, script_t& out){
    ByteReader r(bytes.data(), bytes.size());
    if (!r.checkMagic("MPYC")) return false;
    uint32_t version;
    if (!r.u32(version)) return false;
    if (version != 1) return false;

    std::vector<unsigned char> copy = bytes;
    deobfuscatePayload(copy);
    r = ByteReader(copy.data(), copy.size());
    r.pos = 8;
    if (!r.str(out.name)) return false;
    if (!r.str(out.source)) return false;

    int32_t mainParentIdx;
    if (!readFunction(r, out.mainFunction, mainParentIdx)) return false;
    out.mainFunction.parentClass = nullptr;

    uint32_t n;
    if (!r.u32(n)) return false;
    out.StringLibrary.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!r.str(out.StringLibrary[i])) return false;

    if (!r.u32(n)) return false;
    out.functions.resize(n);
    for (uint32_t i = 0; i < n; ++i){
        int32_t pidx;
        if (!readFunction(r, out.functions[i], pidx)) return false;
        out.functions[i].parentClass = nullptr;
    }

    if (!r.u32(n)) return false;
    out.classes.resize(n);
    for (uint32_t i = 0; i < n; ++i) if (!readClass(r, out.classes[i])) return false;

    if (!readVariables(r, out.variables)) return false;
    return true;
}




bool vm::setBytecode(const std::vector<unsigned char>& bytecode){
    if (bytecode.empty()){
        raiseError("ValueError", "bytecode is empty");
        return false;
    }
    script_t tmp;
    if (!deserializeScript(bytecode, tmp)){
        raiseError("ValueError", "invalid or unsupported bytecode");
        return false;
    }
    script = std::move(tmp);
    return true;
}

bool vm::setBytecode(script_t s){

    script = std::move(s);
    return true;
}

std::vector<unsigned char> vm::compileBytecode(const std::string& source){

    script_t s = compile(source);
    if (hasError()) return {};

    return serializeScript(s);
}

std::vector<unsigned char> vm::complieBytecode(script_t& script){
    return serializeScript(script);
}







// Pyd Kind
// pyd kind
// Pyd Kind
enum PydKind{
    PK_NONE = 0, PK_BOOL, PK_INT, PK_FLOAT, PK_STR, PK_BYTES,
    PK_TUPLE, PK_LIST, PK_DICT, PK_MODULE, PK_OPAQUE, PK_KIND_COUNT
};

// Pyd 类型 Obj
// pyd type obj
// Pyd ᠲᠥᠷᠥᠯ Obj
struct PydTypeObj{
    long long ob_refcnt;
    void* ob_type;
    const char* name;
    unsigned long flags;
};

// Pyd Obj
// pyd obj
// Pyd Obj
struct PydObj;

using PydDictItems = std::vector<std::pair<PydObj*, PydObj*>>;

// Pyd Obj
// pyd obj
// Pyd Obj
struct PydObj{
    long long ob_refcnt = 1;
    void* ob_type = nullptr;
    int kind = PK_NONE;
    long long ival = 0;
    double dval = 0;
    std::string sval;
    PydDictItems* dictItems = nullptr;
    std::vector<PydObj*>* items = nullptr;
    std::string name;
    void* fnPtr = nullptr;
    int fnFlags = 0;
};


static PydTypeObj g_pydTypeNone = {1, nullptr, "NoneType", 0};

static PydTypeObj g_pydTypeBool = {1, nullptr, "bool", (1UL << 24)};

static PydTypeObj g_pydTypeInt = {1, nullptr, "int", (1UL << 24)};

static PydTypeObj g_pydTypeFloat = {1, nullptr, "float", 0};

static PydTypeObj g_pydTypeStr = {1, nullptr, "str", (1UL << 28)};

static PydTypeObj g_pydTypeBytes = {1, nullptr, "bytes", (1UL << 27)};

static PydTypeObj g_pydTypeTuple = {1, nullptr, "tuple", (1UL << 26)};

static PydTypeObj g_pydTypeList = {1, nullptr, "list", (1UL << 25)};

static PydTypeObj g_pydTypeDict = {1, nullptr, "dict", (1UL << 29)};

static PydTypeObj g_pydTypeModule = {1, nullptr, "module", 0};

static PydTypeObj g_pydTypeModuleDef = {1, nullptr, "module_def", 0};

static PydTypeObj* g_pydKinds[PK_KIND_COUNT] = {};

static bool g_pydInited = false;

static PydObj* g_pydNone = nullptr;

static PydObj* g_pydTrue = nullptr;

static PydObj* g_pydFalse = nullptr;

static PydObj* g_pydExcTypeError = nullptr;

static PydObj* g_pydExcValueError = nullptr;

static PydObj* g_pydExcRuntimeError = nullptr;

static PydObj* g_pydExcOSError = nullptr;

static PydObj* g_pydExcMemoryError = nullptr;

static PydObj* g_pydExcOverflowError = nullptr;

static PydObj* g_pydExcImportError = nullptr;

static PydObj* g_pydErrorType = nullptr;

static std::string g_pydErrorMsg;


static PydObj* g_symPyExc_TypeError = nullptr;

static PydObj* g_symPyExc_ValueError = nullptr;

static PydObj* g_symPyExc_RuntimeError = nullptr;

static PydObj* g_symPyExc_OSError = nullptr;

static PydObj* g_symPyExc_MemoryError = nullptr;

static PydObj* g_symPyExc_OverflowError = nullptr;

static PydObj* g_symPyExc_ImportError = nullptr;

static std::map<std::string, void*> g_pydShimExports;


// pyd 新 Obj
// pyd new obj
// pyd ᠰᠢᠨ᠎ᠡ Obj
static PydObj* pydNewObj(int kind){
    PydObj* o = new PydObj();
    o->kind = kind;
    o->ob_type = (void*)g_pydKinds[kind];
    return o;
}

// pyd 新 整数
// pyd new int
// pyd ᠰᠢᠨ᠎ᠡ ᠳ᠋ᠠᠩ ᠲᠣ᠎
static PydObj* pydNewInt(long long v){
    PydObj* o = pydNewObj(PK_INT);
    o->ival = v;
    return o;
}

// pyd 新 浮点
// pyd new float
// pyd ᠰᠢᠨ᠎ᠡ ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ
static PydObj* pydNewFloat(double v){
    PydObj* o = pydNewObj(PK_FLOAT);
    o->dval = v;
    return o;
}

// pyd 新 布尔
// pyd new bool
// pyd ᠰᠢᠨ᠎ᠡ ᠪᠤᠯ
static PydObj* pydNewBool(bool v){
    return v ? g_pydTrue : g_pydFalse;
}

// pyd 新 Str
// pyd new str
// pyd ᠰᠢᠨ᠎ᠡ Str
static PydObj* pydNewStr(const std::string& v){
    PydObj* o = pydNewObj(PK_STR);
    o->sval = v;
    return o;
}

// pyd 新 元组
// pyd new tuple
// pyd ᠰᠢᠨ᠎ᠡ ᠬᠣᠶᠠᠳᠠᠯ ᠲᠠᠯᠠᠭ᠌
static PydObj* pydNewTuple(){
    PydObj* o = pydNewObj(PK_TUPLE);
    o->items = new std::vector<PydObj*>();
    return o;
}

// pyd 新 字典
// pyd new dict
// pyd ᠰᠢᠨ᠎ᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
static PydObj* pydNewDict(){
    PydObj* o = pydNewObj(PK_DICT);
    o->dictItems = new PydDictItems();
    return o;
}

// pyd 新 Exc 类型
// pyd new exc type
// pyd ᠰᠢᠨ᠎ᠡ Exc ᠲᠥᠷᠥᠯ
static PydObj* pydNewExcType(const std::string& name){
    PydObj* o = pydNewObj(PK_OPAQUE);
    o->name = name;
    return o;
}

// pyd 字典 获取
// pyd dict get
// pyd ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
static PydObj* pydDictGet(PydObj* d, const std::string& key){
    if (!d || !d->dictItems) return nullptr;
    for (auto& kv : *d->dictItems){
        if (kv.first && kv.first->kind == PK_STR && kv.first->sval == key) return kv.second;
    }
    return nullptr;
}

// pyd 字典 集合
// pyd dict set
// pyd ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ
static void pydDictSet(PydObj* d, const std::string& key, PydObj* v){
    if (!d) return;
    if (!d->dictItems) d->dictItems = new PydDictItems();
    for (auto& kv : *d->dictItems){
        if (kv.first && kv.first->kind == PK_STR && kv.first->sval == key){
            kv.second = v;
            return;
        }
    }
    d->dictItems->push_back({pydNewStr(key), v});
}

// pyd Str Of
// pyd str of
// pyd Str Of
static std::string pydStrOf(PydObj* o){
    if (!o) return "None";
    switch (o->kind){
        case PK_NONE: return "None";
        case PK_BOOL: return o->ival ? "True" : "False";
        case PK_INT: return std::to_string(o->ival);
        case PK_FLOAT: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", o->dval);
            return buf;
        }
        case PK_STR: case PK_BYTES: return o->sval;
        case PK_OPAQUE: return o->name;
        default: return "<" + std::string(g_pydKinds[o->kind] ? g_pydKinds[o->kind]->name : "object") + " object>";
    }
}

// pyd Repr Of
// pyd repr of
// pyd Repr Of
static std::string pydReprOf(PydObj* o){
    if (!o) return "None";
    if (o->kind == PK_STR) return "'" + o->sval + "'";
    return pydStrOf(o);
}

// pyd As Long Long
// pyd as long long
// pyd As Long Long
static long long pydAsLongLong(PydObj* o, bool* ok){
    if (ok) *ok = true;
    if (!o){ if (ok) *ok = false; return 0; }
    if (o->kind == PK_BOOL || o->kind == PK_INT) return o->ival;
    if (o->kind == PK_FLOAT) return static_cast<long long>(o->dval);
    if (ok) *ok = false;
    return 0;
}

// pyd 字节串 Of
// pyd bytes of
// pyd ᠪᠢᠲ᠋ᠢ ᠦᠰᠦᠭ Of
static PydObj* pydBytesOf(PydObj* o){

    if (o && (o->kind == PK_BYTES || o->kind == PK_STR)) return o;
    return nullptr;
}


// pyd 集合 错误
// pyd set error
// pyd ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ ᠲᠠᠯᠳ᠋ᠠᠭᠤᠯᠭ᠎ᠠ
static void pydSetError(PydObj* excType, const std::string& msg){
    g_pydErrorType = excType;
    g_pydErrorMsg = msg;
}

// pyd Format V
// pyd format v
// pyd Format V
static std::string pydFormatV(const char* fmt, va_list ap){
    std::string out;
    for (const char* p = fmt; p && *p; ++p){
        if (*p != '%'){ out.push_back(*p); continue; }
        ++p;
        if (!*p) break;
        std::string lenMod;
        while (*p && (*p == 'l' || *p == 'z' || *p == 'h' || *p == 'q' || *p == 'j' || *p == 't')){
            lenMod.push_back(*p);
            ++p;
        }
        char conv = *p;
        switch (conv){
            case 's': {
                const char* s = va_arg(ap, const char*);
                out += s ? s : "(null)";
                break;
            }
            case 'S': {
                PydObj* o = va_arg(ap, PydObj*);
                out += pydStrOf(o);
                break;
            }
            case 'R': case 'A': {
                PydObj* o = va_arg(ap, PydObj*);
                out += pydReprOf(o);
                break;
            }
            case 'd': case 'i': {
                long long v;
                if (lenMod == "ll" || lenMod == "z" || lenMod == "j" || lenMod == "q" || lenMod == "t")
                    v = va_arg(ap, long long);
                else if (lenMod == "l")
                    v = va_arg(ap, long);
                else
                    v = va_arg(ap, int);
                out += std::to_string(v);
                break;
            }
            case 'u': {
                unsigned long long v;
                if (lenMod == "ll" || lenMod == "z" || lenMod == "j" || lenMod == "q" || lenMod == "t")
                    v = va_arg(ap, unsigned long long);
                else if (lenMod == "l")
                    v = va_arg(ap, unsigned long);
                else
                    v = static_cast<unsigned long long>(va_arg(ap, unsigned int));
                out += std::to_string(v);
                break;
            }
            case 'x': {
                unsigned long long v = va_arg(ap, unsigned long long);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%llx", v);
                out += buf;
                break;
            }
            case 'p': {
                void* v = va_arg(ap, void*);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%p", v);
                out += buf;
                break;
            }
            case '%': out.push_back('%'); break;
            default: out.push_back('%'); out.push_back(conv); break;
        }
    }
    return out;
}


// Pyd Py 模块 Def Init
// pyd py module def init
// Pyd Py ᠦᠪᠲ᠋ᠠᠢ Def Init
static void* Pyd_PyModuleDef_Init(void* def){

    if (!def) return nullptr;
    ((PydObj*)def)->ob_type = (void*)&g_pydTypeModuleDef;
    return def;
}

// Pyd Py 模块 加 整数 常量
// pyd py module add int constant
// Pyd Py ᠦᠪᠲ᠋ᠠᠢ ᠨᠡᠮᠡᠵᠡ ᠳ᠋ᠠᠩ ᠲᠣ᠎ ᠲᠣᠬᠲᠠᠭᠤᠯᠭ᠎ᠠ
static int Pyd_PyModule_AddIntConstant(void* module, const char* name, long v){
    PydObj* m = static_cast<PydObj*>(module);
    if (!m || m->kind != PK_MODULE) return -1;
    pydDictSet(m, name ? name : "", pydNewInt(v));
    return 0;
}

// Pyd Py 模块 加 字符串 常量
// pyd py module add string constant
// Pyd Py ᠦᠪᠲ᠋ᠠᠢ ᠨᠡᠮᠡᠵᠡ ᠦᠰᠦᠭ ᠲᠣᠬᠲᠠᠭᠤᠯᠭ᠎ᠠ
static int Pyd_PyModule_AddStringConstant(void* module, const char* name, const char* v){
    PydObj* m = static_cast<PydObj*>(module);
    if (!m || m->kind != PK_MODULE) return -1;
    pydDictSet(m, name ? name : "", pydNewStr(v ? v : ""));
    return 0;
}

// Pyd Py 模块 加 对象
// pyd py module add object
// Pyd Py ᠦᠪᠲ᠋ᠠᠢ ᠨᠡᠮᠡᠵᠡ ᠣᠪᠶᠡᠭᠲ
static int Pyd_PyModule_AddObject(void* module, const char* name, void* obj){
    PydObj* m = static_cast<PydObj*>(module);
    if (!m || m->kind != PK_MODULE || !obj) return -1;
    pydDictSet(m, name ? name : "", static_cast<PydObj*>(obj));
    return 0;
}

// Pyd Py 模块 加 对象 Ref
// pyd py module add object ref
// Pyd Py ᠦᠪᠲ᠋ᠠᠢ ᠨᠡᠮᠡᠵᠡ ᠣᠪᠶᠡᠭᠲ Ref
static int Pyd_PyModule_AddObjectRef(void* module, const char* name, void* obj){
    return Pyd_PyModule_AddObject(module, name, obj);
}

// Pyd Py Err 集合 字符串
// pyd py err set string
// Pyd Py Err ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ ᠦᠰᠦᠭ
static void Pyd_PyErr_SetString(void* excType, const char* msg){
    pydSetError(static_cast<PydObj*>(excType), msg ? msg : "");
}

// Pyd Py Err Format
// pyd py err format
// Pyd Py Err Format
static void Pyd_PyErr_Format(void* excType, const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    std::string msg = pydFormatV(fmt, ap);
    va_end(ap);
    pydSetError(static_cast<PydObj*>(excType), msg);
}

// Pyd Py Err 集合 Exc 从 Windows Err
// pyd py err set exc from windows err
// Pyd Py Err ᠨᠢᠳᠡᠳᠡᠯ ᠰᠡᠲ Exc ᠡᠴᠡ Windows Err
static void Pyd_PyErr_SetExcFromWindowsErr(void* excType, int ierr){
// 未显式给出错误码时读取当前平台的系统错误码
// Read the platform system error code when none is supplied
// ᠲᠣᠭ᠎ᠠ ᠦᠭᠡᠢ ᠪᠣᠯᠤᠭᠰᠠᠨ ᠳᠤ ᠲᠠᠯᠠᠪᠠᠢ ᠶᠢᠨ ᠠᠯᠳᠠᠭ᠎ᠠ ᠤᠨ ᠳᠤᠭ᠎ᠠ ᠢ ᠤᠩᠰᠢᠨ᠎ᠠ
    int code = ierr ? ierr : static_cast<int>(mpLastError());
    pydSetError(static_cast<PydObj*>(excType), "Windows error " + std::to_string(code));
}

// Pyd Dec Ref
// pyd dec ref
// Pyd Dec Ref
static void Pyd_DecRef(void* op){
    PydObj* o = static_cast<PydObj*>(op);
    if (!o) return;

    if (o->ob_refcnt > 0) o->ob_refcnt--;
}

// Pyd Py Mem 自由
// pyd py mem free
// Pyd Py Mem ᠴᠡᠯᠡᠭᠡᠨ᠎ᠡ
static void Pyd_PyMem_Free(void* p){
    std::free(p);
}

// Raw Py 缓冲区
// raw py buffer
// Raw Py ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
struct RawPyBuffer{
    void* buf;
    void* obj;
    long long len;
    long long itemsize;
    int readonly;
    int ndim;
    char* format;
    long long* shape;
    long long* strides;
    long long* suboffsets;
    void* internal;
};

// Pyd Py 对象 获取 缓冲区
// pyd py object get buffer
// Pyd Py ᠣᠪᠶᠡᠭᠲ ᠬᠠᠯᠠᠬᠤ ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ
static int Pyd_PyObject_GetBuffer(void* obj, RawPyBuffer* view, int flags){
    (void)flags;
    PydObj* o = static_cast<PydObj*>(obj);
    PydObj* bytes = pydBytesOf(o);
    if (!bytes || !view){
        pydSetError(g_pydExcTypeError, "a bytes-like object is required");
        return -1;
    }
    view->buf = const_cast<char*>(bytes->sval.c_str());
    view->obj = bytes;
    bytes->ob_refcnt++;
    view->len = static_cast<long long>(bytes->sval.size());
    view->itemsize = 1;
    view->readonly = 1;
    view->ndim = 1;
    view->format = const_cast<char*>("B");
    view->shape = nullptr;
    view->strides = nullptr;
    view->suboffsets = nullptr;
    view->internal = nullptr;
    return 0;
}

// Pyd Py 缓冲区 减引用
// pyd py buffer release
// Pyd Py ᠪᠣᠯᠣᠭᠤᠨ᠎ᠠ ᠲᠡᠢᠶᠡᠨ ᠬᠠᠯᠠᠬᠤ
static void Pyd_PyBuffer_Release(RawPyBuffer* view){
    if (!view) return;
    if (view->obj) Pyd_DecRef(view->obj);
    view->buf = nullptr;
    view->obj = nullptr;
    view->len = 0;
}

// Pyd Py Unicode As Wide Char 字符串
// pyd py unicode as wide char string
// Pyd Py Unicode As Wide Char ᠦᠰᠦᠭ
static wchar_t* Pyd_PyUnicode_AsWideCharString(void* obj, size_t* size){
    PydObj* o = static_cast<PydObj*>(obj);
    if (!o || (o->kind != PK_STR && o->kind != PK_BYTES)){
        pydSetError(g_pydExcTypeError, "expected str");
        return nullptr;
    }
    std::wstring wide = utf8ToWide(o->sval);
    wchar_t* buffer = static_cast<wchar_t*>(std::malloc((wide.size() + 1) * sizeof(wchar_t)));
    if (!buffer) return nullptr;
    std::memcpy(buffer, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
    if (size) *size = wide.size();
    return buffer;
}

// Pyd Py O S F S Path
// pyd py o s f s path
// Pyd Py O S F S Path
static void* Pyd_PyOS_FSPath(void* obj){
    PydObj* o = static_cast<PydObj*>(obj);
    if (o && (o->kind == PK_STR || o->kind == PK_BYTES)){
        o->ob_refcnt++;
        return o;
    }
    pydSetError(g_pydExcTypeError, "expected str, bytes or os.PathLike object");
    return nullptr;
}

// Pyd Py 类型 获取 Flags
// pyd py type get flags
// Pyd Py ᠲᠥᠷᠥᠯ ᠬᠠᠯᠠᠬᠤ Flags
static unsigned long Pyd_PyType_GetFlags(PydTypeObj* type){
    return type ? type->flags : 0;
}

// Pyd Py 类型 获取 Qual 标识符
// pyd py type get qual name
// Pyd Py ᠲᠥᠷᠥᠯ ᠬᠠᠯᠠᠬᠤ Qual ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
static PydObj* Pyd_PyType_GetQualName(PydTypeObj* type){
    return pydNewStr(type && type->name ? type->name : "object");
}

// Pyd Py 获取 常量 Borrowed
// pyd py get constant borrowed
// Pyd Py ᠬᠠᠯᠠᠬᠤ ᠲᠣᠬᠲᠠᠭᠤᠯᠭ᠎ᠠ Borrowed
static void* Pyd_Py_GetConstantBorrowed(int constantId){
    (void)constantId;
    return g_pydNone;
}

// Pyd Py Eval Save Thread
// pyd py eval save thread
// Pyd Py Eval Save Thread
static void* Pyd_PyEval_SaveThread(void){
    return nullptr;
}

// Pyd Py Eval Restore Thread
// pyd py eval restore thread
// Pyd Py Eval Restore Thread
static void Pyd_PyEval_RestoreThread(void* state){
    (void)state;
}


static int Pyd_PyArg_ParseTupleAndKeywords(void* argsObj, void* kwObj, const char* format,

                                           char** kwlist, ...){
    PydObj* args = static_cast<PydObj*>(argsObj);
    PydObj* kw = static_cast<PydObj*>(kwObj);
    std::vector<PydObj*> positional;
    if (args && args->kind == PK_TUPLE && args->items) positional = *args->items;
    size_t posIndex = 0;
    int kwIndex = 0;
    bool optional = false;
    bool hasError = false;
    if (!format) format = "";
    va_list ap;
    va_start(ap, kwlist);
    for (const char* p = format; *p; ++p){
        char c = *p;
        if (c == '|'){ optional = true; continue; }
        if (c == ' ' || c == '$' || c == '\t') continue;
        if (c == ':' || c == ';') break;
        std::string kwName = (kwlist && kwlist[kwIndex]) ? kwlist[kwIndex] : "";
        kwIndex++;
        PydObj* obj = nullptr;
        if (kw && !kwName.empty()) obj = pydDictGet(kw, kwName);
        if (!obj && posIndex < positional.size()) obj = positional[posIndex++];
        if (!obj){
            if (optional) continue;
            pydSetError(g_pydExcTypeError,
                        "missing required argument '" + (kwName.empty() ? std::string("?") : kwName) + "'");
            hasError = true;
            break;
        }
        switch (c){
            case 'O':{
                PydObj** out = va_arg(ap, PydObj**);
                *out = obj;
                break;
            }
            case 'i':{
                int* out = va_arg(ap, int*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<int>(v);
                break;
            }
            case 'I':{
                unsigned int* out = va_arg(ap, unsigned int*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<unsigned int>(v);
                break;
            }
            case 'l':{
                long* out = va_arg(ap, long*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<long>(v);
                break;
            }
            case 'k':{
                unsigned long* out = va_arg(ap, unsigned long*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<unsigned long>(v);
                break;
            }
            case 'L':{
                long long* out = va_arg(ap, long long*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = v;
                break;
            }
            case 'K':{
                unsigned long long* out = va_arg(ap, unsigned long long*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<unsigned long long>(v);
                break;
            }
            case 'b':{
                signed char* out = va_arg(ap, signed char*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<signed char>(v);
                break;
            }
            case 'B':{
                unsigned char* out = va_arg(ap, unsigned char*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<unsigned char>(v);
                break;
            }
            case 'h':{
                short* out = va_arg(ap, short*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<short>(v);
                break;
            }
            case 'H':{
                unsigned short* out = va_arg(ap, unsigned short*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = static_cast<unsigned short>(v);
                break;
            }
            case 'n':{
                long long* out = va_arg(ap, long long*);
                bool ok = false;
                long long v = pydAsLongLong(obj, &ok);
                if (!ok){ pydSetError(g_pydExcTypeError, "an integer is required"); hasError = true; }                 else *out = v;
                break;
            }
            case 'd':{
                double* out = va_arg(ap, double*);
                if (obj->kind == PK_FLOAT) *out = obj->dval;
                else if (obj->kind == PK_INT || obj->kind == PK_BOOL) *out = static_cast<double>(obj->ival);
                else { pydSetError(g_pydExcTypeError, "a float is required"); hasError = true; }
                break;
            }
            case 'f':{
                float* out = va_arg(ap, float*);
                if (obj->kind == PK_FLOAT) *out = static_cast<float>(obj->dval);
                else if (obj->kind == PK_INT || obj->kind == PK_BOOL) *out = static_cast<float>(obj->ival);
                else { pydSetError(g_pydExcTypeError, "a float is required"); hasError = true; }
                break;
            }
            case 'p':{
                int* out = va_arg(ap, int*);
                *out = (obj->kind == PK_BOOL) ? (obj->ival ? 1 : 0)
                       : (obj->kind == PK_NONE ? 0 : 1);
                break;
            }
            case 's':{
                const char** out = va_arg(ap, const char**);
                if (obj->kind == PK_STR || obj->kind == PK_BYTES) *out = obj->sval.c_str();
                else { pydSetError(g_pydExcTypeError, "a str is required"); hasError = true; }
                break;
            }
            case 'z':{
                const char** out = va_arg(ap, const char**);
                if (obj->kind == PK_NONE) *out = nullptr;
                else if (obj->kind == PK_STR || obj->kind == PK_BYTES) *out = obj->sval.c_str();
                else { pydSetError(g_pydExcTypeError, "a str is required"); hasError = true; }
                break;
            }
            case 'u':{
                PydObj** out = va_arg(ap, PydObj**);
                if (obj->kind == PK_STR) *out = obj;
                else { pydSetError(g_pydExcTypeError, "a str is required"); hasError = true; }
                break;
            }
            case 'U':{
                PydObj** out = va_arg(ap, PydObj**);
                *out = obj;
                break;
            }
            default:

                pydSetError(g_pydExcTypeError, "unsupported argument format");
                hasError = true;
                break;
        }
        if (hasError) break;
    }
    va_end(ap);
    return hasError ? 0 : 1;
}


// pyd Ensure Init
// pyd ensure init
// pyd Ensure Init
static void pydEnsureInit(){
    if (g_pydInited) return;
    g_pydInited = true;
    g_pydKinds[PK_NONE] = &g_pydTypeNone;
    g_pydKinds[PK_BOOL] = &g_pydTypeBool;
    g_pydKinds[PK_INT] = &g_pydTypeInt;
    g_pydKinds[PK_FLOAT] = &g_pydTypeFloat;
    g_pydKinds[PK_STR] = &g_pydTypeStr;
    g_pydKinds[PK_BYTES] = &g_pydTypeBytes;
    g_pydKinds[PK_TUPLE] = &g_pydTypeTuple;
    g_pydKinds[PK_LIST] = &g_pydTypeList;
    g_pydKinds[PK_DICT] = &g_pydTypeDict;
    g_pydKinds[PK_MODULE] = &g_pydTypeModule;
    g_pydKinds[PK_OPAQUE] = &g_pydTypeNone;
    g_pydNone = pydNewObj(PK_NONE);
    g_pydNone->ob_type = (void*)&g_pydTypeNone;
    g_pydTrue = pydNewObj(PK_BOOL);
    g_pydTrue->ival = 1;
    g_pydFalse = pydNewObj(PK_BOOL);
    g_pydFalse->ival = 0;
    g_pydExcTypeError = pydNewExcType("TypeError");
    g_pydExcValueError = pydNewExcType("ValueError");
    g_pydExcRuntimeError = pydNewExcType("RuntimeError");
    g_pydExcOSError = pydNewExcType("OSError");
    g_pydExcMemoryError = pydNewExcType("MemoryError");
    g_pydExcOverflowError = pydNewExcType("OverflowError");
    g_pydExcImportError = pydNewExcType("ImportError");

    g_symPyExc_TypeError = g_pydExcTypeError;
    g_symPyExc_ValueError = g_pydExcValueError;
    g_symPyExc_RuntimeError = g_pydExcRuntimeError;
    g_symPyExc_OSError = g_pydExcOSError;
    g_symPyExc_MemoryError = g_pydExcMemoryError;
    g_symPyExc_OverflowError = g_pydExcOverflowError;
    g_symPyExc_ImportError = g_pydExcImportError;
    g_pydShimExports["PyExc_TypeError"] = (void*)&g_symPyExc_TypeError;
    g_pydShimExports["PyExc_ValueError"] = (void*)&g_symPyExc_ValueError;
    g_pydShimExports["PyExc_RuntimeError"] = (void*)&g_symPyExc_RuntimeError;
    g_pydShimExports["PyExc_OSError"] = (void*)&g_symPyExc_OSError;
    g_pydShimExports["PyExc_MemoryError"] = (void*)&g_symPyExc_MemoryError;
    g_pydShimExports["PyExc_OverflowError"] = (void*)&g_symPyExc_OverflowError;
    g_pydShimExports["PyExc_ImportError"] = (void*)&g_symPyExc_ImportError;

    g_pydShimExports["PyExc_WindowsError"] = (void*)&g_symPyExc_OSError;

    g_pydShimExports["PyModuleDef_Init"] = (void*)&Pyd_PyModuleDef_Init;
    g_pydShimExports["PyModule_AddIntConstant"] = (void*)&Pyd_PyModule_AddIntConstant;
    g_pydShimExports["PyModule_AddStringConstant"] = (void*)&Pyd_PyModule_AddStringConstant;
    g_pydShimExports["PyModule_AddObject"] = (void*)&Pyd_PyModule_AddObject;
    g_pydShimExports["PyModule_AddObjectRef"] = (void*)&Pyd_PyModule_AddObjectRef;
    g_pydShimExports["PyErr_SetString"] = (void*)&Pyd_PyErr_SetString;
    g_pydShimExports["PyErr_Format"] = (void*)&Pyd_PyErr_Format;
    g_pydShimExports["PyErr_SetExcFromWindowsErr"] = (void*)&Pyd_PyErr_SetExcFromWindowsErr;
    g_pydShimExports["_Py_DecRef"] = (void*)&Pyd_DecRef;
    g_pydShimExports["Py_DecRef"] = (void*)&Pyd_DecRef;
    g_pydShimExports["PyMem_Free"] = (void*)&Pyd_PyMem_Free;
    g_pydShimExports["PyObject_GetBuffer"] = (void*)&Pyd_PyObject_GetBuffer;
    g_pydShimExports["PyBuffer_Release"] = (void*)&Pyd_PyBuffer_Release;
    g_pydShimExports["PyUnicode_AsWideCharString"] = (void*)&Pyd_PyUnicode_AsWideCharString;
    g_pydShimExports["PyOS_FSPath"] = (void*)&Pyd_PyOS_FSPath;
    g_pydShimExports["PyType_GetFlags"] = (void*)&Pyd_PyType_GetFlags;
    g_pydShimExports["PyType_GetQualName"] = (void*)&Pyd_PyType_GetQualName;
    g_pydShimExports["Py_GetConstantBorrowed"] = (void*)&Pyd_Py_GetConstantBorrowed;
    g_pydShimExports["PyEval_SaveThread"] = (void*)&Pyd_PyEval_SaveThread;
    g_pydShimExports["PyEval_RestoreThread"] = (void*)&Pyd_PyEval_RestoreThread;
    g_pydShimExports["PyArg_ParseTupleAndKeywords"] = (void*)&Pyd_PyArg_ParseTupleAndKeywords;
}


// pyd 从 虚拟机
// pyd from vm
// pyd ᠡᠴᠡ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ
static PydObj* pydFromVm(vm& vm, const value& v){
    (void)vm;
    if (v.type == "none" || v.ptr == nullptr) return g_pydNone;
    if (v.type == "bool") return pydNewBool(*static_cast<bool*>(v.ptr));
    if (v.type == "int") return pydNewInt(*static_cast<long long*>(v.ptr));
    if (v.type == "float") return pydNewFloat(*static_cast<double*>(v.ptr));
    if (v.type == "str" || v.type == "bytes"){
        PydObj* o = pydNewObj(v.type == "bytes" ? PK_BYTES : PK_STR);
        o->sval = *static_cast<std::string*>(v.ptr);
        return o;
    }
    if (v.type == "list" || v.type == "tuple"){
        PydObj* o = pydNewObj(v.type == "list" ? PK_LIST : PK_TUPLE);
        o->items = new std::vector<PydObj*>();
        auto* source = static_cast<std::vector<value>*>(v.ptr);
        for (auto& item : *source) o->items->push_back(pydFromVm(vm, item));
        return o;
    }
    if (v.type == "dict"){
        PydObj* o = pydNewDict();
        auto* source = static_cast<dict_t*>(v.ptr);
        for (auto& kv : *source) o->dictItems->push_back({pydFromVm(vm, kv.first), pydFromVm(vm, kv.second)});
        return o;
    }

    PydObj* o = pydNewObj(PK_OPAQUE);
    o->name = vm.pythonTypeName(v);
    return o;
}

// pyd 转 虚拟机
// pyd to vm
// pyd ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ
static value pydToVm(vm& vm, PydObj* obj){
    if (!obj) return vm.makeNone();
    switch (obj->kind){
        case PK_NONE: return vm.makeNone();
        case PK_BOOL: return vm.makeBool(obj->ival != 0);
        case PK_INT: return vm.makeInt(obj->ival);
        case PK_FLOAT: return vm.makeFloat(obj->dval);
        case PK_STR: case PK_BYTES: return vm.makeString(obj->sval);
        case PK_TUPLE: case PK_LIST: {
            std::vector<value> out;
            if (obj->items) for (auto* item : *obj->items) out.push_back(pydToVm(vm, item));
            return obj->kind == PK_TUPLE ? vm.makeTuple(out) : vm.makeList(out);
        }
        case PK_DICT: {
            std::vector<std::pair<value, value>> out;
            if (obj->dictItems) for (auto& kv : *obj->dictItems) out.push_back({pydToVm(vm, kv.first), pydToVm(vm, kv.second)});
            return vm.makeDict(out);
        }
        default:
            return vm.makeNone();
    }
}


static PydObj* pydInvokeCallable(PydObj* fn, PydObj* self,

                                 std::vector<PydObj*>& args,

                                 std::vector<std::pair<std::string, PydObj*>>& kwargs){
    if (!fn || !fn->fnPtr) return nullptr;
    PydObj* tuple = pydNewTuple();
    *tuple->items = args;
    PydObj* kwdict = nullptr;
    if (!kwargs.empty()){
        kwdict = pydNewDict();
        for (auto& kv : kwargs) kwdict->dictItems->push_back({pydNewStr(kv.first), kv.second});
    }
    int flags = fn->fnFlags;
    void* f = fn->fnPtr;

    if (flags & 0x0002){
        return ((PydObj* (*)(PydObj*, PydObj*, PydObj*))f)(self, tuple, kwdict);
    }
    if (flags & 0x0080){
        return ((PydObj* (*)(PydObj*, PydObj* const*, long long))f)(
            self, args.empty() ? nullptr : args.data(), static_cast<long long>(args.size()));
    }
    if (flags & 0x0001){
        return ((PydObj* (*)(PydObj*, PydObj*))f)(self, tuple);
    }
    if (flags & 0x0004){
        return ((PydObj* (*)(PydObj*, PydObj*))f)(self, nullptr);
    }
    if (flags & 0x0008){
        return ((PydObj* (*)(PydObj*, PydObj*))f)(self, args.empty() ? g_pydNone : args[0]);
    }
    return nullptr;
}


// Pyd Image Info
// pyd image info
// Pyd Image Info
struct PydImageInfo{
    void* base = nullptr;
    size_t size = 0;
};

// pyd 获取 Export Proc
// pyd get export proc
// pyd ᠬᠠᠯᠠᠬᠤ Export Proc
static void* pydGetExportProc(void* base, const char* procName){
    if (!base || !procName) return nullptr;
#if defined(MINIPY_OS_WINDOWS)
    unsigned char* image = static_cast<unsigned char*>(base);
    int lfanew = *reinterpret_cast<int*>(image + 0x3c);
    unsigned char* nt = image + lfanew;
    unsigned char* opt = nt + 24;
    uint32_t exportRva = *reinterpret_cast<uint32_t*>(opt + 112);
    uint32_t exportSize = *reinterpret_cast<uint32_t*>(opt + 116);
    if (!exportRva || !exportSize) return nullptr;
    unsigned char* expDir = image + exportRva;
    uint32_t numNames = *reinterpret_cast<uint32_t*>(expDir + 24);
    uint32_t funcsRva = *reinterpret_cast<uint32_t*>(expDir + 28);
    uint32_t namesRva = *reinterpret_cast<uint32_t*>(expDir + 32);
    uint32_t ordinalsRva = *reinterpret_cast<uint32_t*>(expDir + 36);
    uint32_t* funcs = reinterpret_cast<uint32_t*>(image + funcsRva);
    uint32_t* names = reinterpret_cast<uint32_t*>(image + namesRva);
    uint16_t* ordinals = reinterpret_cast<uint16_t*>(image + ordinalsRva);
    for (uint32_t i = 0; i < numNames; ++i){
        const char* name = reinterpret_cast<const char*>(image + names[i]);
        if (std::strcmp(name, procName) == 0){
            uint16_t ordinal = ordinals[i];
            uint32_t funcRva = funcs[ordinal];
            if (funcRva >= exportRva && funcRva < exportRva + exportSize){
                return nullptr;
            }
            return image + funcRva;
        }
    }
    return nullptr;
#else
    return mpGetProcAddress(reinterpret_cast<mp_module>(base), procName);
#endif
}

#if defined(MINIPY_OS_WINDOWS)
static bool pydMapImage(const std::vector<unsigned char>& data, std::string& error,

                        PydImageInfo& info, const std::string& moduleName){
    (void)moduleName;
    auto fail = [&](const std::string& message){
        error = message;
        return false;
    };
    if (data.size() < 0x40) return fail("file too small");
    unsigned char* raw = const_cast<unsigned char*>(data.data());
    int lfanew = *reinterpret_cast<int*>(raw + 0x3c);
    if (lfanew <= 0 || static_cast<size_t>(lfanew) + 24 > data.size()) return fail("bad DOS header");
    unsigned char* nt = raw + lfanew;
    if (std::memcmp(nt, "PE\0\0", 4) != 0) return fail("not a PE file");
    uint16_t machine = *reinterpret_cast<uint16_t*>(nt + 4);
    uint16_t numSections = *reinterpret_cast<uint16_t*>(nt + 6);
    uint16_t optSize = *reinterpret_cast<uint16_t*>(nt + 20);
    unsigned char* opt = nt + 24;
    uint16_t magic = *reinterpret_cast<uint16_t*>(opt);
    if (machine != 0x8664) return fail("only x64 pyd is supported");
    if (magic != 0x20b) return fail("not a PE32+ image");
    uint32_t entryRva = *reinterpret_cast<uint32_t*>(opt + 16);
    uint64_t imageBase = *reinterpret_cast<uint64_t*>(opt + 24);
    uint32_t sizeOfImage = *reinterpret_cast<uint32_t*>(opt + 56);
    uint32_t sizeOfHeaders = *reinterpret_cast<uint32_t*>(opt + 60);
    uint32_t importRva = *reinterpret_cast<uint32_t*>(opt + 120);
    uint32_t importSize = *reinterpret_cast<uint32_t*>(opt + 124);
    uint32_t relocRva = *reinterpret_cast<uint32_t*>(opt + 152);
    uint32_t relocSize = *reinterpret_cast<uint32_t*>(opt + 156);
    if (sizeOfImage == 0) return fail("bad image size");
    unsigned char* sections = opt + optSize;

// Section Info
// section info
// Section Info
    struct SectionInfo{
        uint32_t va = 0;
        uint32_t vsize = 0;
        uint32_t rawSize = 0;
        uint32_t rawPtr = 0;
        uint32_t chars = 0;
    };
    std::vector<SectionInfo> sectionList;
    for (uint16_t i = 0; i < numSections; ++i){
        unsigned char* s = sections + i * 40;
        SectionInfo si;
        si.vsize = *reinterpret_cast<uint32_t*>(s + 8);
        si.va = *reinterpret_cast<uint32_t*>(s + 12);
        si.rawSize = *reinterpret_cast<uint32_t*>(s + 16);
        si.rawPtr = *reinterpret_cast<uint32_t*>(s + 20);
        si.chars = *reinterpret_cast<uint32_t*>(s + 36);
        sectionList.push_back(si);
    }

    void* base = VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(imageBase)),
                              sizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!base){
        base = VirtualAlloc(nullptr, sizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!base) return fail("VirtualAlloc failed");
    unsigned char* image = static_cast<unsigned char*>(base);
    uint64_t delta = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(base)) - imageBase;

    size_t headerSize = sizeOfHeaders < data.size() ? sizeOfHeaders : data.size();
    std::memcpy(image, raw, headerSize);
    for (auto& si : sectionList){
        if (!si.rawSize || !si.rawPtr) continue;
        if (si.va + si.rawSize > sizeOfImage) continue;
        size_t copySize = si.rawSize;
        if (si.rawPtr + copySize > data.size()) copySize = data.size() - si.rawPtr;
        std::memcpy(image + si.va, raw + si.rawPtr, copySize);
    }

    if (delta != 0 && relocRva && relocSize){
        uint32_t cursor = 0;
        while (cursor < relocSize){
            unsigned char* block = image + relocRva + cursor;
            uint32_t pageRva = *reinterpret_cast<uint32_t*>(block);
            uint32_t blockSize = *reinterpret_cast<uint32_t*>(block + 4);
            if (blockSize < 8) break;
            for (uint32_t k = 8; k + 1 < blockSize; k += 2){
                uint16_t entry = *reinterpret_cast<uint16_t*>(block + k);
                uint16_t type = entry >> 12;
                uint16_t offset = entry & 0x0fff;
                unsigned char* target = image + pageRva + offset;
                if (type == 10){
                    *reinterpret_cast<uint64_t*>(target) += delta;
                } else if (type == 3){
                    *reinterpret_cast<uint32_t*>(target) += static_cast<uint32_t>(delta);
                }
            }
            cursor += blockSize;
        }
    }

    if (importRva && importSize){
        unsigned char* descriptor = image + importRva;
        while (true){
            uint32_t originalFirstThunk = *reinterpret_cast<uint32_t*>(descriptor);
            uint32_t nameRva = *reinterpret_cast<uint32_t*>(descriptor + 12);
            uint32_t firstThunk = *reinterpret_cast<uint32_t*>(descriptor + 16);
            if (!nameRva && !firstThunk) break;
            const char* dllName = reinterpret_cast<const char*>(image + nameRva);
            std::string dll = dllName ? dllName : "";
            bool isPython = (dll.find("python3") != std::string::npos);
            HMODULE systemModule = nullptr;
            if (!isPython){
                systemModule = LoadLibraryW(utf8ToWide(dll).c_str());
                if (!systemModule){
                    VirtualFree(base, 0, MEM_RELEASE);
                    return fail("cannot load dependency '" + dll + "'");
                }
            }
            uint32_t thunkRva = originalFirstThunk ? originalFirstThunk : firstThunk;
            uint32_t iatRva = firstThunk;
            while (true){
                uint64_t thunk = *reinterpret_cast<uint64_t*>(image + thunkRva);
                if (!thunk) break;
                void* proc = nullptr;
                if (isPython){
                    if (!(thunk & 0x8000000000000000ULL)){
                        const char* funcName = reinterpret_cast<const char*>(image + static_cast<uint32_t>(thunk) + 2);
                        auto it = g_pydShimExports.find(funcName);
                        if (it != g_pydShimExports.end()) proc = it->second;
                    }
                } else {
                    if (thunk & 0x8000000000000000ULL){
                        proc = reinterpret_cast<void*>(GetProcAddress(
                            systemModule, MAKEINTRESOURCEA(static_cast<uint16_t>(thunk & 0xffff))));
                    } else {
                        const char* funcName = reinterpret_cast<const char*>(image + static_cast<uint32_t>(thunk) + 2);
                        proc = reinterpret_cast<void*>(GetProcAddress(systemModule, funcName));
                    }
                }
                if (!proc){
                    std::string missing = (thunk & 0x8000000000000000ULL)
                        ? ("ordinal " + std::to_string(thunk & 0xffff))
                        : reinterpret_cast<const char*>(image + static_cast<uint32_t>(thunk) + 2);
                    VirtualFree(base, 0, MEM_RELEASE);
                    return fail("unresolved import '" + missing + "' from '" + dll + "'");
                }
                *reinterpret_cast<uint64_t*>(image + iatRva) = reinterpret_cast<uint64_t>(proc);
                thunkRva += 8;
                iatRva += 8;
            }
            descriptor += 20;
        }
    }

    for (auto& si : sectionList){
        if (!si.vsize) continue;
        DWORD protection = PAGE_READONLY;
        if (si.chars & 0x20000000) protection = PAGE_EXECUTE_READ;
        else if (si.chars & 0x80000000) protection = PAGE_READWRITE;
        DWORD oldProtect = 0;
        VirtualProtect(image + si.va, si.vsize, protection, &oldProtect);
    }

    if (entryRva){
        typedef BOOL (WINAPI* DllEntryProc)(void*, DWORD, void*);
        DllEntryProc entry = reinterpret_cast<DllEntryProc>(image + entryRva);
        entry(base, 1 , nullptr);
    }
    info.base = base;
    info.size = sizeOfImage;
    return true;
}
#else
// 非 Windows 平台把扩展模块字节写入临时文件后交给 dlopen, 符号由系统链接器解析
// On non-Windows platforms the module bytes go to a temporary file and are handed to
// dlopen, leaving symbol resolution to the system loader
// Windows ᠪᠣᠰᠤ ᠲᠠᠯᠠᠪᠠᠢ ᠳᠤ ᠨᠡᠮᠡᠯᠲᠡ ᠮᠣᠳᠤᠯᠢ ᠶᠢ ᠲᠡᠮᠳᠡᠭ ᠹᠠᠢᠯᠳᠤ ᠪᠢᠴᠢᠭᠡᠳ dlopen ᠢᠶᠠᠷ ᠲᠠᠲᠠᠨ᠎ᠠ
static bool pydMapImage(const std::vector<unsigned char>& data, std::string& error,
                        PydImageInfo& info, const std::string& moduleName){
#if defined(__wasi__)
    // WASI 不支持动态加载, 直接拒绝扩展模块
    // WASI has no dynamic loading, reject extension modules
    // WASI ᠳᠤ ᠨᠡᠮᠡᠯᠲᠡ ᠮᠣᠳᠤᠯᠢ ᠲᠠᠲᠠᠬᠤ ᠦᠭᠡᠢ
    (void)data;
    (void)info;
    (void)moduleName;
    error = "dynamic loading is not supported on WASI";
    return false;
#else
    if (data.empty()){
        error = "empty module file";
        return false;
    }
    std::string tempDir = mpTempDirectory();
    if (tempDir.empty()) tempDir = "/tmp";
    static unsigned int sequence = 0;
    std::string fileName = "minipy_ext_" + std::to_string(static_cast<long long>(::getpid())) +
                           "_" + std::to_string(sequence++) + "_" + moduleName + ".so";
    std::string tempPath = tempDir;
    if (!tempPath.empty() && tempPath.back() != '/'){
        tempPath.push_back('/');
    }
    tempPath += fileName;
    FILE* fp = std::fopen(tempPath.c_str(), "wb");
    if (!fp){
        error = "cannot create temporary module file";
        return false;
    }
    size_t written = std::fwrite(data.data(), 1, data.size(), fp);
    std::fclose(fp);
    if (written != data.size()){
        std::remove(tempPath.c_str());
        error = "cannot write temporary module file";
        return false;
    }
    mp_module handle = mpLoadLibrary(tempPath);
    std::string loadError = mpLibraryError();
    std::remove(tempPath.c_str());
    if (!handle){
        error = loadError.empty() ? std::string("cannot open shared object") : loadError;
        return false;
    }
    info.base = reinterpret_cast<void*>(handle);
    info.size = data.size();
    return true;
#endif
}
#endif


// pyd 构建 模块
// pyd build module
// pyd ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠦᠪᠲ᠋ᠠᠢ
static PydObj* pydBuildModule(void* defBlob, const std::string& fallbackName){
    if (!defBlob) return nullptr;
    unsigned char* def = static_cast<unsigned char*>(defBlob);
    const char* mName = *reinterpret_cast<const char**>(def + 40);
    void* methods = *reinterpret_cast<void**>(def + 64);
    void* slots = *reinterpret_cast<void**>(def + 72);
    long long mSize = *reinterpret_cast<long long*>(def + 56);
    (void)mSize;
    PydObj* module = pydNewObj(PK_MODULE);
    module->name = mName ? mName : fallbackName;
    module->dictItems = new PydDictItems();

    if (methods){
        unsigned char* method = static_cast<unsigned char*>(methods);
        while (true){
            const char* mlName = *reinterpret_cast<const char**>(method + 0);
            void* mlMeth = *reinterpret_cast<void**>(method + 8);
            int mlFlags = *reinterpret_cast<int*>(method + 16);
            if (!mlName || !mlMeth) break;
            PydObj* fn = pydNewObj(PK_OPAQUE);
            fn->name = mlName;
            fn->fnPtr = mlMeth;
            fn->fnFlags = mlFlags;
            pydDictSet(module, mlName, fn);
            method += 32;
        }
    }

    if (slots){
        unsigned char* slot = static_cast<unsigned char*>(slots);
        while (true){
            int slotId = *reinterpret_cast<int*>(slot + 0);
            void* slotValue = *reinterpret_cast<void**>(slot + 8);
            if (slotId == 0 || !slotValue) break;
            if (slotId == 2){
                typedef int (*PyModExecFunc)(PydObj*);
                PyModExecFunc exec = reinterpret_cast<PyModExecFunc>(slotValue);
                g_pydErrorMsg.clear();
                int rc = exec(module);
                if (rc < 0 || !g_pydErrorMsg.empty()) return module;
            }
            slot += 16;
        }
    }
    return module;
}


// Pyd Slot
// pyd slot
// Pyd Slot
struct PydSlot{
    PydObj* module = nullptr;
    PydObj* obj = nullptr;
    bool isFunc = false;
};

static std::vector<PydSlot> g_pydSlots;

template <int N>

static value pydThunk(cffi& args);

// pyd Dispatch Slot
// pyd dispatch slot
// pyd Dispatch Slot
static value pydDispatchSlot(int index, cffi& args){
    vm& vm = *args.vm;
    if (index < 0 || static_cast<size_t>(index) >= g_pydSlots.size()){
        vm.raiseError("RuntimeError", "invalid pyd function slot");
        return vm.makeNone();
    }
    PydSlot& slot = g_pydSlots[static_cast<size_t>(index)];
    std::vector<PydObj*> pargs;
    for (auto& item : args.argv) pargs.push_back(pydFromVm(vm, item));
    std::vector<std::pair<std::string, PydObj*>> pkwargs;
    for (auto& entry : args.kwargv) pkwargs.push_back({entry.first, pydFromVm(vm, entry.second)});
    g_pydErrorType = nullptr;
    g_pydErrorMsg.clear();
    PydObj* result = nullptr;
    if (slot.isFunc) result = pydInvokeCallable(slot.obj, slot.module, pargs, pkwargs);
    else result = slot.obj;
    if (!g_pydErrorMsg.empty()){
        std::string typeName = (g_pydErrorType && !g_pydErrorType->name.empty())
                                   ? g_pydErrorType->name : "RuntimeError";
        std::string message = g_pydErrorMsg;
        g_pydErrorMsg.clear();
        g_pydErrorType = nullptr;
        vm.raiseError(typeName, message);
        return vm.makeNone();
    }
    return pydToVm(vm, result);
}

template <size_t... I>

static std::array<void*, sizeof...(I)> pydMakeThunks(std::index_sequence<I...>){
    return {{ reinterpret_cast<void*>(static_cast<cffi_native_function>(&pydThunk<static_cast<int>(I)>))... }};
}

static const std::array<void*, 512> g_pydThunks = pydMakeThunks(std::make_index_sequence<512>{});

template <int N>

// pyd Thunk
// pyd thunk
// pyd Thunk
static value pydThunk(cffi& args){
    return pydDispatchSlot(N, args);
}


void vm::loadpyd(const std::string& name, std::vector<unsigned char> pydfile){
    pydEnsureInit();
    std::string error;
    PydImageInfo image;
    if (!pydMapImage(pydfile, error, image, name)){
        raiseError("ImportError", "cannot load pyd '" + name + "': " + error);
        return;
    }
    std::string initName = "PyInit_" + name;
    void* initProc = pydGetExportProc(image.base, initName.c_str());
    if (!initProc){
        raiseError("ImportError", "pyd '" + name + "' does not export '" + initName + "'");
        return;
    }
    g_pydErrorType = nullptr;
    g_pydErrorMsg.clear();
    typedef void* (*PydInitFunc)(void);
    PydInitFunc init = reinterpret_cast<PydInitFunc>(initProc);
    void* initResult = init();
    if (!g_pydErrorMsg.empty()){
        std::string typeName = (g_pydErrorType && !g_pydErrorType->name.empty())
                                   ? g_pydErrorType->name : "ImportError";
        std::string message = g_pydErrorMsg;
        g_pydErrorMsg.clear();
        g_pydErrorType = nullptr;
        raiseError(typeName, message);
        return;
    }
    if (!initResult){
        raiseError("ImportError", "initialization of pyd '" + name + "' failed");
        return;
    }
    PydObj* module = nullptr;
    PydObj* head = static_cast<PydObj*>(initResult);
    if (head->ob_type == static_cast<void*>(&g_pydTypeModuleDef)){
        module = pydBuildModule(initResult, name);
    } else if (head->kind == PK_MODULE){
        module = head;
    }
    if (!module){
        raiseError("ImportError", "pyd '" + name + "' returned an unsupported module object");
        return;
    }

    ffilib library;
    library.name = name;
    if (module->dictItems){
        for (auto& entry : *module->dictItems){
            if (!entry.first || entry.first->kind != PK_STR) continue;
            PydObj* value = entry.second;
            if (static_cast<size_t>(g_pydSlots.size()) >= g_pydThunks.size()){
                raiseError("RuntimeError", "too many pyd members");
                return;
            }
            int index = static_cast<int>(g_pydSlots.size());
            PydSlot slot;
            slot.module = module;
            slot.obj = value;
            slot.isFunc = (value && value->fnPtr != nullptr);
            g_pydSlots.push_back(slot);
            if (slot.isFunc) library.functions[entry.first->sval] = g_pydThunks[static_cast<size_t>(index)];
            else library.data[entry.first->sval] = g_pydThunks[static_cast<size_t>(index)];
        }
    }
    loadlib(library);
}
