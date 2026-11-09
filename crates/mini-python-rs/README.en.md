# mini-python-rs

[简体中文](README.md)

`mini-python-rs` embeds the MiniPython interpreter in Rust applications. It statically compiles the C++ implementation from `thirdparty/3.14.0` and exposes source execution, script-module registration, and Rust native-function modules through MiniPython's existing C API. It does not require a separate Python runtime or dynamically loaded `.dll` / `.pyd` modules.

> MiniPython is experimental; it is not a drop-in replacement for CPython. Supported syntax, standard-library behavior, and extension compatibility depend on the upstream implementation.

## Features

- Create and manage a MiniPython VM; execute source, load source in stages, and compile/load bytecode.
- Access globals, class/function reflection, calls, operators, attributes, containers, iteration, and formatting.
- `ffi` exposes the complete raw C API from upstream `minipy.h`; `Interpreter::as_raw()` interoperates with it.
- Register MiniPython source modules and Rust native functions, data, classes, and objects.
- Rust callbacks support `None`, integers, floats, booleans, strings, complex values, and bytes/bytearray.
- The `cross-platform` Cargo feature is enabled by default and exposes Rust `platform::Target` metadata; the C++ platform layer is always compiled.
- Catch Rust panics so they do not cross the C ABI.

## Requirements

- Rust 1.85 or newer (edition 2024).
- A C++20 compiler and target-native build tools required by Cargo's `cc` crate.
- WebAssembly targets require a C/C++ sysroot and linker matching the target ABI; this repository has not validated a wasm build.
- Upstream platform abstractions cover Windows, Linux, macOS, iOS, Android, HarmonyOS, and Emscripten/WASI. This repository is currently verified on Windows with MSVC; other targets still require target-specific validation.

## Cross-platform Cargo feature

`cross-platform` is enabled by default. A dependency may opt in explicitly:

```toml
mini-python-rs = { version = "0.1.3", features = ["cross-platform"] }
```

The feature enables the Rust `mini_python_rs::platform::Target` API. The upstream C++ platform layer is required by the interpreter and remains compiled even with `default-features = false`; disabling the feature only hides the Rust target-metadata API and does not restore the old Windows-only implementation.

## Complete upstream C API

`mini_python_rs::ffi` exposes every C API function, status code, and value-kind constant declared in `minipy.h`. It is a raw `unsafe` interface: callers must follow upstream handle ownership, release ordering, and single-thread-per-VM requirements. `Interpreter::as_raw()` returns a borrowed VM pointer; never free it or use it after the interpreter is dropped.

## Install

```toml
[dependencies]
mini-python-rs = "0.1.3"
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

Closures registered through `NativeModule::function` are retained by their `Interpreter`. `NativeValue` supports `None`, integers, floats, booleans, strings, complex values, and bytes/bytearray. Lists, dictionaries, exceptions, and arbitrary script objects remain available through the raw `ffi` module. Returning `Err(String)` raises a MiniPython `RuntimeError`; Rust panics are caught at the callback boundary.

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
