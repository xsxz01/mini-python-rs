# mini-python-rs

[English](README.en.md)

将 MiniPython 解释器嵌入 Rust：静态编译 C++ 实现，通过 Rust API 执行 MiniPython 脚本，并注册 Rust 原生函数供脚本调用。

> MiniPython 是实验性解释器，不是完整 CPython 替代品。支持的语法、标准库和扩展兼容性以其当前实现为准。

## Workspace

```text
crates/mini-python-rs/       Rust 库封装
crates/mini-python-demo/     TCP server/client 示例
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

## 添加 Rust 依赖

```toml
[dependencies]
mini-python-rs = "0.1.0"
```

要求 Rust 1.85+、C++17 编译器和对应本机工具链。当前在 Windows/MSVC 下验证；MiniPython `ctypes` 实现使用 Windows API，其他目标平台需要单独验证。

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

`NativeValue` 支持 `None`、整数、浮点数、布尔值和字符串。回调闭包由 `Interpreter` 持有；回调返回 `Err` 会作为解释器错误返回，Rust panic 会在 ABI 边界捕获。单个解释器实例遵守 MiniPython 的线程约束：只在一个线程中使用。

更多内容见 [`crates/mini-python-rs/README.md`](crates/mini-python-rs/README.md)、[英文文档](README.en.md) 和 [项目介绍文章](docs/mini-python-rs-introduction.md)。

## 测试

```sh
cargo test --workspace
cargo fmt --all -- --check
```

## 许可

见 [`LICENSE`](LICENSE) 及 vendored MiniPython 源码中的原始版权声明。
