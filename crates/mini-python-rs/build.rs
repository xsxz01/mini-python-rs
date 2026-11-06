use std::path::PathBuf;

fn main() {
    let manifest =
        PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").expect("manifest directory"));
    let upstream = manifest.join("../../thirdparty/3.14.0/src");
    let root = if upstream.is_dir() {
        upstream
    } else {
        manifest.join("vendor/mini-python/src")
    };
    println!("cargo:rerun-if-changed={}", root.display());
    let mut build = cc::Build::new();
    build.cpp(true).std("c++17").include(&root);

    build.flag_if_supported("/utf-8");
    for source in [
        "minipy_capi.cpp",
        "vmware.cpp",
        "cffi.cpp",
        "parser.cpp",
        "lexer.cpp",
    ] {
        build.file(root.join(source));
        println!("cargo:rerun-if-changed={}", root.join(source).display());
    }
    for source in [
        "base64.cpp",
        "bisect.cpp",
        "builtins.cpp",
        "copy.cpp",
        "ctypes.cpp",
        "heapq.cpp",
        "itertools.cpp",
        "json.cpp",
        "math.cpp",
        "operator.cpp",
        "os.cpp",
        "os_path.cpp",
        "random.cpp",
        "re.cpp",
        "statistics.cpp",
        "string.cpp",
        "sys.cpp",
        "time.cpp",
    ] {
        let path = root.join("lib").join(source);
        build.file(&path);
        println!("cargo:rerun-if-changed={}", path.display());
    }
    println!("cargo:rerun-if-changed={}", root.join("minipy.h").display());
    build.compile("minipy");
}
