# MiniPython 作者反馈用：将每个示例单独复制到解释器中运行。
# 不要一次取消所有注释后执行：一项失败会中断当前脚本，使后续项无法被测到。
# 所有示例在标准 Python 3 中均为合法语法；错误摘要来自 MiniPython 的实测。

# 1. 生成器表达式
# generator_expression = (x for x in [1, 2])
# MiniPython: NameError: name 'x' is not defined

# 2. 变量类型注解
# annotated_value: int = 1
# MiniPython: NameError: name 'annotated_value' is not defined

# 3. 一个 with 语句中的多个上下文项
# with open("first.txt", "w") as first, open("second.txt", "w") as second:
#     first.write("a")
#     second.write("b")
# MiniPython: NameError: name 'as' is not defined

# 4. 复数字面量
# complex_value = 1 + 2j
# MiniPython: NameError: name '2j' is not defined

# 5. 二进制整数
# binary_value = 0b10
# MiniPython: NameError: name '0b10' is not defined

# 6. Python 3.12+ 泛型函数类型参数
# def identity[T](value: T) -> T:
#     return value
# MiniPython: NameError: name 'T' is not defined

# 7. Python 3.11+ 异常组处理
# try:
#     raise ExceptionGroup("group", [ValueError("bad")])
# except* ValueError:
#     pass
# MiniPython: exec_main returned success, but the end marker after this block
# was not executed; treat this as silent early termination.
