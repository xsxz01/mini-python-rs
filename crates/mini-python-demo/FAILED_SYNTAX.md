# MiniPython 作者反馈用失败语法复现

逐项源码见 [`scripts/syntax_failures.py`](scripts/syntax_failures.py)。该文件保留为注释示例，便于作者将每一项单独复制到解释器执行；不要把所有失败项放入一次 `exec`，否则第一项错误会阻断后续测试。

这些语法在标准 Python 3 中合法，但在本次 MiniPython 实测中有失败或静默提前终止：

- 生成器表达式
- 变量类型注解
- 多上下文 `with`
- 复数字面量
- 二进制整数
- 泛型函数类型参数
- `except*` 异常组处理

解释器实测和完整输出见 [`SYNTAX.md`](SYNTAX.md)。运行总测试矩阵：

```sh
cargo run -p mini-python-demo --bin syntax
```
