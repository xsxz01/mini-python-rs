use mini_python_rs::Interpreter;

const PROBES: &[(&str, &str)] = &[
    (
        "整数、浮点、布尔、None、字符串",
        "a = 7\nb = 2.5\nc = True\nd = None\ne = 'ok'\n",
    ),
    (
        "算术、比较、逻辑、位运算",
        "a = (2 + 3 * 4 == 14) and not False\nb = 5 | 2\n",
    ),
    ("链式赋值与增强赋值", "a = b = 1\na += 2\nb *= 3\n"),
    ("序列解包", "a, b = (1, 2)\n"),
    (
        "列表/元组/字典/集合显示",
        "a = [1, 2]\nb = (1, 2)\nc = {'x': 1}\nd = {1, 2}\n",
    ),
    ("下标和切片", "a = [1, 2, 3]\nb = a[0]\nc = a[1:]\n"),
    (
        "条件分支",
        "if True:\n    result = 1\nelif False:\n    result = 2\nelse:\n    result = 3\n",
    ),
    (
        "for 循环和 continue/break",
        "total = 0\nfor x in [1, 2, 3]:\n    if x == 2:\n        continue\n    total += x\n    if x == 3:\n        break\n",
    ),
    ("while 循环", "n = 0\nwhile n < 2:\n    n += 1\n"),
    (
        "函数定义、调用、返回值",
        "def f(x):\n    return x + 1\ny = f(2)\n",
    ),
    ("默认参数", "def f(x=3):\n    return x\ny = f()\n"),
    ("lambda", "f = lambda x: x + 1\ny = f(2)\n"),
    (
        "类和实例方法",
        "class A:\n    def f(self):\n        return 1\na = A()\ny = a.f()\n",
    ),
    (
        "异常处理",
        "try:\n    raise ValueError('x')\nexcept ValueError:\n    y = 1\nfinally:\n    z = 2\n",
    ),
    ("assert", "assert 1 == 1\n"),
    ("列表推导式", "a = [x * 2 for x in [1, 2]]\n"),
    ("字典推导式", "a = {x: x for x in [1, 2]}\n"),
    ("集合推导式", "a = {x for x in [1, 2]}\n"),
    ("f-string", "x = 2\ny = f'value={x}'\n"),
    ("导入标准库模块", "import math\ny = math.sqrt(4)\n"),
    (
        "match/case",
        "x = 1\nmatch x:\n    case 1:\n        y = 'one'\n    case _:\n        y = 'other'\n",
    ),
    ("async def / await", "async def f():\n    return 1\n"),
    ("单行复合语句与分号", "a = 1; b = 2\n"),
    ("星号解包", "a, *b = [1, 2, 3]\n"),
    (
        "关键字参数和可变参数",
        "def f(a, *args, **kwargs):\n    return a\ny = f(1, 2, x=3)\n",
    ),
    (
        "关键字专用参数",
        "def f(a, *, b):\n    return a + b\ny = f(1, b=2)\n",
    ),
    (
        "装饰器",
        "def deco(f):\n    return f\n@deco\ndef f():\n    return 1\ny = f()\n",
    ),
    (
        "global 声明",
        "x = 1\ndef f():\n    global x\n    x = 2\nf()\n",
    ),
    (
        "nonlocal 声明",
        "def outer():\n    x = 1\n    def inner():\n        nonlocal x\n        x = 2\n    inner()\n    return x\ny = outer()\n",
    ),
    (
        "with 上下文管理器",
        "with open('syntax_probe.txt', 'w') as f:\n    f.write('ok')\n",
    ),
    ("删除语句", "a = [1, 2]\ndel a[0]\n"),
    ("条件表达式", "x = 1 if True else 2\n"),
    ("海象表达式", "if (x := 2) > 1:\n    y = x\n"),
    ("字典展开", "a = {'x': 1}\nb = {**a, 'y': 2}\n"),
    ("多行续行", "x = (1 +\n     2)\n"),
    (
        "类型注解",
        "x: int = 1\ndef f(a: int) -> int:\n    return a\n",
    ),
    ("bytes 字面量", "x = b'abc'\n"),
    ("原始字符串", "x = r'a\\b'\n"),
    ("Ellipsis 字面量", "x = ...\n"),
    (
        "异步 for 语法",
        "async def f():\n    async for x in y:\n        pass\n",
    ),
    (
        "异步 with 语法",
        "async def f():\n    async with manager:\n        pass\n",
    ),
    (
        "异常组 except*",
        "try:\n    pass\nexcept* ValueError:\n    pass\n",
    ),
    ("类型别名 type statement", "type Alias = int\n"),
    (
        "匹配序列模式",
        "match [1, 2]:\n    case [a, b]:\n        x = a + b\n",
    ),
    (
        "函数仅位置参数",
        "def f(a, /, b):\n    return a + b\ny = f(1, 2)\n",
    ),
    (
        "调用参数展开",
        "args = [1, 2]\nkwargs = {'c': 3}\ndef f(a, b, c):\n    return a + b + c\ny = f(*args, **kwargs)\n",
    ),
    (
        "for else 分支",
        "for x in [1]:\n    pass\nelse:\n    y = 1\n",
    ),
    (
        "while else 分支",
        "x = 0\nwhile x < 1:\n    x += 1\nelse:\n    y = x\n",
    ),
    (
        "多重上下文管理器",
        "with open('syntax_probe.txt', 'w') as a, open('syntax_probe.txt', 'r') as b:\n    x = b.read()\n",
    ),
    (
        "raise from",
        "try:\n    raise ValueError('outer') from TypeError('inner')\nexcept ValueError:\n    pass\n",
    ),
    (
        "异常别名",
        "try:\n    raise ValueError('bad')\nexcept ValueError as error:\n    message = str(error)\n",
    ),
    (
        "match 映射模式",
        "match {'x': 1}:\n    case {'x': value}:\n        y = value\n",
    ),
    (
        "match 类模式",
        "class A:\n    def __init__(self, value):\n        self.value = value\nmatch A(1):\n    case A(value=value):\n        y = value\n",
    ),
    ("复数字面量", "x = 1 + 2j\n"),
    ("数字进制字面量", "x = 0xff + 0b10 + 0o7\n"),
    ("多行字符串字面量", "x = '''line one\nline two'''\n"),
    ("比较链和成员运算", "x = 1 < 2 <= 3 and 2 in [1, 2]\n"),
    (
        "运算符优先级与三元表达式",
        "x = (2 ** 3 << 1) if True else 0\n",
    ),
    ("切片步长", "x = [1, 2, 3, 4][::2]\n"),
    (
        "装饰器参数",
        "def deco(n):\n    def wrap(f):\n        return f\n    return wrap\n@deco(1)\ndef f():\n    return 1\n",
    ),
    (
        "非局部闭包调用",
        "def outer():\n    x = 1\n    def inner():\n        nonlocal x\n        x += 1\n    inner()\n    return x\ny = outer()\n",
    ),
    (
        "async await 表达式",
        "async def f():\n    return await g()\n",
    ),
    ("yield from", "def f():\n    yield from [1, 2]\n"),
    (
        "match guard 与 or pattern",
        "match 1:\n    case 1 | 2 if True:\n        y = 1\n",
    ),
    (
        "类型参数语法",
        "def identity[T](value: T) -> T:\n    return value\n",
    ),
];


fn main() -> Result<(), String> {
    let path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("scripts/syntax.py");
    let script = std::fs::read_to_string(path).map_err(|error| error.to_string())?;
    let mut vm = Interpreter::new()?;
    match vm.exec_main(&script) {
        Ok(()) if vm.get_global_string("syntax_script_complete").as_deref() == Some("yes") => {
            println!("PASS | scripts/syntax.py 示例脚本")
        }
        Ok(()) => println!("FAIL | scripts/syntax.py 未执行到完成标记"),
        Err(error) => println!("FAIL | scripts/syntax.py | {}", error.replace('\n', " | ")),
    }

    for (label, source) in PROBES {
        let mut vm = Interpreter::new()?;
        let marked_source = format!("{source}\n__probe_complete = 'yes'\n");
        match vm.exec_main(&marked_source) {
            Ok(()) if vm.get_global_string("__probe_complete").as_deref() == Some("yes") => {
                println!("PASS | {label}")
            }
            Ok(()) => println!("FAIL | {label} | 未执行到完成标记"),
            Err(error) => println!("FAIL | {label} | {}", error.replace('\n', " | ")),
        }
    }
    let _ = std::fs::remove_file("syntax_probe.txt");
    let _ = std::fs::remove_file("test.txt");
    Ok(())
}
