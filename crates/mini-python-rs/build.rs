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
    build.cpp(true).std("c++20").include(&root);

    build.flag_if_supported("/utf-8");
    for source in [
        "minipy_capi.cpp",
        "vmware.cpp",
        "cffi.cpp",
        "parser.cpp",
        "lexer.cpp",
        "platform.cpp",
    ] {
        let path = root.join(source);
        build.file(&path);
        println!("cargo:rerun-if-changed={}", path.display());
    }

    let lib = root.join("lib");
    let mut sources = std::fs::read_dir(&lib)
        .expect("MiniPython standard-library source directory")
        .map(|entry| entry.expect("MiniPython source entry").path())
        .filter(|path| path.extension().is_some_and(|extension| extension == "cpp"))
        .collect::<Vec<_>>();
    sources.sort();
    for path in sources {
        build.file(&path);
        println!("cargo:rerun-if-changed={}", path.display());
    }

    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    if matches!(target_os.as_str(), "linux" | "android") {
        println!("cargo:rustc-link-lib=dl");
    }

    // WebAssembly targets need an explicit C++ sysroot (libc + libc++) and a
    // clang that can target wasm32. Configure it without changing native builds.
    let target_arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    if target_arch == "wasm32" {
        // Upstream platform.cpp calls __builtin___clear_cache(); LLVM's wasm
        // backend rejects llvm.clear_cache and WebAssembly has no instruction
        // cache to flush, so make that single builtin a no-op.
        build.flag("-D__builtin___clear_cache(begin,end)=((void)0)");

        println!("cargo:rerun-if-env-changed=MINIPY_WASM_CXX");
        if let Ok(compiler) = std::env::var("MINIPY_WASM_CXX") {
            build.compiler(compiler);
        }
        println!("cargo:rerun-if-env-changed=MINIPY_WASM_SYSROOT");
        if let Ok(sysroot) = std::env::var("MINIPY_WASM_SYSROOT") {
            build.flag(format!("--sysroot={sysroot}"));
        }
        println!("cargo:rerun-if-env-changed=MINIPY_WASM_CXXFLAGS");
        if let Ok(extra) = std::env::var("MINIPY_WASM_CXXFLAGS") {
            for flag in extra.split_whitespace() {
                build.flag(flag);
            }
        }
        println!("cargo:rerun-if-env-changed=MINIPY_WASM_LINK_SEARCH");
        if let Ok(search) = std::env::var("MINIPY_WASM_LINK_SEARCH") {
            for dir in std::env::split_paths(&search) {
                println!("cargo:rustc-link-search=native={}", dir.display());
            }
        }
        println!("cargo:rerun-if-env-changed=MINIPY_WASM_LINK_LIBS");
        if let Ok(libs) = std::env::var("MINIPY_WASM_LINK_LIBS") {
            for lib in libs.split(',').map(str::trim).filter(|lib| !lib.is_empty()) {
                println!("cargo:rustc-link-lib={lib}");
            }
        }
    }

    println!("cargo:rerun-if-changed={}", root.join("minipy.h").display());
    println!(
        "cargo:rerun-if-changed={}",
        root.join("platform.hpp").display()
    );
    build.compile("minipy");
}
