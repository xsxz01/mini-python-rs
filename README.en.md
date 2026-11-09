# mini-python-rs

[简体中文](README.md)

Embed the MiniPython interpreter in Rust: statically compile its C++ implementation, execute scripts through a Rust API, and expose Rust native functions to MiniPython.

> MiniPython is experimental and is not a full CPython replacement. Supported syntax, standard-library behavior, and extension compatibility depend on the current upstream implementation.

## Workspace

```text
crates/mini-python-rs/       Rust library wrapper
crates/mini-python-demo/     TCP example and Python 3 syntax reports
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

## Python 3.14 syntax report

Run the parser-backed syntax report:

```sh
cargo run -p mini-python-demo --bin syntax
```

The report lists the tested boundary and unsupported cases in [`SYNTAX.md`](crates/mini-python-demo/SYNTAX.md) and [`FAILED_SYNTAX.md`](crates/mini-python-demo/FAILED_SYNTAX.md). The individually copyable failure snippets are also in [`syntax_failures.py`](crates/mini-python-demo/scripts/syntax_failures.py).

## Add the crate

```toml
[dependencies]
mini-python-rs = "0.1.3"
```

Requires Rust 1.85+, a C++20 compiler, and the target's native build tools. Upstream MiniPython provides platform abstractions for Windows, Linux, macOS, iOS, Android, HarmonyOS, and Emscripten/WASI; this repository is currently verified only on Windows/MSVC, and other targets still need target-specific build validation.

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

`NativeValue` supports `None`, integers, floats, booleans, strings, complex values, and bytes/bytearray. Lists, dictionaries, exceptions, and arbitrary script objects remain available through the raw `ffi` API. `Interpreter` owns registered callbacks; callback errors are reported to the interpreter, and Rust panics are caught at the ABI boundary. A VM must be used on one thread at a time.

See the [Chinese crate README](crates/mini-python-rs/README.md), [English crate README](crates/mini-python-rs/README.en.md), and [project article](docs/mini-python-rs-introduction.md).

## Tests

```sh
cargo test --workspace
cargo fmt --all -- --check
cargo run -p mini-python-demo --bin syntax
```

## License

See [`LICENSE`](LICENSE) and the original notices in the vendored MiniPython source.
