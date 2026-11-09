# mini-python-rs

[English](README.en.md)

`mini-python-rs` 是 MiniPython 解释器的 Rust 嵌入封装。它将 `thirdparty/3.14.0` 中的 C++ 实现静态编译进 Rust crate，并通过现有 C API 提供解释器执行、脚本模块加载和 Rust 原生函数注册能力。不需要单独安装 Python 运行时，也不需要加载 `.dll` / `.pyd` 扩展。

> MiniPython 是实验性解释器，不等同于完整 CPython。Python 语法、标准库和扩展兼容性以 MiniPython 当前实现为准。

## 功能

- 创建和管理 MiniPython VM；执行源码、分阶段加载源码、编译/加载字节码。
- 访问全局变量、类/函数反射、调用、运算、属性、容器、迭代和格式化 API。
- `ffi` 模块暴露上游 `minipy.h` 完整原始 C API；`Interpreter::as_raw()` 可与原始接口互操作。
- 注册 MiniPython 源码模块，以及 Rust 原生函数、数据、类和原生对象。
- Rust 回调支持 `None`、整数、浮点数、布尔值、字符串、复数、bytes/bytearray。
- 默认启用 `cross-platform` Cargo feature，提供 Rust `platform::Target` 目标信息；C++ 平台层始终编译。
- Rust panic 会被截获，不会跨越 C ABI。

## 使用要求

- Rust 1.85 或更新版本（edition 2024）。
- C++20 编译器及 Cargo `cc` crate 所需的目标平台本机工具链。
- WebAssembly 目标需要与目标 ABI 匹配的 C/C++ sysroot 和链接器；本仓库目前未验证 wasm 构建。
- 上游平台层支持 Windows、Linux、macOS、iOS、Android、HarmonyOS 和 Emscripten/WASI；当前仓库验证环境为 Windows + MSVC，其他目标仍需分别验证。

## 上游文档与平台支持

- [MiniPython 官方中文文档](https://www.minipy.site/docs.html) 是 C/C++ 构建和 API 的上游说明；Rust `ffi` 按 vendored `minipy.h` 声明绑定。
- [Python 3.14 官方语言参考](https://docs.python.org/3.14/reference/index.html) 和 [完整语法规范](https://docs.python.org/3.14/reference/grammar.html) 是语法测试基线；本 crate 不承诺与 CPython 语义兼容。

## 跨平台 Cargo feature

`cross-platform` 默认启用。依赖中可显式声明：

```toml
mini-python-rs = { version = "0.1.3", features = ["cross-platform"] }
```

该 feature 启用 `mini_python_rs::platform::Target` Rust API。底层 MiniPython 平台层是解释器必需部分，即使设置 `default-features = false` 仍会编译；关闭 feature 只隐藏 Rust 目标信息 API，不会切回旧的 Windows-only 实现。

## 完整上游 C API

`mini_python_rs::ffi` 暴露 `minipy.h` 中全部 C API 函数、状态码和值类型常量。该模块是原始 `unsafe` 接口；调用方须遵循上游句柄所有权、释放顺序和单 VM 单线程约束。`Interpreter::as_raw()` 返回借用的 VM 指针；不得释放该指针或在解释器销毁后继续使用。

## 安装

```toml
[dependencies]
mini-python-rs = "0.1.3"
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

`NativeModule::function` 的闭包在解释器运行期间由 `Interpreter` 持有。`NativeValue` 支持 `None`、整数、浮点数、布尔值、字符串、复数和 bytes/bytearray；列表、字典、异常及任意脚本对象通过 `ffi` 原始 API 处理。回调返回 `Err(String)` 时，MiniPython 收到 `RuntimeError`；Rust panic 会转换成回调错误。

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
