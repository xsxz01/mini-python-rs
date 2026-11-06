use mini_python_rs::{Interpreter, NativeModule, NativeValue};

pub fn register(interpreter: &mut Interpreter) -> Result<(), String> {
    let module = NativeModule::new("demo_native").function("process_request", |args| match args {
        [NativeValue::String(text)] => {
            Ok(NativeValue::String(format!("MiniPython received: {text}")))
        }
        [_] => Err("process_request expects a string".to_owned()),
        _ => Err("process_request expects exactly one argument".to_owned()),
    });
    interpreter.load_native_module(module)
}
