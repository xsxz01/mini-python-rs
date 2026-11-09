#include "../cffi.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 取得异常的显示类型名
// Get the display type name of an exception
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠲᠥᠷᠥᠯ ᠨᠡᠷ᠎ᠡ
std::string excTypeName(cffi& a, const value& exc){
    if (exc.type == "str") return a.vm->toString(exc);
    if (exc.type == "exception" && exc.ptr){
        auto* payload = static_cast<ExceptionPayload*>(exc.ptr);
        return payload->type;
    }
    if (exc.type == "class" && exc.ptr){
        return static_cast<class_t*>(exc.ptr)->name;
    }
    if (exc.type == "native_object" && exc.ptr){
        auto* inst = static_cast<ffi_instance*>(exc.ptr);
        if (inst->typeName == "exception" && inst->handle){
            return static_cast<ExceptionPayload*>(inst->handle)->type;
        }
        if (inst->klass) return inst->klass->name;
    }
    return "NoneType";
}

// 取得异常的消息文本
// Get the message text of an exception
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠮᠡᠰᠰᠡᠵᠢ
std::string excMessage(cffi& a, const value& exc){
    if (exc.type == "exception" && exc.ptr){
        auto* payload = static_cast<ExceptionPayload*>(exc.ptr);
        return payload->msg;
    }
    if (exc.type == "native_object" && exc.ptr){
        auto* inst = static_cast<ffi_instance*>(exc.ptr);
        if (inst->typeName == "exception" && inst->handle){
            return static_cast<ExceptionPayload*>(inst->handle)->msg;
        }
    }
    if (exc.type == "none") return "";
    return a.vm->toString(exc);
}

// 组装 "Type: msg\n" 形式的异常行列表
// Build the list of exception lines like "Type: msg\n"
// "Type: msg\n" ᠬᠡᠯᠪᠡᠷᠢ ᠶᠢᠨ ᠪᠢᠴᠢᠭ
std::vector<std::string> exceptionLines(cffi& a, const std::string& type,
                                        const std::string& msg){
    std::vector<std::string> lines;
    if (msg.empty()){
        lines.push_back(type + "\n");
    } else {
        lines.push_back(type + ": " + msg + "\n");
    }
    return lines;
}

// 解析 format_exception 的新式/旧式参数
// Parse new-style / old-style args of format_exception
// format_exception ᠤᠨ ᠠᠷᠭᠤᠮᠡᠨᠲ ᠢ ᠰᠢᠨᠵᠢᠯᠡᠬᠦ
bool parseExceptionArgs(cffi& a, std::string& type, std::string& msg){
    if (a.argc() >= 3 || (a.argc() == 2 && a.arg(0).type == "class")){
        type = excTypeName(a, a.arg(0));
        msg = excMessage(a, a.arg(1));
        return true;
    }
    value exc = a.argc() > 0 ? a.arg(0) : a.none();
    type = excTypeName(a, exc);
    msg = excMessage(a, exc);
    return true;
}

value listOfLines(cffi& a, const std::vector<std::string>& lines){
    std::vector<value> items;
    for (auto& line : lines) items.push_back(a.mkstr(line));
    return a.mklist(items);
}

// fn format_exception_only
// fn format_exception_only
// fn format_exception_only
value fnFormatExceptionOnly(cffi& a){
    std::string type, msg;
    parseExceptionArgs(a, type, msg);
    return listOfLines(a, exceptionLines(a, type, msg));
}

// fn format_exception
// fn format_exception
// fn format_exception
value fnFormatException(cffi& a){
    std::string type, msg;
    parseExceptionArgs(a, type, msg);
    return listOfLines(a, exceptionLines(a, type, msg));
}

// fn print_exception
// fn print_exception
// fn print_exception
value fnPrintException(cffi& a){
    std::string type, msg;
    parseExceptionArgs(a, type, msg);
    for (auto& line : exceptionLines(a, type, msg)) std::cerr << line;
    std::cerr.flush();
    return a.none();
}

// fn format_exc
// fn format_exc
// fn format_exc
value fnFormatExc(cffi& a){
    if (a.vm->hasError()){
        return a.mkstr(a.vm->exceptionType() + ": " + a.vm->getLastErr() + "\n");
    }
    return a.mkstr("NoneType: None\n");
}

// fn print_exc
// fn print_exc
// fn print_exc
value fnPrintExc(cffi& a){
    if (a.vm->hasError()){
        std::cerr << a.vm->exceptionType() << ": " << a.vm->getLastErr() << "\n";
    } else {
        std::cerr << "NoneType: None\n";
    }
    std::cerr.flush();
    return a.none();
}

// fn format_tb
// fn format_tb
// fn format_tb
value fnFormatTb(cffi& a){
    return a.mklist({});
}

// fn extract_tb
// fn extract_tb
// fn extract_tb
value fnExtractTb(cffi& a){
    return a.mklist({});
}

// fn print_tb
// fn print_tb
// fn print_tb
value fnPrintTb(cffi& a){
    return a.none();
}

// fn format_stack
// fn format_stack
// fn format_stack
value fnFormatStack(cffi& a){
    return a.mklist({});
}

// fn extract_stack
// fn extract_stack
// fn extract_stack
value fnExtractStack(cffi& a){
    return a.mklist({});
}

// fn print_stack
// fn print_stack
// fn print_stack
value fnPrintStack(cffi& a){
    return a.none();
}

// fn print_last
// fn print_last
// fn print_last
value fnPrintLast(cffi& a){
    if (a.vm->hasError()){
        std::cerr << a.vm->exceptionType() << ": " << a.vm->getLastErr() << "\n";
        std::cerr.flush();
    }
    return a.none();
}

// FrameSummary 构造器
// FrameSummary constructor
// FrameSummary ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value frameSummaryCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["filename"] = a.arg(0);
    inst->attributes["lineno"] = a.arg(1);
    inst->attributes["name"] = a.arg(2);
    if (a.argc() > 3) inst->attributes["line"] = a.arg(3);
    else inst->attributes["line"] = a.mkstr("");
    return obj;
}

// FrameSummary __repr__
// FrameSummary __repr__
// FrameSummary __repr__
value frameSummaryRepr(cffi& a){
    std::string out = "<FrameSummary file " + a.vm->toString(a.attr("filename")) +
                      ", line " + a.vm->toString(a.attr("lineno")) +
                      " in " + a.vm->toString(a.attr("name")) + ">";
    return a.mkstr(out);
}

// TracebackException 构造器
// TracebackException constructor
// TracebackException ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value teCtor(cffi& a){
    std::string type, msg;
    parseExceptionArgs(a, type, msg);
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["exc_type"] = a.mkstr(type);
    inst->attributes["_msg"] = a.mkstr(msg);
    inst->attributes["__cause__"] = a.none();
    inst->attributes["__context__"] = a.none();
    inst->attributes["__suppress_context__"] = a.mkbool(false);
    return obj;
}

// TracebackException.format
// TracebackException.format
// TracebackException.format
value teFormat(cffi& a){
    std::string type = a.vm->toString(a.attr("exc_type"));
    std::string msg = a.vm->toString(a.attr("_msg"));
    return listOfLines(a, exceptionLines(a, type, msg));
}

// TracebackException.format_exception_only
// TracebackException.format_exception_only
// TracebackException.format_exception_only
value teFormatExceptionOnly(cffi& a){
    return teFormat(a);
}

// TracebackException __str__
// TracebackException __str__
// TracebackException __str__
value teStr(cffi& a){
    std::string msg = a.vm->toString(a.attr("_msg"));
    return a.mkstr(msg);
}

// StackSummary 构造器
// StackSummary constructor
// StackSummary ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value stackSummaryCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["frames"] = a.mklist({});
    return obj;
}

// StackSummary.extract (类方法, 返回空 StackSummary)
// StackSummary.extract (class method, returns empty StackSummary)
// StackSummary.extract
value stackSummaryExtract(cffi& a){
    return a.mkobject(nullptr);
}

// StackSummary.format
// StackSummary.format
// StackSummary.format
value stackSummaryFormat(cffi& a){
    return a.mklist({});
}

} // namespace

// cffi traceback lib
// cffi traceback lib
// cffi traceback lib
ffilib cffi_traceback_lib(){
    ffi_class frameSummary = cffi_class(
        "FrameSummary", (void*)frameSummaryCtor,
        {
            {"__repr__", (void*)frameSummaryRepr},
            {"__str__", (void*)frameSummaryRepr}
        });
    ffi_class teClass = cffi_class(
        "TracebackException", (void*)teCtor,
        {
            {"format", (void*)teFormat},
            {"format_exception_only", (void*)teFormatExceptionOnly},
            {"__str__", (void*)teStr}
        });
    ffi_class stackSummary = cffi_class(
        "StackSummary", (void*)stackSummaryCtor,
        {
            {"extract", (void*)stackSummaryExtract},
            {"format", (void*)stackSummaryFormat}
        });

    return cffi_lib(
        "traceback",
        {
            {"format_exception", (void*)fnFormatException},
            {"format_exception_only", (void*)fnFormatExceptionOnly},
            {"print_exception", (void*)fnPrintException},
            {"format_exc", (void*)fnFormatExc},
            {"print_exc", (void*)fnPrintExc},
            {"print_last", (void*)fnPrintLast},
            {"format_tb", (void*)fnFormatTb},
            {"extract_tb", (void*)fnExtractTb},
            {"print_tb", (void*)fnPrintTb},
            {"format_stack", (void*)fnFormatStack},
            {"extract_stack", (void*)fnExtractStack},
            {"print_stack", (void*)fnPrintStack}
        },
        {},
        {
            {"FrameSummary", frameSummary},
            {"TracebackException", teClass},
            {"StackSummary", stackSummary}
        });
}

} // namespace minipy
