//! Python 3.14 语法覆盖矩阵。
//!
//! 每个用例都在独立 VM 中执行源码，并要求执行到末尾的完成标记；仅返回成功但未执行到
//! 标记的情况记为“静默中断”。用例与官方语法产生式的对应关系由
//! `scripts/python314_grammar.tsv` 维护，运行器启动时会双向校验清单与用例常量。
//!
//! 基线：<https://docs.python.org/3.14/reference/grammar.html>（Python 3.14.10）。

use std::collections::{BTreeMap, BTreeSet};

use mini_python_rs::Interpreter;

struct Case {
    id: &'static str,
    title: &'static str,
    source: &'static str,
}

const CASES: &[Case] = &[
    Case {
        id: "module/statements",
        title: "模块级语句序列与注释空行",
        source: "# 注释行\n\nx = 1\ny = 2\n",
    },
    Case {
        id: "module/semicolons",
        title: "分号分隔的单行复合语句",
        source: "a = 1; b = 2;\n",
    },
    Case {
        id: "module/expression-statement",
        title: "表达式语句与字符串表达式",
        source: "'doc'\n1 + 1\n",
    },
    Case {
        id: "module/compound-block",
        title: "缩进块与单行块两种 block",
        source: "class Empty: pass\nclass Indented:\n    value = 1\n",
    },
    Case {
        id: "module/nested-simple",
        title: "嵌套定义中的 simple_stmt",
        source: "def outer():\n    def inner(): return 1\n    return inner\nresult = outer()()\n",
    },
    Case {
        id: "assign/chain",
        title: "链式赋值",
        source: "a = b = c = 1\n",
    },
    Case {
        id: "assign/attributes",
        title: "属性与下标赋值目标",
        source: "class Box:\n    pass\nbox = Box()\nbox.value = 1\nitems = [0, 0]\nitems[1] = 2\n",
    },
    Case {
        id: "assign/tuple-target",
        title: "括号元组赋值目标",
        source: "(left, right) = (1, 2)\n",
    },
    Case {
        id: "assign/star",
        title: "星号解包目标",
        source: "a, *rest = [1, 2, 3]\n",
    },
    Case {
        id: "assign/list-target",
        title: "列表赋值目标",
        source: "[b, c] = [1, 2]\n",
    },
    Case {
        id: "assign/nested-unpack",
        title: "嵌套解包目标",
        source: "(a, (b, c)) = (1, (2, 3))\n",
    },
    Case {
        id: "assign/annotated-name",
        title: "变量标注（含无初值）",
        source: "x: int = 1\ny: str\ny = 'ok'\n",
    },
    Case {
        id: "assign/annotated-target",
        title: "属性与下标目标标注",
        source: "class Holder:\n    pass\nholder = Holder()\nholder.count: int = 1\nmapping = {}\nmapping['k']: int = 2\n",
    },
    Case {
        id: "assign/annotated-paren",
        title: "括号单目标标注",
        source: "(name): int = 1\n",
    },
    Case {
        id: "assign/annotated-yield",
        title: "标注赋值的右值可为 yield 表达式",
        source: "def g():\n    x: int = yield 1\n",
    },
    Case {
        id: "assign/augassign",
        title: "整数增强赋值运算符",
        source: "a = 8\na += 1\na -= 1\na *= 2\na //= 2\na %= 3\na **= 2\na &= 3\na |= 1\na ^= 1\na <<= 1\na >>= 1\na /= 2\n",
    },
    Case {
        id: "assign/augassign-matmul",
        title: "矩阵乘增强赋值",
        source: "class Matrix:\n    def __imatmul__(self, other):\n        return self\nmatrix = Matrix()\nmatrix @= Matrix()\n",
    },
    Case {
        id: "stmt/return",
        title: "return 带值与不带值",
        source: "def f(value):\n    return value\ndef g():\n    return\n",
    },
    Case {
        id: "stmt/raise-from",
        title: "raise ... from ...",
        source: "try:\n    raise ValueError('outer') from TypeError('inner')\nexcept ValueError:\n    handled = True\n",
    },
    Case {
        id: "stmt/raise-bare",
        title: "裸 raise 重新抛出",
        source: "def f():\n    try:\n        raise ValueError('v')\n    except ValueError:\n        raise\n    return 1\ntry:\n    f()\nexcept ValueError:\n    caught = True\n",
    },
    Case {
        id: "stmt/pass",
        title: "pass 语句",
        source: "if True:\n    pass\n",
    },
    Case {
        id: "stmt/break",
        title: "break 语句",
        source: "for item in [1]:\n    break\n",
    },
    Case {
        id: "stmt/continue",
        title: "continue 语句",
        source: "total = 0\nfor item in [1, 2]:\n    if item == 2:\n        continue\n    total += item\n",
    },
    Case {
        id: "stmt/global",
        title: "global 声明",
        source: "counter = 0\ndef bump():\n    global counter\n    counter = 1\nbump()\n",
    },
    Case {
        id: "stmt/nonlocal",
        title: "nonlocal 声明",
        source: "def outer():\n    value = 1\n    def inner():\n        nonlocal value\n        value = 2\n    inner()\n    return value\nresult = outer()\n",
    },
    Case {
        id: "stmt/del",
        title: "del 名称、下标与属性目标",
        source: "class Bag:\n    pass\nbag = Bag()\nbag.item = 1\nvalues = [1, 2]\nmapping = {'k': 1}\ndel bag.item\ndel values[0]\ndel mapping['k']\ndel values\n",
    },
    Case {
        id: "stmt/yield",
        title: "yield 语句形式",
        source: "def gen():\n    yield 1\n    yield 2, 3\nresult = list(gen())\n",
    },
    Case {
        id: "stmt/assert",
        title: "assert 及断言消息",
        source: "assert 1 == 1\nassert 2 == 2, 'message'\n",
    },
    Case {
        id: "stmt/docstring-class",
        title: "类体中的文档字符串",
        source: "class Documented:\n    'doc string'\n    value = 1\n",
    },
    Case {
        id: "import/name",
        title: "import 点号与别名",
        source: "import math\nimport os.path as path\nvalue = math.sqrt(4)\n",
    },
    Case {
        id: "import/from",
        title: "from import 及别名",
        source: "from math import sqrt\nfrom math import sqrt as root\nvalue = root(4)\n",
    },
    Case {
        id: "import/from-parens",
        title: "from import 括号列表",
        source: "from math import (sqrt as root,)\nvalue = root(4)\n",
    },
    Case {
        id: "import/from-star",
        title: "from import *",
        source: "from math import *\nvalue = sqrt(4)\n",
    },
    Case {
        id: "import/from-relative",
        title: "相对导入点号形式",
        source: "from . import helper\n",
    },
    Case {
        id: "import/from-dots",
        title: "多级相对导入",
        source: "from ... import helper\n",
    },
    Case {
        id: "type/alias",
        title: "type 类型别名",
        source: "type Alias = int\nvalue: Alias = 1\n",
    },
    Case {
        id: "type/alias-params",
        title: "带类型参数的类型别名",
        source: "type Pair[T] = list\n",
    },
    Case {
        id: "type/params-simple",
        title: "函数类型参数",
        source: "def identity[T](value: T) -> T:\n    return value\nresult = identity(1)\n",
    },
    Case {
        id: "type/params-bounds",
        title: "类型参数上界与可变参数",
        source: "def collect[T: int, *Ts, **P](first: T) -> T:\n    return first\nresult = collect(1)\n",
    },
    Case {
        id: "type/class-params",
        title: "类类型参数",
        source: "class Container[T: int]:\n    pass\n",
    },
    Case {
        id: "type/alias-starred-default",
        title: "类型参数默认值语法",
        source: "type Bucket[T = int, *Ts = *tuple[int, ...]] = tuple[T, *Ts]\n",
    },
    Case {
        id: "compound/if",
        title: "if/elif/else",
        source: "value = 2\nif value == 1:\n    label = 'one'\nelif value == 2:\n    label = 'two'\nelse:\n    label = 'other'\n",
    },
    Case {
        id: "compound/if-oneline",
        title: "单行 if 块",
        source: "value = 1\nif value: result = 'yes'\n",
    },
    Case {
        id: "compound/while-else",
        title: "while/else",
        source: "count = 0\nwhile count < 2:\n    count += 1\nelse:\n    done = count\n",
    },
    Case {
        id: "compound/for-else",
        title: "for/else",
        source: "total = 0\nfor item in [1, 2]:\n    total += item\nelse:\n    finished = total\n",
    },
    Case {
        id: "compound/for-type-comment",
        title: "for 语句类型注释",
        source: "for item in [1]:  # type: int\n    value = item\n",
    },
    Case {
        id: "compound/async-for",
        title: "async for",
        source: "async def consume(source):\n    async for item in source:\n        pass\n",
    },
    Case {
        id: "compound/with-single",
        title: "单个 with 项（带 as）",
        source: "with open('syntax_with_a.txt', 'w') as handle:\n    handle.write('ok')\n",
    },
    Case {
        id: "compound/with-anonymous",
        title: "不带 as 的 with 项",
        source: "with open('syntax_with_b.txt', 'w'):\n    pass\n",
    },
    Case {
        id: "compound/with-multi",
        title: "多上下文 with",
        source: "with open('syntax_with_a.txt', 'w') as first, open('syntax_with_a.txt', 'r') as second:\n    text = second.read()\n",
    },
    Case {
        id: "compound/with-parens",
        title: "括号包裹的上下文列表",
        source: "with (open('syntax_with_a.txt', 'w') as first, open('syntax_with_b.txt', 'w') as second):\n    first.write('a')\n",
    },
    Case {
        id: "compound/with-parens-single",
        title: "括号包裹的单个上下文项",
        source: "with (open('syntax_with_b.txt', 'w') as handle):\n    handle.write('x')\n",
    },
    Case {
        id: "compound/async-with",
        title: "async with",
        source: "async def locked(manager):\n    async with manager:\n        pass\n",
    },
    Case {
        id: "compound/try-finally",
        title: "try/finally",
        source: "try:\n    value = 1\nfinally:\n    cleanup = True\n",
    },
    Case {
        id: "compound/try-except-else",
        title: "try/except/else",
        source: "try:\n    value = 1\nexcept ValueError:\n    value = 2\nelse:\n    value = 3\n",
    },
    Case {
        id: "compound/except-as",
        title: "except 别名与 finally",
        source: "try:\n    raise ValueError('bad')\nexcept ValueError as error:\n    message = str(error)\nfinally:\n    closed = True\n",
    },
    Case {
        id: "compound/except-bare",
        title: "裸 except",
        source: "try:\n    raise TypeError('bad')\nexcept:\n    handled = True\n",
    },
    Case {
        id: "compound/except-tuple",
        title: "括号元组异常的 except",
        source: "try:\n    raise TypeError('bad')\nexcept (ValueError, TypeError):\n    handled = True\n",
    },
    Case {
        id: "compound/except-exprs",
        title: "PEP 758 省略括号的多异常 except",
        source: "try:\n    raise TypeError('bad')\nexcept ValueError, TypeError:\n    handled = True\n",
    },
    Case {
        id: "compound/except-star",
        title: "except* 异常组",
        source: "try:\n    raise ExceptionGroup('group', [ValueError('bad')])\nexcept* ValueError:\n    handled = True\n",
    },
    Case {
        id: "compound/except-star-as",
        title: "except* 别名形式",
        source: "try:\n    raise ExceptionGroup('group', [ValueError('bad')])\nexcept* ValueError as group:\n    handled = True\n",
    },
    Case {
        id: "function/def",
        title: "函数定义、参数与返回值",
        source: "def add(left, right):\n    return left + right\nresult = add(1, 2)\n",
    },
    Case {
        id: "function/return-annotation",
        title: "函数返回标注",
        source: "def double(value: int) -> int:\n    return value * 2\nresult = double(2)\n",
    },
    Case {
        id: "function/type-comment",
        title: "函数类型注释",
        source: "def combine(left, right):\n    # type: (int, int) -> int\n    return left + right\nresult = combine(1, 2)\n",
    },
    Case {
        id: "function/async",
        title: "async def 与 await 表达式",
        source: "async def fetch():\n    return 1\nasync def run():\n    return await fetch()\n",
    },
    Case {
        id: "function/decorators",
        title: "装饰器（含表达式与多层）",
        source: "def deco(function):\n    return function\n@deco\n@(lambda function: function)\ndef decorated():\n    return 1\n@deco\nclass Decorated:\n    pass\n",
    },
    Case {
        id: "function/params-basic",
        title: "普通参数、默认值与关键字参数",
        source: "def call(first, second=2, *items, flag=3, **options):\n    return first\nresult = call(1, 2, 3, flag=4, extra=5)\n",
    },
    Case {
        id: "function/params-slash",
        title: "位置专用参数",
        source: "def head(first, second, /):\n    return first\ndef tail(first=1, /, second=2):\n    return first + second\nresult = head(1, 2) + tail()\n",
    },
    Case {
        id: "function/params-star-annotation",
        title: "星号参数标注",
        source: "def gather(*items: int, **options: str):\n    return items\nresult = gather(1, 2, key='value')\n",
    },
    Case {
        id: "function/params-bare-star",
        title: "裸星号分隔的关键字专用参数",
        source: "def split(first, *, second=2):\n    return first + second\nresult = split(1, second=3)\n",
    },
    Case {
        id: "class/def",
        title: "类定义与基类",
        source: "class Base:\n    pass\nclass Child(Base):\n    pass\n",
    },
    Case {
        id: "class/keywords",
        title: "类关键字参数与空类体",
        source: "class Base:\n    pass\nclass Configured(Base, metaclass=type):\n    pass\n",
    },
    Case {
        id: "class/methods",
        title: "类方法与实例属性",
        source: "class Tally:\n    def __init__(self):\n        self.value = 0\n    def bump(self):\n        self.value += 1\n        return self.value\ntally = Tally()\nresult = tally.bump()\n",
    },
    Case {
        id: "lambda/basic",
        title: "lambda 基本参数与默认值",
        source: "add = lambda left, right=2: left + right\nresult = add(1)\n",
    },
    Case {
        id: "lambda/star",
        title: "lambda 星号与关键字参数",
        source: "pack = lambda *args, **kwargs: (args, kwargs)\nresult = len(pack(1, key=2))\n",
    },
    Case {
        id: "lambda/slash",
        title: "lambda 位置专用参数（含默认值）",
        source: "head = lambda first, /: first\nwith_default = lambda first=1, /: first\nresult = head(1) + with_default()\n",
    },
    Case {
        id: "match/literal",
        title: "match 数字、字符串与常量字面量模式",
        source: "def classify(value):\n    match value:\n        case -1:\n            return 'negative'\n        case 0:\n            return 'zero'\n        case 'text':\n            return 'text'\n        case None:\n            return 'none'\n        case True:\n            return 'true'\n        case False:\n            return 'false'\n        case _:\n            return 'other'\nresult = classify(0)\n",
    },
    Case {
        id: "match/complex-literal",
        title: "match 复数字面量模式",
        source: "def classify(value):\n    match value:\n        case 1 + 2j:\n            return 'complex'\n        case _:\n            return 'other'\nresult = classify(1 + 2j)\n",
    },
    Case {
        id: "match/capture",
        title: "捕获模式与列表序列模式",
        source: "match [1, 2]:\n    case [first, second]:\n        result = first\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/wildcard",
        title: "通配模式",
        source: "match 1:\n    case _:\n        result = 1\n",
    },
    Case {
        id: "match/value",
        title: "值模式与属性路径",
        source: "class Constants:\n    LIMIT = 3\ndef check(value):\n    match value:\n        case Constants.LIMIT:\n            return 'limit'\n        case _:\n            return 'other'\nresult = check(3)\n",
    },
    Case {
        id: "match/group",
        title: "分组模式",
        source: "match 1:\n    case (1):\n        result = 'one'\n    case _:\n        result = 'other'\n",
    },
    Case {
        id: "match/sequence-tuple",
        title: "元组序列模式（含空序列）",
        source: "match (1, 2):\n    case (first, second):\n        result = first\n    case ():\n        result = 0\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/sequence-star",
        title: "序列模式中的星号",
        source: "match [1, 2, 3]:\n    case [first, *middle, last]:\n        result = last\n    case [*_]:\n        result = 0\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/open-sequence",
        title: "开放式 sequence 模式",
        source: "match 1, 2:\n    case first, second:\n        result = first\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/mapping",
        title: "映射模式（空、键值与 ** 剩余）",
        source: "def inspect(value):\n    match value:\n        case {}:\n            return 'empty'\n        case {'kind': kind, **rest}:\n            return kind\n        case _:\n            return 'other'\nresult = inspect({'kind': 'x'})\n",
    },
    Case {
        id: "match/mapping-value-key",
        title: "映射模式的点号键",
        source: "class Keys:\n    NAME = 'name'\nmatch {'name': 1}:\n    case {Keys.NAME: value}:\n        result = value\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/class-empty",
        title: "无参类模式",
        source: "class Marker:\n    pass\nmatch Marker():\n    case Marker():\n        result = 'marker'\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/class-positional",
        title: "类模式位置参数",
        source: "class Point:\n    __match_args__ = ('x', 'y')\n    def __init__(self, x, y):\n        self.x = x\n        self.y = y\nmatch Point(1, 2):\n    case Point(first, second):\n        result = first\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/class-keyword",
        title: "类模式关键字参数与混合形式",
        source: "class Point:\n    def __init__(self, x, y):\n        self.x = x\n        self.y = y\nmatch Point(1, 2):\n    case Point(x=1, y=2):\n        result = 'exact'\n    case Point(1, second):\n        result = second\n    case _:\n        result = None\n",
    },
    Case {
        id: "match/or-as-guard",
        title: "或模式、as 模式与 guard",
        source: "match 2:\n    case 1 | 2 | 3 as number if number > 1:\n        result = number\n    case _:\n        result = 0\n",
    },
    Case {
        id: "match/subject-tuple",
        title: "裸元组 subject 表达式",
        source: "match 1, 2:\n    case (first, second):\n        result = first\n    case _:\n        result = None\n",
    },
    Case {
        id: "expr/numbers-integer",
        title: "二进制、十六进制与整数字面量",
        source: "a = 0b1010\nb = 0xFF\nc = 1000\n",
    },
    Case {
        id: "expr/numbers-underscore",
        title: "带下划线的数字字面量",
        source: "a = 1_000\n",
    },
    Case {
        id: "expr/numbers-octal",
        title: "八进制字面量",
        source: "a = 0o17\n",
    },
    Case {
        id: "expr/numbers-float",
        title: "浮点字面量",
        source: "a = 1.5e3\n",
    },
    Case {
        id: "expr/numbers-complex",
        title: "复数字面量",
        source: "a = 1 + 2j\n",
    },
    Case {
        id: "expr/constants",
        title: "True、False、None 与字符串",
        source: "a = True\nb = False\nc = None\nd = 'text'\ne = \"other\"\n",
    },
    Case {
        id: "expr/conditional",
        title: "条件表达式",
        source: "value = 1 if True else 2\n",
    },
    Case {
        id: "expr/yield-exprs",
        title: "yield 与 yield from 表达式",
        source: "def gen():\n    yield 1\n    yield from [2, 3]\n    sent = yield 4\nresult = list(gen())\n",
    },
    Case {
        id: "expr/star-expressions",
        title: "星号表达式与元组表达式",
        source: "items = [1, 2]\nb = (*items, 3)\nc = 1,\nd = 1, 2,\n",
    },
    Case {
        id: "expr/namedexpr",
        title: "海象表达式",
        source: "if (value := 2) > 1:\n    result = value\n",
    },
    Case {
        id: "expr/bool-ops",
        title: "or、and 与 not",
        source: "a = True or False\nb = True and False\nc = not a\n",
    },
    Case {
        id: "expr/comparison",
        title: "全部比较运算符",
        source: "a = 1 < 2 <= 3\nb = 3 > 2 >= 2\nc = 1 == 1\nd = 1 != 2\ne = 2 in [1, 2]\nf = 3 not in [1, 2]\ng = None is None\nh = 1 is not None\n",
    },
    Case {
        id: "expr/bitwise",
        title: "按位或、异或、与",
        source: "a = 1 | 2 ^ 3 & 1\n",
    },
    Case {
        id: "expr/shift",
        title: "移位运算",
        source: "a = 1 << 2 >> 1\n",
    },
    Case {
        id: "expr/sum",
        title: "加法与减法",
        source: "a = 1 + 2 - 3\n",
    },
    Case {
        id: "expr/term",
        title: "乘、除、整除、取模与矩阵乘",
        source: "a = 2 * 3 // 2 % 2 / 2\n",
    },
    Case {
        id: "expr/matmul",
        title: "矩阵乘运算符",
        source: "class Matrix:\n    def __matmul__(self, other):\n        return 1\nresult = Matrix() @ Matrix()\n",
    },
    Case {
        id: "expr/factor",
        title: "一元加、减与按位取反",
        source: "a = -1\nb = +1\nc = ~1\n",
    },
    Case {
        id: "expr/power",
        title: "幂运算右结合",
        source: "a = 2 ** 3 ** 0\n",
    },
    Case {
        id: "expr/primary",
        title: "属性、调用、下标与生成器实参",
        source: "class Box:\n    value = 1\nbox = Box()\na = box.value\nb = len([1, 2])\nc = [1, 2][0]\nd = sum(item for item in [1, 2])\n",
    },
    Case {
        id: "expr/slices",
        title: "切片的各种形式",
        source: "values = [1, 2, 3, 4]\na = values[0]\nb = values[1:3]\nc = values[::2]\nd = values[1:]\ne = values[:2]\nf = values[:]\n",
    },
    Case {
        id: "expr/slice-tuple",
        title: "多维下标元组切片",
        source: "matrix = [[1, 2], [3, 4]]\nrow = matrix[1, 0]\npart = matrix[0:1, 0]\n",
    },
    Case {
        id: "expr/starred-slice",
        title: "星号解包下标",
        source: "values = [1, 2]\nselected = values[*values]\n",
    },
    Case {
        id: "expr/group",
        title: "括号分组",
        source: "a = (1 + 2)\n",
    },
    Case {
        id: "expr/group-yield",
        title: "括号内的 yield 分组",
        source: "def gen():\n    sent = (yield 1)\nresult = list(gen())\n",
    },
    Case {
        id: "expr/string-concat",
        title: "相邻字符串与 f-string 拼接",
        source: "a = 'ab' 'cd'\nb = f'{1}' 'x'\n",
    },
    Case {
        id: "expr/fstring",
        title: "f-string 替换字段与转换",
        source: "value = 2\ntext = 'x'\na = f'value={value}'\nb = f'{text!r}'\nc = f'{value=}'\n",
    },
    Case {
        id: "expr/fstring-format",
        title: "f-string 格式说明",
        source: "value = 2\ntext = f'{value:>4}'\n",
    },
    Case {
        id: "expr/fstring-nested-spec",
        title: "f-string 嵌套格式说明",
        source: "value = 2\ntext = f'{value:>{value}}'\n",
    },
    Case {
        id: "expr/tstring",
        title: "模板字符串（PEP 750）",
        source: "value = 1\nwidth = 4\ntext = t'value={value}'\npadded = t'{value:>{width}}'\n",
    },
    Case {
        id: "expr/ellipsis",
        title: "Ellipsis 字面量",
        source: "a = ...\n",
    },
    Case {
        id: "expr/ellipsis-tuple",
        title: "Ellipsis 作为元组元素",
        source: "a = (...,)\n",
    },
    Case {
        id: "expr/ellipsis-default",
        title: "Ellipsis 作为参数默认值",
        source: "def f(value=...):\n    return value\nresult = f()\n",
    },
    Case {
        id: "expr/list",
        title: "列表显示与解包",
        source: "values = [1, 2]\na = [*values, 3]\nb = []\n",
    },
    Case {
        id: "expr/tuple",
        title: "元组显示（含空元组）",
        source: "a = (1, 2)\nb = ()\nc = (*a, 3)\n",
    },
    Case {
        id: "expr/set",
        title: "集合显示",
        source: "a = {1, 2}\nb = {1}\n",
    },
    Case {
        id: "expr/dict",
        title: "字典显示与 ** 展开",
        source: "base = {'x': 1}\na = {'y': 2}\nb = {**base, 'z': 3}\nc = {}\n",
    },
    Case {
        id: "expr/listcomp",
        title: "列表推导式与单个 if 子句",
        source: "a = [x * 2 for x in [1, 2]]\nb = [x for x in [1, 2, 3] if x > 1]\n",
    },
    Case {
        id: "expr/listcomp-multi-if",
        title: "列表推导式连续 if 子句",
        source: "c = [x for x in [1, 2, 3] if x > 1 if x < 3]\n",
    },
    Case {
        id: "expr/setcomp",
        title: "集合推导式",
        source: "a = {x for x in [1, 2]}\n",
    },
    Case {
        id: "expr/dictcomp",
        title: "字典推导式",
        source: "a = {x: x for x in [1, 2]}\n",
    },
    Case {
        id: "expr/genexp",
        title: "生成器表达式",
        source: "a = list(x for x in [1, 2])\n",
    },
    Case {
        id: "expr/comprehension-nested",
        title: "推导式嵌套 for 子句",
        source: "pairs = [x + y for x in [1] for y in [2]]\n",
    },
    Case {
        id: "expr/comprehension-async",
        title: "异步推导式",
        source: "async def build(source):\n    a = [item async for item in source]\n    b = {item async for item in source}\n    c = {item: item async for item in source}\n",
    },
    Case {
        id: "expr/call-positional",
        title: "位置与关键字实参",
        source: "def call(first, second, third=3):\n    return first + second + third\na = call(1, 2)\nb = call(first=1, second=2, third=4)\n",
    },
    Case {
        id: "expr/call-starred",
        title: "星号与双星号实参",
        source: "def call(first, second, third=3):\n    return first + second + third\nvalues = [1, 2]\noptions = {'third': 4}\na = call(*values)\nb = call(1, *[2], **options)\n",
    },
    Case {
        id: "expr/target-attribute",
        title: "属性赋值目标",
        source: "class Holder:\n    pass\nholder = Holder()\nholder.item = 1\n",
    },
    Case {
        id: "expr/target-subscript",
        title: "下标赋值目标",
        source: "values = [0]\nvalues[0] = 1\n",
    },
    Case {
        id: "expr/target-tuple-of-attributes",
        title: "属性组成的元组目标",
        source: "class Holder:\n    pass\nfirst = Holder()\nsecond = Holder()\n(first.item, second.item) = (1, 2)\n",
    },
    Case {
        id: "expr/del-nested",
        title: "del 嵌套与括号目标组",
        source: "first = [1]\nsecond = [2]\ndel first[0], second[0]\n",
    },
    Case {
        id: "expr/del-tuple",
        title: "del 元组目标组",
        source: "first = 1\nsecond = 2\ndel (first, second)\n",
    },
    Case {
        id: "expr/del-list",
        title: "del 列表目标组",
        source: "first = 1\nsecond = 2\ndel [first, second]\n",
    },
];

const INVENTORY: &str = include_str!("../../scripts/python314_grammar.tsv");
const MARKER: &str = "__syntax_case_complete";

#[derive(Clone, Copy, PartialEq, Eq)]
enum Outcome {
    Pass,
    ParseError,
    RuntimeError,
    EarlyStop,
}

impl Outcome {
    fn label(self) -> &'static str {
        match self {
            Self::Pass => "PASS",
            Self::ParseError => "解析失败",
            Self::RuntimeError => "运行期失败",
            Self::EarlyStop => "静默中断",
        }
    }
}

struct Production {
    name: &'static str,
    section: &'static str,
    eligible: bool,
    reason: &'static str,
    cases: Vec<&'static str>,
    implicit: bool,
}

struct Inventory {
    productions: Vec<Production>,
    eligible: usize,
    excluded: usize,
    covered: usize,
}

impl Inventory {
    fn load() -> Result<Self, String> {
        let mut lines = INVENTORY.lines();
        let meta = lines.next().ok_or("语法清单为空")?;
        let meta = meta.strip_suffix('\r').unwrap_or(meta);
        let mut declared = BTreeMap::new();
        let mut meta_fields = meta.split('\t');
        if meta_fields.next() != Some("# inventory") {
            return Err("语法清单缺少 # inventory 头部".to_owned());
        }
        for field in meta_fields {
            let (key, value) = field
                .split_once('=')
                .ok_or_else(|| format!("清单头部字段无效: {field}"))?;
            if key == "python" {
                continue;
            }
            let value = value
                .parse::<usize>()
                .map_err(|_| format!("清单头部字段不是整数: {field}"))?;
            declared.insert(key, value);
        }

        let header = lines.next().ok_or("语法清单缺少列名行")?;
        let header = header.strip_suffix('\r').unwrap_or(header);
        if header != "rule\tsection\teligible\treason\talternatives\tcases" {
            return Err(format!("语法清单列名不符: {header}"));
        }

        let mut productions = Vec::new();
        let mut names = BTreeSet::new();
        for line in lines {
            let line = line.strip_suffix('\r').unwrap_or(line);
            if line.is_empty() {
                continue;
            }
            let fields: Vec<&'static str> = line.split('\t').collect();
            if fields.len() != 6 {
                return Err(format!("语法清单行列数不是 6: {line}"));
            }
            let (name, section, kind, reason, alternatives, cases) = (
                fields[0], fields[1], fields[2], fields[3], fields[4], fields[5],
            );
            if !names.insert(name) {
                return Err(format!("语法清单重复的产生式: {name}"));
            }
            let eligible = match kind {
                "E" => true,
                "X" => false,
                other => return Err(format!("未知的计入标记 {other}: {name}")),
            };
            alternatives
                .parse::<usize>()
                .map_err(|_| format!("候选数不是整数: {name}"))?;
            let implicit = cases == "*";
            let cases = if implicit || cases.is_empty() {
                Vec::new()
            } else {
                cases.split(',').collect()
            };
            productions.push(Production {
                name,
                section,
                eligible,
                reason,
                cases,
                implicit,
            });
        }

        let case_ids: BTreeSet<&'static str> = CASES.iter().map(|case| case.id).collect();
        let mut referenced = BTreeSet::new();
        for production in &productions {
            if production.implicit {
                referenced.extend(case_ids.iter().copied());
            } else {
                for case in &production.cases {
                    if !case_ids.contains(case) {
                        return Err(format!(
                            "清单引用了不存在的用例: {} -> {case}",
                            production.name
                        ));
                    }
                    referenced.insert(*case);
                }
            }
            if production.eligible && !production.implicit && production.cases.is_empty() {
                return Err(format!("计入产生式缺少用例: {}", production.name));
            }
            if !production.eligible && production.reason.is_empty() {
                return Err(format!("未计入产生式缺少理由: {}", production.name));
            }
        }
        if let Some(missing) = case_ids.difference(&referenced).next() {
            return Err(format!("用例没有映射到任何产生式: {missing}"));
        }

        let eligible = productions.iter().filter(|item| item.eligible).count();
        let excluded = productions.len() - eligible;
        let covered = productions
            .iter()
            .filter(|item| item.eligible && (item.implicit || !item.cases.is_empty()))
            .count();
        let uncovered: Vec<&str> = productions
            .iter()
            .filter(|item| item.eligible && !item.implicit && item.cases.is_empty())
            .map(|item| item.name)
            .collect();
        if !uncovered.is_empty() {
            return Err(format!("计入产生式未覆盖: {}", uncovered.join(", ")));
        }

        for (key, actual) in [
            ("eligible", eligible),
            ("excluded", excluded),
            ("cases", CASES.len()),
        ] {
            match declared.get(key) {
                Some(value) if *value == actual => {}
                Some(value) => {
                    return Err(format!(
                        "清单头部 {key}={value} 与运行器统计 {actual} 不一致"
                    ));
                }
                None => return Err(format!("清单头部缺少 {key} 计数")),
            }
        }

        Ok(Self {
            productions,
            eligible,
            excluded,
            covered,
        })
    }

    fn total(&self) -> usize {
        self.productions.len()
    }
}

fn is_parse_error(kind: &str, error: &str) -> bool {
    matches!(kind, "SyntaxError" | "IndentationError" | "TabError")
        || error.contains("SyntaxError:")
        || error.contains("IndentationError:")
        || error.contains("TabError:")
}

fn run_case(case: &Case) -> Result<(Outcome, String), String> {
    let mut interpreter = Interpreter::new()?;
    let source = format!("{}\n{MARKER} = 'yes'\n", case.source);
    match interpreter.exec_main(&source) {
        Ok(()) => {
            if interpreter.get_global_string(MARKER).as_deref() == Some("yes") {
                Ok((Outcome::Pass, String::new()))
            } else {
                Ok((
                    Outcome::EarlyStop,
                    "执行返回成功，但完成标记未执行；判定为静默提前终止".to_owned(),
                ))
            }
        }
        Err(error) => {
            let kind = interpreter.exception_type().unwrap_or_default();
            let outcome = if is_parse_error(&kind, &error) {
                Outcome::ParseError
            } else {
                Outcome::RuntimeError
            };
            Ok((outcome, error.replace('\n', " | ")))
        }
    }
}

fn main() -> Result<(), String> {
    let path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("scripts/syntax.py");
    let script = std::fs::read_to_string(path).map_err(|error| error.to_string())?;
    let mut interpreter = Interpreter::new()?;
    match interpreter.exec_main(&script) {
        Ok(())
            if interpreter
                .get_global_string("syntax_script_complete")
                .as_deref()
                == Some("yes") =>
        {
            println!("PASS | scripts/syntax.py | 作者综合示例脚本");
        }
        Ok(()) => println!("FAIL | scripts/syntax.py | 未执行到完成标记"),
        Err(error) => println!("FAIL | scripts/syntax.py | {}", error.replace('\n', " | ")),
    }

    let inventory = Inventory::load()?;

    let mut counts = [0usize; 4];
    for case in CASES {
        let (outcome, detail) = run_case(case)?;
        match outcome {
            Outcome::Pass => {
                counts[0] += 1;
                println!("PASS | {} | {}", case.id, case.title);
            }
            Outcome::ParseError => {
                counts[1] += 1;
                println!("FAIL | {} | {} | {}", case.id, outcome.label(), case.title);
                println!("    {detail}");
            }
            Outcome::RuntimeError => {
                counts[2] += 1;
                println!("FAIL | {} | {} | {}", case.id, outcome.label(), case.title);
                println!("    {detail}");
            }
            Outcome::EarlyStop => {
                counts[3] += 1;
                println!("FAIL | {} | {} | {}", case.id, outcome.label(), case.title);
                println!("    {detail}");
            }
        }
    }

    let [pass, parse_error, runtime_error, early_stop] = counts;
    let total = CASES.len();
    let pass_percent = if total == 0 {
        0.0
    } else {
        pass as f64 * 100.0 / total as f64
    };
    let coverage_percent = if inventory.eligible == 0 {
        0.0
    } else {
        inventory.covered as f64 * 100.0 / inventory.eligible as f64
    };

    println!();
    println!(
        "用例统计 | 总数={total} | 通过={pass} | 解析失败={parse_error} | 运行期失败={runtime_error} | 静默中断={early_stop} | 通过率={pass_percent:.2}%"
    );
    println!(
        "清单统计 | 产生式={} | 计入={} | 未计入={} | 已覆盖={} | 覆盖率={coverage_percent:.2}%",
        inventory.total(),
        inventory.eligible,
        inventory.excluded,
        inventory.covered
    );
    let mut sections: Vec<(&str, usize, usize)> = Vec::new();
    for production in &inventory.productions {
        if !production.eligible {
            continue;
        }
        let index = sections
            .iter()
            .position(|(section, _, _)| *section == production.section)
            .unwrap_or_else(|| {
                sections.push((production.section, 0, 0));
                sections.len() - 1
            });
        sections[index].1 += 1;
        if production.implicit || !production.cases.is_empty() {
            sections[index].2 += 1;
        }
    }
    for (section, eligible, covered) in &sections {
        println!("分区统计 | {section} | 计入={eligible} | 覆盖={covered}");
    }

    println!(
        "SUMMARY {{\"cases\":{total},\"pass\":{pass},\"parse_error\":{parse_error},\"runtime_error\":{runtime_error},\"early_stop\":{early_stop},\"pass_percent\":{pass_percent:.2},\"productions\":{},\"eligible\":{},\"excluded\":{},\"covered\":{},\"coverage_percent\":{coverage_percent:.2}}}",
        inventory.total(),
        inventory.eligible,
        inventory.excluded,
        inventory.covered
    );

    for name in [
        "syntax_probe.txt",
        "syntax_with_a.txt",
        "syntax_with_b.txt",
        "test.txt",
    ] {
        let _ = std::fs::remove_file(name);
    }
    Ok(())
}
