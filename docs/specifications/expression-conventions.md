# Expression Conventions

The contract between `codegen_generate_expression` (`Codegen.c`) and the code that calls it. A calling convention says
what holds across a call; these conventions say what holds across evaluating one expression. Every expression's
generated code follows them, and every caller (a statement, or a parent expression evaluating its operands) relies on
nothing beyond them.

Binding: code that disagrees with them is a bug (or this doc has to change first). Decided 2026-10-20.

---

## 1. Result

The value ends up in `%rax`. `codegen_generate_expression` returns nothing: the expression's type in sema's table
(ADR-024) says how much of `%rax` holds the value: `WIDTH_LONG` for `%eax` (an `int`), `WIDTH_QUAD` for all of `%rax`
(a pointer).

## 2. Preserved registers: `%rbp` only

Every register except `%rbp` is caller-saved. An expression gives no guarantee about any other register's value after
it runs, so a caller that needs a value across evaluating a subexpression keeps it on the stack, not in a register.
`%rbp` is the frame pointer: every local is addressed from it (`-N(%rbp)`), so an expression never writes it. (`%rsp`
is covered by rule 4.) This is the stack
machine (`codegen-roadmap.md`, Steps 0c and 3): a binary operator pushes its left operand while it computes the right
one.

## 3. Clobbered

An expression may overwrite any general-purpose register except `%rbp`, and the flags (`RFLAGS`). A caller can't rely on a condition
code set before the expression ran.

## 4. Stack balance

`%rsp` on exit equals `%rsp` on entry. Every `push` in an expression's code has a matching `pop` on every path through
it.

- Why: the caller finds its own pushed values with `pop`, so an unbalanced child makes the parent pop the wrong value.
  From slice 9 on, the 16-byte alignment before a `call` is computed assuming balance.
- Gotcha: an imbalance usually doesn't crash where it happens. It shows up as a wrong value in an enclosing operator, or
  as a crash inside a callee once calls exist.
