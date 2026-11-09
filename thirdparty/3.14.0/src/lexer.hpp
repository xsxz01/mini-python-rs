// 词法分析器头文件: token 类型定义与扫描入口
// Lexer header: token types and scanning entry point
// ᠯᠡᠰᠢᠺᠰᠡᠷ ᠦᠨ ᠲᠣᠣᠯᠭ᠎ᠠ ᠪᠢᠴᠢᠭ
#ifndef LEXER_HPP
#define LEXER_HPP

#include <iostream>
#include <vector>
#include <string>

// token 类型常量
// Token type constants
// ᠲᠣᠺᠡᠨ ᠦᠨ ᠲᠥᠷᠥᠯ ᠶᠢᠨ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ
#define tokenType_string 0
#define tokenType_value 1
#define tokenType_symbol 2
#define tokenType_number 3
#define tokenType_newline 4
#define tokenType_indent 5
#define tokenType_dedent 6
#define tokenType_fstring 7

namespace minipy{

// 词法单元: 源码中的最小语义片段
// Lexical token: smallest semantic unit in source code
// ᠬᠡᠯᠡ ᠪᠡᠷᠢᠯᠳᠡᠢᠰᠢᠶ᠋᠎ᠠ ᠲᠣᠺᠡᠨ
struct token{
    unsigned int type;
    std::string value;
    int line = 0;
    int col = 0;
    int endCol = 0;
    bool bytes = false;
};


// 把源码切分成 token 序列
// Split source code into a token sequence
// ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠢ ᠲᠣᠺᠡᠨ ᠦᠨ ᠳᠠᠷᠠᠯᠠᠯᠳ᠋ᠭ᠎ᠠ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
std::vector<token> minipyGetTokens(std::string code);


// 调试用: 打印 token 序列
// Debug helper: print a token sequence
// ᠳ᠋ᠡᠪᠤᠭ ᠬᠡᠷᠡᠴᠡᠯ: ᠲᠣᠺᠡᠨ ᠦᠨ ᠳᠠᠷᠠᠯᠠᠯᠳ᠋ᠭ᠎ᠠ ᠢ ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
void minipyPrintTokens(std::vector<token> tokens);

}


#endif
