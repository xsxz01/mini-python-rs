//! Raw bindings to MiniPython's complete C API from `minipy.h`.
//!
//! Every function in this module is unsafe. Callers must uphold the upstream C API's
//! handle ownership, lifetime, aliasing, callback and thread-affinity requirements.
//! Functions returning owned handles or buffers require the matching release/free
//! function documented by the C API. A VM must outlive every handle created from it.

use std::ffi::{c_char, c_int, c_void};

pub const MP_OK: c_int = 0;
pub const MP_ERROR: c_int = -1;
pub const MP_TYPE_ERROR: c_int = -2;
pub const MP_NOT_FOUND: c_int = -3;

pub const MP_KIND_NONE: c_int = 0;
pub const MP_KIND_INT: c_int = 1;
pub const MP_KIND_FLOAT: c_int = 2;
pub const MP_KIND_STR: c_int = 3;
pub const MP_KIND_LIST: c_int = 4;
pub const MP_KIND_TUPLE: c_int = 5;
pub const MP_KIND_DICT: c_int = 6;
pub const MP_KIND_RANGE: c_int = 7;
pub const MP_KIND_OBJECT: c_int = 8;
pub const MP_KIND_NATIVE: c_int = 9;
pub const MP_KIND_FUNC: c_int = 10;
pub const MP_KIND_MODULE: c_int = 11;
pub const MP_KIND_OTHER: c_int = 12;
pub const MP_KIND_COMPLEX: c_int = 13;
pub const MP_KIND_BYTES: c_int = 14;
pub const MP_KIND_EXCEPTION: c_int = 15;

/// Native callback ABI. Registration APIs accept this callback as an opaque `void*`.
pub type NativeFunctionCallback =
    unsafe extern "C" fn(user: *mut c_void, ctx: *mut c_void, out: *mut *mut c_void) -> c_int;
/// Native data getter ABI. Registration APIs accept this callback as an opaque `void*`.
pub type NativeDataGetter = unsafe extern "C" fn(user: *mut c_void, vm: *mut c_void) -> *mut c_void;

unsafe extern "C" {
    pub fn mp_vm_new() -> *mut c_void;
    pub fn mp_vm_free(vm: *mut c_void);
    pub fn mp_vm_use_stdlib(vm: *mut c_void) -> c_int;
    pub fn mp_vm_add_module(vm: *mut c_void, name: *const c_char, source: *const c_char) -> c_int;
    pub fn mp_vm_load_pyd(
        vm: *mut c_void,
        name: *const c_char,
        data: *const u8,
        size: usize,
    ) -> c_int;
    pub fn mp_vm_load_source(vm: *mut c_void, source: *const c_char) -> c_int;
    pub fn mp_vm_load_bytecode(vm: *mut c_void, data: *const u8, size: usize) -> c_int;
    pub fn mp_vm_compile_bytecode(
        vm: *mut c_void,
        source: *const c_char,
        out: *mut *mut u8,
        out_size: *mut usize,
    ) -> c_int;
    pub fn mp_vm_run(vm: *mut c_void) -> c_int;
    pub fn mp_vm_exec_source(vm: *mut c_void, source: *const c_char) -> c_int;
    pub fn mp_vm_exec_main(vm: *mut c_void, source: *const c_char) -> c_int;

    pub fn mp_vm_last_error(vm: *mut c_void) -> *const c_char;
    pub fn mp_vm_has_error(vm: *mut c_void) -> c_int;
    pub fn mp_vm_exception_type(vm: *mut c_void) -> *const c_char;
    pub fn mp_vm_clear_error(vm: *mut c_void);
    pub fn mp_vm_raise(vm: *mut c_void, kind: *const c_char, message: *const c_char);
    pub fn mp_vm_save_error(vm: *mut c_void) -> *mut c_void;
    pub fn mp_vm_restore_error(vm: *mut c_void, state: *mut c_void);
    pub fn mp_error_state_free(state: *mut c_void);

    pub fn mp_vm_get_global(vm: *mut c_void, name: *const c_char) -> *mut c_void;
    pub fn mp_vm_set_global(vm: *mut c_void, name: *const c_char, value: *mut c_void) -> c_int;
    pub fn mp_vm_del_global(vm: *mut c_void, name: *const c_char) -> c_int;
    pub fn mp_vm_global_names(
        vm: *mut c_void,
        names: *mut *mut *mut c_char,
        count: *mut usize,
    ) -> c_int;
    pub fn mp_free_names(names: *mut *mut c_char, count: usize);

    pub fn mp_vm_find_class(vm: *mut c_void, name: *const c_char, out: *mut *mut c_void) -> c_int;
    pub fn mp_vm_find_function(
        vm: *mut c_void,
        name: *const c_char,
        out: *mut *mut c_void,
    ) -> c_int;
    pub fn mp_vm_has_lib(vm: *mut c_void, name: *const c_char) -> c_int;
    pub fn mp_vm_heap_count(vm: *mut c_void, out_count: *mut i64) -> c_int;
    pub fn mp_vm_heap_snapshot(
        vm: *mut c_void,
        out: *mut *mut *mut c_void,
        out_count: *mut usize,
    ) -> c_int;

    pub fn mp_none(vm: *mut c_void) -> *mut c_void;
    pub fn mp_int(vm: *mut c_void, value: i64) -> *mut c_void;
    pub fn mp_float(vm: *mut c_void, value: f64) -> *mut c_void;
    pub fn mp_bool(vm: *mut c_void, value: c_int) -> *mut c_void;
    pub fn mp_str(vm: *mut c_void, utf8: *const c_char) -> *mut c_void;
    pub fn mp_str_n(vm: *mut c_void, utf8: *const c_char, len: usize) -> *mut c_void;
    pub fn mp_list(vm: *mut c_void) -> *mut c_void;
    pub fn mp_tuple(vm: *mut c_void) -> *mut c_void;
    pub fn mp_dict(vm: *mut c_void) -> *mut c_void;
    pub fn mp_range(vm: *mut c_void, start: i64, stop: i64, step: i64) -> *mut c_void;
    pub fn mp_dict_items(
        vm: *mut c_void,
        keys: *mut *mut c_void,
        values: *mut *mut c_void,
        count: usize,
    ) -> *mut c_void;
    pub fn mp_dict_view(vm: *mut c_void, dict: *mut c_void, kind: c_int) -> *mut c_void;
    pub fn mp_complex(vm: *mut c_void, real: f64, imag: f64) -> *mut c_void;
    pub fn mp_exception(
        vm: *mut c_void,
        kind: *const c_char,
        message: *const c_char,
    ) -> *mut c_void;
    pub fn mp_super(vm: *mut c_void, klass: *mut c_void, obj: *mut c_void) -> *mut c_void;
    pub fn mp_make_object(vm: *mut c_void, klass: *mut c_void) -> *mut c_void;
    pub fn mp_create_class(
        vm: *mut c_void,
        name: *const c_char,
        bases: *mut *mut c_void,
        base_count: usize,
        namespace_dict: *mut c_void,
    ) -> *mut c_void;
    pub fn mp_bind_method(vm: *mut c_void, obj: *mut c_void, name: *const c_char) -> *mut c_void;

    pub fn mp_seq_len(seq: *mut c_void, out_len: *mut usize) -> c_int;
    pub fn mp_seq_get(seq: *mut c_void, index: usize, out: *mut *mut c_void) -> c_int;
    pub fn mp_seq_set(seq: *mut c_void, index: usize, item: *mut c_void) -> c_int;
    pub fn mp_seq_append(seq: *mut c_void, item: *mut c_void) -> c_int;
    pub fn mp_seq_insert(seq: *mut c_void, index: usize, item: *mut c_void) -> c_int;
    pub fn mp_dict_len(dict: *mut c_void, out_len: *mut usize) -> c_int;
    pub fn mp_dict_get(dict: *mut c_void, key: *mut c_void, out: *mut *mut c_void) -> c_int;
    pub fn mp_dict_set(dict: *mut c_void, key: *mut c_void, value: *mut c_void) -> c_int;
    pub fn mp_dict_del(dict: *mut c_void, key: *mut c_void) -> c_int;
    pub fn mp_dict_get_index(
        dict: *mut c_void,
        index: usize,
        out_key: *mut *mut c_void,
        out_value: *mut *mut c_void,
    ) -> c_int;
    pub fn mp_value_retain(value: *mut c_void) -> *mut c_void;
    pub fn mp_value_release(value: *mut c_void);

    pub fn mp_value_kind(value: *mut c_void) -> c_int;
    pub fn mp_value_type_name(value: *mut c_void) -> *const c_char;
    pub fn mp_value_as_int(value: *mut c_void) -> i64;
    pub fn mp_value_as_float(value: *mut c_void) -> f64;
    pub fn mp_value_as_bool(value: *mut c_void) -> c_int;
    pub fn mp_value_as_str(value: *mut c_void) -> *const c_char;
    pub fn mp_value_as_str_n(value: *mut c_void, out_len: *mut usize) -> *const c_char;
    pub fn mp_value_is_complex(value: *mut c_void) -> c_int;
    pub fn mp_value_as_complex(value: *mut c_void, real: *mut f64, imag: *mut f64) -> c_int;
    pub fn mp_value_as_bytes(value: *mut c_void, out_len: *mut usize) -> *const c_char;
    pub fn mp_value_class(value: *mut c_void) -> *mut c_void;
    pub fn mp_value_dir_names(
        value: *mut c_void,
        names: *mut *mut *mut c_char,
        count: *mut usize,
    ) -> c_int;
    pub fn mp_value_container(value: *mut c_void) -> *mut c_void;
    pub fn mp_value_equal(a: *mut c_void, b: *mut c_void) -> c_int;
    pub fn mp_value_compare(a: *mut c_void, b: *mut c_void) -> c_int;
    pub fn mp_value_hash(value: *mut c_void, hashable: *mut c_int) -> i64;
    pub fn mp_value_to_str(value: *mut c_void) -> *mut c_char;
    pub fn mp_value_to_repr(value: *mut c_void) -> *mut c_char;
    pub fn mp_free(ptr: *mut c_void);

    pub fn mp_call(
        vm: *mut c_void,
        callee: *mut c_void,
        args: *mut *mut c_void,
        argc: usize,
    ) -> *mut c_void;
    pub fn mp_call_kw(
        vm: *mut c_void,
        callee: *mut c_void,
        args: *mut *mut c_void,
        argc: usize,
        kwargs: *mut c_void,
    ) -> *mut c_void;
    pub fn mp_call_name(
        vm: *mut c_void,
        name: *const c_char,
        args: *mut *mut c_void,
        argc: usize,
    ) -> *mut c_void;
    pub fn mp_binary(
        vm: *mut c_void,
        op: *const c_char,
        left: *mut c_void,
        right: *mut c_void,
    ) -> *mut c_void;
    pub fn mp_unary(vm: *mut c_void, op: *const c_char, value: *mut c_void) -> *mut c_void;
    pub fn mp_getattr(vm: *mut c_void, obj: *mut c_void, name: *const c_char) -> *mut c_void;
    pub fn mp_setattr(
        vm: *mut c_void,
        obj: *mut c_void,
        name: *const c_char,
        value: *mut c_void,
    ) -> c_int;
    pub fn mp_delattr(vm: *mut c_void, obj: *mut c_void, name: *const c_char) -> c_int;
    pub fn mp_hasattr(vm: *mut c_void, obj: *mut c_void, name: *const c_char) -> c_int;
    pub fn mp_getitem(vm: *mut c_void, obj: *mut c_void, key: *mut c_void) -> *mut c_void;
    pub fn mp_setitem(
        vm: *mut c_void,
        obj: *mut c_void,
        key: *mut c_void,
        value: *mut c_void,
    ) -> c_int;
    pub fn mp_delitem(vm: *mut c_void, obj: *mut c_void, key: *mut c_void) -> c_int;
    pub fn mp_iterate(
        vm: *mut c_void,
        iterable: *mut c_void,
        out: *mut *mut *mut c_void,
        out_count: *mut usize,
    ) -> c_int;
    pub fn mp_value_array_free(items: *mut *mut c_void, count: usize);
    pub fn mp_isinstance(vm: *mut c_void, obj: *mut c_void, cls: *mut c_void) -> c_int;
    pub fn mp_builtin_type(vm: *mut c_void, type_name: *const c_char) -> *mut c_void;
    pub fn mp_call_method(
        vm: *mut c_void,
        obj: *mut c_void,
        name: *const c_char,
        args: *mut *mut c_void,
        argc: usize,
    ) -> *mut c_void;
    pub fn mp_apply_format(vm: *mut c_void, value: *mut c_void, spec: *const c_char)
    -> *mut c_void;
    pub fn mp_percent_format(vm: *mut c_void, format: *mut c_void, arg: *mut c_void)
    -> *mut c_void;
    pub fn mp_is_subclass_of(
        vm: *mut c_void,
        sub: *mut c_void,
        builtin_name: *const c_char,
    ) -> c_int;
    pub fn mp_builtin_base_name(vm: *mut c_void, klass: *mut c_void) -> *const c_char;
    pub fn mp_match_pattern(
        vm: *mut c_void,
        subject: *mut c_void,
        pattern: *const c_char,
        captures_out: *mut *mut c_void,
    ) -> c_int;

    pub fn mp_lib_new(name: *const c_char) -> *mut c_void;
    pub fn mp_lib_free(lib: *mut c_void);
    pub fn mp_lib_add_function(
        lib: *mut c_void,
        name: *const c_char,
        callback: *mut c_void,
        user: *mut c_void,
    ) -> c_int;
    pub fn mp_lib_add_data(
        lib: *mut c_void,
        name: *const c_char,
        getter: *mut c_void,
        user: *mut c_void,
    ) -> c_int;
    pub fn mp_lib_add_const(lib: *mut c_void, name: *const c_char, value: *mut c_void) -> c_int;
    pub fn mp_class_new(name: *const c_char) -> *mut c_void;
    pub fn mp_class_free(klass: *mut c_void);
    pub fn mp_class_add_method(
        klass: *mut c_void,
        name: *const c_char,
        callback: *mut c_void,
        user: *mut c_void,
    ) -> c_int;
    pub fn mp_class_add_data(
        klass: *mut c_void,
        name: *const c_char,
        getter: *mut c_void,
        user: *mut c_void,
    ) -> c_int;
    pub fn mp_lib_add_class(lib: *mut c_void, name: *const c_char, klass: *mut c_void) -> c_int;
    pub fn mp_vm_load_lib(vm: *mut c_void, lib: *mut c_void) -> c_int;
    pub fn mp_make_native(
        vm: *mut c_void,
        lib: *mut c_void,
        klass: *mut c_void,
        handle: *mut c_void,
    ) -> *mut c_void;
    pub fn mp_value_native_handle(value: *mut c_void) -> *mut c_void;
    pub fn mp_value_native_set(value: *mut c_void, name: *const c_char, item: *mut c_void)
    -> c_int;
    pub fn mp_value_native_get(value: *mut c_void, name: *const c_char) -> *mut c_void;

    pub fn mp_ctx_argc(ctx: *mut c_void) -> usize;
    pub fn mp_ctx_arg(ctx: *mut c_void, index: usize) -> *mut c_void;
    pub fn mp_ctx_kwarg(ctx: *mut c_void, name: *const c_char) -> *mut c_void;
    pub fn mp_ctx_has_kwarg(ctx: *mut c_void, name: *const c_char) -> c_int;
    pub fn mp_ctx_vm(ctx: *mut c_void) -> *mut c_void;
    pub fn mp_ctx_self(ctx: *mut c_void) -> *mut c_void;
    pub fn mp_ctx_error(ctx: *mut c_void, kind: *const c_char, message: *const c_char);
    pub fn mp_ctx_make_native(ctx: *mut c_void, handle: *mut c_void) -> *mut c_void;
}
