# MiniPython Python 3 语法实测报告

## 测试方法

报告完全依据 MiniPython 解释器运行结果，不依据 parser 源码推断。执行：

```sh
cargo run -p mini-python-demo --bin syntax
```

运行器先通过新绑定的 `Interpreter::exec_main` 执行作者提供的 `scripts/syntax.py`；该脚本中的交互输入已替换为固定值，文件读写示例仍实际执行，运行器随后清理它生成的 `test.txt`。之后每个表格条目都在独立 VM 中执行对应 Python 源码，输出 `PASS` / `FAIL` 和失败 traceback。测试运行器返回成功表示探针流程完整完成，不代表每条语法都通过。

作者综合脚本：**PASS**。扩展探针：**59 项 PASS，7 项 FAIL**。每项都要求解释器执行到末尾标记；只返回成功但提前停止也计为失败。


### PASS

| 类别 | 实测语法 |
| --- | --- |
| 基础表达式 | 整数、浮点数、布尔值、`None`、字符串；算术/比较/逻辑/位运算；比较链、成员运算、运算符优先级、条件表达式 |
| 赋值与语句 | 链式赋值、增强赋值、序列解包、星号解包、单行分号、海象表达式、`del`、`assert` |
| 容器与索引 | 列表、元组、字典、集合显示；字典展开；下标、切片及步长 |
| 分支与循环 | `if/elif/else`、`for`、`while`、`break`、`continue`、循环 `else` |
| 函数与作用域 | 顶层函数调用、默认参数、位置专用参数、关键字专用参数、`*args` / `**kwargs`、调用参数展开、装饰器（含参数化装饰器）、`global`、`nonlocal`、闭包调用、`yield`、`yield from` |
| 类与模式 | 类、实例方法、match 序列/映射/类模式、guard 和 or-pattern |
| 异常与上下文 | `try/except/finally`、异常别名、`raise from`、`except*`、单个及异步上下文管理器语法 |
| 字符串/字面量 | f-string、bytes、原始字符串、多行字符串、Ellipsis、数字进制字面量 |
| 模块与异步 | `import math` 并调用函数、`async def`、`await` 表达式、`async for`、`async with` 定义源码 |
| 其他 | 多行续行、`type Alias = int` 类型别名 |

### FAIL

| 探针 | 解释器错误摘要 |
| --- | --- |
| 生成器表达式 `(x for x in [1, 2])` | `NameError: name 'x' is not defined` |
| 类型注解 `x: int = 1` | `NameError: name 'x' is not defined`，定位在被注解的变量名处 |
| 同一个 `with` 语句中的多个上下文项 | `NameError: name 'as' is not defined` |
| 复数字面量 `2j` | `NameError: name '2j' is not defined` |
| 二进制数字字面量 `0b10` | `NameError: name '0b10' is not defined` |
| Python 3.12+ 泛型函数类型参数 `def identity[T](...)` | `NameError: name 'T' is not defined` |
| `except* ValueError` 异常组处理 | `exec_main` 返回成功，但完成标记未执行；判为 `FAIL`（执行中断且未返回异常）。 |

## Rust 绑定更新

Rust `Interpreter` 新增 `load_source`、`run`、`exec_main`、`exception_type` 和 `clear_error`，分别绑定上游 C API 的源码加载/执行、`__main__` 执行、异常类型查询和错误状态清理。`exec` 保留为原有 `mp_vm_exec_source` 包装。

工作区构建脚本优先使用 `thirdparty/3.14.0/src`，因此本报告运行的是该目录中的更新版实现。`crates/mini-python-rs/vendor/mini-python/src` 是 crates.io 打包回退源码；本次未将上游改动同步到该副本。离开本地工作区的打包构建仍可能使用旧实现，发布前必须同步更新后的上游源文件并验证 `cargo package` 构建。

## 结论与未测边界

解释器实测显示，作者综合脚本成功，但探针仍发现上述六项失败。因此，作者所称“完美支持 Python 3 的所有语法”**未被本次测试证实**，并且现有实测已经找到反例。测试没有覆盖 Python 3 全部语法产生式、每项语法的所有边界情况，也没有验证全部语义或标准库兼容性；其余语法标为未测，不推断通过或失败。
