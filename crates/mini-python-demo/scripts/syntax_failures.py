# MiniPython 当前失败项复现脚本。
# 每段单独运行于 try/except，避免一个运行时异常阻断后续复现。
# 目标是让 MiniPython 作者逐项确认；这些示例在标准 CPython 中应是合法语法。

print("MiniPython failed-syntax reproductions")

# 1. 生成器表达式：当前报 NameError: name 'x' is not defined。
try:
    generator_expression = (x for x in [1, 2])
    print("generator expression unexpectedly passed")
except Exception as error:
    print("FAIL generator expression:", error)

# 2. 变量注解：当前报 NameError，定位在注解目标变量处。
try:
    annotated_value: int = 1
    print("variable annotation unexpectedly passed")
except Exception as error:
    print("FAIL variable annotation:", error)

# 3. 一个 with 语句使用多个上下文项：当前报 NameError: name 'as' is not defined。
try:
    with open("syntax_failure_a.txt", "w") as first, open("syntax_failure_b.txt", "w") as second:
        first.write("a")
        second.write("b")
    print("multiple with-items unexpectedly passed")
except Exception as error:
    print("FAIL multiple with-items:", error)

# 4. 复数字面量：当前报 NameError: name '2j' is not defined。
try:
    complex_value = 1 + 2j
    print("complex literal unexpectedly passed")
except Exception as error:
    print("FAIL complex literal:", error)

# 5. 二进制整数：当前报 NameError: name '0b10' is not defined。
try:
    binary_value = 0b10
    print("binary integer unexpectedly passed")
except Exception as error:
    print("FAIL binary integer literal:", error)

# 6. Python 3.12+ 泛型函数类型参数：当前报 NameError: name 'T' is not defined。
try:
    def identity[T](value: T) -> T:
        return value
    print("generic function type parameter unexpectedly passed")
except Exception as error:
    print("FAIL generic function type parameter:", error)

# 7. except* 异常组：探针的 exec_main 返回成功，但后续完成标记没有执行。
# 保持最后一项，因为 MiniPython 当前可能在此处静默提前结束执行。
print("Testing except* (last probe; current runtime may stop here)")
try:
    try:
        raise ExceptionGroup("group", [ValueError("bad")])
    except* ValueError:
        pass
except Exception as error:
    print("FAIL except*:", error)
print("except* probe reached end")
