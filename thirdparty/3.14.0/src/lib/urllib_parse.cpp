#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace minipy {

namespace {

// URL 拆分结果状态
// URL split result state
// URL ᠰᠠᠯᠭᠠᠬᠤ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct UrlPartsState{
    std::string scheme;
    std::string netloc;
    std::string path;
    std::string query;
    std::string fragment;
    std::string username;
    std::string password;
    std::string hostname;
    std::string port;
    bool hasQuery = false;
    bool hasFragment = false;
};

// 始终安全的字符
// Characters that are always safe
// ᠰᠡᠬᠡᠯᠲᠡ ᠦᠭᠡᠢ ᠦᠰᠦᠭ
bool alwaysSafe(unsigned char c){
    return std::isalnum(c) || c == '_' || c == '.' || c == '-' || c == '~';
}

// 百分号编码
// Percent-encode a byte string
// ᠬᠤᠪᠢ ᠲᠡᠮᠳᠡᠭᠲᠦ ᠴᠢᠨᠠᠭᠠᠯ
std::string percentEncode(const std::string& text, const std::string& safe, bool plus){
    std::string out;
    for (unsigned char c : text){
        if (plus && c == ' '){
            out.push_back('+');
            continue;
        }
        if (alwaysSafe(c) || safe.find(static_cast<char>(c)) != std::string::npos){
            out.push_back(static_cast<char>(c));
            continue;
        }
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%%%02X", c);
        out += hex;
    }
    return out;
}

// 十六进制字符值
// Value of one hexadecimal digit
// ᠠᠷᠪᠠᠨ ᠵᠤᠷᠭᠠᠨ ᠲᠣᠭ᠎ᠠ ᠶᠢᠨ ᠦᠰᠦᠭ
int hexDigit(unsigned char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 百分号解码
// Percent-decode a byte string
// ᠬᠤᠪᠢ ᠲᠡᠮᠳᠡᠭ ᠢ ᠲᠠᠢᠯᠠᠬᠤ
std::string percentDecode(const std::string& text, bool plus){
    std::string out;
    for (size_t i = 0; i < text.size();++i){
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (plus && c == '+'){
            out.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < text.size()){
            int hi = hexDigit(static_cast<unsigned char>(text[i + 1]));
            int lo = hexDigit(static_cast<unsigned char>(text[i + 2]));
            if (hi >= 0 && lo >= 0){
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

// 判断合法 scheme
// Whether a scheme is syntactically valid
// ᠬᠡᠯᠪᠡᠷᠢ ᠨᠡᠷᠡ ᠬᠦᠴᠢᠨᠲᠡᠢ ᠡᠰᠡᠬᠦ
bool validScheme(const std::string& scheme){
    if (scheme.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(scheme[0]))) return false;
    for (char c : scheme){
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            c != '+' && c != '-' && c != '.') return false;
    }
    return true;
}

// 解析主机与端口信息
// Parse host and port details
// ᠬᠣᠰᠢᠭᠤᠨ ᠪᠤᠯᠤᠨ ᠡᠭᠦᠳᠡ ᠢ ᠰᠠᠯᠭᠠᠬᠤ
void fillHostInfo(UrlPartsState& state){
    std::string netloc = state.netloc;
    std::string userinfo;
    size_t at = netloc.rfind('@');
    if (at != std::string::npos){
        userinfo = netloc.substr(0, at);
        netloc = netloc.substr(at + 1);
        size_t colon = userinfo.find(':');
        if (colon == std::string::npos){
            state.username = userinfo;
        } else {
            state.username = userinfo.substr(0, colon);
            state.password = userinfo.substr(colon + 1);
        }
    }
    if (!netloc.empty() && netloc[0] == '['){
        size_t close = netloc.find(']');
        if (close != std::string::npos){
            state.hostname = netloc.substr(0, close + 1);
            if (close + 1 < netloc.size() && netloc[close + 1] == ':'){
                state.port = netloc.substr(close + 2);
            } else if (close + 2 < netloc.size() && netloc[close + 1] == ':' &&
                       netloc[close + 2] == ':'){
                state.port = netloc.substr(close + 2);
            }
        } else {
            state.hostname = netloc;
        }
        return;
    }
    size_t colon = netloc.rfind(':');
    if (colon != std::string::npos){
        state.hostname = netloc.substr(0, colon);
        state.port = netloc.substr(colon + 1);
    } else {
        state.hostname = netloc;
    }
}

// 拆分 URL
// Split a URL into components
// URL ᠢ ᠬᠡᠰᠡᠭ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
UrlPartsState splitUrl(const std::string& url, bool allowFragments){
    UrlPartsState state;
    std::string rest = url;
    size_t hash = rest.find('#');
    if (hash != std::string::npos){
        if (allowFragments){
            state.fragment = rest.substr(hash + 1);
            state.hasFragment = true;
            rest = rest.substr(0, hash);
        }
    }
    size_t question = rest.find('?');
    if (question != std::string::npos){
        state.query = rest.substr(question + 1);
        state.hasQuery = true;
        rest = rest.substr(0, question);
    }
    size_t colon = rest.find(':');
    if (colon != std::string::npos){
        std::string candidate = rest.substr(0, colon);
        size_t slash = rest.find('/');
        if (validScheme(candidate) &&
            (slash == std::string::npos || colon < slash)){
            state.scheme = candidate;
            rest = rest.substr(colon + 1);
        }
    }
    if (rest.size() >= 2 && rest[0] == '/' && rest[1] == '/'){
        size_t end = rest.find('/', 2);
        if (end == std::string::npos){
            state.netloc = rest.substr(2);
            rest.clear();
        } else {
            state.netloc = rest.substr(2, end - 2);
            rest = rest.substr(end);
        }
    }
    state.path = rest;
    fillHostInfo(state);
    return state;
}

// 规范化点段
// Remove dot segments from a path
// ᠳ᠋ᠣᠲ ᠬᠡᠰᠡᠭ ᠢ ᠠᠷᠢᠯᠭᠠᠬᠤ
std::string removeDotSegments(const std::string& path){
    if (path.empty()) return path;
    std::vector<std::string> parts;
    std::string current;
    size_t i = 0;
    bool absolute = path[0] == '/';
    if (absolute) i = 1;
    while (i <= path.size()){
        size_t next = path.find('/', i);
        current = path.substr(i, next == std::string::npos ? std::string::npos : next - i);
        if (current == ".."){
            if (!parts.empty()) parts.pop_back();
        } else if (current != "." && !current.empty()){
            parts.push_back(current);
        }
        if (next == std::string::npos) break;
        i = next + 1;
    }
    std::string out;
    if (absolute) out = "/";
    for (size_t k = 0; k < parts.size(); ++k){
        if (k) out += "/";
        out += parts[k];
    }
    if (!absolute && parts.empty()) return path;
    return out;
}

// 查找 FFI 类
// Find an FFI class in the current library
// ᠣᠳᠣᠮᠤ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠡᠴᠡ FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠬᠠᠯᠠᠬᠤ
const ffi_class* findClass(cffi& a, const std::string& name){
    if (!a.library) return nullptr;
    auto it = a.library->classes.find(name);
    if (it == a.library->classes.end()) return nullptr;
    return &it->second;
}

// 构建 URL 结果对象
// Build a URL result object
// URL ᠶᠢᠨ ᠦᠷ᠎ᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeUrlObject(cffi& a, const std::string& className,
                    const UrlPartsState& state, bool includeParams){
    value obj = a.none();
    const ffi_class* klass = findClass(a, className);
    if (klass && a.library){
        obj = a.vm->makeNativeObject(a.library, klass, new UrlPartsState(state));
    }
    if (obj.type != "native_object") obj = a.none();
    a.setattr(obj, "scheme", a.mkstr(state.scheme));
    a.setattr(obj, "netloc", a.mkstr(state.netloc));
    std::string path = state.path;
    std::string params;
    if (includeParams){
        size_t semi = path.rfind(';');
        if (semi != std::string::npos){
            params = path.substr(semi + 1);
            path = path.substr(0, semi);
            a.setattr(obj, "params", a.mkstr(params));
        } else {
            a.setattr(obj, "params", a.mkstr(""));
        }
    }
    a.setattr(obj, "path", a.mkstr(path));
    a.setattr(obj, "query", a.mkstr(state.query));
    a.setattr(obj, "fragment", a.mkstr(state.fragment));
    a.setattr(obj, "username", state.username.empty() ? a.none() : a.mkstr(state.username));
    a.setattr(obj, "password", state.password.empty() ? a.none() : a.mkstr(state.password));
    a.setattr(obj, "hostname", state.hostname.empty() ? a.none() : a.mkstr(state.hostname));
    a.setattr(obj, "port", state.port.empty() ? a.none() : a.mkint(std::atoll(state.port.c_str())));
    return obj;
}

// 读取字符串列表
// Read a list of strings
// ᠦᠰᠦᠭ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠢ ᠦᠨᠰᠢᠬᠦ
bool readStrings(cffi& a, const value& source, std::vector<std::string>& out){
    std::vector<value> items;
    a.vm->materializeIterable(source, items);
    if (a.vm->hasError()) return false;
    for (const auto& item : items) out.push_back(a.vm->toString(item));
    return true;
}
}

// fn Quote
// fn quote
// fn Quote
static value fnQuote(cffi& a){
    std::string text = a.arg_str(0);
    std::string safe = (a.argc() > 1) ? a.arg_str(1) : "/";
    return a.mkstr(percentEncode(text, safe, false));
}

// fn Quote Plus
// fn quote_plus
// fn Quote Plus
static value fnQuotePlus(cffi& a){
    std::string text = a.arg_str(0);
    std::string safe = (a.argc() > 1) ? a.arg_str(1) : "";
    return a.mkstr(percentEncode(text, safe, true));
}

// fn Unquote
// fn unquote
// fn Unquote
static value fnUnquote(cffi& a){
    std::string text = a.arg_str(0);
    return a.mkstr(percentDecode(text, false));
}

// fn Unquote Plus
// fn unquote_plus
// fn Unquote Plus
static value fnUnquotePlus(cffi& a){
    std::string text = a.arg_str(0);
    return a.mkstr(percentDecode(text, true));
}

// fn Url Split
// fn urlsplit
// fn Url Split
static value fnUrlSplit(cffi& a){
    std::string url = a.arg_str(0);
    bool allowFragments = (a.argc() > 1) ? a.arg_bool(1) : true;
    UrlPartsState state = splitUrl(url, allowFragments);
    return makeUrlObject(a, "SplitResult", state, false);
}

// fn Url Parse
// fn urlparse
// fn Url Parse
static value fnUrlParse(cffi& a){
    std::string url = a.arg_str(0);
    bool allowFragments = (a.argc() > 2) ? a.arg_bool(2) : true;
    UrlPartsState state = splitUrl(url, allowFragments);
    return makeUrlObject(a, "ParseResult", state, true);
}

// fn Geturl
// fn geturl
// fn Geturl
static value fnGeturl(cffi& a){
    value self = a.self();
    std::string scheme = a.vm->toString(a.attr("scheme"));
    std::string netloc = a.vm->toString(a.attr("netloc"));
    std::string path = a.vm->toString(a.attr("path"));
    std::string query = a.vm->toString(a.attr("query"));
    std::string fragment = a.vm->toString(a.attr("fragment"));
    value params = a.attr("params");
    if (params.type == "str" && a.vm->toString(params).size() > 0){
        path += ";" + a.vm->toString(params);
    }
    std::string out;
    if (!netloc.empty() || (path.size() >= 2 && path.substr(0, 2) == "//")){
        if (path.size() >= 1 && path[0] != '/') path = "/" + path;
        out = "//" + netloc + path;
    } else {
        out = path;
    }
    if (!scheme.empty()) out = scheme + ":" + out;
    if (!query.empty()) out += "?" + query;
    if (!fragment.empty()) out += "#" + fragment;
    (void)self;
    return a.mkstr(out);
}

// fn Url Unparse
// fn urlunparse
// fn Url Unparse
static value fnUrlUnparse(cffi& a){
    std::vector<std::string> parts;
    if (!readStrings(a, a.arg(0), parts)) return a.none();
    while (parts.size() < 6) parts.push_back("");
    std::string scheme = parts[0], netloc = parts[1], path = parts[2];
    std::string params = parts[3], query = parts[4], fragment = parts[5];
    if (!params.empty()) path += ";" + params;
    std::string out;
    if (!netloc.empty() || (path.size() >= 2 && path.substr(0, 2) == "//")){
        if (!path.empty() && path[0] != '/') path = "/" + path;
        out = "//" + netloc + path;
    } else {
        out = path;
    }
    if (!scheme.empty()) out = scheme + ":" + out;
    if (!query.empty()) out += "?" + query;
    if (!fragment.empty()) out += "#" + fragment;
    return a.mkstr(out);
}

// fn Url Join
// fn urljoin
// fn Url Join
static value fnUrlJoin(cffi& a){
    std::string base = a.arg_str(0);
    std::string url = a.arg_str(1);
    if (url.empty()) return a.mkstr(base);
    UrlPartsState us = splitUrl(url, true);
    if (!us.scheme.empty()) return a.mkstr(url);
    UrlPartsState bs = splitUrl(base, true);
    UrlPartsState out = bs;
    out.fragment = us.hasFragment ? us.fragment : bs.fragment;
    out.hasFragment = us.hasFragment || bs.hasFragment;
    out.query = us.hasQuery ? us.query : bs.query;
    out.hasQuery = us.hasQuery || bs.hasQuery;
    if (!us.netloc.empty()){
        out.scheme = bs.scheme;
        out.netloc = us.netloc;
        out.path = removeDotSegments(us.path);
        fillHostInfo(out);
    } else if (us.path.empty()){
        out.netloc = bs.netloc;
        out.path = bs.path;
    } else if (us.path[0] == '/'){
        out.netloc = bs.netloc;
        out.path = removeDotSegments(us.path);
    } else {
        std::string merged;
        if (!bs.netloc.empty() && bs.path.empty()){
            merged = "/" + us.path;
        } else {
            size_t slash = bs.path.rfind('/');
            merged = (slash == std::string::npos) ? us.path
                                                  : bs.path.substr(0, slash + 1) + us.path;
        }
        out.netloc = bs.netloc;
        out.path = removeDotSegments(merged);
    }
    fillHostInfo(out);
    std::string merged;
    if (!out.netloc.empty() || (out.path.size() >= 2 && out.path.substr(0, 2) == "//")){
        if (!out.path.empty() && out.path[0] != '/') out.path = "/" + out.path;
        merged = "//" + out.netloc + out.path;
    } else {
        merged = out.path;
    }
    if (!out.scheme.empty()) merged = out.scheme + ":" + merged;
    if (!out.query.empty()) merged += "?" + out.query;
    if (!out.fragment.empty()) merged += "#" + out.fragment;
    return a.mkstr(merged);
}

// fn Url Defrag
// fn urldefrag
// fn Url Defrag
static value fnUrlDefrag(cffi& a){
    UrlPartsState state = splitUrl(a.arg_str(0), true);
    std::string base;
    if (!state.scheme.empty()) base += state.scheme + ":";
    if (!state.netloc.empty() || (state.path.size() >= 2 && state.path.substr(0, 2) == "//")){
        if (!state.path.empty() && state.path[0] != '/' && !state.netloc.empty()){
            state.path = "/" + state.path;
        }
        base += "//" + state.netloc;
    }
    base += state.path;
    if (state.hasQuery) base += "?" + state.query;
    std::vector<value> items = {a.mkstr(base), a.mkstr(state.fragment)};
    return a.vm->makeTuple(items);
}

// fn Parse Qsl
// fn parse_qsl
// fn Parse Qsl
static value fnParseQsl(cffi& a){
    std::string query = a.arg_str(0);
    bool keepBlank = (a.argc() > 1) ? a.arg_bool(1) : false;
    std::string separator = (a.argc() > 5) ? a.arg_str(5) : "&";
    std::vector<value> result;
    size_t begin = 0;
    while (begin <= query.size()){
        size_t end = query.find(separator, begin);
        if (end == std::string::npos) end = query.size();
        std::string field = query.substr(begin, end - begin);
        size_t eq = field.find('=');
        std::string name, valueText;
        bool hasValue = false;
        if (eq == std::string::npos){
            name = percentDecode(field, true);
        } else {
            name = percentDecode(field.substr(0, eq), true);
            valueText = percentDecode(field.substr(eq + 1), true);
            hasValue = true;
        }
        if (!name.empty() || hasValue || keepBlank){
            std::vector<value> pair = {a.mkstr(name), a.mkstr(valueText)};
            result.push_back(a.vm->makeTuple(pair));
        }
        if (end >= query.size()) break;
        begin = end + separator.size();
    }
    return a.mklist(result);
}

// fn Parse Qs
// fn parse_qs
// fn Parse Qs
static value fnParseQs(cffi& a){
    std::string query = a.arg_str(0);
    bool keepBlank = (a.argc() > 1) ? a.arg_bool(1) : false;
    std::string separator = (a.argc() > 5) ? a.arg_str(5) : "&";
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    size_t begin = 0;
    while (begin <= query.size()){
        size_t end = query.find(separator, begin);
        if (end == std::string::npos) end = query.size();
        std::string field = query.substr(begin, end - begin);
        size_t eq = field.find('=');
        std::string name, valueText;
        bool hasValue = false;
        if (eq == std::string::npos){
            name = percentDecode(field, true);
        } else {
            name = percentDecode(field.substr(0, eq), true);
            valueText = percentDecode(field.substr(eq + 1), true);
            hasValue = true;
        }
        if (!name.empty() || hasValue || keepBlank){
            bool found = false;
            for (auto& group : groups){
                if (group.first == name){
                    group.second.push_back(valueText);
                    found = true;
                    break;
                }
            }
            if (!found) groups.push_back({name, {valueText}});
        }
        if (end >= query.size()) break;
        begin = end + separator.size();
    }
    std::vector<std::pair<value, value>> items;
    for (auto& group : groups){
        std::vector<value> values;
        for (auto& item : group.second) values.push_back(a.mkstr(item));
        items.push_back({a.mkstr(group.first), a.mklist(values)});
    }
    return a.mkdict(items);
}

// 编码一组键值
// Encode one key/value pair
// ᠨᠢᠭᠡ ᠬᠣᠰᠤᠷᠤᠭ ᠢ ᠴᠢᠨᠠᠭᠠᠬᠤ
static std::string encodePair(const std::string& key, const std::string& value){
    return percentEncode(key, "", true) + "=" + percentEncode(value, "", true);
}

// fn Url Encode
// fn urlencode
// fn Url Encode
static value fnUrlEncode(cffi& a){
    bool doseq = (a.argc() > 1) ? a.arg_bool(1) : false;
    std::string result;
    auto append = [&](const std::string& chunk){
        if (!result.empty()) result += "&";
        result += chunk;
    };
    value source = a.arg(0);
    if (source.type == "dict" && source.ptr){
        auto* items = static_cast<dict_t*>(source.ptr);
        for (auto& item : *items){
            std::string key = a.vm->toString(item.first);
            value mapped = item.second;
            if (doseq && (mapped.type == "list" || mapped.type == "tuple")){
                std::vector<value> values;
                a.vm->materializeIterable(mapped, values);
                for (auto& v : values) append(encodePair(key, a.vm->toString(v)));
            } else {
                append(encodePair(key, a.vm->toString(mapped)));
            }
        }
    } else {
        std::vector<value> pairs;
        a.vm->materializeIterable(source, pairs);
        if (a.vm->hasError()) return a.none();
        for (auto& pair : pairs){
            std::vector<value> fields;
            a.vm->materializeIterable(pair, fields);
            if (fields.size() < 2) continue;
            std::string key = a.vm->toString(fields[0]);
            if (doseq && (fields[1].type == "list" || fields[1].type == "tuple")){
                std::vector<value> values;
                a.vm->materializeIterable(fields[1], values);
                for (auto& v : values) append(encodePair(key, a.vm->toString(v)));
            } else {
                append(encodePair(key, a.vm->toString(fields[1])));
            }
        }
    }
    return a.mkstr(result);
}

// cffi urllib.parse lib
// cffi urllib.parse lib
// cffi urllib.parse lib
ffilib cffi_urllib_parse_lib(){
    ffi_class splitClass;
    splitClass.name = "SplitResult";
    splitClass.methods = {
        {"geturl", (void*)fnGeturl},
        {"__str__", (void*)fnGeturl}
    };
    ffi_class parseClass;
    parseClass.name = "ParseResult";
    parseClass.methods = {
        {"geturl", (void*)fnGeturl},
        {"__str__", (void*)fnGeturl}
    };
    return cffi_lib(
        "urllib.parse",
        {
            {"quote", (void*)fnQuote},
            {"quote_plus", (void*)fnQuotePlus},
            {"unquote", (void*)fnUnquote},
            {"unquote_plus", (void*)fnUnquotePlus},
            {"urlsplit", (void*)fnUrlSplit},
            {"urlparse", (void*)fnUrlParse},
            {"urlunparse", (void*)fnUrlUnparse},
            {"urljoin", (void*)fnUrlJoin},
            {"urldefrag", (void*)fnUrlDefrag},
            {"parse_qsl", (void*)fnParseQsl},
            {"parse_qs", (void*)fnParseQs},
            {"urlencode", (void*)fnUrlEncode}
        },
        {},
        {
            {"SplitResult", splitClass},
            {"ParseResult", parseClass}
        });
}

}
