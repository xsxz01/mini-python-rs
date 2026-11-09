#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {

namespace {

// 短选项定义
// Short option definition
// ᠪᠣᠭᠤᠨᠢ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠲᠣᠭᠲᠠᠭᠤᠯᠤᠯ
struct ShortOpt{
    char name = 0;
    int mode = 0;
};

// 长选项定义
// Long option definition
// ᠤᠷᠲᠤ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠲᠣᠭᠲᠠᠭᠤᠯᠤᠯ
struct LongOpt{
    std::string name;
    int mode = 0;
};

// 解析结果
// Parse result
// ᠲᠠᠢᠯᠠᠭᠰᠠᠨ ᠦᠷ᠎ᠡ
struct ParseResult{
    std::vector<std::pair<std::string, std::string>> options;
    std::vector<std::string> rest;
};

// 读取字符串列表
// Read a list of strings
// ᠦᠰᠦᠭ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠢ ᠦᠨᠰᠢᠬᠦ
bool readStrings(cffi& a, const value& source, std::vector<std::string>& out){
    std::vector<value> items;
    a.vm->materializeIterable(source, items);
    if (a.vm->hasError()) return false;
    for (auto& item : items) out.push_back(a.vm->toString(item));
    return true;
}

// 解析短选项串
// Parse the short option specification
// ᠪᠣᠭᠤᠨᠢ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠦᠰᠦᠭ ᠢ ᠲᠠᠢᠯᠠᠬᠤ
std::vector<ShortOpt> parseShortSpec(const std::string& spec){
    std::vector<ShortOpt> result;
    size_t i = 0;
    while (i < spec.size()){
        ShortOpt opt;
        opt.name = spec[i];
        i++;
        if (i < spec.size() && spec[i] == ':'){
            opt.mode = 1;
            i++;
            if (i < spec.size() && spec[i] == ':'){
                opt.mode = 2;
                i++;
            }
        }
        result.push_back(opt);
    }
    return result;
}

// 解析长选项列表
// Parse the long option list
// ᠤᠷᠲᠤ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠢ ᠲᠠᠢᠯᠠᠬᠤ
std::vector<LongOpt> parseLongSpec(const std::vector<std::string>& spec){
    std::vector<LongOpt> result;
    for (const auto& item : spec){
        LongOpt opt;
        if (!item.empty() && item.back() == '='){
            opt.name = item.substr(0, item.size() - 1);
            opt.mode = 1;
        } else {
            opt.name = item;
            opt.mode = 0;
        }
        result.push_back(opt);
    }
    return result;
}

// 查找短选项
// Find a short option
// ᠪᠣᠭᠤᠨᠢ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠢ ᠬᠠᠯᠠᠬᠤ
const ShortOpt* findShort(const std::vector<ShortOpt>& options, char name){
    for (const auto& opt : options){
        if (opt.name == name) return &opt;
    }
    return nullptr;
}

// 查找长选项 (支持唯一前缀)
// Find a long option (unique prefix allowed)
// ᠤᠷᠲᠤ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠢ ᠬᠠᠯᠠᠬᠤ
int findLong(const std::vector<LongOpt>& options, const std::string& name, bool& ambiguous){
    ambiguous = false;
    int match = -1;
    for (size_t i = 0; i < options.size(); ++i){
        if (options[i].name == name) return static_cast<int>(i);
        if (options[i].name.compare(0, name.size(), name) == 0){
            if (match >= 0) ambiguous = true;
            else match = static_cast<int>(i);
        }
    }
    return match;
}

// 抛出不识别选项错误
// Raise an unrecognized-option error
// ᠲᠠᠨᠢᠭᠳᠠᠭ᠎ᠠ ᠦᠭᠡᠢ ᠰᠣᠨᠭᠣᠯᠲᠠ ᠶᠢᠨ ᠠᠯᠳᠠᠭ᠎ᠠ
value raiseUnknown(cffi& a, const std::string& opt){
    a.vm->raiseError("GetoptError", "option " + opt + " not recognized");
    return a.none();
}

// 转为返回值
// Build the return value
// ᠦᠷ᠎ᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value buildResult(cffi& a, const ParseResult& parsed){
    std::vector<value> optionValues;
    for (const auto& opt : parsed.options){
        std::vector<value> pair = {a.mkstr(opt.first), a.mkstr(opt.second)};
        optionValues.push_back(a.vm->makeTuple(pair));
    }
    std::vector<value> rest;
    for (const auto& item : parsed.rest) rest.push_back(a.mkstr(item));
    std::vector<value> result = {a.mklist(optionValues), a.mklist(rest)};
    return a.vm->makeTuple(result);
}

// 通用解析入口
// Common parse entry
// ᠨᠢᠶᠢᠲᠡ ᠲᠠᠢᠯᠠᠯ ᠦᠨ ᠣᠷᠣᠯᠭ᠎ᠠ
value parseOptions(cffi& a, bool gnuMode){
    std::vector<std::string> args;
    if (!readStrings(a, a.arg(0), args)) return a.none();
    std::string shortSpec = a.arg_str(1);
    std::vector<std::string> longSpec;
    if (a.argc() > 2 && a.arg(2).type != "none"){
        if (!readStrings(a, a.arg(2), longSpec)) return a.none();
    }
    auto shortOptions = parseShortSpec(shortSpec);
    auto longOptions = parseLongSpec(longSpec);
    ParseResult parsed;
    size_t i = 0;
    while (i < args.size()){
        std::string token = args[i];
        if (token == "--"){
            i++;
            while (i < args.size()) parsed.rest.push_back(args[i++]);
            break;
        }
        if (token.size() >= 2 && token[0] == '-' && token[1] == '-'){
            std::string body = token.substr(2);
            std::string name = body;
            std::string attached;
            bool hasAttached = false;
            size_t eq = body.find('=');
            if (eq != std::string::npos){
                name = body.substr(0, eq);
                attached = body.substr(eq + 1);
                hasAttached = true;
            }
            bool ambiguous = false;
            int index = findLong(longOptions, name, ambiguous);
            if (index < 0) return raiseUnknown(a, "--" + name);
            if (ambiguous){
                a.vm->raiseError("GetoptError", "option --" + name + " not a unique prefix");
                return a.none();
            }
            const LongOpt& opt = longOptions[static_cast<size_t>(index)];
            std::string value;
            if (opt.mode == 1){
                if (hasAttached) value = attached;
                else if (i + 1 < args.size()) value = args[++i];
                else {
                    a.vm->raiseError("GetoptError", "option --" + opt.name + " requires argument");
                    return a.none();
                }
            } else if (hasAttached){
                a.vm->raiseError("GetoptError", "option --" + opt.name + " must not have an argument");
                return a.none();
            }
            parsed.options.push_back({"--" + opt.name, value});
            i++;
            continue;
        }
        if (token.size() >= 2 && token[0] == '-'){
            size_t pos = 1;
            bool consumedNext = false;
            while (pos < token.size()){
                char name = token[pos];
                const ShortOpt* opt = findShort(shortOptions, name);
                if (!opt){
                    return raiseUnknown(a, std::string("-") + name);
                }
                std::string value;
                if (opt->mode == 1){
                    if (pos + 1 < token.size()){
                        value = token.substr(pos + 1);
                    } else if (i + 1 < args.size()){
                        value = args[++i];
                        consumedNext = true;
                    } else {
                        a.vm->raiseError("GetoptError", std::string("option -") + name +
                                                             " requires argument");
                        return a.none();
                    }
                    parsed.options.push_back({std::string("-") + name, value});
                    pos = token.size();
                    break;
                }
                if (opt->mode == 2){
                    if (pos + 1 < token.size()) value = token.substr(pos + 1);
                    parsed.options.push_back({std::string("-") + name, value});
                    pos = token.size();
                    break;
                }
                parsed.options.push_back({std::string("-") + name, ""});
                pos++;
            }
            (void)consumedNext;
            i++;
            continue;
        }
        if (!gnuMode){
            while (i < args.size()) parsed.rest.push_back(args[i++]);
            break;
        }
        parsed.rest.push_back(token);
        i++;
    }
    return buildResult(a, parsed);
}
}

// fn Getopt
// fn getopt
// fn Getopt
static value fnGetopt(cffi& a){
    return parseOptions(a, false);
}

// fn Gnu Getopt
// fn gnu_getopt
// fn Gnu Getopt
static value fnGnuGetopt(cffi& a){
    return parseOptions(a, true);
}

// cffi getopt lib
// cffi getopt lib
// cffi getopt lib
ffilib cffi_getopt_lib(){
    return cffi_lib(
        "getopt",
        {
            {"getopt", (void*)fnGetopt},
            {"gnu_getopt", (void*)fnGnuGetopt}
        });
}

}
