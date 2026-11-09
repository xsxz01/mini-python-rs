# MiniPython 失败语法复现（Python 3.14 基线）

逐项源码同时保留在 [`scripts/syntax_failures.py`](scripts/syntax_failures.py)，以注释形式存放，便于单独复制。
下列源码均为 CPython 3.14 合法模块（已用 Python 3.14.0 的 `compile()` 校验）；错误摘要来自 MiniPython 实测，
由 `cargo run -p mini-python-demo --bin syntax` 输出。请逐项单独执行，不要把全部片段放进一次 `exec`，
否则第一项错误会阻断后续复现。

语法基线：[Python 3.14 完整语法规范](https://docs.python.org/3.14/reference/grammar.html)；
语言参考：[Python 3.14 语言参考](https://docs.python.org/3.14/reference/index.html)；
MiniPython 官方文档：[minipy.site/docs.html](https://www.minipy.site/docs.html)。

完整的通过/失败统计、清单覆盖口径见 [`SYNTAX.md`](SYNTAX.md)。

## 失败清单（共 30 项）

| 根因 | 用例 | 类别 |
| --- | --- | --- |
| 括号元组与列表解包目标 | `assign/tuple-target` | 运行期/语义失败 |
| 括号元组与列表解包目标 | `assign/list-target` | 运行期/语义失败 |
| 括号元组与列表解包目标 | `assign/nested-unpack` | 运行期/语义失败 |
| 括号元组与列表解包目标 | `expr/target-tuple-of-attributes` | 运行期/语义失败 |
| 括号单目标标注 | `assign/annotated-paren` | 运行期/语义失败 |
| from-import 括号列表与星号导入 | `import/from-parens` | 运行期/语义失败 |
| from-import 括号列表与星号导入 | `import/from-star` | 运行期/语义失败 |
| PEP 695 类型参数 | `type/alias-params` | 运行期/语义失败 |
| PEP 695 类型参数 | `type/class-params` | 静默提前终止 |
| PEP 695 类型参数 | `type/alias-starred-default` | 运行期/语义失败 |
| 括号包裹的 with 上下文 | `compound/with-parens` | 运行期/语义失败 |
| 括号包裹的 with 上下文 | `compound/with-parens-single` | 运行期/语义失败 |
| except 元组、多表达式与 except* | `compound/except-tuple` | 运行期/语义失败 |
| except 元组、多表达式与 except* | `compound/except-exprs` | 运行期/语义失败 |
| except 元组、多表达式与 except* | `compound/except-star` | 静默提前终止 |
| except 元组、多表达式与 except* | `compound/except-star-as` | 运行期/语义失败 |
| lambda 位置专用参数 | `lambda/slash` | 运行期/语义失败 |
| match 字面量与序列模式 | `match/complex-literal` | 运行期/语义失败 |
| match 字面量与序列模式 | `match/open-sequence` | 运行期/语义失败 |
| match 字面量与序列模式 | `match/subject-tuple` | 运行期/语义失败 |
| 数字字面量 | `expr/numbers-underscore` | 运行期/语义失败 |
| 数字字面量 | `expr/numbers-octal` | 运行期/语义失败 |
| 星号解包下标 | `expr/starred-slice` | 运行期/语义失败 |
| 括号内的 yield | `expr/group-yield` | 运行期/语义失败 |
| f-string 嵌套格式说明 | `expr/fstring-nested-spec` | 运行期/语义失败 |
| 模板字符串（PEP 750） | `expr/tstring` | 运行期/语义失败 |
| Ellipsis 的位置 | `expr/ellipsis-tuple` | 运行期/语义失败 |
| Ellipsis 的位置 | `expr/ellipsis-default` | 运行期/语义失败 |
| 推导式子句 | `expr/listcomp-multi-if` | 运行期/语义失败 |
| 推导式子句 | `expr/comprehension-nested` | 运行期/语义失败 |

## 逐项源码与 traceback

### 括号元组与列表解包目标

目标被当作单个目标解析，触发解包数量错误或目标名未定义。

#### `assign/tuple-target` — 括号元组赋值目标

```python
(left, right) = (1, 2)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    (left, right) = (1, 2)
                    ~^~~~~
ValueError: too many values to unpack (expected 1)
```

#### `assign/list-target` — 列表赋值目标

```python
[b, c] = [1, 2]
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    [b, c] = [1, 2]
     ^
NameError: name 'b' is not defined
```

#### `assign/nested-unpack` — 嵌套解包目标

```python
(a, (b, c)) = (1, (2, 3))
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    (a, (b, c)) = (1, (2, 3))
                  ~^~~~~~~~~~
ValueError: too many values to unpack (expected 1)
```

#### `expr/target-tuple-of-attributes` — 属性组成的元组目标

```python
class Holder:
    pass
first = Holder()
second = Holder()
(first.item, second.item) = (1, 2)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 5, in <module>
    (first.item, second.item) = (1, 2)
                                ~^~~~~
ValueError: too many values to unpack (expected 1)
```

### 括号单目标标注

`(name): int = 1` 的括号目标未被识别，标注语句被静默丢弃。

#### `assign/annotated-paren` — 括号单目标标注

```python
(name): int = 1
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    (name): int = 1
     ^^^^
NameError: name 'name' is not defined
```

### from-import 括号列表与星号导入

语法被接受，但括号列表项与 `import *` 导入的名称没有进入命名空间。

#### `import/from-parens` — from import 括号列表

```python
from math import (sqrt as root,)
value = root(4)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    from math import (sqrt as root,)
                      ^^^^
NameError: name 'sqrt' is not defined
```

#### `import/from-star` — from import *

```python
from math import *
value = sqrt(4)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    value = sqrt(4)
            ^^^^
NameError: name 'sqrt' is not defined
```

### PEP 695 类型参数

类型别名参数、类类型参数与默认值形式的类型参数解析或执行不完整；类类型参数会静默提前终止。

#### `type/alias-params` — 带类型参数的类型别名

```python
type Pair[T] = list
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    type Pair[T] = list
                   ^^^^
NameError: name 'Pair' is not defined
```

#### `type/class-params` — 类类型参数

```python
class Container[T: int]:
    pass
```

MiniPython 实测：

```text
未返回异常，但完成标记未执行；解释器静默提前终止。
```

#### `type/alias-starred-default` — 类型参数默认值语法

```python
type Bucket[T = int, *Ts = *tuple[int, ...]] = tuple[T, *Ts]
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    type Bucket[T = int, *Ts = *tuple[int, ...]] = tuple[T, *Ts]
                                                   ~~~~~^^^
TypeError: 'native_object' object is not subscriptable
```

### 括号包裹的 with 上下文

`with (...)` 中的 `as` 目标未绑定，括号内单项与多项都受影响。

#### `compound/with-parens` — 括号包裹的上下文列表

```python
with (open('syntax_with_a.txt', 'w') as first, open('syntax_with_b.txt', 'w') as second):
    first.write('a')
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    with (open('syntax_with_a.txt', 'w') as first, open('syntax_with_b.txt', 'w') as second):
                                                                                  ^^
NameError: name 'as' is not defined. Did you mean: 'abs'?
```

#### `compound/with-parens-single` — 括号包裹的单个上下文项

```python
with (open('syntax_with_b.txt', 'w') as handle):
    handle.write('x')
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    handle.write('x')
    ~~~~~~~~~~~~^^^^^
NameError: name 'handle' is not defined
```

### except 元组、多表达式与 except*

括号元组异常与 PEP 758 多异常未按类型集合匹配；`except*` 存在静默提前终止与 `as` 解析失败。

#### `compound/except-tuple` — 括号元组异常的 except

```python
try:
    raise TypeError('bad')
except (ValueError, TypeError):
    handled = True
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    raise TypeError('bad')
TypeError: bad
```

#### `compound/except-exprs` — PEP 758 省略括号的多异常 except

```python
try:
    raise TypeError('bad')
except ValueError, TypeError:
    handled = True
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    raise TypeError('bad')
TypeError: bad
```

#### `compound/except-star` — except* 异常组

```python
try:
    raise ExceptionGroup('group', [ValueError('bad')])
except* ValueError:
    handled = True
```

MiniPython 实测：

```text
未返回异常，但完成标记未执行；解释器静默提前终止。
```

#### `compound/except-star-as` — except* 别名形式

```python
try:
    raise ExceptionGroup('group', [ValueError('bad')])
except* ValueError as group:
    handled = True
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 3, in <module>
    except* ValueError as group:
                       ^^
NameError: name 'as' is not defined. Did you mean: 'abs'?
```

### lambda 位置专用参数

lambda 的 `/` 分隔符未被识别，参数名被当作表达式求值。

#### `lambda/slash` — lambda 位置专用参数（含默认值）

```python
head = lambda first, /: first
with_default = lambda first=1, /: first
result = head(1) + with_default()
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    head = lambda first, /: first
                            ^^^^^
NameError: name 'first' is not defined. Did you mean: 'list'?
```

### match 字面量与序列模式

复数字面量模式与开放式序列模式被当作普通语句；裸元组 subject 的模式捕获绑定失败。

#### `match/complex-literal` — match 复数字面量模式

```python
def classify(value):
    match value:
        case 1 + 2j:
            return 'complex'
        case _:
            return 'other'
result = classify(1 + 2j)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 5, in <module>
    case _:
    ^^^^
NameError: name 'case' is not defined. Did you mean: 'False'?
```

#### `match/open-sequence` — 开放式 sequence 模式

```python
match 1, 2:
    case first, second:
        result = first
    case _:
        result = None
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    case first, second:
    ^^^^
NameError: name 'case' is not defined. Did you mean: 'False'?
```

#### `match/subject-tuple` — 裸元组 subject 表达式

```python
match 1, 2:
    case (first, second):
        result = first
    case _:
        result = None
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    case (first, second):
          ^^^^^
NameError: name 'first' is not defined. Did you mean: 'list'?
```

### 数字字面量

下划线分隔与八进制字面量未实现，标识符被当作变量名。

#### `expr/numbers-underscore` — 带下划线的数字字面量

```python
a = 1_000
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    a = 1_000
        ^^^^^
NameError: name '1_000' is not defined
```

#### `expr/numbers-octal` — 八进制字面量

```python
a = 0o17
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    a = 0o17
        ^^^^
NameError: name '0o17' is not defined
```

### 星号解包下标

`values[*values]` 被解析为 `None` 下标。

#### `expr/starred-slice` — 星号解包下标

```python
values = [1, 2]
selected = values[*values]
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    selected = values[*values]
               ~~~~~~^^^^^^^^
TypeError: list indices must be integers or slices, not NoneType
```

### 括号内的 yield

生成器中 `(yield 1)` 被执行时按普通名称处理。

#### `expr/group-yield` — 括号内的 yield 分组

```python
def gen():
    sent = (yield 1)
result = list(gen())
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 3, in <module>
    result = list(gen())
                  ~~~^^
  File "<main>", line 2, in gen
    sent = (yield 1)
            ^^^^^
NameError: name 'yield' is not defined
```

### f-string 嵌套格式说明

格式说明中的替换字段未被求值，原样传入格式解析。

#### `expr/fstring-nested-spec` — f-string 嵌套格式说明

```python
value = 2
text = f'{value:>{value}}'
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    text = f'{value:>{value}}'
ValueError: Invalid format specifier '>{value}'
```

### 模板字符串（PEP 750）

`t'...'` 未被识别为字符串前缀，按名称 `t` 处理。

#### `expr/tstring` — 模板字符串（PEP 750）

```python
value = 1
width = 4
text = t'value={value}'
padded = t'{value:>{width}}'
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 3, in <module>
    text = t'value={value}'
           ^
NameError: name 't' is not defined
```

### Ellipsis 的位置

Ellipsis 作为元组元素或参数默认值时被解析为空属性访问。

#### `expr/ellipsis-tuple` — Ellipsis 作为元组元素

```python
a = (...,)
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    a = (...,)
        ^^^^^
AttributeError: 'tuple' object has no attribute ''
```

#### `expr/ellipsis-default` — Ellipsis 作为参数默认值

```python
def f(value=...):
    return value
result = f()
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 2, in <module>
    return value
           ^^^^^
NameError: name 'value' is not defined. Did you mean: 'False'?
```

### 推导式子句

连续 `if` 子句与嵌套 `for` 子句的目标绑定不完整。

#### `expr/listcomp-multi-if` — 列表推导式连续 if 子句

```python
c = [x for x in [1, 2, 3] if x > 1 if x < 3]
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    c = [x for x in [1, 2, 3] if x > 1 if x < 3]
                                          ^
NameError: name 'x' is not defined
```

#### `expr/comprehension-nested` — 推导式嵌套 for 子句

```python
pairs = [x + y for x in [1] for y in [2]]
```

MiniPython 实测：

```text
Traceback (most recent call last):
  File "<main>", line 1, in <module>
    pairs = [x + y for x in [1] for y in [2]]
  File "", line 1, in __comp_0
NameError: name 'y' is not defined
```

