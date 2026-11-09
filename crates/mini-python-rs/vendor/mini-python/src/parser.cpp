#include "parser.hpp"
#include <map>
#include <utility>
#include <cstdlib>
#include <algorithm>

namespace minipy{



namespace {

// 表达式 Result
// expression result
// ᠢᠯᠡᠳᠬᠡᠯᠳᠡ Result
struct ExprResult{
    std::string text;
    std::string type;
    span_t span;
};

// mark Text
// mark text
// mark Text
std::string markText(const std::string& text, const span_t& span){
    if (!span.valid()) return text;
    std::string marker;
    marker.push_back('\x01');
    marker += std::to_string(span.line) + "_" +
              std::to_string(span.startCol) + "_" +
              std::to_string(span.endCol) + "_" +
              std::to_string(span.anchorStart) + "_" +
              std::to_string(span.anchorEnd);
    if (!span.parts.empty()){
        marker += "|";
        for (size_t i = 0; i < span.parts.size(); ++i){
            if (i > 0) marker += "_";
            marker += std::to_string(span.parts[i].first) + ":" + std::to_string(span.parts[i].second);
        }
    }
    marker.push_back('\x02');
    return text + marker;
}

// strip Trailing Marker
// strip trailing marker
// strip Trailing Marker
std::string stripTrailingMarker(const std::string& text){
    if (text.size() >= 3 && text.back() == '\x02'){
        size_t start = text.rfind('\x01');
        if (start != std::string::npos) return text.substr(0, start);
    }
    return text;
}

long long g_nextCallId = 1;

// 新 调用 Id
// new call id
// ᠰᠢᠨ᠎ᠡ ᠢᠳᠡᠬᠡᠯᠳᠡ Id
long long newCallId(){
    return g_nextCallId++;
}

// 是否 Pure 调用 Result
// is pure call result
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Pure ᠢᠳᠡᠬᠡᠯᠳᠡ Result
bool isPureCallResult(const std::string& text){
    std::string plain = stripTrailingMarker(text);
    if (plain.compare(0, 5, "@fret") != 0) return false;
    for (size_t i = 5; i < plain.size(); ++i){
        if (plain[i] < '0' || plain[i] > '9') return false;
    }
    return true;
}

// 是否 Plain 调用 标识符
// is plain call name
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Plain ᠢᠳᠡᠬᠡᠯᠳᠡ ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
bool isPlainCallName(const std::string& callee){
    if (callee.empty()) return false;
    if (!((callee[0] >= 'a' && callee[0] <= 'z') ||
          (callee[0] >= 'A' && callee[0] <= 'Z') || callee[0] == '_')){
        return false;
    }
    for (char c : callee){
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')){
            return false;
        }
    }
    return true;
}

// 词法单元 源码范围
// token span
// ᠲᠣᠺᠡᠨ ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
span_t tokenSpan(const token& tk){
    span_t span;
    span.line = tk.line;
    span.startCol = tk.col;
    span.endCol = tk.endCol > 0 ? tk.endCol : tk.col;
    span.anchorStart = span.startCol;
    span.anchorEnd = span.endCol;
    return span;
}

// union 源码范围
// union span
// union ᠬᠤᠷᠢᠶᠠᠲ᠋ᠠᠢ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ
span_t unionSpan(const span_t& left, const span_t& right){
    span_t span;
    if (!left.valid()) return right;
    if (!right.valid()) return left;
    span.line = left.line;
    span.startCol = left.startCol;
    span.endCol = right.line == left.line ? right.endCol : left.endCol;
    span.anchorStart = left.anchorStart;
    span.anchorEnd = left.anchorEnd;
    span.parts = left.parts;
    return span;
}

// 解析 Context
// parse context
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Context
struct ParseContext{
    std::map<std::string, std::string> types;
    std::string returnType;
    bool isGlobal = false;

    std::vector<std::string> globalNames;

    std::vector<std::string> nonlocalNames;

    std::vector<std::vector<size_t>> breakStack;
    std::vector<std::vector<size_t>> continueStack;
};

// end 词法单元
// end token
// end ᠲᠣᠺᠡᠨ
const token& endToken(){
    static const token value = {tokenType_dedent, ""};
    return value;
}

// 语法分析器
// parser
// ᠫᠠᠷᠰᠡᠷ
struct Parser{
    const std::vector<token>& tokens;
    script_t& script;
    size_t pos = 0;

    int defDepth = 0;

    int lambdaCounter = 0;

    int compCounter = 0;

    int forCounter = 0;

    int ternCounter = 0;

// shift Jump Targets
// shift jump targets
// shift Jump Targets
    void shiftJumpTargets(std::vector<execute_t>& b, size_t start, long long delta){
        auto shift = [&](std::string& s){
            if (s.empty()) return;
            char* endp = nullptr;
            long long v = std::strtoll(s.c_str(), &endp, 10);
            if (endp == s.c_str()) return;
            s = std::to_string(v + delta);
        };
        for (size_t k = start; k < b.size(); ++k){
            execute_t& e = b[k];
            if (e.operation == eop_jmp || e.operation == eop_else){
                if (e.operands.size() > 0) shift(e.operands[0]);
            } else if (e.operation == eop_if || e.operation == eop_elif){
                if (e.operands.size() > 1) shift(e.operands[1]);
            } else if (e.operation == eop_for){
                if (e.operands.size() > 3){
                    shift(e.operands[2]);
                    shift(e.operands[3]);
                }
            }
            e.jumpTarget = (size_t)(-1);
        }
    }
    Parser(const std::vector<token>& tokens, script_t& script)

// tokens
// tokens
// tokens
        : tokens(tokens), script(script) {}

// at End
// at end
// at End
    bool atEnd() const { return pos >= tokens.size(); }

// cur
// cur
// cur
    const token& cur() const { return atEnd() ? endToken() : tokens[pos]; }

// look
// look
// look
    const token& look(size_t n) const {
        return (pos + n < tokens.size()) ? tokens[pos + n] : endToken();
    }

// 是否 Symbol
// is symbol
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Symbol
    bool isSymbol(const std::string& value) const {
        return cur().type == tokenType_symbol && cur().value == value;
    }

// 是否 Word
// is word
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Word
    bool isWord(const std::string& value) const {
        return cur().type == tokenType_value && cur().value == value;
    }

// match Symbol
// match symbol
// match Symbol
    bool matchSymbol(const std::string& value){
        if (isSymbol(value)){
            pos++;
            return true;
        }
        return false;
    }

// match Word
// match word
// match Word
    bool matchWord(const std::string& value){
        if (isWord(value)){
            pos++;
            return true;
        }
        return false;
    }

// expect Symbol
// expect symbol
// expect Symbol
    void expectSymbol(const std::string& value){
        if (!matchSymbol(value) && !atEnd()){
            pos++;
        }
    }

// expect Word
// expect word
// expect Word
    std::string expectWord(){
        if (cur().type == tokenType_value || cur().type == tokenType_number){
            std::string value = cur().value;
            pos++;
            return value;
        }
        return "";
    }

// at Line End
// at line end
// at Line End
    bool atLineEnd() const {
        return atEnd() || cur().type == tokenType_newline || cur().type == tokenType_dedent;
    }

// eat Newline
// eat newline
// eat Newline
    void eatNewline(){
        if (!atEnd() && cur().type == tokenType_newline) pos++;
    }

// emit
// emit
// emit
    void emit(std::vector<execute_t>& body, execute_t ins){
        ins.line = cur().line;
        body.push_back(ins);
    }

// skip Newlines
// skip newlines
// skip Newlines
    void skipNewlines(){
        while (!atEnd() && cur().type == tokenType_newline) pos++;
    }


// 字符串 Ref
// string ref
// ᠦᠰᠦᠭ Ref
    std::string stringRef(const std::string& value){
        for (size_t i = 0; i < script.StringLibrary.size(); ++i){
            if (script.StringLibrary[i] == value){
                return "#" + std::to_string(i);
            }
        }
        script.StringLibrary.push_back(value);
        return "#" + std::to_string(script.StringLibrary.size() - 1);
    }


    ExprResult parseFString(std::vector<execute_t>& body, ParseContext& ctx,
                            const std::string& content){
        std::string text = stringRef("");
        std::string literal;
        auto flushLiteral = [&](){
            if (!literal.empty()){
                text += ";+;" + stringRef(literal);
                literal.clear();
            }
        };
        size_t i = 0;
        while (i < content.size()){
            char c = content[i];
            if (c == '{'){
                if (i + 1 < content.size() && content[i + 1] == '{'){
                    literal += '{';
                    i += 2;
                    continue;
                }
                flushLiteral();
                size_t start = i + 1;
                size_t end = start;
                int depth = 1;
                char innerQuote = 0;
                bool escape = false;
                while (end < content.size() && depth > 0){
                    char ec = content[end];
                    if (innerQuote){
                        if (escape) escape = false;
                        else if (ec == '\\') escape = true;
                        else if (ec == innerQuote) innerQuote = 0;
                    } else {
                        if (ec == '\'' || ec == '"') innerQuote = ec;
                        else if (ec == '{') depth++;
                        else if (ec == '}') depth--;
                    }
                    if (depth > 0) end++;
                }
                std::string inner = content.substr(start, end - start);

                std::string exprText = inner;
                std::string specText;
                char conversion = 0;
                int bracket = 0;
                char exprQuote = 0;
                for (size_t k = 0; k < exprText.size(); ++k){
                    char ec = exprText[k];
                    if (exprQuote){
                        if (ec == exprQuote) exprQuote = 0;
                        continue;
                    }
                    if (ec == '\'' || ec == '"'){ exprQuote = ec; continue; }
                    if (ec == '(' || ec == '[' || ec == '{') bracket++;
                    else if (ec == ')' || ec == ']' || ec == '}') bracket--;
                    else if (ec == ':' && bracket == 0){
                        specText = inner.substr(k + 1);
                        exprText = exprText.substr(0, k);
                        break;
                    }
                    else if (ec == '!' && bracket == 0 && k + 1 < exprText.size() &&
                             (exprText[k + 1] == 'r' || exprText[k + 1] == 's' || exprText[k + 1] == 'a') &&
                             (k + 2 >= exprText.size() || exprText[k + 2] == ':')){
                        conversion = exprText[k + 1];
                        specText = (k + 2 < exprText.size() && exprText[k + 2] == ':')
                            ? inner.substr(k + 3) : std::string();
                        exprText = exprText.substr(0, k);
                        break;
                    }
                }
                std::vector<token> innerTokens = minipyGetTokens(exprText);
                int baseLine = (pos > 0 && pos <= tokens.size()) ? tokens[pos - 1].line : 0;
                if (baseLine > 0){
                    for (auto& innerToken : innerTokens) innerToken.line += baseLine - 1;
                }
                Parser sub(innerTokens, script);
                ExprResult expr = sub.parseExpression(body, ctx);
                if (!expr.text.empty()){
                    std::string valueText = expr.text;
                    bool needConvert = (expr.type != "str");
                    if (conversion){
                        execute_t convertIns;
                        convertIns.operation = eop_call;
                        convertIns.callId = newCallId();
                        convertIns.operands = {conversion == 'r' ? "repr" :
                                               (conversion == 'a' ? "ascii" : "str"), expr.text};
                        convertIns.line = baseLine;
                        body.push_back(convertIns);
                        valueText = "@fret" + std::to_string(convertIns.callId);
                        needConvert = false;
                    }
                    if (!specText.empty()){

                        execute_t fmtIns;
                        fmtIns.operation = eop_call;
                        fmtIns.callId = newCallId();
                        fmtIns.operands = {"format", valueText, stringRef(specText)};
                        fmtIns.line = baseLine;
                        body.push_back(fmtIns);
                        valueText = "@fret" + std::to_string(fmtIns.callId);
                    } else if (needConvert){
                        execute_t convertIns;
                        convertIns.operation = eop_call;
                        convertIns.callId = newCallId();
                        convertIns.operands = {"str", expr.text};
                        convertIns.line = baseLine;
                        body.push_back(convertIns);
                        valueText = "@fret" + std::to_string(convertIns.callId);
                    }
                    text += ";+;(" + valueText + ")";
                }
                i = (end < content.size()) ? end + 1 : content.size();
                continue;
            }
            if (c == '}'){
                if (i + 1 < content.size() && content[i + 1] == '}'){
                    literal += '}';
                    i += 2;
                    continue;
                }
                i++;
                continue;
            }
            literal += c;
            i++;
        }
        flushLiteral();
        return {text, "str"};
    }

    void recordVariable(ParseContext& ctx, class_t* cls,
                        const std::string& name, const std::string& type){
        if (cls){
            for (auto& v : cls->variables){
                if (v.name == name){
                    v.type = type;
                    return;
                }
            }
            variable_t v;
            v.name = name;
            v.type = type;
            cls->variables.push_back(v);
            return;
        }
        if (!ctx.isGlobal) return;
        for (auto& v : script.variables){
            if (v.name == name){
                v.type = type;
                return;
            }
        }
        variable_t v;
        v.name = name;
        v.type = type;
        script.variables.push_back(v);
    }

// 解析 Statements
// parse statements
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Statements
    void parseStatements(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        while (!atEnd()){
            if (cur().type == tokenType_newline){
                pos++;
                continue;
            }
            if (cur().type == tokenType_dedent){
                break;
            }
            parseStatement(body, ctx, cls);
        }
    }

// 解析 Suite
// parse suite
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Suite
    void parseSuite(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        if (!atEnd() && cur().type == tokenType_newline){
            pos++;
            if (!atEnd() && cur().type == tokenType_indent){
                pos++;
                parseStatements(body, ctx, cls);
                if (!atEnd() && cur().type == tokenType_dedent){
                    pos++;
                }
            }
            return;
        }

        if (!atLineEnd()){
            while (!atLineEnd()){
                parseStatement(body, ctx, cls);
                if (!atEnd() && isSymbol(";")){
                    pos++;
                } else {
                    break;
                }
            }
        }
    }

// skip Line
// skip line
// skip Line
    void skipLine(){
        while (!atEnd() && cur().type != tokenType_newline && cur().type != tokenType_dedent){
            pos++;
        }
        eatNewline();
    }

// emit Loop Jump
// emit loop jump
// emit Loop Jump
    void emitLoopJump(std::vector<execute_t>& body, std::vector<std::vector<size_t>>& stack){
        if (stack.empty()) return;
        execute_t ins;
        ins.operation = eop_jmp;
        ins.operands = {"0"};
        stack.back().push_back(body.size());
        emit(body, ins);
    }

// skip Until
// skip until
// skip Until
    void skipUntil(const std::vector<std::string>& stops){
        int depth = 0;
        while (!atEnd()){
            if (cur().type == tokenType_newline || cur().type == tokenType_dedent){
                return;
            }
            if (cur().type == tokenType_symbol){
                const std::string& value = cur().value;
                if (depth == 0){
                    for (auto& stop : stops){
                        if (value == stop) return;
                    }
                }
                if (value == "(" || value == "[" || value == "{"){
                    depth++;
                } else if (value == ")" || value == "]" || value == "}"){
                    if (depth > 0) depth--;
                }
            }
            pos++;
        }
    }

// skip Balanced
// skip balanced
// skip Balanced
    void skipBalanced(){
        if (!isSymbol("(")) return;
        int depth = 0;
        while (!atEnd()){
            if (cur().type == tokenType_symbol){
                if (cur().value == "("){
                    depth++;
                } else if (cur().value == ")"){
                    depth--;
                    if (depth == 0){
                        pos++;
                        return;
                    }
                }
            }
            pos++;
        }
    }



    ExprResult parseDecoratorLine(std::vector<execute_t>& body, ParseContext& ctx,
                                  std::string& outRawText){
        pos++;

        size_t savedPos = pos;
        size_t scanPos = pos;
        int depth = 0;
        while (scanPos < tokens.size()){
            const auto& t = tokens[scanPos];
            if (t.type == tokenType_newline){
                if (depth <= 0) break;
            }
            if (t.type == tokenType_symbol){
                if (t.value == "(") depth++;
                else if (t.value == ")") depth--;
            }
            scanPos++;
        }
        outRawText.clear();
        for (size_t i = savedPos; i < scanPos; ++i){
            const auto& v = tokens[i].value;

            bool noSpaceBefore = !outRawText.empty() &&
                (v == "." || v == "(" || v == ")" || v == "[" || v == "]" ||
                 v == "," || v == ":" || v == ";");

            if (!outRawText.empty()){
                char last = outRawText.back();
                bool prevNoSpace = (last == '.' || last == '(' || last == '[' ||
                                    last == ',' || last == ':' || last == ';');
                if (noSpaceBefore || prevNoSpace){

                } else {
                    outRawText += " ";
                }
            }
            outRawText += v;
        }

        ExprResult result = parsePrimary(body, ctx);

        pos = scanPos;

        if (!atEnd() && cur().type == tokenType_newline) pos++;
        return result;
    }

// 解析 Statement
// parse statement
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Statement
    void parseStatement(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        if (atEnd() || cur().type == tokenType_newline) return;

        std::vector<ExprResult> decorators;
        std::vector<std::string> decoratorRawTexts;

        while (!atEnd() && cur().type == tokenType_symbol && cur().value == "@"){
            std::string rawText;
            ExprResult deco = parseDecoratorLine(body, ctx, rawText);
            decorators.push_back(deco);
            decoratorRawTexts.push_back(rawText);
        }

        bool isAsync = false;
        if (!atEnd() && cur().type == tokenType_value && cur().value == "async"){
            size_t savedPos = pos;
            pos++;
            if (!atEnd() && cur().type == tokenType_value &&
                (cur().value == "def" || cur().value == "class")){
                isAsync = true;
            } else {
                pos = savedPos;
            }
        }
        if (cur().type == tokenType_value){
            const std::string& word = cur().value;

            if (isAsync && word == "def"){
                parseDef(cls, true, ctx, decorators, decoratorRawTexts, &body);
                return;
            }
            if (word == "def"){
                parseDef(cls, false, ctx, decorators, decoratorRawTexts, &body);
                return;
            }
            if (word == "class"){
                parseClass(decorators, &body);
                return;
            }
            if (word == "match"){


                size_t scan = pos + 1;
                int depth = 0;
                bool isMatchStmt = false;
                for (; scan < tokens.size(); ++scan){
                    const token& tk = tokens[scan];
                    if (tk.type == tokenType_newline || tk.type == tokenType_dedent) break;
                    if (tk.type == tokenType_symbol){
                        const std::string& sv = tk.value;
                        if (sv == "(" || sv == "[" || sv == "{") depth++;
                        else if (sv == ")" || sv == "]" || sv == "}"){
                            if (depth > 0) depth--;
                        } else if (sv == ":" && depth == 0){
                            isMatchStmt = true;
                            break;
                        }
                    }
                }
                if (isMatchStmt){
                    parseMatch(body, ctx, cls);
                    return;
                }
            }
            if (word == "if"){
                parseIf(body, ctx, cls);
                return;
            }
            if (word == "while"){
                parseWhile(body, ctx, cls);
                return;
            }
            if (word == "for"){
                parseFor(body, ctx, cls);
                return;
            }
            if (word == "try"){
                parseTry(body, ctx, cls);
                return;
            }
            if (word == "with"){
                parseWith(body, ctx, cls);
                return;
            }
            if (word == "return"){
                parseReturn(body, ctx);
                return;
            }
            if (word == "raise"){
                parseRaise(body, ctx);
                return;
            }
            if (word == "del"){
                parseDel(body, ctx);
                return;
            }
            if (word == "pass"){
                skipLine();
                return;
            }
            if (word == "break" || word == "continue"){
                pos++;
                if (word == "break"){
                    emitLoopJump(body, ctx.breakStack);
                } else {
                    emitLoopJump(body, ctx.continueStack);
                }
                skipLine();
                return;
            }
            if (word == "import"){
                parseImport(body);
                return;
            }
            if (word == "from"){
                parseFromImport(body);
                return;
            }
        }
        parseSimpleStatement(body, ctx, cls);
    }
    void parseDef(class_t* cls, bool isAsync, ParseContext& ctx,
                  const std::vector<ExprResult>& decorators,
                  const std::vector<std::string>& decoratorRawTexts,
                  std::vector<execute_t>* body){
        pos++;
        std::string name = expectWord();

// Python 3.12+ 泛型类型参数 def f[T](...), 注解整体被忽略, 直接跳过 [...]
// PEP 695 type parameters def f[T](...): annotations are ignored, skip [...]
// PEP 695 ᠶᠢᠨ ᠲᠠᠶᠢᠯᠪᠤᠷᠢ ᠶᠢ ᠠᠯᠭᠤᠰᠬᠢᠨ᠎ᠠ
        if (isSymbol("[")){
            int typeParamDepth = 0;
            while (!atEnd()){
                if (cur().type == tokenType_symbol){
                    if (cur().value == "[") typeParamDepth++;
                    else if (cur().value == "]"){
                        typeParamDepth--;
                        pos++;
                        if (typeParamDepth == 0) break;
                        continue;
                    }
                }
                pos++;
            }
        }
        expectSymbol("(");
        defDepth++;
        std::vector<std::string> args;
        std::vector<std::pair<int, std::string>> defaultArgs;
        int variadicArg = -1;
        int keywordArg = -1;
        int positionalOnlyCount = 0;
        int keywordOnlyStart = -1;
        if (!isSymbol(")")){
            while (!atEnd()){
                int argIndex = static_cast<int>(args.size());
                if (cur().type == tokenType_value){
                    args.push_back(cur().value);
                    pos++;
                } else if (isSymbol("**")){
                    pos++;

                    if (!atEnd() && cur().type == tokenType_value){
                        keywordArg = static_cast<int>(args.size());
                        args.push_back(cur().value);
                        pos++;
                    }
                    argIndex = -1;
                } else if (isSymbol("*")){
                    pos++;

                    if (!atEnd() && cur().type == tokenType_value){
                        variadicArg = static_cast<int>(args.size());
                        args.push_back(cur().value);
                        pos++;
                        argIndex = -1;
                    } else {

                        if (keywordOnlyStart < 0) keywordOnlyStart = static_cast<int>(args.size());
                        argIndex = -1;
                    }
                } else if (isSymbol("/")){

                    positionalOnlyCount = static_cast<int>(args.size());
                    pos++;
                    argIndex = -1;
                } else {
                    break;
                }

                if (isSymbol(":")){
                    pos++;
                    skipUntil({",", ")", "="});
                }

                if (isSymbol("=")){
                    pos++;
                    std::vector<execute_t> scratchBody;
                    std::vector<execute_t>& defBody = body ? *body : scratchBody;
                    ExprResult defExpr = parseExpression(defBody, ctx);
                    if (argIndex >= 0){
                        defaultArgs.push_back({argIndex, defExpr.text});
                    }
                }
                if (keywordOnlyStart < 0 && variadicArg < 0){  }
                if (!matchSymbol(",")) break;
            }
        }
        expectSymbol(")");

        if (isSymbol("->")){
            pos++;
            skipUntil({":"});
        }
        expectSymbol(":");
        function_t function;

        if (defDepth > 1){
            function.name = name + "_inner_" + std::to_string(script.functions.size());
        } else {
            function.name = name;
        }
        function.argNames = args;
        function.isCoroutine = isAsync;
        function.variadicArg = variadicArg;
        function.keywordArg = keywordArg;
        function.positionalOnlyCount = positionalOnlyCount;
        function.defaultArgs = defaultArgs;
        function.keywordOnlyStart = keywordOnlyStart;
        ParseContext fctx;
        fctx.isGlobal = false;
        for (auto& arg : args){
            fctx.types[arg] = "";
        }
        parseSuite(function.body, fctx, nullptr);
        function.returnType = fctx.returnType;

        function.globalNames = fctx.globalNames;
        function.nonlocalNames = fctx.nonlocalNames;

        for (auto& ins : function.body){
            if (ins.operation == eop_yield || ins.operation == eop_yield_from){
                function.isGenerator = true;
                break;
            }
        }

        if (isAsync) function.isCoroutine = true;



        if (!decorators.empty()){
            if (cls != nullptr){

                for (size_t i = 0; i < decorators.size(); ++i){
                    std::string raw = (i < decoratorRawTexts.size()) ? decoratorRawTexts[i] : decorators[i].text;
                    function.decorators.push_back(raw);
                }
            } else if (body){

                std::string currentArg = name;
                for (auto it = decorators.rbegin(); it != decorators.rend(); ++it){
                    const ExprResult& dec = *it;
                    execute_t callIns;
                    callIns.operation = eop_call;
                    callIns.callId = newCallId();
                    callIns.operands.push_back(dec.text);
                    callIns.operands.push_back(currentArg);
                    callIns.span = dec.span;
                    callIns.operandSpans.push_back(dec.span);
                    callIns.operandSpans.push_back(dec.span);
                    emit(*body, callIns);
                    currentArg = "@fret" + std::to_string(callIns.callId);
                }
                execute_t assign;
                assign.operation = eop_assign;
                assign.operands = {name, "", currentArg};
                assign.span = decorators.front().span;
                assign.operandSpans.push_back(span_t{});
                assign.operandSpans.push_back(span_t{});
                assign.operandSpans.push_back(decorators.front().span);
                emit(*body, assign);
            }
        }
        bool isNestedDef = (defDepth > 1) && (body != nullptr);
        if (cls){
            cls->methods.push_back(std::move(function));
        } else {

            if (isNestedDef){
                execute_t fnIns;
                fnIns.operation = eop_function;


                fnIns.operands.push_back(function.name);
                fnIns.operands.push_back(name);
                emit(*body, fnIns);
            }
            script.functions.push_back(std::move(function));
        }
        defDepth--;
    }

// 解析 类
// parse class
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
    void parseClass(const std::vector<ExprResult>& decorators = {},
                    std::vector<execute_t>* body = nullptr){
        pos++;
        std::string name = expectWord();
        class_t klass;
        klass.name = name;
        if (isSymbol("(")){
            pos++;

            int depth = 1;
            size_t kwScanPos = pos;



            std::string rawParenContent;
            size_t parenStart = pos;
            {
                int d = 1;
                size_t sp = pos;
                while (!atEnd() && d > 0){
                    if (isSymbol("(")){ d++; rawParenContent += cur().value; pos++; }
                    else if (isSymbol(")")){ d--; if (d > 0) rawParenContent += cur().value; pos++; }
                    else { rawParenContent += cur().value; pos++; }
                }
            }





            std::string& content = rawParenContent;



            std::vector<std::string> parts;
            {
                int d = 0;
                size_t begin = 0;
                for (size_t i = 0; i <= content.size(); ++i){
                    char c = (i < content.size()) ? content[i] : ',';
                    if (c == '(' || c == '[' || c == '{') d++;
                    else if (c == ')' || c == ']' || c == '}'){ if (d > 0) d--; }
                    else if (c == ',' && d == 0){
                        parts.push_back(content.substr(begin, i - begin));
                        begin = i + 1;
                    }
                }
            }
            auto trim = [](const std::string& s) -> std::string {
                size_t a = 0, b = s.size();
                while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
                while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) b--;
                return s.substr(a, b - a);
            };
            std::string baseText;
            for (auto& part : parts){
                std::string item = trim(part);
                if (item.empty()) continue;

                int d = 0;
                size_t eq = std::string::npos;
                for (size_t i = 0; i < item.size(); ++i){
                    char c = item[i];
                    if (c == '(' || c == '[' || c == '{') d++;
                    else if (c == ')' || c == ']' || c == '}'){ if (d > 0) d--; }
                    else if (c == '=' && d == 0){ eq = i; break; }
                }
                if (eq != std::string::npos){
                    std::string kwName = trim(item.substr(0, eq));
                    std::string kwValue = trim(item.substr(eq + 1));
                    if (kwName == "metaclass") klass.metaclassExpr = kwValue;
                    else klass.classKwargs.push_back({kwName, kwValue});
                } else {
                    if (!baseText.empty()) baseText += ", ";
                    baseText += item;
                }
            }
            klass.baseName = baseText;
        }
        expectSymbol(":");
        std::vector<execute_t>& initBody = klass.initBody;
        ParseContext cctx;
        cctx.isGlobal = false;
        parseSuite(initBody, cctx, &klass);
        script.classes.push_back(std::move(klass));


        if (body){
            execute_t classIns;
            classIns.operation = eop_class;
            classIns.operands.push_back(std::to_string(script.classes.size() - 1));
            classIns.operands.push_back(name);
            emit(*body, classIns);
        }

        if (body && !decorators.empty()){
            std::string currentArg = name;
            for (auto it = decorators.rbegin(); it != decorators.rend(); ++it){
                const ExprResult& dec = *it;
                execute_t callIns;
                callIns.operation = eop_call;
                callIns.callId = newCallId();
                callIns.operands.push_back(dec.text);
                callIns.operands.push_back(currentArg);
                callIns.span = dec.span;
                callIns.operandSpans.push_back(dec.span);
                callIns.operandSpans.push_back(dec.span);
                emit(*body, callIns);
                currentArg = "@fret" + std::to_string(callIns.callId);
            }
            execute_t assign;
            assign.operation = eop_assign;
            assign.operands = {name, "", currentArg};
            assign.span = decorators.front().span;
            assign.operandSpans.push_back(span_t{});
            assign.operandSpans.push_back(span_t{});
            assign.operandSpans.push_back(decorators.front().span);
            emit(*body, assign);
        }
    }

// 解析 条件
// parse if
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢᠶᠡᠬᠡ ᠬᠡᠷᠡᠴᠡᠯ
    void parseIf(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        pos++;
        ExprResult cond = parseExpression(body, ctx);
        expectSymbol(":");
        size_t ifIndex = body.size();
        execute_t ins;
        ins.operation = eop_if;
        ins.operands = {cond.text, std::to_string(ifIndex + 1), "0"};
        ins.span = cond.span;
        ins.operandSpans = {cond.span};
        emit(body, ins);
        parseSuite(body, ctx, cls);
        size_t lastIndex = ifIndex;
        int lastFalseOperand = 2;
        while (matchWord("elif")){
            size_t entry = body.size();
            body[lastIndex].operands[lastFalseOperand] = std::to_string(entry);
            ExprResult elifCond = parseExpression(body, ctx);
            expectSymbol(":");
            size_t elifIndex = body.size();
            execute_t elifIns;
            elifIns.operation = eop_elif;
            elifIns.operands = {elifCond.text, "0"};
            elifIns.span = elifCond.span;
            elifIns.operandSpans = {elifCond.span};
            emit(body, elifIns);
            parseSuite(body, ctx, cls);
            lastIndex = elifIndex;
            lastFalseOperand = 1;
        }
        if (matchWord("else")){
            expectSymbol(":");
            size_t elseIndex = body.size();
            body[lastIndex].operands[lastFalseOperand] = std::to_string(elseIndex);
            execute_t elseIns;
            elseIns.operation = eop_else;
            elseIns.operands = {"0"};
            emit(body, elseIns);
            parseSuite(body, ctx, cls);
            body[elseIndex].operands[0] = std::to_string(body.size());
        } else {
            body[lastIndex].operands[lastFalseOperand] = std::to_string(body.size());
        }
        eatNewline();
    }

// 解析 当
// parse while
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠤᠨᠳᠤ ᠬᠡᠷᠡᠴᠡᠯ
    void parseWhile(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        pos++;

        size_t loopTarget = body.size();
        ExprResult cond = parseExpression(body, ctx);
        expectSymbol(":");
        size_t ifIndex = body.size();
        execute_t ins;
        ins.operation = eop_loop_if;
        ins.operands = {cond.text, std::to_string(ifIndex + 1), "0"};
        ins.span = cond.span;
        ins.operandSpans = {cond.span};
        emit(body, ins);
        ctx.breakStack.push_back({});
        ctx.continueStack.push_back({});
        parseSuite(body, ctx, cls);
        execute_t jmp;
        jmp.operation = eop_jmp;
        jmp.operands = {std::to_string(loopTarget)};
        emit(body, jmp);

        size_t normalTarget = body.size();
        if (matchWord("else")){
            expectSymbol(":");
            parseSuite(body, ctx, cls);
        }
        size_t endIndex = body.size();
        for (size_t jump : ctx.breakStack.back()){
            body[jump].operands[0] = std::to_string(endIndex);
        }
        for (size_t jump : ctx.continueStack.back()){
            body[jump].operands[0] = std::to_string(loopTarget);
        }
        ctx.breakStack.pop_back();
        ctx.continueStack.pop_back();
        body[ifIndex].operands[1] = std::to_string(ifIndex + 1);
        body[ifIndex].operands[2] = std::to_string(normalTarget);
        eatNewline();
    }
    void parseFor(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls,
                  bool isAsync = false){
        pos++;
        std::string var;
        std::vector<std::string> targets;
        {
            size_t p = pos;
            while (true){
                std::string item;
                if (!parseTargetItem(p, item)){ targets.clear(); break; }
                targets.push_back(item);
                if (p < tokens.size() && tokens[p].type == tokenType_symbol && tokens[p].value == ","){
                    p++;
                    continue;
                }
                break;
            }
            bool foundIn = (p < tokens.size() && tokens[p].type == tokenType_value &&
                            tokens[p].value == "in");
            if (foundIn){
                pos = p;
            } else {
                targets.clear();
            }
        }
        if (targets.empty()){
            var = expectWord();
        } else {
            var = targets.size() == 1 ? targets[0] : "__for_item_" + std::to_string(forCounter++);
            if (targets.size() == 1 && !var.empty() && var[0] == '*') var = var.substr(1);
        }
        matchWord("in");
        ExprResult iterable = parseExpressionList(body, ctx);
        expectSymbol(":");
        size_t forIndex = body.size();
        execute_t ins;
        ins.operation = eop_for;
        ins.operands = {var, iterable.text, "0", "0"};
        if (isAsync) ins.operands.push_back("async");
        ins.span = iterable.span;
        ins.operandSpans = {{}, iterable.span};
        emit(body, ins);

        if (targets.size() > 1){
            execute_t unpackIns;
            unpackIns.operation = eop_unpack;
            unpackIns.operands.push_back(var);
            for (auto& t : targets) unpackIns.operands.push_back(t);
            emit(body, unpackIns);
            for (auto& t : targets){
                std::string plain = t;
                if (!plain.empty() && plain[0] == '*') plain = plain.substr(1);
                if (plain.size() >= 2 && plain.front() == '(') continue;
                ctx.types[plain] = "";
                recordVariable(ctx, cls, plain, "");
            }
        }
        ctx.breakStack.push_back({});
        ctx.continueStack.push_back({});
        parseSuite(body, ctx, cls);
        execute_t jmp;
        jmp.operation = eop_jmp;
        jmp.operands = {std::to_string(forIndex)};
        emit(body, jmp);

        size_t normalTarget = body.size();
        if (matchWord("else")){
            expectSymbol(":");
            parseSuite(body, ctx, cls);
        }
        size_t endIndex = body.size();
        for (size_t jump : ctx.breakStack.back()){
            body[jump].operands[0] = std::to_string(endIndex);
            body[jump].breakFor = forIndex;
        }
        for (size_t jump : ctx.continueStack.back()){
            body[jump].operands[0] = std::to_string(forIndex);
        }
        ctx.breakStack.pop_back();
        ctx.continueStack.pop_back();
        body[forIndex].operands[2] = std::to_string(forIndex + 1);
        body[forIndex].operands[3] = std::to_string(normalTarget);
        eatNewline();
    }















// pat Enc
// pat enc
// pat Enc
    static std::string patEnc(const std::string& value){
        return std::to_string(value.size()) + ":" + value;
    }

// 解析 Pattern
// parse pattern
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Pattern
    std::string parsePattern(){
        std::string p = parsePatternOr();
        if (isWord("as")){
            pos++;
            std::string name = expectWord();
            if (name.empty()) name = "_";
            p = "A" + patEnc(name) + p;
        }
        return p;
    }

// 解析 Pattern 或
// parse pattern or
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Pattern ᠡᠰᠡᠪᠡᠯ
    std::string parsePatternOr(){
        std::string p = parsePatternAtom();
        while (isSymbol("|")){
            pos++;
            p = "O" + p + parsePatternAtom();
        }
        return p;
    }

// 解析 Pattern Atom
// parse pattern atom
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Pattern Atom
    std::string parsePatternAtom(){
        if (isSymbol("*")){
            pos++;
            std::string name = expectWord();
            if (name.empty()) name = "_";
            return "T" + patEnc(name);
        }
        if (isSymbol("-") && look(1).type == tokenType_number){
            pos++;
            std::string num = expectWord();
            return "L" + patEnc("-" + num);
        }
        if (cur().type == tokenType_number){
            return "L" + patEnc(expectWord());
        }
        if (cur().type == tokenType_string){
            std::string value = cur().value;
            pos++;
            return "L" + patEnc(stringRef(value));
        }
        if (cur().type == tokenType_value){
            std::string name = expectWord();
            if (name == "None" || name == "True" || name == "False"){
                return "L" + patEnc(name);
            }
            std::string dotted = name;
            bool hasDot = false;
            while (isSymbol(".") && look(1).type == tokenType_value){
                pos++;
                dotted += "." + expectWord();
                hasDot = true;
            }
            if (isSymbol("(")){
                return parseClassPattern(dotted);
            }
            if (hasDot) return "V" + patEnc(dotted);
            if (name == "_") return "W";
            return "C" + patEnc(name);
        }
        if (isSymbol("(")){
            pos++;
            if (matchSymbol(")")) return "S" + patEnc("0");
            std::vector<std::string> subs;
            subs.push_back(parsePattern());
            bool isSeq = false;
            while (matchSymbol(",")){
                isSeq = true;
                if (isSymbol(")")) break;
                subs.push_back(parsePattern());
            }
            expectSymbol(")");

            if (!isSeq) return subs[0];
            std::string out = "S" + patEnc(std::to_string(subs.size()));
            for (auto& s : subs) out += s;
            return out;
        }
        if (isSymbol("[")){
            pos++;
            std::vector<std::string> subs;
            while (!isSymbol("]") && !atEnd()){
                subs.push_back(parsePattern());
                if (!matchSymbol(",")) break;
                if (isSymbol("]")) break;
            }
            expectSymbol("]");
            std::string out = "S" + patEnc(std::to_string(subs.size()));
            for (auto& s : subs) out += s;
            return out;
        }
        if (isSymbol("{")){
            pos++;
            std::vector<std::string> entries;
            int count = 0;
            while (!isSymbol("}") && !atEnd()){
                if (isSymbol("**")){
                    pos++;
                    std::string name = expectWord();
                    if (name.empty()) name = "_";
                    entries.push_back("R" + patEnc(name));
                } else {
                    std::string key = parsePatternKeyExpr();
                    expectSymbol(":");
                    std::string sub = parsePattern();
                    entries.push_back("E" + patEnc(key) + sub);
                }
                count++;
                if (!matchSymbol(",")) break;
                if (isSymbol("}")) break;
            }
            expectSymbol("}");
            std::string out = "M" + patEnc(std::to_string(count));
            for (auto& e : entries) out += e;
            return out;
        }

        pos++;
        return "W";
    }

// 解析 Pattern Key 表达式
// parse pattern key expression
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Pattern Key ᠢᠯᠡᠳᠬᠡᠯᠳᠡ
    std::string parsePatternKeyExpr(){
        std::string out;
        while (!atEnd() && !isSymbol(":") && !isSymbol(",") && !isSymbol("}") &&
               cur().type != tokenType_newline && cur().type != tokenType_dedent){
            if (cur().type == tokenType_string){
                out += stringRef(cur().value);
                pos++;
            } else {
                out += cur().value;
                pos++;
            }
        }
        return out;
    }

// 解析 类 Pattern
// parse class pattern
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ Pattern
    std::string parseClassPattern(const std::string& clsExpr){
        pos++;
        std::vector<std::string> posSubs;
        std::vector<std::pair<std::string, std::string>> kwSubs;
        while (!isSymbol(")") && !atEnd()){
            if (cur().type == tokenType_value && look(1).type == tokenType_symbol && look(1).value == "="){
                std::string attr = cur().value;
                pos += 2;
                kwSubs.push_back({attr, parsePattern()});
            } else {
                posSubs.push_back(parsePattern());
            }
            if (!matchSymbol(",")) break;
            if (isSymbol(")")) break;
        }
        expectSymbol(")");
        std::string out = "K" + patEnc(clsExpr) + patEnc(std::to_string(posSubs.size()));
        for (auto& s : posSubs) out += s;
        out += patEnc(std::to_string(kwSubs.size()));
        for (auto& kv : kwSubs) out += patEnc(kv.first) + kv.second;
        return out;
    }

// 解析 Match
// parse match
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Match
    void parseMatch(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        pos++;
        ExprResult subject = parseExpression(body, ctx);
        std::string subjectTemp = "__match_subject_" + std::to_string(newCallId());
        execute_t assignSub;
        assignSub.operation = eop_assign;

        assignSub.operands = {subjectTemp, "", subject.text};
        assignSub.span = subject.span;
        assignSub.operandSpans = {{}, {}, subject.span};
        emit(body, assignSub);
        ctx.types[subjectTemp] = subject.type;

        expectSymbol(":");
        eatNewline();
        if (!atEnd() && cur().type == tokenType_indent) pos++;

        std::vector<std::pair<size_t, int>> pendingNext;
        std::vector<size_t> endJmps;
        bool prevWildcard = false;

        while (!atEnd() && isWord("case")){
            size_t entryIndex = body.size();
            if (prevWildcard){

                execute_t raiseIns;
                raiseIns.operation = eop_raise;
                raiseIns.operands = {"SyntaxError", stringRef("wildcard makes remaining patterns unreachable")};
                emit(body, raiseIns);
            }
            for (auto& pr : pendingNext){
                body[pr.first].operands[pr.second] = std::to_string(entryIndex);
            }
            pendingNext.clear();
            prevWildcard = false;
            pos++;
            std::string pattern = parsePattern();
            std::string okVar = "__match_ok_" + std::to_string(newCallId());
            execute_t matchIns;
            matchIns.operation = eop_match;
            matchIns.operands = {subjectTemp, pattern, okVar};
            matchIns.span = subject.span;
            emit(body, matchIns);
            size_t idxIfMatch = body.size();
            execute_t ifMatch;
            ifMatch.operation = eop_if;
            ifMatch.operands = {okVar, "0", "0"};
            emit(body, ifMatch);
            std::string guardText;
            size_t idxIfGuard = (size_t)(-1);
            if (isWord("if")){
                pos++;
                ExprResult guard = parseExpression(body, ctx);
                guardText = guard.text;
                idxIfGuard = body.size();
                execute_t ifGuard;
                ifGuard.operation = eop_if;
                ifGuard.operands = {guardText, "0", "0"};
                emit(body, ifGuard);
            }
            size_t bodyStart = body.size();

            body[idxIfMatch].operands[1] = std::to_string(guardText.empty() ? bodyStart : idxIfMatch + 1);
            pendingNext.push_back({idxIfMatch, 2});
            if (idxIfGuard != (size_t)(-1)){
                body[idxIfGuard].operands[1] = std::to_string(bodyStart);
                pendingNext.push_back({idxIfGuard, 2});
            }
            expectSymbol(":");
            parseSuite(body, ctx, cls);
            execute_t jmp;
            jmp.operation = eop_jmp;
            jmp.operands = {"0"};
            emit(body, jmp);
            endJmps.push_back(body.size() - 1);
            prevWildcard = (pattern == "W");
            while (!atEnd() && cur().type == tokenType_newline) pos++;
        }

        size_t endIndex = body.size();
        for (auto& pr : pendingNext){
            body[pr.first].operands[pr.second] = std::to_string(endIndex);
        }
        for (size_t j : endJmps){
            body[j].operands[0] = std::to_string(endIndex);
        }
        if (!atEnd() && cur().type == tokenType_dedent) pos++;
        eatNewline();
    }


// 解析 With
// parse with
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ With
    void parseWith(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        pos++;

// 单个上下文项的解析状态, with a, b: 按嵌套 with 展开
// state of one context item; with a, b: expands like nested with blocks
// ᠨᠢᠭᠡ context item ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
        struct WithItem{
            std::string ctxTemp;
            std::string asVar;
            span_t span;
            size_t tryIndex = 0;
        };
        std::vector<WithItem> items;

        while (true){
            WithItem item;

            ExprResult exprResult = parseExpression(body, ctx);
            item.span = exprResult.span;

            int withId = newCallId();
            item.ctxTemp = "@wctx" + std::to_string(withId);

            execute_t assignCtx;
            assignCtx.operation = eop_assign;
            assignCtx.operands = {item.ctxTemp, "", exprResult.text};
            assignCtx.span = exprResult.span;
            emit(body, assignCtx);

            if (matchWord("as")){
                if (cur().type == tokenType_value){
                    item.asVar = cur().value;
                    pos++;
                }
            }

            execute_t enterCall;
            enterCall.operation = eop_call;
            enterCall.callId = newCallId();
            enterCall.operands.push_back(item.ctxTemp + ".__enter__");

            enterCall.span = item.span;
            enterCall.operandSpans.push_back(item.span);
            emit(body, enterCall);

            if (!item.asVar.empty()){
                execute_t assignVar;
                assignVar.operation = eop_assign;
                assignVar.operands = {item.asVar, "", "@fret" + std::to_string(enterCall.callId)};
                assignVar.span = item.span;
                assignVar.operandSpans.push_back(span_t{});
                assignVar.operandSpans.push_back(span_t{});
                assignVar.operandSpans.push_back(item.span);
                emit(body, assignVar);
            }

            item.tryIndex = body.size();
            execute_t tryIns;
            tryIns.operation = eop_try;
            tryIns.operands = {"0", "0", "0"};
            emit(body, tryIns);

            items.push_back(item);

            if (!matchSymbol(",")) break;
        }

        expectSymbol(":");

        parseSuite(body, ctx, cls);

// 按相反顺序收尾, 后进入的先退出, 与嵌套 with 行为一致
// finish in reverse order: the later entered exits first, like nested with
// ᠰᠢᠭᠦᠳᠡᠷᠢ ᠳᠠᠷᠠᠭ᠎ᠠ ᠪᠡᠷ ᠲᠡᠭᠦᠰᠭᠡᠨ᠎ᠡ
        for (auto it = items.rbegin(); it != items.rend(); ++it){
            size_t bodyEnd = body.size();
            execute_t skip;
            skip.operation = eop_jmp;
            skip.operands = {"0"};
            emit(body, skip);

            size_t finallyIdx = body.size();
            execute_t finIns;
            finIns.operation = eop_finally;
            finIns.operands = {"0"};
            emit(body, finIns);

            execute_t exitCall;
            exitCall.operation = eop_call;
            exitCall.callId = newCallId();
            int exitCallId = exitCall.callId;
            exitCall.operands.push_back(it->ctxTemp + ".__exit__");
            exitCall.operands.push_back("$exc_type");
            exitCall.operands.push_back("$exc_value");
            exitCall.operands.push_back("$exc_tb");
            exitCall.span = it->span;
            exitCall.operandSpans.push_back(it->span);
            emit(body, exitCall);

            execute_t suppressIns;
            suppressIns.operation = eop_suppress_if_truthy;
            suppressIns.operands = {"@fret" + std::to_string(exitCallId)};
            suppressIns.span = it->span;
            emit(body, suppressIns);

            size_t finallyEnd = body.size();
            execute_t finJmp;
            finJmp.operation = eop_jmp;
            finJmp.operands = {"0"};
            emit(body, finJmp);

            size_t endIndex = body.size();

            body[it->tryIndex].operands[1] = std::to_string(finallyIdx);
            body[it->tryIndex].operands[2] = std::to_string(endIndex);

            body[bodyEnd].operands[0] = std::to_string(finallyIdx);

            body[finallyEnd].operands[0] = std::to_string(endIndex);

            body[finallyIdx].operands[0] = std::to_string(endIndex);
        }

        eatNewline();
    }

// 解析 尝试
// parse try
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢᠶᠡᠬᠡ ᠵᠢᠳᠡ
    void parseTry(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){
        pos++;
        expectSymbol(":");

        size_t tryIndex = body.size();


        execute_t tryIns;
        tryIns.operation = eop_try;
        tryIns.operands = {"0", "0", "0"};
        emit(body, tryIns);

        parseSuite(body, ctx, cls);


        size_t bodyEnd = body.size();
        execute_t skip;
        skip.operation = eop_jmp;
        skip.operands = {"0"};
        emit(body, skip);

        size_t elseStart = (size_t)(-1);
        size_t elseEndJmpIdx = (size_t)(-1);


        std::vector<std::string> exceptTypes;
        std::vector<std::string> exceptVars;
        std::vector<size_t> handlerIndices;
        std::vector<size_t> handlerEndJmpIdx;

        while (matchWord("except")){
            size_t handlerIdx = body.size();
            execute_t exceptIns;
            exceptIns.operation = eop_except;
            std::string typeText = "";
            std::string asVar = "";
            bool bareColon = cur().type == tokenType_symbol && cur().value == ":";
            if (!atLineEnd() && !bareColon){

                if (cur().type == tokenType_value){
                    typeText = cur().value;
                    pos++;
                    while (isSymbol(".") && pos + 1 < tokens.size() &&
                           tokens[pos + 1].type == tokenType_value){
                        pos++;
                        typeText += "." + cur().value;
                        pos++;
                    }
                } else {
                    ExprResult e = parseExpression(body, ctx);
                    typeText = e.text;

                    size_t m1 = typeText.rfind('\x01');
                    size_t m2 = typeText.rfind('\x02');
                    if (m1 != std::string::npos && m2 != std::string::npos && m2 > m1)
                        typeText = typeText.substr(0, m1);
                }

                while (matchSymbol(",")){
                    parseExpression(body, ctx);
                }
                if (matchWord("as")){
                    if (cur().type == tokenType_value){
                        asVar = cur().value;
                        pos++;
                    }
                }
            }
            expectSymbol(":");
            exceptIns.operands = {typeText, asVar, "0"};
            emit(body, exceptIns);
            exceptTypes.push_back(typeText);
            exceptVars.push_back(asVar);
            handlerIndices.push_back(handlerIdx);
            parseSuite(body, ctx, cls);
            size_t handlerEnd = body.size();
            execute_t jmpAfter;
            jmpAfter.operation = eop_jmp;
            jmpAfter.operands = {"0"};
            emit(body, jmpAfter);
            handlerEndJmpIdx.push_back(handlerEnd);
            eatNewline();
        }


        if (matchWord("else")){
            expectSymbol(":");
            elseStart = body.size();
            parseSuite(body, ctx, cls);
            elseEndJmpIdx = body.size();
            execute_t elseJmp;
            elseJmp.operation = eop_jmp;
            elseJmp.operands = {"0"};
            emit(body, elseJmp);
        }

        size_t finallyIdx = (size_t)(-1);
        size_t finallyEndJmpIdx = (size_t)(-1);
        if (matchWord("finally")){
            expectSymbol(":");
            finallyIdx = body.size();
            execute_t finIns;
            finIns.operation = eop_finally;
            finIns.operands = {"0"};
            emit(body, finIns);
            parseSuite(body, ctx, cls);
            finallyEndJmpIdx = body.size();
            execute_t finJmp;
            finJmp.operation = eop_jmp;
            finJmp.operands = {"0"};
            emit(body, finJmp);
        }

        size_t endIndex = body.size();


        body[tryIndex].operands[0] = std::to_string(exceptTypes.size());
        body[tryIndex].operands[1] = std::to_string(finallyIdx == (size_t)(-1) ? endIndex : finallyIdx);
        body[tryIndex].operands[2] = std::to_string(endIndex);
        for (size_t h : handlerIndices){
            body[tryIndex].operands.push_back(std::to_string(h));
        }


        size_t normalJumpTarget = (elseStart != (size_t)(-1))
            ? elseStart
            : (finallyIdx == (size_t)(-1) ? endIndex : finallyIdx);
        body[bodyEnd].operands[0] = std::to_string(normalJumpTarget);

        if (elseEndJmpIdx != (size_t)(-1)){
            size_t afterElse = (finallyIdx == (size_t)(-1)) ? endIndex : finallyIdx;
            body[elseEndJmpIdx].operands[0] = std::to_string(afterElse);
        }



        size_t afterExceptIdx = (finallyIdx == (size_t)(-1)) ? endIndex : finallyIdx;
        for (size_t h = 0; h < handlerIndices.size(); ++h){
            body[handlerEndJmpIdx[h]].operands[0] = std::to_string(afterExceptIdx);
            body[handlerIndices[h]].operands[2] = std::to_string(afterExceptIdx);
        }




        if (finallyIdx != (size_t)(-1) && finallyEndJmpIdx != (size_t)(-1)){
            body[finallyEndJmpIdx].operands[0] = std::to_string(endIndex);

            body[finallyIdx].operands[0] = std::to_string(endIndex);
        }

        eatNewline();
    }

// 解析 返回
// parse return
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
    void parseReturn(std::vector<execute_t>& body, ParseContext& ctx){
        pos++;
        execute_t ins;
        ins.operation = eop_return;
        if (!atLineEnd()){
            ExprResult value = parseExpressionList(body, ctx);

            if (isPureCallResult(value.text) && !body.empty() &&
                body.back().operation == eop_call && !body.back().operands.empty() &&
                isPlainCallName(stripTrailingMarker(body.back().operands[0]))){
                body.back().hidePropagationCaret = true;
            }
            ins.operands.push_back(value.text);
            ins.span = value.span;
            ins.operandSpans = {value.span};
            if (ctx.returnType.empty() && !value.type.empty()){
                ctx.returnType = value.type;
            }
        }
        emit(body, ins);
        eatNewline();
    }


// 解析 Del
// parse del
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Del
    void parseDel(std::vector<execute_t>& body, ParseContext& ctx){
        pos++;
        size_t start = pos;
        size_t end = 0;
        std::string target;
        if (atLineEnd() || !scanLValue(end, target) || target.empty()){
            skipLine();
            return;
        }


        if (target.back() == ']'){

            std::vector<size_t> bracketStack;
            size_t openToken = start;
            for (size_t i = start; i < end; ++i){
                if (tokens[i].type != tokenType_symbol) continue;
                if (tokens[i].value == "["){
                    bracketStack.push_back(i);
                } else if (tokens[i].value == "]" && !bracketStack.empty()){
                    openToken = bracketStack.back();
                    bracketStack.pop_back();
                }
            }
            int depth = 0;
            size_t open = std::string::npos;
            for (size_t i = target.size(); i > 0; --i){
                char c = target[i - 1];
                if (c == ']'){ depth++; }
                else if (c == '['){ depth--; if (depth == 0){ open = i - 1; break; } }
            }
            if (open == std::string::npos || open == 0){
                skipLine();
                return;
            }
            span_t targetSpan = tokenSpan(tokens[start]);
            span_t lastSpan = tokenSpan(tokens[end - 1]);
            span_t openSpan = tokenSpan(tokens[openToken]);
            targetSpan.endCol = lastSpan.endCol;
            targetSpan.anchorStart = openSpan.startCol;
            targetSpan.anchorEnd = lastSpan.endCol;
            std::string baseText = target.substr(0, open);
            std::string indexText = target.substr(open + 1, target.size() - open - 2);

            bool isSlice = false;
            int sliceDepth = 0;
            for (size_t i = 0; i < indexText.size(); ++i){
                char c = indexText[i];
                if (c == '(' || c == '[' || c == '{') sliceDepth++;
                else if (c == ')' || c == ']' || c == '}') sliceDepth--;
                else if (c == ':' && sliceDepth == 0){ isSlice = true; break; }
                else if (c == '\'' || c == '"') break;
            }
            if (isSlice){
                execute_t delIns;
                delIns.operation = eop_delslice;
                delIns.operands = {baseText, indexText};
                delIns.span = targetSpan;
                delIns.operandSpans = {targetSpan, targetSpan};
                emit(body, delIns);
            } else {
                std::vector<std::string> args = {baseText, indexText};
                std::vector<span_t> argSpans = {targetSpan, targetSpan};
                emitCall(body, "__delitem__", args, targetSpan, targetSpan, argSpans);
            }
            pos = end;
            eatNewline();
            return;
        }


        size_t dot = target.rfind('.');
        if (dot != std::string::npos){
            std::string baseName = target.substr(0, dot);
            std::string member = target.substr(dot + 1);
            span_t callSpan = tokenSpan(tokens[start]);
            if (end > start) callSpan.endCol = tokenSpan(tokens[end - 1]).endCol;
            std::vector<std::string> args = {baseName, member};

            execute_t ins;
            ins.operation = eop_delattr;
            ins.operands = {baseName, member};
            ins.span = callSpan;
            emit(body, ins);
            pos = end;
            eatNewline();
            return;
        }


        span_t delSpan = tokenSpan(tokens[start]);
        if (end > start) delSpan.endCol = tokenSpan(tokens[end - 1]).endCol;
        execute_t ins;
        ins.operation = eop_delvar;
        ins.operands = {target};
        ins.span = delSpan;
        emit(body, ins);
        pos = end;
        eatNewline();
    }


// 解析 抛出
// parse raise
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠡᠷᠭᠢᠵᠡ ᠣᠷᠣᠭᠤᠯᠬᠤ
    void parseRaise(std::vector<execute_t>& body, ParseContext& ctx){
        pos++;
        execute_t ins;
        ins.operation = eop_raise;
        if (!atLineEnd() && cur().type == tokenType_value && cur().value != "from"){
            token nameTok = cur();
            std::string typeName = cur().value;
            pos++;
            ins.operands.push_back(typeName);
            span_t nameSpan = tokenSpan(nameTok);
            nameSpan.anchorStart = nameSpan.startCol;
            nameSpan.anchorEnd = nameSpan.endCol;
            ins.operandSpans.push_back(nameSpan);
            if (isSymbol("(")){
                pos++;
                if (!isSymbol(")")){
                    while (!atEnd()){
                        ExprResult arg = parseExpression(body, ctx);
                        ins.operands.push_back(arg.text);
                        ins.operandSpans.push_back(arg.span);
                        if (!matchSymbol(",")) break;
                    }
                }
                expectSymbol(")");
            }

            if (isWord("from")){
                pos++;
                parseExpression(body, ctx);
            }
        } else if (!atLineEnd()){

            ins.operands.push_back("");
            ExprResult value = parseExpression(body, ctx);
            ins.operands.push_back(value.text);
            ins.operandSpans.push_back(value.span);
            if (isWord("from")){
                pos++;
                parseExpression(body, ctx);
            }
        }

        emit(body, ins);
        eatNewline();
    }

// 解析 导入
// parse import
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠪᠠᠭ᠌ᠠᠪᠣᠷᠢᠶ᠎ᠠ
    void parseImport(std::vector<execute_t>& body){
        pos++;

        while (!atLineEnd()){
            std::string moduleName;
            if (cur().type == tokenType_value){
                moduleName = cur().value;
                pos++;

                while (isSymbol(".")){
                    pos++;
                    if (cur().type == tokenType_value){
                        moduleName += "." + cur().value;
                        pos++;
                    }
                }
            } else {
                break;
            }
            std::string alias = moduleName;
            if (isWord("as")){
                pos++;
                if (cur().type == tokenType_value){
                    alias = cur().value;
                    pos++;
                }
            }
            execute_t ins;
            ins.operation = eop_import;
            ins.operands = {moduleName, alias};
            emit(body, ins);
            if (!matchSymbol(",")) break;
        }
        eatNewline();
    }

// 解析 从 导入
// parse from import
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠡᠴᠡ ᠪᠠᠭ᠌ᠠᠪᠣᠷᠢᠶ᠎ᠠ
    void parseFromImport(std::vector<execute_t>& body){
        pos++;
        std::string moduleName;
        if (cur().type == tokenType_value){
            moduleName = cur().value;
            pos++;
            while (isSymbol(".")){
                pos++;
                if (cur().type == tokenType_value){
                    moduleName += "." + cur().value;
                    pos++;
                }
            }
        }




        if (!isWord("import")){

            skipLine();
            return;
        }
        pos++;
        while (!atLineEnd()){
            if (cur().type == tokenType_symbol && cur().value == "*"){

                pos++;
                break;
            }
            std::string memberName;
            if (cur().type == tokenType_value){
                memberName = cur().value;
                pos++;
            } else {
                break;
            }
            std::string alias = memberName;
            if (isWord("as")){
                pos++;
                if (cur().type == tokenType_value){
                    alias = cur().value;
                    pos++;
                }
            }
            execute_t ins;
            ins.operation = eop_from_import;
            ins.operands = {moduleName, memberName, alias};
            emit(body, ins);
            if (!matchSymbol(",")) break;
        }
        eatNewline();
    }

// 是否 Augmented Op
// is augmented op
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Augmented Op
    bool isAugmentedOp(const std::string& value) const {
        return value == "+=" || value == "-=" || value == "*=" || value == "/=" ||
               value == "//=" || value == "**=" || value == "%=" ||
               value == "&=" || value == "|=" || value == "^=" ||
               value == "<<=" || value == ">>=";
    }

// 扫描 L 值
// scan l value
// ᠶᠢᠨᠳᠡᠬᠡ L ᠭᠠᠷᠴᠠᠭ
    bool scanLValue(size_t& end, std::string& target){
        return scanLValueFrom(pos, end, target);
    }


// 扫描 L 值 从
// scan l value from
// ᠶᠢᠨᠳᠡᠬᠡ L ᠭᠠᠷᠴᠠᠭ ᠡᠴᠡ
    bool scanLValueFrom(size_t start, size_t& end, std::string& target){
        size_t p = start;
        if (p >= tokens.size() || tokens[p].type != tokenType_value) return false;

        target = tokens[p].value;
        p++;
        while (p < tokens.size()){
            if (tokens[p].type == tokenType_symbol && tokens[p].value == "." &&
                p + 1 < tokens.size() && tokens[p + 1].type == tokenType_value){
                target += "." + tokens[p + 1].value;
                p += 2;
                continue;
            }
            if (tokens[p].type == tokenType_symbol && tokens[p].value == "["){
                size_t q = p + 1;
                int depth = 1;
                std::string inner;
                while (q < tokens.size() && depth > 0){
                    const token& tk = tokens[q];
                    if (tk.type == tokenType_symbol){
                        if (tk.value == "["){
                            depth++;
                        } else if (tk.value == "]"){
                            depth--;
                            if (depth == 0) break;
                        }
                    }
                    if (tk.type == tokenType_string){
                        inner += stringRef(tk.value);
                    } else if (tk.type == tokenType_number ||
                               tk.type == tokenType_value){
                        inner += tk.value;
                    } else if (tk.type == tokenType_fstring){

                        return false;
                    } else if (tk.type == tokenType_symbol){
                        if (tk.value == "." || tk.value == "[" || tk.value == "]" ||
                            tk.value == "(" || tk.value == ")" || tk.value == "," ||
                            tk.value == ":"){
                            inner += tk.value;
                        } else if (inner.empty() || inner.back() == '(' ||
                                   inner.back() == ',' || inner.back() == ':' ||
                                   inner.back() == ';'){

                            inner += tk.value + ";";
                        } else {
                            inner += ";" + tk.value + ";";
                        }
                    }
                    q++;
                }
                if (depth != 0 || q >= tokens.size()) return false;
                target += "[" + inner + "]";
                p = q + 1;
                continue;
            }
            break;
        }
        end = p;
        return true;
    }


// 查找 Top Level Equals
// find top level equals
// ᠬᠠᠯᠠᠬᠤ Top Level Equals
    size_t findTopLevelEquals(size_t start) const {
        int depth = 0;
        for (size_t i = start; i < tokens.size(); ++i){
            const token& tk = tokens[i];
            if (tk.type == tokenType_newline || tk.type == tokenType_dedent) break;
            if (tk.type != tokenType_symbol) continue;
            if (tk.value == "(" || tk.value == "[" || tk.value == "{") depth++;
            else if (tk.value == ")" || tk.value == "]" || tk.value == "}") depth--;
            else if (tk.value == "=" && depth == 0) return i;
        }
        return std::string::npos;
    }


// match Paren At
// match paren at
// match Paren At
    size_t matchParenAt(size_t start) const {
        int depth = 0;
        for (size_t i = start; i < tokens.size(); ++i){
            const token& tk = tokens[i];
            if (tk.type != tokenType_symbol) continue;
            if (tk.value == "(" || tk.value == "[" || tk.value == "{") depth++;
            else if (tk.value == ")" || tk.value == "]" || tk.value == "}"){
                depth--;
                if (depth == 0) return i;
            }
        }
        return std::string::npos;
    }


// 解析 Target Item
// parse target item
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Target Item
    bool parseTargetItem(size_t& p, std::string& out){
        bool star = false;
        if (p < tokens.size() && tokens[p].type == tokenType_symbol && tokens[p].value == "*"){
            star = true;
            p++;
        }
        if (p < tokens.size() && tokens[p].type == tokenType_symbol && tokens[p].value == "("){
            size_t close = matchParenAt(p);
            if (close == std::string::npos) return false;
            std::vector<std::string> subs;
            size_t q = p + 1;
            while (q < close){
                std::string sub;
                if (!parseTargetItem(q, sub)) return false;
                subs.push_back(sub);
                if (q < close && tokens[q].type == tokenType_symbol && tokens[q].value == ","){
                    q++;
                    continue;
                }
                break;
            }
            std::string joined;
            for (size_t i = 0; i < subs.size(); ++i){
                if (i > 0) joined += ",";
                joined += subs[i];
            }
            out = "(" + joined + ")";
            p = close + 1;
        } else {
            size_t e = 0;
            std::string t;
            if (!scanLValueFrom(p, e, t)) return false;
            out = t;
            p = e;
        }
        if (star) out = "*" + out;
        return true;
    }


// 解析 Expression 列表
// parse expression list
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Expression ᠲᠠᠯᠠᠭ᠌
    ExprResult parseExpressionList(std::vector<execute_t>& body, ParseContext& ctx){
        ExprResult first = parseExpression(body, ctx);
        if (!isSymbol(",") || cur().line != first.span.line) return first;
        std::vector<ExprResult> items{first};
        while (isSymbol(",") && cur().line == first.span.line){
            pos++;
            if (atLineEnd() || isSymbol(")") || isSymbol("]") || isSymbol("}")) break;
            items.push_back(parseExpression(body, ctx));
        }
        std::string text = "(";
        span_t span = first.span;
        for (size_t i = 0; i < items.size(); ++i){
            if (i > 0) text += ",";
            text += items[i].text;
            span = unionSpan(span, items[i].span);
        }
        text += ")";
        return {text, "tuple", span};
    }


    void parseUnpackAssignment(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls,
                               const std::vector<std::string>& targets){
        ExprResult value = parseExpressionList(body, ctx);
        execute_t ins;
        ins.operation = eop_unpack;
        ins.operands.push_back(value.text);
        for (auto& t : targets) ins.operands.push_back(t);
        ins.span = value.span;
        ins.operandSpans.push_back(value.span);
        for (size_t i = 0; i < targets.size(); ++i) ins.operandSpans.push_back(span_t{});
        emit(body, ins);
        for (auto& t : targets){
            std::string plain = t;
            if (!plain.empty() && plain[0] == '*') plain = plain.substr(1);
            if (plain.size() >= 2 && plain.front() == '(') continue;
            ctx.types[plain] = "";
            recordVariable(ctx, cls, plain, "");
        }
    }


    void parseChainAssignment(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls,
                              const std::vector<std::string>& targets){
        ExprResult value = parseExpressionList(body, ctx);
        for (auto& t : targets){
            execute_t ins;
            ins.operation = eop_assign;
            ins.operands = {t, value.type, value.text};
            ins.span = value.span;
            ins.operandSpans.push_back({});
            ins.operandSpans.push_back({});
            ins.operandSpans.push_back(value.span);
            emit(body, ins);
            ctx.types[t] = value.type;
            recordVariable(ctx, cls, t, value.type);
        }
    }

// 解析 Simple Statement
// parse simple statement
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Simple Statement
    void parseSimpleStatement(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls){

        if (matchWord("global")){
            do {
                std::string gname = expectWord();
                ctx.globalNames.push_back(gname);
            } while (matchSymbol(","));
            skipLine();
            return;
        }

        if (matchWord("nonlocal")){
            do {
                std::string nname = expectWord();
                ctx.nonlocalNames.push_back(nname);
            } while (matchSymbol(","));
            skipLine();
            return;
        }

        if (matchWord("assert")){
            ExprResult cond = parseExpression(body, ctx);
            std::vector<std::string> raiseOps{"AssertionError"};
            if (matchSymbol(",")){
                ExprResult msg = parseExpression(body, ctx);
                raiseOps.push_back(msg.text);
            }
            size_t ifIdx = body.size();
            execute_t ifIns;
            ifIns.operation = eop_if;
            ifIns.operands = {cond.text, "0", "0"};
            ifIns.span = cond.span;
            ifIns.operandSpans = {cond.span};
            emit(body, ifIns);
            execute_t raiseIns;
            raiseIns.operation = eop_raise;
            raiseIns.operands = raiseOps;
            emit(body, raiseIns);
            size_t after = body.size();
            body[ifIdx].operands[1] = std::to_string(after);
            body[ifIdx].operands[2] = std::to_string(ifIdx + 1);
            eatNewline();
            return;
        }
        if (cur().type == tokenType_value || isSymbol("(") || isSymbol("*")){
            size_t end = 0;
            std::string target;
            if (scanLValue(end, target)){
                const token& after = (end < tokens.size()) ? tokens[end] : endToken();
                if (after.type == tokenType_symbol && after.value == "="){
                    pos = end + 1;

                    std::vector<std::string> chainTargets{target};
                    while (true){
                        size_t e2 = 0;
                        std::string t2;
                        if (scanLValueFrom(pos, e2, t2) &&
                            e2 < tokens.size() && tokens[e2].type == tokenType_symbol &&
                            tokens[e2].value == "="){
                            chainTargets.push_back(t2);
                            pos = e2 + 1;
                        } else {
                            break;
                        }
                    }
                    if (chainTargets.size() > 1){
                        parseChainAssignment(body, ctx, cls, chainTargets);
                    } else {
                        parseAssignment(body, ctx, cls, target);
                    }
                    eatNewline();
                    return;
                }
                if (after.type == tokenType_symbol && isAugmentedOp(after.value)){
                    pos = end + 1;
                    parseAugmentedAssignment(body, ctx, cls, target, after.value);
                    eatNewline();
                    return;
                }

// 带类型注解的赋值 (x: int = 1), 注解部分直接跳过, 按普通赋值处理
// annotated assignment (x: int = 1): skip the annotation, assign as usual
// ᠲᠠᠶᠢᠯᠪᠤᠷᠢ ᠲᠠᠢ ᠣᠩᠭᠣᠳᠠᠬᠤ (x: int = 1)
                if (after.type == tokenType_symbol && after.value == ":" &&
                    target != "lambda"){
                    pos = end + 1;
                    int annDepth = 0;
                    while (!atEnd() && cur().type != tokenType_newline &&
                           cur().type != tokenType_dedent){
                        if (cur().type == tokenType_symbol){
                            const std::string& sv = cur().value;
                            if (sv == "(" || sv == "[" || sv == "{") annDepth++;
                            else if (sv == ")" || sv == "]" || sv == "}") annDepth--;
                            else if (sv == "=" && annDepth == 0) break;
                        }
                        pos++;
                    }
                    if (!atEnd() && cur().type == tokenType_symbol && cur().value == "="){
                        pos++;
                        parseAssignment(body, ctx, cls, target);
                    }
                    eatNewline();
                    return;
                }
            }
            {

                size_t eqPos = findTopLevelEquals(pos);
                if (eqPos != std::string::npos){
                    size_t p = pos;
                    std::vector<std::string> targets;
                    bool ok = true;
                    while (p < eqPos){
                        std::string item;
                        if (!parseTargetItem(p, item)){ ok = false; break; }
                        targets.push_back(item);
                        if (p < eqPos){
                            if (tokens[p].type == tokenType_symbol && tokens[p].value == ","){
                                p++;
                                if (p >= eqPos) break;
                                continue;
                            }
                            ok = false;
                            break;
                        }
                    }
                    if (ok && !targets.empty() && p == eqPos){
                        pos = eqPos + 1;
                        parseUnpackAssignment(body, ctx, cls, targets);
                        eatNewline();
                        return;
                    }
                }
            }

            if (matchWord("yield")){
                bool isYieldFrom = false;

                if (matchWord("from")){

                    isYieldFrom = true;
                }
                std::string exprText;
                span_t yieldSpan;
                yieldSpan.line = tokens[pos-1].line;
                yieldSpan.startCol = tokens[pos-1].col;
                if (isYieldFrom){

                    ExprResult er = parseExpression(body, ctx);
                    exprText = er.text;
                    yieldSpan.endCol = er.span.endCol;
                    yieldSpan.anchorStart = yieldSpan.startCol;
                    yieldSpan.anchorEnd = yieldSpan.endCol;
                    execute_t ins;
                    ins.operation = eop_yield_from;
                    ins.operands = {std::string(), exprText};
                    ins.span = yieldSpan;
                    ins.operandSpans = {er.span};
                    emit(body, ins);
                } else if (!atLineEnd()){

                    ExprResult er = parseExpression(body, ctx);
                    exprText = er.text;
                    yieldSpan.endCol = er.span.endCol;
                    yieldSpan.anchorStart = yieldSpan.startCol;
                    yieldSpan.anchorEnd = yieldSpan.endCol;
                    execute_t ins;
                    ins.operation = eop_yield;
                    ins.operands = {std::string(), exprText};
                    ins.span = yieldSpan;
                    ins.operandSpans = {er.span};
                    emit(body, ins);
                } else {

                    yieldSpan.endCol = tokens[pos-1].endCol > 0 ? tokens[pos-1].endCol : tokens[pos-1].col;
                    yieldSpan.anchorStart = yieldSpan.startCol;
                    yieldSpan.anchorEnd = yieldSpan.endCol;
                    execute_t ins;
                    ins.operation = eop_yield;
                    ins.operands = {std::string(), std::string("None")};
                    ins.span = yieldSpan;
                    emit(body, ins);
                }
                eatNewline();
                return;
            }
        }


        if (!atLineEnd()){

            if (matchWord("await")){

                ExprResult er = parseExpression(body, ctx);

                execute_t awaitIns;
                awaitIns.operation = eop_await;
                awaitIns.operands = {std::string(), er.text};
                awaitIns.span.line = tokens[pos-1].line;
                awaitIns.span.startCol = tokens[pos-1].col;
                awaitIns.span.endCol = er.span.endCol;
                awaitIns.span.anchorStart = awaitIns.span.startCol;
                awaitIns.span.anchorEnd = awaitIns.span.endCol;
                awaitIns.operandSpans = {er.span};
                emit(body, awaitIns);
            } else {
                ExprResult er = parseExpression(body, ctx);

                execute_t exprIns;
                exprIns.operation = eop_expr;
                exprIns.operands = {er.text};
                exprIns.span = er.span;
                exprIns.operandSpans = {er.span};
                emit(body, exprIns);
            }
        }
        eatNewline();
    }

    void parseAssignment(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls,
                         const std::string& name){

        if (cur().type == tokenType_value && cur().value == "yield"){
            token yieldTok = cur();
            pos++;
            bool isYieldFrom = matchWord("from");
            std::string exprText = "None";
            span_t exprSpan;
            exprSpan.line = yieldTok.line;
            exprSpan.startCol = yieldTok.col;
            std::vector<span_t> operandSpans;
            if (isYieldFrom || !atLineEnd()){
                ExprResult er = parseExpression(body, ctx);
                exprText = er.text;
                exprSpan.endCol = er.span.endCol;
                exprSpan.anchorStart = exprSpan.startCol;
                exprSpan.anchorEnd = exprSpan.endCol;
                operandSpans.push_back(er.span);
            }
            execute_t ins;
            ins.operation = isYieldFrom ? eop_yield_from : eop_yield;
            ins.operands = {name, exprText};
            ins.span = exprSpan;
            ins.operandSpans = operandSpans;
            emit(body, ins);
            ctx.types[name] = "";
            recordVariable(ctx, cls, name, "");
            return;
        }

        if (cur().type == tokenType_value && cur().value == "await"){
            token awaitTok = cur();
            pos++;
            ExprResult er = parseExpression(body, ctx);
            span_t awaitSpan;
            awaitSpan.line = awaitTok.line;
            awaitSpan.startCol = awaitTok.col;
            awaitSpan.endCol = er.span.endCol;
            awaitSpan.anchorStart = awaitSpan.startCol;
            awaitSpan.anchorEnd = awaitSpan.endCol;
            execute_t ins;
            ins.operation = eop_await;
            ins.operands = {name, er.text};
            ins.span = awaitSpan;
            ins.operandSpans = {er.span};
            emit(body, ins);
            ctx.types[name] = "";
            recordVariable(ctx, cls, name, "");
            return;
        }
        if (isSymbol("[")){
            token openTok = cur();
            size_t savePos = pos;
            pos++;

            ExprResult firstElement;
            bool isComp = false;
            size_t probeStart = body.size();
            if (!isSymbol("]")){
                size_t tmpPos = pos;
                firstElement = parseExpression(body, ctx);

                body.resize(probeStart);
                if (isWord("for")){
                    isComp = true;
                }
            }
            if (isComp){

                pos = savePos;
                ExprResult value = parseExpression(body, ctx);
                execute_t ins;
                ins.operation = eop_assign;
                ins.operands = {name, value.type, value.text};
                ins.span = value.span;
                ins.operandSpans.push_back({});
                ins.operandSpans.push_back({});
                ins.operandSpans.push_back(value.span);
                emit(body, ins);
                ctx.types[name] = value.type.empty() ? "list" : value.type;
                recordVariable(ctx, cls, name, ctx.types[name]);
                return;
            }

            pos = savePos;
            pos++;
            std::vector<std::string> elements;
            std::vector<span_t> elementSpans;
            if (!isSymbol("]")){
                while (!atEnd()){
                    ExprResult element = parseExpression(body, ctx);
                    elements.push_back(element.text);
                    elementSpans.push_back(element.span);
                    if (!matchSymbol(",")) break;
                    if (isSymbol("]")) break;
                }
            }
            token closeTok = cur();
            expectSymbol("]");

            span_t listSpan;
            listSpan.line = openTok.line;
            listSpan.startCol = openTok.col;
            listSpan.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
            listSpan.anchorStart = listSpan.startCol;
            listSpan.anchorEnd = listSpan.endCol;

            execute_t ins;
            ins.operation = eop_assign;
            ins.operands.push_back(name);
            ins.operands.push_back("list");
            for (auto& element : elements){
                ins.operands.push_back(element);
            }
            ins.span = listSpan;
            ins.operandSpans.push_back({});
            ins.operandSpans.push_back({});
            for (auto& span : elementSpans){
                ins.operandSpans.push_back(span);
            }
            emit(body, ins);

            ctx.types[name] = "list";
            recordVariable(ctx, cls, name, "list");
            if (cls != nullptr && name == "_fields_"){
                std::string joined = "[";
                for (size_t ei = 0; ei < elements.size(); ++ei){
                    if (ei > 0) joined += ",";
                    joined += elements[ei];
                }
                joined += "]";
                cls->fieldsText = joined;
            }
            return;
        }

        ExprResult value = parseExpressionList(body, ctx);


        if (name.find('.') == std::string::npos && isPureCallResult(value.text) &&
            !body.empty() && body.back().operation == eop_call){
            body.back().hidePropagationCaret = true;
        }

        execute_t ins;
        ins.operation = eop_assign;
        ins.operands = {name, value.type, value.text};
        ins.span = value.span;
        ins.operandSpans.push_back({});
        ins.operandSpans.push_back({});
        ins.operandSpans.push_back(value.span);
        emit(body, ins);

        ctx.types[name] = value.type;
        recordVariable(ctx, cls, name, value.type);

        if (cls != nullptr && name == "_fields_"){
            cls->fieldsText = value.text;
        }
    }

    void parseAugmentedAssignment(std::vector<execute_t>& body, ParseContext& ctx, class_t* cls,
                                  const std::string& name, const std::string& opWithEq){
        std::string op = opWithEq.substr(0, opWithEq.size() - 1);
        ExprResult value = parseExpression(body, ctx);

        std::string type;
        auto it = ctx.types.find(name);
        if (it != ctx.types.end()) type = it->second;
        if (type.empty()) type = value.type;


        execute_t ins;
        ins.operation = eop_augassign;
        ins.operands = {name, op, value.text};
        ins.span = value.span;
        ins.operandSpans.push_back({});
        ins.operandSpans.push_back({});
        ins.operandSpans.push_back(value.span);
        emit(body, ins);

        ctx.types[name] = type;
        recordVariable(ctx, cls, name, type);
    }


// binary Op
// binary op
// binary Op
    std::string binaryOp() const {
        if (cur().type == tokenType_symbol){
            const std::string& value = cur().value;
            if (value == "+" || value == "-" || value == "*" || value == "/" ||
                value == "//" || value == "%" || value == "**" ||
                value == "==" || value == "!=" || value == "<" || value == ">" ||
                value == "<=" || value == ">=" ||
                value == "|" || value == "&" || value == "^" ||
                value == "<<" || value == ">>"){
                return value;
            }
        }
        if (cur().type == tokenType_value){
            if (cur().value == "and" || cur().value == "or"){
                return cur().value;
            }
            if (cur().value == "in"){
                return "in";
            }
            if (cur().value == "is"){
                if (look(1).type == tokenType_value && look(1).value == "not"){
                    return "is not";
                }
                return "is";
            }
            if (cur().value == "not" &&
                look(1).type == tokenType_value && look(1).value == "in"){
                return "not in";
            }
        }
        return "";
    }

// 优先级
// precedence
// ᠠᠨᠲᠠᠷᠲᠠ
    int precedence(const std::string& op) const {

        if (op == "or") return 1;
        if (op == "and") return 2;
        if (op == "|") return 3;
        if (op == "^") return 4;
        if (op == "&") return 5;
        if (op == "==" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=" ||
            op == "in" || op == "not in" || op == "is" || op == "is not") return 6;
        if (op == "<<" || op == ">>") return 7;
        if (op == "+" || op == "-") return 8;
        if (op == "*" || op == "/" || op == "//" || op == "%") return 9;
        if (op == "**") return 10;
        return 0;
    }

    std::string combineType(const std::string& op,
                            const std::string& left,
                            const std::string& right) const {
        if (op == "==" || op == "!=" || op == "<" || op == ">" ||
            op == "<=" || op == ">=" || op == "and" || op == "or" ||
            op == "in" || op == "not in" || op == "is" || op == "is not"){
            return "bool";
        }
        if (op == "+" && left == "str" && right == "str"){
            return "str";
        }
        if (op == "*" && ((left == "str" && (right == "int" || right == "str")) ||
                          (right == "str" && left == "int"))){
            return "str";
        }
        if (op == "|"){
            if (left == "dict" && right == "dict") return "dict";
            if ((left == "int" || left == "bool") &&
                (right == "int" || right == "bool")) return "int";
            return "";
        }
        if (op == "/"){
            return "float";
        }
        if (left == "float" || right == "float"){
            return "float";
        }
        if (left == "int" && right == "int"){
            return "int";
        }
        return "";
    }

// 解析 Expression
// parse expression
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Expression
    ExprResult parseExpression(std::vector<execute_t>& body, ParseContext& ctx, int minPrec = 0){
        size_t insMark = body.size();
        ExprResult left = parseUnary(body, ctx);


        std::vector<std::string> chainOperands;
        std::vector<std::string> chainOps;

        while (!atEnd()){
            std::string op = binaryOp();
            if (op.empty()) break;
            int prec = precedence(op);
            if (prec < minPrec) break;

            const token& opFirst = cur();
            pos += (op == "is not" || op == "not in") ? 2 : 1;
            const token& opLast = tokens[pos - 1];
            span_t opSpan;
            opSpan.line = opFirst.line;
            opSpan.startCol = opFirst.col;
            opSpan.endCol = opLast.endCol > 0 ? opLast.endCol : opLast.col;
            opSpan.anchorStart = opSpan.startCol;
            opSpan.anchorEnd = opSpan.endCol;

            int nextMin = (op == "**") ? prec : prec + 1;
            ExprResult right = parseExpression(body, ctx, nextMin);

            span_t combined = unionSpan(left.span, right.span);
            combined.anchorStart = opSpan.startCol;
            combined.anchorEnd = opSpan.endCol;

            if (op == "<" || op == ">" || op == "<=" || op == ">="){
                combined.anchorStart = combined.startCol;
                combined.anchorEnd = combined.endCol;
            }

            bool isCmp = (op == "==" || op == "!=" || op == "<" || op == ">" ||
                          op == "<=" || op == ">=" || op == "in" || op == "not in" ||
                          op == "is" || op == "is not");
            if (isCmp){
                if (chainOps.empty()) chainOperands.push_back(left.text);
                chainOperands.push_back(right.text);
                chainOps.push_back(op);
            } else {
                chainOperands.clear();
                chainOps.clear();
            }

            left.text = left.text + ";" + markText(op, opSpan) + ";" + right.text;
            left.type = combineType(op, left.type, right.type);
            left.span = combined;
        }
        if (chainOps.size() >= 2){
            std::string built;
            for (size_t i = 0; i < chainOps.size(); ++i){
                if (i > 0) built += ";and;";
                built += "(" + chainOperands[i] + ";" + chainOps[i] + ";" + chainOperands[i + 1] + ")";
            }
            left.text = built;
            left.type = "bool";
        }

        if (minPrec == 0 && isSymbol(":=")){
            std::string leftPlain = stripTrailingMarker(left.text);
            bool assignable = isPlainCallName(leftPlain) &&
                              leftPlain != "True" && leftPlain != "False" && leftPlain != "None";
            pos++;
            if (assignable){
                ExprResult rhs = parseExpression(body, ctx, 0);
                execute_t ins;
                ins.operation = eop_assign;
                ins.operands = {leftPlain, rhs.type, rhs.text};
                ins.span = unionSpan(left.span, rhs.span);
                ins.operandSpans.push_back({});
                ins.operandSpans.push_back({});
                ins.operandSpans.push_back(rhs.span);
                emit(body, ins);
                ctx.types[leftPlain] = rhs.type;
                span_t combined = unionSpan(left.span, rhs.span);
                return {leftPlain, rhs.type, combined};
            }

            ExprResult rhs = parseExpression(body, ctx, 0);
            std::string message = "cannot assign to " + (leftPlain.empty() ? "expression" : leftPlain);
            execute_t raiseIns;
            raiseIns.operation = eop_raise;
            raiseIns.operands = {"SyntaxError", stringRef(message)};
            raiseIns.span = left.span;
            raiseIns.operandSpans.push_back(left.span);
            raiseIns.operandSpans.push_back(left.span);
            emit(body, raiseIns);
            return {leftPlain, "", unionSpan(left.span, rhs.span)};
        }


        if (minPrec == 0 && isWord("if")){
            size_t savedPos = pos;
            std::vector<execute_t> savedTail(body.begin() + insMark, body.end());
            pos++;

            std::vector<execute_t> thenIns(body.begin() + insMark, body.end());
            body.resize(insMark);
            ExprResult cond = parseExpression(body, ctx);
            if (!matchWord("else")){

                pos = savedPos;
                body.resize(insMark);
                body.insert(body.end(), savedTail.begin(), savedTail.end());
                return left;
            }
            size_t elseMark = body.size();
            ExprResult other = parseExpression(body, ctx);
            std::vector<execute_t> elseIns(body.begin() + elseMark, body.end());
            body.resize(elseMark);

            std::string temp = "__tern_" + std::to_string(ternCounter++);
            size_t ifIdx = body.size();
            execute_t ifIns;
            ifIns.operation = eop_if;
            ifIns.operands = {cond.text, "0", "0"};
            ifIns.span = cond.span;
            ifIns.operandSpans = {cond.span, cond.span, cond.span};
            emit(body, ifIns);

            size_t thenStart = body.size();
            size_t thenShift = static_cast<long long>(thenStart) - static_cast<long long>(insMark);
            body.insert(body.end(), thenIns.begin(), thenIns.end());
            shiftJumpTargets(body, thenStart, thenShift);

            execute_t assignThen;
            assignThen.operation = eop_assign;
            assignThen.operands = {temp, left.type, left.text};
            assignThen.span = left.span;
            assignThen.operandSpans = {{}, {}, left.span};
            emit(body, assignThen);

            size_t jmpIdx = body.size();
            execute_t jmpIns;
            jmpIns.operation = eop_jmp;
            jmpIns.operands = {"0"};
            emit(body, jmpIns);

            size_t elseStart = body.size();
            long long elseShift = static_cast<long long>(elseStart) - static_cast<long long>(elseMark);
            body.insert(body.end(), elseIns.begin(), elseIns.end());
            shiftJumpTargets(body, elseStart, elseShift);

            execute_t assignElse;
            assignElse.operation = eop_assign;
            assignElse.operands = {temp, other.type, other.text};
            assignElse.span = other.span;
            assignElse.operandSpans = {{}, {}, other.span};
            emit(body, assignElse);

            size_t endIdx = body.size();
            body[ifIdx].operands[1] = std::to_string(thenStart);
            body[ifIdx].operands[2] = std::to_string(elseStart);
            body[jmpIdx].operands[0] = std::to_string(endIdx);

            ctx.types[temp] = (left.type == other.type) ? left.type : "";
            return {temp, (left.type == other.type) ? left.type : "", unionSpan(left.span, other.span)};
        }
        return left;
    }

// 解析 Unary
// parse unary
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Unary
    ExprResult parseUnary(std::vector<execute_t>& body, ParseContext& ctx){
        if (cur().type == tokenType_symbol &&
            (cur().value == "-" || cur().value == "+" || cur().value == "~")){
            token opToken = cur();
            pos++;
            ExprResult operand = parseUnary(body, ctx);
            span_t span = unionSpan(tokenSpan(opToken), operand.span);
            span.anchorStart = span.startCol;
            span.anchorEnd = span.endCol;
            return {markText(opToken.value, tokenSpan(opToken)) + ";" + operand.text,
                    operand.type, span};
        }
        if (isWord("not")){
            token opToken = cur();
            pos++;
            ExprResult operand = parseUnary(body, ctx);
            span_t span = unionSpan(tokenSpan(opToken), operand.span);
            span.anchorStart = span.startCol;
            span.anchorEnd = span.endCol;
            return {markText("not", tokenSpan(opToken)) + ";" + operand.text, "bool", span};
        }
        return parsePrimary(body, ctx);
    }

    std::string emitCall(std::vector<execute_t>& body, const std::string& callee,
                         const std::vector<std::string>& args, const span_t& callSpan,
                         const span_t& calleeSpan, const std::vector<span_t>& argSpans,
                         const std::vector<std::pair<std::string, std::string>>& kwargs = {},
                         const std::vector<std::string>& argsUnpack = {},
                         const std::vector<std::string>& kwargsUnpack = {}){
        execute_t ins;
        ins.operation = eop_call;
        ins.callId = newCallId();
        ins.kwargs = kwargs;
        ins.argsUnpack = argsUnpack;
        ins.kwargsUnpack = kwargsUnpack;
        ins.operands.push_back(callee);
        for (auto& arg : args){
            ins.operands.push_back(arg);
        }
        ins.span = callSpan;
        ins.operandSpans.push_back(calleeSpan);
        for (auto& span : argSpans){
            ins.operandSpans.push_back(span);
        }
        emit(body, ins);
        return "@fret" + std::to_string(ins.callId);
    }

// 查找 返回 类型
// find return type
// ᠬᠠᠯᠠᠬᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ ᠲᠥᠷᠥᠯ
    std::string findReturnType(const std::string& name) const {
        for (auto& function : script.functions){
            if (function.name == name && !function.returnType.empty()){
                return function.returnType;
            }
        }
        return "";
    }


    ExprResult parsePostfix(std::vector<execute_t>& body, ParseContext& ctx,
                            std::string text, std::string type, span_t atomSpan){
        atomSpan.parts.clear();
        while (!atEnd()){
            if (isSymbol("[")){
                token openTok = cur();
                pos++;

                std::string startText;
                if (!isSymbol(":") && !isSymbol("]")){
                    startText = parseExpression(body, ctx).text;
                }
                std::string indexText = startText;
                if (isSymbol(":")){
                    pos++;
                    std::string stopText;
                    if (!isSymbol(":") && !isSymbol("]")){
                        stopText = parseExpression(body, ctx).text;
                    }
                    std::string stepText;
                    bool hasStep = false;
                    if (isSymbol(":")){
                        pos++;
                        hasStep = true;
                        if (!isSymbol("]")){
                            stepText = parseExpression(body, ctx).text;
                        }
                    }
                    indexText = startText + ":" + stopText;
                    if (hasStep) indexText += ":" + stepText;
                }
                token closeTok = cur();
                expectSymbol("]");
                span_t part;
                part.line = openTok.line;
                part.startCol = openTok.col;
                part.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
                text += "[" + indexText + "]";
                atomSpan = unionSpan(atomSpan, part);
                atomSpan.parts.push_back({part.startCol, part.endCol});
                type = "";
                continue;
            }
            if (isSymbol(".")){
                pos++;
                token memberTok = cur();
                std::string member = expectWord();
                span_t part = tokenSpan(memberTok);
                text += "." + member;
                span_t next = atomSpan;
                if (part.valid() && next.valid() && part.line == next.line){
                    next.endCol = part.endCol;
                }

                next.anchorStart = next.startCol;
                next.anchorEnd = next.endCol;
                next.parts.push_back({part.startCol, part.endCol});
                atomSpan = next;
                type = "";
                continue;
            }
            if (isSymbol("(")){
                token openTok = cur();
                pos++;
                std::vector<std::string> args;
                std::vector<span_t> argSpans;
                std::vector<std::pair<std::string, std::string>> kwargs;
                std::vector<std::string> argsUnpack;
                std::vector<std::string> kwargsUnpack;
                bool genexpClosed = false;
                if (!isSymbol(")")){

// 试探首个实参: 后面紧跟 for 时整体按生成器表达式处理 f(x for x in y)
// probe the first argument: a trailing for means f(x for x in y)
// ᠡᠬᠦᠨ ᠦ argument ᠢ ᠰᠢᠯᠭᠠᠨ᠎ᠠ
                    size_t argSave = pos;
                    size_t argMark = body.size();
                    ExprResult probeArg = parseExpression(body, ctx);
                    (void)probeArg;
                    if (isWord("for")){
                        pos = argSave;
                        body.resize(argMark);
                        ExprResult ge = parseComprehension(body, ctx, openTok, "gen");
                        args.push_back(ge.text);
                        argSpans.push_back(ge.span);
                        genexpClosed = true;
                    } else {
                        pos = argSave;
                        body.resize(argMark);
                    }
                }
                if (!genexpClosed && !isSymbol(")")){
                    while (!atEnd()){
                        if (isSymbol("**")){
                            pos++;
                            ExprResult u = parseExpression(body, ctx);
                            kwargsUnpack.push_back(u.text);
                        } else if (isSymbol("*")){
                            pos++;
                            ExprResult u = parseExpression(body, ctx);
                            argsUnpack.push_back(u.text);
                        } else if (cur().type == tokenType_value &&
                            look(1).type == tokenType_symbol && look(1).value == "="){

                            std::string kwName = cur().value;
                            pos += 2;
                            ExprResult kw = parseExpression(body, ctx);
                            kwargs.push_back({kwName, kw.text});
                        } else {
                            ExprResult arg = parseExpression(body, ctx);
                            args.push_back(arg.text);
                            argSpans.push_back(arg.span);
                        }
                        if (!matchSymbol(",")) break;
                        if (isSymbol(")")) break;
                    }
                }
                token closeTok = genexpClosed ? tokens[pos - 1] : cur();
                if (!genexpClosed) expectSymbol(")");
                span_t callSpan = atomSpan;
                if (closeTok.endCol > 0 && callSpan.valid() && closeTok.line == callSpan.line){
                    callSpan.endCol = closeTok.endCol;
                }

                callSpan.anchorStart = openTok.col;
                callSpan.anchorEnd = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
                std::string callee = text;
                text = emitCall(body, callee, args, callSpan, atomSpan, argSpans, kwargs, argsUnpack, kwargsUnpack);
                span_t resultSpan = callSpan;
                resultSpan.parts.clear();
                atomSpan = resultSpan;
                type = findReturnType(callee);
                continue;
            }
            break;
        }
        return {markText(text, atomSpan), type, atomSpan};
    }




    ExprResult parseComprehension(std::vector<execute_t>& body, ParseContext& ctx,
                                   const token& openTok, const std::string& compType){

        size_t outSetupStart = body.size();
        ExprResult outExpr = parseExpression(body, ctx);
        size_t keyEnd = body.size();

        std::vector<execute_t> outSetup;
        {
            std::string plainOut = stripTrailingMarker(outExpr.text);
            if (body.size() > outSetupStart && isPlainCallName(plainOut) &&
                body.back().operation == eop_assign && body.back().operands.size() >= 3 &&
                body.back().operands[0] == plainOut){
                outSetup.assign(body.begin() + outSetupStart, body.end());
                body.erase(body.begin() + outSetupStart, body.end());
                keyEnd = body.size();
            }
        }
        bool isDict = (compType == "dict");

        std::vector<execute_t> keyIns(body.begin() + outSetupStart, body.begin() + keyEnd);
        body.erase(body.begin() + outSetupStart, body.begin() + keyEnd);
        std::string valueExprText;
        std::vector<execute_t> valueIns;
        if (isDict){
            expectSymbol(":");
            size_t valueStart = body.size();
            ExprResult valueExpr = parseExpression(body, ctx);
            valueExprText = valueExpr.text;
            valueIns.assign(body.begin() + valueStart, body.end());
            body.erase(body.begin() + valueStart, body.end());
        }


        matchWord("for");
        std::string varName = expectWord();
        matchWord("in");
        size_t iterStart = body.size();
        ExprResult iterable = parseExpression(body, ctx);
        std::vector<execute_t> iterIns(body.begin() + iterStart, body.end());
        body.erase(body.begin() + iterStart, body.end());


        std::string condText;
        span_t condSpan;
        std::vector<execute_t> condIns;
        if (isWord("if")){
            pos++;
            size_t condStart = body.size();
            ExprResult condExpr = parseExpression(body, ctx);
            condText = condExpr.text;
            condSpan = condExpr.span;
            condIns.assign(body.begin() + condStart, body.end());
            body.erase(body.begin() + condStart, body.end());
        }


        if (compType == "list") expectSymbol("]");
        else if (compType == "gen") expectSymbol(")");
        else expectSymbol("}");




        if (!outSetup.empty() && compType == "list"){
            std::string listTemp = "__comp_result_" + std::to_string(compCounter++);
            execute_t newListIns;
            newListIns.operation = eop_assign;
            newListIns.operands = {listTemp, "list"};
            newListIns.span = tokenSpan(openTok);
            emit(body, newListIns);
            ctx.types[listTemp] = "list";

            for (auto& ins : iterIns) emit(body, ins);
            size_t forIdx = body.size();
            execute_t forIns;
            forIns.operation = eop_for;
            forIns.operands = {varName, iterable.text, "0", "0"};
            forIns.span = iterable.span;
            forIns.operandSpans = {{}, iterable.span};
            emit(body, forIns);

            size_t bodyStart = body.size();
            size_t ifIdx = (size_t)(-1);
            if (!condText.empty()){
                for (auto& ins : condIns) emit(body, ins);
                ifIdx = body.size();
                execute_t ifIns;
                ifIns.operation = eop_if;
                ifIns.operands = {condText, "0", "0"};
                ifIns.span = condSpan;
                emit(body, ifIns);
            }
            for (auto& setupIns : outSetup){
                emit(body, setupIns);
            }
            for (auto& ins : keyIns) emit(body, ins);
            execute_t appendIns;
            appendIns.operation = eop_call;
            appendIns.callId = newCallId();
            appendIns.operands = {listTemp + ".append", outExpr.text};
            appendIns.span = outExpr.span;
            appendIns.operandSpans = {{}, outExpr.span};
            emit(body, appendIns);

            size_t jmpIdx = body.size();
            execute_t jmpBack;
            jmpBack.operation = eop_jmp;
            jmpBack.operands = {std::to_string(forIdx)};
            emit(body, jmpBack);
            size_t endIdx = body.size();

            body[forIdx].operands[2] = std::to_string(bodyStart);
            body[forIdx].operands[3] = std::to_string(endIdx);
            if (ifIdx != (size_t)(-1)){
                body[ifIdx].operands[1] = std::to_string(ifIdx + 1);
                body[ifIdx].operands[2] = std::to_string(jmpIdx);
            }

            if (ctx.types.find(varName) == ctx.types.end()){
                execute_t delIns;
                delIns.operation = eop_delvar;
                delIns.operands = {varName};
                emit(body, delIns);
            }

            ExprResult compResult;
            compResult.text = listTemp;

            compResult.type = "";
            compResult.span = tokenSpan(openTok);
            return compResult;
        }


        std::string anonName = "__comp_" + std::to_string(compCounter++);
        function_t genFn;
        genFn.name = anonName;
        genFn.isGenerator = true;








        for (auto& ins : iterIns) genFn.body.push_back(ins);
        size_t forIdx = genFn.body.size();
        execute_t forIns;
        forIns.operation = eop_for;
        forIns.operands = {varName, iterable.text, "0", "0"};
        forIns.span = iterable.span;
        forIns.operandSpans = {{}, iterable.span};
        genFn.body.push_back(forIns);

        size_t bodyStartIdx = genFn.body.size();
        size_t condIdx = static_cast<size_t>(-1);
        if (!condText.empty()){
            for (auto& ins : condIns) genFn.body.push_back(ins);
            condIdx = genFn.body.size();
            execute_t ifIns;
            ifIns.operation = eop_if;
            ifIns.operands = {condText, "0", "0"};
            ifIns.span = condSpan;
            genFn.body.push_back(ifIns);
        }


        for (auto& setupIns : outSetup) genFn.body.push_back(setupIns);
        for (auto& ins : keyIns) genFn.body.push_back(ins);
        for (auto& ins : valueIns) genFn.body.push_back(ins);

        execute_t yieldIns;
        yieldIns.operation = eop_yield;
        if (isDict){
            yieldIns.operands = {"", "(" + outExpr.text + "," + valueExprText + ")"};
        } else {
            yieldIns.operands = {"", outExpr.text};
        }
        genFn.body.push_back(yieldIns);

        size_t jmpIdx = genFn.body.size();
        execute_t jmpBack;
        jmpBack.operation = eop_jmp;
        jmpBack.operands = {std::to_string(forIdx)};
        genFn.body.push_back(jmpBack);

        size_t endIdx = genFn.body.size();
        genFn.body[forIdx].operands[2] = std::to_string(bodyStartIdx);
        genFn.body[forIdx].operands[3] = std::to_string(endIdx);
        if (condIdx != static_cast<size_t>(-1)){
            genFn.body[condIdx].operands[1] = std::to_string(condIdx + 1);
            genFn.body[condIdx].operands[2] = std::to_string(jmpIdx);
        }


        script.functions.push_back(std::move(genFn));


        execute_t fnIns;
        fnIns.operation = eop_function;
        fnIns.operands.push_back(anonName);
        fnIns.line = openTok.line;
        emit(body, fnIns);


        execute_t callFnIns;
        callFnIns.operation = eop_call;
        callFnIns.callId = newCallId();
        callFnIns.operands.push_back(anonName);
        callFnIns.line = openTok.line;
        emit(body, callFnIns);

// 生成器表达式不加 list/set/dict 包装, 直接得到生成器对象
// generator expressions are not wrapped; the generator object is the result
// generator ᠢᠯᠡᠳᠬᠡᠯ ᠢ ᠬᠤᠪᠢᠯᠭᠠᠰᠢ ᠦᠭᠡᠢ ᠪᠡᠷ ᠰᠢᠭᠦᠳᠡᠷᠢ ᠠᠪᠬᠤ
        if (compType == "gen"){
            ExprResult result;
            result.text = "@fret" + std::to_string(callFnIns.callId);
            result.type = "generator";
            result.span = tokenSpan(openTok);
            return result;
        }

        std::string wrapper = (compType == "dict") ? "dict" :
                              (compType == "set")  ? "set"  : "list";
        execute_t wrapIns;
        wrapIns.operation = eop_call;
        wrapIns.callId = newCallId();
        wrapIns.operands.push_back(wrapper);
        wrapIns.operands.push_back("@fret" + std::to_string(callFnIns.callId));
        wrapIns.line = openTok.line;
        span_t compSpan = tokenSpan(openTok);
        wrapIns.span = compSpan;
        wrapIns.hidePropagationCaret = true;
        emit(body, wrapIns);


        ExprResult result;
        result.text = "@fret" + std::to_string(wrapIns.callId);
        result.type = (compType == "list") ? "list" :
                      (compType == "dict") ? "dict" : "set";
        result.span = compSpan;
        return result;
    }

// 解析 Primary
// parse primary
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Primary
    ExprResult parsePrimary(std::vector<execute_t>& body, ParseContext& ctx){
        if (atEnd() || atLineEnd()){
            return {"", ""};
        }


        if (isWord("lambda")){
            token lamTok = cur();
            pos++;

            std::string anonName = "__lambda_" + std::to_string(lambdaCounter++);

            function_t lambdaFn;
            lambdaFn.name = anonName;


            int variadicArg = -1;
            int keywordArg = -1;
            std::vector<std::pair<int, std::string>> defaultArgs;
            bool firstParam = true;
            while (!atEnd()){
                if (isSymbol(":")) break;
                if (!firstParam){
                    if (matchSymbol(",")) firstParam = true;
                    else break;
                }
                int argIndex = static_cast<int>(lambdaFn.argNames.size());
                if (isSymbol("**")){
                    pos++;
                    if (!atEnd() && cur().type == tokenType_value){
                        keywordArg = static_cast<int>(lambdaFn.argNames.size());
                        lambdaFn.argNames.push_back(cur().value);
                        pos++;
                    }
                    firstParam = false;
                    continue;
                }
                if (isSymbol("*")){
                    pos++;
                    if (!atEnd() && cur().type == tokenType_value){
                        variadicArg = static_cast<int>(lambdaFn.argNames.size());
                        lambdaFn.argNames.push_back(cur().value);
                        pos++;
                    }
                    firstParam = false;
                    continue;
                }
                if (cur().type == tokenType_value && cur().value != ":"){
                    lambdaFn.argNames.push_back(cur().value);
                    pos++;

                    if (isSymbol("=")){
                        pos++;
                        ExprResult defExpr = parseExpression(lambdaFn.body, ctx);
                        defaultArgs.push_back({argIndex, defExpr.text});
                    }
                    firstParam = false;
                    continue;
                }
                break;
            }
            expectSymbol(":");

            lambdaFn.variadicArg = variadicArg;
            lambdaFn.keywordArg = keywordArg;
            lambdaFn.defaultArgs = defaultArgs;


            ExprResult bodyExpr = parseExpression(lambdaFn.body, ctx);
            execute_t retIns;
            retIns.operation = eop_return;
            retIns.operands.push_back(bodyExpr.text);
            retIns.span = bodyExpr.span;
            lambdaFn.body.push_back(retIns);

            lambdaFn.returnType = "";


            script.functions.push_back(std::move(lambdaFn));
            defDepth++;


            execute_t fnIns;
            fnIns.operation = eop_function;
            fnIns.operands.push_back(anonName);
            fnIns.line = lamTok.line;
            emit(body, fnIns);

            defDepth--;


            span_t lamSpan = tokenSpan(lamTok);

            ExprResult result = {anonName, "function", lamSpan};
            return parsePostfix(body, ctx, anonName, "", lamSpan);
        }

        if (cur().type == tokenType_number){
            token tk = cur();
            std::string value = tk.value;
            pos++;
            span_t span = tokenSpan(tk);
            return parsePostfix(body, ctx, value,
                                value.find('.') != std::string::npos ? "float" : "int", span);
        }

        if (cur().type == tokenType_string){
            token tk = cur();
            bool isBytes = tk.bytes;
            std::string joined = tk.value;
            pos++;

            while (cur().type == tokenType_string){
                isBytes = isBytes || cur().bytes;
                joined += cur().value;
                pos++;
            }
            std::string ref = (isBytes ? "b" : "") + stringRef(joined);
            span_t span = tokenSpan(tk);
            return parsePostfix(body, ctx, ref, isBytes ? "bytes" : "str", span);
        }

        if (cur().type == tokenType_fstring){
            token tk = cur();
            std::string content = tk.value;
            pos++;
            ExprResult inner = parseFString(body, ctx, content);
            span_t span = tokenSpan(tk);

            return parsePostfix(body, ctx, "(" + inner.text + ")", "str", span);
        }

        if (cur().type == tokenType_value){
            token nameTok = cur();
            std::string name = nameTok.value;
            if (name == "True" || name == "False"){
                pos++;
                span_t span = tokenSpan(nameTok);
                return parsePostfix(body, ctx, name, "bool", span);
            }
            if (name == "None"){
                pos++;
                span_t span = tokenSpan(nameTok);
                return parsePostfix(body, ctx, "None", "", span);
            }
            pos++;

            std::string type;
            span_t atomSpan = tokenSpan(nameTok);
            auto it = ctx.types.find(name);
            if (it != ctx.types.end()){
                type = it->second;
            }
            return parsePostfix(body, ctx, name, type, atomSpan);
        }

        if (matchSymbol("(")){
            token openTok = tokens[pos - 1];

            if (isSymbol(")")){
                token closeTok = cur();
                pos++;
                span_t span;
                span.line = openTok.line;
                span.startCol = openTok.col;
                span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
                span.anchorStart = span.startCol;
                span.anchorEnd = span.endCol;
                return parsePostfix(body, ctx, "()", "tuple", span);
            }

            size_t savePos = pos;
            size_t probeStart = body.size();
            ExprResult first = parseExpression(body, ctx);

// 圆括号内出现 for 即为生成器表达式 (x for x in items)
// 'for' inside parentheses means a generator expression (x for x in items)
// ᠬᠠᠯᠲᠤ ᠳᠣᠲᠣᠷ᠎ᠠ for ᠪᠠᠶᠢᠪᠠᠯ generator ᠢᠯᠡᠳᠬᠡᠯ ᠮᠥᠨ
            if (isWord("for")){
                pos = savePos;
                body.resize(probeStart);
                ExprResult compResult = parseComprehension(body, ctx, openTok, "gen");
                return compResult;
            }

            std::vector<std::string> elements;
            elements.push_back(first.text);
            bool isTuple = false;
            while (matchSymbol(",")){
                isTuple = true;
                if (isSymbol(")")) break;
                ExprResult element = parseExpression(body, ctx);
                elements.push_back(element.text);
            }
            token closeTok = cur();
            expectSymbol(")");
            span_t span;
            span.line = openTok.line;
            span.startCol = openTok.col;
            span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
            span.anchorStart = first.span.hasAnchor() ? first.span.anchorStart : span.startCol;
            span.anchorEnd = first.span.hasAnchor() ? first.span.anchorEnd : span.endCol;

            if (!isTuple){

                return parsePostfix(body, ctx, "(" + first.text + ")", first.type, span);
            }
            std::string joined;
            for (size_t i = 0; i < elements.size(); ++i){
                if (i > 0) joined += ",";
                joined += elements[i];
            }
            if (elements.size() == 1) joined += ",";
            return parsePostfix(body, ctx, "(" + joined + ")", "tuple", span);
        }

        if (isSymbol("[")){
            token openTok = cur();
            pos++;

            if (isSymbol("]")){
                token closeTok = cur();
                pos++;
                span_t span;
                span.line = openTok.line;
                span.startCol = openTok.col;
                span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
                span.anchorStart = span.startCol;
                span.anchorEnd = span.endCol;
                return parsePostfix(body, ctx, "[]", "list", span);
            }

            size_t savePos = pos;
            size_t probeStart = body.size();
            ExprResult firstExpr = parseExpression(body, ctx);
            if (isWord("for")){

                pos = savePos;
                body.resize(probeStart);
                ExprResult compResult = parseComprehension(body, ctx, openTok, "list");
                return compResult;
            }

            pos = savePos;
            body.resize(probeStart);
            std::vector<std::string> elements;
            while (!atEnd()){
                ExprResult element = parseExpression(body, ctx);
                elements.push_back(element.text);
                if (!matchSymbol(",")) break;
                if (isSymbol("]")) break;
            }
            token closeTok = cur();
            expectSymbol("]");
            std::string joined;
            for (size_t i = 0; i < elements.size(); ++i){
                if (i > 0) joined += ",";
                joined += elements[i];
            }
            span_t span;
            span.line = openTok.line;
            span.startCol = openTok.col;
            span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
            span.anchorStart = span.startCol;
            span.anchorEnd = span.endCol;
            return parsePostfix(body, ctx, "[" + joined + "]", "list", span);
        }

        if (isSymbol("{")){
            token openTok = cur();
            pos++;

            if (isSymbol("}")){
                token closeTok = cur();
                pos++;
                span_t span;
                span.line = openTok.line;
                span.startCol = openTok.col;
                span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
                span.anchorStart = span.startCol;
                span.anchorEnd = span.endCol;

// 解析 Postfix
// parse postfix
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Postfix
                return parsePostfix(body, ctx, "{}", "dict", span);
            }

            size_t savePos = pos;
            size_t probeStart = body.size();
            ExprResult firstExpr = parseExpression(body, ctx);
            if (isWord("for")){

                pos = savePos;
                body.resize(probeStart);
                ExprResult compResult = parseComprehension(body, ctx, openTok, "set");
                return compResult;
            }
            if (isSymbol(":")){

                pos++;
                ExprResult valueExpr = parseExpression(body, ctx);
                if (isWord("for")){

                    pos = savePos;
                    body.resize(probeStart);
                    ExprResult compResult = parseComprehension(body, ctx, openTok, "dict");
                    return compResult;
                }
            }

            pos = savePos;
            body.resize(probeStart);
            std::vector<std::string> pairs;
            while (!atEnd()){
                if (isSymbol("**")){
                    pos++;
                    ExprResult spread = parseExpression(body, ctx);
                    pairs.push_back("**" + spread.text);
                } else {
                    ExprResult key = parseExpression(body, ctx);
                    if (!matchSymbol(":")) break;
                    ExprResult value = parseExpression(body, ctx);
                    pairs.push_back(key.text + ":" + value.text);
                }
                if (!matchSymbol(",")) break;
                if (isSymbol("}")) break;
            }
            token closeTok = cur();
            expectSymbol("}");
            std::string joined;
            for (size_t i = 0; i < pairs.size(); ++i){
                if (i > 0) joined += ",";
                joined += pairs[i];
            }
            span_t span;
            span.line = openTok.line;
            span.startCol = openTok.col;
            span.endCol = closeTok.endCol > 0 ? closeTok.endCol : closeTok.col;
            span.anchorStart = span.startCol;
            span.anchorEnd = span.endCol;

// 解析 Postfix
// parse postfix
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Postfix
            return parsePostfix(body, ctx, "{" + joined + "}", "dict", span);
        }


        pos++;
        return {"", ""};
    }
};

}

// cpy 解析 Tokens
// cpy parse tokens
// cpy ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ Tokens
script_t minipyParseTokens(std::vector<token> tokens){
    script_t script;

    Parser parser(tokens, script);
    ParseContext globalContext;
    globalContext.isGlobal = true;
    parser.parseStatements(script.mainFunction.body, globalContext, nullptr);


    for (auto& klass : script.classes){
        for (auto& method : klass.methods){
            method.parentClass = &klass;
        }
    }

    return script;
}

}
