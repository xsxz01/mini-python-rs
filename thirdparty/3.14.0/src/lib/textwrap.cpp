#include "../cffi.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 读取字符串属性
// Read a string attribute from self
// ᠬᠢᠶ᠎ᠡ ᠡᠴᠡ ᠦᠰᠦᠭ ᠲᠠᠯᠠᠭ᠌
std::string attrText(cffi& a, const std::string& name, const std::string& fallback){
    value v = a.attr(name);
    if (v.type == "none") return fallback;
    return a.vm->toString(v);
}

// 读取整型属性
// Read an integer attribute from self
// ᠬᠢᠶ᠎ᠡ ᠡᠴᠡ ᠲᠣᠭ᠎ᠠ ᠲᠠᠯᠠᠭ᠌
long long attrInt(cffi& a, const std::string& name, long long fallback){
    value v = a.attr(name);
    if (v.type == "none") return fallback;
    return a.vm->toInt(v);
}

// 读取布尔属性
// Read a boolean attribute from self
// ᠬᠢᠶ᠎ᠡ ᠡᠴᠡ ᠦᠨ᠎ᠡ ᠲᠠᠯᠠᠭ᠌
bool attrBool(cffi& a, const std::string& name, bool fallback){
    value v = a.attr(name);
    if (v.type == "none") return fallback;
    return a.vm->truthy(v);
}

// 空白字符判断
// Whitespace test
// ᠬᠣᠭᠤᠰᠤᠨ ᠦᠰᠦᠭ
bool isSpaceChar(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

// 扩展制表符
// Expand tab characters
// ᠲᠠᠪᠯᠢᠭ᠎ᠠ ᠲᠡᠮᠳᠡᠭ ᠢ ᠬᠡᠮᠵᠢᠶ᠎ᠡᠯᠡᠬᠦ
std::string expandTabs(const std::string& text, int tabsize){
    if (tabsize <= 0) return text;
    std::string out;
    int column = 0;
    for (char c : text){
        if (c == '\t'){
            int spaces = tabsize - (column % tabsize);
            out += std::string(static_cast<size_t>(spaces), ' ');
            column += spaces;
        } else {
            out.push_back(c);
            if (c == '\n' || c == '\r') column = 0;
            else column++;
        }
    }
    return out;
}

// 切分为空白块与单词块
// Split text into whitespace and word chunks
// ᠪᠢᠴᠢᠭ ᠢ ᠬᠡᠰᠡᠭ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
std::vector<std::string> splitChunks(const std::string& text, bool breakHyphens){
    std::vector<std::string> chunks;
    size_t i = 0;
    while (i < text.size()){
        if (isSpaceChar(text[i])){
            size_t start = i;
            while (i < text.size() && isSpaceChar(text[i])) i++;
            std::string chunk = text.substr(start, i - start);
            std::string merged;
            for (char c : chunk) merged.push_back(c == '\r' ? ' ' : c);
            chunks.push_back(merged);
        } else {
            size_t start = i;
            while (i < text.size() && !isSpaceChar(text[i])){
                if (breakHyphens && text[i] == '-' && i > start){
                    i++;
                    break;
                }
                i++;
            }
            chunks.push_back(text.substr(start, i - start));
        }
    }
    return chunks;
}

// 断开超长单词
// Break a word that is longer than the line width
// ᠬᠡᠮᠵᠢᠶ᠎ᠡ ᠡᠴᠡ ᠳᠠᠪᠠᠭᠰᠠᠨ ᠦᠭᠡ ᠢ ᠬᠤᠪᠢᠶ᠎ᠠ
void handleLongWord(std::vector<std::string>& chunks, std::vector<std::string>& line,
                    long long& lineLength, long long width, bool breakLongWords,
                    bool breakHyphens){
    long long spaceLeft = width - lineLength;
    if (spaceLeft < 1) spaceLeft = 1;
    if (breakLongWords && !chunks.empty()){
        std::string chunk = chunks.back();
        size_t end = static_cast<size_t>(spaceLeft);
        if (end > chunk.size()) end = chunk.size();
        if (breakHyphens){
            size_t hyphen = chunk.rfind('-', end - 1);
            if (hyphen != std::string::npos && hyphen + 1 > 0){
                end = hyphen + 1;
            }
        }
        std::string head = chunk.substr(0, end);
        chunks.back() = chunk.substr(end);
        line.push_back(head);
        lineLength += static_cast<long long>(head.size());
        if (chunks.back().empty()) chunks.pop_back();
    } else if (line.empty() && !chunks.empty()){
        line.push_back(chunks.back());
        lineLength += static_cast<long long>(chunks.back().size());
        chunks.pop_back();
    }
}

// 核心换行算法
// Core wrapping algorithm
// ᠭᠣᠣᠯ ᠮᠥᠷᠢᠨ ᠠᠷᠭ᠎ᠠ
std::vector<std::string> wrapText(const std::string& text, long long width,
                                  const std::string& initialIndent,
                                  const std::string& subsequentIndent,
                                  bool expand, bool replaceWhitespace,
                                  bool breakLongWords, bool dropWhitespace,
                                  bool breakHyphens, long long tabsize,
                                  long long maxLines,
                                  const std::string& placeholder){
    std::vector<std::string> lines;
    if (width <= 0) width = 1;
    std::string body = text;
    if (expand) body = expandTabs(body, static_cast<int>(tabsize));
    if (replaceWhitespace){
        for (char& c : body){
            if (isSpaceChar(c)) c = ' ';
        }
    }
    std::vector<std::string> chunks = splitChunks(body, breakHyphens);
    while (!chunks.empty()){
        std::string indent = lines.empty() ? initialIndent : subsequentIndent;
        long long available = width - static_cast<long long>(indent.size());
        if (available < 1) available = 1;

        if (dropWhitespace && !lines.empty() && !chunks.empty()){
            std::string first = chunks.front();
            bool allSpace = true;
            for (char c : first) if (!isSpaceChar(c)) allSpace = false;
            if (allSpace) chunks.erase(chunks.begin());
        }
        std::vector<std::string> current;
        long long currentLength = 0;
        while (!chunks.empty()){
            long long chunkLength = static_cast<long long>(chunks.front().size());
            if (currentLength + chunkLength <= available){
                current.push_back(chunks.front());
                currentLength += chunkLength;
                chunks.erase(chunks.begin());
            } else {
                break;
            }
        }
        if (!chunks.empty() && static_cast<long long>(chunks.front().size()) > available){
            std::vector<std::string> reversed;
            reversed.assign(chunks.rbegin(), chunks.rend());
            handleLongWord(reversed, current, currentLength, available,
                           breakLongWords, breakHyphens);
            chunks.assign(reversed.rbegin(), reversed.rend());
        }
        if (dropWhitespace && !current.empty()){
            std::string last = current.back();
            bool allSpace = true;
            for (char c : last) if (!isSpaceChar(c)) allSpace = false;
            if (allSpace){
                currentLength -= static_cast<long long>(last.size());
                current.pop_back();
            }
        }
        if (!current.empty()){
            bool allowLine = true;
            if (maxLines > 0 && static_cast<long long>(lines.size()) + 1 >= maxLines){
                bool trailingOnly = false;
                if (!chunks.empty() && dropWhitespace && chunks.size() == 1){
                    trailingOnly = true;
                    for (char c : chunks.front()) if (!isSpaceChar(c)) trailingOnly = false;
                    if (trailingOnly) trailingOnly = currentLength <= available;
                }
                if (!(chunks.empty() || trailingOnly)) allowLine = false;
            }
            if (allowLine){
                std::string joined;
                for (auto& part : current) joined += part;
                lines.push_back(indent + joined);
            } else {
                while (!current.empty()){
                    std::string last = current.back();
                    bool allSpace = true;
                    for (char c : last) if (!isSpaceChar(c)) allSpace = false;
                    if (!allSpace &&
                        currentLength + static_cast<long long>(placeholder.size()) <= available){
                        current.push_back(placeholder);
                        std::string joined;
                        for (auto& part : current) joined += part;
                        lines.push_back(indent + joined);
                        break;
                    }
                    currentLength -= static_cast<long long>(last.size());
                    current.pop_back();
                }
                if (current.empty()){
                    if (!lines.empty()){
                        std::string previous = lines.back();
                        while (!previous.empty() && isSpaceChar(previous.back())) previous.pop_back();
                        if (static_cast<long long>(previous.size() +
                                                   placeholder.size()) <= width){
                            lines.back() = previous + placeholder;
                            break;
                        }
                    }
                    std::string trimmed = placeholder;
                    while (!trimmed.empty() && isSpaceChar(trimmed.front())) trimmed.erase(trimmed.begin());
                    lines.push_back(indent + trimmed);
                }
                break;
            }
        }
    }
    return lines;
}

// 从属性读取配置并换行
// Read configuration from attributes and wrap
// ᠲᠠᠯᠠᠭ᠌ ᠡᠴᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠦᠨᠰᠢᠵᠦ ᠮᠥᠷᠢᠯᠡᠬᠦ
std::vector<std::string> wrapFromSelf(cffi& a, const std::string& text){
    long long width = attrInt(a, "width", 70);
    std::string initial = attrText(a, "initial_indent", "");
    std::string subsequent = attrText(a, "subsequent_indent", "");
    bool expand = attrBool(a, "expand_tabs", true);
    bool replaceWhitespace = attrBool(a, "replace_whitespace", true);
    bool breakLongWords = attrBool(a, "break_long_words", true);
    bool dropWhitespace = attrBool(a, "drop_whitespace", true);
    bool breakHyphens = attrBool(a, "break_on_hyphens", true);
    long long tabsize = attrInt(a, "tabsize", 8);
    long long maxLines = attrInt(a, "max_lines", 0);
    value placeholderValue = a.attr("placeholder");
    std::string placeholder = placeholderValue.type == "none" ? " [...]"
                                                              : a.vm->toString(placeholderValue);
    return wrapText(text, width, initial, subsequent, expand, replaceWhitespace,
                    breakLongWords, dropWhitespace, breakHyphens, tabsize,
                    maxLines, placeholder);
}
}

// TextWrapper 构造器
// TextWrapper constructor
// TextWrapper ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnWrapperConstructor(cffi& a){
    value obj = a.mkobject(nullptr);
    long long width = (a.argc() >= 1) ? a.arg_int(0) : 70;
    if (a.has_kwarg("width")) width = a.kwarg_int("width", width);
    a.setattr(obj, "width", a.mkint(width));
    a.setattr(obj, "initial_indent", a.kwarg("initial_indent").type != "none"
                                        ? a.kwarg("initial_indent") : a.mkstr(""));
    a.setattr(obj, "subsequent_indent", a.kwarg("subsequent_indent").type != "none"
                                           ? a.kwarg("subsequent_indent") : a.mkstr(""));
    a.setattr(obj, "expand_tabs", a.kwarg_bool("expand_tabs", true) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "replace_whitespace", a.kwarg_bool("replace_whitespace", true) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "fix_sentence_endings", a.kwarg_bool("fix_sentence_endings", false) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "break_long_words", a.kwarg_bool("break_long_words", true) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "drop_whitespace", a.kwarg_bool("drop_whitespace", true) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "break_on_hyphens", a.kwarg_bool("break_on_hyphens", true) ? a.mkbool(true) : a.mkbool(false));
    a.setattr(obj, "tabsize", a.mkint(a.kwarg_int("tabsize", 8)));
    a.setattr(obj, "max_lines", a.kwarg("max_lines").type != "none"
                                    ? a.kwarg("max_lines") : a.mkint(0));
    a.setattr(obj, "placeholder", a.kwarg("placeholder").type != "none"
                                      ? a.kwarg("placeholder") : a.mkstr(" [...]"));
    return obj;
}

// TextWrapper.wrap
// TextWrapper.wrap
// TextWrapper.wrap
static value fnWrapperWrap(cffi& a){
    auto lines = wrapFromSelf(a, a.arg_str(0));
    std::vector<value> items;
    for (auto& line : lines) items.push_back(a.mkstr(line));
    return a.mklist(items);
}

// TextWrapper.fill
// TextWrapper.fill
// TextWrapper.fill
static value fnWrapperFill(cffi& a){
    auto lines = wrapFromSelf(a, a.arg_str(0));
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i){
        if (i) out.push_back('\n');
        out += lines[i];
    }
    return a.mkstr(out);
}

// 使用临时配置换行
// Wrap using temporary configuration
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠠᠰᠢᠭᠯᠠᠵᠤ ᠮᠥᠷᠢᠯᠡᠬᠦ
static std::vector<std::string> wrapWithArgs(cffi& a, const std::string& text, long long width){
    std::string initial = a.kwarg("initial_indent").type != "none" ? a.kwarg_str("initial_indent", "") : "";
    std::string subsequent = a.kwarg("subsequent_indent").type != "none" ? a.kwarg_str("subsequent_indent", "") : "";
    return wrapText(text, width, initial, subsequent,
                    a.kwarg_bool("expand_tabs", true),
                    a.kwarg_bool("replace_whitespace", true),
                    a.kwarg_bool("break_long_words", true),
                    a.kwarg_bool("drop_whitespace", true),
                    a.kwarg_bool("break_on_hyphens", true),
                    a.kwarg_int("tabsize", 8),
                    a.kwarg_int("max_lines", 0),
                    a.kwarg("placeholder").type != "none" ? a.kwarg_str("placeholder", " [...]") : " [...]");
}

// fn Wrap
// fn wrap
// fn Wrap
static value fnWrap(cffi& a){
    long long width = (a.argc() > 1) ? a.arg_int(1) : a.kwarg_int("width", 70);
    auto lines = wrapWithArgs(a, a.arg_str(0), width);
    std::vector<value> items;
    for (auto& line : lines) items.push_back(a.mkstr(line));
    return a.mklist(items);
}

// fn Fill
// fn fill
// fn Fill
static value fnFill(cffi& a){
    long long width = (a.argc() > 1) ? a.arg_int(1) : a.kwarg_int("width", 70);
    auto lines = wrapWithArgs(a, a.arg_str(0), width);
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i){
        if (i) out.push_back('\n');
        out += lines[i];
    }
    return a.mkstr(out);
}

// fn Shorten
// fn shorten
// fn Shorten
static value fnShorten(cffi& a){
    std::string text = a.arg_str(0);
    long long width = a.arg_int(1);
    if (static_cast<long long>(text.size()) <= width) return a.mkstr(text);
    std::string placeholder = a.kwarg("placeholder").type != "none"
                                  ? a.kwarg_str("placeholder", " [...]") : " [...]";
    auto lines = wrapText(text, width,
                          a.kwarg_str("initial_indent", ""),
                          a.kwarg_str("subsequent_indent", ""),
                          a.kwarg_bool("expand_tabs", true),
                          a.kwarg_bool("replace_whitespace", true),
                          a.kwarg_bool("break_long_words", true),
                          a.kwarg_bool("drop_whitespace", true),
                          a.kwarg_bool("break_on_hyphens", true),
                          a.kwarg_int("tabsize", 8), 1, placeholder);
    return a.mkstr(lines.empty() ? text : lines.front());
}

// fn Dedent
// fn dedent
// fn Dedent
static value fnDedent(cffi& a){
    std::string text = a.arg_str(0);
    std::vector<std::string> lines;
    size_t begin = 0;
    while (begin <= text.size()){
        size_t end = text.find('\n', begin);
        if (end == std::string::npos) end = text.size();
        lines.push_back(text.substr(begin, end - begin));
        if (end >= text.size()) break;
        begin = end + 1;
    }
    int margin = -1;
    for (auto& line : lines){
        bool blank = true;
        for (char c : line) if (!isSpaceChar(c)) blank = false;
        if (blank) continue;
        std::string expanded = expandTabs(line, 8);
        int count = 0;
        while (count < static_cast<int>(expanded.size()) &&
               expanded[static_cast<size_t>(count)] == ' ') count++;
        if (margin < 0 || count < margin) margin = count;
    }
    if (margin < 0) margin = 0;
    for (auto& line : lines){
        bool blank = true;
        for (char c : line) if (!isSpaceChar(c)) blank = false;
        if (blank){
            line.clear();
            continue;
        }
        std::string expanded = expandTabs(line, 8);
        if (static_cast<int>(expanded.size()) >= margin &&
            expanded.compare(0, static_cast<size_t>(margin), std::string(static_cast<size_t>(margin), ' ')) == 0){
            line = expanded.substr(static_cast<size_t>(margin));
        }
    }
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i){
        if (i) out.push_back('\n');
        out += lines[i];
    }
    return a.mkstr(out);
}

// fn Indent
// fn indent
// fn Indent
static value fnIndent(cffi& a){
    std::string text = a.arg_str(0);
    std::string prefix = a.arg_str(1);
    value predicate = (a.argc() > 2) ? a.arg(2) : a.none();
    std::vector<std::string> lines;
    size_t begin = 0;
    while (begin <= text.size()){
        size_t end = text.find('\n', begin);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(begin, end - begin);
        if (end < text.size()) line.push_back('\n');
        lines.push_back(line);
        if (end >= text.size()) break;
        begin = end + 1;
    }
    for (auto& line : lines){
        std::string content = line;
        if (!content.empty() && content.back() == '\n') content.pop_back();
        bool use;
        if (predicate.type == "none"){
            use = !content.empty();
            if (use){
                bool blank = true;
                for (char c : content) if (!isSpaceChar(c)) blank = false;
                use = !blank;
            }
        } else {
            std::vector<value> args = {a.mkstr(content)};
            value result = a.vm->callValue(predicate, args);
            if (a.vm->hasError()) return a.none();
            use = a.vm->truthy(result);
        }
        if (use) line = prefix + line;
    }
    std::string out;
    for (auto& line : lines) out += line;
    return a.mkstr(out);
}

// cffi textwrap lib
// cffi textwrap lib
// cffi textwrap lib
ffilib cffi_textwrap_lib(){
    ffi_class wrapperClass;
    wrapperClass.name = "TextWrapper";
    wrapperClass.constructor = (void*)fnWrapperConstructor;
    wrapperClass.methods = {
        {"wrap", (void*)fnWrapperWrap},
        {"fill", (void*)fnWrapperFill}
    };
    return cffi_lib(
        "textwrap",
        {
            {"wrap", (void*)fnWrap},
            {"fill", (void*)fnFill},
            {"shorten", (void*)fnShorten},
            {"dedent", (void*)fnDedent},
            {"indent", (void*)fnIndent}
        },
        {},
        {
            {"TextWrapper", wrapperClass}
        });
}

}
