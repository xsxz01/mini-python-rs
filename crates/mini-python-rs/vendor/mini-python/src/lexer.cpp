#include "lexer.hpp"

#include <cctype>

namespace {

// 是否 Symbol Char
// is symbol char
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ Symbol Char
bool isSymbolChar(char c){
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
           c == ',' || c == ':' || c == '.' || c == ';' ||
           c == '<' || c == '>' || c == '!' || c == '&' || c == '|' ||
           c == '^' || c == '%' || c == '~' || c == '@';
}

// 追加 Utf8
// append utf8
// ᠲᠡᠨᠳᠡᠭᠡ ᠨᠡᠮᠡᠵᠡ Utf8
void appendUtf8(std::string& out, unsigned int cp){
    if (cp < 0x80){
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800){
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000){
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// decode Escape
// decode escape
// decode Escape
bool decodeEscape(const std::string& code, size_t& pos, std::string& body, bool isBytes = false){
    size_t n = code.size();
    if (pos + 1 >= n){ body += '\\'; pos++; return true; }
    char esc = code[pos + 1];
    auto hexVal = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    switch (esc){
        case 'n': body += '\n'; pos += 2; return true;
        case 't': body += '\t'; pos += 2; return true;
        case 'r': body += '\r'; pos += 2; return true;
        case '0': body += '\0'; pos += 2; return true;
        case 'a': body += '\a'; pos += 2; return true;
        case 'b': body += '\b'; pos += 2; return true;
        case 'f': body += '\f'; pos += 2; return true;
        case 'v': body += '\v'; pos += 2; return true;
        case '\\': body += '\\'; pos += 2; return true;
        case '\'': body += '\''; pos += 2; return true;
        case '"': body += '"'; pos += 2; return true;
        case '\n': pos += 2; return true;
        case 'x': {
            if (pos + 3 < n){
                int a = hexVal(code[pos + 2]), b = hexVal(code[pos + 3]);
                if (a >= 0 && b >= 0){
                    if (isBytes) body.push_back(static_cast<char>(a * 16 + b));
                    else appendUtf8(body, static_cast<unsigned int>(a * 16 + b));
                    pos += 4;
                    return true;
                }
            }
            body += 'x';
            pos += 2;
            return true;
        }
        case 'u': {
            if (!isBytes && pos + 5 < n){
                unsigned int cp = 0;
                bool ok = true;
                for (int i = 2; i <= 5; ++i){
                    int v = hexVal(code[pos + static_cast<size_t>(i)]);
                    if (v < 0){ ok = false; break; }
                    cp = cp * 16 + static_cast<unsigned int>(v);
                }
                if (ok){ appendUtf8(body, cp); pos += 6; return true; }
            }
            body += 'u';
            pos += 2;
            return true;
        }
        case 'U': {
            if (!isBytes && pos + 9 < n){
                unsigned int cp = 0;
                bool ok = true;
                for (int i = 2; i <= 9; ++i){
                    int v = hexVal(code[pos + static_cast<size_t>(i)]);
                    if (v < 0){ ok = false; break; }
                    cp = cp * 16 + static_cast<unsigned int>(v);
                }
                if (ok){ appendUtf8(body, cp); pos += 10; return true; }
            }
            body += 'U';
            pos += 2;
            return true;
        }
        default:
            if (esc >= '1' && esc <= '7'){

                unsigned int cp = 0;
                size_t k = 1;
                while (k <= 3 && pos + k < n && code[pos + k] >= '0' && code[pos + k] <= '7'){
                    cp = cp * 8 + static_cast<unsigned int>(code[pos + k] - '0');
                    k++;
                }
                if (isBytes) body.push_back(static_cast<char>(cp & 0xFF));
                else appendUtf8(body, cp);
                pos += k;
                return true;
            }
            body += esc;
            pos += 2;
            return true;
    }
}

void flushValue(std::vector<minipy::token>& tokens, std::string& value,
                int startCol, int endCol, int line){
    if (value.empty()) return;
    minipy::token tk;
    tk.type = tokenType_value;
    tk.value = value;
    tk.line = line;
    tk.col = startCol;
    tk.endCol = endCol;
    tokens.push_back(tk);
    value.clear();
}


// 是否 字符串 Prefix Chars
// is string prefix chars
// ᠬᠡᠷᠡᠴᠡᠯᠲᠡᠢ ᠦᠰᠦᠭ Prefix Chars
bool isStringPrefixChars(const std::string& s, bool& outF, bool& outRaw, bool& outBytes){
    bool hasF = false, hasR = false, hasB = false, hasU = false;
    for (char c : s){
        if (c == 'f' || c == 'F'){ if (hasF) return false; hasF = true; }
        else if (c == 'r' || c == 'R'){ if (hasR) return false; hasR = true; }
        else if (c == 'b' || c == 'B'){ if (hasB || hasU) return false; hasB = true; }
        else if (c == 'u' || c == 'U'){ if (hasU || hasB) return false; hasU = true; }
        else return false;
    }
    outF = hasF;
    outRaw = hasR;
    outBytes = hasB;
    return true;
}
}

namespace minipy{

// cpy 获取 Tokens
// cpy get tokens
// cpy ᠬᠠᠯᠠᠬᠤ Tokens
std::vector<token> minipyGetTokens(std::string code){
    std::vector<token> tokens;

    if (code.size() >= 3 &&
        static_cast<unsigned char>(code[0]) == 0xEF &&
        static_cast<unsigned char>(code[1]) == 0xBB &&
        static_cast<unsigned char>(code[2]) == 0xBF){
        code.erase(0, 3);
    }
    std::vector<int> indentStack;
    indentStack.push_back(0);
    int bracketDepth = 0;
    size_t pos = 0;
    int lineNumber = 1;
    int lastLineNumber = 1;
    int curCol = 1;

    auto advanceLine = [&](){

        pos++;
        lineNumber++;
        lastLineNumber = lineNumber;
        curCol = 1;
    };

    auto skipWhitespace = [&](int& outIndent){
        outIndent = 0;
        while (pos < code.size()){
            char c = code[pos];
            if (c == ' '){
                outIndent += 1;
                curCol++;
                pos++;
            } else if (c == '\t'){
                outIndent += 4;
                curCol++;
                pos++;
            } else if (c == '\r'){

                curCol++;
                pos++;
            } else {
                break;
            }
        }
    };

    auto handleLineStart = [&](){
        int indent = 0;
        skipWhitespace(indent);
        if (pos >= code.size()) return false;

        if (code[pos] == '\n'){
            advanceLine();
            return false;
        }

        if (code[pos] == '#'){
            while (pos < code.size() && code[pos] != '\n'){
                curCol++;
                pos++;
            }
            if (pos < code.size()) advanceLine();
            return false;
        }
        bool continuation = (bracketDepth > 0);
        if (!continuation){
            if (indent > indentStack.back()){
                indentStack.push_back(indent);
                tokens.push_back({tokenType_indent, "", lineNumber, 0, 0});
            } else {
                while (indent < indentStack.back() && indentStack.size() > 1){
                    indentStack.pop_back();
                    tokens.push_back({tokenType_dedent, "", lineNumber, 0, 0});
                }
            }
        }
        return true;
    };

    if (pos < code.size()) handleLineStart();
    std::string value;
    int valueStartCol = curCol;

    while (pos < code.size()){
        char c = code[pos];

        if (c == '\n'){
            flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            bool continuation = (bracketDepth > 0);


            size_t lineStart = pos;
            while (lineStart > 0 && code[lineStart - 1] != '\n') lineStart--;
            bool backslashCont = false;
            for (size_t k = pos; k > lineStart; ){
                k--;
                char lc = code[k];
                if (lc == ' ' || lc == '\t' || lc == '\r') continue;
                if (lc == '\\') backslashCont = true;
                break;
            }
            if (bracketDepth == 0 && !backslashCont){
                tokens.push_back({tokenType_newline, "", lineNumber, 0, 0});
            }
            advanceLine();

            while (pos < code.size() && !handleLineStart()){

            }
            if (pos < code.size()) valueStartCol = curCol;
            continue;
        }

        if (c == '\r'){
            curCol++;
            pos++;
            continue;
        }

        if (c == ' ' || c == '\t'){
            flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            curCol++;
            pos++;

            while (pos < code.size() && (code[pos] == ' ' || code[pos] == '\t')){
                curCol++;
                pos++;
            }
            if (pos < code.size()) valueStartCol = curCol;
            continue;
        }

        if (c == '#'){
            flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            while (pos < code.size() && code[pos] != '\n'){
                curCol++;
                pos++;
            }
            if (pos < code.size()) valueStartCol = 0;
            continue;
        }


        if ((c == '"' || c == '\'') && pos + 2 < code.size() &&
            code[pos + 1] == c && code[pos + 2] == c){
            bool fstring = false, rawStr = false, bytesStr = false;
            if (!isStringPrefixChars(value, fstring, rawStr, bytesStr)){

                flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            } else {

                value.clear();
            }

            int quoteCol = curCol;
            int quoteLine = lineNumber;
            char quoteChar = c;
            bool isTripleFString = fstring;

            pos += 3;
            curCol += 3;

            std::string body;
            int endLine = quoteLine;
            int endCol = 0;
            bool closed = false;
            while (pos < code.size()){
                if (code[pos] == quoteChar && pos + 2 < code.size() &&
                    code[pos + 1] == quoteChar && code[pos + 2] == quoteChar){
                    endCol = curCol + 3;
                    closed = true;
                    pos += 3;
                    curCol += 3;
                    break;
                }

                if (!rawStr && code[pos] == '\\' && pos + 1 < code.size()){
                    size_t before = pos;
                    decodeEscape(code, pos, body, bytesStr);
                    curCol += static_cast<int>(pos - before);
                    continue;
                }

                if (code[pos] == '\n'){
                    body += '\n';
                    endLine = lineNumber + 1;
                    advanceLine();
                    continue;
                }
                if (code[pos] == '\r'){

                    pos++;
                    curCol++;
                    if (pos < code.size() && code[pos] == '\n'){
                        body += '\n';
                        endLine = lineNumber + 1;
                        advanceLine();
                    }
                    continue;
                }
                body += code[pos];
                pos++;
                curCol++;
            }
            if (!closed){
                endLine = lineNumber;
                endCol = quoteCol + 3;
            }

            token tk;
            tk.type = isTripleFString ? tokenType_fstring : tokenType_string;
            tk.value = body;
            tk.line = quoteLine;
            tk.col = quoteCol;
            tk.endCol = endCol > 0 ? endCol : quoteCol + 3;
            tk.bytes = bytesStr;
            tokens.push_back(tk);


            valueStartCol = curCol;
            continue;
        }

        if (c == '\'' || c == '"'){
            bool fstring = false, rawStr = false, bytesStr = false;
            if (!isStringPrefixChars(value, fstring, rawStr, bytesStr)){
                flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            } else {
                value.clear();
            }
            int quoteCol = curCol;
            int quoteLine = lineNumber;
            char quoteChar = c;
            bool isFStr = fstring;
            pos++;
            curCol++;
            std::string body;
            while (pos < code.size()){
                char cc = code[pos];
                if (!rawStr && cc == '\\' && pos + 1 < code.size()){
                    size_t before = pos;
                    decodeEscape(code, pos, body, bytesStr);
                    curCol += static_cast<int>(pos - before);
                    continue;
                }
                if (cc == quoteChar){
                    pos++;
                    curCol++;
                    break;
                }
                if (cc == '\n'){
                    break;
                }
                body += cc;
                pos++;
                curCol++;
            }
            token tk;
            tk.type = isFStr ? tokenType_fstring : tokenType_string;
            tk.value = body;
            tk.line = quoteLine;
            tk.col = quoteCol;
            tk.endCol = curCol - 1;
            tk.bytes = bytesStr;
            tokens.push_back(tk);
            valueStartCol = curCol;
            continue;
        }

        if ((c == 'e' || c == 'E') && !value.empty()){
            bool numeric = true;
            int dotCount = 0;
            for (unsigned char cc : value){
                if (std::isdigit(cc)) continue;
                if (cc == '.' && dotCount == 0){ dotCount++; continue; }
                numeric = false; break;
            }
            if (numeric){
                value += c;
                pos++; curCol++;
                if (pos < code.size() && (code[pos] == '+' || code[pos] == '-')){
                    value += code[pos]; pos++; curCol++;
                }
                if (pos < code.size() && std::isdigit(static_cast<unsigned char>(code[pos]))){
                    while (pos < code.size() && std::isdigit(static_cast<unsigned char>(code[pos]))){
                        value += code[pos]; pos++; curCol++;
                    }
                    continue;
                }
                value.pop_back(); pos--; curCol--;
            }
        }

        else if ((c == 'x' || c == 'X') && !value.empty() && value == "0"){
            std::string hex = value + c;
            size_t p2 = pos + 1;
            while (p2 < code.size()){
                char h = code[p2];
                if (std::isxdigit(static_cast<unsigned char>(h))){ hex += h; p2++; }
                else break;
            }
            if (hex.size() > 2){
                value = hex;
                pos = p2; curCol = pos;
                continue;
            }
        }

        if (isSymbolChar(c)){
            flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);
            int symCol = curCol;
            pos++;
            curCol++;
            token sym;
            sym.type = tokenType_symbol;
            sym.value = std::string(1, c);
            sym.line = lineNumber;
            sym.col = symCol;
            sym.endCol = symCol;
            tokens.push_back(sym);
            if (c == '(' || c == '[' || c == '{') bracketDepth++;
            else if (c == ')' || c == ']' || c == '}'){
                if (bracketDepth > 0) bracketDepth--;
            }
            valueStartCol = curCol;
            continue;
        }

        if (value.empty()) valueStartCol = curCol;
        value += c;
        pos++;
        curCol++;
    }
    flushValue(tokens, value, valueStartCol, curCol - 1, lineNumber);

    while (indentStack.size() > 1){
        indentStack.pop_back();
        token tk;
        tk.type = tokenType_dedent;
        tk.line = lastLineNumber;
        tokens.push_back(tk);
    }

    for (size_t i = 0; i + 2 < tokens.size(); ){
        auto& t1 = tokens[i];
        auto& t2 = tokens[i + 1];
        auto& t3 = tokens[i + 2];
        if (t1.type == tokenType_symbol && t2.type == tokenType_symbol && t3.type == tokenType_symbol){
            std::string op3 = t1.value + t2.value + t3.value;
            if (op3 == "//=" || op3 == "**=" || op3 == "<<=" || op3 == ">>="){
                token merged;
                merged.type = tokenType_symbol;
                merged.value = op3;
                merged.line = t1.line;
                merged.col = t1.col;
                merged.endCol = t3.endCol;
                tokens.erase(tokens.begin() + i, tokens.begin() + i + 3);
                tokens.insert(tokens.begin() + i, merged);
                continue;
            }
        }
        i++;
    }

    for (size_t i = 0; i + 1 < tokens.size(); ){
        auto& t1 = tokens[i];
        auto& t2 = tokens[i + 1];
        if (t1.type == tokenType_symbol && t2.type == tokenType_symbol){
            std::string op2 = t1.value + t2.value;
            if (op2 == "+=" || op2 == "-=" || op2 == "*=" || op2 == "/=" ||
                op2 == "==" || op2 == ">=" || op2 == "<=" || op2 == "!=" ||
                op2 == "&=" || op2 == "|=" || op2 == "^=" || op2 == "%=" ||
                op2 == "//" || op2 == "**" || op2 == "->" ||
                op2 == ":=" || op2 == "<<" || op2 == ">>"){
                token merged;
                merged.type = tokenType_symbol;
                merged.value = op2;
                merged.line = t1.line;
                merged.col = t1.col;
                merged.endCol = t2.endCol;
                tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
                tokens.insert(tokens.begin() + i, merged);
                continue;
            }
        }
        i++;
    }

    for (auto& tk : tokens){
        if (tk.type != tokenType_value) continue;
        bool hasDigit = false, hasExp = false, hasDot = false, hasSign = false;
        bool numeric = true;
        for (size_t i = 0; i < tk.value.size(); ++i){
            unsigned char cc = static_cast<unsigned char>(tk.value[i]);
            if (std::isdigit(cc)){ hasDigit = true; continue; }
            if (cc == '.'){ if (hasDot){ numeric = false; break; } hasDot = true; continue; }
            if (cc == 'e' || cc == 'E'){
                if (hasExp || !hasDigit){ numeric = false; break; }
                hasExp = true; continue;
            }
            if ((cc == '+' || cc == '-') && hasExp && i + 1 < tk.value.size()){
                if (hasSign){ numeric = false; break; }
                hasSign = true; continue;
            }
            numeric = false; break;
        }
        if (numeric && hasDigit) tk.type = tokenType_number;
    }

    for (size_t i = 0; i + 2 < tokens.size(); ){
        auto& t1 = tokens[i];
        auto& t2 = tokens[i + 1];
        auto& t3 = tokens[i + 2];
        if (t1.type == tokenType_number && t2.type == tokenType_symbol && t2.value == "." &&
            t3.type == tokenType_number){
            token merged;
            merged.type = tokenType_number;
            merged.value = t1.value + "." + t3.value;
            merged.line = t1.line;
            merged.col = t1.col;
            merged.endCol = t3.endCol;
            tokens.erase(tokens.begin() + i, tokens.begin() + i + 3);
            tokens.insert(tokens.begin() + i, merged);
            continue;
        }
        i++;
    }
    return tokens;
}

// cpy 打印 Tokens
// cpy print tokens
// cpy ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ Tokens
void minipyPrintTokens(std::vector<token> tokens){
    for (auto& t : tokens){
        std::string typeStr;
        switch (t.type){
            case tokenType_string:  typeStr = "String";  break;
            case tokenType_value:   typeStr = "Value";   break;
            case tokenType_symbol:  typeStr = "Symbol";  break;
            case tokenType_number:  typeStr = "Number";  break;
            case tokenType_newline: typeStr = "Newline"; break;
            case tokenType_indent:  typeStr = "Indent";  break;
            case tokenType_dedent:  typeStr = "Dedent";  break;
            case tokenType_fstring: typeStr = "FString"; break;
            default: typeStr = "Unknown";
        }
        std::cout << "Line " << t.line << " Col " << t.col << "-" << t.endCol
                  << " TokenType: " << typeStr
                  << ", TokenValue: \"" << t.value << "\"" << std::endl;
    }
}

}
