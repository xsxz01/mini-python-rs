# 【一起学Rust | 项目实战】在 Rust 中嵌入 MiniPython：从 C++ 解释器到 Rust 原生模块

有些项目需要给用户留一层脚本能力：网络服务想让请求处理规则可以调整，桌面工具想支持用户脚本，业务程序需要把一段简单表达式交给解释器执行。

这时候通常有两种做法：直接嵌入 Python，但要处理 Python 运行时、部署和版本依赖；或者自己写一套规则语言，结果很快就要补变量、函数、模块、异常，越写越像在造解释器。

这次我把 MiniPython 的 C++ 实现接进了 Rust workspace，做成 `mini-python-rs`，再用一个 TCP demo 把 Rust → MiniPython → Rust 原生函数的调用链跑通。重点不是“Rust 重写了 Python”，而是通过已有 C API 把解释器嵌进 Rust 程序，并把少量宿主能力注册给脚本使用。

项目地址：<https://github.com/xsxz01/mini-python-rs>

---

## 一、这个库解决什么问题

这个封装主要解决三件事：

| 问题 | 做法 |
| --- | --- |
| Rust 应用要执行脚本，但不想单独部署 CPython | 将 MiniPython C++ 源码静态编译进 Rust crate |
| 脚本要调用宿主程序能力 | 通过 MiniPython 已有 C API，把 Rust 回调注册为可导入模块 |
| 只看一段孤立的 FFI 代码不容易确认是否可用 | 用 TCP 服务端、MiniPython handler 脚本和 Rust 客户端组成端到端 demo |

先说清楚边界：MiniPython 是实验性解释器，不是完整 CPython 替代品。语法、标准库和扩展兼容情况要以当前 MiniPython 实现为准。这个项目解决的是“在 Rust 程序里嵌入一个轻量解释器并接入宿主函数”，不是保证所有 Python 项目都能原样运行。

## 二、workspace 怎么组织

仓库拆成两个 Cargo package：

```text
mini-python-rs/
├── Cargo.toml                    # workspace
├── crates/
│   ├── mini-python-rs/           # Rust 库 + C++ 构建脚本
│   └── mini-python-demo/         # TCP server/client demo
└── thirdparty/3.14.0/            # MiniPython 上游 C++ 源码
```

`mini-python-rs/build.rs` 用 `cc` crate 编译解释器实现和标准库 C++ 文件，不编译原项目的命令行入口 `minipy_demo.cpp`。因此 Rust 应用链接的是静态编译进去的解释器实现，不需要运行时再加载 `.dll` 或 `.pyd`。

发布到 crates.io 时还有一个容易遗漏的点：Cargo crate 不能依赖包目录之外的 `../../thirdparty` 文件。库包会随 crate 携带一份 MiniPython C++ 源码副本；在 workspace 中开发时优先编译仓库里的上游源码，打包后则使用 crate 自带的源码。这保证消费者安装 crate 后仍能构建。

当前构建需要 C++17 编译器和本机工具链。仓库已在 Windows + MSVC 上验证；上游 `ctypes` 实现使用 Windows API，其他平台要单独确认可移植性。

## 三、Rust 侧先把解释器跑起来

最小使用方式很简单：

```toml
[dependencies]
mini-python-rs = "0.1.0"
```

然后创建 VM、执行源码：

```rust
use mini_python_rs::Interpreter;

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    vm.exec("answer = 40 + 2")?;
    Ok(())
}
```

`Interpreter` 负责持有 VM 句柄，并在 drop 时释放。MiniPython 的约定是一个 VM 只在一个线程里使用，所以这里没有把同一个解释器包装成可并发共享对象。

## 四、把 Rust 函数注册成 MiniPython 模块

解释器能执行代码还不够，脚本一般需要访问宿主能力。MiniPython 的 C API 提供了库描述、函数注册和 `mp_vm_load_lib`；Rust wrapper 把跨 ABI 的部分封装到 `NativeModule` 和 `NativeValue`。

```rust
use mini_python_rs::{Interpreter, NativeModule, NativeValue};

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
```

在 Rust 侧看起来就是一个闭包；跨到 C API 时，wrapper 会读取解释器参数、转成 `NativeValue`，再把闭包返回的值转回 MiniPython value。回调错误会通过 C API 传回解释器；Rust panic 会在 ABI 边界被捕获，不让 unwind 穿过 C++。

当前封装支持 `None`、整数、浮点数、布尔值和字符串。对复杂对象、类、动态属性等能力，应该按实际需求扩展，而不是先暴露一套调用方无法安全使用的通用裸指针接口。

## 五、TCP demo：看完整调用链

demo 的服务端监听一个 TCP 地址，每个连接读取一行请求。Rust 把请求字符串放入解释器全局变量 `request`，然后执行 `scripts/handler.py`：

```python
import demo_native

response = demo_native.process_request(request)
```

`demo_native` 不是 Python 文件，而是在 `src/module/mod.rs` 中由 Rust 注册的原生模块。处理函数返回响应字符串，服务端再把它写回 TCP 连接。

启动服务端：

```powershell
cargo run -p mini-python-demo --bin mini-python-demo
```

另开终端运行客户端：

```powershell
cargo run -p mini-python-demo --bin client -- 127.0.0.1:7878 "hello MiniPython"
```

预期输出：

```text
MiniPython received: hello MiniPython
```

这个例子不是为了展示复杂网络协议，而是把边界说清楚：TCP I/O 由 Rust 负责，请求处理策略由 MiniPython 脚本编排，特定宿主功能由 Rust 回调实现。

## 六、适合哪些场景

- **应用内规则引擎**：把过滤、转换或简单路由逻辑放到脚本侧，宿主通过受控函数暴露数据和动作。
- **桌面工具的用户扩展**：允许用户写小脚本自动化重复操作，应用本体仍由 Rust 管理。
- **网络服务的可配置处理步骤**：原型阶段可以把部分处理流程脚本化；生产服务还要考虑脚本隔离、超时和资源限制。
- **Rust/C++ 混合项目**：已有 C++ 解释器，希望在 Rust 程序里静态链接并通过统一 C API 调用。
- **嵌入式脚本实验和教学**：完整观察解释器、宿主回调、参数转换和错误返回的过程。

如果脚本来自不可信用户，不要把“嵌入式”误认为“沙箱”。执行权限、文件访问、CPU/内存限制和超时策略仍需要宿主单独设计。

## 七、手动验证与测试

运行整个 workspace 的测试：

```powershell
cargo test --workspace
```

测试覆盖 Rust 原生模块注册、多个回调分派和回调错误传递。TCP 客户端/服务端的端到端验证则确认从 socket 输入到 MiniPython 脚本、再到 Rust callback 并返回 socket 的链路可工作。

## 总结

这个项目的核心价值，是把已有 MiniPython C++ 实现接入 Rust 工程，并提供一层可以实际使用的 Rust 原生模块封装。静态链接解决部署时的解释器依赖问题，C API 负责跨语言边界，MiniPython 脚本负责可调整的处理逻辑，Rust 仍负责系统 I/O 和宿主能力。

它适合在明确解释器能力边界的前提下做嵌入和扩展。如果你的需求是完整 CPython 兼容、运行任意第三方 Python 包，或者执行不可信脚本，还需要进一步评估解释器兼容性与沙箱设计。
