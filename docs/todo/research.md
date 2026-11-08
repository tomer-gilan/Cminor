# Research

Topics to study. Each one lists the questions to answer, not the answers.

---

## Translation phases (C11 §5.1.1.2)

The standard defines the "stages of compilation" as **8 translation phases** (not 7). My compiler has no preprocessor, but several phases still apply to the lexer.

- [ ] Read §5.1.1.2 in N1570 and summarize each phase in one line.
- [ ] Figure out which phases my compiler performs, which it skips, and where each one lives (lexer, parser, or nowhere):
  - [ ] **Phase 1**: source character mapping (and trigraphs). Relates to the "7-bit ASCII source only" idea: is that a phase-1 decision?
  - [ ] **Phase 2**: backslash-newline **line splicing**. This is *not* part of the preprocessor directives. Does SUBSET.md include it or exclude it? What does my lexer do with `in\` + newline + `t` today?
  - [ ] **Phase 3**: decomposition into preprocessing tokens; **comments are replaced by one space**. My lexer still has a TODO for comments. Which comment forms are in the subset?
  - [ ] **Phase 4**: preprocessing directives and macro expansion. Out of scope (SUBSET.md), but know where the boundary is.
  - [ ] **Phase 5**: escape sequences in character constants and string literals are converted. My lexer already does this. Confirm it matches the standard's list.
  - [ ] **Phase 6**: **adjacent string literal concatenation** (`"ab" "cd"` → `"abcd"`). My lexer doesn't do this. Is it in the subset? If yes, does it belong in the lexer, a pass between lexer and parser, or the parser?
  - [ ] **Phase 7**: preprocessing tokens become tokens; syntactic and semantic analysis (my lexer, parser, semantic analysis).
  - [ ] **Phase 8**: linking (handed off to gcc/`ld`, per ADR-001).
- [ ] Record the outcome in SUBSET.md (what's supported) and, where it's a design choice, in `architecture-decisions.md`.

---

## Source slices (done: the literal limit is gone; this is background for lazy decoding)

Background: a *slice* (or *view*) is a (pointer or offset, length) pair that refers to text that already exists
somewhere, instead of copying it. Your `Range` and `TextView` are slices, and identifiers already work this way: the
token's `Range` points into the source text, and the identifier table stores `TextView`s into it. String and character
literals don't: the lexer decodes them into a growable `Text` (this replaced the old fixed 4096-byte buffer, and with it
the 4095-byte limit and the silent truncation). The questions below are now only about whether decoding eagerly is the
best design.

- [ ] Why can't a string literal's *contents* just be a slice of the source, the way an identifier's can? (Hint: what do escape sequences do to the length?)
- [ ] What are the options for decoding without a fixed-size buffer?
  - Decode into a growable buffer (`realloc` as needed).
  - Measure first, then allocate exactly: one pass counts the decoded length, a second pass writes it.
  - Keep only the `Range` on the token and decode lazily, when the string table (or codegen) needs the bytes.
- [ ] Note that the decoded length is never *longer* than the spelling (every escape is ≥ 2 characters that decode to 1 byte). How could that give a simple exact-or-larger allocation?
- [ ] Which of these fits ADR-010 (literals interned as bytes + length) best?

---

## Memory checking: Valgrind

The build already runs on Ubuntu (CLion remote toolchain, ADR-001), so all of this is available.

- [ ] **Valgrind (Memcheck)**: runs the unmodified binary on a simulated CPU and tracks every allocation and memory access. Slow (~20-50x), needs no recompilation.
  - [ ] Run: `valgrind --leak-check=full --show-leak-kinds=all ./c_compiler test.c`
  - [ ] Learn the leak categories: *definitely lost*, *indirectly lost*, *possibly lost*, *still reachable*. Which ones matter, and why "indirectly lost" usually goes away once you fix the "definitely lost" block that owned it.
  - [ ] Learn to read the other errors: invalid read/write, use of uninitialised value, invalid free. `--track-origins=yes` shows where an uninitialised value came from.
- [ ] `ctest -T memcheck`: running the whole test suite under Valgrind.

---

## Sanitizers

Compiler instrumentation: the compiler inserts checks into the binary at build time. Much faster than Valgrind (~2x), and errors are reported at the exact moment they happen, with a stack trace.

- [ ] **How they work**: shadow memory, redzones around allocations, instrumented loads/stores. Why that makes them fast.
- [ ] **AddressSanitizer (ASan)**: `-fsanitize=address -fno-omit-frame-pointer -g`. Heap/stack/global buffer overflows, use-after-free, double free.
- [ ] **LeakSanitizer (LSan)**: included in ASan on Linux; reports leaks at exit. How it compares to Valgrind's leak report.
- [ ] **UndefinedBehaviorSanitizer (UBSan)**: `-fsanitize=undefined`. Signed overflow, bad shifts, misaligned access, out-of-range enum/bool values, etc. Can be combined with ASan. Look at `-fno-sanitize-recover` (stop on the first error instead of continuing).
- [ ] **MemorySanitizer (MSan)**: uninitialised reads. Clang-only, and needs everything instrumented. Is it worth it here, given Valgrind covers the same thing?
- [ ] Which sanitizers can be combined in one build, and which can't (ASan + MSan, ASan + Valgrind).
- [ ] Reading a sanitizer report: the error type, the access, where the memory was allocated and freed.
- [ ] Wire a sanitizer build into CMake as a separate build type or CLion profile, so the plain Debug build stays plain. Run the test suite under it.
- [x] Also turn on warnings: `-Wall -Wextra` (on in `CMakeLists.txt` for both targets).
- [ ] Maybe later: `-Wpedantic`, `-Wconversion`. What does each add, and how noisy is it on this codebase?
- [ ] The limit shared by all dynamic tools: they only find bugs on paths that actually *run*, so they're only as good as the test inputs (see "Code coverage").

---

## Code coverage

Measures which parts of the code the tests actually executed. It tells you what is **untested**. It does not tell you that tested code is correct.

- [ ] **Kinds of coverage**: function, line (statement), branch, condition. Why 100% line coverage can still miss a branch (`if` without `else`, short-circuit `&&`/`||`). What MC/DC is, and why safety-critical code requires it.
- [ ] **gcov**: GCC's coverage tool. Build with `--coverage` (= `-fprofile-arcs -ftest-coverage`), run the tests, and read the `.gcda`/`.gcno` data.
- [ ] **Reports**: `lcov` + `genhtml`, or `gcovr`, for an HTML report with lines colored by hit/miss.
- [ ] **CLion**: "Run with Coverage" on the CTest configuration; the coverage tool window and editor gutter highlighting.
- [ ] Why coverage builds should use `-O0` (optimization merges and removes lines, which confuses the counts).
- [ ] Keep coverage as a separate build type (like the sanitizer build), not part of Debug.
- [ ] How to use it well: look for **uncovered branches in the lexer** (error paths, EOF paths), then write tests for them. Don't chase a percentage; a target number invites tests that execute code without checking anything.
- [ ] Coverage ≠ correctness: a line can run under a test with no assertion that would catch it being wrong.

---

## Compile-time checks: `_Static_assert`

From the 2026-09-30 code review (`reviews/2026-09-30-code-review.md`, Minor → Token.h): the "DO NOT FORGET TO UPDATE THE ALIASES" comment in `Token.h` is a job for the compiler. C11 has `_Static_assert`.

- [ ] What `_Static_assert(constant-expression, "message")` does (C11 §6.7.10), and where it's allowed: file scope, block scope, inside a struct?
- [ ] What counts as an integer constant expression (C11 §6.6)? Are enum constants one? Is `sizeof` of an array? Is a `const int` variable? (It isn't in C, unlike C++.)
- [ ] Go through the aliases at the end of `TokenKind` and ask, for each one, which relationship `_Static_assert` could check. For example: is `TOKEN__PUNC_LAST` really the member just before `TOKEN__SPELLABLE_COUNT`? Is `TOKEN__KW_LAST` right before `TOKEN__PUNC_FIRST`?
- [ ] Which mistakes it **can't** catch. For example, a new keyword added after `TOKEN_KW_RETURN` without updating `TOKEN__KW_LAST` still compiles if the checks only compare aliases with each other. Which check would catch that one?
- [ ] Can it check that `token_kind_spelling_table` has an entry for every spellable kind? (Hint: an array's size vs. its *initialized* entries, and designated initializers.) If not, that belongs in a unit test (see `testing.md`).
- [ ] Other places in the codebase where a comment says "keep X in sync with Y" or a table must match an enum (`diagnostics_table` vs. `DiagnosticCode`), and whether a static assert fits there.

---

## Unit testing

Background and decisions are in `testing.md`. Open questions:

- [ ] Compare harnesses after the hand-rolled one works: Unity, µnit, CMocka, Criterion. What does each add (fixtures, mocking, death tests, output format)?
- [ ] How CTest works: `enable_testing()`, `add_test`, labels, `ctest --output-on-failure`, running a subset with `-R`.
- [ ] **One CTest entry per test case, not per executable.** With the hand-rolled harness, one executable is one CTest entry, so a failure shows as a single red "lexer" line. Option: the runner takes a test name as an argument (`test_lexer lex_eq_eq_semicolon`) and CMake registers one `add_test` per name. How do you keep the CMake list of names in sync with the test functions without maintaining it by hand twice? How do frameworks (and CMake's `gtest_discover_tests`, for comparison) solve this?
- [ ] Mocking in C: what it is, and when (if ever) this project would need it.
- [ ] Testing that an `assert` fires ("death tests"): how frameworks do it with `fork()`, and whether it's worth it here.

---

## Random test programs: Csmith

**Csmith** (University of Utah) generates random C programs that are free of undefined behavior and print a checksum of
their global variables. You compile each one with two compilers and compare the outputs (*differential testing*). This
grows out of the differential check in `codegen-roadmap.md` (Testing, 2.): that one uses hand-written programs, while
this one uses generated ones.

- [ ] How Csmith avoids undefined behavior (safe-math wrappers, pointer and initialization analysis), and why a generator
  for a compiler *has* to.
- [ ] What its output depends on: `csmith.h`, `printf`, `#include`, structs, pointers, loops. How much of that is
  outside `SUBSET.md` (the preprocessor certainly is), and which command-line options turn features off (`--no-structs`,
  `--no-pointers`, ...). Can it be cut down to Cminor's subset at all, or only to a later slice's?
- [ ] If not: what would a minimal home-made generator look like? Random expression trees for `int expr(void)`, gcc as
  the oracle, `-fsanitize=undefined` to throw out programs with UB, a fixed or printed seed so a failure can be replayed.
- [ ] **Test-case reduction:** a failing random program is thousands of lines long. What **C-Reduce** (and `cvise`) do
  with it, and what the "interestingness test" script is.
- [ ] Related: **YARPGen** (another generator, aimed at optimizers), and what Csmith's paper found when it tested
  **CompCert** (a formally verified compiler). Which generator fits a non-optimizing compiler better?
- [ ] Where it runs: random tests aren't deterministic (see `testing.md`), so not in the commit check. A separate CTest
  label, or a script run by hand?
