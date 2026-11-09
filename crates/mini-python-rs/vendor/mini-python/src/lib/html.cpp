#include "../cffi.hpp"

#include <cctype>
#include <string>
#include <unordered_map>

namespace minipy {

namespace {

// 常用命名实体表
// Common named character entities
// ᠨᠡᠷ᠎ᠡ ᠲᠡᠮᠳᠡᠭ ᠦᠨ ᠬᠡᠰᠢᠭ
const std::unordered_map<std::string, std::string>& entityTable(){
    static const std::unordered_map<std::string, std::string> table = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
        {"nbsp", "\xC2\xA0"}, {"iexcl", "\xC2\xA1"}, {"cent", "\xC2\xA2"},
        {"pound", "\xC2\xA3"}, {"curren", "\xC2\xA4"}, {"yen", "\xC2\xA5"},
        {"sect", "\xC2\xA7"}, {"uml", "\xC2\xA8"}, {"copy", "\xC2\xA9"},
        {"ordf", "\xC2\xAA"}, {"laquo", "\xC2\xAB"}, {"not", "\xC2\xAC"},
        {"shy", "\xC2\xAD"}, {"reg", "\xC2\xAE"}, {"macr", "\xC2\xAF"},
        {"deg", "\xC2\xB0"}, {"plusmn", "\xC2\xB1"}, {"sup2", "\xC2\xB2"},
        {"sup3", "\xC2\xB3"}, {"acute", "\xC2\xB4"}, {"micro", "\xC2\xB5"},
        {"para", "\xC2\xB6"}, {"middot", "\xC2\xB7"}, {"cedil", "\xC2\xB8"},
        {"sup1", "\xC2\xB9"}, {"ordm", "\xC2\xBA"}, {"raquo", "\xC2\xBB"},
        {"frac14", "\xC2\xBC"}, {"frac12", "\xC2\xBD"}, {"frac34", "\xC2\xBE"},
        {"iquest", "\xC2\xBF"}, {"Agrave", "\xC3\x80"}, {"Aacute", "\xC3\x81"},
        {"Acirc", "\xC3\x82"}, {"Atilde", "\xC3\x83"}, {"Auml", "\xC3\x84"},
        {"Aring", "\xC3\x85"}, {"AElig", "\xC3\x86"}, {"Ccedil", "\xC3\x87"},
        {"Egrave", "\xC3\x88"}, {"Eacute", "\xC3\x89"}, {"Ecirc", "\xC3\x8A"},
        {"Euml", "\xC3\x8B"}, {"Igrave", "\xC3\x8C"}, {"Iacute", "\xC3\x8D"},
        {"Icirc", "\xC3\x8E"}, {"Iuml", "\xC3\x8F"}, {"ETH", "\xC3\x90"},
        {"Ntilde", "\xC3\x91"}, {"Ograve", "\xC3\x92"}, {"Oacute", "\xC3\x93"},
        {"Ocirc", "\xC3\x94"}, {"Otilde", "\xC3\x95"}, {"Ouml", "\xC3\x96"},
        {"times", "\xC3\x97"}, {"Oslash", "\xC3\x98"}, {"Ugrave", "\xC3\x99"},
        {"Uacute", "\xC3\x9A"}, {"Ucirc", "\xC3\x9B"}, {"Uuml", "\xC3\x9C"},
        {"Yacute", "\xC3\x9D"}, {"THORN", "\xC3\x9E"}, {"szlig", "\xC3\x9F"},
        {"agrave", "\xC3\xA0"}, {"aacute", "\xC3\xA1"}, {"acirc", "\xC3\xA2"},
        {"atilde", "\xC3\xA3"}, {"auml", "\xC3\xA4"}, {"aring", "\xC3\xA5"},
        {"aelig", "\xC3\xA6"}, {"ccedil", "\xC3\xA7"}, {"egrave", "\xC3\xA8"},
        {"eacute", "\xC3\xA9"}, {"ecirc", "\xC3\xAA"}, {"euml", "\xC3\xAB"},
        {"igrave", "\xC3\xAC"}, {"iacute", "\xC3\xAD"}, {"icirc", "\xC3\xAE"},
        {"iuml", "\xC3\xAF"}, {"eth", "\xC3\xB0"}, {"ntilde", "\xC3\xB1"},
        {"ograve", "\xC3\xB2"}, {"oacute", "\xC3\xB3"}, {"ocirc", "\xC3\xB4"},
        {"otilde", "\xC3\xB5"}, {"ouml", "\xC3\xB6"}, {"divide", "\xC3\xB7"},
        {"oslash", "\xC3\xB8"}, {"ugrave", "\xC3\xB9"}, {"uacute", "\xC3\xBA"},
        {"ucirc", "\xC3\xBB"}, {"uuml", "\xC3\xBC"}, {"yacute", "\xC3\xBD"},
        {"thorn", "\xC3\xBE"}, {"yuml", "\xC3\xBF"},
        {"Alpha", "\xCE\x91"}, {"Beta", "\xCE\x92"}, {"Gamma", "\xCE\x93"},
        {"Delta", "\xCE\x94"}, {"Epsilon", "\xCE\x95"}, {"Zeta", "\xCE\x96"},
        {"Eta", "\xCE\x97"}, {"Theta", "\xCE\x98"}, {"Iota", "\xCE\x99"},
        {"Kappa", "\xCE\x9A"}, {"Lambda", "\xCE\x9B"}, {"Mu", "\xCE\x9C"},
        {"Nu", "\xCE\x9D"}, {"Xi", "\xCE\x9E"}, {"Omicron", "\xCE\x9F"},
        {"Pi", "\xCE\xA0"}, {"Rho", "\xCE\xA1"}, {"Sigma", "\xCE\xA3"},
        {"Tau", "\xCE\xA4"}, {"Upsilon", "\xCE\xA5"}, {"Phi", "\xCE\xA6"},
        {"Chi", "\xCE\xA7"}, {"Psi", "\xCE\xA8"}, {"Omega", "\xCE\xA9"},
        {"alpha", "\xCE\xB1"}, {"beta", "\xCE\xB2"}, {"gamma", "\xCE\xB3"},
        {"delta", "\xCE\xB4"}, {"epsilon", "\xCE\xB5"}, {"zeta", "\xCE\xB6"},
        {"eta", "\xCE\xB7"}, {"theta", "\xCE\xB8"}, {"iota", "\xCE\xB9"},
        {"kappa", "\xCE\xBA"}, {"lambda", "\xCE\xBB"}, {"mu", "\xCE\xBC"},
        {"nu", "\xCE\xBD"}, {"xi", "\xCE\xBE"}, {"omicron", "\xCE\xBF"},
        {"pi", "\xCF\x80"}, {"rho", "\xCF\x81"}, {"sigmaf", "\xCF\x82"},
        {"sigma", "\xCF\x83"}, {"tau", "\xCF\x84"}, {"upsilon", "\xCF\x85"},
        {"phi", "\xCF\x86"}, {"chi", "\xCF\x87"}, {"psi", "\xCF\x88"},
        {"omega", "\xCF\x89"},
        {"ensp", "\xE2\x80\x82"}, {"emsp", "\xE2\x80\x83"},
        {"thinsp", "\xE2\x80\x89"}, {"ndash", "\xE2\x80\x93"},
        {"mdash", "\xE2\x80\x94"}, {"lsquo", "\xE2\x80\x98"},
        {"rsquo", "\xE2\x80\x99"}, {"sbquo", "\xE2\x80\x9A"},
        {"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"},
        {"bdquo", "\xE2\x80\x9E"}, {"dagger", "\xE2\x80\xA0"},
        {"Dagger", "\xE2\x80\xA1"}, {"bull", "\xE2\x80\xA2"},
        {"hellip", "\xE2\x80\xA6"}, {"permil", "\xE2\x80\xB0"},
        {"prime", "\xE2\x80\xB2"}, {"Prime", "\xE2\x80\xB3"},
        {"lsaquo", "\xE2\x80\xB9"}, {"rsaquo", "\xE2\x80\xBA"},
        {"oline", "\xE2\x80\xBE"}, {"frasl", "\xE2\x81\x84"},
        {"euro", "\xE2\x82\xAC"}, {"trade", "\xE2\x84\xA2"},
        {"alefsym", "\xE2\x84\xB5"}, {"larr", "\xE2\x86\x90"},
        {"uarr", "\xE2\x86\x91"}, {"rarr", "\xE2\x86\x92"},
        {"darr", "\xE2\x86\x93"}, {"forall", "\xE2\x88\x80"},
        {"part", "\xE2\x88\x82"}, {"exist", "\xE2\x88\x83"},
        {"empty", "\xE2\x88\x85"}, {"nabla", "\xE2\x88\x87"},
        {"isin", "\xE2\x88\x88"}, {"notin", "\xE2\x88\x89"},
        {"prod", "\xE2\x88\x8F"}, {"sum", "\xE2\x88\x91"},
        {"minus", "\xE2\x88\x92"}, {"lowast", "\xE2\x88\x97"},
        {"radic", "\xE2\x88\x9A"}, {"infin", "\xE2\x88\x9E"},
        {"ang", "\xE2\x88\xA0"}, {"and", "\xE2\x88\xA7"},
        {"or", "\xE2\x88\xA8"}, {"cap", "\xE2\x88\xA9"},
        {"cup", "\xE2\x88\xAA"}, {"int", "\xE2\x88\xAB"},
        {"there4", "\xE2\x88\xB4"}, {"sim", "\xE2\x88\xBC"},
        {"cong", "\xE2\x89\x85"}, {"asymp", "\xE2\x89\x88"},
        {"ne", "\xE2\x89\xA0"}, {"equiv", "\xE2\x89\xA1"},
        {"le", "\xE2\x89\xA4"}, {"ge", "\xE2\x89\xA5"},
        {"sub", "\xE2\x8A\x82"}, {"sup", "\xE2\x8A\x83"},
        {"nsub", "\xE2\x8A\x84"}, {"sube", "\xE2\x8A\x86"},
        {"supe", "\xE2\x8A\x87"}, {"oplus", "\xE2\x8A\x95"},
        {"otimes", "\xE2\x8A\x97"}, {"perp", "\xE2\x8A\xA5"},
        {"sdot", "\xE2\x8B\x85"}, {"lceil", "\xE2\x8C\x88"},
        {"rceil", "\xE2\x8C\x89"}, {"lfloor", "\xE2\x8C\x8A"},
        {"rfloor", "\xE2\x8C\x8B"}, {"lang", "\xE2\x9F\xA8"},
        {"rang", "\xE2\x9F\xA9"}, {"loz", "\xE2\x97\x8A"},
        {"spades", "\xE2\x99\xA0"}, {"clubs", "\xE2\x99\xA3"},
        {"hearts", "\xE2\x99\xA5"}, {"diams", "\xE2\x99\xA6"}
    };
    return table;
}

// 数字实体转 UTF-8
// Convert a numeric entity to UTF-8
// ᠲᠣᠭ᠎ᠠ ᠲᠡᠮᠳᠡᠭ ᠢ UTF-8 ᠪᠣᠯᠭᠠᠨ᠎ᠠ
std::string codePointToUtf8(long long code){
    std::string out;
    if (code < 0) return out;
    if (code < 0x80){
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800){
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000){
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x110000){
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
    return out;
}

// 十进制数字串转整数
// Parse a decimal digit string
// ᠠᠷᠪᠠᠨ ᠲᠣᠭ᠎ᠠ ᠢ ᠦᠨᠰᠢᠬᠦ
bool parseDecimal(const std::string& text, long long& result){
    if (text.empty()) return false;
    result = 0;
    for (char c : text){
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        result = result * 10 + (c - '0');
        if (result > 0x10FFFF) return false;
    }
    return true;
}

// 十六进制数字串转整数
// Parse a hexadecimal digit string
// ᠠᠷᠪᠠᠨ ᠵᠤᠷᠭᠠᠨ ᠲᠣᠭ᠎ᠠ ᠢ ᠦᠨᠰᠢᠬᠦ
bool parseHex(const std::string& text, long long& result){
    if (text.empty()) return false;
    result = 0;
    for (char c : text){
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        result = result * 16 + digit;
        if (result > 0x10FFFF) return false;
    }
    return true;
}
}

// fn Escape
// fn escape
// fn Escape
static value fnEscape(cffi& a){
    std::string text = a.arg_str(0);
    bool quote = (a.argc() > 1) ? a.arg_bool(1) : true;
    std::string result;
    result.reserve(text.size());
    for (char c : text){
        if (c == '&') result += "&amp;";
        else if (c == '<') result += "&lt;";
        else if (c == '>') result += "&gt;";
        else if (quote && c == '"') result += "&quot;";
        else if (quote && c == '\'') result += "&#x27;";
        else result.push_back(c);
    }
    return a.mkstr(result);
}

// fn Unescape
// fn unescape
// fn Unescape
static value fnUnescape(cffi& a){
    std::string text = a.arg_str(0);
    std::string result;
    size_t i = 0;
    while (i < text.size()){
        if (text[i] != '&'){
            result.push_back(text[i]);
            i++;
            continue;
        }
        size_t semi = text.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 40){
            result.push_back(text[i]);
            i++;
            continue;
        }
        std::string body = text.substr(i + 1, semi - i - 1);
        bool replaced = false;
        if (!body.empty() && body[0] == '#'){
            long long code = 0;
            if (body.size() > 1 && (body[1] == 'x' || body[1] == 'X')){
                replaced = parseHex(body.substr(2), code);
            } else {
                replaced = parseDecimal(body.substr(1), code);
            }
            if (replaced){
                result += codePointToUtf8(code);
                i = semi + 1;
                continue;
            }
        } else {
            auto it = entityTable().find(body);
            if (it != entityTable().end()){
                result += it->second;
                i = semi + 1;
                continue;
            }
        }
        result.push_back(text[i]);
        i++;
    }
    return a.mkstr(result);
}

// cffi html lib
// cffi html lib
// cffi html lib
ffilib cffi_html_lib(){
    return cffi_lib(
        "html",
        {
            {"escape", (void*)fnEscape},
            {"unescape", (void*)fnUnescape}
        });
}

}
