mod module;

use mini_python_rs::Interpreter;
use std::fs;
use std::io::{self, BufRead, BufReader, Write};
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;

fn main() -> io::Result<()> {
    let address = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "127.0.0.1:7878".to_owned());
    let listener = TcpListener::bind(&address)?;
    println!("MiniPython TCP line server listening on {address}");
    let mut interpreter = Interpreter::new().map_err(io::Error::other)?;
    module::register(&mut interpreter).map_err(io::Error::other)?;
    let script_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("scripts/handler.py");
    let script = fs::read_to_string(script_path)?;
    for connection in listener.incoming() {
        match connection {
            Ok(stream) => {
                if let Err(error) = serve_client(stream, &mut interpreter, &script) {
                    eprintln!("client request failed: {error}");
                }
            }
            Err(error) => eprintln!("accept failed: {error}"),
        }
    }
    Ok(())
}

fn serve_client(
    mut stream: TcpStream,
    interpreter: &mut Interpreter,
    script: &str,
) -> io::Result<()> {
    let mut request = Vec::new();
    let read = BufReader::new(stream.try_clone()?).read_until(b'\n', &mut request)?;
    if read == 0 {
        return Ok(());
    }
    if request.last() == Some(&b'\n') {
        request.pop();
        if request.last() == Some(&b'\r') {
            request.pop();
        }
    }
    let request = String::from_utf8(request)
        .map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error))?;

    interpreter
        .set_global_string("request", &request)
        .map_err(io::Error::other)?;
    interpreter.exec(script).map_err(io::Error::other)?;
    let response = interpreter
        .get_global_string("response")
        .ok_or_else(|| io::Error::other("MiniPython did not produce a string response"))?;
    stream.write_all(response.as_bytes())?;
    stream.write_all(b"\n")
}
