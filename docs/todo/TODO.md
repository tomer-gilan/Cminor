# TODO

## Diagnostics mechanism

Replace the scattered `// TODO: issue diagnostic` comments with a real reporting path.

- [x] Decide where diagnostics are collected: in `Context` (ADR-012).
- [ ] Implement the collection: a `List` of diagnostic instances in `Context`, printed per stage, sorted by position, in the format from `implementation-defined.md` (header line + source line + `^` caret).
- [ ] Track an error count, so the driver can stop after a stage that reported errors (don't run the parser on a broken token stream).
- [x] Decide on emit-immediately vs. collect-then-print: collect (ADR-012).
- [x] Fix `diagnostic_is_valid_code` in `Diagnostic.c`: `0 < code` rejects code `0` (`DIAGNOSTIC_CODE_LEXER_UNKNOWN_CHARACTER`).
- [ ] Add missing codes: empty character constant, multi-character constant, unterminated literal (at EOF).
- [ ] Wire up every lexer call site:
    - [ ] `lexer_text_view_to_int`: constant out of bounds
  - [ ] `lexer_lex_literal_to_buffer`: invalid escape sequence
  - [ ] `lexer_lex_literal_to_buffer`: unexpected EOF (unterminated literal)
    - [x] `lexer_lex_literal_to_buffer`: literal longer than the old fixed buffer. Gone: literals are growable `Text`
      and have no length limit (`implementation-defined.md`).
  - [ ] `lexer_lex_constant_int`: invalid numerical sequence (`123abc`)
  - [ ] `lexer_lex_constant_char`: empty character constant (`''`)
  - [ ] `lexer_lex_constant_char`: multi-character constant (`'ab'`)
  - [ ] `lexer_lex_token`: unknown character
- [ ] Audit asserts that fire on *user/environment* errors rather than compiler bugs (e.g. `argc > 1` in
  `driver_create`, `ftell` failure in `text_create_from_path`) and turn them into diagnostics.

## Allocation: `xmalloc` / `xrealloc`

`assert(ptr != NULL)` after an allocation disappears under `NDEBUG`, and most call sites don't check at all.

- [x] Add `xmalloc` / `xrealloc` (and maybe `xstrdup`) wrappers that abort with a message on failure.
- [x] Replace every raw call site:
  - [x] `Context.c` (`context_create`)
  - [x] `Driver.c` (`driver_create`)
  - [x] `Lexer.c` (`lexer_create`, both literal buffers, per-token allocation in `lexer_lex_source_text`)
  - [x] `List.c` (`list_create` x2, `list_expand`)
  - [x] `SourceText.c` (text buffer, `SourceText` struct)
  - [x] `TextView.c` (`text_view_list_duplicate`)
  - [x] `Utils.c` (`utils_allocate_and_copy_string`)
- [x] Remove the now-redundant `assert(... != NULL && "malloc failed")` lines.

## Lexer changes from the 2026-09-30 decisions

Decided in `implementation-defined.md`, `SUBSET.md` (pending, see below) and the ADRs; the lexer doesn't do these yet.

- [x] **Bug: string literals can't hold `\0`** (ADR-010). Fixed: literals are now `Text` (bytes + length), interned with
  the length-aware `text_list_compare`. Regression test: `test_lexer_string_interning_with_nul`.
- [x] Escape sequences: add `\0`, remove `\e` (a GNU extension, not standard C).
- [x] Whitespace: accept all of space, `\t`, `\n`, `\v`, `\f`, `\r` (CRLF files currently fail on every line). Only `\n` advances the line count.
- [ ] Comments: lex and skip `/* ... */` and `// ...` (an unterminated `/*` is an error). Don't confuse `/` the operator with the start of a comment.
- [ ] Bytes 0x80–0xFF: allowed raw inside literals and comments, *unknown character* error anywhere else.
- [ ] Integer constant that doesn't fit in `int`: error, instead of silently becoming 0.
- [ ] `sizeof` keyword (decided 2026-10-13, `SUBSET.md` A.1.2). Needed by parser slice 14, not before.

## SUBSET.md updates

Decisions from the 2026-09-30 quiz, to be written into `SUBSET.md`. Written 2026-10-13, together with the parser
scope from the 2026-10-13 quiz (A.2), except for the EBNF:

- [x] A.1.2 keyword note: rewrite it. "Only signed integers and signed characters" leaves out `void`. `void` covers function return types, `(void)` parameter lists and **`void*`**. Decided: implicit `void*` ↔ `T*`, as in C; no dereference or arithmetic on `void*`.
- [x] Notable left-outs: backslash-newline line splicing (out), adjacent string literal concatenation (out for now). `sizeof` changed on 2026-10-13: the expression form is in.
- [x] Comments: both `/* */` and `//` are in (translation phase 3; each comment becomes one space).
- [x] A.1.5/A.1.6 escape sequences: exactly the 11 simple escapes `\'` `\"` `\?` `\\` `\a` `\b` `\f` `\n` `\r` `\t` `\v`, plus `\0`. No other octal escapes, no hex escapes, no `\e`.
- [x] Unary operators: all six prefix operators `- + ! ~ * &` are in.
- [ ] Fill in the **A.2 Phrase structure grammar** EBNF (the scope is written; the grammar isn't): A.2.1 Expressions, A.2.2 Declarations, A.2.3 Statements, A.2.4 External definitions. Write each as a trimmed-down (E)BNF of what Cminor accepts (see `annex-a-breakdown.md`). This is the parser's spec, so it has to exist before the parser does.
- [x] Record pointer − pointer as unsupported (only pointer ± int; see `implementation-defined.md`).

## Parser

- [ ] **Driver integration.** The driver lexes the file, stops if the lexer reported errors (ADR-012), then creates a
  `Parser` on the token list, calls `parser_parse`, and stops if the parser's error flag is set. The `Parser` and the
  token list are destroyed after parsing; the AST lives in `Context`'s arena. Related: the "Real `main`" item under
  "Other".
- [ ] **AST dump, ELI15.** Before designing the dump format (parser roadmap Step 2), get a from-scratch explanation:
  what an AST dump is, why tests compare dump strings instead of node fields, and what the format options look like
  (S-expressions like `(+ 1 (* 2 3))` vs. indented trees, and what to print for ranges and ids).

## Docs

- [ ] Fix `overview.md`: it's still pre-decision generic advice. Record the actual answers to §2 and §6 (implementation language, host, preprocessor, backend, self-hosting); they're now in ADR-001 and ADR-004 to ADR-006, so link to those, and replace §4 ("minimal subset") with a pointer to `SUBSET.md`, which it currently contradicts (e.g. bitwise operators).
- [ ] System V AMD64 ABI / codegen cheat sheet in `Cheat_Sheets/` (argument and return registers, caller- vs. callee-saved, 16-byte stack alignment before `call`, stack frame layout). Needed before codegen, not before.
- [ ] *(Deferred out of v1, 2026-10-12: golden files wait for the parser's dump, testing roadmap Step 7.)* Token-dump format spec: the exact line format of the lexer's token dump (fields, order, separators, trailing newline). It's the contract for the golden-file tests (`testing.md`, "The dump format"), so settle it before writing any `.expected` file.

## Research

See `research.md` (translation phases, Valgrind, sanitizers, code coverage, unit testing), `testing.md` (test suite
structure), and `roadmaps/` (the testing and parser roadmaps).

## Possible future goals

Not planned; recorded so the trade-offs don't have to be re-derived.

- [ ] **Migrate the compiler's own source from C11 to C23** (`CMAKE_C_STANDARD 23`). The main gain: `f()` becomes a
  real prototype (it means `(void)`, as in Cminor), so empty parameter lists get argument checking. Checked on
  2026-10-16: every `.c` file compiles as `gnu23` with GCC 15.2 with no errors. The costs:
  - it supersedes ADR-004, so it needs a new ADR;
  - the toolchain floor rises to GCC 13+ / Clang 16+;
  - `bool`, `true`, `false`, `nullptr`, `typeof`, `constexpr`, `static_assert` become keywords;
  - most references (books, chibicc) are C99/C11.

## Other

- [ ] *(Deferred out of v1, 2026-10-12, same reason.)* Real `main`: build a `Driver`, lex the file, dump tokens (kind, spelling, line:column). `main.c` is still a sample C program.
