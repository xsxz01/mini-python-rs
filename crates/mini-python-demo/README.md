# mini-python-demo

MiniPython 的 Rust 集成示例，包含 TCP 服务端/客户端、Rust 与 Python 双向调用、数据类型示例和语法覆盖测试。

## TCP 服务端与客户端

启动服务端：

```sh
cargo run -p mini-python-demo --bin mini-python-demo
```

默认监听 `127.0.0.1:7878`。另开终端发送一行请求：

```sh
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello"
```

Rust 在 `src/module/mod.rs` 注册静态原生模块 `demo_native`；`scripts/handler.py` 导入该模块并调用 `process_request`。此示例通过 MiniPython C API 注册回调，不加载动态模块文件。

## Rust 与 Python 互操作

```sh
cargo run -p mini-python-demo --bin rust_calls_python
cargo run -p mini-python-demo --bin python_calls_rust
cargo run -p mini-python-demo --bin data_types
```

`rust_calls_python` 从 Rust 设置字符串全局变量、执行 Python 函数并导入 Rust 注册的源码模块；`python_calls_rust` 从 Python 调用 `demo_native` 中的多个 Rust 函数；`data_types` 展示 `NativeValue` 的基础类型及复数、bytes/bytearray。列表、字典、异常和任意脚本对象通过底层 `ffi` 接口处理。

## Python 3.14 语法测试

```sh
cargo run -p mini-python-demo --bin syntax
```

测试范围、覆盖口径、通过/失败项和未支持例程见 [`SYNTAX.md`](SYNTAX.md) 与 [`FAILED_SYNTAX.md`](FAILED_SYNTAX.md)。报告以 Python 3.14 官方语法规范为基线；探针结果不代表完整 CPython 语义或标准库兼容性。

失败例程源码和 MiniPython 实测错误摘要见 [`FAILED_SYNTAX.md`](FAILED_SYNTAX.md)；对应的独立 Python 文件为 [`scripts/syntax_failures.py`](scripts/syntax_failures.py)。其中源码以注释逐项存放，供单独复制到 MiniPython 复现。

## 官方参考

- [MiniPython 官方中文文档](https://www.minipy.site/docs.html)
- [Python 3.14 官方语言参考](https://docs.python.org/3.14/reference/index.html)
- [Python 3.14 完整语法规范](https://docs.python.org/3.14/reference/grammar.html)
