# Parser Roadmap

The step-by-step plan for the Cminor parser: tokens in, AST out. Start it once [`testing-roadmap.md`](testing-roadmap.md)
is finished.

---

## How the roadmaps fit together

The compiler is built in **vertical slices**. Each slice adds one language feature to every stage of the pipeline, and
ends with a program that compiles, runs, and passes an end-to-end test.

There is **one roadmap per stage** (parser, semantic analysis, codegen). Each one lists only its own stage's work, but
they share their step numbers:

> **Step N of every stage roadmap, together, is slice N.**

For example, slice 4 is "bitwise and shift operators": parser Step 4 parses them, sema Step 4 type-checks them, and
codegen Step 4 emits `sal`/`sar`/`and`/`or`/`xor`. A slice is done when Step N is done in every roadmap and its
end-to-end tests pass. Work slice by slice: N in the parser, then N in sema, then N in codegen, then N+1.

The semantic-analysis roadmap is [`sema-roadmap.md`](sema-roadmap.md) and the codegen roadmap is
[`codegen-roadmap.md`](codegen-roadmap.md). Both reuse the slice table below, so the slice list must stay identical
across all three.

### The slices

| #  | Slice                                                     | Example program                        |
|----|-----------------------------------------------------------|----------------------------------------|
| 0  | Preparation (each stage's own setup, no language feature) | —                                      |
| 1  | `return` of an integer constant                           | `int main(void) { return 2; }`         |
| 2  | Unary operators and parentheses                           | `return -(~2);`                        |
| 3  | Arithmetic: `* / % + -`                                   | `return 1 + 2 * 3;`                    |
| 4  | Bitwise and shift: `<< >> & ^ \|`                         | `return 1 << 2 + 1;`                   |
| 5  | Relational, equality, logical: `< > <= >= == != && \|\|`  | `return 1 < 2 && 3 != 4;`              |
| 6  | Local variables, assignment, expression statements        | `int x = 2; x = x * 3; return x;`      |
| 7  | Blocks and `if` / `else`                                  | `if (x) { ... } else ...`              |
| 8  | `while`                                                   | `while (i < 10) i = i + 1;`            |
| 9  | Functions: parameters, calls, prototypes, `void`          | `int add(int a, int b); ... add(1, 2)` |
| 10 | Global variables                                          | `int counter; int main(void) { ... }`  |
| 11 | Pointers: `*` and `&`                                     | `int *p = &x; *p = 3;`                 |
| 12 | Arrays and indexing                                       | `int a[3]; a[1] = 5;`                  |
| 13 | `char`, character constants, string literals              | `char *s = "hi"; return s[1];`         |
| 14 | `sizeof` (expression form)                                | `int a[3]; return sizeof a;`           |

Everything comes from [`SUBSET.md`](../specifications/SUBSET.md). If a slice seems to need something that isn't in it (`for`,
`sizeof(int)`, casts, `++`, `?:`, `+=`), it's out of scope until `SUBSET.md` says otherwise. Its "v2 candidates" list
holds what's planned after v1.

---

## Testing, from Step 1

Every step adds tests in the same three layers. A step isn't done until all of them are in place and pass, both in the
normal build and in the sanitizer build (testing roadmap Step 6).

1. **Unit tests: AST dump strings.** `test_parser` parses a small source string, dumps the AST as text (designed in
   Step 2), and compares it to an expected string, e.g. `"1 + 2 * 3"` → `(+ 1 (* 2 3))`. One line per case, so dozens
   of cases cost almost nothing. Expected values are written by hand from `SUBSET.md` and the standard's precedence
   rules, never copied from the parser's output. Step 1 predates the dump, so its tests check node fields directly.
2. **Error tests.** Each step lists inputs that must be rejected. Check the diagnostic's code and position, and that
   it's the only diagnostic (ADR-020).
3. **Golden files.** One or two realistic programs per step in `tests/golden/parser/`, run through the golden-file
   runner with `--dump-ast`. The runner is the testing roadmap's Step 7, which was deferred out of v1 on 2026-10-12 and
   not started, so the parser roadmap builds it (Step 0f) instead of waiting for it. Until then, 1 and 2 carry the
   tests.

The end-to-end test (compile, link, run, check the exit code) belongs to the codegen roadmap's Step N, but every slice's
example program is chosen so that it can become that test.

---

## Step 0: Preparation

Nothing here parses anything yet. It's the spec, the design and the test oracle, so that Step 1 starts with no open
questions.

**Step 0a: Prerequisites**

- [x] The testing roadmap is finished for v1 (Step 7, the golden-file runner, is deferred, see Step 0f), including
  Step 8: diagnostics are collected in `Context` (ADR-012).
- [x] The driver can stop after a stage that reported errors (`TODO.md`: error count, not done yet).
- Parser diagnostic codes are added as each step needs them, starting in Step 1 (no upfront range).

**Step 0b: The grammar (`SUBSET.md`, A.2)**

- [x] Write A.2.1 (expressions), A.2.2 (declarations), A.2.3 (statements) and A.2.4 (external definitions) as a trimmed
  EBNF of what Cminor accepts, starting from the standard's Annex A (see [`annex-a-breakdown.md`](../specifications/annex-a-breakdown.md)).
- [x] Settle the open decisions below, and record each one in `SUBSET.md` or `implementation-defined.md` (2026-10-13).

Decisions (2026-10-13; the details are in `SUBSET.md` A.2):

- **Comma operator:** out. `,` only separates arguments, parameters and declarators.
- **Multiple declarators** (`int a, b;`): in. `int *p, q;` makes `q` an `int`.
- **Initializers:** locals take any expression. Globals take only an integer or character constant (optionally with one
  leading `-`) or a string literal. Brace initializers are out, so `{` `}` only delimit blocks.
- **Parenthesized declarators** (`int (*p)[3]`, function pointers): out.
- **`()` vs `(void)`:** `()` means `(void)` (C23's rule), recorded in `implementation-defined.md`.
- **Unnamed parameters:** allowed in prototypes, required in definitions.
- **Declarations in blocks:** anywhere (C99), but never as an `if`/`while` body.
- **`--x` and `a--b`:** rejected. Two `-` (or two `+`) tokens whose ranges touch are a syntax error; `- -x` is fine.
- **`-2147483648`:** already documented in `implementation-defined.md` (write `-2147483647 - 1`).
- **Arrays:** multi-dimensional arrays are in. A size is a single integer-constant token, so nothing needs constant
  folding.
- **`sizeof`:** in, expression form only (`sizeof x`), type `int`. It needs a new lexer keyword (slice 14).
- **Calling libc:** in, through hand-written non-variadic prototypes (`int putchar(int c);`, `void *malloc(int size);`),
  linked against libc. No `printf` until v2's `...`.
- **`break`, `continue`, `for`, `do`-`while`:** not in v1.
- **v2 candidates** (optional): `sizeof(type)`, casts, constant folding (constant global initializers and array
  sizes), brace initializers, `...` in prototypes, prefix and postfix `++`/`--`.

**Step 0c: Study**

- [x] *Crafting Interpreters*, ch. 5–6 (representing code, parsing expressions) and ch. 8–9 (statements): recursive
  descent from scratch.
- [x] ~One expression technique: Eli Bendersky, "Parsing expressions by precedence climbing" (recommended: one loop plus
  a precedence table, instead of one function per level), or Bob Nystrom, "Pratt Parsers: Expression Parsing Made Easy".
- [x] ~chibicc's `parse.c`, in its early commits: a C parser written in C.
- [x] Be able to answer:
    - How does your loop make `a = b = c` right-associative but `a - b - c` left-associative?
    - How do `-x * y` and `*p[1]` parse?
    - Why is `1 = x` usually a *semantic* error (not an lvalue), and not a syntax error?
    - What is the dangling-else problem, and why does recursive descent resolve it the standard way for free?

**Step 0d: AST design (ADRs)**

Each of these gets an ADR in [`architecture-decisions.md`](../specifications/architecture-decisions.md):

- [x] **Node families:** separate `Expr`, `Stmt` and `Decl` types (recommended), rather than one `Node` union. Then a
  statement can't end up where an expression belongs. *(ADR-015)*
- [x] **Ranges:** every node carries a `Range` (ADR-008), spanning the whole construct. A binary node spans from its
  left operand's start to its right operand's end. Sema diagnostics will point at nodes. *(ADR-016: punctuation
  included)*
- [x] **Ownership:** an arena owned by `Context` (ADR-007), freed once (recommended), versus a destroy function per node
  kind. *(ADR-017: arena, sequences as `next` pointers)*
- [x] **Types:** declarations carry types (`int *a[3]`), so a `Type` representation (`int`, `char`, `void`, pointer-to,
  array-of, function) is needed now. Decide whether the parser builds `Type`s directly (recommended, since there's no
  `typedef`) or stores raw declarator syntax for sema to interpret. *(ADR-019: the parser builds them, from slice 6)*
- [x] **Identifiers:** nodes hold the interned id from the lexer, not strings. *(ADR-018)*
- [x] **Error policy:** stop at the first syntax error (recommended for v1), or report it and resynchronize at `;` / `}`
  (panic mode). The error tests in every step depend on this. *(ADR-020: stop at the first error; panic mode later,
  maybe)*

**Step 0e: The AST dump format**

Moved to Step 2 on 2026-10-16. Slice 1's tree is small enough to test field by field; slice 2's nested unary
expressions are where that stops scaling.

**Step 0f: Test plumbing** (tooling, so Claude can help directly)

- [x] `test_parser` executable registered with CTest.
- [x] A helper for error tests: parse a string, check that it fails with a given diagnostic code at a given offset,
  and that it's the only diagnostic.
- The dump-compare helper moved to Step 2, together with the dump.
- [ ] The golden-file runner (testing roadmap, Step 7, deferred out of v1 and not started): build it here, then wire
  `tests/golden/parser/` into it, using `--dump-ast`. This is also when the real `main` and the dump format work
  (`TODO.md`) happen. Optional until the unit tests above are in place; slices 1 to 3 can go ahead without it.

---

## Steps 1–14: one per slice

Each step lists what's parsed, the AST it adds, the tests, and the gotchas.

**Step 1: `return` of a constant**

- [x] Token cursor: `peek`, `advance`, `expect(kind)` (reports a diagnostic on a mismatch). It relies on the lexer
  always ending the list with an EOF token, so peeking never runs off the end.
- [x] Parse exactly `int <ident> ( void ) { return <int-constant> ; }`. The general declarator parser comes in Steps
  9–12; the narrow form keeps this step about the skeleton.
- [x] AST: translation unit, function definition, compound statement, `return` statement, integer constant.
- [x] Tests: `int main(void) { return 2; }` parses into the right nodes (checked field by field: the dump comes in
  Step 2); the function name is any identifier, not just `main`.
- [x] Error tests: missing `;`, missing `}`, `return` without an expression (in an `int` function that's a sema matter
  later, so decide where it's caught), trailing tokens after the function.
- Gotcha: an error at EOF needs a sensible position (the EOF token's range is empty).

**Step 2: Unary operators and parentheses**

- [ ] **The AST dump** (moved here from Step 0e). Specify the format (S-expressions recommended: `(+ 1 (* 2 3))`,
  `(return (int 2))`, `(func main int (params) (block ...))`): deterministic, stable, readable, and complete enough.
  Decide which fields are dumped: ranges or not (recommended: not in the unit-test dump, maybe behind a flag), and
  types or not (sema's dump will add them, see `sema-roadmap.md`). It's the test oracle from this step on.
- [ ] Test plumbing (Claude): a helper that parses a string, dumps the AST, and compares it to an expected string,
  printing both on a mismatch. Move slice 1's field-by-field tests to dump strings, keeping ranges in a few dedicated
  tests.
- [ ] Unary `- ~ !` and `+` (all of the prefix operators except `*` and `&`, which come in Step 11), and `( expr )`.
- [ ] AST: unary node.
- [ ] Tests: `-2`, `~-2`, `!!1`, `-(-(2))`, `(((2)))`.
- [ ] Error tests: `()`, `(2`, `-` followed by `;`.
- [ ] Error tests: `--2` and `++2` (touching same signs). `- -2`, `-(-2)` and `-+2` are fine.
- Gotchas: the `--` check needs ranges, not just token kinds, since the lexer drops whitespace. Parentheses produce no node, so the inner node's range is widened
  to include them (ADR-016, decided 2026-11-06).

**Step 3: Arithmetic**

- [ ] Binary `* / %` and `+ -`: precedence climbing is introduced here.
- [ ] AST: binary node.
- [ ] Tests: precedence (`1 + 2 * 3`), left associativity (`1 - 2 - 3` → `(- (- 1 2) 3)`, `8 / 4 / 2`), mixing with
  unary operators (`-1 * -2`, `-(1 + 2)`).
- [ ] Error tests: `1 +`, `* 2` (until Step 11), `1 2`.
- Gotcha: division by a constant zero isn't a parser error. The parser accepts `1 / 0`.

**Step 4: Bitwise and shift**

- [ ] `<< >>`, `&`, `^`, `|`: a few more rows in the precedence table.
- [ ] Tests: `1 << 2 + 1` → `(<< 1 (+ 2 1))` (shift binds *looser* than `+`); `1 | 2 ^ 3 & 4` → `(| 1 (^ 2 (& 3 4)))`.
- [ ] Error tests: `1 <<`, `1 & & 2` (until Step 11 makes unary `&` valid, and even then it's a sema error).
- Gotcha: C's precedence wart: `x & 1 == 0` parses as `x & (1 == 0)`. Test it in Step 5, once `==` exists, and get it
  right *even though it's surprising*.

**Step 5: Relational, equality, logical**

- [ ] `< > <= >=`, `== !=`, `&&`, `||`.
- [ ] Tests: `1 < 2 == 1` → `(== (< 1 2) 1)`; `a < b < c` → `(< (< a b) c)` is legal; `1 || 2 && 3` → `(|| 1 (&& 2 3))`;
  the `x & 1 == 0` wart.
- Gotcha: short-circuit evaluation of `&&` and `||` is codegen's concern; the parser just builds binary nodes. Consider
  a separate node kind (or flag) for them anyway, since codegen treats them so differently.

**Step 6: Local variables, assignment, expression statements**

- [ ] Declarations `int x;`, `int x = <expr>;` and multiple declarators `int a, b = 1;`, identifiers as
  primary expressions, `=` (right-associative, lowest precedence), expression statements, and the null statement `;`.
- [ ] AST: variable declaration, identifier, assignment, expression statement.
- [ ] Tests: `a = b = 1` → `(= a (= b 1))`; `x = 1 + 2`; `int a, b = 2, c;`; declaration vs. expression statement chosen by the first token
  (`int`/`char`/`void` → declaration).
- [ ] Error tests: `int ;`, `int 1x;`, `int x = ;`, `x = ;`, `int a, ;`, `int a b;`.
- Gotchas: parse the left side of `=` as an ordinary expression and leave the lvalue check (`1 = x`) to sema. Scope
  starts right after the declarator (`int x = x;` refers to the new `x`), but that's sema's business too.

**Step 7: Blocks and `if` / `else`**

- [ ] Nested compound statements, `if ( expr ) stmt`, `if ( expr ) stmt else stmt`.
- [ ] AST: `if` node with an optional `else` branch.
- [ ] Tests: dangling else: `if (a) if (b) x = 1; else x = 2;` binds the `else` to the inner `if`. Nested blocks, empty
  blocks `{}`, `if (x);`.
- [ ] Error tests: `if x) ...`, `else` without `if`, **`if (x) int y;`**.
- Gotcha: that last one is a real C rule: a declaration is a *block item*, not a *statement*, so it can't be the body of
  an `if`. The grammar needs both "statement" and "block item".

**Step 8: `while`**

- [ ] `while ( expr ) stmt`.
- [ ] Tests: `while (i < 10) i = i + 1;`, `while (x);`, nested loops, `while` inside `if`.
- [ ] Error tests: `while () ...`, `while (1)` at EOF.

**Step 9: Functions**

- [ ] General function definitions and prototypes, parameter lists (`(void)`, `()` meaning `(void)`, and parameters
  whose names are optional in prototypes), the `void` return type, `return;`, and calls `f(a, b)` as a postfix expression.
- [ ] AST: parameter list, call node.
- [ ] Tests: several functions in one file, prototype then definition, calls with zero, one and many arguments, a call
  inside an argument (`f(g(1), 2)`), `f(1)(2)` (grammatically valid, sema rejects it).
- [ ] Error tests: `int f(int a,) {}`, `f(1,)`, a definition inside a function (no nested functions in C).
- Gotchas: in a call, the `,` separates arguments; it's not the comma operator. A missing parameter name in a
  *definition* is easiest to report in sema (the parser doesn't know yet whether a `{` follows when it parses the
  parameters). `int f(), g;` is legal with multiple declarators.

**Step 10: Global variables**

- [ ] Top-level variable declarations next to functions.
- [ ] Tests: globals before and after functions, globals with initializers (`int g = -1;`, `char *s = "hi";`).
- [ ] Error tests: `int x` at EOF, `int = 1;`.
- Gotchas: at the top level, `int f` can start a variable, a prototype, or a definition, so parse the declarator first
  and then decide by the next token (`;`, `=`, `{`). `int x; int x;` is legal C (tentative definitions), and checking
  that is sema's job. The parser can accept any expression as a global's initializer and leave the "constant or string
  literal only" rule to sema, which gives a better diagnostic than a syntax error.

**Step 11: Pointers**

- [ ] Pointer declarators (`int *p`, `int **pp`), unary `*` and `&` in expressions.
- [ ] Tests: `*p = 3`, `&x`, `**pp`, `*p * *q` (the same token as unary and binary), `a & &b`.
- [ ] Error tests: `int * ;`, `&`, `*;`.
- Gotchas: unary vs. binary `*` and `&` are decided by *position* (start of an operand → unary). With no `typedef`, C's
  "is `a * b;` a declaration?" ambiguity can't happen.

**Step 12: Arrays**

- [ ] Array declarators with a size that is one integer-constant token (`int a[3]`, `int *a[3]`, `int a[2][3]`), and
  postfix indexing `a[i]`.
- [ ] Tests: `a[1] = 5`, `a[i][j]`, `*p[1]` → `(* ([] p 1))` (postfix binds tighter
  than unary), `int *a[3]` is an array of pointers.
- [ ] Error tests: `int a[];`, `int a[x];`, `int a[2 + 1];` (all syntax errors, since a size is one token), `int f(int
  a[]);`, `a[1`. `int a[0];` parses, and sema rejects it.
- Gotcha: `a[i]` is defined as `*(a + i)`, so `i[a]` is legal C. Decide whether the AST keeps an index node
  (recommended, for diagnostics) or lowers it to `*` and `+` right away.

**Step 13: `char`, character constants, string literals**

- [ ] `char` in declarations, string literals as primary expressions.
- [ ] Tests: `char c = 'a';` (the lexer already turns `'a'` into the `int` constant 97, which is correct C: character
  constants have type `int`), `char *s = "hi";`, `"hi"[1]`.
- [ ] Error tests: `"a" "b"` (adjacent string literal concatenation is out, per `TODO.md`).
- Gotcha: the dump should show string literals by their interned id or escaped contents, never by raw bytes (a `\0` or
  `\n` would break the one-line format).

**Step 14: `sizeof`**

- [ ] Prerequisite (lexer, Tomer's): `sizeof` keyword token.
- [ ] `sizeof` *unary-expression*, at the same level as the other prefix operators.
- [ ] AST: unary node (or a separate `sizeof` node: sema treats it very differently, since the operand isn't evaluated).
- [ ] Tests: `sizeof x`, `sizeof *p`, `sizeof a[0]`, `sizeof (x)`, `sizeof -x`, `sizeof x + 1` → `(+ (sizeof x) 1)`,
  `sizeof sizeof x`.
- [ ] Error tests: `sizeof(int)`, `sizeof(char *)` (v2), `sizeof;`.
- Gotcha: `sizeof (x)` is legal: `(x)` is a primary expression. Only a type keyword after the `(` makes it the v2 form.

---

## After the last slice

- [ ] Coverage run over the parser: every branch, including every error path, has run at least once.
- [ ] A larger golden-file program that uses every slice together.
- [ ] Milestone tag (`parser-v1`), with Tomer's approval.
