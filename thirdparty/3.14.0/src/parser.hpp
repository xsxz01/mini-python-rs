#ifndef PARSER_HPP
#define PARSER_HPP

#include <vector>
#include <string>
#include <utility>
#include "lexer.hpp"

namespace minipy{

// 类 t
// class t
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ t
struct class_t;


// 源码位置 t
// span t
// ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ t
struct span_t{
    int line = 0;
    int startCol = 0;
    int endCol = 0;
    int anchorStart = 0;
    int anchorEnd = 0;

    std::vector<std::pair<int, int>> parts;

// valid
// valid
// valid
    bool valid() const { return line > 0 && startCol > 0 && endCol >= startCol; }

// 拥有 Anchor
// has anchor
// ᠲᠡᠢ Anchor
    bool hasAnchor() const { return anchorStart > 0 && anchorEnd >= anchorStart; }
};

#define eop_assign 0
#define eop_call 1
#define eop_if 2
#define eop_else 3
#define eop_elif 4
#define eop_return 5
#define eop_jmp 6
#define eop_inc 7
#define eop_dec 8
#define eop_for 9
#define eop_raise 10
#define eop_try 11
#define eop_except 12
#define eop_finally 13
#define eop_import 14
#define eop_from_import 15
#define eop_loop_if 16
#define eop_yield 17
#define eop_await 18
#define eop_yield_from 19
#define eop_function 20
#define eop_augassign 21
#define eop_delattr 22
#define eop_delvar 23
#define eop_suppress_if_truthy 24
#define eop_match 25
#define eop_class 26
#define eop_unpack 27
#define eop_expr 28
#define eop_delslice 29


// execute t
// execute t
// execute t
struct execute_t{
    unsigned int operation;
    std::vector<std::string> operands;

    std::vector<std::pair<std::string, std::string>> kwargs;

    std::vector<std::string> argsUnpack;
    std::vector<std::string> kwargsUnpack;
    int line = 0;
    span_t span;
    std::vector<span_t> operandSpans;

    bool hidePropagationCaret = false;

    long long callId = 0;

    size_t jumpTarget = (size_t)(-1);

    size_t breakFor = (size_t)(-1);
};


// 函数 t
// function t
// ᠶᠢᠳᠡ ᠶᠢᠨ ᠠᠷᠭᠠ t
struct function_t{
    std::string name = "";
    std::string returnType = "";
    std::vector<execute_t> body;










































    std::vector<std::string> argNames;
    class_t* parentClass = nullptr;

    std::vector<std::string> globalNames;

    bool isGenerator = false;
    bool isCoroutine = false;

    std::vector<std::string> decorators;


    int variadicArg = -1;
    int keywordArg = -1;

    int positionalOnlyCount = 0;

    std::vector<std::string> capturedNames;

    std::vector<std::pair<int, std::string>> defaultArgs;

    int keywordOnlyStart = -1;

    std::vector<std::string> nonlocalNames;

    void* defaultCache = nullptr;
};


// variable t
// variable t
// variable t
struct variable_t{
    std::string name = "";
    std::string type = "";
    void* value = nullptr;


    std::vector<variable_t*> children;
};


// 类 t
// class t
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ t
struct class_t{
    std::string name = "";
    std::string baseName = "";
    std::string metaclassExpr = "";
    std::string fieldsText = "";
    std::vector<function_t> methods;
    std::vector<variable_t> variables;


    void* attributes = nullptr;

    std::vector<execute_t> initBody;

    class_t* baseClass = nullptr;
    std::string builtinBase = "";

    std::vector<std::pair<std::string, std::string>> classKwargs;
};



// 脚本 t
// script t
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ t
struct script_t{
    std::string name = "";
    std::string source = "";
    function_t mainFunction;

    std::vector<std::string> StringLibrary;
    std::vector<function_t> functions;
    std::vector<class_t> classes;
    std::vector<variable_t> variables;
};

script_t minipyParseTokens(std::vector<token> tokens);

}


#endif
