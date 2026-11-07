use mini_python_rs::{Interpreter, NativeModule, NativeValue};

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    let echo = NativeModule::new("types_native").function("describe", |args| match args {
        [NativeValue::None] => Ok(NativeValue::String("None".to_owned())),
        [NativeValue::Integer(value)] => Ok(NativeValue::String(format!("int:{value}"))),
        [NativeValue::Float(value)] => Ok(NativeValue::String(format!("float:{value}"))),
        [NativeValue::Boolean(value)] => Ok(NativeValue::String(format!("bool:{value}"))),
        [NativeValue::String(value)] => Ok(NativeValue::String(format!("str:{value}"))),
        _ => Err("describe expects one supported scalar".to_owned()),
    });
    vm.load_native_module(echo)?;
    vm.exec(
        "import types_native\na = types_native.describe(None)\nb = types_native.describe(42)\nc = types_native.describe(3.5)\nd = types_native.describe(True)\ne = types_native.describe('hello')\nsummary = a + ', ' + b + ', ' + c + ', ' + d + ', ' + e\n",
    )?;
    println!(
        "Python -> Rust scalar values: {}",
        vm.get_global_string("summary").ok_or("summary missing")?
    );
    Ok(())
}
