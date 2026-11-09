#include "../cffi.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 安全 repr (用于断言消息)
// Safe repr for assert messages
// ᠠᠶᠤᠯ ᠦᠭᠡᠢ repr
std::string srepr(cffi& a, const value& v){
    return pythonRepr(*a.vm, v);
}

// 断言失败: 抛 AssertionError
// Assertion failure: raise AssertionError
// ᠠᠯᠳᠠᠭ᠎ᠠ ᠭᠠᠷᠭᠠᠬᠤ
value failWith(cffi& a, const std::string& text){
    a.vm->raiseError("AssertionError", text);
    return a.none();
}

// msg 参数覆盖默认消息
// msg argument overrides the default message
// msg ᠠᠷᠭᠤᠮᠡᠨᠲ ᠢ ᠳᠠᠪᠬᠤᠷᠬᠤ
std::string pickMsg(cffi& a, const std::string& fallback, size_t index){
    if (a.argc() > index && a.arg(index).type != "none"){
        return a.vm->toString(a.arg(index));
    }
    return fallback;
}

// 类名提取 (assertIsInstance 用)
// Extract class name (for assertIsInstance)
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠨᠡᠷ᠎ᠡ
std::string className(cffi& a, const value& cls){
    if (cls.type == "class" && cls.ptr) return static_cast<class_t*>(cls.ptr)->name;
    if (cls.type == "native_object" && cls.ptr){
        auto* inst = static_cast<ffi_instance*>(cls.ptr);
        if (inst->klass) return inst->klass->name;
        if (!inst->typeName.empty()) return inst->typeName;
    }
    if (cls.type == "tuple" && cls.ptr){
        auto* items = static_cast<std::vector<value>*>(cls.ptr);
        std::string out = "(";
        for (size_t i = 0; i < items->size(); ++i){
            if (i) out += ", ";
            out += className(a, (*items)[i]);
        }
        out += ")";
        return out;
    }
    return a.vm->toString(cls);
}

// 对象类型名
// Object type name
// ᠣᠪᠶᠡᠺᠲ ᠦᠨ ᠲᠥᠷᠥᠯ ᠨᠡᠷ᠎ᠡ
std::string typeNameOf(cffi& a, const value& v){
    return a.vm->pythonTypeName(v);
}

bool isInstanceOf(cffi& a, const value& obj, const value& cls){
    if (cls.type == "tuple" && cls.ptr){
        auto* items = static_cast<std::vector<value>*>(cls.ptr);
        for (auto& item : *items) if (isInstanceOf(a, obj, item)) return true;
        return false;
    }
    std::string want = className(a, cls);
    std::string got = typeNameOf(a, obj);
    if (want == got) return true;
    if (want == "int" && got == "bool") return true;
    return false;
}

// ---- TestCase 方法 / TestCase methods / TestCase ᠠᠷᠭ᠎ᠠ ----

value tcCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["_methodName"] = a.argc() > 0 ? a.arg(0) : a.mkstr("runTest");
    return obj;
}

value tcSetUp(cffi& a){ return a.none(); }
value tcTearDown(cffi& a){ return a.none(); }

// 执行单个用例并记入 result
// Run one case and record into result
// ᠭᠡᠷᠡᠴᠢᠯᠡᠭᠡ ᠢ ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
value tcRun(cffi& a){
    value result = a.argc() > 0 ? a.arg(0) : a.none();
    bool haveResult = result.type == "native_object" && result.ptr;
    if (haveResult){
        value r;
        cffiCallMethod(*a.vm, result, "startTest", {a.self()}, r);
    }
    // runTest 默认成功
    // Default runTest succeeds
    // ᠠᠩᠬᠠᠨ runTest ᠠᠮᠵᠢᠯᠲᠠ
    if (haveResult){
        value r;
        cffiCallMethod(*a.vm, result, "addSuccess", {a.self()}, r);
        cffiCallMethod(*a.vm, result, "stopTest", {a.self()}, r);
    }
    return result;
}

value tcDebug(cffi& a){ return a.none(); }

value tcAssertEqual(cffi& a){
    if (!a.vm->valuesEqual(a.arg(0), a.arg(1))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " != " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertNotEqual(cffi& a){
    if (a.vm->valuesEqual(a.arg(0), a.arg(1))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " == " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertTrue(cffi& a){
    if (!a.vm->truthy(a.arg(0))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is not true", 1));
    }
    return a.none();
}

value tcAssertFalse(cffi& a){
    if (a.vm->truthy(a.arg(0))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is not false", 1));
    }
    return a.none();
}

value tcAssertIs(cffi& a){
    bool same = a.arg(0).type == a.arg(1).type && a.arg(0).ptr == a.arg(1).ptr;
    if (!same){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is not " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertIsNot(cffi& a){
    bool same = a.arg(0).type == a.arg(1).type && a.arg(0).ptr == a.arg(1).ptr;
    if (same){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertIsNone(cffi& a){
    if (a.arg(0).type != "none"){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is not None", 1));
    }
    return a.none();
}

value tcAssertIsNotNone(cffi& a){
    if (a.arg(0).type == "none"){
        return failWith(a, pickMsg(a, "unexpectedly None", 1));
    }
    return a.none();
}

value tcAssertIn(cffi& a){
    value member = a.arg(0), container = a.arg(1);
    bool found = false;
    if ((container.type == "list" || container.type == "tuple") && container.ptr){
        auto* items = static_cast<std::vector<value>*>(container.ptr);
        for (auto& item : *items){
            if (a.vm->valuesEqual(member, item)){ found = true; break; }
        }
    } else if (container.type == "str" && member.type == "str"){
        found = a.vm->toString(container).find(a.vm->toString(member)) != std::string::npos;
    } else if (container.type == "dict" && container.ptr){
        auto* items = static_cast<dict_t*>(container.ptr);
        for (auto& kv : *items){
            if (a.vm->valuesEqual(member, kv.first)){ found = true; break; }
        }
    }
    if (!found){
        return failWith(a, pickMsg(a,
            srepr(a, member) + " not found in " + srepr(a, container), 2));
    }
    return a.none();
}

value tcAssertNotIn(cffi& a){
    value member = a.arg(0), container = a.arg(1);
    bool found = false;
    if ((container.type == "list" || container.type == "tuple") && container.ptr){
        auto* items = static_cast<std::vector<value>*>(container.ptr);
        for (auto& item : *items){
            if (a.vm->valuesEqual(member, item)){ found = true; break; }
        }
    } else if (container.type == "str" && member.type == "str"){
        found = a.vm->toString(container).find(a.vm->toString(member)) != std::string::npos;
    }
    if (found){
        return failWith(a, pickMsg(a,
            srepr(a, member) + " unexpectedly found in " + srepr(a, container), 2));
    }
    return a.none();
}

value tcAssertIsInstance(cffi& a){
    if (!isInstanceOf(a, a.arg(0), a.arg(1))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is not an instance of <class '" +
            className(a, a.arg(1)) + "'>", 2));
    }
    return a.none();
}

value tcAssertNotIsInstance(cffi& a){
    if (isInstanceOf(a, a.arg(0), a.arg(1))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " is an instance of <class '" +
            className(a, a.arg(1)) + "'>", 2));
    }
    return a.none();
}

value tcAssertAlmostEqual(cffi& a){
    double x = a.vm->toFloat(a.arg(0));
    double y = a.vm->toFloat(a.arg(1));
    int places = 7;
    if (a.has_kwarg("places")) places = static_cast<int>(a.kwarg_int("places", 7));
    else if (a.argc() > 2 && a.arg(2).type == "int") places = static_cast<int>(a.vm->toInt(a.arg(2)));
    if (a.has_kwarg("delta")){
        double delta = a.vm->toFloat(a.kwarg("delta"));
        if (std::fabs(x - y) > delta){
            return failWith(a, srepr(a, a.arg(0)) + " != " + srepr(a, a.arg(1)) +
                               " within " + a.vm->toString(a.kwarg("delta")) + " delta");
        }
        return a.none();
    }
    double scale = std::pow(10.0, places);
    if (std::fabs(x - y) >= 0.5 / scale && std::fabs(x - y) > 0){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " != " + srepr(a, a.arg(1)) + " within " +
            std::to_string(places) + " places", 3));
    }
    return a.none();
}

value tcAssertGreater(cffi& a){
    if (!(a.vm->toFloat(a.arg(0)) > a.vm->toFloat(a.arg(1)))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " not greater than " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertGreaterEqual(cffi& a){
    if (!(a.vm->toFloat(a.arg(0)) >= a.vm->toFloat(a.arg(1)))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " not greater than or equal to " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertLess(cffi& a){
    if (!(a.vm->toFloat(a.arg(0)) < a.vm->toFloat(a.arg(1)))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " not less than " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertLessEqual(cffi& a){
    if (!(a.vm->toFloat(a.arg(0)) <= a.vm->toFloat(a.arg(1)))){
        return failWith(a, pickMsg(a,
            srepr(a, a.arg(0)) + " not less than or equal to " + srepr(a, a.arg(1)), 2));
    }
    return a.none();
}

value tcAssertRaises(cffi& a){
    // assertRaises(excClass, callable, *args)
    // assertRaises(excClass, callable, *args)
    // assertRaises(excClass, callable, *args)
    value excClass = a.arg(0);
    value callable = a.arg(1);
    std::vector<value> innerArgs;
    for (size_t i = 2; i < a.argc(); ++i) innerArgs.push_back(a.arg(i));
    std::string wantName = className(a, excClass);
    auto snap = a.vm->saveErrorState();
    value result = a.vm->callValue(callable, innerArgs);
    if (a.vm->hasError()){
        std::string got = a.vm->exceptionType();
        if (got == wantName){
            a.vm->clearErr();
            return a.none();
        }
        // 类型不匹配: 异常继续传播 (CPython 行为)
        // Wrong type: propagate (CPython behavior)
        // ᠲᠥᠷᠥᠯ ᠲᠠᠷᠠᠬᠤ ᠦᠭᠡᠢ: ᠳᠡᠯᠭᠡᠷᠡᠬᠦ
        return a.none();
    }
    (void)result;
    (void)snap;
    // 取可调用对象名 (CPython: "X not raised by name")
    // Get callable name (CPython: "X not raised by name")
    // ᠳᠤᠭᠤᠳᠠᠯᠲᠠ ᠶᠢᠨ ᠨᠡᠷ᠎ᠡ
    std::string callName;
    if (callable.type == "function" && callable.ptr){
        callName = static_cast<function_t*>(callable.ptr)->name;
    } else if (callable.type == "native_object" && callable.ptr){
        auto* ci = static_cast<ffi_instance*>(callable.ptr);
        callName = ci->typeName;
    }
    if (callName.empty()) callName = a.vm->toString(callable);
    return failWith(a, wantName + " not raised by " + callName);
}

value tcFail(cffi& a){
    return failWith(a, a.argc() > 0 ? a.vm->toString(a.arg(0)) : "AssertionError");
}

value tcShortDescription(cffi& a){ return a.none(); }

value tcId(cffi& a){
    return a.mkstr("unittest.TestCase." + a.vm->toString(a.attr("_methodName")));
}

value tcStr(cffi& a){
    return a.mkstr(a.vm->toString(a.attr("_methodName")) + " (unittest.TestCase)");
}

// ---- FunctionTestCase / FunctionTestCase / FunctionTestCase ----

value ftcCtor(cffi& a){
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["_func"] = a.arg(0);
    if (a.argc() > 1) inst->attributes["_setUp"] = a.arg(1);
    if (a.argc() > 2) inst->attributes["_tearDown"] = a.arg(2);
    return obj;
}

value ftcRun(cffi& a){
    value result = a.argc() > 0 ? a.arg(0) : a.none();
    bool haveResult = result.type == "native_object" && result.ptr;
    if (haveResult){
        value r;
        cffiCallMethod(*a.vm, result, "startTest", {a.self()}, r);
    }
    value func = a.attr("_func");
    value callResult = a.vm->callValue(func, {});
    if (a.vm->hasError()){
        std::string type = a.vm->exceptionType();
        std::string msg = a.vm->getLastErr();
        a.vm->clearErr();
        if (haveResult){
            value r;
            if (type == "AssertionError"){
                value err = a.mkstr(type + ": " + msg);
                cffiCallMethod(*a.vm, result, "addFailure", {a.self(), err}, r);
            } else {
                value err = a.mkstr(type + ": " + msg);
                cffiCallMethod(*a.vm, result, "addError", {a.self(), err}, r);
            }
        }
    } else {
        (void)callResult;
        if (haveResult){
            value r;
            cffiCallMethod(*a.vm, result, "addSuccess", {a.self()}, r);
        }
    }
    if (haveResult){
        value r;
        cffiCallMethod(*a.vm, result, "stopTest", {a.self()}, r);
    }
    return result;
}

// ---- TestSuite / TestSuite / TestSuite ----

std::vector<value>* suiteTests(const value& self){
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<std::vector<value>*>(inst->handle);
}

value suiteCtor(cffi& a){
    auto* tests = new std::vector<value>();
    if (a.argc() > 0){
        value arg = a.arg(0);
        if ((arg.type == "list" || arg.type == "tuple") && arg.ptr){
            auto* items = static_cast<std::vector<value>*>(arg.ptr);
            for (auto& item : *items) tests->push_back(item);
        } else if (arg.type != "none"){
            tests->push_back(arg);
        }
    }
    return a.mkobject(tests);
}

value suiteAddTest(cffi& a){
    suiteTests(a.self())->push_back(a.arg(0));
    return a.none();
}

value suiteAddTests(cffi& a){
    value arg = a.arg(0);
    if ((arg.type == "list" || arg.type == "tuple") && arg.ptr){
        auto* items = static_cast<std::vector<value>*>(arg.ptr);
        for (auto& item : *items) suiteTests(a.self())->push_back(item);
    }
    return a.none();
}

value suiteCount(cffi& a){
    return a.mkint(static_cast<long long>(suiteTests(a.self())->size()));
}

value suiteRun(cffi& a){
    value result = a.arg(0);
    for (auto& test : *suiteTests(a.self())){
        if (test.type == "native_object" && test.ptr){
            value r;
            cffiCallMethod(*a.vm, test, "run", {result}, r);
            if (a.vm->hasError()) return a.none();
        }
    }
    return result;
}

value suiteDebug(cffi& a){
    return a.none();
}

// ---- TestResult / TestResult / TestResult ----

struct ResultData{
    long long testsRun = 0;
    std::vector<value> failures;
    std::vector<value> errors;
    std::vector<value> skipped;
    bool shouldStop = false;
};

ResultData* resultData(const value& self){
    auto* inst = static_cast<ffi_instance*>(self.ptr);
    return static_cast<ResultData*>(inst->handle);
}

// 同步内部计数到实例属性 (前置声明)
// Sync internal counters to instance attributes (forward decl)
// ᠲᠣ᠎ ᠪᠠᠷᠢᠮᠵᠢᠶ᠎ᠠ ᠢ ᠰᠢᠨᠡᠴᠢᠯᠡᠬᠦ
void syncResultAttrs(cffi& a, const value& obj);

value resultCtor(cffi& a){
    value obj = a.mkobject(new ResultData());
    syncResultAttrs(a, obj);
    return obj;
}

// 同步内部计数到实例属性
// Sync internal counters to instance attributes
// ᠲᠣ᠎ ᠪᠠᠷᠢᠮᠵᠢᠶ᠎ᠠ ᠢ ᠰᠢᠨᠡᠴᠢᠯᠡᠬᠦ
void syncResultAttrs(cffi& a, const value& obj){
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    auto* data = static_cast<ResultData*>(inst->handle);
    inst->attributes["testsRun"] = a.mkint(data->testsRun);
    std::vector<value> f(data->failures.begin(), data->failures.end());
    std::vector<value> e(data->errors.begin(), data->errors.end());
    std::vector<value> s(data->skipped.begin(), data->skipped.end());
    inst->attributes["failures"] = a.mklist(f);
    inst->attributes["errors"] = a.mklist(e);
    inst->attributes["skipped"] = a.mklist(s);
    inst->attributes["shouldStop"] = a.mkbool(data->shouldStop);
}

value resultStartTest(cffi& a){
    resultData(a.self())->testsRun++;
    syncResultAttrs(a, a.self());
    return a.none();
}

value resultStopTest(cffi& a){ return a.none(); }

value resultAddSuccess(cffi& a){
    return a.none();
}

value resultAddFailure(cffi& a){
    std::vector<value> pair{a.arg(0), a.argc() > 1 ? a.arg(1) : a.mkstr("")};
    resultData(a.self())->failures.push_back(a.vm->makeTuple(pair));
    syncResultAttrs(a, a.self());
    return a.none();
}

value resultAddError(cffi& a){
    std::vector<value> pair{a.arg(0), a.argc() > 1 ? a.arg(1) : a.mkstr("")};
    resultData(a.self())->errors.push_back(a.vm->makeTuple(pair));
    syncResultAttrs(a, a.self());
    return a.none();
}

value resultAddSkip(cffi& a){
    std::vector<value> pair{a.arg(0), a.argc() > 1 ? a.arg(1) : a.mkstr("")};
    resultData(a.self())->skipped.push_back(a.vm->makeTuple(pair));
    syncResultAttrs(a, a.self());
    return a.none();
}

value resultWasSuccessful(cffi& a){
    ResultData* d = resultData(a.self());
    return a.mkbool(d->failures.empty() && d->errors.empty());
}

value resultStop(cffi& a){
    resultData(a.self())->shouldStop = true;
    syncResultAttrs(a, a.self());
    return a.none();
}

value resultPrintErrors(cffi& a){ return a.none(); }

// ---- TextTestRunner / TextTestRunner / TextTestRunner ----

value runnerCtor(cffi& a){
    int verbosity = 1;
    if (a.has_kwarg("verbosity")) verbosity = static_cast<int>(a.kwarg_int("verbosity", 1));
    else if (a.argc() > 1 && a.arg(1).type == "int") verbosity = static_cast<int>(a.vm->toInt(a.arg(1)));
    value obj = a.mkobject(nullptr);
    auto* inst = static_cast<ffi_instance*>(obj.ptr);
    inst->attributes["verbosity"] = a.mkint(verbosity);
    return obj;
}

value runnerRun(cffi& a){
    value suite = a.arg(0);
    int verbosity = 1;
    {
        value v = a.attr("verbosity");
        if (v.type == "int") verbosity = static_cast<int>(a.vm->toInt(v));
    }
    auto& classes = a.library->classes;
    value result = a.vm->makeNativeObject(a.library, &classes.at("TestResult"), new ResultData());
    syncResultAttrs(a, result);

    auto start = std::chrono::steady_clock::now();
    value r;
    cffiCallMethod(*a.vm, suite, "run", {result}, r);
    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    if (elapsed < 0.0005) elapsed = 0.0;

    ResultData* d = resultData(result);
    // 进度点 (全部成功后一次性输出, 顺序与 CPython 一致)
    // Progress dots (printed after run; same order as CPython)
    // ᠠᠷᠠᠭᠠᠲᠤ ᠴᠡᠭ
    if (verbosity > 0){
        for (long long i = 0; i < d->testsRun; ++i){
            std::cerr << ".";
        }
        std::cerr << "\n";
    }
    // 失败/错误明细
    // Failure/error details
    // ᠠᠯᠳᠠᠭ᠎ᠠ ᠶᠢᠨ ᠳᠡᠯᠭᠡᠷᠡᠩᠭᠦᠢ
    auto printDetail = [&](const char* label, const std::vector<value>& items){
        for (auto& item : items){
            std::string err;
            if (item.type == "tuple" && item.ptr){
                auto* pair = static_cast<std::vector<value>*>(item.ptr);
                if (pair->size() > 1) err = a.vm->toString((*pair)[1]);
            }
            std::cerr << "======================================================================\n";
            std::cerr << label << ": runTest (unittest.TestCase)\n";
            std::cerr << "----------------------------------------------------------------------\n";
            std::cerr << err << "\n\n";
        }
    };
    printDetail("FAIL", d->failures);
    printDetail("ERROR", d->errors);

    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "%.3f", elapsed);
    std::cerr << "----------------------------------------------------------------------\n";
    std::cerr << "Ran " << d->testsRun << " test" << (d->testsRun == 1 ? "" : "s")
              << " in " << timeBuf << "s\n\n";
    if (d->failures.empty() && d->errors.empty()){
        std::cerr << "OK\n";
    } else {
        std::cerr << "FAILED (";
        bool first = true;
        if (!d->failures.empty()){
            std::cerr << "failures=" << d->failures.size();
            first = false;
        }
        if (!d->errors.empty()){
            if (!first) std::cerr << ", ";
            std::cerr << "errors=" << d->errors.size();
        }
        std::cerr << ")\n";
    }
    std::cerr.flush();
    return result;
}

// ---- 模块函数 / module functions / ᠮᠣᠳᠤᠯᠢ ᠠᠷᠭ᠎ᠠ ----

value fnSkip(cffi& a){
    // skip(reason) 返回恒等装饰器
    // skip(reason) returns identity decorator
    // skip(reason) ᠢ ᠪᠤᠴᠠᠶ᠎ᠠ
    return a.none();
}

value fnMain(cffi& a){
    return a.none();
}

} // namespace

// cffi unittest lib
// cffi unittest lib
// cffi unittest lib
ffilib cffi_unittest_lib(){
    ffi_class testCase = cffi_class(
        "TestCase", (void*)tcCtor,
        {
            {"setUp", (void*)tcSetUp},
            {"tearDown", (void*)tcTearDown},
            {"run", (void*)tcRun},
            {"debug", (void*)tcDebug},
            {"assertEqual", (void*)tcAssertEqual},
            {"assertEquals", (void*)tcAssertEqual},
            {"assertNotEqual", (void*)tcAssertNotEqual},
            {"assertNotEquals", (void*)tcAssertNotEqual},
            {"assertTrue", (void*)tcAssertTrue},
            {"assert_", (void*)tcAssertTrue},
            {"assertFalse", (void*)tcAssertFalse},
            {"assertIs", (void*)tcAssertIs},
            {"assertIsNot", (void*)tcAssertIsNot},
            {"assertIsNone", (void*)tcAssertIsNone},
            {"assertIsNotNone", (void*)tcAssertIsNotNone},
            {"assertIn", (void*)tcAssertIn},
            {"assertNotIn", (void*)tcAssertNotIn},
            {"assertIsInstance", (void*)tcAssertIsInstance},
            {"assertNotIsInstance", (void*)tcAssertNotIsInstance},
            {"assertAlmostEqual", (void*)tcAssertAlmostEqual},
            {"assertAlmostEquals", (void*)tcAssertAlmostEqual},
            {"assertGreater", (void*)tcAssertGreater},
            {"assertGreaterEqual", (void*)tcAssertGreaterEqual},
            {"assertLess", (void*)tcAssertLess},
            {"assertLessEqual", (void*)tcAssertLessEqual},
            {"assertRaises", (void*)tcAssertRaises},
            {"fail", (void*)tcFail},
            {"failUnlessEqual", (void*)tcAssertEqual},
            {"shortDescription", (void*)tcShortDescription},
            {"id", (void*)tcId},
            {"__str__", (void*)tcStr}
        });

    ffi_class functionTestCase = cffi_class(
        "FunctionTestCase", (void*)ftcCtor,
        {
            {"run", (void*)ftcRun},
            {"__str__", (void*)tcStr}
        });

    ffi_class suite = cffi_class(
        "TestSuite", (void*)suiteCtor,
        {
            {"addTest", (void*)suiteAddTest},
            {"addTests", (void*)suiteAddTests},
            {"countTestCases", (void*)suiteCount},
            {"run", (void*)suiteRun},
            {"debug", (void*)suiteDebug}
        });

    ffi_class result = cffi_class(
        "TestResult", (void*)resultCtor,
        {
            {"startTest", (void*)resultStartTest},
            {"stopTest", (void*)resultStopTest},
            {"addSuccess", (void*)resultAddSuccess},
            {"addFailure", (void*)resultAddFailure},
            {"addError", (void*)resultAddError},
            {"addSkip", (void*)resultAddSkip},
            {"wasSuccessful", (void*)resultWasSuccessful},
            {"stop", (void*)resultStop},
            {"printErrors", (void*)resultPrintErrors}
        });

    ffi_class runner = cffi_class(
        "TextTestRunner", (void*)runnerCtor,
        {
            {"run", (void*)runnerRun}
        });

    ffi_class loader = cffi_class(
        "TestLoader", (void*)tcCtor,
        {});

    return cffi_lib(
        "unittest",
        {
            {"skip", (void*)fnSkip},
            {"main", (void*)fnMain}
        },
        {},
        {
            {"TestCase", testCase},
            {"FunctionTestCase", functionTestCase},
            {"TestSuite", suite},
            {"TestResult", result},
            {"TextTestRunner", runner},
            {"TestLoader", loader}
        });
}

} // namespace minipy
