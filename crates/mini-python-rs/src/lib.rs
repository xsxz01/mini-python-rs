use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt;
use std::ptr::NonNull;

const MP_OK: c_int = 0;

unsafe extern "C" {
    fn mp_vm_new() -> *mut c_void;
    fn mp_vm_free(vm: *mut c_void);
    fn mp_vm_use_stdlib(vm: *mut c_void) -> c_int;
    fn mp_vm_exec_source(vm: *mut c_void, source: *const c_char) -> c_int;
    fn mp_vm_exec_main(vm: *mut c_void, source: *const c_char) -> c_int;
    fn mp_vm_load_source(vm: *mut c_void, source: *const c_char) -> c_int;
    fn mp_vm_run(vm: *mut c_void) -> c_int;
    fn mp_vm_exception_type(vm: *mut c_void) -> *const c_char;
    fn mp_vm_clear_error(vm: *mut c_void);
    fn mp_vm_last_error(vm: *mut c_void) -> *const c_char;
    fn mp_vm_get_global(vm: *mut c_void, name: *const c_char) -> *mut c_void;
    fn mp_vm_set_global(vm: *mut c_void, name: *const c_char, value: *mut c_void) -> c_int;
    fn mp_vm_add_module(vm: *mut c_void, name: *const c_char, source: *const c_char) -> c_int;
    fn mp_str_n(vm: *mut c_void, utf8: *const c_char, len: usize) -> *mut c_void;
    fn mp_value_as_str_n(value: *mut c_void, out_len: *mut usize) -> *const c_char;
    fn mp_value_release(value: *mut c_void);
    fn mp_value_kind(value: *mut c_void) -> c_int;
    fn mp_value_type_name(value: *mut c_void) -> *const c_char;
    fn mp_value_as_int(value: *mut c_void) -> i64;
    fn mp_value_as_float(value: *mut c_void) -> f64;
    fn mp_value_as_bool(value: *mut c_void) -> c_int;
    fn mp_none(vm: *mut c_void) -> *mut c_void;
    fn mp_int(vm: *mut c_void, value: i64) -> *mut c_void;
    fn mp_float(vm: *mut c_void, value: f64) -> *mut c_void;
    fn mp_bool(vm: *mut c_void, value: c_int) -> *mut c_void;
    fn mp_lib_new(name: *const c_char) -> *mut c_void;
    fn mp_lib_free(lib: *mut c_void);
    fn mp_lib_add_function(
        lib: *mut c_void,
        name: *const c_char,
        callback: *mut c_void,
        user: *mut c_void,
    ) -> c_int;
    fn mp_vm_load_lib(vm: *mut c_void, lib: *mut c_void) -> c_int;
    fn mp_ctx_argc(ctx: *mut c_void) -> usize;
    fn mp_ctx_arg(ctx: *mut c_void, index: usize) -> *mut c_void;
    fn mp_ctx_error(ctx: *mut c_void, kind: *const c_char, message: *const c_char);
}

/// Rust 原生回调接口支持的 Python 标量值。
///
/// 目前只允许这五种类型跨越原生回调边界；列表、字典及任意 Python 对象尚未封装。
///
/// # 示例
/// ```
/// use mini_python_rs::NativeValue;
///
/// let value = NativeValue::Integer(42);
/// assert_eq!(value, NativeValue::Integer(42));
/// ```
#[derive(Clone, Debug, PartialEq)]
pub enum NativeValue {
    /// Python 的 `None`。
    None,
    /// Python 整数，对应 Rust `i64`。
    Integer(i64),
    /// Python 浮点数，对应 Rust `f64`。
    Float(f64),
    /// Python 布尔值。
    Boolean(bool),
    /// Python UTF-8 字符串。
    String(String),
}

type NativeFunction = Box<dyn Fn(&[NativeValue]) -> Result<NativeValue, String>>;

struct NativeCallback {
    vm: *mut c_void,
    function: NativeFunction,
}

/// 可导入的 Rust 原生模块。
///
/// 通过 [`Interpreter::load_native_module`] 注册后，Python 可使用 `import <name>` 导入。
///
/// # 示例
/// ```
/// use mini_python_rs::{NativeModule, NativeValue};
///
/// let module = NativeModule::new("native_math").function("add", |args| match args {
///     [NativeValue::Integer(a), NativeValue::Integer(b)] => {
///         Ok(NativeValue::Integer(a + b))
///     }
///     _ => Err("add 需要两个整数".to_owned()),
/// });
/// ```
pub struct NativeModule {
    name: String,
    functions: Vec<(String, NativeFunction)>,
}

impl NativeModule {
    /// 创建一个空的原生模块描述。
    ///
    /// # 参数
    /// - `name`：Python 侧导入的模块名。
    ///
    /// # 返回值
    /// 可继续链式添加回调的模块描述。
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            functions: Vec::new(),
        }
    }

    /// 向模块注册一个 Rust 回调。
    ///
    /// 回调接收位置参数切片，成功时返回 `NativeValue`，失败时返回错误信息；错误会作为
    /// Python `RuntimeError` 传播。回调闭包必须满足 `'static`，并由对应的解释器持有。
    ///
    /// # 参数
    /// - `name`：Python 侧函数名。
    /// - `function`：处理参数并返回值或错误的闭包。
    ///
    /// # 返回值
    /// 更新后的模块描述，用于继续链式注册。
    pub fn function<F>(mut self, name: impl Into<String>, function: F) -> Self
    where
        F: Fn(&[NativeValue]) -> Result<NativeValue, String> + 'static,
    {
        self.functions.push((name.into(), Box::new(function)));
        self
    }
}

unsafe extern "C" fn native_trampoline(
    user: *mut c_void,
    ctx: *mut c_void,
    out: *mut *mut c_void,
) -> c_int {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if user.is_null() || ctx.is_null() || out.is_null() {
            return Err("invalid native callback context".to_owned());
        }
        // SAFETY: user points to a boxed callback retained by its Interpreter.
        let callback = unsafe { &mut *user.cast::<NativeCallback>() };
        let count = unsafe { mp_ctx_argc(ctx) };
        let mut args = Vec::with_capacity(count);
        for index in 0..count {
            let handle = unsafe { mp_ctx_arg(ctx, index) };
            if handle.is_null() {
                return Err(format!("failed to read argument {index}"));
            }
            let value = unsafe { native_value_from_handle(handle) };
            unsafe { mp_value_release(handle) };
            args.push(value?);
        }
        let value = (callback.function)(&args)?;
        let handle = unsafe { native_value_to_handle(callback.vm, value) };
        if handle.is_null() {
            return Err("failed to construct native return value".to_owned());
        }
        unsafe { out.write(handle) };
        Ok(())
    }));

    match result {
        Ok(Ok(())) => MP_OK,
        Ok(Err(message)) => {
            set_callback_error(ctx, &message);
            -1
        }
        Err(_) => {
            set_callback_error(ctx, "Rust native callback panicked");
            -1
        }
    }
}

unsafe fn native_value_from_handle(handle: *mut c_void) -> Result<NativeValue, String> {
    match unsafe { mp_value_kind(handle) } {
        0 => Ok(NativeValue::None),
        1 => {
            let kind = unsafe { mp_value_type_name(handle) };
            if !kind.is_null() && unsafe { CStr::from_ptr(kind) }.to_bytes() == b"bool" {
                Ok(NativeValue::Boolean(
                    unsafe { mp_value_as_bool(handle) } != 0,
                ))
            } else {
                Ok(NativeValue::Integer(unsafe { mp_value_as_int(handle) }))
            }
        }
        2 => Ok(NativeValue::Float(unsafe { mp_value_as_float(handle) })),
        3 => {
            let mut len = 0;
            let text = unsafe { mp_value_as_str_n(handle, &mut len) };
            if text.is_null() {
                return Err("failed to read string argument".to_owned());
            }
            let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), len) };
            Ok(NativeValue::String(
                String::from_utf8_lossy(bytes).into_owned(),
            ))
        }
        kind => Err(format!("unsupported MiniPython argument kind {kind}")),
    }
}

unsafe fn native_value_to_handle(vm: *mut c_void, value: NativeValue) -> *mut c_void {
    match value {
        NativeValue::None => unsafe { mp_none(vm) },
        NativeValue::Integer(value) => unsafe { mp_int(vm, value) },
        NativeValue::Float(value) => unsafe { mp_float(vm, value) },
        NativeValue::Boolean(value) => unsafe { mp_bool(vm, i32::from(value)) },
        NativeValue::String(value) => unsafe { mp_str_n(vm, value.as_ptr().cast(), value.len()) },
    }
}

fn set_callback_error(ctx: *mut c_void, message: &str) {
    let message = CString::new(message)
        .unwrap_or_else(|_| CString::new("callback error contained NUL").unwrap());
    unsafe { mp_ctx_error(ctx, c"RuntimeError".as_ptr(), message.as_ptr()) };
}

struct NativeLibraryHandle(*mut c_void);

impl Drop for NativeLibraryHandle {
    fn drop(&mut self) {
        unsafe { mp_lib_free(self.0) };
    }
}
/// 嵌入式 MiniPython 解释器。每个实例只能在一个线程中使用；不要跨线程并发访问同一实例。
pub struct Interpreter {
    vm: NonNull<c_void>,
    native_callbacks: Vec<Box<NativeCallback>>,
}

impl Interpreter {
    /// 创建解释器并加载 MiniPython 内置标准库。
    ///
    /// # 返回值
    /// 成功返回新解释器；分配 VM 或初始化标准库失败时返回错误文本。
    ///
    /// # 示例
    /// ```
    /// use mini_python_rs::Interpreter;
    ///
    /// let mut vm = Interpreter::new()?;
    /// vm.exec("answer = str(40 + 2)")?;
    /// assert_eq!(vm.get_global_string("answer").as_deref(), Some("42"));
    /// # Ok::<(), String>(())
    /// ```
    pub fn new() -> Result<Self, String> {
        // SAFETY: The C API returns an opaque owned VM handle or null.
        let vm = NonNull::new(unsafe { mp_vm_new() })
            .ok_or_else(|| "MiniPython failed to allocate an interpreter".to_owned())?;
        let interpreter = Self {
            vm,
            native_callbacks: Vec::new(),
        };
        // SAFETY: vm is a valid handle owned by this instance.
        if unsafe { mp_vm_use_stdlib(vm.as_ptr()) } != MP_OK {
            return Err(interpreter.last_error());
        }
        Ok(interpreter)
    }

    /// 编译并执行 Python 源码，顶层变量写入此解释器的全局作用域。
    ///
    /// # 参数
    /// - `source`：UTF-8 Python 源码；不能包含 NUL 字节。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；编译或执行失败时返回 traceback 文本。
    ///
    /// # 示例
    /// ```
    /// # use mini_python_rs::Interpreter;
    /// # let mut vm = Interpreter::new()?;
    /// vm.exec("message = '你好'")?;
    /// assert_eq!(vm.get_global_string("message").as_deref(), Some("你好"));
    /// # Ok::<(), String>(())
    /// ```
    pub fn exec(&mut self, source: &str) -> Result<(), String> {
        let source = CString::new(source)
            .map_err(|_| "MiniPython source must not contain a NUL byte".to_owned())?;
        // SAFETY: vm is valid and source is NUL-terminated for the duration of the call.
        if unsafe { mp_vm_exec_source(self.vm.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 只编译并装载源码，不立即执行；随后调用 [`Interpreter::run`]。
    ///
    /// # 参数
    /// - `source`：UTF-8 Python 源码；不能包含 NUL 字节。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；源码装载失败时返回 traceback 文本。
    pub fn load_source(&mut self, source: &str) -> Result<(), String> {
        let source = CString::new(source)
            .map_err(|_| "MiniPython source must not contain a NUL byte".to_owned())?;
        if unsafe { mp_vm_load_source(self.vm.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 执行先前通过 [`Interpreter::load_source`] 装载的源码。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；Python 执行失败时返回 traceback 文本。
    pub fn run(&mut self) -> Result<(), String> {
        if unsafe { mp_vm_run(self.vm.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 以 Python `__main__` 模块语义编译并执行源码。
    ///
    /// # 参数
    /// - `source`：UTF-8 Python 源码；不能包含 NUL 字节。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；编译或执行失败时返回 traceback 文本。
    ///
    /// # 示例
    /// ```
    /// # use mini_python_rs::Interpreter;
    /// # let mut vm = Interpreter::new()?;
    /// vm.exec_main("def twice(x):\n    return x * 2\nanswer = str(twice(21))")?;
    /// assert_eq!(vm.get_global_string("answer").as_deref(), Some("42"));
    /// # Ok::<(), String>(())
    /// ```
    pub fn exec_main(&mut self, source: &str) -> Result<(), String> {
        let source = CString::new(source)
            .map_err(|_| "MiniPython source must not contain a NUL byte".to_owned())?;
        if unsafe { mp_vm_exec_main(self.vm.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 查询当前待处理 Python 异常的类型名。
    ///
    /// # 返回值
    /// 有待处理异常时返回类型名（如 `Some("ValueError")`）；无异常或名称为空时返回 `None`。
    pub fn exception_type(&self) -> Option<String> {
        let value = unsafe { mp_vm_exception_type(self.vm.as_ptr()) };
        if value.is_null() {
            return None;
        }
        let name = unsafe { CStr::from_ptr(value) }.to_string_lossy();
        (!name.is_empty()).then(|| name.into_owned())
    }

    /// 清除解释器保存的待处理错误状态。
    ///
    /// 本方法不返回值；调用前可用 [`Interpreter::exception_type`] 查询异常类型。
    pub fn clear_error(&mut self) {
        unsafe { mp_vm_clear_error(self.vm.as_ptr()) }
    }

    /// 将 Python 源码注册为可导入模块。
    ///
    /// # 参数
    /// - `name`：模块导入名。
    /// - `source`：模块源码；模块名和源码均不能包含 NUL 字节。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；注册或编译失败时返回解释器错误文本。
    ///
    /// # 示例
    /// ```
    /// # use mini_python_rs::Interpreter;
    /// # let mut vm = Interpreter::new()?;
    /// vm.add_module("greetings", "def greet(name):\n    return 'Hi, ' + name")?;
    /// vm.exec_main("import greetings\nanswer = str(greetings.greet('Ada'))")?;
    /// assert_eq!(vm.get_global_string("answer").as_deref(), Some("Hi, Ada"));
    /// # Ok::<(), String>(())
    /// ```
    pub fn add_module(&mut self, name: &str, source: &str) -> Result<(), String> {
        let name = CString::new(name).map_err(|_| "module name contains a NUL byte".to_owned())?;
        let source = CString::new(source)
            .map_err(|_| "module source must not contain a NUL byte".to_owned())?;
        // SAFETY: vm is valid; both strings remain NUL-terminated during the call.
        if unsafe { mp_vm_add_module(self.vm.as_ptr(), name.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 将 Rust 回调集合静态注册到解释器，供 Python 导入和调用。
    ///
    /// # 参数
    /// - `module`：由 [`NativeModule`] 构造的模块描述；回调只能传递 [`NativeValue`] 支持的标量。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；模块、函数注册或加载失败时返回解释器错误文本。
    ///
    /// # 示例
    /// ```
    /// # use mini_python_rs::{Interpreter, NativeModule, NativeValue};
    /// # let mut vm = Interpreter::new()?;
    /// let module = NativeModule::new("native_math").function("add", |args| match args {
    ///     [NativeValue::Integer(a), NativeValue::Integer(b)] => Ok(NativeValue::Integer(a + b)),
    ///     _ => Err("需要两个整数".to_owned()),
    /// });
    /// vm.load_native_module(module)?;
    /// vm.exec_main("from native_math import add\nanswer = str(add(20, 22))")?;
    /// assert_eq!(vm.get_global_string("answer").as_deref(), Some("42"));
    /// # Ok::<(), String>(())
    /// ```
    pub fn load_native_module(&mut self, module: NativeModule) -> Result<(), String> {
        let name = CString::new(module.name)
            .map_err(|_| "native module name contains a NUL byte".to_owned())?;
        let library = unsafe { mp_lib_new(name.as_ptr()) };
        if library.is_null() {
            return Err("failed to allocate native module descriptor".to_owned());
        }
        let library = NativeLibraryHandle(library);

        for (function_name, function) in module.functions {
            let function_name = CString::new(function_name)
                .map_err(|_| "native function name contains a NUL byte".to_owned())?;
            let mut callback = Box::new(NativeCallback {
                vm: self.vm.as_ptr(),
                function,
            });
            let user = (&mut *callback as *mut NativeCallback).cast::<c_void>();
            self.native_callbacks.push(callback);
            // SAFETY: callback is retained by self; the C API stores its user pointer until VM teardown.
            let status = unsafe {
                mp_lib_add_function(
                    library.0,
                    function_name.as_ptr(),
                    native_trampoline as *const () as *mut c_void,
                    user,
                )
            };
            if status != MP_OK {
                return Err(self.last_error());
            }
        }

        // The C API copies the library into the VM and retains callback nodes after this descriptor is freed.
        let status = unsafe { mp_vm_load_lib(self.vm.as_ptr(), library.0) };
        if status == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 将 UTF-8 字符串设置到解释器全局作用域。
    ///
    /// # 参数
    /// - `name`：全局变量名，不能包含 NUL 字节。
    /// - `value`：字符串内容；支持 UTF-8 和内嵌 NUL 字节。
    ///
    /// # 返回值
    /// 成功返回 `Ok(())`；名称或值构造/设置失败时返回错误文本。
    pub fn set_global_string(&mut self, name: &str, value: &str) -> Result<(), String> {
        let name = CString::new(name).map_err(|_| "global name contains a NUL byte".to_owned())?;
        // mp_str_n accepts an explicit byte length, including embedded NULs.
        let value = unsafe { mp_str_n(self.vm.as_ptr(), value.as_ptr().cast(), value.len()) };
        if value.is_null() {
            return Err(self.last_error());
        }
        // The temporary handle is caller-owned regardless of whether setting succeeds.
        let status = unsafe { mp_vm_set_global(self.vm.as_ptr(), name.as_ptr(), value) };
        unsafe { mp_value_release(value) };
        if status == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// 读取全局作用域中的字符串变量。
    ///
    /// # 参数
    /// - `name`：要查询的全局变量名。
    ///
    /// # 返回值
    /// 变量存在且为字符串时返回其内容；名称含 NUL、变量不存在或变量不是字符串时返回 `None`。
    pub fn get_global_string(&self, name: &str) -> Option<String> {
        let name = CString::new(name).ok()?;
        let value = unsafe { mp_vm_get_global(self.vm.as_ptr(), name.as_ptr()) };
        if value.is_null() {
            return None;
        }
        let mut length = 0;
        let text = unsafe { mp_value_as_str_n(value, &mut length) };
        let output = if text.is_null() {
            None
        } else {
            Some(
                String::from_utf8_lossy(unsafe {
                    std::slice::from_raw_parts(text.cast::<u8>(), length)
                })
                .into_owned(),
            )
        };
        unsafe { mp_value_release(value) };
        output
    }

    fn last_error(&self) -> String {
        // SAFETY: C API returns a VM-owned UTF-8 C string valid until the next VM call.
        let error = unsafe { mp_vm_last_error(self.vm.as_ptr()) };
        if error.is_null() {
            return "MiniPython operation failed without an error message".to_owned();
        }
        // Copy before another VM call; lossy conversion avoids UB for malformed C strings.
        unsafe { CStr::from_ptr(error) }
            .to_string_lossy()
            .into_owned()
    }
}

impl Drop for Interpreter {
    fn drop(&mut self) {
        // SAFETY: This instance uniquely owns the VM and has no outstanding value handles.
        unsafe { mp_vm_free(self.vm.as_ptr()) }
    }
}

impl fmt::Debug for Interpreter {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("Interpreter")
            .finish_non_exhaustive()
    }
}

#[cfg(test)]
mod tests {
    use super::Interpreter;
    use std::sync::{LazyLock, Mutex, MutexGuard};

    static TEST_LOCK: LazyLock<Mutex<()>> = LazyLock::new(|| Mutex::new(()));

    fn test_lock() -> MutexGuard<'static, ()> {
        TEST_LOCK.lock().expect("test lock")
    }

    #[test]
    fn executes_source_and_reports_python_errors() {
        let _guard = test_lock();
        let mut interpreter = Interpreter::new().expect("interpreter");
        interpreter.exec("answer = 40 + 2").expect("valid source");
        let error = interpreter.exec("raise ValueError('broken')").unwrap_err();
        assert!(error.contains("ValueError"), "unexpected error: {error}");
    }

    #[test]
    fn registers_namespaced_script_modules() {
        let _guard = test_lock();
        let mut interpreter = Interpreter::new().expect("interpreter");
        interpreter
            .add_module("tools", "def greet(name):\n    return 'greet: ' + name\n")
            .expect("register module");
        interpreter
            .exec("import tools\nresult = tools.greet('Ada')")
            .expect("execute module function");
        assert_eq!(
            interpreter.get_global_string("result").as_deref(),
            Some("greet: Ada")
        );
    }

    #[test]
    fn invokes_multiple_rust_native_module_functions() {
        let _guard = test_lock();
        use super::{NativeModule, NativeValue};

        let mut interpreter = Interpreter::new().expect("interpreter");
        let module = NativeModule::new("rust_math")
            .function("add", |args| match args {
                [NativeValue::Integer(a), NativeValue::Integer(b)] => {
                    Ok(NativeValue::Integer(a + b))
                }
                _ => Err("add expects two integers".to_owned()),
            })
            .function("greet", |args| match args {
                [NativeValue::String(name)] => Ok(NativeValue::String(format!("hello {name}"))),
                _ => Err("greet expects one string".to_owned()),
            });
        interpreter
            .load_native_module(module)
            .expect("load Rust module");
        interpreter
            .exec(
                "import rust_math\nsum_text = str(rust_math.add(12, 34))\ngreeting = rust_math.greet('Ada')",
            )
            .expect("call Rust functions from MiniPython");
        assert_eq!(
            interpreter.get_global_string("sum_text").as_deref(),
            Some("46")
        );
        assert_eq!(
            interpreter.get_global_string("greeting").as_deref(),
            Some("hello Ada")
        );
    }

    #[test]
    fn exposes_source_loading_main_execution_and_error_state_apis() {
        let _guard = test_lock();
        let mut interpreter = Interpreter::new().expect("interpreter");
        interpreter
            .load_source("loaded_value = 40 + 2")
            .expect("load source");
        interpreter.run().expect("run loaded source");

        interpreter
            .exec_main("main_value = 'main'")
            .expect("execute as main");
        assert_eq!(
            interpreter.get_global_string("main_value").as_deref(),
            Some("main")
        );

        assert!(interpreter.exec_main("raise ValueError('bad')").is_err());
        assert_eq!(interpreter.exception_type().as_deref(), Some("ValueError"));
        interpreter.clear_error();
        assert_eq!(interpreter.exception_type(), None);
    }

    #[test]
    fn propagates_native_callback_errors_into_minipython() {
        let _guard = test_lock();
        use super::NativeModule;

        let mut interpreter = Interpreter::new().expect("interpreter");
        let module = NativeModule::new("rust_errors")
            .function("fail", |_| Err("native test failure".to_owned()));
        interpreter.load_native_module(module).expect("load module");
        let error = interpreter
            .exec("import rust_errors\nrust_errors.fail()")
            .unwrap_err();
        assert!(
            error.contains("native test failure"),
            "unexpected error: {error}"
        );
    }
}
