# mini-python-demo

TCP line-server demo backed by MiniPython and a statically linked Rust native module.

Start the server:

```sh
cargo run -p mini-python-demo --bin mini-python-demo
```

The server listens on `127.0.0.1:7878` by default. Pass an address as the first argument to override it. In another terminal, send one line and receive one line:

```sh
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello"
```

The Rust module is registered in `src/module/mod.rs` as `demo_native`; `scripts/handler.py` imports it and calls `process_request`. The Rust callbacks are linked into the executable and registered through MiniPython's C API; no dynamic module file is loaded.

## Language-boundary examples

Each command runs a focused example:

```sh
cargo run -p mini-python-demo --bin rust_calls_python
cargo run -p mini-python-demo --bin python_calls_rust
cargo run -p mini-python-demo --bin data_types
cargo run -p mini-python-demo --bin syntax
```

`rust_calls_python` passes a string global from Rust, executes a Python-defined function, and imports a source module registered with `Interpreter::add_module`. `python_calls_rust` imports `demo_native` and invokes Rust callbacks with multiple typed arguments. `data_types` demonstrates the five scalar variants supported by `NativeValue`: `None`, `Integer(i64)`, `Float(f64)`, `Boolean`, and `String`. The wrapper does not expose Python collection/object values through native callbacks or general typed globals.


失败语法复现脚本见 [`scripts/syntax_failures.py`](scripts/syntax_failures.py)，说明见 [`FAILED_SYNTAX.md`](FAILED_SYNTAX.md)。
See [`SYNTAX.md`](SYNTAX.md) for parser-backed syntax coverage and the unverified/unsupported boundary. The inventory describes parser paths, not full CPython semantic compatibility.
