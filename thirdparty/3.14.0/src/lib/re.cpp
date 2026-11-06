#include "../cffi.hpp"

#include <regex>
#include <string>
#include <vector>

namespace minipy {





using namespace minipy;

namespace {

// 构建 Regex
// build regex
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ Regex
bool buildRegex(cffi& a, const std::string& pattern, std::regex& expression){
    try {
        expression = std::regex(pattern, std::regex::ECMAScript);
        return true;
    } catch (const std::regex_error& error){
        a.vm->raiseError("re.error", std::string(error.what()));
        return false;
    }
}

// match 转 Data
// match to data
// match ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Data
value matchToData(cffi& a, const std::match_results<std::string::const_iterator>& match){
    std::vector<value> groups;
    for (size_t i = 0; i < match.size(); ++i){
        if (match[i].matched) groups.push_back(a.mkstr(match[i].str()));
        else groups.push_back(a.none());
    }
    return a.mklist(groups);
}

// 转换 Replacement
// convert replacement
// ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Replacement
std::string convertReplacement(const std::string& replacement){
    std::string result;
    for (size_t i = 0; i < replacement.size(); ++i){
        if (replacement[i] == '\\' && i + 1 < replacement.size()){
            char next = replacement[i + 1];
            if (next >= '0' && next <= '9'){
                result += '$';
                result += next;
                i++;
                continue;
            }
            if (next == '\\'){
                result += '$';
                result += '$';
                i++;
                continue;
            }
        }
        if (replacement[i] == '$') result += "$";
        result += replacement[i];
    }
    return result;
}
}

// fn Match
// fn match
// fn Match
static value fnMatch(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);
    std::smatch match;
    if (std::regex_search(text, match, expression) && match.position() == 0){
        return matchToData(a, match);
    }
    return a.none();
}

// fn 查找
// fn search
// fn ᠬᠠᠯᠠᠬᠤ
static value fnSearch(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);
    std::smatch match;
    if (std::regex_search(text, match, expression)) return matchToData(a, match);
    return a.none();
}

// fn Fullmatch
// fn fullmatch
// fn Fullmatch
static value fnFullmatch(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);
    std::smatch match;
    if (std::regex_match(text, match, expression)) return matchToData(a, match);
    return a.none();
}

// fn Findall
// fn findall
// fn Findall
static value fnFindall(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);
    std::vector<value> result;
    auto begin = std::sregex_iterator(text.begin(), text.end(), expression);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it){
        const std::smatch& match = *it;
        if (match.size() <= 1){
            result.push_back(a.mkstr(match.str()));
            continue;
        }
        std::vector<value> groups;
        for (size_t i = 1; i < match.size(); ++i){
            groups.push_back(match[i].matched ? a.mkstr(match[i].str()) : a.none());
        }
        if (groups.size() == 1) result.push_back(groups[0]);
        else result.push_back(a.mklist(groups));
    }
    return a.mklist(result);
}

// fn Finditer
// fn finditer
// fn Finditer
static value fnFinditer(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);

    std::vector<value> result;
    auto begin = std::sregex_iterator(text.begin(), text.end(), expression);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) result.push_back(matchToData(a, *it));
    return a.mklist(result);
}

// fn 减
// fn sub
// fn ᠬᠠᠯᠠᠬᠤ
static value fnSub(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string replacement = convertReplacement(a.arg_str(1));
    std::string text = a.arg_str(2);
    try {
        return a.mkstr(std::regex_replace(text, expression, replacement));
    } catch (const std::regex_error& error){
        a.vm->raiseError("re.error", std::string(error.what()));
        return a.none();
    }
}

// fn Subn
// fn subn
// fn Subn
static value fnSubn(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string replacement = convertReplacement(a.arg_str(1));
    std::string text = a.arg_str(2);
    try {
        long long count = 0;
        auto begin = std::sregex_iterator(text.begin(), text.end(), expression);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) count++;
        std::vector<value> result;
        result.push_back(a.mkstr(std::regex_replace(text, expression, replacement)));
        result.push_back(a.mkint(count));
        return a.vm->makeTuple(result);
    } catch (const std::regex_error& error){
        a.vm->raiseError("re.error", std::string(error.what()));
        return a.none();
    }
}

// fn 拆分
// fn split
// fn ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠬᠤ
static value fnSplit(cffi& a){
    std::regex expression;
    if (!buildRegex(a, a.arg_str(0), expression)) return a.none();
    std::string text = a.arg_str(1);
    std::vector<value> result;
    std::string remaining = text;
    std::smatch match;
    while (std::regex_search(remaining, match, expression)){
        result.push_back(a.mkstr(match.prefix().str()));
        for (size_t i = 1; i < match.size(); ++i){
            if (match[i].matched) result.push_back(a.mkstr(match[i].str()));
        }
        remaining = match.suffix().str();
        if (match.length() == 0 && remaining.empty()) break;
    }
    result.push_back(a.mkstr(remaining));
    return a.mklist(result);
}

// fn Escape
// fn escape
// fn Escape
static value fnEscape(cffi& a){
    std::string text = a.arg_str(0);
    std::string result;
    for (char c : text){
        if (std::string(".*+?^${}()|[]\\").find(c) != std::string::npos){
            result += '\\';
        }
        result += c;
    }
    return a.mkstr(result);
}



// fn 编译
// fn compile
// fn ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
static value fnCompile(cffi& a){
    std::string pattern = a.arg_str(0);
    std::regex expression;
    if (!buildRegex(a, pattern, expression)) return a.none();
    return a.mkstr(pattern);
}

// data Ignorecase
// data ignorecase
// data Ignorecase
static value dataIgnorecase(cffi& a){ return a.mkint(2); }

// data Multiline
// data multiline
// data Multiline
static value dataMultiline(cffi& a){ return a.mkint(8); }

// data Dotall
// data dotall
// data Dotall
static value dataDotall(cffi& a){ return a.mkint(16); }

// data Ascii
// data ascii
// data Ascii
static value dataAscii(cffi& a){ return a.mkint(256); }

// data Locale
// data locale
// data Locale
static value dataLocale(cffi& a){ return a.mkint(4); }

// data Verbose
// data verbose
// data Verbose
static value dataVerbose(cffi& a){ return a.mkint(64); }

// data Unicode
// data unicode
// data Unicode
static value dataUnicode(cffi& a){ return a.mkint(32); }

// cffi re lib
// cffi re lib
// cffi re lib
ffilib cffi_re_lib(){
    return cffi_lib(
        "re",
        {
            {"match", (void*)fnMatch},
            {"search", (void*)fnSearch},
            {"fullmatch", (void*)fnFullmatch},
            {"findall", (void*)fnFindall},
            {"finditer", (void*)fnFinditer},
            {"sub", (void*)fnSub},
            {"subn", (void*)fnSubn},
            {"split", (void*)fnSplit},
            {"escape", (void*)fnEscape},
            {"compile", (void*)fnCompile}
        },
        {
            {"A", (void*)dataAscii},
            {"ASCII", (void*)dataAscii},
            {"I", (void*)dataIgnorecase},
            {"IGNORECASE", (void*)dataIgnorecase},
            {"L", (void*)dataLocale},
            {"LOCALE", (void*)dataLocale},
            {"M", (void*)dataMultiline},
            {"MULTILINE", (void*)dataMultiline},
            {"S", (void*)dataDotall},
            {"DOTALL", (void*)dataDotall},
            {"X", (void*)dataVerbose},
            {"VERBOSE", (void*)dataVerbose},
            {"U", (void*)dataUnicode},
            {"UNICODE", (void*)dataUnicode}
        });
}


}
