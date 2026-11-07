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

/// Values supported by the safe native callback interface.
#[derive(Clone, Debug, PartialEq)]
pub enum NativeValue {
    None,
    Integer(i64),
    Float(f64),
    Boolean(bool),
    String(String),
}

type NativeFunction = Box<dyn Fn(&[NativeValue]) -> Result<NativeValue, String>>;

struct NativeCallback {
    vm: *mut c_void,
    function: NativeFunction,
}

/// A statically linked collection of Rust callbacks exposed as a MiniPython module.
pub struct NativeModule {
    name: String,
    functions: Vec<(String, NativeFunction)>,
}

impl NativeModule {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            functions: Vec::new(),
        }
    }

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
/// An embedded MiniPython interpreter. Use each instance from one thread only.
pub struct Interpreter {
    vm: NonNull<c_void>,
    native_callbacks: Vec<Box<NativeCallback>>,
}

impl Interpreter {
    /// Creates an interpreter and loads MiniPython's built-in standard library.
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

    /// Compiles and executes Python source in this interpreter's global scope.
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

    /// Loads source without executing it. Call [`Interpreter::run`] to execute it.
    pub fn load_source(&mut self, source: &str) -> Result<(), String> {
        let source = CString::new(source)
            .map_err(|_| "MiniPython source must not contain a NUL byte".to_owned())?;
        if unsafe { mp_vm_load_source(self.vm.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// Executes source already loaded with [`Interpreter::load_source`].
    pub fn run(&mut self) -> Result<(), String> {
        if unsafe { mp_vm_run(self.vm.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// Executes source using the upstream API's `__main__` module semantics.
    pub fn exec_main(&mut self, source: &str) -> Result<(), String> {
        let source = CString::new(source)
            .map_err(|_| "MiniPython source must not contain a NUL byte".to_owned())?;
        if unsafe { mp_vm_exec_main(self.vm.as_ptr(), source.as_ptr()) } == MP_OK {
            Ok(())
        } else {
            Err(self.last_error())
        }
    }

    /// Returns the pending Python exception type, if any.
    pub fn exception_type(&self) -> Option<String> {
        let value = unsafe { mp_vm_exception_type(self.vm.as_ptr()) };
        if value.is_null() {
            return None;
        }
        let name = unsafe { CStr::from_ptr(value) }.to_string_lossy();
        (!name.is_empty()).then(|| name.into_owned())
    }

    /// Clears the interpreter's pending error state.
    pub fn clear_error(&mut self) {
        unsafe { mp_vm_clear_error(self.vm.as_ptr()) }
    }

    /// Registers source code under a module namespace for later imports and calls.
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

    /// Statically registers Rust callbacks as an importable MiniPython module.
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

    /// Stores a UTF-8 string in the interpreter's global namespace.
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

    /// Reads a global string, returning `None` when the name is missing or not a string.
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
