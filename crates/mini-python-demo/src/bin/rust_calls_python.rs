use mini_python_rs::Interpreter;

fn main() -> Result<(), String> {
    let mut vm = Interpreter::new()?;
    vm.set_global_string("name", "Rust")?;
    vm.add_module(
        "greetings",
        "def welcome(person, count):\n    return 'Welcome ' + person + ' ' + str(count) + ' times\n",
    )?;
    vm.exec("import greetings\nmessage = greetings.welcome(name, 2)\n")?;
    println!(
        "Python module function result: {}",
        vm.get_global_string("message").ok_or("message missing")?
    );
    vm.exec("rust_sum = str(20 + 22)\n")?;
    println!(
        "Python expression result: {}",
        vm.get_global_string("rust_sum").ok_or("rust_sum missing")?
    );
    Ok(())
}
