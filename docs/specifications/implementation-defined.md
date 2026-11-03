# Implementation-Defined Behavior in Cminor

The C standard leaves some behavior to the implementation, and requires every implementation to **document its choices** (C11 §3.4.1, the full list is Annex J.3). This file is that documentation for Cminor. It also records a few *unspecified* and *undefined* behaviors where Cminor still makes a concrete, documented choice.

Everything here is binding: tests use these rules as the spec, and code that disagrees with them is a bug (or the doc has to change first).

**Status** column: ✅ the current code matches · 🔧 decided but not implemented yet (tracked in `TODO.md`) · ⏳ belongs to a stage that doesn't exist yet.

Decided 2026-09-30. Parser-scope rows (`sizeof`, `()` parameter lists) added 2026-10-13.

---

## Target data model

Cminor follows the System V AMD64 ABI (see ADR-001), the same as gcc on x86-64 Linux. That keeps Cminor programs compatible with libc.

| Type | Size | Alignment | Representation | Status |
|---|---|---|---|---|
| `char` | 1 byte | 1 | **signed**, two's complement, range −128..127 | ⏳ |
| `int` | 4 bytes | 4 | two's complement, range −2147483648..2147483647 | ⏳ |
| any pointer (incl. `void*`) | 8 bytes | 8 | a 64-bit address; null pointer = all bits zero | ⏳ |

- Plain `char` is signed (J.3.4). Gotcha: a byte ≥ 0x80 in a string literal reads back as a **negative** `char`.

## Code model

| Behavior | Cminor choice | Status |
|---|---|---|
| Reach of code and static data | The **small code model**, gcc's default (`-mcmodel=small`): the program's code and static data (globals, string literals) together fit within 2 GB, so every global is reached as `symbol(%rip)`, whose displacement is a signed 32 bits (±2 GB from the instruction). | ⏳ |

Gotcha: a program that breaks the assumption, e.g. a global array larger than about 2 GB, compiles and assembles, then
fails at **link** time with a relocation-overflow error. Stack frames have the same ±2 GB limit, since frame offsets are
32-bit displacements too (`-8(%rbp)`). Whether sema rejects such objects up front with a diagnostic is still open.

## Translation environment

| Behavior | Cminor choice | Status |
|---|---|---|
| **How diagnostics are identified** (J.3.1) | gcc style, 1-based: `file:line:col: severity: message`, then the offending source line and a `^` caret under the column. See the example below. | 🔧 |
| Accepted forms of `main` (J.3.2) | `int main(void)` (or `int main()`, which means the same in Cminor) and `int main(int argc, char** argv)` only. Falling off the end of `main` returns 0 (required by C99+, not a choice). | ⏳ |

```
main.c:3:14: error: invalid escape sequence '\q'
    char c = '\q';
             ^
```

## Source characters and lexing

| Behavior | Cminor choice | Status |
|---|---|---|
| Source character set (J.3.3, J.3.4) | 7-bit ASCII. The source is read as raw bytes; nothing is ever decoded as Unicode. | ✅ |
| Bytes 0x80–0xFF | Allowed **only inside string literals, character constants and comments**, copied through byte for byte. Anywhere else: an *unknown character* error. | 🔧 |
| Unicode escapes and prefixes | No `\u`/`\U`, no `L""`/`u8""`/`u""`/`U""` literals, no wide characters. | ✅ (not supported) |
| Whitespace | All standard C whitespace: space, `\t`, `\n`, `\v`, `\f`, `\r`. So CRLF files work. Only `\n` starts a new line when counting lines for diagnostics. | 🔧 (today: space, `\t`, `\n` only) |
| Significant characters in an identifier (J.3.3) | **All of them, no limit.** Two identifiers are the same iff they're spelled identically. | ✅ |

Gotcha: a UTF-8 character such as `é` is **two bytes**, so `'é'` is a multi-character constant (an error, see below), while `"é"` is fine: a 3-byte array including the terminator.

## Constants and literals

| Behavior                                                       | Cminor choice                                                                                                                                                       | Status                 |
|----------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------|
| Decimal constant that doesn't fit in `int` (e.g. `2147483648`) | **Error** (`NUMERIC_CONSTANT_OUT_OF_BOUNDS`). There's no `long` to fall back on.                                                                                 | 🔧 (today: silently 0) |
| Multi-character constant, e.g. `'ab'` (J.3.4)                  | **Error.** The token's value is the first character, so lexing continues.                                                                                        | 🔧                     |
| Empty character constant `''`                                  | Error (it isn't valid C anyway).                                                                                                                                 | 🔧                     |
| Escape sequences                                               | The 11 simple escapes `\' \" \? \\ \a \b \f \n \r \t \v`, plus `\0`. No other octal escapes, no hex escapes, no `\e`.                                            | ✅                     |
| Unknown escape, e.g. `\q`                                      | Error.                                                                                                                                                           | 🔧                     |
| Maximum string literal length                                  | **None.** The decoded literal grows as needed (`Text`), so there is no limit besides available memory. (The C standard only requires 4095 characters, §5.2.4.1.) | ✅                     |
| String literal storage                                         | Read-only data (`.rodata`). Identical literals share one copy. Writing to a literal is UB, and in practice crashes with SIGSEGV.                                 | ⏳                     |
| Embedded `\0` in string literals                               | Supported: literals are stored as bytes + length (ADR-010).                                                                                                      | ✅                     |

Gotcha: `-2147483648` is unary minus applied to the constant `2147483648`, which doesn't fit, so it's an error. Write `INT_MIN` as `-2147483647 - 1`.

## Conversions

| Behavior | Cminor choice | Status |
|---|---|---|
| Converting an out-of-range `int` to `char` (J.3.5), e.g. `char c = 300;` | The low 8 bits, reinterpreted as signed (300 → 44, 200 → −56). **Warning** when the compiler can see a constant that doesn't fit. | ⏳ |

## Operators

| Behavior | Cminor choice | Status |
|---|---|---|
| `x >> n` with `x` negative (J.3.5) | **Arithmetic** shift (`sar`), which copies the sign bit: `-8 >> 1 == -4`. | ⏳ |
| Pointer − pointer | **Not supported** (a semantic error). Only pointer ± int. (In C its type would be `ptrdiff_t`, i.e. `long`, which Cminor doesn't have.) | ⏳ |
| `sizeof` result type | **`int`**, not `size_t` (Cminor has no unsigned or `long` types). Values: `char` 1, `int` 4, any pointer 8, an array N × its element size. Only the `sizeof expr` form exists in v1. | ⏳ |
| Integer division and `%` | Truncates toward zero. This is *defined* by C99+, listed here only because people often assume otherwise. | ⏳ |

Gotcha: since `sizeof` gives a signed `int`, `sizeof c - 2 < 0` (with `char c`) is **true** in Cminor but false in C,
where the subtraction is done in unsigned `size_t` and wraps around. It's the one known place where Cminor and C disagree
on a program both accept.

## Functions

| Behavior | Cminor choice | Status |
|---|---|---|
| Empty parameter list `int f()` | Means **no parameters**, exactly like `int f(void)`. This is C23's rule. In C11, `()` declares a function without a prototype, so `f(1)` would be accepted; in Cminor it's an error. Cminor only rejects more, it never changes what an accepted program means. | ⏳ |
| Calling libc with Cminor types, e.g. `void *malloc(int size);` instead of `size_t` | Technically UB (the declaration doesn't match libc's). In practice it works **as long as codegen zero-extends 32-bit arguments**: the ABI leaves the upper 32 bits of an `int` argument's register undefined, but `malloc` reads all 64 bits of `%rdi`. A 32-bit `mov` into `%edi` zero-extends, so it's fine; a sign-extending move or stale upper bits give `malloc` a huge size. Negative sizes become ~4 GB. | ⏳ |

## Evaluation order (unspecified behavior)

| Behavior | Cminor choice | Status |
|---|---|---|
| Order of evaluating function arguments and the operands of binary operators | **Left to right, promised.** | ⏳ |
| `&&`, `\|\|` | Left to right with short-circuiting. Required by the standard, not a choice. | ⏳ |

Correct C programs can't depend on argument order, but Cminor tests may.

## Undefined behavior policy

Cminor adds **no runtime checks**. It emits the plain instruction and the result is whatever the CPU does, exactly like `gcc -O0`. This is *observed* behavior, not a guarantee:

| UB | What actually happens |
|---|---|
| Signed overflow (`INT_MAX + 1`) | Wraps (two's complement). |
| Division by zero, `INT_MIN / -1`, `INT_MIN % -1` | `idiv` faults: SIGFPE, and the program dies. |
| Shift by ≥ 32 or by a negative amount | The CPU masks the count to its low 5 bits. |
| Modifying a string literal | SIGSEGV (it's in `.rodata`). |
| Null or dangling pointer dereference | Usually SIGSEGV. |
