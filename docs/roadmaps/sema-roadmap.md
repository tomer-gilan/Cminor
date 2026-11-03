# Semantic Analysis Roadmap

The step-by-step plan for Cminor's semantic analysis ("sema"): the parser's AST in; a verdict and a side table out, with
the AST itself untouched (ADR-023). Sema enforces the rules the grammar can't express: every name is declared, types
fit together, the left side of `=` is something that can be assigned to. It also records what later stages need:
which declaration each name refers to, and the type of every expression.

---

## How the roadmaps fit together

Same scheme as [`parser-roadmap.md`](parser-roadmap.md), "How the roadmaps fit together": one roadmap per stage, with
shared step numbers.

> **Step N of every stage roadmap, together, is slice N.**

Work slice by slice: parser Step N, then sema Step N, then codegen Step N, then N+1. A slice is done when Step N is done
in every roadmap and its end-to-end test passes.

### The slices

Copied from `parser-roadmap.md`. The list must stay identical across the stage roadmaps, so change all of them together.

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

Everything comes from [`SUBSET.md`](../specifications/SUBSET.md) and
[`implementation-defined.md`](../specifications/implementation-defined.md). The rules sema enforces are mostly C11's
**constraints** (the "Constraints" paragraphs of §6.5–§6.9): a constraint violation needs a diagnostic, which is
exactly sema's job. Cminor may reject more than C does, but it never changes what an accepted program means
(`implementation-defined.md`, "Functions").

### Light slices

Slices 1–5 only have `int` values, so there's almost nothing to check: sema records types and passes. The real work
starts in slice 6 (names and scopes) and slice 9 (functions), and the type rules get dense in slices 11–13 (pointers,
arrays, `char`). An error test only goes in once its expectation holds for the whole subset: `-p` on a pointer is
tested in Step 11, not in Step 2.

---

## Testing, from Step 1

The same three layers as the parser. A step isn't done until all of them are in place and pass, both in the normal
build and in the sanitizer build.

1. **Unit tests.** `test_sema` parses and checks a small source string, then compares a typed dump to an expected
   string: the parser's AST dump (parser Step 2) with each expression's type from sema's table (ADR-023), e.g.
   `(+:int 1:int (*:int 2:int 3:int))`. Until the dump exists (Step 1), tests read sema's tables directly. Expected values are written
   by hand from the standard and `SUBSET.md`, never copied from sema's output.
2. **Error tests.** Each step lists programs that parse but must be rejected. Check the diagnostic's code and position,
   and that it's the only one (ADR-025).
3. **Golden files.** One or two realistic programs per step in `tests/golden/sema/`, through the golden-file runner
   with the typed dump, once the runner exists (parser Step 0f).

---

## Step 0: Preparation

**Step 0a: Prerequisites**

- [x] Parser Step N is done before sema Step N starts.
- [x] Parser change (Tomer's), needed by Step 1: every expression node gets an `int id`, numbered 0 to n−1 by a counter
  in the parser, and n is available to sema (ADR-023).
- [x] The driver calls sema after a parse that reported no errors (`Driver.c` already has the empty slot), and stops
  before codegen if sema reported any (ADR-012).
- Sema diagnostic codes are added as each step needs them, starting in Step 1 (no upfront range), as in the parser.

**Step 0b: Study**

- [ ] *Crafting Interpreters*, ch. 11 ("Resolving and Binding"): a separate pass that resolves every name, with a
  stack of scopes.
- [ ] The parts of C11 (N1570) sema implements, read as needed per step rather than up front: §6.2.1 (scopes), §6.2.5
  (types), §6.3.1.1 and §6.3.1.8 (integer promotions, usual arithmetic conversions), §6.3.2.1 (lvalues, and arrays
  decaying to pointers), and the "Constraints" paragraphs of §6.5 (operators), §6.7 (declarations), §6.8.6.4
  (`return`) and §6.9 (external definitions).
- [x] Be able to answer:
    - What is an lvalue? Why is `x = 1` fine and `x + 1 = 1` not, when both left sides have type `int`?
    - When does an array become a pointer to its first element, and in which two places doesn't it (in Cminor)?
    - In `int x = x;`, which `x` is on the right? Why?
    - Why does `char c; c + c` have type `int`?
    - What's the difference between a *declaration* and a *definition* of a function? Which one can appear twice?

**Step 0c: Design (ADRs)**

Each of these gets an ADR in [`architecture-decisions.md`](../specifications/architecture-decisions.md):

Decided 2026-10-16:

- [x] **A separate pass:** sema walks the finished AST, as in the pipeline of `overview.md`, rather than checking
  during parsing as chibicc does. *(ADR-021)*
- [x] **Symbol table:** a stack of scopes (file scope at the bottom, one per block on top), each mapping an interned
  identifier id (ADR-018) to a symbol: what it is, its `Type`, and its declaration node. Lookup goes innermost-out.
  *(ADR-022)*
- [x] **Sema never modifies the AST.** Its results are a side table indexed by a parser-assigned expression id: one
  `ExpressionSemantics` per expression, holding its `Type` and, for an identifier, its symbol. Lvalue-ness is a
  function (`sema_is_lvalue`), not stored. *(ADR-023)*
- [x] **Implicit conversions:** sema records each expression's type *after* array decay, so decay is decided only in
  sema. Codegen derives pointer scaling and `char` ↔ `int` widths from the recorded types. No conversion nodes.
  *(ADR-024)*
- [x] **Error policy:** stop at the first error, like the parser. *(ADR-025)*

**Step 0d: Test plumbing** (tooling, so Claude can help directly)

- [ ] `test_sema` executable registered with CTest.
- [ ] A helper that parses and checks a string and expects no diagnostics, and a helper for error tests: a given code,
  at a given offset, and the count the error policy implies.
- [ ] Once the parser's dump exists (parser Step 2): a typed dump and a dump-compare helper.

---

## Steps 1–14: one per slice

Each step lists what sema checks, what it records, the tests, and the gotchas.

**Step 1: `return` of a constant**

- [ ] The sema pass itself: walk the translation unit, each function definition and its body.
- [ ] Symbol table: file scope only. Each function definition declares its name there.
- [ ] Record: an integer constant has type `int`.
- [ ] Error tests: `int f(void) { return 1; } int f(void) { return 2; }`: a function defined twice (the parser test
  `same_name_twice` deliberately leaves this to sema). Report it at the second definition's name.
- [ ] Tests: the slice 1 program passes with no diagnostics; two functions with different names pass.
- Gotchas: a translation unit without `main` is *not* a compile error: it's the linker's (and C allows it, e.g. for a
  library). A "note: previous definition here" after the redefinition error is nice, but it's a second diagnostic, so
  decide how it fits the error policy and the count check.

**Step 2: Unary operators and parentheses**

- [ ] Record: `-x`, `+x`, `~x` and `!x` have type `int` (after integer promotion of the operand; `!` always gives
  `int`, 0 or 1).
- [ ] Tests: the types of `-2`, `~-2`, `!!1`, `-(-(2))`.
- No error tests yet: every operand is an `int`. Operand errors (`-p`, `~p` on a pointer) arrive in Step 11.

**Step 3: Arithmetic**

- [ ] Record: `* / % + -` on two integers have type `int` (the usual arithmetic conversions, which in Cminor always end
  at `int`).
- [ ] Tests: the types of `1 + 2 * 3` and `-1 * -2`.
- Gotchas: `1 / 0` is accepted: division by zero is undefined at run time, not a constraint violation. A warning for a
  constant zero divisor is optional; if you add one, remember that warnings are diagnostics too, and count in the
  tests.

**Step 4: Bitwise and shift**

- [ ] Record: `& ^ |` and `<< >>` have type `int`.
- Gotchas: shifts don't use the usual arithmetic conversions: each operand is promoted on its own, and the result has
  the type of the promoted *left* operand. That's invisible while everything is `int`, but write it the right way now.
  A constant shift count ≥ 32 or < 0 is undefined behavior (`implementation-defined.md`), a candidate for an optional
  warning.

**Step 5: Relational, equality, logical**

- [ ] Record: `< > <= >= == != && ||` all have type `int` (0 or 1), whatever the operand types.
- Gotchas: pointer comparisons arrive in Step 11. `&&` and `||` short-circuit, which is codegen's concern; sema only
  types them.

**Step 6: Local variables, assignment, expression statements**

- [ ] Block scopes: each compound statement opens a scope and closes it at its `}`. A declaration adds a symbol to the
  current scope; an identifier in an expression is looked up from the innermost scope outwards.
- [ ] Record: each identifier's symbol, and its type; an identifier that names a variable is an lvalue. `=` has the
  type of its left operand and is not an lvalue.
- [ ] Error tests: an undeclared identifier (`return x;`); the same name declared twice in one scope (`int x; int
  x;` inside a block); `1 = 2`; `x + 1 = 2`; `void x;` (`void` only as a return type, `(void)` or `void *`).
- [ ] Tests: shadowing in an inner block is legal (`int x = 1; { int x = 2; }`); `int a, b = a;` (`a` is in scope
  for `b`'s initializer); `int x = x;` refers to the new `x`; `(x) = 1` is legal (a parenthesized lvalue is still
  one).
- Gotchas: a scope starts right after the declarator, before its initializer, so `int x = x;` reads the uninitialized
  new `x` (undefined at run time, but legal; maybe a warning). Where each local lives on the stack is codegen's
  decision, not sema's.

**Step 7: Blocks and `if` / `else`**

- [ ] The condition must have scalar type (an integer or, from Step 11, a pointer).
- [ ] Tests: nested blocks with shadowing at several depths; a name declared in an `if`'s block isn't visible after it.
- [ ] Error tests: using a variable after the block that declared it has closed.
- Gotcha: `if (x) int y;` never reaches sema: it's a syntax error (parser Step 7).

**Step 8: `while`**

- [ ] The condition must have scalar type, as for `if`.
- [ ] Tests: a loop that declares a variable in its body block; nested loops with shadowing.

**Step 9: Functions**

- [ ] File scope holds every function, declared by a prototype or a definition, from its declarator on (so recursion
  works). Parameters go in the scope of the function's body block.
- [ ] Redeclaration: a prototype and a later definition must agree on the return type and the parameter types. Two
  definitions of one function are an error (Step 1).
- [ ] Calls: the callee must be a declared function. The argument count must match the parameter count exactly
  (`()` means `(void)`, so `int f(); f(1)` is an error, `implementation-defined.md`), and each argument must be
  assignable to its parameter's type.
- [ ] `return`: `return expr;` in a `void` function and `return;` in a non-`void` function are both errors (§6.8.6.4).
  A call to a `void` function has no value, so `int x = f();` is an error.
- [ ] `main`: only `int main(void)`, `int main()` and `int main(int argc, char **argv)` are accepted
  (`implementation-defined.md`).
- [ ] Error tests: calling an undeclared function (C11 has no implicit declarations); wrong argument count;
  conflicting prototypes (`int f(int); char f(int);`); an unnamed parameter in a definition (the parser accepts it,
  parser Step 9); a parameter and a top-level local with the same name (`int f(int a) { int a; }`); `f(1)(2)`; calling
  a variable (`int x; x(1);`).
- [ ] Tests: prototype then definition; definition then call; recursion; a call to a libc prototype
  (`int putchar(int c);`).
- Gotchas: parameters and the body's outermost block share **one** scope, so `int a;` at the top of the body
  redeclares `a`, while `{ int a; }` one block deeper shadows it. Falling off the end of a non-`void` function isn't
  an error (falling off `main` returns 0, codegen's job); a warning is optional.

**Step 10: Global variables**

- [ ] Globals live in file scope, next to functions: a variable and a function can't share a name.
- [ ] Initializers: only an integer or character constant, optionally with one leading `-`, or a string literal for a
  `char *` (`SUBSET.md` A.2.2). The parser accepts any expression there (parser Step 10), so sema gives the error.
- [ ] Error tests: `int g = 1 + 2;`, `int g = x;`, `int *gp = &x;`, `int g = "hi";`, `int x = 1; int x = 2;`,
  `int x; char x;`.
- [ ] Tests: a global used and assigned inside functions; a local that shadows a global.
- Gotchas: `int x; int x;` at file scope is legal C (tentative definitions). Decide whether Cminor accepts it as C does,
  or rejects it: rejecting is allowed, since Cminor may reject more than C, but never change a program's meaning.

**Step 11: Pointers**

- [ ] Types: pointer-to `T` (ADR-019). `&x` needs an lvalue operand and gives a pointer; `*p` needs a pointer operand,
  not `void *`, and gives an lvalue.
- [ ] Arithmetic: pointer ± integer and integer + pointer give the pointer type, scaled by the pointee's size
  (codegen derives the scale from the recorded type, ADR-024). Pointer − pointer is an error
  (`implementation-defined.md`), as is any other arithmetic on a pointer, and any arithmetic on `void *`.
- [ ] Comparisons and assignment: two pointers of the same type, or one of them `void *` (`SUBSET.md`), or a pointer and
  the **null pointer constant**, an integer constant `0`.
- [ ] Error tests: `&1`, `&(x + 1)`, `*x` on an `int`, `*v` on a `void *`, `p - q`, `p * 2`, `-p`, `~p`, `p = 1`,
  `int *p; char *q = p;`.
- [ ] Tests: `int *p = &x; *p = 3;`, `**pp`, `p = 0`, `if (p)`, `!p`, `void *v = p; p = v;`.
- Gotcha: `p = 0` is legal but `p = 1` isn't. Only an integer constant *expression* with value 0 is a null pointer
  constant, and in Cminor that means the token `0` (maybe with parentheses around it), since there's no constant folding.

**Step 12: Arrays**

- [ ] Types: array-of `T` with a size. A size of 0 is an error (parser Step 12 accepts `int a[0]`).
- [ ] Decay: an array in an expression becomes a pointer to its first element, **except** as the operand of `&` or
  `sizeof` (§6.3.2.1). Sema records the type after decay in its type table (ADR-024), so each use of `a` gets the
  right one. An array is an lvalue but not a *modifiable* one, so `a = b` is an error.
- [ ] Indexing: `a[i]` follows `*(a + i)`: after decay, one operand must be a pointer and the other an integer, so
  `i[a]` is legal. The result is an lvalue.
- [ ] Error tests: `int a[0];`, `a = b` with two arrays, `a[p]` with two pointers, `int a[3] = 1;` (no brace
  initializers in v1), `x[1]` on an `int`.
- [ ] Tests: `a[1] = 5`, `i[a]`, `a[i][j]` (the element is itself an array, which decays again), `int *p = a;`.
- Gotcha: `&a` has type pointer-to-array-of-3-`int`, which Cminor can't even spell (no parenthesized declarators), so
  `int *p = &a;` is an incompatible pointer assignment. It's an error, even though the address is the same.

**Step 13: `char`, character constants, string literals**

- [ ] Types: `char` (signed, 1 byte). In arithmetic it's promoted to `int`; assigning an `int` to a `char` converts
  (truncates).
- [ ] A string literal has type array-of-`char`, with room for the terminating `\0` (`"hi"` is `char[3]`), and decays
  like any array.
- [ ] Warning: a constant that doesn't fit in `char`, e.g. `char c = 300;` (`implementation-defined.md`,
  "Conversions").
- [ ] Error tests: `char *s = 'a';` (a character constant is an `int`), `int *p = "hi";`.
- [ ] Tests: `char c = 'a'; return c + 1;` (type `int`), `char *s = "hi"; return s[1];`, `"hi"[1]`, a global
  `char *s = "hi";`.
- Gotcha: writing through a pointer into a string literal is accepted (it's undefined behavior at run time, and a
  SIGSEGV in practice, `implementation-defined.md`). Without `const`, sema can't tell.

**Step 14: `sizeof`**

- [ ] Prerequisite: parser Step 14.
- [ ] `sizeof expr` has type `int` (`implementation-defined.md`). Sema computes its value from the operand's type
  (`char` 1, `int` 4, any pointer 8, an array N × its element), so codegen just emits a constant.
- [ ] The operand is **not evaluated** and **doesn't decay**: `int a[3]; sizeof a` is 12, `sizeof "hi"` is 3.
- [ ] Error tests: `sizeof f` (a function), `sizeof f()` where `f` returns `void`.
- [ ] Tests: `sizeof x`, `sizeof *p`, `sizeof a`, `sizeof a[0]`, `sizeof (x)`, `sizeof sizeof x` (4), `sizeof c` with
  `char c` (1), `sizeof f()` with an `int`-returning `f` (4, and `f` is never called: check that in the end-to-end
  test).
- Gotcha: `sizeof c - 2 < 0` is true in Cminor and false in C (`implementation-defined.md`, "Operators"). It's the one
  documented difference, so test it explicitly.

---

## After the last slice

- [ ] Coverage run over sema: every branch, including every error path, has run at least once.
- [ ] A larger golden-file program that uses every slice together.
- [ ] Milestone tag (`sema-v1`), with Tomer's approval.
