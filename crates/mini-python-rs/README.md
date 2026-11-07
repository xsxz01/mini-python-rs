# mini-python-rs

[English](README.en.md)

`mini-python-rs` 是 MiniPython 解释器的 Rust 嵌入封装。它将 `thirdparty/3.14.0` 中的 C++ 实现静态编译进 Rust crate，并通过现有 C API 提供解释器执行、脚本模块加载和 Rust 原生函数注册能力。不需要单独安装 Python 运行时，也不需要加载 `.dll` / `.pyd` 扩展。

> MiniPython 是实验性解释器，不等同于完整 CPython。Python 语法、标准库和扩展兼容性以 MiniPython 当前实现为准。

## 功能

- 创建和管理 MiniPython VM。
- 执行 Python 源码，并读取/设置字符串全局变量。
- 注册 MiniPython 源码模块。
- 将静态链接的 Rust 函数注册成可 `import` 的模块。
- 在 Rust 回调中处理 `None`、整数、浮点数、布尔值和字符串；返回值或错误传回解释器。
- C++ callback 以 C ABI 边界调用，Rust panic 会被截获，不跨越 ABI。

## 使用要求

- Rust 1.85 或更新版本（edition 2024）。
- 可用的 C++17 编译器和 Cargo `cc` crate 所需的本机编译工具链。
- 当前验证环境为 Windows + MSVC；MiniPython 自带 `ctypes` 实现依赖 Windows API。其他平台是否可构建，取决于上游 C++ 源码的可移植性。

## 安装

```toml
[dependencies]
mini-python-rs = "0.1.2"
```


## Rust API 文档

在仓库根目录运行：

```sh
cargo doc -p mini-python-rs --no-deps --open
```

浏览器会打开 `target/doc/mini_python_rs/index.html`。公开类型和方法的 `///` 注释会显示在 API 页面中。发布到 crates.io 后也可从 [docs.rs](https://docs.rs/mini-python-rs/latest/mini_python_rs/) 在线查看。
## 执行脚本

```rust
use mini_python_rs::Interpreter;

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    vm.exec("answer = 40 + 2")?;
    Ok(())
}
```

上游还提供分阶段执行接口：`load_source(source)` 只加载源码，随后调用 `run()` 执行；`exec_main(source)` 以 `__main__` 方式执行源码。`exception_type()` 可读取待处理 Python 异常类型，`clear_error()` 清理错误状态。`exec(source)` 保持原有直接执行接口。

## 注册 Rust 原生模块

```rust
use mini_python_rs::{Interpreter, NativeModule, NativeValue};

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    let module = NativeModule::new("native_math")
        .function("add", |args| match args {
            [NativeValue::Integer(a), NativeValue::Integer(b)] => {
                Ok(NativeValue::Integer(a + b))
            }
            _ => Err("add expects two integers".to_owned()),
        });

    vm.load_native_module(module)?;
    vm.exec("import native_math\nanswer = native_math.add(20, 22)")?;
    Ok(())
}
```

`NativeModule::function` 的闭包在解释器运行期间由 `Interpreter` 持有。支持的参数和返回值类型为 `NativeValue::{None, Integer, Float, Boolean, String}`。回调返回 `Err(String)` 时，MiniPython 收到 `RuntimeError`；Rust panic 会转换成回调错误。

一个 `Interpreter` 遵循 MiniPython 的线程约束：同一实例只在一个线程内使用。不要让 VM 或其回调跨线程并发访问。

## Workspace 示例

仓库中的 TCP demo 展示完整调用链：Rust TCP 服务接收一行请求，将输入写入 VM 全局变量；MiniPython 执行 `scripts/handler.py`；脚本调用 Rust 原生模块 `demo_native.process_request` 并返回结果。

```powershell
cargo run -p mini-python-demo --bin mini-python-demo
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello"
```

## 构建和测试

```sh
cargo test -p mini-python-rs
cargo test --workspace
```

crate 包含 MiniPython C++ 源码副本，确保发布到 crates.io 后仍可独立构建；仓库开发时优先使用 `thirdparty/3.14.0/src` 下的上游源码。

## 许可与来源

Rust 封装和 MiniPython C++ 源码的许可/版权信息见仓库 `LICENSE` 及 vendored 源文件中的原始声明。MiniPython 源码声明版权为 RySoftware，并要求再分发时保留版权与许可声明。
