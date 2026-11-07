# MiniPython 作者反馈用失败语法复现

这些语法在标准 Python 3 中合法，但在当前 Rust 集成的 MiniPython 版本中实测失败或提前停止。每一项单独放在 `try/except` 中，便于作者逐项修复。

- 生成器表达式
- 变量类型注解
- 多上下文 `with`
- 复数字面量
- 二进制整数
- 泛型函数类型参数
- `except*` 异常组

运行：

```sh
cargo run -p mini-python-demo --bin syntax_failures
```
