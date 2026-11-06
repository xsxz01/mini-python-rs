#include "../cffi.hpp"

#include <cctype>

namespace minipy {




using namespace minipy;

// data Ascii Lowercase
// data ascii lowercase
// data Ascii Lowercase
static value dataAsciiLowercase(cffi& a){ return a.mkstr("abcdefghijklmnopqrstuvwxyz"); }

// data Ascii Uppercase
// data ascii uppercase
// data Ascii Uppercase
static value dataAsciiUppercase(cffi& a){ return a.mkstr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"); }

// data Ascii Letters
// data ascii letters
// data Ascii Letters
static value dataAsciiLetters(cffi& a){ return a.mkstr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"); }

// data Digits
// data digits
// data Digits
static value dataDigits(cffi& a){ return a.mkstr("0123456789"); }

// data Hexdigits
// data hexdigits
// data Hexdigits
static value dataHexdigits(cffi& a){ return a.mkstr("0123456789abcdefABCDEF"); }

// data Octdigits
// data octdigits
// data Octdigits
static value dataOctdigits(cffi& a){ return a.mkstr("01234567"); }

// data Whitespace
// data whitespace
// data Whitespace
static value dataWhitespace(cffi& a){ return a.mkstr(" \t\n\r\x0b\x0c"); }

// data Punctuation
// data punctuation
// data Punctuation
static value dataPunctuation(cffi& a){ return a.mkstr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"); }

// data Printable
// data printable
// data Printable
static value dataPrintable(cffi& a){
    std::string text;
    for (int c = 0x20; c <= 0x7e; ++c) text += static_cast<char>(c);
    text += " \t\n\r\x0b\x0c";
    return a.mkstr(text);
}

// fn Capwords
// fn capwords
// fn Capwords
static value fnCapwords(cffi& a){
    std::string text = a.arg_str(0);
    std::string separator = (a.argc() > 1) ? a.arg_str(1) : " ";
    std::string result;
    size_t begin = 0;
    bool first = true;
    while (begin <= text.size()){
        size_t end = (separator.empty()) ? text.size() : text.find(separator, begin);
        if (end == std::string::npos) end = text.size();
        std::string word = text.substr(begin, end - begin);
        if (!word.empty()){
            word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
            for (size_t i = 1; i < word.size(); ++i){
                word[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(word[i])));
            }
        }
        if (!first) result += separator;
        result += word;
        first = false;
        if (end >= text.size()) break;
        begin = end + separator.size();
    }
    return a.mkstr(result);
}

// cffi 字符串 lib
// cffi string lib
// cffi ᠦᠰᠦᠭ lib
ffilib cffi_string_lib(){
    return cffi_lib(
        "string",
        {
            {"capwords", (void*)fnCapwords}
        },
        {
            {"ascii_lowercase", (void*)dataAsciiLowercase},
            {"ascii_uppercase", (void*)dataAsciiUppercase},
            {"ascii_letters", (void*)dataAsciiLetters},
            {"digits", (void*)dataDigits},
            {"hexdigits", (void*)dataHexdigits},
            {"octdigits", (void*)dataOctdigits},
            {"whitespace", (void*)dataWhitespace},
            {"punctuation", (void*)dataPunctuation},
            {"printable", (void*)dataPrintable}
        });
}


}
