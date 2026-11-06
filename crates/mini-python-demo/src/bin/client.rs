use std::env;
use std::io::{self, BufRead, BufReader, Write};
use std::net::TcpStream;

fn main() -> io::Result<()> {
    let mut arguments = env::args().skip(1);
    let address = arguments
        .next()
        .unwrap_or_else(|| "127.0.0.1:7878".to_owned());
    let message = arguments
        .next()
        .unwrap_or_else(|| "hello MiniPython".to_owned());

    let mut stream = TcpStream::connect(&address)?;
    stream.write_all(message.as_bytes())?;
    stream.write_all(b"\n")?;
    stream.flush()?;

    let mut response = String::new();
    BufReader::new(stream).read_line(&mut response)?;
    print!("{response}");
    Ok(())
}
