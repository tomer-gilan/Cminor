# Testing Roadmap

The step-by-step plan for building the Cminor test suite. The background, structure and decisions behind it are
in [](../todo/testing.md).

Built step by step. Each step ends in a state that works and can be committed.

**Step 0: Prerequisites**

- [x] A way to create a `SourceText`/`Context` from an in-memory string (`source_text_create_from_buffer` +
  `context_create`).

The real `main` was moved from here to step 7: the harness comes first, so bugs show up one function at a time in unit
tests, not all at once through `main`.

**Step 1: Library split**

- [x] Move everything except `main.c` into a CMake library; `main` links it.
- [x] The compiler still builds and runs exactly as before.

**Step 2: Hand-rolled harness**

- [x] `CHECK` macro (s) and a runner `main` that returns non-zero on any failure.
- [x] One trivial test. Watch it fail (break it on purpose), then pass.

**Step 3: CTest + CLion**

- [x] `enable_testing()` and `add_test` for the test executable.
- [x] Tests appear in CLion's test tree; run them from there.

**Step 4: First real unit tests**, one sub-step per test executable.

**Step 4a: `List`** (`test_list`)

- [x] Push past capacity, pop from empty, `find_or_push` returns the existing index.

**Step 4b: `TextView`** (`test_text_view`)

- [x] Equality, including different lengths with a shared prefix.

**Step 4c: Lexer** (`test_lexer`)

- [x] Single-token and multi-token tests (written by Tomer).
- [x] Regression tests from bugs already found: `" x"`, `"x\n"`, a file that is just `+`, `==;`, `''`, `'ab'`, `123abc`,
  4095- and 4096-char string literals (the old fixed-buffer boundary, now just long literals), an empty file, identifier
  interning (`a b a` → same id for both `a`s), and string interning with `\0` (ADR-010). Written by Claude (marked in
  each test's doc comment). The error cases only check the tokens and recovery; their diagnostics are Step 8.

**Step 5: Commit check (ADR-002)**

- [x] Enable "Run Tests" in CLion's commit dialog with the CTest configuration.
- [x] Verify: break a test, try to commit, confirm it's caught.

**Step 6: Sanitizer build**

- [x] ASan + UBSan build configuration (`-DENABLE_SANITIZERS=ON`, see [`../testing.md`](../todo/testing.md), "Sanitizer and
  coverage builds"); the whole suite runs under it.
- [x] The sanitizers are on by default in Debug builds (`ENABLE_SANITIZERS` defaults to ON when `CMAKE_BUILD_TYPE` is
  Debug), so the normal "All CTest" run, and with it the commit check, is sanitized. The Debug profile's cache now says
  ON.
- [x] `CMakePresets.json` defines a `coverage` preset (shared, versioned) for CLion; enable it in Settings → Build,
  Execution, Deployment → CMake.
- [x] Fix whatever it finds, and add a regression test for each. The sanitizer build found the two known
  `source_text_create_from_path` leaks (file name copied twice, nothing freed for an invalid path) through
  `tests/test_source_text.c` (written by Claude). Tomer fixed `SourceText.c`; all 4 CTest tests now pass under ASan +
  UBSan (checked 2026-10-11 with a fresh Debug build in WSL).

**Step 7: Golden-file tests (ADR-003)**

> **Deferred, out of v1 scope** (Tomer's call, 2026-10-12). The lexer is covered by unit tests (Steps 4c, 6, 8, 9), so
> golden files for it are not needed to call the lexer done. Nothing here was started. The golden-file runner is built
> when a stage actually needs it, which is the parser's `--dump-ast` (see [`parser-roadmap.md`](parser-roadmap.md), Step
> 0f); then the lexer's token-dump golden files can be added on top, with the same runner. The unticked items below are
> the plan for that day, not work owed before `lexer-v1`.

- [ ] A real `main` that drives the compiler and dumps tokens, ideally behind a flag like `--dump-tokens` (`main.c` is
  still a sample program).
- [ ] Settle the token-dump format.
- [ ] Runner with pass/fail per file and an "update expected" mode; register with CTest.
- [ ] Extend `.gitattributes` (it already pins LF for `*.sh`) to the test inputs and expected files.
- [ ] First golden files: every punctuator, every keyword, a small realistic program.

**Step 8: Diagnostics in tests**

- [x] Diagnostics are collected in `Context` and every lexer call site reports one, so the error cases are tested: which
  code, at which position (exact offset and length), in which order, and that lexing continues after it. 12
  `test_lexer_diag_*` tests in `tests/test_lexer.c` (written by Claude, 2026-10-11), one per diagnostic code plus clean
  input and several-errors-in-order; the Step 4c token tests still check the tokens.
- [x] The two tests that failed on purpose are settled: Tomer fixed the escape-sequence range in `Lexer.c`
  (`test_lexer_diag_invalid_escape_sequence`), and decided that `'ab` cut by the end of the file reports both the
  missing delimiter and the multi-character constant, so `test_lexer_diag_no_cascade` expects both.

**Step 9: Coverage**

- [x] Coverage build configuration (`-DENABLE_COVERAGE=ON`) and `tests/coverage.sh`, which lists the lines that never
  ran and the branches never taken.
- [x] Find the uncovered lexer branches and test them: `Lexer.c` went from 86.8% of lines and 76.8% of branches taken to
  100% of lines and 92.0% of branches (11 tests added to `test_lexer.c`, written by Claude). What remains is `lexer_pop`
  's `peek != LEXER_EOF` check (`Lexer.c:77`): every caller peeks first, so it can't be reached through the public
  interface.
- [x] Coverage is automatic but never blocks a commit: the CMake target `coverage` prints the report on demand, and
  `-DENABLE_COVERAGE_CHECK=ON` adds a CTest test `coverage` that fails if a line of `Lexer.c` never ran. Both are off
  the normal path ("All CTest" and the commit check don't run them); the failing check is for CI (see "Later").
- [ ] After the lexer changes (comments, diagnostics), look at the never-taken branches in the report and cover the new
  ones. **Deferred with comments:** comment lexing is out of v1 scope (`TODO.md`), so this waits for it; it doesn't
  block `lexer-v1`. The diagnostics half is done (Step 8).

**Step 10: Lexer milestone**

- [ ] Claude reminds Tomer that the roadmap is finished. With his approval, tag the commit:
  `git tag -a lexer-v1 -m "Lexer complete for v1 scope"`. The roadmap counts as finished for v1 with Steps 7 and the
  last
  Step 9 item deferred (see their notes); Tomer's decision, 2026-10-12.

**Later**

- Parser tests (unit tests, AST dump, golden files) are part of [`parser-roadmap.md`](parser-roadmap.md), from its first
  step.
- [ ] End-to-end tests (compile, link, run, check exit code/stdout), per [
  `../educational/followup-setup-testing-expectations.md`](../educational/followup-setup-testing-expectations.md).
- [ ] CI (GitHub Actions) as a second safety net.
