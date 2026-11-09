#include "../cffi.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 单个选项定义
// Single option definition
// ᠨᠢᠭᠡᠴᠡ ᠰᠣᠩᠭᠣᠯᠲᠠ
struct OptDef{
    std::vector<std::string> shorts;
    std::vector<std::string> longs;
    std::string dest;
    std::string action = "store"; // store/store_true/store_false/append/count
    std::string type = "string";  // string/int/float/choice
    value defaultValue;
    bool hasDefault = false;
    std::string metavar;
    std::vector<std::string> choices;
    std::string help;
    std::string constValue; // store_const 暂不使用
};

// OptionParser 内部状态
// OptionParser internal state
// OptionParser ᠦᠨ ᠳᠣᠲᠤᠭᠠᠳᠤ ᠪᠠᠶᠢᠳᠠᠯ
struct ParserState{
    std::string prog;
    std::string usage;
    std::string description;
    std::vector<OptDef> options;
};

ParserState* stateOf(const value& self){
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<ParserState*>(inst->handle);
}

// 由选项字符串推导 dest
// Derive dest from option strings
// ᠰᠣᠩᠭᠣᠯᠲᠠ ᠡᠴᠡ dest ᠢ ᠭᠠᠷᠭᠠᠬᠤ
std::string deriveDest(const OptDef& def){
    if (!def.longs.empty()){
        std::string d = def.longs[0].substr(2);
        for (auto& c : d) if (c == '-') c = '_';
        return d;
    }
    if (!def.shorts.empty()){
        std::string d = def.shorts[0].substr(1);
        for (auto& c : d) if (c == '-') c = '_';
        return d;
    }
    return "";
}

// usage 行生成 (简化版)
// Build usage line (simplified)
// usage ᠮᠥᠷ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
std::string buildUsage(ParserState* st){
    if (!st->usage.empty()){
        std::string u = st->usage;
        size_t pos;
        while ((pos = u.find("%prog")) != std::string::npos){
            u.replace(pos, 5, st->prog);
        }
        // CPython: "usage:" 前缀大写, 无前缀则补 "Usage: "
        // CPython: capitalize "usage:" prefix, prepend "Usage: " if absent
        // CPython: "usage:" ᠢ ᠶᠡᠬᠡᠳᠭᠡᠵᠦ, ᠦᠭᠡᠢ ᠪᠣᠯ "Usage: " ᠨᠡᠮᠡᠨ᠎ᠡ
        if (u.rfind("usage:", 0) == 0) u = "Usage:" + u.substr(6);
        else u = "Usage: " + u;
        return u;
    }
    std::string out = "Usage: " + st->prog + " [options]";
    return out;
}

// 错误处理: 输出 usage + 错误, 抛 SystemExit(2)
// Error handling: print usage + error, raise SystemExit(2)
// ᠠᠯᠳᠠᠭ᠎ᠠ ᠪᠠᠨ ᠪᠣᠯᠪᠠᠰᠤᠷᠠᠭᠤᠯᠬᠤ
void parserError(cffi& a, ParserState* st, const std::string& message){
    std::cerr << buildUsage(st) << "\n";
    std::cerr << st->prog << ": error: " << message << "\n";
    std::cerr.flush();
    a.vm->raiseError("SystemExit", "2");
}

// 类型转换
// Type conversion
// ᠲᠥᠷᠥᠯ ᠬᠤᠪᠢᠷᠠᠭᠤᠯᠬᠤ
value convertValue(cffi& a, ParserState* st, const OptDef& def, const std::string& text){
    if (def.type == "int"){
        try {
            size_t used = 0;
            long long v = std::stoll(text, &used);
            if (used != text.size()) throw std::invalid_argument("bad");
            return a.mkint(v);
        } catch (...){
            parserError(a, st, "option " + def.longs[0] + ": invalid int value: '" + text + "'");
            return a.none();
        }
    }
    if (def.type == "float"){
        try {
            size_t used = 0;
            double v = std::stod(text, &used);
            if (used != text.size()) throw std::invalid_argument("bad");
            return a.mkfloat(v);
        } catch (...){
            parserError(a, st, "option " + def.longs[0] + ": invalid float value: '" + text + "'");
            return a.none();
        }
    }
    if (def.type == "choice"){
        for (auto& c : def.choices) if (c == text) return a.mkstr(text);
        parserError(a, st, "option " + def.longs[0] + ": invalid choice: '" + text + "'");
        return a.none();
    }
    return a.mkstr(text);
}

// 需要带值的 action
// Whether the action takes a value
// ᠠᠷᠭ᠎ᠠ ᠦᠨᠡ ᠲᠡᠢ ᠶᠤᠤ
bool takesValue(const std::string& action){
    return action == "store" || action == "append";
}

// parse_args 核心
// Core of parse_args
// parse_args ᠦᠨ ᠭᠣᠣᠯ
value parseArgs(cffi& a, ParserState* st, const std::vector<std::string>& argv,
                value& valuesOut){
    // 初始化默认值
    // Initialize defaults
    // ᠠᠩᠬᠨᠤ ᠦᠨ᠎ᠡ ᠪᠡᠷ ᠡᠬᠢᠯᠡᠭᠦᠯᠬᠦ
    std::map<std::string, value> result;
    for (auto& def : st->options){
        if (result.count(def.dest)) continue;
        if (def.hasDefault) result[def.dest] = def.defaultValue;
        else result[def.dest] = a.none();
    }

    std::vector<value> leftover;
    for (size_t i = 0; i < argv.size(); ++i){
        std::string token = argv[i];
        if (token == "--"){
            for (++i; i < argv.size(); ++i) leftover.push_back(a.mkstr(argv[i]));
            break;
        }
        if (token.size() >= 2 && token[0] == '-' && token[1] == '-'){
            // 长选项
            // Long option
            // ᠤᠷᠲᠤ ᠰᠣᠩᠭᠣᠯᠲᠠ
            std::string name = token.substr(2);
            std::string inlineVal;
            bool hasInline = false;
            size_t eq = name.find('=');
            if (eq != std::string::npos){
                inlineVal = name.substr(eq + 1);
                name = name.substr(0, eq);
                hasInline = true;
            }
            const OptDef* found = nullptr;
            for (auto& def : st->options){
                for (auto& l : def.longs){
                    if (l == "--" + name){ found = &def; break; }
                }
                if (found) break;
            }
            // 前缀缩写匹配
            // Abbreviated prefix match
            // ᠲᠣᠪᠴᠢᠯᠠᠭᠰᠠᠨ ᠰᠣᠩᠭᠣᠯᠲᠠ
            if (!found){
                const OptDef* candidate = nullptr;
                int matches = 0;
                for (auto& def : st->options){
                    for (auto& l : def.longs){
                        if (l.size() > 2 && l.substr(2).rfind(name, 0) == 0){
                            candidate = &def; matches++; break;
                        }
                    }
                }
                if (matches == 1) found = candidate;
            }
            if (!found){
                parserError(a, st, "no such option: --" + name);
                return a.none();
            }
            if (takesValue(found->action)){
                std::string raw;
                if (hasInline) raw = inlineVal;
                else if (i + 1 < argv.size()) raw = argv[++i];
                else {
                    parserError(a, st, "--" + name + " option requires an argument");
                    return a.none();
                }
                value converted = convertValue(a, st, *found, raw);
                if (a.vm->hasError()) return a.none();
                if (found->action == "append"){
                    if (result[found->dest].type != "list"){
                        result[found->dest] = a.mklist({});
                    }
                    auto* lst = static_cast<std::vector<value>*>(result[found->dest].ptr);
                    lst->push_back(converted);
                } else {
                    result[found->dest] = converted;
                }
            } else if (found->action == "store_true"){
                result[found->dest] = a.mkbool(true);
            } else if (found->action == "store_false"){
                result[found->dest] = a.mkbool(false);
            } else if (found->action == "count"){
                value curVal = result[found->dest];
                long long cur = curVal.type == "none" ? 0 : a.vm->toInt(curVal);
                result[found->dest] = a.mkint(cur + 1);
            }
            continue;
        }
        if (token.size() >= 2 && token[0] == '-'){
            // 短选项 (支持 -abc 组合与 -fVALUE)
            // Short options (support -abc bundling and -fVALUE)
            // ᠪᠣᠭᠣᠨᠢ ᠰᠣᠩᠭᠣᠯᠲᠠ
            bool consumedNext = false;
            for (size_t k = 1; k < token.size(); ++k){
                std::string name = token.substr(k, 1);
                const OptDef* found = nullptr;
                for (auto& def : st->options){
                    for (auto& s : def.shorts){
                        if (s == "-" + name){ found = &def; break; }
                    }
                    if (found) break;
                }
                if (!found){
                    parserError(a, st, "no such option: -" + name);
                    return a.none();
                }
                if (takesValue(found->action)){
                    std::string raw;
                    if (k + 1 < token.size()) raw = token.substr(k + 1);
                    else if (i + 1 < argv.size()){ raw = argv[++i]; consumedNext = true; }
                    else {
                        parserError(a, st, "-" + name + " option requires an argument");
                        return a.none();
                    }
                    value converted = convertValue(a, st, *found, raw);
                    if (a.vm->hasError()) return a.none();
                    if (found->action == "append"){
                        if (result[found->dest].type != "list"){
                            result[found->dest] = a.mklist({});
                        }
                        auto* lst = static_cast<std::vector<value>*>(result[found->dest].ptr);
                        lst->push_back(converted);
                    } else {
                        result[found->dest] = converted;
                    }
                    break;
                } else if (found->action == "store_true"){
                    result[found->dest] = a.mkbool(true);
                } else if (found->action == "store_false"){
                    result[found->dest] = a.mkbool(false);
                } else if (found->action == "count"){
                    value curVal = result[found->dest];
                    long long cur = curVal.type == "none" ? 0 : a.vm->toInt(curVal);
                    result[found->dest] = a.mkint(cur + 1);
                }
            }
            (void)consumedNext;
            continue;
        }
        leftover.push_back(a.mkstr(token));
    }

    // 创建 Values 实例
    // Create the Values instance
    // Values ᠶᠢᠨᠢᠰ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    auto classIt = a.library->classes.find("Values");
    value values = a.vm->makeNativeObject(a.library, &classIt->second, nullptr);
    auto* vi = static_cast<ffi_instance*>(values.ptr);
    for (auto& kv : result) vi->attributes[kv.first] = kv.second;
    valuesOut = values;
    return a.mklist(leftover);
}

// ---- OptionParser 方法 / OptionParser methods / OptionParser ᠠᠷᠭ᠎ᠠ ----

value parserCtor(cffi& a){
    auto* st = new ParserState();
    st->prog = "prog";
    if (a.has_kwarg("usage")) st->usage = a.kwarg_str("usage", "");
    if (a.has_kwarg("prog")) st->prog = a.kwarg_str("prog", "prog");
    if (a.has_kwarg("description")) st->description = a.kwarg_str("description", "");
    value obj = a.mkobject(st);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["prog"] = a.mkstr(st->prog);
    return obj;
}

value parserAddOption(cffi& a){
    ParserState* st = stateOf(a.self());
    OptDef def;
    for (size_t i = 0; i < a.argc(); ++i){
        std::string s = a.arg_str(i);
        if (s.rfind("--", 0) == 0) def.longs.push_back(s);
        else if (s.rfind("-", 0) == 0) def.shorts.push_back(s);
    }
    if (a.has_kwarg("dest")) def.dest = a.kwarg_str("dest", "");
    if (a.has_kwarg("action")) def.action = a.kwarg_str("action", "store");
    if (a.has_kwarg("type")) def.type = a.kwarg_str("type", "string");
    if (a.has_kwarg("default")){
        def.defaultValue = a.kwarg("default");
        def.hasDefault = true;
    }
    if (a.has_kwarg("metavar")) def.metavar = a.kwarg_str("metavar", "");
    if (a.has_kwarg("help")) def.help = a.kwarg_str("help", "");
    if (a.has_kwarg("choices")){
        value ch = a.kwarg("choices");
        if (ch.type == "list" || ch.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(ch.ptr);
            for (auto& item : *items) def.choices.push_back(a.vm->toString(item));
        }
        def.type = "choice";
    }
    if (def.dest.empty()) def.dest = deriveDest(def);
    st->options.push_back(def);
    return a.none();
}

value parserParseArgs(cffi& a){
    ParserState* st = stateOf(a.self());
    std::vector<std::string> argv;
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value listVal = a.arg(0);
        if (listVal.type == "list" || listVal.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(listVal.ptr);
            for (auto& item : *items) argv.push_back(a.vm->toString(item));
        }
    }
    value values;
    value rest = parseArgs(a, st, argv, values);
    if (a.vm->hasError()) return a.none();
    std::vector<value> pair;
    pair.push_back(values);
    pair.push_back(rest);
    return a.vm->makeTuple(pair);
}

value parserSetDefaults(cffi& a){
    ParserState* st = stateOf(a.self());
    for (auto& kv : a.kwargv){
        for (auto& def : st->options){
            if (def.dest == kv.first){
                def.defaultValue = kv.second;
                def.hasDefault = true;
            }
        }
    }
    return a.none();
}

value parserGetUsage(cffi& a){
    ParserState* st = stateOf(a.self());
    // CPython get_usage() 以换行结尾
    // CPython get_usage() ends with a newline
    // CPython get_usage() ᠰᠢᠯᠲᠠᠭᠠᠨ ᠦᠯᠡᠳᠬᠡᠯ ᠲᠠᠢ
    return a.mkstr(buildUsage(st) + "\n");
}

value parserPrintHelp(cffi& a){
    ParserState* st = stateOf(a.self());
    std::cout << buildUsage(st) << "\n\nOptions:\n";
    for (auto& def : st->options){
        std::string line = "  ";
        for (size_t i = 0; i < def.shorts.size(); ++i){
            if (i) line += ", ";
            line += def.shorts[i];
        }
        for (size_t i = 0; i < def.longs.size(); ++i){
            if (!def.shorts.empty() || i) line += ", ";
            line += def.longs[i];
        }
        if (takesValue(def.action)){
            line += " " + (def.metavar.empty() ? "VALUE" : def.metavar);
        }
        if (!def.help.empty()) line += "\t" + def.help;
        std::cout << line << "\n";
    }
    std::cout.flush();
    return a.none();
}

value parserErrorMethod(cffi& a){
    ParserState* st = stateOf(a.self());
    parserError(a, st, a.arg_str(0));
    return a.none();
}

// Values 构造器
// Values constructor
// Values ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value valuesCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    if (a.argc() > 0 && a.arg(0).type == "dict"){
        auto* items = static_cast<dict_t*>(a.arg(0).ptr);
        for (auto& kv : *items){
            inst->attributes[a.vm->toString(kv.first)] = kv.second;
        }
    }
    return obj;
}

// Option 构造器 (简化)
// Option constructor (simplified)
// Option ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value optionCtor(cffi& a){
    return a.none();
}

} // namespace

// cffi optparse lib
// cffi optparse lib
// cffi optparse lib
ffilib cffi_optparse_lib(){
    ffi_class optionParser = cffi_class(
        "OptionParser", (void*)parserCtor,
        {
            {"add_option", (void*)parserAddOption},
            {"parse_args", (void*)parserParseArgs},
            {"set_defaults", (void*)parserSetDefaults},
            {"get_usage", (void*)parserGetUsage},
            {"print_help", (void*)parserPrintHelp},
            {"error", (void*)parserErrorMethod}
        });
    ffi_class values = cffi_class("Values", (void*)valuesCtor, {});
    ffi_class option = cffi_class("Option", (void*)optionCtor, {});

    return cffi_lib(
        "optparse",
        {},
        {},
        {
            {"OptionParser", optionParser},
            {"Values", values},
            {"Option", option},
            {"OptionGroup", option}
        });
}

} // namespace minipy
