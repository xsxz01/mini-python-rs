#include "../cffi.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 单个 doctest 示例
// One doctest example
// ᠨᠢᠭᠡᠴᠡ doctest ᠵᠢᠰᠢᠶ᠎ᠠ
struct Example{
    std::string source;
    std::string want;
    long long lineno = 0;
};

// 从文本提取示例
// Extract examples from text
// ᠲᠡᠺᠰᠲ ᠡᠴᠡ ᠵᠢᠰᠢᠶ᠎ᠡ ᠢ ᠭᠠᠷᠭᠠᠬᠤ
std::vector<Example> extractExamples(const std::string& text){
    std::vector<Example> out;
    std::istringstream in(text);
    std::string line;
    long long lineno = 0;
    while (std::getline(in, line)){
        lineno++;
        if (line.rfind(">>> ", 0) != 0) continue;
        Example ex;
        ex.lineno = lineno;
        ex.source = line.substr(4);
        // 续行 ("... ")
        // Continuation lines ("... ")
        // ᠦᠷᠭᠦᠳᠬᠡᠯ ᠮᠥᠷ
        std::streampos before;
        while (true){
            before = in.tellg();
            if (!std::getline(in, line)) break;
            if (line.rfind("... ", 0) == 0 || line == "..."){
                lineno++;
                ex.source += "\n" + (line.size() >= 4 ? line.substr(4) : "");
            } else break;
        }
        // 期望输出
        // Expected output
        // ᠬᠦᠯᠡᠡᠵᠡᠬᠦ ᠭᠠᠷᠤᠯᠭ᠎ᠠ
        std::string want;
        bool firstWant = true;
        while (true){
            std::string cur = line;
            bool haveLine = static_cast<bool>(in);
            if (!haveLine) break;
            if (cur.rfind(">>> ", 0) == 0){
                in.seekg(before);
                break;
            }
            if (cur.empty()){
                // 空行结束期望输出
                // Blank line ends expected output
                // ᠬᠤᠷᠤᠰᠤᠨ ᠮᠥᠷ ᠲᠥᠭᠦᠰᠬᠡᠬᠦ
                break;
            }
            if (!firstWant) want += "\n";
            want += cur;
            firstWant = false;
            before = in.tellg();
            if (!std::getline(in, line)) break;
        }
        if (!firstWant || true) ex.want = want;
        out.push_back(ex);
        // 若当前行是 ">>> " 则回退一行
        // If current line starts a new example, rewind one line
        // ᠰᠢᠨ᠎ᠡ ᠵᠢᠰᠢᠶ᠎ᠡ ᠡᠬᠢᠯᠡᠬᠦ
        if (in && line.rfind(">>> ", 0) == 0){
            in.seekg(before);
        }
    }
    return out;
}

// 规范化行尾空白
// Normalize trailing whitespace
// ᠮᠥᠷ ᠦᠨ ᠰᠡᠭᠦᠯ ᠦᠨ ᠬᠤᠷᠤᠰᠤᠨ ᠢ ᠠᠷᠢᠯᠭᠠᠬᠤ
std::string normalizeWs(const std::string& text){
    std::string out;
    bool pendingSpace = false;
    for (char c : text){
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            pendingSpace = true;
        } else {
            if (pendingSpace){ out += ' '; pendingSpace = false; }
            out += c;
        }
    }
    return out;
}

// 比较期望与实际 (支持 Traceback 特例)
// Compare expected vs actual (Traceback special case)
// ᠬᠦᠯᠡᠡᠵᠡᠬᠦ ᠪᠠ ᠪᠣᠳᠣᠲᠤ ᠶᠢ ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠬᠤ
bool outputMatches(const std::string& want, const std::string& got,
                   const std::string& excLine, long long flags){
    std::string wantNorm = want;
    std::string gotNorm = got;
    if (!wantNorm.empty() && wantNorm.back() == '\n') wantNorm.pop_back();
    if (!gotNorm.empty() && gotNorm.back() == '\n') gotNorm.pop_back();
    if ((flags & 4) != 0){ // NORMALIZE_WHITESPACE
        return normalizeWs(wantNorm) == normalizeWs(gotNorm);
    }
    if (wantNorm.rfind("Traceback (most recent call last):", 0) == 0){
        // 异常匹配: 比较最后一行
        // Exception match: compare the last line
        // ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠬᠤ
        if (excLine.empty()) return false;
        std::string wantLast = wantNorm;
        size_t nl = wantNorm.rfind('\n');
        if (nl != std::string::npos) wantLast = wantNorm.substr(nl + 1);
        return wantLast == excLine;
    }
    return wantNorm == gotNorm;
}

// 判断示例源码是否语句 (语句无 repr 输出)
// Whether example source is a statement (statements have no repr output)
// ᠮᠥᠷ ᠮᠥᠨ ᠦᠦ ᠭᠡᠳᠭᠢᠶᠡᠭ ᠰᠢᠯᠭᠠᠬᠤ
bool doctestIsStatement(const std::string& src){
    size_t b = src.find_first_not_of(" \t");
    if (b == std::string::npos) return true;
    std::string s = src.substr(b);
    static const char* kws[] = {
        "import", "from", "def", "class", "if", "for", "while", "with",
        "try", "return", "raise", "del", "pass", "break", "continue",
        "global", "nonlocal", "assert", "yield", "await",
        "elif", "else", "except", "finally"
    };
    for (auto* kw : kws){
        size_t n = std::strlen(kw);
        if (s.rfind(kw, 0) == 0 &&
            (s.size() == n || !(std::isalnum((unsigned char)s[n]) || s[n] == '_'))){
            return true;
        }
    }
    // 顶层赋值 (= / += / ...) 检测, 排除 ==, <=, >=, !=, :=
    // Top-level assignment detection, excluding comparisons and walrus
    // ᠳᠡᠭᠡᠳᠦ ᠲᠦᠪᠰᠢᠨ ᠦᠨ᠎ᠡ ᠣᠯᠭᠣᠯᠲᠠ ᠶᠢ ᠰᠢᠯᠭᠠᠬᠤ
    int depth = 0;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i){
        char c = s[i];
        if (quote){
            if (c == '\\'){ ++i; continue; }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\''){ quote = c; continue; }
        if (c == '(' || c == '[' || c == '{'){ depth++; continue; }
        if (c == ')' || c == ']' || c == '}'){ depth--; continue; }
        if (c == '#') break;
        if (c == '=' && depth == 0){
            char prev = i ? s[i - 1] : '\0';
            char next = (i + 1 < s.size()) ? s[i + 1] : '\0';
            if (next == '=' || prev == ':' || prev == '!' ||
                prev == '<' || prev == '>' || prev == '=') continue;
            return true;
        }
    }
    return false;
}

// TestResults 构造
// Build a TestResults value
// TestResults ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeTestResults(cffi& a, long long failed, long long attempted){
    auto classIt = a.library->classes.find("TestResults");
    value obj = a.vm->makeNativeObject(a.library, &classIt->second, nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["failed"] = a.mkint(failed);
    inst->attributes["attempted"] = a.mkint(attempted);
    return obj;
}

// TestResults __repr__
// TestResults __repr__
// TestResults __repr__
value testResultsRepr(cffi& a){
    return a.mkstr("TestResults(failed=" + a.vm->toString(a.attr("failed")) +
                   ", attempted=" + a.vm->toString(a.attr("attempted")) + ")");
}

// fn testmod: 无 docstring 体系, 恒为空测试
// fn testmod: no docstring system, always empty
// fn testmod: ᠬᠤᠷᠤᠰᠤᠨ
value fnTestmod(cffi& a){
    return makeTestResults(a, 0, 0);
}

// fn testfile: 解析文件并执行示例
// fn testfile: parse file and run examples
// fn testfile: ᠪᠢᠴᠢᠭ ᠢ ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
value fnTestfile(cffi& a){
    std::string filename = a.arg_str(0);
    long long flags = 0;
    if (a.has_kwarg("optionflags")) flags = a.kwarg_int("optionflags", 0);
    bool verbose = a.kwarg_bool("verbose", false);

    std::ifstream in(filename, std::ios::binary);
    if (!in){
        a.vm->raiseError("FileNotFoundError",
                         "[Errno 2] No such file or directory: '" + filename + "'");
        return a.none();
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();

    auto examples = extractExamples(text);
    long long attempted = 0, failed = 0;
    for (auto& ex : examples){
        if ((flags & 16) != 0) continue; // SKIP
        attempted++;
        // 捕获 stdout 执行
        // Run with captured stdout
        // stdout ᠢ ᠪᠠᠷᠢᠵᠤ ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
        std::ostringstream capture;
        auto* oldBuf = std::cout.rdbuf(capture.rdbuf());
        if (!doctestIsStatement(ex.source)){
            // 表达式: 打印 repr (交互式语义); 失败回退语句执行
            // Expression: print repr (interactive semantics); fallback to statement
            // ᠡᠮᠬᠢᠳᠬᠡᠯ: repr ᠢ ᠬᠡᠪᠯᠡᠨ᠎ᠡ; ᠠᠮᠵᠢᠯᠲᠠ ᠦᠭᠡᠢ ᠪᠣᠯ ᠮᠥᠷ ᠪᠡᠷ
            std::string wrapped = "_docres = (" + ex.source +
                                  ")\nif _docres is not None:\n    print(repr(_docres))";
            a.vm->execSource(wrapped);
            if (a.vm->hasError()){
                a.vm->clearErr();
                capture.str("");
                capture.clear();
                a.vm->execSource(ex.source);
            }
        } else {
            a.vm->execSource(ex.source);
        }
        std::cout.rdbuf(oldBuf);
        std::string got = capture.str();
        std::string excLine;
        if (a.vm->hasError()){
            excLine = a.vm->exceptionType() + ": " + a.vm->getLastErr();
            a.vm->clearErr();
        }
        bool ok = excLine.empty() &&
                  outputMatches(ex.want, got, excLine, flags);
        if (!excLine.empty()){
            ok = outputMatches(ex.want, got, excLine, flags);
        }
        if (ok){
            if (verbose){
                std::cout << "Trying:\n    " << ex.source << "\n";
                if (ex.want.empty()){
                    std::cout << "Expecting nothing\n";
                } else {
                    std::cout << "Expecting:\n";
                    std::istringstream wl(ex.want);
                    std::string l;
                    while (std::getline(wl, l)) std::cout << "    " << l << "\n";
                }
                std::cout << "ok\n";
            }
            continue;
        }
        failed++;
        // 失败报告
        // Failure report
        // ᠠᠯᠳᠠᠭ᠎ᠠ ᠶᠢᠨ ᠲᠤᠰᠢᠶ᠎ᠠ
        std::cout << "**********************************************************************\n";
        std::cout << "File \"" << filename << "\", line " << ex.lineno
                  << ", in " << filename << "\n";
        std::cout << "Failed example:\n";
        {
            std::istringstream sl(ex.source);
            std::string l;
            while (std::getline(sl, l)) std::cout << "    " << l << "\n";
        }
        std::cout << "Expected:\n";
        {
            std::istringstream wl(ex.want);
            std::string l;
            while (std::getline(wl, l)) std::cout << "    " << l << "\n";
        }
        std::cout << "Got:\n";
        if (!excLine.empty()){
            std::cout << "    " << excLine << "\n";
        } else {
            std::istringstream gl(got);
            std::string l;
            while (std::getline(gl, l)) std::cout << "    " << l << "\n";
        }
        std::cout << "**********************************************************************\n";
    }
    if (verbose){
        // 汇总块 (CPython 格式)
        // Summary block (CPython format)
        // ᠨᠡᠶᠢᠲᠡᠯᠡᠯ ᠪᠯᠣᠺ
        if (failed == 0){
            std::cout << "1 item passed all tests:\n";
            std::cout << "   " << attempted << " tests in " << filename << "\n";
        } else {
            std::cout << "**********************************************************************\n";
            std::cout << "1 item had failures:\n";
            std::cout << "   " << failed << " of " << attempted << " in " << filename << "\n";
        }
        std::cout << attempted << " tests in 1 item.\n";
        std::cout << (attempted - failed) << " passed";
        if (failed > 0) std::cout << " and " << failed << " failed";
        std::cout << ".\n";
        if (failed > 0){
            std::cout << "***Test Failed*** " << failed << " failures.\n";
        } else {
            std::cout << "Test passed.\n";
        }
    }
    return makeTestResults(a, failed, attempted);
}

// fn register_optionflag
// fn register_optionflag
// fn register_optionflag
value fnRegisterOptionflag(cffi& a){
    static long long nextFlag = 1 << 13;
    long long flag = nextFlag;
    nextFlag <<= 1;
    return a.mkint(flag);
}

// OutputChecker.check_output
// OutputChecker.check_output
// OutputChecker.check_output
value ocCheckOutput(cffi& a){
    std::string want = a.arg_str(0);
    std::string got = a.arg_str(1);
    long long flags = a.argc() > 2 ? a.arg_int(2) : 0;
    return a.mkbool(outputMatches(want, got, "", flags));
}

// OutputChecker.output_difference
// OutputChecker.output_difference
// OutputChecker.output_difference
value ocOutputDifference(cffi& a){
    return a.mkstr("");
}

value ocCtor(cffi& a){ return a.none(); }

// TestResults 构造器: TestResults(failed, attempted)
// TestResults constructor: TestResults(failed, attempted)
// TestResults ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ ᠠᠷᠭ᠎ᠠ
value testResultsCtor(cffi& a){
    long long failed = a.argc() > 0 ? a.arg_int(0) : 0;
    long long attempted = a.argc() > 1 ? a.arg_int(1) : 0;
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["failed"] = a.mkint(failed);
    inst->attributes["attempted"] = a.mkint(attempted);
    return obj;
}

} // namespace

// cffi doctest lib
// cffi doctest lib
// cffi doctest lib
ffilib cffi_doctest_lib(){
    ffi_class testResults = cffi_class(
        "TestResults", (void*)testResultsCtor,
        {
            {"__repr__", (void*)testResultsRepr}
        });
    ffi_class outputChecker = cffi_class(
        "OutputChecker", (void*)ocCtor,
        {
            {"check_output", (void*)ocCheckOutput},
            {"output_difference", (void*)ocOutputDifference}
        });

    return cffi_lib(
        "doctest",
        {
            {"testmod", (void*)fnTestmod},
            {"testfile", (void*)fnTestfile},
            {"register_optionflag", (void*)fnRegisterOptionflag}
        },
        {
            {"DONT_ACCEPT_TRUE_FOR_1", (void*)+[] (cffi& a) -> value { return a.mkint(1); }},
            {"DONT_ACCEPT_BLANKLINE", (void*)+[] (cffi& a) -> value { return a.mkint(2); }},
            {"NORMALIZE_WHITESPACE", (void*)+[] (cffi& a) -> value { return a.mkint(4); }},
            {"ELLIPSIS", (void*)+[] (cffi& a) -> value { return a.mkint(8); }},
            {"SKIP", (void*)+[] (cffi& a) -> value { return a.mkint(16); }},
            {"IGNORE_EXCEPTION_DETAIL", (void*)+[] (cffi& a) -> value { return a.mkint(32); }},
            {"REPORT_UDIFF", (void*)+[] (cffi& a) -> value { return a.mkint(64); }},
            {"REPORT_CDIFF", (void*)+[] (cffi& a) -> value { return a.mkint(128); }},
            {"REPORT_NDIFF", (void*)+[] (cffi& a) -> value { return a.mkint(256); }},
            {"REPORT_ONLY_FIRST_FAILURE", (void*)+[] (cffi& a) -> value { return a.mkint(512); }},
            {"FAIL_FAST", (void*)+[] (cffi& a) -> value { return a.mkint(1024); }},
            {"COMPARISON_FLAGS", (void*)+[] (cffi& a) -> value { return a.mkint(63); }},
            {"REPORTING_FLAGS", (void*)+[] (cffi& a) -> value { return a.mkint(960); }}
        },
        {
            {"TestResults", testResults},
            {"OutputChecker", outputChecker}
        });
}

} // namespace minipy
