#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 选项条目
// option entry
// ᠰᠣᠩᠭᠤᠯᠲᠠ
struct CpEntry{
    std::string key;
    std::string value;
    bool hasValue = true;
};

// 解析器状态
// parser state
// ᠰᠢᠨᠵᠢᠯᠡᠭᠴᠢ ᠪᠠᠶᠢᠳᠠᠯ
struct CpState{
    bool raw = false;
    bool allowNoValue = false;
    std::vector<CpEntry> defaults;
    std::vector<std::pair<std::string, std::vector<CpEntry>>> sections;
};

// 小写键
// lowercase key
// ᠵᠢᠵᠢᠭ ᠦᠰᠦᠭ
std::string lowerKey(const std::string& text){
    std::string out = text;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// 去除首尾空白
// trim whitespace
// ᠬᠣᠭᠤᠰᠤᠨ ᠠᠷᠢᠯᠭᠠᠬᠤ
std::string trimText(const std::string& text){
    size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

// 查找节
// find section
// ᠬᠡᠰᠡᠭ ᠬᠠᠶᠢᠬᠤ
std::vector<CpEntry>* findSection(CpState* state, const std::string& name){
    for (auto& item : state->sections)
        if (item.first == name) return &item.second;
    return nullptr;
}

// 查找条目
// find entry
// ᠲᠣᠬᠢᠶᠠᠯᠳᠤᠯᠭ᠎ᠠ ᠬᠠᠶᠢᠬᠤ
CpEntry* findEntry(std::vector<CpEntry>& entries, const std::string& key){
    for (auto& entry : entries)
        if (entry.key == key) return &entry;
    return nullptr;
}

// 读取选项值 (含默认回退)
// get option value with default fallback
// ᠲᠣᠬᠢᠶᠠᠯᠳᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠤᠨ᠎ᠠ
bool getRaw(CpState* state, const std::string& section, const std::string& option,
            std::string& value, bool& hasValue){
    std::string key = lowerKey(option);
    if (section != "DEFAULT"){
        auto* entries = findSection(state, section);
        if (!entries) return false;
        if (auto* entry = findEntry(*entries, key)){
            value = entry->value;
            hasValue = entry->hasValue;
            return true;
        }
    }
    if (auto* entry = findEntry(state->defaults, key)){
        value = entry->value;
        hasValue = entry->hasValue;
        return true;
    }
    return false;
}

// 插值替换
// interpolation substitute
// ᠣᠷᠤᠯᠴᠠᠭᠤᠯᠭ᠎ᠠ
bool interpolate(CpState* state, const std::string& section, const std::string& input,
                 std::string& output, std::string& error, int depth = 0){
    if (depth > 10){
        error = "InterpolationDepthError";
        return false;
    }
    std::string out;
    size_t i = 0;
    while (i < input.size()){
        char c = input[i];
        if (c == '%'){
            if (i + 1 < input.size() && input[i + 1] == '%'){
                out.push_back('%');
                i += 2;
                continue;
            }
            if (i + 1 < input.size() && input[i + 1] == '('){
                size_t close = input.find(")s", i + 2);
                if (close == std::string::npos){
                    error = "InterpolationSyntaxError: '%' must be followed by '%' or '(', found: '" +
                            input.substr(i) + "'";
                    return false;
                }
                std::string name = input.substr(i + 2, close - (i + 2));
                std::string inner;
                bool hasValue = true;
                if (!getRaw(state, section, name, inner, hasValue)){
                    error = "InterpolationMissingOptionError: Bad value substitution: option '" +
                            lowerKey(name) + "' in section '" + section +
                            "' contains an interpolation key";
                    return false;
                }
                std::string resolved;
                if (!interpolate(state, section, inner, resolved, error, depth + 1))
                    return false;
                out += resolved;
                i = close + 2;
                continue;
            }
            error = "InterpolationSyntaxError: '%' must be followed by '%' or '(', found: '" +
                    input.substr(i) + "'";
            return false;
        }
        out.push_back(c);
        ++i;
    }
    output = out;
    return true;
}

// 解析文本
// parse text
// ᠲᠡᠬᠰᠲ ᠰᠢᠨᠵᠢᠯᠡᠬᠦ
bool parseText(CpState* state, const std::string& text, const std::string& source,
               std::string& error){
    std::vector<std::string> lines;
    std::string current;
    for (char c : text){
        if (c == '\n'){
            if (!current.empty() && current.back() == '\r') current.pop_back();
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()){
        if (!current.empty() && current.back() == '\r') current.pop_back();
        lines.push_back(current);
    }
    std::vector<CpEntry>* target = &state->defaults;
    std::string curSection = "DEFAULT";
    CpEntry* lastEntry = nullptr;
    bool sawContent = false;
    for (size_t li = 0; li < lines.size(); ++li){
        std::string line = lines[li];
        std::string stripped = trimText(line);
        if (stripped.empty()) continue;
        if (stripped[0] == '#' || stripped[0] == ';') continue;
        if (!sawContent && stripped[0] != '['){
            error = "MissingSectionHeaderError: File contains no section headers.\n" +
                    source + ", line: " + std::to_string(li + 1) + "\n'" + line + "'";
            return false;
        }
        sawContent = true;
        if (stripped[0] == '['){
            size_t close = stripped.rfind(']');
            if (close == std::string::npos){
                error = "ParsingError: File contains parsing errors: " + source +
                        "\n\t[line " + std::to_string(li + 1) + "]: '" + line + "'";
                return false;
            }
            std::string name = trimText(stripped.substr(1, close - 1));
            if (findSection(state, name)){
                error = "DuplicateSectionError: While reading from " + source +
                        " [line " + std::to_string(li + 1) + "]: section '" + name +
                        "' already exists";
                return false;
            }
            if (name == "DEFAULT"){
                target = &state->defaults;
                curSection = "DEFAULT";
                lastEntry = nullptr;
                continue;
            }
            state->sections.push_back({name, {}});
            target = &state->sections.back().second;
            curSection = name;
            lastEntry = nullptr;
            continue;
        }
        bool continuation = !line.empty() &&
                            (line[0] == ' ' || line[0] == '\t');
        if (continuation && lastEntry){
            lastEntry->value += "\n" + stripped;
            continue;
        }
        size_t eq = stripped.find('=');
        size_t colon = stripped.find(':');
        size_t delim = std::string::npos;
        if (eq != std::string::npos && colon != std::string::npos)
            delim = std::min(eq, colon);
        else if (eq != std::string::npos) delim = eq;
        else delim = colon;
        std::string key;
        std::string val;
        bool hasValue = true;
        if (delim == std::string::npos){
            if (!state->allowNoValue){
                error = "ParsingError: File contains parsing errors: " + source +
                        "\n\t[line " + std::to_string(li + 1) + "]: '" + line + "'";
                return false;
            }
            key = lowerKey(trimText(stripped));
            hasValue = false;
        } else {
            key = lowerKey(trimText(stripped.substr(0, delim)));
            hasValue = true;
            val = trimText(stripped.substr(delim + 1));
        }
        if (key.empty()){
            error = "ParsingError: File contains parsing errors: " + source +
                    "\n\t[line " + std::to_string(li + 1) + "]: '" + line + "'";
            return false;
        }
        target->push_back({key, val, hasValue});
        lastEntry = &target->back();
    }
    return true;
}

// 取得状态
// get state from self
// ᠪᠠᠶᠢᠳᠠᠯ ᠬᠠᠯᠠᠬᠤ
CpState* cpStateOf(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        return static_cast<CpState*>(inst->handle);
    }
    return nullptr;
}

// 构造器
// constructor
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value cpConstructor(cffi& a, bool raw){
    auto* state = new CpState();
    state->raw = raw;
    state->allowNoValue = a.kwarg_bool("allow_no_value", false);
    return a.mkobject(state);
}

value fnRawCtor(cffi& a){ return cpConstructor(a, true); }
value fnCtor(cffi& a){ return cpConstructor(a, false); }

// sections()
// sections()
// sections()
value fnSections(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mklist({});
    std::vector<value> names;
    for (auto& item : state->sections) names.push_back(a.mkstr(item.first));
    return a.mklist(names);
}

// defaults()
// defaults()
// defaults()
value fnDefaults(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mkdict({});
    std::vector<std::pair<value, value>> items;
    for (auto& entry : state->defaults){
        value v = entry.hasValue ? a.mkstr(entry.value) : a.none();
        items.push_back(std::pair<value, value>(a.mkstr(entry.key), v));
    }
    return a.mkdict(items);
}

// add_section(name)
// add_section(name)
// add_section
value fnAddSection(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::string name = a.arg_str(0);
    if (name == "DEFAULT"){
        a.vm->raiseError("ValueError", "Invalid section name: DEFAULT");
        return a.none();
    }
    if (findSection(state, name)){
        a.vm->raiseError("DuplicateSectionError", "Section '" + name + "' already exists");
        return a.none();
    }
    state->sections.push_back({name, {}});
    return a.none();
}

// has_section(name)
// has_section(name)
// has_section
value fnHasSection(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mkbool(false);
    return a.mkbool(findSection(state, a.arg_str(0)) != nullptr);
}

// remove_section(name)
// remove_section(name)
// remove_section
value fnRemoveSection(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mkbool(false);
    std::string name = a.arg_str(0);
    for (size_t i = 0; i < state->sections.size(); ++i){
        if (state->sections[i].first == name){
            state->sections.erase(state->sections.begin() + static_cast<long long>(i));
            return a.mkbool(true);
        }
    }
    return a.mkbool(false);
}

// options(section)
// options(section)
// options
value fnOptions(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::string section = a.arg_str(0);
    std::vector<value> names;
    std::vector<std::string> seen;
    auto addKey = [&](const std::string& key){
        if (std::find(seen.begin(), seen.end(), key) == seen.end()){
            seen.push_back(key);
            names.push_back(a.mkstr(key));
        }
    };
    for (auto& entry : state->defaults) addKey(entry.key);
    if (auto* entries = findSection(state, section)){
        for (auto& entry : *entries) addKey(entry.key);
    } else if (section != "DEFAULT"){
        a.vm->raiseError("NoSectionError", "No section: '" + section + "'");
        return a.none();
    }
    return a.mklist(names);
}

// has_option(section, option)
// has_option
// has_option
value fnHasOption(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mkbool(false);
    std::string value;
    bool hasValue = false;
    return a.mkbool(getRaw(state, a.arg_str(0), a.arg_str(1), value, hasValue));
}

// 读取并插值
// get value with interpolation
// ᠤᠨ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
bool cpGet(cffi& a, CpState* state, const std::string& section, const std::string& option,
           std::string& value, bool raw, bool& missingSection, bool& missingOption){
    missingSection = false;
    missingOption = false;
    if (section != "DEFAULT" && !findSection(state, section)){
        missingSection = true;
        return false;
    }
    bool hasValue = true;
    if (!getRaw(state, section, option, value, hasValue)){
        missingOption = true;
        return false;
    }
    if (!hasValue){
        value = "";
        return true;
    }
    if (!raw && !state->raw){
        std::string out;
        std::string error;
        if (!interpolate(state, section, value, out, error)){
            size_t colon = error.find(':');
            std::string type = colon == std::string::npos ? error : error.substr(0, colon);
            std::string msg = colon == std::string::npos ? "" : trimText(error.substr(colon + 1));
            a.vm->raiseError(type, msg);
            return false;
        }
        value = out;
    }
    return true;
}

// get(section, option, *, raw=False, vars=None, fallback)
// get
// get
value fnGet(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    bool raw = a.kwarg_bool("raw", false);
    value fallback;
    bool hasFallback = a.has_kwarg("fallback");
    if (hasFallback) fallback = a.kwarg("fallback");
    std::string value;
    bool missingSection = false;
    bool missingOption = false;
    if (!cpGet(a, state, a.arg_str(0), a.arg_str(1), value, raw,
               missingSection, missingOption)){
        if (a.vm->hasError()) return a.none();
        if (hasFallback) return fallback;
        if (missingSection){
            a.vm->raiseError("NoSectionError", "No section: '" + a.arg_str(0) + "'");
            return a.none();
        }
        a.vm->raiseError("NoOptionError", "No option '" + lowerKey(a.arg_str(1)) +
                                          "' in section: '" + a.arg_str(0) + "'");
        return a.none();
    }
    return a.mkstr(value);
}

// set(section, option, value)
// set
// set
value fnSet(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::string section = a.arg_str(0);
    std::string key = lowerKey(a.arg_str(1));
    std::string val = a.argc() > 2 ? a.vm->toString(a.arg(2)) : "";
    std::vector<CpEntry>* entries = nullptr;
    if (section == "DEFAULT") entries = &state->defaults;
    else entries = findSection(state, section);
    if (!entries){
        a.vm->raiseError("NoSectionError", "No section: '" + section + "'");
        return a.none();
    }
    if (auto* entry = findEntry(*entries, key)){
        entry->value = val;
        entry->hasValue = true;
    } else {
        entries->push_back({key, val, true});
    }
    return a.none();
}

// remove_option(section, option)
// remove_option
// remove_option
value fnRemoveOption(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mkbool(false);
    std::string section = a.arg_str(0);
    std::string key = lowerKey(a.arg_str(1));
    auto* entries = findSection(state, section);
    if (!entries){
        a.vm->raiseError("NoSectionError", "No section: '" + section + "'");
        return a.mkbool(false);
    }
    for (size_t i = 0; i < entries->size(); ++i){
        if ((*entries)[i].key == key){
            entries->erase(entries->begin() + static_cast<long long>(i));
            return a.mkbool(true);
        }
    }
    return a.mkbool(false);
}

// getint / getfloat / getboolean
// typed getters
// ᠲᠣᠭ᠎ᠠ ᠬᠠᠯᠠᠬᠤ
value typedGet(cffi& a, int kind){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    bool raw = a.kwarg_bool("raw", false);
    value fallback;
    bool hasFallback = a.has_kwarg("fallback");
    if (hasFallback) fallback = a.kwarg("fallback");
    std::string value;
    bool missingSection = false;
    bool missingOption = false;
    if (!cpGet(a, state, a.arg_str(0), a.arg_str(1), value, raw,
               missingSection, missingOption)){
        if (a.vm->hasError()) return a.none();
        if (hasFallback) return fallback;
        if (missingSection)
            a.vm->raiseError("NoSectionError", "No section: '" + a.arg_str(0) + "'");
        else
            a.vm->raiseError("NoOptionError", "No option '" + lowerKey(a.arg_str(1)) +
                                              "' in section: '" + a.arg_str(0) + "'");
        return a.none();
    }
    std::string lowered = value;
    for (auto& c : lowered) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (kind == 0){
        try {
            size_t used = 0;
            long long number = std::stoll(trimText(value), &used);
            return a.mkint(number);
        } catch (...){
            if (hasFallback) return fallback;
            a.vm->raiseError("ValueError", "invalid literal for int() with base 10: '" +
                                           value + "'");
            return a.none();
        }
    }
    if (kind == 1){
        try {
            double number = std::stod(trimText(value));
            return a.mkfloat(number);
        } catch (...){
            if (hasFallback) return fallback;
            a.vm->raiseError("ValueError", "could not convert string to float: '" +
                                           value + "'");
            return a.none();
        }
    }
    if (lowered == "1" || lowered == "yes" || lowered == "true" || lowered == "on")
        return a.mkbool(true);
    if (lowered == "0" || lowered == "no" || lowered == "false" || lowered == "off")
        return a.mkbool(false);
    if (hasFallback) return fallback;
    a.vm->raiseError("ValueError", "Not a boolean: " + value);
    return a.none();
}

value fnGetInt(cffi& a){ return typedGet(a, 0); }
value fnGetFloat(cffi& a){ return typedGet(a, 1); }
value fnGetBool(cffi& a){ return typedGet(a, 2); }

// items()
// items()
// items
value fnItems(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.mklist({});
    std::vector<CpEntry> merged = state->defaults;
    bool raw = a.kwarg_bool("raw", false);
    std::string section;
    if (a.argc() > 0 && a.arg(0).type == "str"){
        section = a.arg_str(0);
        auto* entries = findSection(state, section);
        if (!entries){
            a.vm->raiseError("NoSectionError", "No section: '" + section + "'");
            return a.none();
        }
        for (auto& entry : *entries){
            if (auto* existing = findEntry(merged, entry.key)) *existing = entry;
            else merged.push_back(entry);
        }
    }
    std::vector<value> pairs;
    for (auto& entry : merged){
        std::string val = entry.value;
        if (entry.hasValue && !raw && !state->raw){
            std::string out;
            std::string error;
            if (interpolate(state, section.empty() ? "DEFAULT" : section,
                            entry.value, out, error)) val = out;
        }
        value v = entry.hasValue ? a.mkstr(val) : a.none();
        pairs.push_back(a.vm->makeTuple({a.mkstr(entry.key), v}));
    }
    return a.mklist(pairs);
}

// read(filenames)
// read
// read
value fnRead(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::vector<value> files;
    a.vm->materializeIterable(a.arg(0), files);
    if (files.empty() && a.arg(0).type == "str") files.push_back(a.arg(0));
    std::vector<value> loaded;
    for (auto& f : files){
        std::string path = a.vm->toString(f);
        std::ifstream in(path, std::ios::binary);
        if (!in) continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string error;
        if (!parseText(state, ss.str(), "'" + path + "'", error)){
            a.vm->raiseError(error.substr(0, error.find(':')), error);
            return a.none();
        }
        loaded.push_back(a.mkstr(path));
    }
    return a.mklist(loaded);
}

// read_string(text, source='<string>')
// read_string
// read_string
value fnReadString(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::string source = a.argc() > 1 ? "'" + a.arg_str(1) + "'" : "'<string>'";
    std::string error;
    if (!parseText(state, a.arg_str(0), source, error)){
        size_t colon = error.find(':');
        std::string type = error.substr(0, colon);
        std::string msg = colon == std::string::npos ? "" : trimText(error.substr(colon + 1));
        a.vm->raiseError(type, msg.empty() ? error : msg);
        return a.none();
    }
    return a.none();
}

// read_file(file)
// read_file
// read_file
value fnReadFile(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    value result;
    if (!cffiCallMethod(*a.vm, a.arg(0), "read", {}, result) ||
        result.type != "str" || !result.ptr){
        a.vm->raiseError("TypeError", "file object expected");
        return a.none();
    }
    std::string error;
    if (!parseText(state, *static_cast<std::string*>(result.ptr), "'<file>'", error)){
        size_t colon = error.find(':');
        std::string type = error.substr(0, colon);
        std::string msg = colon == std::string::npos ? "" : trimText(error.substr(colon + 1));
        a.vm->raiseError(type, msg.empty() ? error : msg);
        return a.none();
    }
    return a.none();
}

// read_dict(dict)
// read_dict
// read_dict
value fnReadDict(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    value input = a.arg(0);
    if (input.type != "dict" || !input.ptr) return a.none();
    for (auto& sectionPair : *static_cast<dict_t*>(input.ptr)){
        std::string section = a.vm->toString(sectionPair.first);
        std::vector<CpEntry>* entries = findSection(state, section);
        if (!entries){
            state->sections.push_back({section, {}});
            entries = &state->sections.back().second;
        }
        if (sectionPair.second.type != "dict" || !sectionPair.second.ptr) continue;
        for (auto& optPair : *static_cast<dict_t*>(sectionPair.second.ptr)){
            std::string key = lowerKey(a.vm->toString(optPair.first));
            std::string val = a.vm->toString(optPair.second);
            if (auto* existing = findEntry(*entries, key)){
                existing->value = val;
                existing->hasValue = true;
            } else {
                entries->push_back({key, val, true});
            }
        }
    }
    return a.none();
}

// write(file)
// write
// write
value fnWrite(cffi& a){
    CpState* state = cpStateOf(a);
    if (!state) return a.none();
    std::ostringstream out;
    if (!state->defaults.empty()){
        out << "[DEFAULT]\n";
        for (auto& entry : state->defaults)
            if (entry.hasValue) out << entry.key << " = " << entry.value << "\n";
            else out << entry.key << "\n";
        out << "\n";
    }
    for (auto& section : state->sections){
        out << "[" << section.first << "]\n";
        for (auto& entry : section.second)
            if (entry.hasValue) out << entry.key << " = " << entry.value << "\n";
            else out << entry.key << "\n";
        out << "\n";
    }
    value result;
    cffiCallMethod(*a.vm, a.arg(0), "write", {a.mkstr(out.str())}, result);
    return a.none();
}

// optionxform(text)
// optionxform
// optionxform
value fnOptionXform(cffi& a){
    return a.mkstr(lowerKey(a.arg_str(0)));
}

// get 默认节名
// DEFAULTSECT data
// DEFAULT ᠨᠡᠷᠡ
value dataDefaultSect(cffi& a){ return a.mkstr("DEFAULT"); }

}

// cffi configparser 库
// cffi configparser library
// cffi configparser ᠨᠣᠮ
ffilib cffi_configparser_lib(){
    ffi_class rawClass;
    rawClass.name = "RawConfigParser";
    rawClass.constructor = (void*)fnRawCtor;
    rawClass.methods = {
        {"sections", (void*)fnSections},
        {"defaults", (void*)fnDefaults},
        {"add_section", (void*)fnAddSection},
        {"has_section", (void*)fnHasSection},
        {"remove_section", (void*)fnRemoveSection},
        {"options", (void*)fnOptions},
        {"has_option", (void*)fnHasOption},
        {"get", (void*)fnGet},
        {"set", (void*)fnSet},
        {"remove_option", (void*)fnRemoveOption},
        {"getint", (void*)fnGetInt},
        {"getfloat", (void*)fnGetFloat},
        {"getboolean", (void*)fnGetBool},
        {"items", (void*)fnItems},
        {"read", (void*)fnRead},
        {"read_string", (void*)fnReadString},
        {"read_file", (void*)fnReadFile},
        {"read_dict", (void*)fnReadDict},
        {"write", (void*)fnWrite},
        {"optionxform", (void*)fnOptionXform}
    };

    ffi_class parserClass = rawClass;
    parserClass.name = "ConfigParser";
    parserClass.constructor = (void*)fnCtor;

    ffi_class errorClass;
    errorClass.name = "Error";
    ffi_class noSectionClass;
    noSectionClass.name = "NoSectionError";
    ffi_class dupSectionClass;
    dupSectionClass.name = "DuplicateSectionError";
    ffi_class noOptionClass;
    noOptionClass.name = "NoOptionError";
    ffi_class interpolationClass;
    interpolationClass.name = "InterpolationError";
    ffi_class missingHeaderClass;
    missingHeaderClass.name = "MissingSectionHeaderError";
    ffi_class parsingClass;
    parsingClass.name = "ParsingError";

    return cffi_lib(
        "configparser",
        {},
        {
            {"DEFAULTSECT", (void*)dataDefaultSect}
        },
        {
            {"RawConfigParser", rawClass},
            {"ConfigParser", parserClass},
            {"SafeConfigParser", parserClass},
            {"Error", errorClass},
            {"NoSectionError", noSectionClass},
            {"DuplicateSectionError", dupSectionClass},
            {"NoOptionError", noOptionClass},
            {"InterpolationError", interpolationClass},
            {"InterpolationMissingOptionError", interpolationClass},
            {"InterpolationSyntaxError", interpolationClass},
            {"MissingSectionHeaderError", missingHeaderClass},
            {"ParsingError", parsingClass}
        });
}

}
