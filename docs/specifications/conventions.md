# Coding Conventions

How the Cminor compiler's own source is written. These rules were inferred from the existing code and confirmed on 2026-09-30. New code follows them; code that doesn't is either a bug or a reason to change this file first.

---

## Naming

- **Types:** `PascalCase` (`SourceText`, `TokenKind`, `FilePosition`).
- **Functions:** `module_verb[_object]` in snake_case, where the prefix is the module or type name (`list_find_or_push`,
  `file_position_get_source_text`, `lexer_lex_source_text`).
- **Enum constants:** `TYPENAME_VALUE` in UPPER_SNAKE (`TOKEN_KW_INT`, `DIAGNOSTIC_SEVERITY_ERROR`).
- **Sentinels and aliases** use a double underscore after the prefix: `TOKEN__SPELLABLE_COUNT`, `DIAGNOSTIC_CODE__COUNT`, `TOKEN__KW_FIRST`.
- **Files:** one module per `PascalCase.c` / `PascalCase.h` pair. Include guards: `C_COMPILER_<NAME>_H`.

## Modules and objects

- The object parameter is named **`this`** and comes first.
- Heap objects come in **`X_create` / `X_destroy`** pairs. `create` returns a handle, or `NULL` when the failure is the *user's* fault (e.g. a bad path). A broken precondition is an assert, not `NULL`.
- A struct is **opaque** (declared in the header, defined in the `.c`) when it has private invariants: `Lexer`, `List`.
- **Plain-data** structs are public and passed by value: `Token`, `Range`, `FilePosition`, `TextView`.

## Integer types

- Sizes, lengths, offsets and indices are **`int`**, not `size_t`. This is deliberate:
  - It avoids signed/unsigned comparison bugs.
  - `-1` works as the "not found" sentinel (`list_index_of`).
  - It stays inside the Cminor subset (no `unsigned`, no `size_t`), keeping self-hosting possible (ADR-004).

## Characters and bytes

Plain `char` is signed on the host (GCC, x86-64), so a byte ≥ 0x80 reads back **negative** (`0x88` → −120). These rules keep that from mattering:

- **Store bytes as `char`** (`Text`, `TextView`, buffers). String literals and libc use `char*`, and `memcpy`/`memcmp` preserve and compare the bits regardless.
- **Never treat a stored `char` as a number directly**: no arithmetic, array indexing, `<ctype.h>` calls, shifts, or comparisons against values > 127 on `data[i]`.
- **Read numeric values through one choke point** that returns `(unsigned char)` widened to `int`, giving 0..255 (or a negative sentinel like `LEXER_EOF`). Today that's `lexer_peek_at` (`Lexer.c`). New readers, e.g. ADR-010's escape decoder, either use it or follow the same pattern.
- **Pass byte values between functions as `int`** in 0..255, like `getc` and `isalpha`, never as `char` parameters.
- **Storing a value back** (`data[i] = (char)c` with `c` in 0..255) is implementation-defined, not UB. GCC keeps the low 8 bits, so the stored byte is exact. Keep the explicit cast to show the narrowing is deliberate.

Comparing a stored `char` with a character literal below 128 (`c == '\n'`) is always safe.

## `const`

- **Deep const:** a pointer parameter is `const` only if calling the function can't change the observable state of the
  object, *including state reached through its fields*. Example: `file_position_get_source_text` takes a non-
  `const Context*`, because the `SourceText*` it returns can be used to modify the context's state.
- Pointee `const` (`const List* this`) appears in both declarations and definitions.
- Top-level `const` on value parameters (`const int offset`) appears in **definitions only**. It's legal to leave it out of the header because top-level const isn't part of the function's type.

## Macros

- **Function-like macros are banned**, except where only a macro can do the job. Each exception must justify itself in its doc comment. Current exceptions:
    - `text_view_value_of_literal` (TextView.h): needs `sizeof` of a string literal at compile time.
  - The test harness's `CHECK` (`tests/Harness.h`): needs `__FILE__`, `__LINE__` and `#cond`.
  - The test harness's `RUN_TEST` (`tests/Harness.h`): needs the test function's name as text (`#test`).
- **Object-like constants** (`#define LEXER_EOF ...`) are tolerated, but `enum` or `static const` is preferred.

## Errors

The full policy and its rationale are in **ADR-011** (`architecture-decisions.md`). In short:
- `assert(cond && "message")` is only for compiler bugs and broken caller preconditions.
- Anything wrong in the *user's source* becomes a diagnostic, and processing continues.
- Library code never calls `exit`. Only the driver decides to stop, after a stage that reported errors. The one exception is allocation failure (ADR-013).

## Ownership

- By default a pointer parameter is **borrowed**: the caller keeps ownership.
- Every **transfer** is documented with `@warning ownership of ... is transferred to the caller/callee` (see `List.h`).
- Containers **own their elements** and take a `destroy_element` callback (`list_create(free)`).
- **`*_list_*` adapters** (`text_view_list_compare`, `source_text_list_destroy`, `utils_string_list_compare`) wrap typed
  functions in the `void*` signatures `List` expects. Mark them with a `// for list.h` comment.

## Documentation

- Doxygen syntax, per [Cheat_Sheets/Doxygen_Cheat_Sheet.md](../Cheat_Sheets/Doxygen_Cheat_Sheet.md).
- Every public function gets a `/** */` block in its header: `@param`, `@return` / `@retval`, and `@warning` / `@attention` / `@note` for contracts.
- `static` functions get the same treatment in their `.c` file.
- Struct members: `///<` trailing comments.
- Inline: code names in `@c`, parameter names in `@b`.

## Formatting

- `.clang-format` (in `code/`) is the formatter's source of truth: 4 spaces, `else` on its own line, indented `case`
  labels, `int* p` (the star belongs to the type), no re-wrapping of long lines.
- `target_compile_options(... "-iquote ...")` in `CMakeLists.txt` (not `-I`): project headers are found by
  `#include "x.h"` only, so one named like a system header can't hide it from `#include <x.h>`.
