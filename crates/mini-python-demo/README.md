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
