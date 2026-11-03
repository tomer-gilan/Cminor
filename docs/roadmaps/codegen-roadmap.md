# Code Generation Roadmap

The step-by-step plan for Cminor's code generation ("codegen"): the AST and sema's side table in, x86-64 assembly (AT&T
syntax, ADR-005) out, as a `.s` file that `gcc` assembles and links against libc (ADR-001). Codegen reads the AST
(ADR-023: it never changes it) and the types and symbols sema recorded, and decides everything the machine needs:
instructions, registers, stack slots, labels, sections, and the System V calling convention.

---

## How the roadmaps fit together

Same scheme as [`parser-roadmap.md`](parser-roadmap.md), "How the roadmaps fit together": one roadmap per stage, with
shared step numbers.

> **Step N of every stage roadmap, together, is slice N.**

Work slice by slice: parser Step N, then sema Step N, then codegen Step N, then N+1. A slice is done when Step N is done
in every roadmap and its end-to-end test passes. The end-to-end tests live here: codegen is the stage where a slice's
example program finally runs.

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
[`implementation-defined.md`](../specifications/implementation-defined.md). The second one is codegen's spec more than
any other stage's: type sizes, signed `char`, arithmetic `>>`, truncating division, left-to-right evaluation, falling off
`main`, and what each undefined behavior does in practice (the plain instruction, like `gcc -O0`).

### Where the work is

Unlike sema, codegen is busy from the start. The heavy steps:

- **Step 1:** the whole pipeline runs end to end for the first time: the driver writes a `.s` file, and the end-to-end
  runner assembles, links and runs it.
- **Step 3:** binary operators need a place for the left operand while the right one is computed (the stack machine).
- **Step 6:** the stack frame: a slot for every local.
- **Step 9:** the calling convention. The hardest step, and the one whose bugs crash far away from their cause.
- **Steps 11–13:** widths. Pointers are 8 bytes, `int` 4, `char` 1, and every load, store and conversion has to match.

Codegen has **no diagnostics of its own**: any program that reaches it was accepted by sema, so anything codegen can't
handle is a compiler bug, caught by an `assert` (ADR-011).

---

## Testing, from Step 1

A step isn't done until its tests are in place and pass, both in the normal build and in the sanitizer build (where
the compiler itself runs under ASan and UBSan).

1. **End-to-end tests** (the main oracle). One small Cminor program per case in `tests/e2e/`: compile it to `.s`, assemble
   and link it with `gcc`, run it, and compare its exit status (and, from Step 9, its standard output) with the
   expectation in the program's `.exit` file. Expected values are written by hand from the standard and
   `implementation-defined.md`, never copied from the program's output.
2. **Differential check** (optional, from Step 1). The runner also compiles each program with `gcc` and compares the
   two results. It's a cross-check of the hand-written expectation, not a replacement for it, and the one documented
   difference (`sizeof c - 2 < 0`, Step 14) is marked as expected to differ.
3. **Assembly golden files** (few). A handful of `.s` outputs in `tests/golden/codegen/`, through the golden-file
   runner (parser Step 0f), for things only the assembly shows: sections, `.globl`, the `.note.GNU-stack` line, label
   naming. Keep them few: every codegen refactor rewrites all of them.

There are no codegen error tests. The closest thing is in Step 1: a program that sema rejects produces no `.s` and a
non-zero exit status from the compiler.

---

## Step 0: Preparation

**Step 0a: Prerequisites**

- [x] Parser Step N and sema Step N are done before codegen Step N starts.
- [ ] Driver (Tomer's): the codegen slot (`Driver.c`, already there and empty) runs only when no stage reported an
  error, and gets the AST and `SemaResult`.
- [ ] Driver (Tomer's): the assembly goes to an output file. Decide the command line: a second argument, `-o out.s`, or
  the input's name with `.s` (`compilation_flags_from_arguments` is still a TODO).
- [ ] Driver (Tomer's): the compiler's exit status is non-zero when any error was reported. `main` returns 0 today, so
  a script can't tell a rejected program from a compiled one.
- [ ] Toolchain on the Linux host: `gcc` (with `as` and `ld`) and `gdb`. `objdump` comes with binutils.

**Step 0b: Study**

- [ ] For every slice, before its step: compile its example with
  `gcc -O0 -S -fno-asynchronous-unwind-tables -o - x.c` and read the output. (The flag drops the `.cfi_` directives,
  which are only noise at this point.) It's AT&T syntax, like Cminor's.
- [ ] Nora Sandler, *Writing a C Compiler* (No Starch, 2024): chapters 1–10 follow almost the same slices. It goes
  through an IR (TACKY); read it for the assembly, whatever Step 0c decides about an IR.
- [ ] Abdulaziz Ghuloum, "An Incremental Approach to Compiler Construction" (2006, 11 pages): where this slice-by-slice
  scheme comes from.
- [ ] chibicc's `codegen.c`, in its early commits: a stack-machine codegen in C.
- [ ] The System V AMD64 ABI (psABI), §3.2 "Function Calling Sequence": registers, the stack frame, alignment. Before
  Step 9 at the latest, with the ABI cheat sheet from `TODO.md`.
- [ ] Be able to answer:
    - What do `pushq %rbp` and `movq %rsp, %rbp` do at the start of a function, and what does `leave` replace?
    - Why does `movl $-1, %eax` set `%rax` to `0x00000000FFFFFFFF`, when `movb` and `movw` leave the upper bits alone?
    - In AT&T syntax, after `subl %ecx, %eax`, which register holds what? After `cmpl %ecx, %eax`, does `setl %al`
      mean `eax < ecx` or `ecx < eax`?
    - What does `idivl %ecx` divide, where do the quotient and the remainder go, and what has to happen to `%edx`
      first?
    - What must `%rsp` be, modulo 16, right before a `call`? What is it on entry to a function, and why?
    - Which registers may a function you call overwrite, and which must it give back unchanged?
    - Why does a program that runs `return 256;` from `main` exit with status 0, and `return -1;` with 255?

**Step 0c: Design (ADRs)**

Each of these gets an ADR in [`architecture-decisions.md`](../specifications/architecture-decisions.md):

- [ ] **AST straight to assembly, or an IR first.** Straight to assembly (recommended for v1, and what `overview.md`
  allows): one walk over the AST, reading sema's table. An IR (three-address code, like Sandler's TACKY) splits
  "what to compute" from "which instructions", which makes optimization and register allocation possible later, but
  every slice then lowers twice. ADR-005 already says "possibly with its own IR later".
- [ ] **Expression evaluation:** a stack machine (recommended): every expression leaves its value in `%rax` (or
  `%eax`), and a binary operator pushes its left operand while it computes the right one. The code is slow and
  obviously correct. The alternative, register allocation, is a project of its own.
- [ ] **Where the assembly text goes:** into an in-memory `Text` that the driver writes to the file (recommended: golden
  tests and debugging can read it without files), or straight into a `FILE*`.
- [ ] **Who runs `gcc`:** the compiler stops at `.s`, like `gcc -S`, and the end-to-end runner assembles, links and runs
  (recommended: the compiler stays free of process handling and POSIX). Or the compiler spawns `gcc` itself and
  produces an executable.
- [ ] **Stack frame:** every function gets a `%rbp`-based frame (as at `-O0`), and every local and parameter its own
  slot for the whole function, with no slot reuse between sibling blocks. The slot offsets live in a codegen-side table
  keyed by symbol, never in the AST or sema's table (ADR-023). Can wait until Step 6.
- [ ] **Labels:** internal labels start with `.L` (so they can't collide with a C identifier and stay out of the symbol
  table), numbered by one counter per translation unit.

**Step 0d: Test plumbing** (tooling, so Claude can help directly)

- [ ] The end-to-end runner: a script that, for each `tests/e2e/**/*.c`, runs the compiler, `gcc`, and the program (with
  a timeout), and compares the result with the expectation in a file next to it (`name.exit` holds the expected exit
  status, e.g. `7`; the expected standard output comes in Step 9). The expectation stays outside the program, so a
  test needs nothing the compiler doesn't support yet, such as comments. It reports a crash as a signal (SIGSEGV,
  SIGFPE), not as an exit status.
- [ ] Registered with CTest, one test per program, so a failure names its program. It runs in the sanitizer build too.
- [ ] The optional differential mode (Testing, 2.).
- [ ] Once the golden-file runner exists (parser Step 0f): `.s` golden files.

---

## Steps 1–14: one per slice

Each step lists what codegen emits, the end-to-end tests, and the gotchas.

**Step 1: `return` of a constant**

- [ ] The codegen pass itself: walk the translation unit and emit every function definition: `.text`, `.globl f`,
  `f:`, and the body.
- [ ] `return <constant>`: `movl $2, %eax`, then `ret`.
- [ ] The driver writes the output file, and exits non-zero on errors (Step 0a).
- [ ] Decide whether the prologue and epilogue (`pushq %rbp` … `popq %rbp`) start now or in Step 6. Now costs two lines
  and means Step 6 adds `subq` instead of restructuring.
- [ ] Tests: the slice 1 program exits with 2; `return 0;`; `return 255;`; a file with a second function that `main`
  never calls (both are emitted, and it still links).
- [ ] Test: a program sema rejects (a function defined twice) makes the compiler exit non-zero and write no `.s`.
- Gotchas: newer `ld` warns "missing .note.GNU-stack section implies executable stack" unless the file ends with
  `.section .note.GNU-stack,"",@progbits`, as gcc's output does. An exit status is only 8 bits, so expected values
  stay in 0–255, or are written mod 256. A translation unit without `main` compiles but doesn't link: every
  end-to-end test needs a `main`.

**Step 2: Unary operators and parentheses**

- [ ] `-x`: `negl %eax`; `~x`: `notl %eax`; `+x`: nothing; `!x`: compare with 0, `sete %al`, then zero-extend `%al`
  into `%eax`. Parentheses produce no node, so no code.
- [ ] Tests: `return -(~2);` (3); `return !0;` (1); `return !5;` (0); `return -2;` (254); `return ~0;` (255);
  `return -(-(2));` (2).
- Gotcha: `sete` writes only `%al`. Without `movzbl %al, %eax` the other 24 bits are whatever was there before, and `!5`
  can "return" anything.

**Step 3: Arithmetic**

- [ ] The stack machine: compute the left operand, `pushq %rax`, compute the right one, move it to `%ecx`, `popq %rax`,
  then `%eax op %ecx`. Left before right, always: `implementation-defined.md` promises left-to-right, and it becomes
  observable once calls have side effects (Step 9).
- [ ] `+ - *`: `addl`, `subl`, `imull`. `/` and `%`: `cltd` (sign-extend `%eax` into `%edx`), then `idivl %ecx`; the
  quotient is in `%eax`, the remainder in `%edx`.
- [ ] Tests: `1 + 2 * 3` (7); `10 - 3 - 2` (5); `7 / 2` (3); `-7 / 2` (−3, so 253); `-7 % 2` (−1, so 255);
  `7 % -2` (1). No test for `2147483647 + 1`: the wrap is UB.
- Gotchas: `subl %ecx, %eax` is `eax = eax - ecx` (source first in AT&T), so `10 - 3` comes out as −7 if the operands
  are swapped. Zeroing `%edx` instead of `cltd` breaks every negative division. `pushl` doesn't exist in 64-bit mode:
  push and pop are always 8 bytes. Division by zero dies with SIGFPE, and so does `(-2147483647 - 1) / -1`.

**Step 4: Bitwise and shift**

- [ ] `& ^ |`: `andl`, `xorl`, `orl`. `<<`: `sall %cl, %eax`; `>>`: `sarl %cl, %eax` (arithmetic,
  `implementation-defined.md`).
- [ ] Tests: `1 << 2 + 1` (8); `-8 >> 1` (−4, so 252); `5 & 3` (1); `5 | 3` (7); `5 ^ 3` (6); `-1 >> 31` (−1, so 255).
- Gotchas: a variable shift count can only come from `%cl`, so the right operand has to end up in `%ecx`. `shrl` is the
  *logical* shift: it compiles, runs, and gives the wrong answer for negative numbers only.

**Step 5: Relational, equality, logical**

- [ ] `< > <= >= == !=`: `cmpl %ecx, %eax`, then `setl`/`setg`/`setle`/`setge`/`sete`/`setne`, then `movzbl`.
- [ ] `&&` and `||`: short-circuit with jumps, so this is the first step with labels (Step 0c). The result is always 0
  or 1.
- [ ] Tests: `1 < 2 && 3 != 4` (1); `2 < 1` (0); `-1 < 0` (1); `5 && 7` (1, not 7); `0 || 0` (0);
  `0 && 1 / 0` (0, and no SIGFPE: this is the short-circuit test); `1 || 1 / 0` (1); `1 < 2 == 1` (1).
- Gotchas: `setl`/`setg` are the *signed* conditions; `setb`/`seta` are unsigned, and only differ on negative numbers,
  so `-1 < 0` is the test that catches it. Labels must be unique in the whole file, not just in one function.

**Step 6: Local variables, assignment, expression statements**

- [ ] The frame: give every local a slot at a negative offset from `%rbp`, and reserve the total with `subq $N, %rsp`
  in the prologue (Step 0c).
- [ ] Two ways to compute an expression: its *value* (into `%eax`), and, for an lvalue, its *address* (into `%rax`,
  with `leaq`). An identifier's value is a load from its slot; `=` computes the left side's address, pushes it,
  computes the right side's value, pops the address and stores. The value of `x = 3` is 3.
- [ ] Declarations with an initializer store it; expression statements compute and discard; `;` emits nothing.
- [ ] `return` in the middle of a function jumps to one epilogue. Falling off the end of `main` returns 0
  (`implementation-defined.md`): emit `movl $0, %eax` before `main`'s epilogue.
- [ ] Tests: `int x = 2; x = x * 3; return x;` (6); `int a, b; a = b = 3; return a + b;` (6); `int x; x = 5; return
  (x) = x + 1;` (6); `int main(void) { int x = 3; }` (0); `1 + 2; ; return 4;` (4).
- Gotchas: key the slot table by *symbol*, not by name: two `x`s in different blocks (Step 7) are different variables.
  Keep `N` a multiple of 16, or every call in Step 9 starts misaligned. Reading an uninitialized local is UB, so no test
  may depend on its value (`int x = x;` included).

**Step 7: Blocks and `if` / `else`**

- [ ] `if`: compute the condition, `cmpl $0, %eax`, `je` to the `else` label (or the end); `else` jumps over itself.
- [ ] Blocks need no code: their locals already have slots in the function's frame (Step 0c).
- [ ] Tests: `if (1) return 1; return 2;` (1); `if (0) return 1; else return 3;` (3); the dangling else
  (`if (1) if (0) x = 1; else x = 2;` gives 2); `int x = 1; { int x = 2; } return x;` (1); a block that assigns to an
  outer variable.
- Gotcha: the `then` branch must jump over the `else` branch; forgetting the `jmp` runs both, and the test only notices
  if the `else` branch changes something.

**Step 8: `while`**

- [ ] `while`: a label at the condition, `je` to the end label, the body, `jmp` back to the condition.
- [ ] Tests: `while (i < 10) i = i + 1;` (10); a sum of 1..10 (55); a loop that runs zero times; nested loops; a
  `return` inside the loop.
- Gotcha: a wrong jump makes an infinite loop, and without the runner's timeout (Step 0d) the test suite hangs instead
  of failing.

**Step 9: Functions**

- [ ] Calls: compute the arguments left to right, pushing each one, then pop them into `%edi`, `%esi`, `%edx`, `%ecx`,
  `%r8d`, `%r9d`, and `call`. Decide whether more than six arguments are supported (they go on the stack) or rejected
  by sema, and document it.
- [ ] Definitions: the prologue stores each parameter register into the parameter's slot. The return value is in
  `%eax`; a `void` function's `return;` just jumps to the epilogue.
- [ ] Stack alignment: `%rsp` must be a multiple of 16 at every `call`. With pushes inside expressions, that means
  tracking the push depth while generating, and padding when it's odd.
- [ ] libc: `call putchar@PLT`, as gcc writes it.
- [ ] Tests (the runner now also checks standard output): `add(1, 2)` (3); recursive factorial of 5 (120); recursive
  fib(10) (55); a prototype before `main` and the definition after it; `f(g(1), 2)`; six arguments; `putchar(72);
  putchar(105); putchar(10);` prints `Hi`; argument order made visible with `putchar` calls inside the arguments.
- Gotchas: `f(g(1), 2)` breaks if arguments go straight into their registers: the call to `g` overwrites `%edi`.
  Misalignment usually doesn't crash in your code but inside libc (an SSE instruction such as `movaps`), far from the
  bug. A callee may overwrite `%rax`, `%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8`–`%r11`; if codegen ever keeps something in
  `%rbx` or `%r12`–`%r15`, it must save and restore them. Writing an `int` argument with a 32-bit `mov` zero-extends
  it, which is what the `malloc(int)` row of `implementation-defined.md` relies on.

**Step 10: Global variables**

- [ ] Each global: `.globl x`, then in `.data` (with an initializer: `.long 5`) or `.bss` (without one: `.zero 4`),
  aligned to its size.
- [ ] Access: `x(%rip)`, RIP-relative. Codegen tells a global from a local by the symbol sema recorded.
- [ ] Tests: `int counter; ... counter = counter + 1; return counter;` (1); `int g = -1; ... return g;` (255); a global
  set in one function and read in another; a local that shadows a global.
- Gotcha: `movl x, %eax` (absolute addressing) assembles fine and fails at *link* time: gcc links a position-independent
  executable by default, and `ld` asks you to "recompile with -fPIE". Use `x(%rip)`.

**Step 11: Pointers**

- [ ] `&x`: the address computation from Step 6 (`leaq -4(%rbp), %rax`). `*p`: compute `p`'s value, then load through
  it; as an lvalue, `*p`'s address is `p`'s value.
- [ ] Widths come from the recorded types (ADR-024): a pointer is loaded, stored, pushed and compared as 8 bytes
  (`movq`, `cmpq`), an `int` as 4.
- [ ] Pointer ± integer: sign-extend the integer to 64 bits (`movslq` or `cltq`), multiply it by the pointee's size,
  then `addq`/`subq`.
- [ ] Tests: `int x; int *p = &x; *p = 3; return x;` (3); `**pp`; a `swap(int *a, int *b)`; `p == &x` (1); `p = 0;
  return !p;` (1); `void *v = &x; int *q = v; return *q;`.
- Gotchas: a pointer loaded with `movl` loses its upper 32 bits and segfaults later, at the dereference. A negative
  index that isn't sign-extended adds about 4 billion. Pointer comparisons: gcc uses the unsigned conditions (`setb`);
  for valid pointers the two agree, but match gcc.

**Step 12: Arrays**

- [ ] Slots and globals of N × element size, aligned to the element.
- [ ] Decay (ADR-024): an identifier recorded as a pointer whose declaration is an array means "emit its address", not
  a load. Never load an array-typed value: there is no such thing in a register.
- [ ] `a[i]` is `*(a + i)`: the Step 11 addition, then a load (or, as an lvalue, the address). When the element is
  itself an array (`a[i][j]`), `a[i]` is an address and isn't loaded.
- [ ] Tests: fill an array in a loop and sum it; `a[1][2]` in an `int a[2][3]`; `i[a]`; an array passed to a function
  as a pointer; a global array.
- Gotcha: the multi-dimensional case only works if every step uses the *recorded* type: the scale of `a[i]` in
  `int a[2][3]` is 12, not 4.

**Step 13: `char`, character constants, string literals**

- [ ] `char` loads sign-extend (`movsbl`, signed `char`); stores write one byte (`movb %al, ...`).
- [ ] String literals: in `.section .rodata`, under `.L` labels, one copy per distinct literal
  (`implementation-defined.md`). Emit their bytes so that `\0`, `"` and `\n` inside survive (`.byte` lists are the
  safest); a global `char *s = "hi";` is `.quad` of the label.
- [ ] Tests: `char c = 'a'; return c + 1;` (98); `char c = 200; return c < 0;` (1); `char c = 300; return c;` (44);
  `"hi"[1]` (105); a `putchar` loop over a string; a global `char *s = "hi";`; a `char` parameter and a `char` return
  value.
- Gotchas: `movzbl` instead of `movsbl` makes `char` unsigned, and only bytes ≥ 0x80 show it. `.string` adds its own
  terminating `\0`, so don't add a second one. Sign-extend a `char` to 32 bits when passing it as an argument:
  clang-compiled code (some libcs) assumes the caller did. Writing into a literal is a SIGSEGV (`.rodata`); don't test
  it, or test it as an expected crash.

**Step 14: `sizeof`**

- [ ] Emit the constant sema computed (`movl $12, %eax`), and **no code for the operand**: it's never evaluated.
- [ ] Tests: `int a[3]; return sizeof a;` (12); `sizeof c` (1); `sizeof p` (8); `sizeof "hi"` (3); `sizeof f()` with
  an `f` that calls `putchar`, and standard output stays empty; `char c; return sizeof c - 2 < 0;` (1).
- Gotcha: that last test is the documented difference from C (`implementation-defined.md`, "Operators"): gcc gives 0.
  Mark it as expected to differ in the differential check.

---

## After the last slice

- [ ] Coverage run over codegen: every branch has run at least once.
- [ ] A larger end-to-end program that uses every slice together (a sieve of primes, a string reversal, a recursive
  algorithm over an array).
- [ ] Milestone tag (`codegen-v1`, and with it Cminor v1), with Tomer's approval.
