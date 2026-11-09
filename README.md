# mini-python-rs

[English](README.en.md)

将 MiniPython 解释器嵌入 Rust：静态编译 C++ 实现，通过 Rust API 执行 MiniPython 脚本，并注册 Rust 原生函数供脚本调用。

> MiniPython 是实验性解释器，不是完整 CPython 替代品。支持的语法、标准库和扩展兼容性以其当前实现为准。

## Workspace

```text
crates/mini-python-rs/       Rust 库封装
crates/mini-python-demo/     TCP 示例、Python 3 语法报告
thirdparty/3.14.0/           MiniPython C++ 实现
```

## 运行 TCP demo

终端一启动服务端：

```powershell
cargo run -p mini-python-demo --bin mini-python-demo
```

终端二运行客户端：

```powershell
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello MiniPython"
```

服务端将请求交给 `scripts/handler.py` 执行，脚本导入静态注册的 Rust 模块 `demo_native`，调用 `process_request` 后把结果回传。预期输出：

```text
MiniPython received: hello MiniPython
```

## 调用方向、数据类型与语法示例

```sh
cargo run -p mini-python-demo --bin rust_calls_python
cargo run -p mini-python-demo --bin python_calls_rust
cargo run -p mini-python-demo --bin data_types
cargo run -p mini-python-demo --bin syntax  # Python 3.14 语法实测
```

参数传递示例见 [`mini-python-demo`](crates/mini-python-demo/README.md)，语法覆盖口径和实测结果见 [`SYNTAX.md`](crates/mini-python-demo/SYNTAX.md)，失败语法例程见 [`FAILED_SYNTAX.md`](crates/mini-python-demo/FAILED_SYNTAX.md)。

## 添加 Rust 依赖

```toml
[dependencies]
mini-python-rs = "0.1.3"
```

要求 Rust 1.85+、C++20 编译器和相应本机工具链。MiniPython 上游提供 Windows、Linux、macOS、iOS、Android、HarmonyOS、Emscripten/WASI 平台抽象；Rust API 及当前构建状态见 [`mini-python-rs/README.md`](crates/mini-python-rs/README.md)。本仓库验证环境为 Windows/MSVC，其他目标需要各自验证。

## 最小示例

```rust
use mini_python_rs::{Interpreter, NativeModule, NativeValue};

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    let math = NativeModule::new("native_math").function("add", |args| match args {
        [NativeValue::Integer(a), NativeValue::Integer(b)] => {
            Ok(NativeValue::Integer(a + b))
        }
        _ => Err("add expects two integers".to_owned()),
    });

    vm.load_native_module(math)?;
    vm.exec("import native_math\nanswer = native_math.add(20, 22)")?;
    Ok(())
}
```

`NativeValue` 支持 `None`、整数、浮点数、布尔值、字符串、复数和 bytes/bytearray。列表、字典、异常和其他对象通过 crate 的原始 `ffi` C API 处理。回调闭包由 `Interpreter` 持有；回调返回 `Err` 会作为解释器错误返回，Rust panic 会在 ABI 边界捕获。单个解释器实例遵守 MiniPython 的线程约束：只在一个线程中使用。

更多内容见 [`crates/mini-python-rs/README.md`](crates/mini-python-rs/README.md)、[英文文档](README.en.md) 和 [项目介绍文章](docs/mini-python-rs-introduction.md)。

## 官方文档

- [MiniPython 官方中文文档](https://www.minipy.site/docs.html)：构建、嵌入、C API 和 C++ 接口；本仓库的绑定以 `thirdparty/3.14.0/src/minipy.h` 为接口依据。
- [Python 3.14 官方语言参考](https://docs.python.org/3.14/reference/index.html) 与 [完整语法规范](https://docs.python.org/3.14/reference/grammar.html)：语法探针的外部基线。上游对 Python 3 语法的概括性说明不替代本仓库的实测结果。

## 测试

```sh
cargo test --workspace
cargo run -p mini-python-demo --bin syntax
```

## 许可

见 [`LICENSE`](LICENSE) 及 vendored MiniPython 源码中的原始版权声明。
