# mini-python-rs

[简体中文](README.md)

`mini-python-rs` embeds the MiniPython interpreter in Rust applications. It statically compiles the C++ implementation from `thirdparty/3.14.0` and exposes source execution, script-module registration, and Rust native-function modules through MiniPython's existing C API. It does not require a separate Python runtime or dynamically loaded `.dll` / `.pyd` modules.

> MiniPython is experimental; it is not a drop-in replacement for CPython. Supported syntax, standard-library behavior, and extension compatibility depend on the upstream implementation.

## Features

- Create and manage a MiniPython VM.
- Execute source and read/write string globals.
- Register MiniPython source modules.
- Expose statically linked Rust functions as importable modules.
- Pass `None`, integers, floats, booleans, and strings across callbacks.
- Catch Rust panics and return callback errors to MiniPython.

## Requirements

- Rust 1.85 or newer (edition 2024).
- A C++17 compiler and native build tools required by Cargo's `cc` crate.
- Verified on Windows with MSVC. The bundled `ctypes` implementation uses Windows APIs; other targets depend on upstream C++ portability.

## Install

```toml
[dependencies]
mini-python-rs = "0.1.1"
```

## Execute source

```rust
use mini_python_rs::Interpreter;

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    vm.exec("answer = 40 + 2")?;
    Ok(())
}
```

## Register a Rust native module

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

Closures registered through `NativeModule::function` are retained by their `Interpreter`. Callback inputs and results use `NativeValue::{None, Integer, Float, Boolean, String}`. Returning `Err(String)` raises a MiniPython `RuntimeError`; Rust panics are caught at the callback boundary.

Each interpreter must be used from one thread at a time, as required by MiniPython.

## Workspace demo

The TCP demo shows the full path: Rust accepts one request line, stores it in a VM global, MiniPython executes `scripts/handler.py`, and that script calls the Rust native function `demo_native.process_request`.

```powershell
cargo run -p mini-python-demo --bin mini-python-demo
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello"
```

## Build and test

```sh
cargo test -p mini-python-rs
cargo test --workspace
```

The crate bundles the MiniPython C++ sources so it can build after publication to crates.io. Workspace development prefers the upstream source under `thirdparty/3.14.0/src`.

## License and attribution

See the repository `LICENSE` and the original notices in the vendored sources. MiniPython's source notice credits RySoftware and requires redistribution to retain its copyright and license notices.
