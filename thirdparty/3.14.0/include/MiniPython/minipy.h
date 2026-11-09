/*
【MiniPython 项目介绍】
MiniPython 是一款实验性开源 Python 解释器库。
项目目标：打造可以嵌入其它程序，也可以独立运行、不依赖外部 Python 环境的轻量解释器。
特点：体积小巧、零外部依赖、架构开放、支持自由扩展，开发集成便捷高效；
同时兼容标准 Python 的 pyd 扩展库生态，能够复用现有的 Python 二进制扩展模块。

版权信息：Copyright (c) RySoftware
授权说明：本项目采用宽松友好开源许可，允许个人、商业项目免费使用、修改、二次分发；
衍生作品需要保留本版权声明，本软件不附带任何明示或暗示担保。
*/

/*
【MiniPython Project Introduction】
MiniPython is an experimental open-source Python interpreter library.
The goal of this project is to build a lightweight interpreter that can be embedded into other programs
or run independently, without relying on an external Python environment.

Features: Small footprint, zero external dependencies, open architecture, freely extensible,
convenient and efficient for integration.
It is compatible with the standard Python pyd extension library ecosystem,
and can reuse existing Python binary extension modules.

Copyright (c) RySoftware
License: Permissive open-source license, free for personal and commercial use, modification and redistribution.
Derivative works must retain this copyright notice. The software is provided without warranty of any kind,
express or implied.
*/

/*
【ᠮᠢᠨᠢ ᠫᠢᠲ᠋ᠢᠣᠨ ᠵᠢᠨ ᠲᠥᠰᠣᠯ ᠤᠨ ᠲᠠᠨᠢᠯᠴᠠᠭ᠎ᠠ】
ᠮᠢᠨᠢ ᠫᠢᠲ᠋ᠢᠣᠨ ᠪᠣᠯ ᠰᠢᠯᠭᠠᠨ ᠪᠠᠶᠢᠨ᠎ᠠ ᠠᠩᠬᠢᠨ ᠪᠢᠴᠢᠭ᠌ ᠫᠢᠲ᠋ᠢᠣᠨ ᠪᠢᠴᠢᠭ᠌ ᠰᠠᠷᠢᠭ ᠮᠠᠨᠳᠠᠯ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠮᠠᠨᠳᠠᠯ᠃
ᠲᠥᠰᠣᠯ ᠤᠨ ᠵᠢᠯᠠᠭ᠎ᠠ：ᠪᠤᠰᠠᠳ ᠫᠷᠣᠭᠷᠠᠮᠳᠤ ᠪᠠᠭ᠌ᠠᠪᠣᠷᠢᠭᠤᠯᠵᠤ ᠪᠣᠯᠣᠬᠤ ᠪᠡᠰᠡ ᠪᠢᠴᠡᠨᠳᠡᠨ ᠠᠵᠢᠯᠯᠠᠬᠤ᠂ ᠭᠠᠷᠠᠭ᠎ᠠ ᠫᠢᠲ᠋ᠢᠣᠨ ᠪᠠᠶᠢᠨᠳᠠᠯ ᠭᠡᠷᠡᠭᠴᠡᠨ ᠦᠭᠡᠢ ᠪᠠᠭ᠎ᠠ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠴᠢᠭ᠌ ᠪᠠᠨᠳᠠᠬᠤ᠃

ᠪᠠᠨᠳᠠᠯᠳᠠᠯ᠎ᠠ：ᠪᠠᠭ᠎ᠠ ᠬᠡᠷᠡᠭᠴᠡᠨ ᠦᠭᠡᠢ᠂ ᠰᠠᠷᠢᠭ᠎ᠠ ᠥᠷᠭᠡᠨᠳᠡᠬᠡ᠂ ᠨᠠᠷᠢᠨ ᠪᠠᠶᠢᠨᠳᠠᠯᠳᠠᠨ ᠪᠠᠨᠳᠠᠨ᠎ᠠ᠂ ᠵᠢᠨᠳᠡᠨᠡ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ ᠪᠣᠯᠣᠨ᠎ᠠ᠃
ᠰᠢᠳᠠᠷᠳᠤᠭ᠎ᠠ Python ᠦ pyd ᠰᠠᠷᠢᠭ ᠡᠷᠳᠡᠨᠢ ᠵᠢᠨᠳᠡᠨᠡ ᠪᠠᠨᠳᠠᠨ᠎ᠠ᠂ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠣᠳᠤᠯᠢ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ ᠪᠣᠯᠣᠨ᠎ᠠ᠃

ᠡᠷᠳᠡᠨᠢ ᠡᠵᠡᠨ᠎ᠡ：Copyright (c) RySoftware
ᠵᠢᠨᠳᠡᠨᠡ ᠵᠢᠯᠠᠭ᠎ᠠ：ᠮᠠᠨᠳᠠᠯ ᠵᠢᠨᠳᠡᠨᠡ ᠪᠠᠶᠢᠨ᠎ᠠ ᠵᠢᠨᠳᠡᠨᠡ᠂ ᠬᠦᠮᠡᠨ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠪᠠᠶᠢᠨ᠎ᠠ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ᠂ ᠵᠠᠰᠠᠬᠤ᠂ ᠳᠠᠮᠵᠢᠭᠤᠯᠬᠤ ᠪᠣᠯᠣᠨ᠎ᠠ᠃
ᠳᠠᠮᠵᠢᠭᠤᠯᠰᠠᠨ ᠮᠠᠨᠳᠠᠯᠳᠤ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠡᠷᠳᠡᠨᠢ ᠡᠵᠡᠨ᠎ᠡ ᠮᠡᠳᠡᠭᠡᠯ ᠢ ᠰᠠᠬᠢᠭᠤ ᠪᠣᠯᠣᠨ᠎ᠠ᠂ ᠡᠨᠡ ᠪᠢᠴᠢᠭ᠌ ᠮᠠᠨᠳᠠᠯ ᠬᠠᠮᠠᠨᠳᠠᠭ᠎ᠠ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠠᠶᠢᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠦᠭᠡᠢ᠃
*/

/*
【许可正文｜MIT-like 宽松协议】
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*
【ᠵᠢᠨᠳᠡᠨᠡ ᠵᠢᠨ ᠮᠠᠨᠳᠠᠯ】
ᠡᠨᠡ ᠵᠢᠨᠳᠡᠨᠡ ᠪᠡᠷ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠵᠢᠨᠳᠡᠨᠡ ᠪᠣᠯᠵᠤ᠂ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠬᠦᠮᠡᠨ ᠪᠦᠷᠢᠳᠡᠨ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠣᠯᠣᠨ ᠬᠠᠮᠳᠠᠯᠳᠠᠭ᠎ᠠ ᠪᠢᠴᠢᠭ᠌ ᠮᠠᠨᠳᠠᠯ ᠤᠨ ᠬᠦᠮᠡᠳᠡ ᠪᠠᠨᠳᠠᠨ᠎ᠠ ᠬᠦᠮᠡᠨᠳᠡᠨ᠂ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠡᠷᠢᠵᠤ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ᠂ ᠬᠦᠪᠡᠨᠳᠡᠯᠡᠨᠡ᠂ ᠵᠠᠰᠠᠬᠤ᠂ ᠨᠡᠨᠳᠡᠯᠡᠨᠡ᠂ ᠪᠠᠷᠢᠭᠤᠯᠵᠤ ᠪᠠᠨᠳᠠᠨ᠎ᠠ᠂ ᠳᠠᠮᠵᠢᠭᠤᠯᠬᠤ᠂ ᠳᠠᠮᠵᠢᠭᠤᠯᠵᠤ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ᠂ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠪᠠᠷᠢᠵᠤ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠪᠣᠯᠣᠨ᠎ᠠ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠪᠠᠷᠢᠵᠤ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ ᠪᠣᠯᠣᠨ᠎ᠠ᠃

ᠳᠠᠯᠳᠠᠮᠵᠢᠭᠤᠯᠰᠠᠨ ᠮᠠᠨᠳᠠᠯᠳᠤ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠡᠷᠳᠡᠨᠢ ᠡᠵᠡᠨ᠎ᠡ ᠮᠡᠳᠡᠭᠡᠯ ᠪᠣᠯᠣᠨ ᠡᠨᠡ ᠵᠢᠨᠳᠡᠨᠡ ᠮᠡᠳᠡᠭᠡᠯ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠦᠷᠢ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠳᠤᠷᠠᠨᠳᠠᠨ ᠰᠠᠬᠢᠭᠤ ᠪᠣᠯᠣᠨ᠎ᠠ᠃

ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠣᠯ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠠᠷᠢᠵᠤ ᠪᠠᠨᠳᠠᠨ᠎ᠠ᠂ ᠬᠠᠮᠠᠨᠳᠠᠭ᠎ᠠ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠠᠶᠢᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠦᠭᠡᠢ᠂ ᠪᠣᠯᠣᠨ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠪᠠᠶᠢᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ᠂ ᠬᠡᠷᠡᠭᠴᠡᠨᠳᠡ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ ᠪᠣᠯᠣᠨ ᠡᠷᠳᠡᠨᠢ ᠵᠢᠨᠳᠡᠨᠡ ᠵᠠᠷᠢᠮᠳᠠᠯ ᠦᠭᠡᠢ ᠪᠠᠶᠢᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠪᠠᠶᠢᠨ᠎ᠠ᠃
ᠡᠨᠡ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠡᠷᠳᠡᠨᠢ ᠡᠵᠡᠨ᠎ᠡ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠪᠠᠨᠳᠠᠯᠳᠠᠨ ᠪᠣᠯᠣᠨ ᠮᠠᠨᠳᠠᠯ ᠪᠠᠶᠢᠨ᠎ᠠ ᠬᠡᠷᠡᠭᠯᠡᠨᠡᠨ ᠮᠠᠨᠳᠠᠯᠳᠤᠭ᠎ᠠ ᠪᠠᠶᠢᠨᠳᠠᠯᠳᠠᠨ ᠭᠠᠷᠬᠤ ᠵᠠᠷᠢᠮᠳᠠᠯ ᠪᠠᠨᠳᠠᠨ᠎ᠠ ᠦᠭᠡᠢ᠃
*/
#ifndef MINIPY_H
#define MINIPY_H

// 仅在 C++ 侧引入内部实现头, 纯 C 编译器不会看到任何 C++ 类型
// Internal headers are only pulled in for C++; a pure C compiler never sees them
// ᠵᠥᠪᠬᠡᠨ C++ ᠳᠤ ᠳᠣᠲᠣᠷᠠᠬᠢ ᠲᠣᠯᠤᠭ᠎ᠠᠢ ᠲᠣᠯᠤᠭ᠎ᠠᠢ ᠡᠬᠢ᠂ ᠰᠢᠳᠠᠷᠳᠤᠭ᠎ᠠ C ᠬᠡᠯᠡᠨ ᠦ ᠬᠡᠷᠡᠭᠯᠡᠭᠴᠢ ᠦᠭᠡᠢ
#ifdef __cplusplus
#include "lexer.hpp"
#include "parser.hpp"
#include "vmware.hpp"
#include "cffi.hpp"
#include "platform.hpp"
#endif

// Windows 下把链接栈调到 64MB, 避免深递归溢出
// On Windows, set linker stack to 64MB for deep recursion
// ᠸᠢᠨᠳᠣᠸᠰ ᠳᠡ᠋ 64ᠮᠠᠭ᠋ᠠ ᠰᠲᠠᠭᠭᠡᠷ ᠢ ᠲᠣᠬᠢᠷᠠᠭᠤᠯᠬᠤ
#if defined(_WIN32)
#pragma comment(linker, "/STACK:67108864")
#endif

// C-API 标准C语言使用的API extern "C"
// Standard C language API for MiniPython
// ᠰᠢᠳᠠᠷᠳᠤᠭ᠎ᠠ C ᠬᠡᠯᠡᠨ ᠦ ᠮᠢᠨᠢᠫᠢᠲᠢᠣᠨ ᠳᠤ ᠬᠡᠷᠡᠭᠯᠡᠨᠡ ᠮᠠᠨᠳᠠᠯ ᠳᠤᠭ᠎ᠠ ᠢᠨᠳᠡᠷᠲᠡᠭᠡᠨᠳᠡᠯᠡᠯᠳᠡᠭᠡᠷᠢ
//
// 所有句柄一律用 void*, 不定义任何结构体, C/C++ 侧都无需知悉内部布局。
// Every handle is a plain void*, no struct is declared: the internal layout
// stays completely hidden from C and C++ callers.
//
// 使用约定 / Conventions / ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ:
// 1. 句柄含义 / Handles:
//      void* vm      解释器实例        / interpreter instance
//      void* value   值句柄 (持 1 引用) / value handle (owns 1 reference)
//      void* lib     FFI 库描述        / FFI library descriptor
//      void* klass   FFI 类描述        / FFI class descriptor
//      void* ctx     原生回调上下文     / native callback context
//      void* state   错误快照          / error snapshot
// 2. 由 C-API 返回的 value 句柄归调用方所有, 用完必须 mp_value_release();
//    作为参数传入的句柄只被借用, 不会被接管或释放。
//    Returned value handles are owned by the caller; parameters are borrowed.
// 3. 字符串统一 UTF-8 (const char*); 返回的 const char* 仅在下一次调用同一个
//    vm 的 API 之前有效, 需要长期保存请自行复制。
//    Strings are UTF-8; returned const char* is valid until the next vm call.
// 4. 错误两段式: 返回 MP_OK 表示成功; 失败用 mp_vm_last_error() 取 traceback,
//    mp_vm_exception_type() 取异常类型名。
//    Two-part error model: return code plus mp_vm_last_error() text.
// 5. C++ 异常绝不跨越 ABI 边界 (内部 try/catch 兜底), C 侧不会崩溃。
//    No C++ exception ever crosses the ABI boundary.
// 6. 一个 vm 只在一个线程内使用, 原生回调在调用线程上同步执行。
//    One vm per thread; native callbacks run synchronously on that thread.
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// ---- 状态码 / Status codes ----
#define MP_OK          0    // 成功 / success
#define MP_ERROR     (-1)   // 失败, 取 mp_vm_last_error() / see mp_vm_last_error()
#define MP_TYPE_ERROR (-2)  // 类型不匹配 / type mismatch
#define MP_NOT_FOUND (-3)   // 未找到 / not found

// ---- 值类型枚举 / Value kind (mp_value_kind 返回值) ----
#define MP_KIND_NONE   0    // None
#define MP_KIND_INT    1    // int / bool
#define MP_KIND_FLOAT  2    // float
#define MP_KIND_STR    3    // str
#define MP_KIND_LIST   4    // list
#define MP_KIND_TUPLE  5    // tuple
#define MP_KIND_DICT   6    // dict (含 keys/values/items 视图)
#define MP_KIND_RANGE  7    // range
#define MP_KIND_OBJECT 8    // 脚本类实例 / 类对象 / class instance, class
#define MP_KIND_NATIVE 9    // FFI / ctypes 原生对象 / native object
#define MP_KIND_FUNC   10   // 函数 / 绑定方法 / function, bound method
#define MP_KIND_MODULE 11   // 模块字典 / module dict
#define MP_KIND_OTHER  12   // 其它 (生成器/切片/...)
#define MP_KIND_COMPLEX 13  // 复数 / complex
#define MP_KIND_BYTES 14    // bytes / bytearray
#define MP_KIND_EXCEPTION 15 // 异常对象 / exception object

// 原生函数回调: user 为注册时透传的数据, ctx 为回调上下文 (用 mp_ctx_* 读写),
// 成功时把新句柄写入 *out 并返回 MP_OK, 失败返回非 MP_OK。
// Native function callback: write a new handle into *out and return MP_OK.
// ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ ᠶᠢᠨ ᠰᠢᠭᠨᠠᠲᠤᠷ᠎ᠠ
// int (*)(void* user, void* ctx, void** out)
//
// 数据取值回调: 返回新句柄 (由解释器接管), NULL 表示失败。
// Data getter callback: return a new handle (taken over) or NULL.
// ᠳ᠋ᠠᠲᠠ ᠶᠢᠨ ᠰᠢᠭᠨᠠᠲᠤᠷ᠎ᠠ
// void* (*)(void* user, void* vm)

// ================= 1. 解释器 / VM =================
// 创建解释器 (builtins 已就绪)
// Create an interpreter (builtins already loaded)
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void*     mp_vm_new(void);

// 销毁解释器 (先释放全部 value 句柄)
// Destroy the interpreter (release every value handle first)
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ ᠦᠬᠡᠬᠡᠵᠡ
void      mp_vm_free(void* vm);

// 加载全部内置标准库 (math/os/json/re/ctypes ...)
// Load every builtin stdlib module
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠢ ᠪᠡᠷᠢᠨ᠎ᠠ
int       mp_vm_use_stdlib(void* vm);

// 注册 Python 模块源码 (编译并缓存, 之后可 import)
// Register a Python module from source (compiled and cached)
// Python ᠮᠣᠳᠤᠯᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
int       mp_vm_add_module(void* vm, const char* name, const char* source);

// 加载 .pyd / .dll 二进制扩展模块
// Load a .pyd / .dll binary extension module
// .pyd ᠬᠡᠯᠪᠡᠷᠢ ᠪᠢᠲ᠋ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
int       mp_vm_load_pyd(void* vm, const char* name,
                         const unsigned char* data, size_t size);

// 载入源码 (只编译装载, 不执行), 之后调用 mp_vm_run
// Load source (compile only), then call mp_vm_run
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int       mp_vm_load_source(void* vm, const char* source);

// 载入序列化字节码, 之后调用 mp_vm_run
// Load serialized bytecode, then call mp_vm_run
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠪᠢᠲ᠋ᠢ ᠡᠴᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int       mp_vm_load_bytecode(void* vm, const unsigned char* data, size_t size);

// 把源码编译为序列化字节码, *out 由 C-API 分配 (用 mp_free 释放)
// Compile source into bytecode, *out is C-API allocated (free with mp_free)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠪᠢᠲ᠋ᠢ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
int       mp_vm_compile_bytecode(void* vm, const char* source,
                                 unsigned char** out, size_t* out_size);

// 执行已载入的脚本
// Execute the loaded script
// ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
int       mp_vm_run(void* vm);

// 编译并执行一段源码 (顶层名字写入全局作用域)
// Compile and execute a source string (top-level names go to globals)
// ᠪᠢᠴᠢᠭᠵᠢᠯᠳᠡᠨ᠎ᠡ ᠢ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
int       mp_vm_exec_source(void* vm, const char* source);

// 以 __main__ 方式执行 (等价于 load_source + run)
// Load and run as __main__ (load_source + run)
// __main__ ᠪᠣᠯᠭᠠᠨ ᠠᠶᠢᠯᠳᠠᠭᠤᠯᠬᠤ
int       mp_vm_exec_main(void* vm, const char* source);

// ---- 错误状态 / Error state ----
// 最近一次错误的 traceback 文本 (无错误返回 "", 指针到下次调用前有效)
// Last error traceback text ("" when no error)
// ᠬᠤᠶᠢᠷᠲᠠ ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠶᠢᠨ ᠪᠢᠴᠢᠭ
const char* mp_vm_last_error(void* vm);

// 是否有未处理的错误 (1/0)
// Whether an error is pending (1/0)
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠲᠡᠢ ᠶᠤᠤ
int       mp_vm_has_error(void* vm);

// 待处理异常的类型名 (如 "ValueError", 无则 "")
// Pending exception type name ("" when none)
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠶᠢᠨ ᠲᠥᠷᠥᠯ ᠨᠡᠷᠡ
const char* mp_vm_exception_type(void* vm);

// 清空错误状态
// Clear the error state
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠪᠡᠯᠴᠡᠡᠷᠢᠶᠡᠨ᠎ᠠ
void      mp_vm_clear_error(void* vm);

// 抛出异常 (供原生回调把错误传播给 Python 侧)
// Raise an exception (from native callbacks)
// ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠪᠡᠷᠭᠢᠵᠡ
void      mp_vm_raise(void* vm, const char* type, const char* message);

// 保存 / 恢复 / 释放错误快照 (state 为 C-API 分配的 void*)
// Save / restore / free an error snapshot
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠠᠶᠢᠳᠠᠯ ᠶᠢᠨ ᠲᠠᠰᠪᠢᠮᠠᠭ
void*     mp_vm_save_error(void* vm);
void      mp_vm_restore_error(void* vm, void* state);
void      mp_error_state_free(void* state);

// ---- 全局变量 / Globals ----
// 读全局变量 (返回新句柄, 不存在返回 NULL)
// Read a global variable (new handle, NULL when missing)
// ᠪᠢᠴᠢᠭ ᠬᠠᠯᠠᠬᠤ
void*     mp_vm_get_global(void* vm, const char* name);

// 设置全局变量 (解释器持有自己的拷贝)
// Set a global variable (the interpreter keeps its own copy)
// ᠪᠢᠴᠢᠭ ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
int       mp_vm_set_global(void* vm, const char* name, void* value);

// 删除全局变量
// Delete a global variable
// ᠪᠢᠴᠢᠭ ᠢ ᠦᠬᠡᠬᠡᠵᠡ
int       mp_vm_del_global(void* vm, const char* name);

// 全局变量名列表 (char** 由 C-API 分配, 用 mp_free_names 释放)
// Global name list (free with mp_free_names)
// ᠪᠢᠴᠢᠭ ᠶᠢᠨ ᠨᠡᠷᠡ ᠵᠢᠰᠢᠶ᠎ᠡ
int       mp_vm_global_names(void* vm, char*** names, size_t* count);
void      mp_free_names(char** names, size_t count);

// ---- 反射 / Introspection ----
// 在脚本中查找类 / 函数, 返回非持有句柄 (释放句柄即可, 不会销毁定义)
// Find a script class / function; returns a non-owning handle
// ᠰᠢᠯᠭᠠᠨ ᠡᠴᠡ ᠬᠠᠯᠠᠬᠤ
int       mp_vm_find_class(void* vm, const char* name, void** out);
int       mp_vm_find_function(void* vm, const char* name, void** out);

// 是否已装载某个 FFI / 脚本模块 (1/0)
// Whether an FFI or script module is loaded (1/0)
// ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠶᠤᠤ
int       mp_vm_has_lib(void* vm, const char* name);

// 堆对象统计 / 快照 (快照数组用 mp_value_array_free 释放)
// Heap object count / snapshot (free array with mp_value_array_free)
// ᠭᠡᠷ ᠲᠤ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠲᠣᠭ᠎ᠠ
int       mp_vm_heap_count(void* vm, long long* out_count);
int       mp_vm_heap_snapshot(void* vm, void*** out, size_t* out_count);

// ================= 2. 构造值 / Value builders =================
// 以下工厂返回新句柄, 调用方负责 mp_value_release
// Builders return a new caller-owned handle
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_none(void* vm);
void* mp_int(void* vm, long long v);
void* mp_float(void* vm, double v);
void* mp_bool(void* vm, int v);
void* mp_str(void* vm, const char* utf8);
void* mp_str_n(void* vm, const char* utf8, size_t len);
void* mp_list(void* vm);
void* mp_tuple(void* vm);
void* mp_dict(void* vm);
void* mp_range(void* vm, long long start, long long stop, long long step);

// 由键值句柄数组构造字典 / 构造 dict_keys(0) / dict_values(1) / dict_items(2) 视图
// Build a dict from key/value handle arrays / build a dict view (kind 0/1/2)
// ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ / ᠦᠶᠢᠯᠳᠡᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_dict_items(void* vm, void** keys, void** values, size_t count);
void* mp_dict_view(void* vm, void* dict, int kind);

// 复数 / 异常对象 / super 代理
// Complex value / exception object / super proxy
// ᠰᠢᠯᠢ ᠲᠣ᠎ / ᠬᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ / super
void* mp_complex(void* vm, double real, double imag);
void* mp_exception(void* vm, const char* type, const char* message);
void* mp_super(void* vm, void* klass, void* obj);

// 用脚本类句柄构造实例 / 动态建类 (等价 type(name, bases, namespace))
// Build an instance from a script class handle / dynamic class creation
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠶᠢᠨᠢᠰ / ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_make_object(void* vm, void* klass);
void* mp_create_class(void* vm, const char* name, void** bases,
                      size_t base_count, void* namespace_dict);

// 把脚本对象上的方法绑定为可直接调用的句柄 (仅脚本对象/类)
// Bind a script-object method into a directly callable handle
// ᠠᠷᠭᠠ ᠢ ᠬᠣᠯᠪᠣᠭᠤᠯᠬᠤ
void* mp_bind_method(void* vm, void* obj, const char* name);

// 容器原地修改 / In-place container mutation
// ᠲᠠᠯᠠᠭ᠌ ᠢ ᠵᠠᠰᠠᠬᠤ
int   mp_seq_len(void* seq, size_t* out_len);
int   mp_seq_get(void* seq, size_t index, void** out);
int   mp_seq_set(void* seq, size_t index, void* item);
int   mp_seq_append(void* seq, void* item);
int   mp_seq_insert(void* seq, size_t index, void* item);
int   mp_dict_len(void* dict, size_t* out_len);
int   mp_dict_get(void* dict, void* key, void** out);
int   mp_dict_set(void* dict, void* key, void* value);
int   mp_dict_del(void* dict, void* key);

// 按顺序读取第 index 个键 / 值 (dict 及三种视图均可)
// Read the index-th key / value pair in order (works for dict and its views)
// ᠳᠤᠭ᠎ᠠ ᠪᠡᠷ ᠬᠠᠯᠠᠬᠤ
int   mp_dict_get_index(void* dict, size_t index, void** out_key, void** out_value);

// 引用计数: 返回同一句柄 (便于链式使用), release 后句柄失效
// Refcount on the handle: retain returns the same handle, release invalidates it
// refcount
void* mp_value_retain(void* value);
void  mp_value_release(void* value);

// ================= 3. 读取值 / Value readers =================
// 值类型 (MP_KIND_*) / Value kind (MP_KIND_*)
// value ᠶᠢᠨ ᠲᠥᠷᠥᠯ
int         mp_value_kind(void* value);

// 类型名字符串 ("int"/"str"/类名/...) / Type name
// ᠲᠥᠷᠥᠯ ᠨᠡᠷᠡ
const char* mp_value_type_name(void* value);

// 数值 / 布尔 / 字符串读取 (失败返回 0 或 NULL)
// Numeric / bool / string reads (0 or NULL on failure)
// ᠬᠠᠯᠠᠬᠤ
long long   mp_value_as_int(void* value);
double      mp_value_as_float(void* value);
int         mp_value_as_bool(void* value);
const char* mp_value_as_str(void* value);
const char* mp_value_as_str_n(void* value, size_t* out_len);

// 复数读写 (成功返回 1 / 0) / bytes、bytearray 读取 (失败返回 NULL)
// Complex read/write (1/0) / bytes & bytearray reads (NULL on failure)
// ᠰᠢᠯᠢ ᠲᠣ᠎ / bytes ᠬᠠᠯᠠᠬᠤ
int         mp_value_is_complex(void* value);
int         mp_value_as_complex(void* value, double* real, double* imag);
const char* mp_value_as_bytes(void* value, size_t* out_len);

// 取值的类型对象: class 原样返回, 脚本对象返回其所属类,
// 内置值返回内置类型对象 (native_object, 如 int/list); 未知返回 NULL
// Type object of a value (class itself / script class / builtin type object)
// ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠬᠠᠯᠠᠬᠤ
void*       mp_value_class(void* value);

// dir(obj) 名字列表 (char** 用 mp_free_names 释放)
// dir(obj) name list (free with mp_free_names)
// dir() ᠶᠢᠨ ᠨᠡᠷᠡ ᠵᠢᠰᠢᠶ᠎ᠡ
int         mp_value_dir_names(void* value, char*** names, size_t* count);

// 取值的容器基类 (dict/list/tuple; 无则 NULL, 返回新句柄)
// Get the container base of a value (dict/list/tuple; NULL otherwise)
// ᠦᠮᠢᠰᠢ ᠬᠠᠯᠠᠬᠤ
void*       mp_value_container(void* value);

// 判等 / 比较 / 哈希 (compare 返回 <0, 0, >0)
// Equality / ordering / hash
// ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠬᠤ
int         mp_value_equal(void* a, void* b);
int         mp_value_compare(void* a, void* b);
long long   mp_value_hash(void* value, int* hashable);

// 转成 Python str() / repr() (新字符串, 用 mp_free 释放)
// Convert to Python str() / repr() (new string, free with mp_free)
// str / repr ᠪᠣᠯᠭᠠᠨ᠎ᠠ
char*       mp_value_to_str(void* value);
char*       mp_value_to_repr(void* value);

// 释放 C-API 分配的字符串 / 缓冲区
// Free a C-API allocated string / buffer
// ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠢ ᠦᠬᠡᠬᠡᠵᠡ
void        mp_free(void* ptr);

// ================= 4. 调用与运算符 / Calls & operators =================
// 调用任意可调用值 (args 可为 NULL)
// Call any callable value (args may be NULL)
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠬᠡᠯᠪᠡᠷᠢ
void* mp_call(void* vm, void* callee, void** args, size_t argc);

// 带关键字参数调用 (kwargs 为 dict 句柄, 可为 NULL)
// Call with keyword arguments (kwargs is a dict handle, may be NULL)
// ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ ᠲᠡᠢ ᠬᠡᠯᠪᠡᠷᠢ
void* mp_call_kw(void* vm, void* callee, void** args, size_t argc, void* kwargs);

// 按名调用全局函数 (等价于 get_global + mp_call)
// Call a global function by name (get_global + mp_call)
// ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ
void* mp_call_name(void* vm, const char* name, void** args, size_t argc);

// 二元运算 (op 取 "+ - * / // % ** << >> & | ^" 或比较 "== != < <= > >="
// 以及 "in" / "not in" / "is" / "is not")
// Binary operator (op as a Python operator text)
// ᠬᠣᠶᠠᠳᠠᠯ ᠦᠶᠢᠯᠳᠡᠯ
void* mp_binary(void* vm, const char* op, void* left, void* right);

// 一元运算 (op 取 "+" / "-" / "~" / "not")
// Unary operator (op: "+", "-", "~", "not")
// ᠨᠢᠳᠡᠳᠡᠯ ᠦᠶᠢᠯᠳᠡᠯ
void* mp_unary(void* vm, const char* op, void* value);

// 属性读写删查 / Attribute access
// ᠠᠲᠲᠷᠢᠪᠶᠤᠲ
void* mp_getattr(void* vm, void* obj, const char* name);
int   mp_setattr(void* vm, void* obj, const char* name, void* value);
int   mp_delattr(void* vm, void* obj, const char* name);
int   mp_hasattr(void* vm, void* obj, const char* name);

// 下标读写删 / Subscript
// ᠬᠠᠶᠢᠷᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ
void* mp_getitem(void* vm, void* obj, void* key);
int   mp_setitem(void* vm, void* obj, void* key, void* value);
int   mp_delitem(void* vm, void* obj, void* key);

// 迭代: 把可迭代对象 (list/tuple/dict/range/str/生成器/...) 展开为句柄数组,
// *out 由 C-API 分配, 用 mp_value_array_free 释放
// Iterate any iterable into an array (free with mp_value_array_free)
// ᠶᠢᠨᠳᠡᠬᠡ
int   mp_iterate(void* vm, void* iterable, void*** out, size_t* out_count);
void  mp_value_array_free(void** items, size_t count);

// isinstance(obj, cls) 语义
// isinstance(obj, cls) semantics
// isinstance() ᠬᠡᠯᠪᠡᠷᠢ
int   mp_isinstance(void* vm, void* obj, void* cls);

// 取内置类型对象 ("int"/"list"/"dict"/"Exception"/...), 未知返回 NULL
// Get a builtin type object (NULL when unknown)
// ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ
void* mp_builtin_type(void* vm, const char* type_name);

// 按名调用对象方法 (等价于 getattr(obj, name)(*args))
// Call an object method by name (getattr(obj, name)(*args))
// ᠠᠷᠭᠠ ᠨᠡᠷᠡᠷ ᠢᠶᠡᠨ ᠬᠠᠯᠠᠬᠤ
void* mp_call_method(void* vm, void* obj, const char* name, void** args, size_t argc);

// format(value, spec) / printf 风格 % 格式化
// format(value, spec) / printf-style % formatting
// format / % ᠬᠡᠪᠡᠷᠢᠶ᠎ᠡᠷᠢᠶᠡᠨ᠎ᠠ
void* mp_apply_format(void* vm, void* value, const char* spec);
void* mp_percent_format(void* vm, void* format, void* arg);

// 判断是否继承自某个内置基类 (1/0) / 取脚本类的内置基类名 (非脚本类返回 "")
// Whether a class inherits a builtin base (1/0) / builtin base name of a script class
// ᠡᠪᠦᠭ ᠰᠢᠯᠭᠠᠬᠤ / ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠭ᠎ᠠ ᠰᠢᠨ᠎ᠡ
int         mp_is_subclass_of(void* vm, void* sub, const char* builtin_name);
const char* mp_builtin_base_name(void* vm, void* klass);

// 模式匹配 (pattern 为内部 IR 文本); 命中返回 1, 未命中 0, 出错 -1;
// *captures_out 收到捕获名字 -> 值的 dict 句柄 (可为 NULL)
// Pattern match on internal IR text; 1 hit, 0 miss, -1 error
// ᠬᠡᠯᠪᠡᠷᠢ ᠲᠠᠭᠠᠷᠤᠯᠳᠤ
int         mp_match_pattern(void* vm, void* subject, const char* pattern,
                             void** captures_out);

// ================= 5. 扩展注册 / Extension (FFI) =================
// 创建一个空 FFI 库描述
// Create an empty FFI library descriptor
// FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_lib_new(const char* name);

// 销毁库描述 (解释器内部保留自己的拷贝, 可在 load_lib 之后立即释放)
// Destroy the descriptor (the interpreter keeps its own copy)
// FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠦᠬᠡᠬᠡᠵᠡ
void  mp_lib_free(void* lib);

// 注册函数 (fn 为 int (*)(void* user, void* ctx, void** out))
// Register a native function
// ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int   mp_lib_add_function(void* lib, const char* name, void* fn, void* user);

// 注册动态数据 (getter 为 void* (*)(void* user, void* vm))
// Register data produced by a getter callback
// ᠳ᠋ᠠᠲᠠ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
int   mp_lib_add_data(void* lib, const char* name, void* getter, void* user);

// 注册常量数据 (注册时按值拷贝)
// Register a constant data value (copied at registration)
// ᠲᠣᠭᠲᠠᠩᠭᠤᠢ ᠳ᠋ᠠᠲᠠ
int   mp_lib_add_const(void* lib, const char* name, void* value);

// 创建 FFI 类描述
// Create an FFI class descriptor
// FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_class_new(const char* name);

// 销毁类描述
// Destroy the class descriptor
// FFI ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠦᠬᠡᠬᠡᠵᠡ
void  mp_class_free(void* klass);

// 给类加方法 / 类属性
// Add a method / class-level data to the class
// ᠠᠷᠭᠠ ᠨᠡᠮᠡᠬᠦ
int   mp_class_add_method(void* klass, const char* name, void* fn, void* user);
int   mp_class_add_data(void* klass, const char* name, void* getter, void* user);

// 把类挂到库上 (name 为 Python 侧类名)
// Attach a class to a library (name visible from Python)
// ᠬᠣᠯᠪᠣᠭᠤᠯᠬᠤ
int   mp_lib_add_class(void* lib, const char* name, void* klass);

// 把库注册进解释器 (之后可 import 或用 lib_name.func(...) 访问)
// Register the library into the interpreter
// FFI ᠪᠢᠲ᠋ᠢ ᠳ᠋ᠦᠩᠳ ᠢ ᠣᠷᠣᠭᠤᠯᠬᠤ
int   mp_vm_load_lib(void* vm, void* lib);

// 直接构造一个原生对象 (对应 vm::makeNativeObject)
// Build a native object directly (vm::makeNativeObject)
// native object ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_make_native(void* vm, void* lib, void* klass, void* handle);

// 取原生对象背后的句柄 (非原生对象返回 NULL)
// Get the native handle behind an object (NULL otherwise)
// native handle ᠬᠠᠯᠠᠬᠤ
void* mp_value_native_handle(void* value);

// 在原生对象上挂 / 取 Python 属性 (对应 ffi_instance::attributes)
// Attach / read a Python attribute on a native object
// ᠠᠲᠲᠷᠢᠪᠶᠤᠲ
int   mp_value_native_set(void* value, const char* name, void* item);
void* mp_value_native_get(void* value, const char* name);

// ================= 6. 原生回调上下文 / Native ctx =================
// 原生回调的 ctx 参数支持以下接口
// Inside a native callback the ctx parameter supports these calls
// ᠨᠠᠲᠢᠪ ᠶᠢᠳᠡ ᠶᠢᠨ ᠬᠤᠷᠢᠶᠠᠯᠳᠠᠭᠤᠯᠭ᠎ᠠ

// 位置参数个数 / 取第 index 个 (越界返回 NULL)
// Positional argument count / get the index-th (NULL when out of range)
// ᠪᠠᠶᠢᠳᠠᠯ ᠬᠣᠶᠠᠳᠠᠯ ᠦᠨ ᠲᠣᠨᠢ
size_t mp_ctx_argc(void* ctx);
void*  mp_ctx_arg(void* ctx, size_t index);

// 关键字参数 / Keyword arguments
// ᠨᠡᠷᠡ ᠲᠣᠭᠲᠠᠭᠤᠯᠭ᠎ᠠ
void* mp_ctx_kwarg(void* ctx, const char* name);
int   mp_ctx_has_kwarg(void* ctx, const char* name);

// 当前上下文所属的解释器 / The interpreter owning this context
// ᠪᠠᠶᠢᠨᠠᠳ᠋ᠣᠭᠰᠣᠨ ᠮᠠᠰᠢᠨ᠎ᠡ
void* mp_ctx_vm(void* ctx);

// 当前 self (方法调用时, 否则 NULL) / Current self (NULL for plain calls)
// ᠣᠳᠣᠮᠤ self
void* mp_ctx_self(void* ctx);

// 在 ctx 上记录错误 (之后返回非 MP_OK 即可传播给 Python 侧)
// Report an error on the ctx (then return non-MP_OK)
// ᠦᠬᠡᠬᠡᠵᠡᠬ᠎ᠠ ᠪᠢᠴᠢᠭ
void  mp_ctx_error(void* ctx, const char* type, const char* message);

// 在 ctx 中构造原生对象作为返回值
// Build a native object as a return value inside the callback
// native object ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
void* mp_ctx_make_native(void* ctx, void* handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MINIPY_H
