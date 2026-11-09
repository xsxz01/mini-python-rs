//! Rust-side target metadata for MiniPython's upstream portable platform layer.
//!
//! The C++ platform layer remains part of the interpreter build on every target; the
//! `cross-platform` Cargo feature exposes this Rust target description to consumers.

/// Target family selected by the Rust compiler for this build.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Target {
    Windows,
    Linux,
    MacOS,
    IOS,
    Android,
    HarmonyOS,
    Emscripten,
    Wasi,
    WebAssembly,
    Other,
}

impl Target {
    /// Returns the target selected at compile time, not the host running the process.
    pub const fn current() -> Self {
        if cfg!(target_os = "windows") {
            Self::Windows
        } else if cfg!(target_env = "ohos") {
            Self::HarmonyOS
        } else if cfg!(target_os = "linux") {
            Self::Linux
        } else if cfg!(target_os = "macos") {
            Self::MacOS
        } else if cfg!(target_os = "ios") {
            Self::IOS
        } else if cfg!(target_os = "android") {
            Self::Android
        } else if cfg!(target_os = "emscripten") {
            Self::Emscripten
        } else if cfg!(target_os = "wasi") {
            Self::Wasi
        } else if cfg!(target_arch = "wasm32") {
            Self::WebAssembly
        } else {
            Self::Other
        }
    }

    /// Whether the Rust target is a WebAssembly target.
    pub const fn is_webassembly(self) -> bool {
        matches!(self, Self::Emscripten | Self::Wasi | Self::WebAssembly)
    }
}
