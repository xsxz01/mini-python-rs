#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// Morsel 保留属性显示名
// Morsel reserved attribute display names
// Morsel ᠨᠡᠷᠡ ᠶᠢᠨ ᠬᠤᠪᠢᠯᠪᠤᠷᠢ
std::map<std::string, std::string>& reservedNames(){
    static std::map<std::string, std::string> table = {
        {"comment", "Comment"}, {"domain", "Domain"}, {"expires", "Expires"},
        {"httponly", "HttpOnly"}, {"max-age", "Max-Age"}, {"path", "Path"},
        {"samesite", "SameSite"}, {"secure", "Secure"}, {"version", "Version"}};
    return table;
}

// 取实例
// Get ffi instance from self
// ᠶᠢᠨᠢᠰ ᠬᠠᠯᠠᠬᠤ
ffi_instance* instOf(const value& self){
    if (self.type != "native_object" || !self.ptr) return nullptr;
    return static_cast<ffi_instance*>(self.ptr);
}

// 属性字符串
// Read attribute as string
// ᠲᠠᠯᠠᠭ᠌ ᠦᠨ ᠪᠢᠴᠢᠭ
std::string attrStr(ffi_instance* inst, const std::string& name){
    if (!inst) return "";
    auto it = inst->attributes.find(name);
    if (it == inst->attributes.end() || it->second.type == "none") return "";
    return (it->second.type == "str" && it->second.ptr)
           ? *static_cast<std::string*>(it->second.ptr)
           : "";
}

// 属性整数
// Read attribute as int
// ᠲᠠᠯᠠᠭ᠌ ᠦᠨ ᠲᠣᠭ᠎ᠠ
long long attrInt(ffi_instance* inst, const std::string& name){
    if (!inst) return 0;
    auto it = inst->attributes.find(name);
    if (it == inst->attributes.end()) return 0;
    if (it->second.type == "int" && it->second.ptr)
        return *static_cast<long long*>(it->second.ptr);
    if (it->second.type == "float" && it->second.ptr)
        return static_cast<long long>(*static_cast<double*>(it->second.ptr));
    if (it->second.type == "bool" && it->second.ptr)
        return *static_cast<bool*>(it->second.ptr) ? 1 : 0;
    return 0;
}

// 合法 Cookie 字符
// Legal cookie character
// ᠴᠥᠪᠡᠷᠡ ᠲᠡᠮᠳᠡᠭ
bool legalChar(char c){
    if (std::isalnum(static_cast<unsigned char>(c))) return true;
    static const char* extra = "!#$%&'*+-.^_`|~:";
    return std::strchr(extra, c) != nullptr;
}

// Cookie 值编码
// Cookie value encode
// ᠬᠥᠷᠢᠶ᠎ᠡ ᠬᠤᠪᠢᠷᠠᠭᠤᠯᠭ᠎ᠠ
std::string quoteValue(const std::string& text){
    bool needs = false;
    for (char c : text){
        if (!legalChar(c)){ needs = true; break; }
    }
    if (!needs) return text;
    std::string out;
    for (char c : text){
        if (legalChar(c)){
            out.push_back(c);
        } else if (c > 0x20 && c < 0x7f){
            out.push_back('\\');
            out.push_back(c);
        } else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\%03o", static_cast<int>(c));
            out += buf;
        }
    }
    return out;
}

// 去引号
// Strip surrounding quotes
// ᠬᠠᠰᠤᠯᠤᠯ
std::string unquote(const std::string& text){
    std::string s = text;
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        s = s.substr(1, s.size() - 2);
    std::string out;
    for (size_t i = 0; i < s.size(); ++i){
        if (s[i] == '\\' && i + 1 < s.size()){
            out.push_back(s[i + 1]);
            ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

// 去除空白
// Trim whitespace
// ᠰᠢᠯᠢᠭᠤᠯᠤᠯ
std::string trim(const std::string& text){
    size_t a = 0, b = text.size();
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    return text.substr(a, b - a);
}

// 小写化
// Lowercase
// ᠳᠣᠣᠷᠠᠰᠢ ᠳᠠᠭᠠᠭᠤᠯᠤᠯ
std::string lower(std::string text){
    for (auto& c : text) c = static_cast<char>(std::tolower(c));
    return text;
}

// 生成 Morsel 对象
// Build a Morsel object
// Morsel ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeMorsel(cffi& a, const std::string& key, const std::string& rawValue,
                 const std::string& coded){
    const ffi_class* klass = nullptr;
    if (a.library){
        auto it = a.library->classes.find("Morsel");
        if (it != a.library->classes.end()) klass = &it->second;
    }
    value obj = a.vm->makeNativeObject(a.library, klass, nullptr);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["key"] = a.mkstr(key);
        inst->attributes["value"] = a.mkstr(rawValue);
        inst->attributes["coded_value"] = a.mkstr(coded);
    }
    return obj;
}

// Morsel 构造
// Morsel constructor
// Morsel ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnMorselCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["key"] = a.mkstr("");
        inst->attributes["value"] = a.mkstr("");
        inst->attributes["coded_value"] = a.mkstr("");
    }
    return obj;
}

// Morsel.set
// Morsel.set
// Morsel.set
static value fnMorselSet(cffi& a){
    auto* inst = instOf(a.self());
    if (!inst) return a.none();
    inst->attributes["key"] = a.mkstr(a.arg_str(0));
    inst->attributes["value"] = a.mkstr(a.arg_str(1));
    inst->attributes["coded_value"] = a.mkstr(a.arg_str(2));
    return a.mkstr(a.arg_str(2));
}

// Morsel.__getitem__
// Morsel.__getitem__
// Morsel.__getitem__
static value fnMorselGetitem(cffi& a){
    auto* inst = instOf(a.self());
    std::string key = a.arg_str(0);
    if (!reservedNames().count(key)){
        a.vm->raiseError("CookieError", "Invalid attribute name " + key);
        return a.none();
    }
    if (!inst) return a.mkstr("");
    auto it = inst->attributes.find(key);
    if (it == inst->attributes.end() || it->second.type == "none") return a.mkstr("");
    return it->second;
}

// Morsel.__setitem__
// Morsel.__setitem__
// Morsel.__setitem__
static value fnMorselSetitem(cffi& a){
    auto* inst = instOf(a.self());
    std::string key = a.arg_str(0);
    if (!reservedNames().count(key)){
        a.vm->raiseError("CookieError", "Invalid attribute name " + key);
        return a.none();
    }
    if (inst) inst->attributes[key] = a.arg(1);
    return a.none();
}

// Morsel.__contains__
// Morsel.__contains__
// Morsel.__contains__
static value fnMorselContains(cffi& a){
    auto* inst = instOf(a.self());
    std::string key = a.arg_str(0);
    if (!inst || !reservedNames().count(key)) return a.mkbool(false);
    auto it = inst->attributes.find(key);
    return a.mkbool(it != inst->attributes.end() && it->second.type != "none");
}

// 格式化单个 Morsel
// Format one Morsel
// Morsel ᠶᠢᠨ ᠬᠡᠯᠪᠡᠷᠢ
std::string formatMorsel(cffi& a, ffi_instance* inst){
    std::string key = attrStr(inst, "key");
    std::string coded = attrStr(inst, "coded_value");
    std::string result = key + "=" + coded;
    for (auto& entry : reservedNames()){
        const std::string& attr = entry.first;
        if (!inst) continue;
        auto it = inst->attributes.find(attr);
        if (it == inst->attributes.end() || it->second.type == "none") continue;
        std::string text;
        if (it->second.type == "str" && it->second.ptr)
            text = *static_cast<std::string*>(it->second.ptr);
        else if (it->second.type == "int")
            text = std::to_string(attrInt(inst, attr));
        else if (it->second.type == "bool")
            text = attrInt(inst, attr) ? "True" : "False";
        else
            text = a.vm->toString(it->second);
        if (attr == "secure" || attr == "httponly"){
            // 标志属性仅输出名称 (CPython: "; Secure")
            // Flag attributes print name only (CPython: "; Secure")
            if (!attrInt(inst, attr)) continue;
            result += "; " + entry.second;
            continue;
        }
        if (text.empty()) continue;
        if (attr == "comment" && it->second.type == "str") text = quoteValue(text);
        result += "; " + entry.second + "=" + text;
    }
    return result;
}

// Morsel.OutputString
// Morsel.OutputString
// Morsel.OutputString
static value fnMorselOutput(cffi& a){
    return a.mkstr(formatMorsel(a, instOf(a.self())));
}

// 格式化 JS 输出
// Format JS output
// JS ᠭᠠᠷᠭ᠎ᠠ
std::string formatJs(ffi_instance* inst){
    std::string key = attrStr(inst, "key");
    std::string coded = attrStr(inst, "coded_value");
    return "<script type=\"text/javascript\">\n<!-- begin hiding\n"
           "document.cookie = " + key + "=" + coded + ";\n"
           "// end hiding -->\n</script>\n";
}

// Morsel.js_output
// Morsel.js_output
// Morsel.js_output
static value fnMorselJs(cffi& a){
    return a.mkstr(formatJs(instOf(a.self())));
}

// Morsel.__str__
// Morsel.__str__
// Morsel.__str__
static value fnMorselStr(cffi& a){
    return fnMorselOutput(a);
}

// Cookie 条目列表
// Cookie item list
// Cookie ᠵᠢᠭᠰᠠᠭᠤᠯᠤᠯ
std::vector<value>* cookieItems(cffi& a, const value& self, bool create){
    auto* inst = instOf(self);
    if (!inst) return nullptr;
    auto it = inst->attributes.find("_morsels");
    if (it == inst->attributes.end() || it->second.type != "list" || !it->second.ptr){
        if (!create) return nullptr;
        inst->attributes["_morsels"] = a.vm->makeList({});
        it = inst->attributes.find("_morsels");
    }
    return static_cast<std::vector<value>*>(it->second.ptr);
}

// 查找 Morsel
// Find a morsel by key
// Morsel ᠬᠠᠯᠠᠬᠤ
value findMorsel(cffi& a, const value& self, const std::string& key){
    std::vector<value>* items = cookieItems(a, self, false);
    if (!items) return a.none();
    for (auto& item : *items){
        auto* inst = instOf(item);
        if (inst && attrStr(inst, "key") == key) return item;
    }
    return a.none();
}

// Cookie 构造
// Cookie constructor
// Cookie ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᢆ
static value fnCookieCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["_morsels"] = a.vm->makeList({});
    }
    return obj;
}

// Cookie.__setitem__
// Cookie.__setitem__
// Cookie.__setitem__
static value fnCookieSetitem(cffi& a){
    std::string key = a.arg_str(0);
    value valueObj = a.arg(1);
    if (valueObj.type == "native_object" && valueObj.ptr &&
        static_cast<ffi_instance*>(valueObj.ptr)->klass &&
        static_cast<ffi_instance*>(valueObj.ptr)->klass->name == "Morsel"){
        // 直接放入 Morsel
        std::vector<value>* items = cookieItems(a, a.self(), true);
        if (items){
            value existing = findMorsel(a, a.self(), key);
            if (existing.type == "native_object"){
                for (auto& item : *items){
                    if (item.ptr == existing.ptr){ item = valueObj; break; }
                }
            } else {
                items->push_back(valueObj);
            }
        }
        return a.none();
    }
    std::string text = valueObj.type == "str" ? a.vm->toString(valueObj)
                                              : a.vm->toString(valueObj);
    std::vector<value>* items = cookieItems(a, a.self(), true);
    value existing = findMorsel(a, a.self(), key);
    if (existing.type == "native_object" && existing.ptr){
        auto* inst = static_cast<ffi_instance*>(existing.ptr);
        inst->attributes["value"] = a.mkstr(text);
        inst->attributes["coded_value"] = a.mkstr(quoteValue(text));
        return a.none();
    }
    if (items) items->push_back(makeMorsel(a, key, text, quoteValue(text)));
    return a.none();
}

// Cookie.__getitem__
// Cookie.__getitem__
// Cookie.__getitem__
static value fnCookieGetitem(cffi& a){
    value found = findMorsel(a, a.self(), a.arg_str(0));
    if (found.type == "native_object") return found;
    a.vm->raiseError("KeyError", a.arg_str(0));
    return a.none();
}

// Cookie.__contains__
// Cookie.__contains__
// Cookie.__contains__
static value fnCookieContains(cffi& a){
    value found = findMorsel(a, a.self(), a.arg_str(0));
    return a.mkbool(found.type == "native_object");
}

// Cookie.__len__
// Cookie.__len__
// Cookie.__len__
static value fnCookieLen(cffi& a){
    std::vector<value>* items = cookieItems(a, a.self(), false);
    return a.mkint(items ? static_cast<long long>(items->size()) : 0);
}

// Cookie.keys
// Cookie.keys
// Cookie.keys
static value fnCookieKeys(cffi& a){
    std::vector<value> out;
    std::vector<value>* items = cookieItems(a, a.self(), false);
    if (items){
        for (auto& item : *items){
            auto* inst = instOf(item);
            if (inst) out.push_back(a.mkstr(attrStr(inst, "key")));
        }
    }
    return a.mklist(out);
}

// Cookie.values
// Cookie.values
// Cookie.values
static value fnCookieValues(cffi& a){
    std::vector<value> out;
    std::vector<value>* items = cookieItems(a, a.self(), false);
    if (items) out.assign(items->begin(), items->end());
    return a.mklist(out);
}

// Cookie.items
// Cookie.items
// Cookie.items
static value fnCookieItems(cffi& a){
    std::vector<value> out;
    std::vector<value>* items = cookieItems(a, a.self(), false);
    if (items){
        for (auto& item : *items){
            auto* inst = instOf(item);
            if (!inst) continue;
            out.push_back(a.vm->makeTuple({a.mkstr(attrStr(inst, "key")), item}));
        }
    }
    return a.mklist(out);
}

// Cookie.get
// Cookie.get
// Cookie.get
static value fnCookieGet(cffi& a){
    value found = findMorsel(a, a.self(), a.arg_str(0));
    if (found.type == "native_object") return found;
    return a.argc() > 1 ? a.arg(1) : a.none();
}

// Cookie.load
// Cookie.load
// Cookie.load
static value fnCookieLoad(cffi& a){
    std::string raw = a.arg_str(0);
    std::vector<value>* items = cookieItems(a, a.self(), true);
    if (items) items->clear();
    value current = a.none();
    std::string text = raw;
    size_t pos = 0;
    bool first = true;
    while (pos < text.size()){
        size_t semi = pos;
        bool inQuotes = false;
        while (semi < text.size()){
            char c = text[semi];
            if (c == '"') inQuotes = !inQuotes;
            if (c == ';' && !inQuotes) break;
            ++semi;
        }
        std::string token = trim(text.substr(pos, semi - pos));
        pos = semi + 1;
        if (token.empty()) continue;
        size_t eq = token.find('=');
        if (eq != std::string::npos){
            std::string name = trim(token.substr(0, eq));
            std::string val = trim(token.substr(eq + 1));
            std::string lowerName = lower(name);
            if (!first && reservedNames().count(lowerName)){
                auto* inst = instOf(current);
                if (inst){
                    if (lowerName == "max-age"){
                        try{ inst->attributes[lowerName] = a.mkint(std::stoll(val)); }
                        catch (...){ inst->attributes[lowerName] = a.mkstr(val); }
                    } else {
                        inst->attributes[lowerName] = a.mkstr(unquote(val));
                    }
                }
                continue;
            }
            std::string plain = unquote(val);
            value morsel = makeMorsel(a, name, plain, quoteValue(plain));
            if (items) items->push_back(morsel);
            current = morsel;
            first = false;
        } else {
            std::string lowerToken = lower(token);
            auto* inst = instOf(current);
            if (inst && (lowerToken == "secure" || lowerToken == "httponly"))
                inst->attributes[lowerToken] = a.mkbool(true);
        }
    }
    return a.none();
}

// Cookie.output
// Cookie.output
// Cookie.output
static value fnCookieOutput(cffi& a){
    std::string header = "Set-Cookie:";
    if (a.has_kwarg("header")) header = a.kwarg_str("header", header);
    else if (a.argc() > 1) header = a.arg_str(1);
    // CPython: 各 morsel 以 \r\n 连接, 无尾换行
    // CPython: morsels joined by \r\n, no trailing newline
    std::string out;
    std::vector<value>* items = cookieItems(a, a.self(), false);
    if (items){
        bool first = true;
        for (auto& item : *items){
            auto* inst = instOf(item);
            if (!inst) continue;
            if (!first) out += "\r\n";
            first = false;
            out += header + " " + formatMorsel(a, inst);
        }
    }
    return a.mkstr(out);
}

// Cookie.js_output
// Cookie.js_output
// Cookie.js_output
static value fnCookieJs(cffi& a){
    std::string out;
    std::vector<value>* items = cookieItems(a, a.self(), false);
    if (items){
        for (auto& item : *items){
            auto* inst = instOf(item);
            if (!inst) continue;
            out += formatJs(inst);
        }
    }
    return a.mkstr(out);
}

// Cookie.__str__
// Cookie.__str__
// Cookie.__str__
static value fnCookieStr(cffi& a){
    return fnCookieOutput(a);
}

}

// cffi http.cookies lib
// cffi http.cookies lib
// cffi http.cookies lib
ffilib cffi_http_cookies_lib(){
    ffi_class morselClass;
    morselClass.name = "Morsel";
    morselClass.constructor = (void*)fnMorselCtor;
    morselClass.methods["set"] = (void*)fnMorselSet;
    morselClass.methods["__getitem__"] = (void*)fnMorselGetitem;
    morselClass.methods["__setitem__"] = (void*)fnMorselSetitem;
    morselClass.methods["__contains__"] = (void*)fnMorselContains;
    morselClass.methods["OutputString"] = (void*)fnMorselOutput;
    morselClass.methods["output"] = (void*)fnMorselOutput;
    morselClass.methods["js_output"] = (void*)fnMorselJs;
    morselClass.methods["__str__"] = (void*)fnMorselStr;

    ffi_class cookieClass;
    cookieClass.name = "SimpleCookie";
    cookieClass.constructor = (void*)fnCookieCtor;
    cookieClass.methods["__setitem__"] = (void*)fnCookieSetitem;
    cookieClass.methods["__getitem__"] = (void*)fnCookieGetitem;
    cookieClass.methods["__contains__"] = (void*)fnCookieContains;
    cookieClass.methods["__len__"] = (void*)fnCookieLen;
    cookieClass.methods["keys"] = (void*)fnCookieKeys;
    cookieClass.methods["values"] = (void*)fnCookieValues;
    cookieClass.methods["items"] = (void*)fnCookieItems;
    cookieClass.methods["get"] = (void*)fnCookieGet;
    cookieClass.methods["load"] = (void*)fnCookieLoad;
    cookieClass.methods["output"] = (void*)fnCookieOutput;
    cookieClass.methods["js_output"] = (void*)fnCookieJs;
    cookieClass.methods["__str__"] = (void*)fnCookieStr;

    ffi_class baseClass = cookieClass;
    baseClass.name = "BaseCookie";

    ffi_class errorClass;
    errorClass.name = "CookieError";

    return cffi_lib(
        "http.cookies",
        {},
        {},
        {
            {"Morsel", morselClass},
            {"BaseCookie", baseClass},
            {"SimpleCookie", cookieClass},
            {"CookieError", errorClass}
        });
}

}
