# MiniPython Python 3.14 语法覆盖实测报告

## 结论摘要

- 语法基线：Python 3.14.10 官方[完整语法规范](https://docs.python.org/3.14/reference/grammar.html)，共列出 205 条产生式。
- 计入矩阵：**194** 条产生式；未计入 **11** 条（逐条理由见 `scripts/python314_grammar.tsv`）。
- 清单覆盖：194 / 194 = **100.00%** 的计入产生式至少映射到一个可执行用例；计入产生式合计 404 个顶层候选分支。
- 用例总数：**144**；每项在独立 VM 中执行，并要求执行到末尾完成标记。
- 完整通过：**114 / 144 = 79.17%**。
- 失败分类：解析失败 **0**、运行期/语义失败 **28**、静默提前终止 **2**。
- 全部 30 个失败用例的源码都是 CPython 3.14 合法模块（已用 Python 3.14.0 的 `compile()` 逐条校验）；逐项源码与真实 traceback 见 [`FAILED_SYNTAX.md`](FAILED_SYNTAX.md)。
- 通过率未达到 99%：实际为 79.17%，失败项与未覆盖口径在下文和失败清单中逐项列出。

本次报告只依据 MiniPython 解释器的实际执行结果，不依据 parser 源码推断。作者综合示例脚本 `scripts/syntax.py` 单独执行并通过；它的结果不计入上面的 144 个用例统计。

## 测试方法

### 基线来源

- 语法产生式：[Python 3.14 完整语法规范](https://docs.python.org/3.14/reference/grammar.html)（页面标注 Python 3.14.10，2026-11-09 更新）。
- 语言语义参考：[Python 3.14 语言参考](https://docs.python.org/3.14/reference/index.html)。
- 解释器文档：[MiniPython 官方中文文档](https://www.minipy.site/docs.html)。

### 产生式计数口径

官方页面共有 205 条产生式。以下 11 条不计入“用户可书写语法”覆盖：

| 类别 | 产生式 | 未计入理由 |
| --- | --- | --- |
| 词法记号别名 | `real_number`、`imaginary_number`、`lambda_param`、`string` | 只能匹配单个 `NUMBER` / `NAME` / `STRING` 词法记号，没有独立书写形式 |
| 不可从模块源码到达的入口 | `interactive`、`eval`、`func_type`、`statement_newline`、`single_compound_stmt`、`type_expressions` | 从 `file` 产生式不可达：`interactive` / `statement_newline` / `single_compound_stmt` 仅用于交互式入口，`eval` 仅供 `eval()`，`func_type` 仅供 `ast.parse(mode="func_type")`，`type_expressions` 仅被 `func_type` 引用 |
| 正向前瞻辅助 | `t_lookahead` | 只用于 `&t_lookahead` 前瞻判断，没有独立书写形式 |

`invalid_*` 错误恢复规则不计入：官方 3.14 语法页面在开头明确说明该列表“omits details related to code generation and error recovery”，页面中不存在 `invalid_*` 产生式，因此本次基线没有可计数的错误恢复规则。

### 用例执行方式

```sh
cargo run -p mini-python-demo --bin syntax
```

运行器先执行作者提供的 `scripts/syntax.py`；随后逐个用例：

1. 新建独立 `Interpreter`（每个用例一个 VM，互不污染全局变量）；
2. 拼接源码与完成标记 `__syntax_case_complete = 'yes'`；
3. 调用 `exec_main`，然后读取标记与异常类型。

判定口径：

| 结果 | 判定条件 |
| --- | --- |
| PASS | `exec_main` 返回成功，且完成标记已写入全局变量 |
| 解析失败 | 返回异常，异常类型为 `SyntaxError` / `IndentationError` / `TabError` |
| 运行期/语义失败 | 返回其他异常，例如 `NameError`、`TypeError`、`ValueError` |
| 静默提前终止 | `exec_main` 返回成功，但完成标记没有执行 |

MiniPython 对部分非法或未支持的行不会抛出 `SyntaxError`，而是跳过该行后继续返回成功；因此“返回成功”不足以证明用例执行完整，必须以完成标记为准。这也是本次矩阵把静默提前终止单列的原因。

### 产生式到用例的映射

映射保存在 [`scripts/python314_grammar.tsv`](scripts/python314_grammar.tsv)，列为：

```
rule	section	eligible	reason	alternatives	cases
```

- `eligible`：`E` 计入、`X` 未计入；
- `reason`：未计入理由（计入行为空）；
- `alternatives`：该产生式的顶层候选分支数（括号内与引号内的 `|` 不计）；
- `cases`：映射的用例 ID，逗号分隔；`*` 表示所有模块级用例都隐含覆盖该产生式（`file`、`statements`、`statement`、`simple_stmts`、`simple_stmt`）。

运行器在启动时做双向校验：

- 清单引用的用例 ID 必须存在于用例常量中；
- 每个用例必须至少映射到一个产生式；
- 每条计入产生式必须有用例；
- 每条未计入产生式必须有理由；
- 清单头部的 `eligible` / `excluded` / `cases` 计数必须与运行器实际统计一致。

校验通过后，运行器输出逐用例结果、分类统计、分区统计，以及一行机器可读汇总：

```text
SUMMARY {"cases":144,"pass":114,"parse_error":0,"runtime_error":28,"early_stop":2,"pass_percent":79.17,"productions":205,"eligible":194,"excluded":11,"covered":194,"coverage_percent":100.00}
```

报告中的统计数字来自本次在 `thirdparty/3.14.0` 解释器上的实测，并与运行器输出一致；如果之后替换了 MiniPython 源码，请以重新运行的 `SUMMARY` 行为准。通过率按 `PASS / 用例总数` 计算，覆盖率为 `覆盖的计入产生式 / 计入产生式`，两者分开报告，不能用覆盖率替代通过率。

## 语法清单覆盖

| 指标 | 数值 |
| --- | --- |
| 官方页面产生式 | 205 |
| 计入产生式 | 194 |
| 未计入产生式 | 11 |
| 计入产生式的顶层候选分支合计 | 404 |
| 已覆盖计入产生式 | 194 |
| 清单覆盖率 | 100.00% |

分区统计（只统计计入产生式）：

| 官方语法分区 | 计入 | 已覆盖 | 候选分支合计 |
| --- | --- | --- | --- |
| STARTING RULES | 1 | 1 | 1 |
| GENERAL STATEMENTS | 5 | 5 | 27 |
| SIMPLE STATEMENTS | 14 | 14 | 32 |
| Import statements | 8 | 8 | 12 |
| Common elements | 2 | 2 | 3 |
| Class definitions | 2 | 2 | 3 |
| Function definitions | 2 | 2 | 4 |
| Function parameters | 15 | 15 | 29 |
| If statement | 3 | 3 | 5 |
| While statement | 1 | 1 | 1 |
| For statement | 1 | 1 | 2 |
| With statement | 2 | 2 | 6 |
| Try statement | 1 | 1 | 3 |
| Except statement | 3 | 3 | 8 |
| Match statement | 34 | 34 | 67 |
| Type statement | 1 | 1 | 1 |
| Type parameter declaration | 6 | 6 | 8 |
| EXPRESSIONS | 12 | 12 | 25 |
| Comparison operators | 12 | 12 | 22 |
| Bitwise operators | 4 | 4 | 9 |
| Arithmetic operators | 4 | 4 | 15 |
| Primary elements | 6 | 6 | 22 |
| Lambda functions | 10 | 10 | 21 |
| LITERALS | 16 | 16 | 21 |
| Dicts | 4 | 4 | 5 |
| Comprehensions & Generators | 6 | 6 | 7 |
| FUNCTION CALL ARGUMENTS | 6 | 6 | 11 |
| Generic targets | 9 | 9 | 24 |
| Targets for del statements | 3 | 3 | 8 |
| TYPING ELEMENTS | 1 | 1 | 2 |

说明：候选分支在映射到的用例内部被成组执行（例如 `augassign` 的 13 个运算符写在同一个用例中），覆盖判定以产生式为粒度；本报告不声称对每个候选分支单独断言了运行结果。

## 用例统计

| 用例分组 | 总数 | PASS | 运行期/语义失败 | 静默提前终止 |
| --- | --- | --- | --- | --- |
| 模块与语句组织（module） | 5 | 5 | 0 | 0 |
| 赋值与标注（assign） | 12 | 8 | 4 | 0 |
| 简单语句（stmt） | 12 | 12 | 0 | 0 |
| 导入（import） | 6 | 4 | 2 | 0 |
| 类型语句与类型参数（type） | 6 | 3 | 2 | 1 |
| 复合语句（compound） | 20 | 14 | 5 | 1 |
| 函数定义与参数（function） | 9 | 9 | 0 | 0 |
| 类（class） | 3 | 3 | 0 | 0 |
| lambda | 3 | 2 | 1 | 0 |
| match | 16 | 13 | 3 | 0 |
| 表达式（expr） | 52 | 41 | 11 | 0 |
| **合计** | **144** | **114** | **28** | **2** |

解析失败用例为 0，因此“未出现解析拒绝且执行到完成标记”的比例为 `(114 + 28) / 144 = 98.61%`。该数字只表示没有出现 `SyntaxError` 级别的拒绝或静默中断，**不是通过率**；28 个用例仍在运行期失败。

### 失败归类

| 根因 | 用例 | 失败类别 |
| --- | --- | --- |
| 括号元组与列表解包目标 | `assign/tuple-target`、`assign/list-target`、`assign/nested-unpack`、`expr/target-tuple-of-attributes` | 运行期 |
| 括号单目标标注 | `assign/annotated-paren` | 运行期 |
| from-import 括号列表与星号导入 | `import/from-parens`、`import/from-star` | 运行期 |
| PEP 695 类型参数 | `type/alias-params`、`type/alias-starred-default` | 运行期 |
| PEP 695 类类型参数 | `type/class-params` | 静默提前终止 |
| 括号包裹的 with 上下文 | `compound/with-parens`、`compound/with-parens-single` | 运行期 |
| except 元组与 PEP 758 多异常 | `compound/except-tuple`、`compound/except-exprs` | 运行期 |
| except* 异常组 | `compound/except-star-as` | 运行期 |
| except* 异常组 | `compound/except-star` | 静默提前终止 |
| lambda 位置专用参数 | `lambda/slash` | 运行期 |
| match 复数字面量与开放式序列模式 | `match/complex-literal`、`match/open-sequence` | 运行期 |
| 裸元组 subject 的模式绑定 | `match/subject-tuple` | 运行期 |
| 数字字面量（下划线、八进制） | `expr/numbers-underscore`、`expr/numbers-octal` | 运行期 |
| 星号解包下标 | `expr/starred-slice` | 运行期 |
| 括号内的 yield | `expr/group-yield` | 运行期 |
| f-string 嵌套格式说明 | `expr/fstring-nested-spec` | 运行期 |
| 模板字符串（PEP 750） | `expr/tstring` | 运行期 |
| Ellipsis 作为元组元素或默认值 | `expr/ellipsis-tuple`、`expr/ellipsis-default` | 运行期 |
| 推导式连续 `if` 与嵌套 `for` | `expr/listcomp-multi-if`、`expr/comprehension-nested` | 运行期 |

逐项源码、真实 traceback 和复制说明见 [`FAILED_SYNTAX.md`](FAILED_SYNTAX.md)；同一批源码以注释形式保存在 [`scripts/syntax_failures.py`](scripts/syntax_failures.py)。

### 已通过的代表性语法

- 基础语句：赋值与增强赋值（含 `@=`）、`del`、`assert`、`global` / `nonlocal`、分号单行语句、`yield` / `yield from`。
- 复合语句：`if/elif/else`、`for/else`、`while/else`、单行块、`async for`、`async with`、多上下文（非括号形式）`with`、`try/finally`、`try/except/else`、`except ... as`、裸 `except`。
- 函数与类：默认参数、位置专用参数（函数）、关键字专用参数、`*args` / `**kwargs`、星号参数标注、函数类型注释、装饰器（含表达式装饰器与类装饰器）、`async def` / `await`、继承与类关键字参数。
- 类型语法：`type` 别名、PEP 695 函数类型参数（含上界、`*Ts`、`**P`）；类型参数默认值形式与类类型参数见失败项。
- 模式匹配：字面量、捕获、通配、值模式、分组、列表/元组序列、星号序列、映射（含 `**rest` 与点号键）、类模式（空、位置、关键字、混合）、或模式、`as` 模式、guard。
- 表达式：全部比较运算符、位运算、移位、算术、矩阵乘、幂与一元运算、条件表达式、海象表达式、切片（含多维元组下标）、f-string（转换、宽度格式、调试 `=`）、集合/字典/元组/列表显示与解包、列表/集合/字典/生成器推导式（含单个 `if`、`async for`）、调用实参（位置、关键字、`*`、`**`）、属性与下标赋值目标、`del` 目标组。

## 与上一版报告的差异

上一版报告基于 66 个探针，结论为 59 PASS / 7 FAIL。本次矩阵扩充为 144 个用例并重新实测；上一版列为失败的生成器表达式、变量标注、多上下文 `with`、复数字面量与二进制字面量在本次实测中通过，`except*` 与 PEP 695 类型参数仍有失败项。两版用例集合不同，因此数字不能直接相减比较。

## 未测边界

- 本报告只验证“能否解析并执行到脚本末尾”，不对每条语句的最终语义结果做逐项断言；PASS 不等于该语法在 CPython 中的语义完全一致。
- 覆盖粒度是产生式，不是每个候选分支的独立断言；词法层（数字、字符串、标识符的完整取值规则）与运算符优先级矩阵未单独穷举。
- 未测交互式入口（`interactive`）、`eval()` 入口、`ast.parse(mode="func_type")` 入口和错误恢复规则（官方页面省略）。
- 未测标准库兼容性、CPython C API 兼容性、异步事件循环行为、异常组语义的完整一致性，以及内存/性能特性。
- 未验证失败用例的底层原因是否与 parser、编译器或 VM 某一层单独对应；本报告只记录可复现的外部行为。

## 复现

```sh
cargo run -p mini-python-demo --bin syntax
```

输出包含：

- 每个用例的 `PASS` / `FAIL` 行（失败行后跟分类与真实 traceback 摘要）；
- 用例统计、清单统计、分区统计；
- 一行 `SUMMARY` JSON，便于脚本解析；
- 运行结束后清理用例生成的 `syntax_probe.txt`、`syntax_with_a.txt`、`syntax_with_b.txt`、`test.txt`。
