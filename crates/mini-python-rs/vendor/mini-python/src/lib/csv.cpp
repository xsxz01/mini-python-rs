#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace minipy {

namespace {

// CSV 引用常量
// CSV quoting constants
// CSV ᠬᠠᠶᠢᠯᠤᠯ ᠲᠣᠭ᠎ᠠ
enum CsvQuoting{
    quoteMinimal = 0,
    quoteAll = 1,
    quoteNonNumeric = 2,
    quoteNone = 3,
    quoteStrings = 4,
    quoteNotNull = 5
};

// 方言定义
// dialect definition
// ᠠᠶᠠᠯᠭᠤ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
struct Dialect{
    std::string delimiter = ",";
    char quotechar = '"';
    bool hasQuotechar = true;
    char escapechar = 0;
    bool hasEscapechar = false;
    bool doublequote = true;
    bool skipinitialspace = false;
    std::string lineterminator = "\r\n";
    long long quoting = quoteMinimal;
    bool strict = false;
};

// 读取状态
// reader state
// ᠤᠨᠰᠢᠬᠤ ᠪᠠᠶᠢᠳᠠᠯ
struct ReaderState{
    std::vector<std::vector<value>> rows;
    std::vector<long long> rowLines;
    std::size_t index = 0;
    long long lineNum = 0;
};

// 写状态
// writer state
// ᠪᠢᠴᠢᠬᠦ ᠪᠠᠶᠢᠳᠠᠯ
struct WriterState{
    Dialect dialect;
    value target;
};

// 字典读状态
// DictReader state
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠤᠨᠰᠢᠬᠤ ᠪᠠᠶᠢᠳᠠᠯ
struct DictReaderState{
    std::vector<std::vector<value>> rows;
    std::vector<long long> rowLines;
    std::size_t index = 0;
    value fieldnames;
    value restkey;
    value restval;
};

// 字典写状态
// DictWriter state
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠴᠢᠬᠦ ᠪᠠᠶᠢᠳᠠᠯ
struct DictWriterState{
    Dialect dialect;
    value target;
    std::vector<value> fieldnames;
    value restval;
    std::string extrasaction = "raise";
};

// 嗅探器状态
// sniffer state
// ᠦᠨᠦᠷᠢᠯᠲᠡ ᠪᠠᠶᠢᠳᠠᠯ
struct SnifferState{
    Dialect preferred;
    bool hasPreferred = false;
};

// 默认 excel 方言
// default excel dialect
// ᠡᠺᠰᠡᠯ ᠠᠶᠠᠯᠭᠤ
Dialect excelDialect(){
    Dialect d;
    return d;
}

// 方言注册表
// dialect registry
// ᠠᠶᠠᠯᠭᠤ ᠪᠦᠷᠢᠳᠡᠭᠦᠯᠦᠯ
std::map<std::string, Dialect>& dialectRegistry(){
    static std::map<std::string, Dialect> registry;
    static bool initialized = false;
    if (!initialized){
        initialized = true;
        registry["excel"] = excelDialect();
        Dialect tab = excelDialect();
        tab.delimiter = "\t";
        registry["excel-tab"] = tab;
        Dialect unix = excelDialect();
        unix.lineterminator = "\n";
        unix.quoting = quoteAll;
        registry["unix"] = unix;
    }
    return registry;
}

// 字符串转单字符
// string to single char
// ᠦᠰᠦᠭ ᠡᠴᠡ ᠨᠢᠭᠡ ᠦᠰᠦᠭ
bool singleChar(const value& v, char& out, bool& has){
    if (v.type == "none"){
        has = false;
        return true;
    }
    if ((v.type == "str" || v.type == "bytes") && v.ptr){
        std::string& s = *static_cast<std::string*>(v.ptr);
        if (s.size() == 1){
            out = s[0];
            has = true;
            return true;
        }
        return false;
    }
    return false;
}

// 发布方言属性
// publish dialect attributes
// ᠠᠶᠠᠯᠭᠤ ᠶᠢᠨ ᠰᠢᠨᠡᠯᠭᠡ
void publishDialect(cffi& a, const value& obj, const Dialect& d){
    a.setattr(obj, "delimiter", a.mkstr(d.delimiter));
    a.setattr(obj, "quotechar", d.hasQuotechar ? a.mkstr(std::string(1, d.quotechar)) : a.none());
    a.setattr(obj, "escapechar", d.hasEscapechar ? a.mkstr(std::string(1, d.escapechar)) : a.none());
    a.setattr(obj, "doublequote", a.mkbool(d.doublequote));
    a.setattr(obj, "skipinitialspace", a.mkbool(d.skipinitialspace));
    a.setattr(obj, "lineterminator", a.mkstr(d.lineterminator));
    a.setattr(obj, "quoting", a.mkint(d.quoting));
    a.setattr(obj, "strict", a.mkbool(d.strict));
}

// 从对象读取方言属性
// read dialect attributes from an object
// ᠣᠪᠶᠡᠭᠲ ᠡᠴᠡ ᠠᠶᠠᠯᠭᠤ ᠬᠠᠶᠢᠯᠤᠯ
bool readDialectAttrs(cffi& a, const value& obj, Dialect& d){
    if (obj.type != "native_object" || obj.ptr == nullptr) return false;
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    if (!inst->klass || inst->klass->name != "Dialect") return false;
    value v = a.attr(obj, "delimiter");
    if (v.type == "str" && v.ptr) d.delimiter = *static_cast<std::string*>(v.ptr);
    v = a.attr(obj, "quotechar");
    if (!singleChar(v, d.quotechar, d.hasQuotechar)) return false;
    v = a.attr(obj, "escapechar");
    if (!singleChar(v, d.escapechar, d.hasEscapechar)) return false;
    v = a.attr(obj, "doublequote");
    if (v.type != "none") d.doublequote = a.vm->truthy(v);
    v = a.attr(obj, "skipinitialspace");
    if (v.type != "none") d.skipinitialspace = a.vm->truthy(v);
    v = a.attr(obj, "lineterminator");
    if (v.type == "str" && v.ptr) d.lineterminator = *static_cast<std::string*>(v.ptr);
    v = a.attr(obj, "quoting");
    if (v.type != "none") d.quoting = a.vm->toInt(v);
    v = a.attr(obj, "strict");
    if (v.type != "none") d.strict = a.vm->truthy(v);
    return true;
}

// 解析方言参数
// resolve dialect parameter
// ᠠᠶᠠᠯᠭᠤ ᠶᠢᠨ ᠬᠠᠶᠢᠯᠤᠯ
bool resolveDialect(cffi& a, std::size_t index, Dialect& d, std::string& error){
    value source;
    if (a.argc() > index) source = a.arg(index);
    else if (a.has_kwarg("dialect")) source = a.kwarg("dialect");
    else return true;
    if (source.type == "none") return true;
    if (source.type == "native_object" && readDialectAttrs(a, source, d)) return true;
    if (source.type == "str" && source.ptr){
        std::string name = *static_cast<std::string*>(source.ptr);
        auto it = dialectRegistry().find(name);
        if (it == dialectRegistry().end()){
            error = "unknown dialect";
            return false;
        }
        d = it->second;
        return true;
    }
    error = "unknown dialect";
    return false;
}

// 应用 fmtparams
// apply fmtparams
// fmtparams ᠢ ᠬᠡᠷᠡᠭᠯᠡᠬᠦ
bool applyFmtparams(cffi& a, Dialect& d){
    if (a.has_kwarg("delimiter")){
        value v = a.kwarg("delimiter");
        if (v.type != "str" || !v.ptr) return false;
        d.delimiter = *static_cast<std::string*>(v.ptr);
    }
    if (a.has_kwarg("quotechar")){
        if (!singleChar(a.kwarg("quotechar"), d.quotechar, d.hasQuotechar)) return false;
    }
    if (a.has_kwarg("escapechar")){
        if (!singleChar(a.kwarg("escapechar"), d.escapechar, d.hasEscapechar)) return false;
    }
    if (a.has_kwarg("doublequote")) d.doublequote = a.vm->truthy(a.kwarg("doublequote"));
    if (a.has_kwarg("skipinitialspace")) d.skipinitialspace = a.vm->truthy(a.kwarg("skipinitialspace"));
    if (a.has_kwarg("lineterminator")){
        value v = a.kwarg("lineterminator");
        if (v.type == "str" && v.ptr) d.lineterminator = *static_cast<std::string*>(v.ptr);
    }
    if (a.has_kwarg("quoting")) d.quoting = a.kwarg_int("quoting", d.quoting);
    if (a.has_kwarg("strict")) d.strict = a.vm->truthy(a.kwarg("strict"));
    return true;
}

// 校验方言
// validate dialect
// ᠠᠶᠠᠯᠭᠤ ᠶᠢ ᠰᠢᠯᠢᠳᠭᠡᠬᠦ
bool validateDialect(cffi& a, const Dialect& d){
    if (d.delimiter.size() != 1){
        a.vm->raiseError("Error", "\"delimiter\" must be a 1-character string");
        return false;
    }
    if (!d.hasQuotechar && d.quoting != quoteNone){
        a.vm->raiseError("Error", "quotechar must be set if quoting enabled");
        return false;
    }
    if (d.quoting < quoteMinimal || d.quoting > quoteNotNull){
        a.vm->raiseError("Error", "bad \"quoting\" value");
        return false;
    }
    return true;
}

// 创建方言对象
// create a Dialect object
// ᠠᠶᠠᠯᠭᠤ ᠣᠪᠶᠡᠭᠲ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeDialectObject(cffi& a, const Dialect& d){
    auto it = a.library->classes.find("Dialect");
    if (it == a.library->classes.end()) return a.none();
    value obj = a.vm->makeNativeObject(a.library, &it->second, new Dialect(d));
    publishDialect(a, obj, d);
    return obj;
}

// 按类名创建对象
// create object of a class by name
// ᠨᠡᠷᠡ ᠶᠢᠨ ᠲᠦᠯᠦᠪ ᠣᠪᠶᠡᠭᠲ
value makeObjectOf(cffi& a, const std::string& className, void* handle){
    auto it = a.library->classes.find(className);
    if (it == a.library->classes.end()) return a.none();
    return a.vm->makeNativeObject(a.library, &it->second, handle);
}

// 收集行
// collect lines from an iterable
// ᠮᠥᠷ ᠴᠤᠭᠯᠠᠬᠤ
bool collectLines(cffi& a, const value& source, std::vector<std::string>& lines,
                  std::string& error){
    std::vector<value> items;
    a.vm->materializeIterable(source, items);
    if (a.vm->hasError()){
        a.vm->clearErr();
        error = "iterable expected";
        return false;
    }
    for (auto& item : items){
        if (item.type == "str" && item.ptr){
            lines.push_back(*static_cast<std::string*>(item.ptr));
        } else if ((item.type == "bytes" || item.type == "bytearray") && item.ptr){
            error = "iterator should return strings, not bytes (the file should be opened in text mode)";
            return false;
        } else {
            error = "iterator should return strings, not " + a.vm->pythonTypeName(item) +
                    " (the file should be opened in text mode)";
            return false;
        }
    }
    return true;
}

// 浮点解析
// parse a float strictly
// ᠬᠣᠷᠤᠮ ᠲᠣᠭ᠎ᠠ ᠬᠠᠶᠢᠯᠤᠯ
bool parseFloatText(const std::string& text, double& out){
    std::string trimmed;
    for (char c : text){
        if (!std::isspace(static_cast<unsigned char>(c))) trimmed.push_back(c);
    }
    if (trimmed.empty()) return false;
    const char* begin = trimmed.c_str();
    char* end = nullptr;
    double result = std::strtod(begin, &end);
    if (end != begin + trimmed.size()) return false;
    out = result;
    return true;
}

// 解析 CSV 行
// parse CSV rows
// CSV ᠮᠥᠷ ᠬᠠᠶᠢᠯᠤᠯ
bool parseCsv(vm& vm, const std::vector<std::string>& lines, const Dialect& d,
              std::vector<std::vector<value>>& rows, std::vector<long long>& rowLines,
              std::string& error){
    std::string field;
    std::vector<value> row;
    bool inQuotes = false;
    bool hadQuote = false;
    long long fieldLimit = 131072;

    auto pushField = [&]() -> bool {
        if (static_cast<long long>(field.size()) > fieldLimit){
            error = "field larger than field limit (" + std::to_string(fieldLimit) + ")";
            return false;
        }
        value fv;
        if (d.quoting == quoteNonNumeric && !hadQuote && !field.empty()){
            double parsed = 0.0;
            if (!parseFloatText(field, parsed)){
                error = "could not convert string to float: '" + field + "'";
                return false;
            }
            fv = vm.makeFloat(parsed);
        } else {
            fv = vm.makeString(field);
        }
        row.push_back(fv);
        field.clear();
        hadQuote = false;
        return true;
    };
    auto finishRow = [&](long long lineNo) -> bool {
        if (!(row.empty() && field.empty() && !hadQuote)){
            if (!pushField()) return false;
        }
        rows.push_back(row);
        rowLines.push_back(lineNo);
        row.clear();
        return true;
    };

    for (std::size_t li = 0; li < lines.size(); ++li){
        const std::string& line = lines[li];
        std::size_t i = 0;
        bool terminated = false;
        bool atFieldStart = true;
        while (i < line.size()){
            char c = line[i];
            if (inQuotes){
                if (d.hasEscapechar && c == d.escapechar && i + 1 < line.size()){
                    field.push_back(line[i + 1]);
                    i += 2;
                    continue;
                }
                if (d.hasQuotechar && c == d.quotechar){
                    if (d.doublequote && i + 1 < line.size() && line[i + 1] == d.quotechar){
                        field.push_back(d.quotechar);
                        i += 2;
                        continue;
                    }
                    inQuotes = false;
                    ++i;
                    atFieldStart = false;
                    continue;
                }
                if (c == '\r' && i + 1 < line.size() && line[i + 1] == '\n'){
                    field.push_back('\n');
                    i += 2;
                    continue;
                }
                field.push_back(c);
                ++i;
                continue;
            }
            if (atFieldStart && d.skipinitialspace && c == ' '){
                ++i;
                continue;
            }
            if (d.hasQuotechar && c == d.quotechar && field.empty() &&
                d.quoting != quoteNone){
                inQuotes = true;
                hadQuote = true;
                atFieldStart = false;
                ++i;
                continue;
            }
            if (c == d.delimiter[0]){
                if (!pushField()) return false;
                atFieldStart = true;
                ++i;
                continue;
            }
            if (c == '\n' || c == '\r'){
                if (!finishRow(static_cast<long long>(li) + 1)) return false;
                terminated = true;
                bool crlf = c == '\r' && i + 1 < line.size() && line[i + 1] == '\n';
                i += crlf ? 2 : 1;
                if (i < line.size()){
                    error = "new-line character seen in unquoted field - "
                            "do you need to open the file with newline=''?";
                    return false;
                }
                break;
            }
            if (d.hasEscapechar && c == d.escapechar){
                if (i + 1 < line.size()){
                    field.push_back(line[i + 1]);
                    i += 2;
                    continue;
                }
                ++i;
                continue;
            }
            field.push_back(c);
            atFieldStart = false;
            ++i;
        }
        if (!inQuotes && !terminated){
            if (!finishRow(static_cast<long long>(li) + 1)) return false;
        }
    }
    if (inQuotes){
        if (d.strict){
            error = "unexpected end of data";
            return false;
        }
        if (!finishRow(static_cast<long long>(lines.size()))) return false;
    }
    return true;
}

// 构造 reader
// build reader
// ᠤᠨᠰᠢᠭᠴᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value buildReader(cffi& a, const value& source, std::size_t dialectIndex,
                  std::string& error){
    std::vector<std::string> lines;
    if (!collectLines(a, source, lines, error)) return a.none();
    Dialect d = excelDialect();
    if (!resolveDialect(a, dialectIndex, d, error)) return a.none();
    if (!applyFmtparams(a, d)){ error = "bad dialect parameter"; return a.none(); }
    if (!validateDialect(a, d)) return a.none();
    auto* state = new ReaderState();
    if (!parseCsv(*a.vm, lines, d, state->rows, state->rowLines, error)){
        delete state;
        return a.none();
    }
    state->lineNum = 0;
    value obj = makeObjectOf(a, "reader", state);
    if (obj.type != "native_object"){
        delete state;
        return a.none();
    }
    a.setattr(obj, "dialect", makeDialectObject(a, d));
    a.setattr(obj, "line_num", a.mkint(0));
    return obj;
}

// 格式化字段
// format one CSV field
// ᠨᠢᠭᠡ ᠲᠠᠯᠠᠭ᠌ ᠬᠡᠪᠯᠡᠬᠦ
bool formatField(vm& vm, const value& field, const Dialect& d, std::string& out,
                 std::string& error){
    bool isNone = field.type == "none";
    bool isString = field.type == "str";
    bool isNumeric = field.type == "int" || field.type == "float" || field.type == "bool";
    std::string text;
    if (isNone) text = "";
    else if (isString) text = *static_cast<std::string*>(field.ptr);
    else text = vm.toString(field);

    bool needQuote = false;
    switch (d.quoting){
        case quoteMinimal:
            needQuote = false;
            break;
        case quoteAll:
            needQuote = true;
            break;
        case quoteNonNumeric:
            needQuote = !isNumeric;
            break;
        case quoteNone:
            needQuote = false;
            break;
        case quoteStrings:
            needQuote = isString;
            break;
        case quoteNotNull:
            needQuote = !isNone;
            break;
    }
    if (d.quoting == quoteMinimal){
        if (d.hasQuotechar && text.find(d.quotechar) != std::string::npos) needQuote = true;
        if (text.find(d.delimiter) != std::string::npos) needQuote = true;
        if (d.hasEscapechar && text.find(d.escapechar) != std::string::npos) needQuote = true;
        if (text.find('\r') != std::string::npos || text.find('\n') != std::string::npos)
            needQuote = true;
    }
    if (needQuote){
        if (!d.hasQuotechar){
            error = "quotechar must be set if quoting enabled";
            return false;
        }
        std::string escaped;
        for (char c : text){
            if (c == d.quotechar){
                if (d.doublequote){
                    escaped.push_back(d.quotechar);
                    escaped.push_back(d.quotechar);
                } else if (d.hasEscapechar){
                    escaped.push_back(d.escapechar);
                    escaped.push_back(c);
                } else {
                    error = "need to escape, but no escapechar set";
                    return false;
                }
            } else if (d.hasEscapechar && c == d.escapechar){
                escaped.push_back(d.escapechar);
                escaped.push_back(c);
            } else {
                escaped.push_back(c);
            }
        }
        out.push_back(d.quotechar);
        out += escaped;
        out.push_back(d.quotechar);
        return true;
    }
    if (d.quoting == quoteNone){
        if (!d.hasEscapechar){
            for (char c : text){
                if (c == d.delimiter[0] || c == '\r' || c == '\n' ||
                    (d.hasQuotechar && c == d.quotechar)){
                    error = "need to escape, but no escapechar set";
                    return false;
                }
            }
        } else {
            for (char c : text){
                if (c == d.delimiter[0] || c == '\r' || c == '\n' ||
                    (d.hasQuotechar && c == d.quotechar) || c == d.escapechar)
                    out.push_back(d.escapechar);
                out.push_back(c);
            }
            return true;
        }
    }
    if (d.hasEscapechar){
        for (char c : text){
            if (c == d.escapechar) out.push_back(d.escapechar);
            out.push_back(c);
        }
        return true;
    }
    out += text;
    return true;
}

// 写入一行
// write one row
// ᠨᠢᠭᠡ ᠮᠥᠷ ᠪᠢᠴᠢᠬᠦ
bool writeRow(cffi& a, const Dialect& dialect, const value& target, const value& row,
              long long& written, std::string& error){
    std::vector<value> fields;
    a.vm->materializeIterable(row, fields);
    if (a.vm->hasError()){
        a.vm->clearErr();
        error = "iterable expected";
        return false;
    }
    std::string line;
    for (std::size_t i = 0; i < fields.size(); ++i){
        if (i > 0) line += dialect.delimiter;
        if (!formatField(*a.vm, fields[i], dialect, line, error)) return false;
    }
    line += dialect.lineterminator;
    value result;
    bool ok = cffiCallMethod(*a.vm, target, "write", {a.mkstr(line)}, result);
    if (!ok){
        a.vm->clearErr();
        error = "argument of type '" + a.vm->pythonTypeName(target) +
                "' is not iterable";
        return false;
    }
    written = static_cast<long long>(line.size());
    return true;
}

// reader 构造
// reader constructor
// reader ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnReaderCtor(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "reader() missing required argument: 'csvfile'");
        return a.none();
    }
    std::string error;
    value obj = buildReader(a, a.arg(0), 1, error);
    if (!error.empty()){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    return obj;
}

// writer 构造
// writer constructor
// writer ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnWriterCtor(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "writer() missing required argument: 'csvfile'");
        return a.none();
    }
    Dialect d = excelDialect();
    std::string error;
    if (!resolveDialect(a, 1, d, error) || !applyFmtparams(a, d)){
        a.vm->raiseError("Error", error.empty() ? "bad dialect parameter" : error);
        return a.none();
    }
    if (!validateDialect(a, d)) return a.none();
    auto* state = new WriterState();
    state->dialect = d;
    state->target = a.arg(0);
    value obj = makeObjectOf(a, "writer", state);
    if (obj.type != "native_object"){
        delete state;
        return a.none();
    }
    a.setattr(obj, "dialect", makeDialectObject(a, d));
    return obj;
}

// reader.__iter__
// reader.__iter__
// reader.__iter__
value fnReaderIter(cffi& a){
    return a.self();
}

// reader.__next__
// reader.__next__
// reader.__next__
value fnReaderNext(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<ReaderState*>(inst->handle);
    if (!state || state->index >= state->rows.size()){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    state->lineNum = state->rowLines[state->index];
    value row = a.mklist(state->rows[state->index]);
    ++state->index;
    a.setattr(self, "line_num", a.mkint(state->lineNum));
    return row;
}

// writer.writerow
// writer.writerow
// writer.writerow
value fnWriterRow(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<WriterState*>(inst->handle);
    long long written = 0;
    std::string error;
    if (!writeRow(a, state->dialect, state->target, a.arg(0), written, error)){
        a.vm->raiseError(error.find("not iterable") != std::string::npos ? "TypeError" : "Error",
                         error);
        return a.none();
    }
    return a.mkint(written);
}

// writer.writerows
// writer.writerows
// writer.writerows
value fnWriterRows(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<WriterState*>(inst->handle);
    std::vector<value> rows;
    a.vm->materializeIterable(a.arg(0), rows);
    if (a.vm->hasError()) return a.none();
    for (auto& row : rows){
        long long written = 0;
        std::string error;
        if (!writeRow(a, state->dialect, state->target, row, written, error)){
            a.vm->raiseError("Error", error);
            return a.none();
        }
    }
    return a.none();
}

// Dialect 构造
// Dialect constructor
// Dialect ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnDialectCtor(cffi& a){
    if (a.argc() == 0 && !a.has_kwarg("delimiter")){
        a.vm->raiseError("Error", "\"delimiter\" must be a unicode character, not NoneType");
        return a.none();
    }
    Dialect d = excelDialect();
    std::string error;
    if (a.argc() >= 1 && !resolveDialect(a, 0, d, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    if (!applyFmtparams(a, d)){
        a.vm->raiseError("Error", "\"delimiter\" must be a 1-character string");
        return a.none();
    }
    if (!validateDialect(a, d)) return a.none();
    return makeDialectObject(a, d);
}

// DictReader 构造
// DictReader constructor
// DictReader ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnDictReaderCtor(cffi& a){
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "DictReader() missing required argument: 'f'");
        return a.none();
    }
    std::vector<std::string> lines;
    std::string error;
    if (!collectLines(a, a.arg(0), lines, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    Dialect d = excelDialect();
    std::size_t dialectIndex = 4;
    if (!resolveDialect(a, dialectIndex, d, error) || !applyFmtparams(a, d)){
        a.vm->raiseError("Error", error.empty() ? "bad dialect parameter" : error);
        return a.none();
    }
    if (!validateDialect(a, d)) return a.none();
    auto* state = new DictReaderState();
    if (!parseCsv(*a.vm, lines, d, state->rows, state->rowLines, error)){
        delete state;
        a.vm->raiseError("Error", error);
        return a.none();
    }
    state->fieldnames = a.argc() > 1 ? a.arg(1) : a.none();
    state->restkey = a.argc() > 2 ? a.arg(2) : a.none();
    state->restval = a.argc() > 3 ? a.arg(3) : a.none();
    value obj = makeObjectOf(a, "DictReader", state);
    if (obj.type != "native_object"){
        delete state;
        return a.none();
    }
    a.setattr(obj, "fieldnames", state->fieldnames);
    a.setattr(obj, "restkey", state->restkey);
    a.setattr(obj, "restval", state->restval);
    a.setattr(obj, "line_num", a.mkint(0));
    value dialectObj = makeDialectObject(a, d);
    a.setattr(obj, "dialect", dialectObj);
    return obj;
}

// DictReader.__iter__
// DictReader.__iter__
// DictReader.__iter__
value fnDictReaderIter(cffi& a){
    return a.self();
}

// DictReader.__next__
// DictReader.__next__
// DictReader.__next__
value fnDictReaderNext(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<DictReaderState*>(inst->handle);
    if (!state) return a.none();
    if (state->fieldnames.type == "none" || !state->fieldnames.ptr){
        if (state->index >= state->rows.size()){
            a.vm->raiseError("StopIteration", "");
            return a.none();
        }
        state->fieldnames = a.mklist(state->rows[state->index]);
        a.setattr(self, "fieldnames", state->fieldnames);
        ++state->index;
    }
    if (state->index >= state->rows.size()){
        a.vm->raiseError("StopIteration", "");
        return a.none();
    }
    std::vector<value> names;
    a.vm->materializeIterable(state->fieldnames, names);
    std::vector<std::pair<value, value>> items;
    const std::vector<value>& row = state->rows[state->index];
    for (std::size_t i = 0; i < names.size(); ++i){
        value val = i < row.size() ? row[i] : state->restval;
        items.push_back(std::pair<value, value>(names[i], val));
    }
    if (row.size() > names.size()){
        std::vector<value> extras(row.begin() + static_cast<long long>(names.size()), row.end());
        items.push_back(std::pair<value, value>(state->restkey, a.mklist(extras)));
    }
    a.setattr(self, "line_num", a.mkint(state->rowLines[state->index]));
    ++state->index;
    return a.mkdict(items);
}

// DictWriter 构造
// DictWriter constructor
// DictWriter ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnDictWriterCtor(cffi& a){
    if (a.argc() < 2){
        a.vm->raiseError("TypeError", "DictWriter() missing required arguments");
        return a.none();
    }
    Dialect d = excelDialect();
    std::string error;
    if (!resolveDialect(a, 4, d, error) || !applyFmtparams(a, d)){
        a.vm->raiseError("Error", error.empty() ? "bad dialect parameter" : error);
        return a.none();
    }
    if (!validateDialect(a, d)) return a.none();
    auto* state = new DictWriterState();
    state->dialect = d;
    state->target = a.arg(0);
    a.vm->materializeIterable(a.arg(1), state->fieldnames);
    if (a.vm->hasError()){
        delete state;
        return a.none();
    }
    state->restval = a.argc() > 2 ? a.arg(2) : a.mkstr("");
    if (a.argc() > 3 && a.arg(3).type == "str" && a.arg(3).ptr)
        state->extrasaction = *static_cast<std::string*>(a.arg(3).ptr);
    if (state->extrasaction != "raise" && state->extrasaction != "ignore"){
        delete state;
        a.vm->raiseError("ValueError", "extrasaction (" + state->extrasaction +
                                        ") must be 'ignore' or 'raise'");
        return a.none();
    }
    value obj = makeObjectOf(a, "DictWriter", state);
    if (obj.type != "native_object"){
        delete state;
        return a.none();
    }
    a.setattr(obj, "fieldnames", a.arg(1));
    a.setattr(obj, "restval", state->restval);
    a.setattr(obj, "extrasaction", a.mkstr(state->extrasaction));
    a.setattr(obj, "dialect", makeDialectObject(a, d));
    return obj;
}

// DictWriter.writeheader
// DictWriter.writeheader
// DictWriter.writeheader
value fnDictWriterHeader(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<DictWriterState*>(inst->handle);
    std::vector<value> fields(state->fieldnames.begin(), state->fieldnames.end());
    std::string error;
    long long written = 0;
    if (!writeRow(a, state->dialect, state->target, a.mklist(fields), written, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    return a.none();
}

// DictWriter.writerow
// DictWriter.writerow
// DictWriter.writerow
value fnDictWriterRow(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<DictWriterState*>(inst->handle);
    value rowdict = a.arg(0);
    if (rowdict.type != "dict" || rowdict.ptr == nullptr){
        a.vm->raiseError("AttributeError", "'" + a.vm->pythonTypeName(rowdict) +
                                          "' object has no attribute 'keys'");
        return a.none();
    }
    auto* dict = static_cast<dict_t*>(rowdict.ptr);
    std::vector<value> extraKeys;
    auto dictLookup = [&](const value& key) -> const value* {
        for (auto& entry : *dict)
            if (a.vm->valuesEqual(entry.first, key)) return &entry.second;
        return nullptr;
    };
    for (auto& entry : *dict){
        bool known = false;
        for (auto& name : state->fieldnames){
            if (a.vm->valuesEqual(name, entry.first)){ known = true; break; }
        }
        if (!known) extraKeys.push_back(entry.first);
    }
    if (!extraKeys.empty() && state->extrasaction == "raise"){
        std::string joined;
        for (std::size_t i = 0; i < extraKeys.size(); ++i){
            if (i > 0) joined += ", ";
            joined += pythonRepr(*a.vm, extraKeys[i]);
        }
        a.vm->raiseError("ValueError", "dict contains fields not in fieldnames: " + joined);
        return a.none();
    }
    std::vector<value> fields;
    for (auto& name : state->fieldnames){
        const value* found = dictLookup(name);
        fields.push_back(found ? *found : state->restval);
    }
    std::string error;
    long long written = 0;
    if (!writeRow(a, state->dialect, state->target, a.mklist(fields), written, error)){
        a.vm->raiseError("Error", error);
        return a.none();
    }
    return a.mkint(written);
}

// DictWriter.writerows
// DictWriter.writerows
// DictWriter.writerows
value fnDictWriterRows(cffi& a){
    std::vector<value> rows;
    a.vm->materializeIterable(a.arg(0), rows);
    if (a.vm->hasError()) return a.none();
    for (auto& row : rows){
        value self = a.self();
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        auto* state = static_cast<DictWriterState*>(inst->handle);
        if (row.type != "dict" || !row.ptr){
            a.vm->raiseError("AttributeError", "'" + a.vm->pythonTypeName(row) +
                                              "' object has no attribute 'keys'");
            return a.none();
        }
        auto* dict = static_cast<dict_t*>(row.ptr);
        std::vector<value> fields;
        for (auto& name : state->fieldnames){
            const value* found = nullptr;
            for (auto& entry : *dict){
                if (a.vm->valuesEqual(entry.first, name)){ found = &entry.second; break; }
            }
            fields.push_back(found ? *found : state->restval);
        }
        std::string error;
        long long written = 0;
        if (!writeRow(a, state->dialect, state->target, a.mklist(fields), written, error)){
            a.vm->raiseError("Error", error);
            return a.none();
        }
    }
    return a.none();
}

// Sniffer 构造
// Sniffer constructor
// Sniffer ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
value fnSnifferCtor(cffi& a){
    return makeObjectOf(a, "Sniffer", new SnifferState());
}

// 文本行拆分
// split sample text into lines
// ᠲᠡᠮᠳᠡᠭ ᠮᠥᠷ ᠬᠤᠪᠢᠶ᠎ᠠ
std::vector<std::string> splitSample(const std::string& text){
    std::vector<std::string> lines;
    std::size_t start = 0;
    std::size_t i = 0;
    while (i < text.size()){
        if (text[i] == '\n' || text[i] == '\r'){
            std::size_t end = i + 1;
            if (text[i] == '\r' && end < text.size() && text[end] == '\n') ++end;
            lines.push_back(text.substr(start, end - start));
            start = end;
            i = end;
        } else {
            ++i;
        }
    }
    if (start < text.size()) lines.push_back(text.substr(start));
    return lines;
}

// 猜测分隔符
// guess delimiter
// ᠬᠤᠪᠢᠶᠠᠭᠴᠢ ᠲᠠᠭᠠᠬᠤ
char guessDelimiter(const std::string& sample, const std::vector<char>& candidates){
    std::vector<std::string> lines = splitSample(sample);
    char best = 0;
    long long bestScore = -1;
    for (char candidate : candidates){
        std::vector<long long> counts;
        for (auto& line : lines){
            bool quoted = false;
            long long count = 0;
            for (std::size_t i = 0; i < line.size(); ++i){
                char c = line[i];
                if (c == '"' || c == '\''){
                    quoted = !quoted;
                    continue;
                }
                if (!quoted && c == candidate) ++count;
            }
            if (count > 0) counts.push_back(count);
        }
        if (counts.empty()) continue;
        long long modal = counts[0];
        long long modalCount = 0;
        for (long long value : counts){
            long long occurrences = 0;
            for (long long other : counts) if (other == value) ++occurrences;
            if (occurrences > modalCount){
                modalCount = occurrences;
                modal = value;
            }
        }
        long long total = 0;
        for (long long value : counts) total += value;
        long long score = modalCount * 1000 + total * 2 + modal;
        if (score > bestScore){
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

// 猜测引用字符
// guess quote character
// ᠬᠠᠶᠢᠯᠤᠭ᠎ᠠ ᠦᠰᠦᠭ ᠲᠠᠭᠠᠬᠤ
char guessQuotechar(const std::string& sample, bool& doublequote, bool& skipinitialspace){
    doublequote = true;
    skipinitialspace = false;
    auto tryQuote = [&](char quote) -> bool {
        std::size_t first = sample.find(quote);
        if (first == std::string::npos) return false;
        std::size_t second = sample.find(quote, first + 1);
        if (second == std::string::npos) return false;
        std::string between = sample.substr(first + 1, second - first - 1);
        if (between.find('\n') != std::string::npos ||
            between.find('\r') != std::string::npos) return false;
        std::size_t doubled = sample.find(std::string(1, quote) + quote);
        doublequote = doubled != std::string::npos;
        if (first > 0 && sample[first - 1] == ' ') skipinitialspace = true;
        return true;
    };
    if (tryQuote('"')) return '"';
    if (tryQuote('\'')) return '\'';
    return 0;
}

// 分析样本
// analyze sample
// ᠲᠡᠮᠳᠡᠭ ᠰᠢᠨᠵᠢᠯᠡᠬᠦ
bool analyzeSample(vm& vm, const std::string& sample, const std::vector<char>& preferred,
                   Dialect& d, bool& hasHeader){
    char delimiter = guessDelimiter(sample, preferred);
    if (delimiter == 0) delimiter = ',';
    d.delimiter = std::string(1, delimiter);
    char quotechar = guessQuotechar(sample, d.doublequote, d.skipinitialspace);
    if (quotechar != 0){
        d.hasQuotechar = true;
        d.quotechar = quotechar;
    } else {
        d.hasQuotechar = false;
    }
    std::vector<std::string> lines = splitSample(sample);
    std::vector<std::vector<value>> rows;
    std::vector<long long> rowLines;
    std::string error;
    Dialect parseDialect = d;
    if (parseCsv(vm, lines, parseDialect, rows, rowLines, error)) {
        hasHeader = false;
        if (rows.size() >= 2){
            bool sawText = false;
            bool sawNumericLater = false;
            for (std::size_t column = 0; column < rows.front().size(); ++column){
                const value& head = rows.front()[column];
                double numVal = 0.0;
                bool headNumeric = head.type == "int" || head.type == "float" ||
                    (head.type == "str" && head.ptr &&
                     parseFloatText(*static_cast<std::string*>(head.ptr), numVal));
                if (head.type == "str" && head.ptr &&
                    !static_cast<std::string*>(head.ptr)->empty() && !headNumeric)
                    sawText = true;
                for (std::size_t r = 1; r < rows.size(); ++r){
                    if (column < rows[r].size()){
                        const value& cell = rows[r][column];
                        if (cell.type == "int" || cell.type == "float"){
                            sawNumericLater = true;
                        } else if (cell.type == "str" && cell.ptr &&
                                   parseFloatText(*static_cast<std::string*>(cell.ptr), numVal)){
                            sawNumericLater = true;
                        }
                    }
                }
                if (headNumeric){
                    for (std::size_t r = 1; r < rows.size(); ++r){
                        if (column < rows[r].size()){
                            const value& cell = rows[r][column];
                            if (cell.type == "str" && cell.ptr &&
                                !parseFloatText(*static_cast<std::string*>(cell.ptr), numVal))
                                sawText = true;
                        }
                    }
                }
            }
            hasHeader = sawText && sawNumericLater;
        }
    } else {
        hasHeader = false;
    }
    (void)vm;
    return true;
}

// Sniffer.sniff
// Sniffer.sniff
// Sniffer.sniff
value fnSnifferSniff(cffi& a){
    std::vector<char> preferred = {',', '\t', ';', ' ', ':', '|'};
    if (a.argc() > 1 && a.arg(1).type == "str" && a.arg(1).ptr){
        preferred.clear();
        for (char c : *static_cast<std::string*>(a.arg(1).ptr)) preferred.push_back(c);
    }
    Dialect d = excelDialect();
    bool hasHeader = false;
    std::string sample = a.arg_str(0);
    analyzeSample(*a.vm, sample, preferred, d, hasHeader);
    return makeDialectObject(a, d);
}

// Sniffer.has_header
// Sniffer.has_header
// Sniffer.has_header
value fnSnifferHasHeader(cffi& a){
    std::vector<char> preferred = {',', '\t', ';', ' ', ':', '|'};
    Dialect d = excelDialect();
    bool hasHeader = false;
    analyzeSample(*a.vm, a.arg_str(0), preferred, d, hasHeader);
    return a.mkbool(hasHeader);
}

// Sniffer.sniff_sample
// Sniffer.sniff_sample
// Sniffer.sniff_sample
value fnSnifferSniffSample(cffi& a){
    value self = a.self();
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    auto* state = static_cast<SnifferState*>(inst->handle);
    std::vector<char> preferred = {',', '\t', ';', ' ', ':', '|'};
    if (a.argc() > 1 && a.arg(1).type == "str" && a.arg(1).ptr){
        preferred.clear();
        for (char c : *static_cast<std::string*>(a.arg(1).ptr)) preferred.push_back(c);
    }
    Dialect d = excelDialect();
    bool hasHeader = false;
    analyzeSample(*a.vm, a.arg_str(0), preferred, d, hasHeader);
    if (state) state->preferred = d;
    a.setattr(self, "delimiter", a.mkstr(d.delimiter));
    a.setattr(self, "quotechar", d.hasQuotechar ? a.mkstr(std::string(1, d.quotechar)) : a.none());
    a.setattr(self, "doublequote", a.mkbool(d.doublequote));
    a.setattr(self, "skipinitialspace", a.mkbool(d.skipinitialspace));
    a.setattr(self, "lineterminator", a.mkstr(d.lineterminator));
    a.setattr(self, "quoting", a.mkint(d.quoting));
    a.setattr(self, "escapechar", d.hasEscapechar ? a.mkstr(std::string(1, d.escapechar)) : a.none());
    return self;
}

// 注册方言
// register dialect
// ᠠᠶᠠᠯᠭᠤ ᠪᠦᠷᠢᠳᠡᠭᠦᠯᠬᠦ
value fnRegisterDialect(cffi& a){
    if (a.argc() < 1 || a.arg(0).type != "str"){
        a.vm->raiseError("TypeError", "register_dialect() argument 1 must be a string");
        return a.none();
    }
    std::string name = a.arg_str(0);
    Dialect d = excelDialect();
    std::string error;
    if (!resolveDialect(a, 1, d, error) || !applyFmtparams(a, d)){
        a.vm->raiseError("Error", error.empty() ? "bad dialect parameter" : error);
        return a.none();
    }
    if (!validateDialect(a, d)) return a.none();
    dialectRegistry()[name] = d;
    return a.none();
}

// 注销方言
// unregister dialect
// ᠠᠶᠠᠯᠭᠤ ᠤᠭᠳᠠᠯᠭ᠎ᠠ
value fnUnregisterDialect(cffi& a){
    std::string name = a.arg_str(0);
    auto it = dialectRegistry().find(name);
    if (it == dialectRegistry().end()){
        a.vm->raiseError("Error", "unknown dialect");
        return a.none();
    }
    dialectRegistry().erase(it);
    return a.none();
}

// 取得方言
// get dialect
// ᠠᠶᠠᠯᠭᠤ ᠬᠠᠯᠠᠬᠤ
value fnGetDialect(cffi& a){
    std::string name = a.arg_str(0);
    auto it = dialectRegistry().find(name);
    if (it == dialectRegistry().end()){
        a.vm->raiseError("Error", "unknown dialect");
        return a.none();
    }
    return makeDialectObject(a, it->second);
}

// 列出方言
// list dialects
// ᠠᠶᠠᠯᠭᠤ ᠵᠢᠭᠰᠠᠬᠤ
value fnListDialects(cffi& a){
    std::vector<value> names;
    for (auto& entry : dialectRegistry()) names.push_back(a.mkstr(entry.first));
    return a.mklist(names);
}

// 字段大小限制
// field size limit
// ᠲᠠᠯᠠᠭ᠌ ᠶᠢᠨ ᠬᠡᠮᠵᠢᠶ᠎ᠡ
value fnFieldSizeLimit(cffi& a){
    static long long limit = 131072;
    long long old = limit;
    if (a.argc() > 0 && a.arg(0).type != "none") limit = a.arg_int(0);
    return a.mkint(old);
}

// 模块数据: 引用常量
// module data: quoting constants
// ᠮᠣᠳᠤᠯ ᠶᠢᠨ ᠳ᠋ᠣᠲᠣᠷᠠᠭᠤ ᠲᠣᠭ᠎ᠠ
value dataQuoteMinimal(cffi& a){ return a.mkint(quoteMinimal); }
value dataQuoteAll(cffi& a){ return a.mkint(quoteAll); }
value dataQuoteNonNumeric(cffi& a){ return a.mkint(quoteNonNumeric); }
value dataQuoteNone(cffi& a){ return a.mkint(quoteNone); }
value dataQuoteStrings(cffi& a){ return a.mkint(quoteStrings); }
value dataQuoteNotNull(cffi& a){ return a.mkint(quoteNotNull); }
value dataVersion(cffi& a){ return a.mkstr("1.0"); }

// 模块数据: excel 方言
// module data: excel dialect
// excel ᠠᠶᠠᠯᠭᠤ
value dataExcel(cffi& a){ return makeDialectObject(a, excelDialect()); }

// 模块数据: excel-tab 方言
// module data: excel-tab dialect
// excel-tab ᠠᠶᠠᠯᠭᠤ
value dataExcelTab(cffi& a){
    Dialect d = excelDialect();
    d.delimiter = "\t";
    return makeDialectObject(a, d);
}

// 模块数据: unix 方言
// module data: unix dialect
// unix ᠠᠶᠠᠯᠭᠤ
value dataUnixDialect(cffi& a){
    Dialect d = excelDialect();
    d.lineterminator = "\n";
    d.quoting = quoteAll;
    return makeDialectObject(a, d);
}

}

// cffi csv 库
// cffi csv library
// cffi csv ᠨᠣᠮ
ffilib cffi_csv_lib(){
    ffi_class dialectClass;
    dialectClass.name = "Dialect";
    dialectClass.constructor = (void*)fnDialectCtor;

    ffi_class readerClass;
    readerClass.name = "reader";
    readerClass.constructor = (void*)fnReaderCtor;
    readerClass.methods = {
        {"__iter__", (void*)fnReaderIter},
        {"__next__", (void*)fnReaderNext}
    };

    ffi_class writerClass;
    writerClass.name = "writer";
    writerClass.constructor = (void*)fnWriterCtor;
    writerClass.methods = {
        {"writerow", (void*)fnWriterRow},
        {"writerows", (void*)fnWriterRows}
    };

    ffi_class dictReaderClass;
    dictReaderClass.name = "DictReader";
    dictReaderClass.constructor = (void*)fnDictReaderCtor;
    dictReaderClass.methods = {
        {"__iter__", (void*)fnDictReaderIter},
        {"__next__", (void*)fnDictReaderNext}
    };

    ffi_class dictWriterClass;
    dictWriterClass.name = "DictWriter";
    dictWriterClass.constructor = (void*)fnDictWriterCtor;
    dictWriterClass.methods = {
        {"writeheader", (void*)fnDictWriterHeader},
        {"writerow", (void*)fnDictWriterRow},
        {"writerows", (void*)fnDictWriterRows}
    };

    ffi_class snifferClass;
    snifferClass.name = "Sniffer";
    snifferClass.constructor = (void*)fnSnifferCtor;
    snifferClass.methods = {
        {"sniff", (void*)fnSnifferSniff},
        {"has_header", (void*)fnSnifferHasHeader},
        {"sniff_sample", (void*)fnSnifferSniffSample}
    };

    return cffi_lib(
        "csv",
        {
            {"reader", (void*)fnReaderCtor},
            {"writer", (void*)fnWriterCtor},
            {"register_dialect", (void*)fnRegisterDialect},
            {"unregister_dialect", (void*)fnUnregisterDialect},
            {"get_dialect", (void*)fnGetDialect},
            {"list_dialects", (void*)fnListDialects},
            {"field_size_limit", (void*)fnFieldSizeLimit}
        },
        {
            {"QUOTE_MINIMAL", (void*)dataQuoteMinimal},
            {"QUOTE_ALL", (void*)dataQuoteAll},
            {"QUOTE_NONNUMERIC", (void*)dataQuoteNonNumeric},
            {"QUOTE_NONE", (void*)dataQuoteNone},
            {"QUOTE_STRINGS", (void*)dataQuoteStrings},
            {"QUOTE_NOTNULL", (void*)dataQuoteNotNull},
            {"__version__", (void*)dataVersion},
            {"excel", (void*)dataExcel},
            {"excel_tab", (void*)dataExcelTab},
            {"unix_dialect", (void*)dataUnixDialect}
        },
        {
            {"Dialect", dialectClass},
            {"reader", readerClass},
            {"writer", writerClass},
            {"DictReader", dictReaderClass},
            {"DictWriter", dictWriterClass},
            {"Sniffer", snifferClass}
        });
}

}
