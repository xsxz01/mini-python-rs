# MiniPython 作者反馈用：将每个示例单独复制到解释器中运行。
# 不要一次取消所有注释后执行：一项失败会中断当前脚本，使后续项无法被测到。
# 所有示例在 CPython 3.14 中均为合法语法（已用 Python 3.14.0 的 compile() 校验）；
# 错误摘要来自 MiniPython 实测，完整统计见 SYNTAX.md，逐项说明见 FAILED_SYNTAX.md。
# 基线：https://docs.python.org/3.14/reference/grammar.html

# ======================================================================
# 1. 括号元组与列表解包目标
# ======================================================================

# ---- assign/tuple-target：括号元组赋值目标
# (left, right) = (1, 2)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       (left, right) = (1, 2)
#                       ~^~~~~
#   ValueError: too many values to unpack (expected 1)


# ---- assign/list-target：列表赋值目标
# [b, c] = [1, 2]
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       [b, c] = [1, 2]
#        ^
#   NameError: name 'b' is not defined


# ---- assign/nested-unpack：嵌套解包目标
# (a, (b, c)) = (1, (2, 3))
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       (a, (b, c)) = (1, (2, 3))
#                     ~^~~~~~~~~~
#   ValueError: too many values to unpack (expected 1)


# ---- expr/target-tuple-of-attributes：属性组成的元组目标
# class Holder:
#     pass
# first = Holder()
# second = Holder()
# (first.item, second.item) = (1, 2)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 5, in <module>
#       (first.item, second.item) = (1, 2)
#                                   ~^~~~~
#   ValueError: too many values to unpack (expected 1)


# ======================================================================
# 2. 括号单目标标注
# ======================================================================

# ---- assign/annotated-paren：括号单目标标注
# (name): int = 1
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       (name): int = 1
#        ^^^^
#   NameError: name 'name' is not defined


# ======================================================================
# 3. from-import 括号列表与星号导入
# ======================================================================

# ---- import/from-parens：from import 括号列表
# from math import (sqrt as root,)
# value = root(4)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       from math import (sqrt as root,)
#                         ^^^^
#   NameError: name 'sqrt' is not defined


# ---- import/from-star：from import *
# from math import *
# value = sqrt(4)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       value = sqrt(4)
#               ^^^^
#   NameError: name 'sqrt' is not defined


# ======================================================================
# 4. PEP 695 类型参数
# ======================================================================

# ---- type/alias-params：带类型参数的类型别名
# type Pair[T] = list
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       type Pair[T] = list
#                      ^^^^
#   NameError: name 'Pair' is not defined


# ---- type/class-params：类类型参数
# class Container[T: int]:
#     pass
# MiniPython 实测：
#   未返回异常，但完成标记未执行；解释器静默提前终止。


# ---- type/alias-starred-default：类型参数默认值语法
# type Bucket[T = int, *Ts = *tuple[int, ...]] = tuple[T, *Ts]
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       type Bucket[T = int, *Ts = *tuple[int, ...]] = tuple[T, *Ts]
#                                                      ~~~~~^^^
#   TypeError: 'native_object' object is not subscriptable


# ======================================================================
# 5. 括号包裹的 with 上下文
# ======================================================================

# ---- compound/with-parens：括号包裹的上下文列表
# with (open('syntax_with_a.txt', 'w') as first, open('syntax_with_b.txt', 'w') as second):
#     first.write('a')
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       with (open('syntax_with_a.txt', 'w') as first, open('syntax_with_b.txt', 'w') as second):
#                                                                                     ^^
#   NameError: name 'as' is not defined. Did you mean: 'abs'?


# ---- compound/with-parens-single：括号包裹的单个上下文项
# with (open('syntax_with_b.txt', 'w') as handle):
#     handle.write('x')
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       handle.write('x')
#       ~~~~~~~~~~~~^^^^^
#   NameError: name 'handle' is not defined


# ======================================================================
# 6. except 元组、多表达式与 except*
# ======================================================================

# ---- compound/except-tuple：括号元组异常的 except
# try:
#     raise TypeError('bad')
# except (ValueError, TypeError):
#     handled = True
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       raise TypeError('bad')
#   TypeError: bad


# ---- compound/except-exprs：PEP 758 省略括号的多异常 except
# try:
#     raise TypeError('bad')
# except ValueError, TypeError:
#     handled = True
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       raise TypeError('bad')
#   TypeError: bad


# ---- compound/except-star：except* 异常组
# try:
#     raise ExceptionGroup('group', [ValueError('bad')])
# except* ValueError:
#     handled = True
# MiniPython 实测：
#   未返回异常，但完成标记未执行；解释器静默提前终止。


# ---- compound/except-star-as：except* 别名形式
# try:
#     raise ExceptionGroup('group', [ValueError('bad')])
# except* ValueError as group:
#     handled = True
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 3, in <module>
#       except* ValueError as group:
#                          ^^
#   NameError: name 'as' is not defined. Did you mean: 'abs'?


# ======================================================================
# 7. lambda 位置专用参数
# ======================================================================

# ---- lambda/slash：lambda 位置专用参数（含默认值）
# head = lambda first, /: first
# with_default = lambda first=1, /: first
# result = head(1) + with_default()
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       head = lambda first, /: first
#                               ^^^^^
#   NameError: name 'first' is not defined. Did you mean: 'list'?


# ======================================================================
# 8. match 字面量与序列模式
# ======================================================================

# ---- match/complex-literal：match 复数字面量模式
# def classify(value):
#     match value:
#         case 1 + 2j:
#             return 'complex'
#         case _:
#             return 'other'
# result = classify(1 + 2j)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 5, in <module>
#       case _:
#       ^^^^
#   NameError: name 'case' is not defined. Did you mean: 'False'?


# ---- match/open-sequence：开放式 sequence 模式
# match 1, 2:
#     case first, second:
#         result = first
#     case _:
#         result = None
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       case first, second:
#       ^^^^
#   NameError: name 'case' is not defined. Did you mean: 'False'?


# ---- match/subject-tuple：裸元组 subject 表达式
# match 1, 2:
#     case (first, second):
#         result = first
#     case _:
#         result = None
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       case (first, second):
#             ^^^^^
#   NameError: name 'first' is not defined. Did you mean: 'list'?


# ======================================================================
# 9. 数字字面量
# ======================================================================

# ---- expr/numbers-underscore：带下划线的数字字面量
# a = 1_000
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       a = 1_000
#           ^^^^^
#   NameError: name '1_000' is not defined


# ---- expr/numbers-octal：八进制字面量
# a = 0o17
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       a = 0o17
#           ^^^^
#   NameError: name '0o17' is not defined


# ======================================================================
# 10. 星号解包下标
# ======================================================================

# ---- expr/starred-slice：星号解包下标
# values = [1, 2]
# selected = values[*values]
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       selected = values[*values]
#                  ~~~~~~^^^^^^^^
#   TypeError: list indices must be integers or slices, not NoneType


# ======================================================================
# 11. 括号内的 yield
# ======================================================================

# ---- expr/group-yield：括号内的 yield 分组
# def gen():
#     sent = (yield 1)
# result = list(gen())
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 3, in <module>
#       result = list(gen())
#                     ~~~^^
#     File "<main>", line 2, in gen
#       sent = (yield 1)
#               ^^^^^
#   NameError: name 'yield' is not defined


# ======================================================================
# 12. f-string 嵌套格式说明
# ======================================================================

# ---- expr/fstring-nested-spec：f-string 嵌套格式说明
# value = 2
# text = f'{value:>{value}}'
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       text = f'{value:>{value}}'
#   ValueError: Invalid format specifier '>{value}'


# ======================================================================
# 13. 模板字符串（PEP 750）
# ======================================================================

# ---- expr/tstring：模板字符串（PEP 750）
# value = 1
# width = 4
# text = t'value={value}'
# padded = t'{value:>{width}}'
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 3, in <module>
#       text = t'value={value}'
#              ^
#   NameError: name 't' is not defined


# ======================================================================
# 14. Ellipsis 的位置
# ======================================================================

# ---- expr/ellipsis-tuple：Ellipsis 作为元组元素
# a = (...,)
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       a = (...,)
#           ^^^^^
#   AttributeError: 'tuple' object has no attribute ''


# ---- expr/ellipsis-default：Ellipsis 作为参数默认值
# def f(value=...):
#     return value
# result = f()
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 2, in <module>
#       return value
#              ^^^^^
#   NameError: name 'value' is not defined. Did you mean: 'False'?


# ======================================================================
# 15. 推导式子句
# ======================================================================

# ---- expr/listcomp-multi-if：列表推导式连续 if 子句
# c = [x for x in [1, 2, 3] if x > 1 if x < 3]
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       c = [x for x in [1, 2, 3] if x > 1 if x < 3]
#                                             ^
#   NameError: name 'x' is not defined


# ---- expr/comprehension-nested：推导式嵌套 for 子句
# pairs = [x + y for x in [1] for y in [2]]
# MiniPython 实测：
#   Traceback (most recent call last):
#     File "<main>", line 1, in <module>
#       pairs = [x + y for x in [1] for y in [2]]
#     File "", line 1, in __comp_0
#   NameError: name 'y' is not defined
