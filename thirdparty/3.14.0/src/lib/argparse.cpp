#include "../cffi.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 单个参数定义
// Single argument definition
// ᠨᠢᠭᠡᠴᠡ ᠠᠷᠭᠤᠮᠡᠨᠲ ᠦᠨ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct ArgDef{
    std::vector<std::string> flags; // 选项串; 空 = 位置参数
    std::string dest;
    std::string action = "store"; // store/store_true/store_false/append/count/store_const/help
    std::string nargs;            // "" / "?" / "*" / "+" / 数字
    std::string typeName = "str"; // str/int/float
    value defaultValue;
    bool hasDefault = false;
    std::string metavar;
    std::vector<std::string> choices;
    std::string help;
    bool required = false;
    value constValue;
    bool hasConst = false;
};

// ArgumentParser 内部状态
// ArgumentParser internal state
// ArgumentParser ᠦᠨ ᠳᠣᠲᠤᠭᠠᠳᠤ ᠪᠠᠶᠢᠳᠠᠯ
struct ArgParserState{
    std::string prog = "prog";
    std::string description;
    std::string epilog;
    std::vector<ArgDef> options;
    std::vector<ArgDef> positionals;
    std::vector<std::string> addOrder; // 按 add_argument 顺序的 dest 列表
    bool allowAbbrev = true;
};

ArgParserState* stateOf(const value& self){
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<ArgParserState*>(inst->handle);
}

// 是否选项串
// Whether a string is an option flag
// ᠰᠣᠩᠭᠣᠯᠲᠠ ᠮᠥᠨ ᠦᠦ
bool isFlag(const std::string& s){
    return s.size() >= 2 && s[0] == '-';
}

// 由 flags 推导 dest
// Derive dest from flags
// flags ᠡᠴᠡ dest ᠢ ᠭᠠᠷᠭᠠᠬᠤ
std::string deriveDest(const ArgDef& def){
    for (auto& f : def.flags){
        if (f.rfind("--", 0) == 0){
            std::string d = f.substr(2);
            for (auto& c : d) if (c == '-') c = '_';
            return d;
        }
    }
    if (!def.flags.empty()){
        std::string d = def.flags[0].substr(1);
        for (auto& c : d) if (c == '-') c = '_';
        return d;
    }
    return "";
}

// 参数的显示名 (metavar 或 dest 大写)
// Display metavar (given or upper dest)
// ᠠᠷᠭᠤᠮᠡᠨᠲ ᠦᠨ ᠨᠡᠷ᠎ᠡ
std::string displayMetavar(const ArgDef& def){
    if (!def.metavar.empty()) return def.metavar;
    if (!def.choices.empty()){
        std::string out = "{";
        for (size_t i = 0; i < def.choices.size(); ++i){
            if (i) out += ",";
            out += def.choices[i];
        }
        out += "}";
        return out;
    }
    if (def.flags.empty()) return def.dest;
    std::string up = def.dest;
    for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up;
}

// 错误时的参数标签 (如 -f/--foo 或 dest)
// Argument label for errors (-f/--foo or dest)
// ᠠᠯᠳᠠᠭ᠎ᠠ ᠶᠢᠨ ᠠᠷᠭᠤᠮᠡᠨᠲ ᠨᠡᠷ᠎ᠡ
std::string argLabel(const ArgDef& def){
    if (def.flags.empty()) return displayMetavar(def);
    std::string out;
    for (size_t i = 0; i < def.flags.size(); ++i){
        if (i) out += "/";
        out += def.flags[i];
    }
    return out;
}

// usage 串生成 (CPython 风格, 不换行)
// Build usage string (CPython style, no wrapping)
// usage ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
std::string buildUsage(ArgParserState* st){
    std::string out = "usage: " + st->prog;
    auto fmtOpt = [](const ArgDef& def) -> std::string {
        std::string flag = def.flags[0];
        bool takes = def.action == "store" || def.action == "append";
        if (!takes) return "[" + flag + "]";
        std::string mv = displayMetavar(def);
        if (def.nargs == "?") return "[" + flag + " [" + mv + "]]";
        if (def.nargs == "*") return "[" + flag + " [" + mv + " ...]]";
        if (def.nargs == "+") return "[" + flag + " " + mv + " [" + mv + " ...]]";
        return "[" + flag + " " + mv + "]";
    };
    // -h 总是存在 (add_help 默认 True)
    // -h always present (add_help defaults True)
    // -h ᠪᠠᠶᠢᠨ᠎ᠠ
    out += " [-h]";
    for (auto& def : st->options) out += " " + fmtOpt(def);
    auto fmtPos = [](const ArgDef& def) -> std::string {
        std::string mv = displayMetavar(def);
        if (def.nargs == "?") return "[" + mv + "]";
        if (def.nargs == "*") return "[" + mv + " ...]";
        if (def.nargs == "+") return mv + " [" + mv + " ...]";
        if (!def.nargs.empty()){
            int n = std::atoi(def.nargs.c_str());
            std::string out;
            for (int i = 0; i < n; ++i){ if (i) out += " "; out += mv; }
            return out;
        }
        return mv;
    };
    for (auto& def : st->positionals) out += " " + fmtPos(def);
    return out;
}

// 错误处理: usage + prog: error: msg → SystemExit(2)
// Error handling: usage + prog: error: msg → SystemExit(2)
// ᠠᠯᠳᠠᠭ᠎ᠠ ᠪᠠᠨ ᠪᠣᠯᠪᠠᠰᠤᠷᠠᠭᠤᠯᠬᠤ
void parserError(cffi& a, ArgParserState* st, const std::string& message){
    std::cerr << buildUsage(st) << "\n";
    std::cerr << st->prog << ": error: " << message << "\n";
    std::cerr.flush();
    a.vm->raiseError("SystemExit", "2");
}

// 类型转换
// Type conversion
// ᠲᠥᠷᠥᠯ ᠬᠤᠪᠢᠷᠠᠭᠤᠯᠬᠤ
value convertArg(cffi& a, ArgParserState* st, const ArgDef& def, const std::string& text){
    if (def.typeName == "int"){
        try {
            size_t used = 0;
            long long v = std::stoll(text, &used);
            if (used != text.size()) throw std::invalid_argument("bad");
            return a.mkint(v);
        } catch (...){
            parserError(a, st, "argument " + argLabel(def) + ": invalid int value: '" + text + "'");
            return a.none();
        }
    }
    if (def.typeName == "float"){
        try {
            size_t used = 0;
            double v = std::stod(text, &used);
            if (used != text.size()) throw std::invalid_argument("bad");
            return a.mkfloat(v);
        } catch (...){
            parserError(a, st, "argument " + argLabel(def) + ": invalid float value: '" + text + "'");
            return a.none();
        }
    }
    return a.mkstr(text);
}

// choices 校验
// Validate choices
// choices ᠢ ᠰᠢᠯᠭᠠᠬᠤ
bool checkChoices(cffi& a, ArgParserState* st, const ArgDef& def, const value& v){
    if (def.choices.empty()) return true;
    std::string text = a.vm->toString(v);
    for (auto& c : def.choices) if (c == text) return true;
    std::string list;
    for (size_t i = 0; i < def.choices.size(); ++i){
        if (i) list += ", ";
        list += "'" + def.choices[i] + "'";
    }
    parserError(a, st, "argument " + argLabel(def) + ": invalid choice: '" + text +
                       "' (choose from " + list + ")");
    return false;
}

// 应用一次 store/append 动作
// Apply one store/append action
// store/append ᠠᠷᠭ᠎ᠠ ᠪᠡᠷ ᠬᠡᠷᠡᠭᠯᠡᠬᠦ
void applyAction(cffi& a, ArgParserState* st, const ArgDef& def,
                 const std::vector<value>& vals, std::map<std::string, value>& result,
                 std::vector<std::string>& order){
    auto record = [&](const value& v){
        if (result.find(def.dest) == result.end()) order.push_back(def.dest);
        result[def.dest] = v;
    };
    if (def.action == "store"){
        value v = vals.empty() ? a.none() : vals[0];
        if (vals.size() > 1) v = a.mklist(vals);
        record(v);
    } else if (def.action == "append"){
        value cur = result.count(def.dest) ? result[def.dest] : a.none();
        if (cur.type != "list") cur = a.mklist({});
        auto* lst = static_cast<std::vector<value>*>(cur.ptr);
        value v = vals.size() == 1 ? vals[0] : a.mklist(vals);
        lst->push_back(v);
        record(cur);
    } else if (def.action == "store_true"){
        record(a.mkbool(true));
    } else if (def.action == "store_false"){
        record(a.mkbool(false));
    } else if (def.action == "count"){
        value cur = result.count(def.dest) ? result[def.dest] : a.none();
        long long n = cur.type == "none" || cur.type == "" ? 0 : a.vm->toInt(cur);
        record(a.mkint(n + 1));
    } else if (def.action == "store_const"){
        record(def.hasConst ? def.constValue : a.none());
    }
}

// nargs 需要的个数
// How many values nargs needs
// nargs ᠬᠡᠳᠦᠬ᠎ᠡ ᠦᠨ᠎ᠡ ᠬᠡᠷᠡᠭᠯᠡᠬᠦ
std::string nargsError(const ArgDef& def){
    if (def.nargs == "+") return "expected at least one argument";
    if (def.nargs.empty()) return "expected one argument";
    int n = std::atoi(def.nargs.c_str());
    return "expected " + std::to_string(n) + " arguments";
}

// parse 核心; 返回 leftover 列表
// Core parse; returns leftover list
// parse ᠦᠨ ᠭᠣᠣᠯ
value doParse(cffi& a, ArgParserState* st, const std::vector<std::string>& argv,
              value& nsOut, bool& hadError, bool allowUnknown = false){
    hadError = false;
    std::map<std::string, value> result;
    std::vector<std::string> order;

    // 默认值
    // Defaults
    // ᠠᠩᠬᠠᠨ ᠦᠨ᠎ᠡ
    auto initDefault = [&](const ArgDef& def){
        if (result.count(def.dest)) return;
        order.push_back(def.dest);
        if (def.hasDefault) result[def.dest] = def.defaultValue;
        else if (def.action == "store_true") result[def.dest] = a.mkbool(false);
        else if (def.action == "store_false") result[def.dest] = a.mkbool(true);
        else result[def.dest] = a.none();
    };
    // 按 add_argument 顺序预设默认值 (CPython Namespace repr 顺序)
    // Preset defaults in add_argument order (CPython Namespace repr order)
    // add_argument ᠳᠠᠷᠠᠭᠠᠯᠠᠯ ᠢᠶᠠᠷ ᠠᠩᠬᠠᠨ ᠦᠨ᠎ᠡ ᠲᠣᠭᠲᠠᠭᠠᠬᠤ
    for (auto& destName : st->addOrder){
        const ArgDef* found = nullptr;
        for (auto& d : st->options) if (d.dest == destName){ found = &d; break; }
        if (!found) for (auto& d : st->positionals) if (d.dest == destName){ found = &d; break; }
        if (found) initDefault(*found);
    }
    std::vector<bool> seenOpt(st->options.size(), false);

    std::vector<std::string> positionalTokens;
    std::vector<std::string> extras;
    bool afterDoubleDash = false;

    for (size_t i = 0; i < argv.size(); ++i){
        std::string token = argv[i];
        if (afterDoubleDash){ positionalTokens.push_back(token); continue; }
        if (token == "--"){ afterDoubleDash = true; continue; }
        if (token.size() >= 2 && token[0] == '-' && token[1] == '-' &&
            token != "--"){
            std::string name = token.substr(2);
            std::string inlineVal;
            bool hasInline = false;
            size_t eq = name.find('=');
            if (eq != std::string::npos){
                inlineVal = name.substr(eq + 1);
                name = name.substr(0, eq);
                hasInline = true;
            }
            int found = -1;
            for (size_t k = 0; k < st->options.size(); ++k){
                for (auto& f : st->options[k].flags){
                    if (f == "--" + name){ found = static_cast<int>(k); break; }
                }
                if (found >= 0) break;
            }
            if (found < 0 && st->allowAbbrev){
                int matches = 0, cand = -1;
                for (size_t k = 0; k < st->options.size(); ++k){
                    for (auto& f : st->options[k].flags){
                        if (f.size() > 2 && f.substr(2).rfind(name, 0) == 0){
                            matches++; cand = static_cast<int>(k); break;
                        }
                    }
                }
                if (matches == 1) found = cand;
                else if (matches > 1){
                    std::string cands;
                    for (auto& def : st->options){
                        for (auto& f : def.flags){
                            if (f.size() > 2 && f.substr(2).rfind(name, 0) == 0){
                                if (!cands.empty()) cands += ", ";
                                cands += f;
                            }
                        }
                    }
                    parserError(a, st, "ambiguous option: --" + name + " could match " + cands);
                    hadError = true; return a.none();
                }
            }
            if (found < 0){
                extras.push_back(token);
                if (hasInline){ /* 整串计入 */ }
                continue;
            }
            const ArgDef& def = st->options[found];
            seenOpt[found] = true;
            if (def.action == "help"){
                // 打印帮助并退出 0
                // Print help and exit 0
                // ᠲᠤᠰᠠᠯᠠᠮᠠᠢ ᠢ ᠬᠡᠪᠯᠡᠵᠦ ᠭᠠᠷᠬᠤ
                std::cout << buildUsage(st) << "\n";
                if (!st->description.empty()) std::cout << "\n" << st->description << "\n";
                std::cout.flush();
                a.vm->raiseError("SystemExit", "0");
                hadError = true; return a.none();
            }
            bool takes = def.action == "store" || def.action == "append";
            if (!takes){
                applyAction(a, st, def, {}, result, order);
                continue;
            }
            // 取值
            // Take values
            // ᠦᠨ᠎ᠡ ᠠᠪᠬᠤ
            std::vector<std::string> raws;
            if (hasInline){
                raws.push_back(inlineVal);
            } else if (def.nargs == "?" || def.nargs == "*"){
                if (i + 1 < argv.size() && !isFlag(argv[i + 1])) raws.push_back(argv[++i]);
            } else if (def.nargs == "+"){
                while (i + 1 < argv.size() && !isFlag(argv[i + 1])) raws.push_back(argv[++i]);
                if (raws.empty()){
                    parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                    hadError = true; return a.none();
                }
            } else if (!def.nargs.empty()){
                int n = std::atoi(def.nargs.c_str());
                while (static_cast<int>(raws.size()) < n && i + 1 < argv.size() &&
                       !isFlag(argv[i + 1])){
                    raws.push_back(argv[++i]);
                }
                if (static_cast<int>(raws.size()) < n){
                    parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                    hadError = true; return a.none();
                }
            } else {
                if (i + 1 < argv.size() && (!isFlag(argv[i + 1]) || argv[i + 1] == "-")){
                    raws.push_back(argv[++i]);
                } else {
                    parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                    hadError = true; return a.none();
                }
            }
            if ((def.nargs == "?" || def.nargs == "*") && raws.empty() && !hasInline){
                if (def.action == "store"){
                    if (result.find(def.dest) == result.end()) order.push_back(def.dest);
                    result[def.dest] = def.hasConst ? def.constValue : a.none();
                }
                continue;
            }
            std::vector<value> vals;
            for (auto& raw : raws){
                value cv = convertArg(a, st, def, raw);
                if (a.vm->hasError()){ hadError = true; return a.none(); }
                if (!checkChoices(a, st, def, cv)){ hadError = true; return a.none(); }
                vals.push_back(cv);
            }
            if (def.nargs.empty()){
                applyAction(a, st, def, {vals[0]}, result, order);
            } else {
                applyAction(a, st, def, vals, result, order);
            }
            continue;
        }
        if (token.size() >= 2 && token[0] == '-' &&
            !(token.size() > 1 && std::isdigit(static_cast<unsigned char>(token[1])))){
            // 短选项串
            // Short option cluster
            // ᠪᠣᠭᠣᠨᠢ ᠰᠣᠩᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠳᠡᠰ
            bool stopCluster = false;
            for (size_t c = 1; c < token.size() && !stopCluster; ++c){
                std::string name = token.substr(c, 1);
                int found = -1;
                for (size_t k = 0; k < st->options.size(); ++k){
                    for (auto& f : st->options[k].flags){
                        if (f == "-" + name){ found = static_cast<int>(k); break; }
                    }
                    if (found >= 0) break;
                }
                if (found < 0){
                    extras.push_back("-" + name);
                    continue;
                }
                const ArgDef& def = st->options[found];
                seenOpt[found] = true;
                if (def.action == "help"){
                    std::cout << buildUsage(st) << "\n";
                    std::cout.flush();
                    a.vm->raiseError("SystemExit", "0");
                    hadError = true; return a.none();
                }
                bool takes = def.action == "store" || def.action == "append";
                if (!takes){
                    applyAction(a, st, def, {}, result, order);
                    continue;
                }
                std::vector<std::string> raws;
                if (c + 1 < token.size()){
                    raws.push_back(token.substr(c + 1));
                    stopCluster = true;
                } else if (def.nargs == "?" || def.nargs == "*"){
                    if (i + 1 < argv.size() && !isFlag(argv[i + 1])) raws.push_back(argv[++i]);
                } else {
                    int need = def.nargs.empty() ? 1 : std::atoi(def.nargs.c_str());
                    while (static_cast<int>(raws.size()) < need && i + 1 < argv.size() &&
                           !isFlag(argv[i + 1])){
                        raws.push_back(argv[++i]);
                    }
                    if (static_cast<int>(raws.size()) < need){
                        parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                        hadError = true; return a.none();
                    }
                }
                stopCluster = true;
                if ((def.nargs == "?" || def.nargs == "*") && raws.empty()){
                    if (def.action == "store"){
                        result[def.dest] = def.hasConst ? def.constValue : a.none();
                    }
                    continue;
                }
                std::vector<value> vals;
                for (auto& raw : raws){
                    value cv = convertArg(a, st, def, raw);
                    if (a.vm->hasError()){ hadError = true; return a.none(); }
                    if (!checkChoices(a, st, def, cv)){ hadError = true; return a.none(); }
                    vals.push_back(cv);
                }
                if (def.nargs.empty()) applyAction(a, st, def, {vals[0]}, result, order);
                else applyAction(a, st, def, vals, result, order);
            }
            continue;
        }
        positionalTokens.push_back(token);
    }

    // 位置参数分配
    // Positional assignment
    // ᠪᠠᠶᠢᠳᠠᠯ ᠠᠷᠭᠤᠮᠡᠨᠲ ᠢ ᠬᠤᠪᠢᠶᠠᠬᠤ
    size_t pi = 0;
    std::vector<std::string> missing;
    for (size_t d = 0; d < st->positionals.size(); ++d){
        const ArgDef& def = st->positionals[d];
        // 后续定义的最小需求
        // Minimum needed by later defs
        // ᠳᠠᠷᠠᠭ᠎ᠠ ᠶᠢᠨ ᠬᠠᠮᠤᠭ ᠪᠠᠭ᠎ᠠ ᠬᠡᠷᠡᠭᠴᠡᠭᠡ
        auto minLater = [&]() -> size_t {
            size_t n = 0;
            for (size_t j = d + 1; j < st->positionals.size(); ++j){
                const ArgDef& ld = st->positionals[j];
                if (ld.nargs.empty()) n += 1;
                else if (ld.nargs == "+") n += 1;
                else if (ld.nargs == "?" || ld.nargs == "*") n += 0;
                else n += static_cast<size_t>(std::atoi(ld.nargs.c_str()));
            }
            return n;
        };
        size_t remaining = positionalTokens.size() - pi;
        if (def.nargs.empty()){
            if (remaining <= minLater()){
                missing.push_back(displayMetavar(def));
                continue;
            }
            value cv = convertArg(a, st, def, positionalTokens[pi++]);
            if (a.vm->hasError()){ hadError = true; return a.none(); }
            if (!checkChoices(a, st, def, cv)){ hadError = true; return a.none(); }
            result[def.dest] = cv;
        } else if (def.nargs == "?"){
            if (remaining > minLater()){
                value cv = convertArg(a, st, def, positionalTokens[pi++]);
                if (a.vm->hasError()){ hadError = true; return a.none(); }
                result[def.dest] = cv;
            }
        } else if (def.nargs == "*"){
            size_t take = remaining > minLater() ? remaining - minLater() : 0;
            std::vector<value> vals;
            for (size_t t = 0; t < take; ++t){
                value cv = convertArg(a, st, def, positionalTokens[pi++]);
                if (a.vm->hasError()){ hadError = true; return a.none(); }
                vals.push_back(cv);
            }
            result[def.dest] = a.mklist(vals);
        } else if (def.nargs == "+"){
            if (remaining <= minLater()){
                parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                hadError = true; return a.none();
            }
            size_t take = remaining - minLater();
            std::vector<value> vals;
            for (size_t t = 0; t < take; ++t){
                value cv = convertArg(a, st, def, positionalTokens[pi++]);
                if (a.vm->hasError()){ hadError = true; return a.none(); }
                vals.push_back(cv);
            }
            result[def.dest] = a.mklist(vals);
        } else {
            int n = std::atoi(def.nargs.c_str());
            if (static_cast<int>(remaining) < n){
                parserError(a, st, "argument " + argLabel(def) + ": " + nargsError(def));
                hadError = true; return a.none();
            }
            std::vector<value> vals;
            for (int t = 0; t < n; ++t){
                value cv = convertArg(a, st, def, positionalTokens[pi++]);
                if (a.vm->hasError()){ hadError = true; return a.none(); }
                vals.push_back(cv);
            }
            result[def.dest] = a.mklist(vals);
        }
    }
    if (pi < positionalTokens.size()){
        for (; pi < positionalTokens.size(); ++pi) extras.push_back(positionalTokens[pi]);
    }

    // required 检查
    // required check
    // required ᠢ ᠰᠢᠯᠭᠠᠬᠤ
    for (size_t k = 0; k < st->options.size(); ++k){
        if (st->options[k].required && !seenOpt[k]){
            missing.push_back(argLabel(st->options[k]));
        }
    }
    if (!missing.empty()){
        std::string list;
        for (size_t m = 0; m < missing.size(); ++m){
            if (m) list += ", ";
            list += missing[m];
        }
        parserError(a, st, "the following arguments are required: " + list);
        hadError = true; return a.none();
    }
    if (!extras.empty() && !allowUnknown){
        std::string list;
        for (size_t e = 0; e < extras.size(); ++e){
            if (e) list += " ";
            list += extras[e];
        }
        parserError(a, st, "unrecognized arguments: " + list);
        hadError = true; return a.none();
    }

    // 构造 Namespace
    // Build the Namespace
    // Namespace ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
    auto classIt = a.library->classes.find("Namespace");
    value ns = a.vm->makeNativeObject(a.library, &classIt->second, nullptr);
    auto* ni = static_cast<ffi_instance*>(ns.ptr);
    std::vector<value> orderVals;
    for (auto& name : order){
        ni->attributes[name] = result[name];
        orderVals.push_back(a.mkstr(name));
    }
    ni->attributes["_order"] = a.mklist(orderVals);
    nsOut = ns;
    std::vector<value> extraVals;
    for (auto& e : extras) extraVals.push_back(a.mkstr(e));
    return a.mklist(extraVals);
}

// ---- ArgumentParser 方法 / ArgumentParser methods / ArgumentParser ᠠᠷᠭ᠎ᠠ ----

value parserCtor(cffi& a){
    auto* st = new ArgParserState();
    if (a.has_kwarg("prog")) st->prog = a.kwarg_str("prog", "prog");
    else st->prog = "test.py";
    if (a.has_kwarg("description")) st->description = a.kwarg_str("description", "");
    if (a.has_kwarg("epilog")) st->epilog = a.kwarg_str("epilog", "");
    if (a.has_kwarg("allow_abbrev"))
        st->allowAbbrev = a.kwarg_bool("allow_abbrev", true);
    value obj = a.mkobject(st);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["prog"] = a.mkstr(st->prog);
    inst->attributes["description"] = a.mkstr(st->description);
    return obj;
}

value parserAddArgument(cffi& a){
    ArgParserState* st = stateOf(a.self());
    ArgDef def;
    bool positional = false;
    if (a.argc() > 0){
        std::string first = a.arg_str(0);
        positional = !isFlag(first);
    }
    if (positional){
        def.dest = a.arg_str(0);
    } else {
        for (size_t i = 0; i < a.argc(); ++i){
            def.flags.push_back(a.arg_str(i));
        }
    }
    if (a.has_kwarg("action")) def.action = a.kwarg_str("action", "store");
    if (a.has_kwarg("nargs")){
        value nv = a.kwarg("nargs");
        if (nv.type == "int") def.nargs = std::to_string(a.vm->toInt(nv));
        else def.nargs = a.vm->toString(nv);
    }
    if (a.has_kwarg("type")){
        value tv = a.kwarg("type");
        if (tv.type == "class" && tv.ptr){
            def.typeName = static_cast<class_t*>(tv.ptr)->name;
        } else if (tv.type == "native_object" && tv.ptr){
            auto* ti = static_cast<ffi_instance*>(tv.ptr);
            def.typeName = ti->typeName.empty()
                ? (ti->klass ? ti->klass->name : "str") : ti->typeName;
        } else {
            def.typeName = a.vm->toString(tv);
        }
        if (def.typeName != "int" && def.typeName != "float") def.typeName = "str";
    }
    if (a.has_kwarg("default")){
        def.defaultValue = a.kwarg("default");
        def.hasDefault = true;
    }
    if (a.has_kwarg("const")){
        def.constValue = a.kwarg("const");
        def.hasConst = true;
    }
    if (a.has_kwarg("metavar")) def.metavar = a.kwarg_str("metavar", "");
    if (a.has_kwarg("help")) def.help = a.kwarg_str("help", "");
    if (a.has_kwarg("dest")) def.dest = a.kwarg_str("dest", "");
    if (a.has_kwarg("required")) def.required = a.kwarg_bool("required", false);
    if (a.has_kwarg("choices")){
        value ch = a.kwarg("choices");
        if (ch.type == "list" || ch.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(ch.ptr);
            for (auto& item : *items) def.choices.push_back(a.vm->toString(item));
        }
    }
    if (!positional && def.dest.empty()) def.dest = deriveDest(def);
    if (positional) st->positionals.push_back(def);
    else st->options.push_back(def);
    st->addOrder.push_back(def.dest);
    return a.none();
}

value parserParseArgs(cffi& a){
    ArgParserState* st = stateOf(a.self());
    std::vector<std::string> argv;
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value listVal = a.arg(0);
        if (listVal.type == "list" || listVal.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(listVal.ptr);
            for (auto& item : *items) argv.push_back(a.vm->toString(item));
        }
    }
    value ns;
    bool hadError = false;
    doParse(a, st, argv, ns, hadError);
    if (hadError || a.vm->hasError()) return a.none();
    return ns;
}

value parserParseKnownArgs(cffi& a){
    ArgParserState* st = stateOf(a.self());
    std::vector<std::string> argv;
    if (a.argc() > 0 && a.arg(0).type != "none"){
        value listVal = a.arg(0);
        if (listVal.type == "list" || listVal.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(listVal.ptr);
            for (auto& item : *items) argv.push_back(a.vm->toString(item));
        }
    }
    value ns;
    bool hadError = false;
    value leftover = doParse(a, st, argv, ns, hadError, true);
    if (hadError || a.vm->hasError()) return a.none();
    std::vector<value> pair{ns, leftover};
    return a.vm->makeTuple(pair);
}

value parserSetDefaults(cffi& a){
    ArgParserState* st = stateOf(a.self());
    for (auto& kv : a.kwargv){
        for (auto& def : st->options){
            if (def.dest == kv.first){ def.defaultValue = kv.second; def.hasDefault = true; }
        }
        for (auto& def : st->positionals){
            if (def.dest == kv.first){ def.defaultValue = kv.second; def.hasDefault = true; }
        }
    }
    return a.none();
}

value parserPrintUsage(cffi& a){
    std::cout << buildUsage(stateOf(a.self())) << "\n";
    std::cout.flush();
    return a.none();
}

value parserPrintHelp(cffi& a){
    ArgParserState* st = stateOf(a.self());
    std::cout << buildUsage(st) << "\n";
    if (!st->description.empty()) std::cout << "\n" << st->description << "\n";
    std::cout.flush();
    return a.none();
}

value parserErrorMethod(cffi& a){
    parserError(a, stateOf(a.self()), a.arg_str(0));
    return a.none();
}

// Namespace 构造器
// Namespace constructor
// Namespace ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value namespaceCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    std::vector<value> orderVals;
    for (auto& kv : a.kwargv){
        inst->attributes[kv.first] = kv.second;
        orderVals.push_back(a.mkstr(kv.first));
    }
    inst->attributes["_order"] = a.mklist(orderVals);
    return obj;
}

// Namespace __repr__: Namespace(a=1, b='x')
// Namespace __repr__: Namespace(a=1, b='x')
// Namespace __repr__
value namespaceRepr(cffi& a){
    std::vector<std::string> order;
    value orderVal = a.attr("_order");
    if (orderVal.type == "list" && orderVal.ptr){
        auto* items = static_cast<std::vector<value>*>(orderVal.ptr);
        for (auto& item : *items) order.push_back(a.vm->toString(item));
    }
    std::string out = "Namespace(";
    bool first = true;
    for (auto& name : order){
        if (!first) out += ", ";
        first = false;
        out += name + "=" + pythonRepr(*a.vm, a.attr(name));
    }
    out += ")";
    return a.mkstr(out);
}

} // namespace

// cffi argparse lib
// cffi argparse lib
// cffi argparse lib
ffilib cffi_argparse_lib(){
    ffi_class argumentParser = cffi_class(
        "ArgumentParser", (void*)parserCtor,
        {
            {"add_argument", (void*)parserAddArgument},
            {"parse_args", (void*)parserParseArgs},
            {"parse_known_args", (void*)parserParseKnownArgs},
            {"set_defaults", (void*)parserSetDefaults},
            {"print_usage", (void*)parserPrintUsage},
            {"print_help", (void*)parserPrintHelp},
            {"error", (void*)parserErrorMethod}
        });
    ffi_class ns = cffi_class(
        "Namespace", (void*)namespaceCtor,
        {
            {"__repr__", (void*)namespaceRepr},
            {"__str__", (void*)namespaceRepr}
        });

    return cffi_lib(
        "argparse",
        {},
        {},
        {
            {"ArgumentParser", argumentParser},
            {"Namespace", ns}
        });
}

} // namespace minipy
