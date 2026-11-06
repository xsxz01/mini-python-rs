# mini-python-rs

[简体中文](README.md)

Embed the MiniPython interpreter in Rust: statically compile its C++ implementation, execute scripts through a Rust API, and expose Rust native functions to MiniPython.

> MiniPython is experimental and is not a full CPython replacement. Supported syntax, standard-library behavior, and extension compatibility depend on the current upstream implementation.

## Workspace

```text
crates/mini-python-rs/       Rust library wrapper
crates/mini-python-demo/     TCP server/client example
thirdparty/3.14.0/           MiniPython C++ implementation
```

## Run the TCP demo

Start the server in one terminal:

```powershell
cargo run -p mini-python-demo --bin mini-python-demo
```

Send a request from another terminal:

```powershell
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello MiniPython"
```

The server executes `scripts/handler.py`. That script imports the statically registered Rust module `demo_native`, calls `process_request`, and returns the result. Expected output:

```text
MiniPython received: hello MiniPython
```

## Add the crate

```toml
[dependencies]
mini-python-rs = "0.1.0"
```

Requires Rust 1.85+, a C++17 compiler, and the corresponding native build tools. Verified on Windows/MSVC. MiniPython's `ctypes` implementation uses Windows APIs, so other targets require separate validation.

## Minimal example

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

`NativeValue` supports `None`, integers, floats, booleans, and strings. `Interpreter` owns registered callbacks; callback errors are reported to the interpreter, and Rust panics are caught at the ABI boundary. A VM must be used on one thread at a time.

See the [Chinese crate README](crates/mini-python-rs/README.md), [English crate README](crates/mini-python-rs/README.en.md), and [project article](docs/mini-python-rs-introduction.md).

## Tests

```sh
cargo test --workspace
cargo fmt --all -- --check
```

## License

See [`LICENSE`](LICENSE) and the original notices in the vendored MiniPython source.
