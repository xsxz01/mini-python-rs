use mini_python_rs::{Interpreter, NativeModule, NativeValue};

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    let native = NativeModule::new("demo_native")
        .function("format_user", |args| match args {
            [
                NativeValue::String(name),
                NativeValue::Integer(age),
                NativeValue::Boolean(active),
            ] => Ok(NativeValue::String(format!("{name}:{age}:active={active}"))),
            _ => Err("format_user expects (str, int, bool)".to_owned()),
        })
        .function("double", |args| match args {
            [NativeValue::Float(value)] => Ok(NativeValue::Float(value * 2.0)),
            _ => Err("double expects one float".to_owned()),
        });
    vm.load_native_module(native)?;
    vm.exec(
        "import demo_native\nuser = demo_native.format_user('Ada', 37, True)\nvalue = demo_native.double(1.25)\nresult = user + ' / ' + str(value)\n",
    )?;
    println!(
        "Rust callback result: {}",
        vm.get_global_string("result").ok_or("result missing")?
    );
    Ok(())
}
