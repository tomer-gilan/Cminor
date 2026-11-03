# Compiler & C Terminology Dictionary

A quick-lookup glossary of terms you'll run into while building this compiler, grouped by **pipeline stage** (in
pipeline order, as in `overview.md` §1), and within a stage from the broad terms to the narrow ones. Each heading keeps
its `[stage]` tag (some terms span two stages) and a `(shape)` hint so you can recognize the term at a glance; the
paragraph below is there if you need the full explanation.

For the deep-dive on C's specific grammar (Annex A), see [annex-a-breakdown.md](annex-a-breakdown.md) — this file is the fast lookup; that one is the walkthrough. For the standard's minimum-guaranteed size/nesting limits (identifier length, string literal length, nesting depth, etc.), see [translation-limits.md](../educational/translation-limits.md).

**Stages:** [Across the pipeline](#across-the-pipeline) · [Preprocessing](#preprocessing) · [Lexing](#lexing) · [Parsing: techniques](#parsing-techniques) · [Parsing: expressions](#parsing-expressions) · [Parsing: declarations](#parsing-declarations) · [Parsing: statements](#parsing-statements) · [Semantic analysis](#semantic-analysis) · [IR generation](#ir-generation) · [Code generation](#code-generation) · [Assembling and linking](#assembling-and-linking) · [Quick precedence cheat-sheet](#quick-precedence-cheat-sheet)

---

## Across the pipeline

Terms that aren't tied to one stage.

### Frontend `[general]` (lexer + parser + semantic analysis)
The part of the compiler that deals with the source language: lexing, parsing, semantic analysis. Contrast with **backend**, which deals with the target machine.
Example: your lexer and parser are frontend; your x86-64 code emitter is backend. An IR sits at the boundary.

### Grammar `[general]` (`expression-statement: expression? ;`)
The formal set of rules defining what strings of tokens count as valid programs, and how they're structured. C's is defined in Annex A of the standard, written in BNF.
Example: `expression-statement: expression? ;` says an expression-statement is an optional expression followed by a semicolon.

### Backus-Naur Form (BNF) `[grammar]` (`rule ::= alt1 | alt2`)
The notation the C standard uses to define its grammar — rules of the form `symbol ::= alternative1 | alternative2`. The standard actually uses a close variant of this.
Example: `constant ::= integer-constant | floating-constant | character-constant`.

### Implementation-defined behavior `[semantic]` (is `char` signed or unsigned?)
Behavior the standard leaves to the compiler to decide, but requires it to be *consistent* and *documented* — unlike undefined behavior, it can't do "anything."
Example: whether plain `char` is signed or unsigned. Your compiler picks one and sticks with it.

### Unspecified behavior `[semantic]` (`f(a(), b())` — which runs first?)
Behavior that's valid and the *result* is one of a defined set of possibilities, but the standard doesn't say which — unlike UB, it's not "anything goes," just "not guaranteed."
Example: the order in which function arguments are evaluated — `f(a(), b())` may call `a()` or `b()` first; a real compiler picks one consistently, but the standard doesn't force it.

### Undefined behavior (UB) `[semantic]` (`INT_MAX + 1`, null deref)
Behavior the standard places **no requirements** on whatsoever — a conforming compiler can do literally anything (crash, produce garbage, "appear to work," format your disk in theory). You don't have to handle UB correctly, but you need to know where it lurks so unexpected input doesn't surprise you.
Example: signed integer overflow (`INT_MAX + 1`), dereferencing a null pointer, reading an uninitialized variable, out-of-bounds array access.

---

## Preprocessing

Not in Cminor: there is no preprocessor.

### Preprocessor `[preprocessor]` (`#define SQUARE(x) ((x)*(x))`)
A text-substitution pass that runs *before* compilation proper: expands `#include`, `#define` macros, evaluates `#ifdef` conditionals. Produces a single expanded source file. Often skipped in a first compiler.
Example: `#define SQUARE(x) ((x)*(x))` then `SQUARE(5)` is textually replaced with `((5)*(5))` before the lexer ever sees it.

---

## Lexing

Characters → tokens.

### Lexer / Lexing / Tokenization `[lexer]` (`int x=5;` → `KW_INT IDENT CONST ...`)
The first stage: turning the raw character stream into a flat sequence of tokens (keywords, identifiers, literals, punctuators), discarding whitespace and comments.
Example: the source `int x=5;` becomes the token stream `KW_INT IDENT("x") PUNCT("=") CONST(5) PUNCT(";")`.

### Token `[lexer]` (`KW_WHILE`, `CONST(123)`)
A single classified unit produced by the lexer: has a kind (keyword, identifier, constant, punctuator, ...) and often a value.
Example: the character sequence `while` becomes one token: `KW_WHILE`. The sequence `123` becomes `CONST(123)`.

### Keyword `[lexer]` (`int`, `while`, `return`)
A reserved word that can never be used as an identifier — the lexer must recognize it before considering it a name.
Example: `int`, `while`, `return`, `if`. The lexer must emit `KW_INT` for `int`, not `IDENT("int")`.

### Identifier `[lexer]` (`my_var`, `_count`, `X123`)
A name: starts with a letter or `_`, followed by any number of letters, digits, or underscores. Used for variables, functions, types.
Example: `my_var`, `_count`, `X123` are valid identifiers. `3x` is not — it starts with a digit, so the lexer reads `3` as a constant and `x` as a separate identifier token.

### Constant `[lexer]` (`42`, `'a'`, `3.14f`, `0x2A`)
A literal value written directly in source: a number or character literal (string literals are a separate category — see **string literal**).
Examples: `42` (integer constant), `3.14f` (float constant), `'a'` (character constant), `052` (octal — leading `0`), `0x2A` (hex).

### String literal `[lexer]` (`"hello"` → `{'h',...,'\0'}`)
A sequence of characters in double quotes, represented as a null-terminated array of `char`.
Example: `"hello"` becomes the byte sequence `{'h','e','l','l','o','\0'}` — 6 bytes, not 5.

### Punctuator `[lexer]` (`[`, `{`, `++`, `->`, `<<=`)
Any non-alphanumeric token: operator symbols and structural delimiters.
Example: `[`, `{`, `++`, `->`, `<<=`, `;`.

### Maximal munch `[lexer]` (`->` as one token, not `-` then `>`)
The rule — mandated by the C standard, not just a convention — that at every position the lexer must consume the *longest* possible sequence of characters that forms a valid token, rather than taking one character at a time. Requires a peek-ahead cursor (look at upcoming characters without consuming them) and checking longer candidate punctuators before shorter ones.
Example: `<<=` lexes as one `SHL_ASSIGN` token, never as `<`, `<=` or `<`, `<`, `=` — the lexer always prefers the 3-character match over the 2- or 1-character ones.
**Gotcha:** maximal munch is purely lexical and greedy — it knows nothing about grammar or programmer intent, which is why `a-->b` lexes as `a -- > b` (postfix-decrement `a`, compare to `b`), not `a - (-> b)`, even though a human might misread it. It's also why the K&R-era compound assignment spelling `x =- 1` was scrapped in favor of `x -= 1` in ANSI C — the old form was genuinely ambiguous against `x = -1` under greedy lexing.

### Lexer hack `[lexer]` (`(a) * b;` — cast or multiply?)
The problem where a C lexer, working alone, cannot tell whether an identifier is a plain variable name or a `typedef`-introduced type name — because that depends on declarations already processed by the *parser*, not on the spelling itself. The standard fix is to share a symbol table between lexer and parser: as the parser processes a `typedef`, it tells the lexer, and the lexer then emits a different token kind (`TYPEDEF_NAME` vs. `IDENTIFIER`) for that same spelling from then on.
Example: `(a) * b;` is a multiplication if `a` is an ordinary variable, but a cast-and-dereference (`*(a)b`, i.e. treat `b` as type `a*`) if `a` was declared via `typedef int a;` earlier. Identical tokens, different meaning — a pure, one-pass, context-free lexer cannot disambiguate this from characters alone.

---

## Parsing: techniques

Tokens → AST: how the parser works.

### Parser / Parsing `[parser]` (tokens → AST)
The stage that consumes the token stream and builds an AST according to the grammar — turning a flat list of tokens into a structured tree.
Example: given tokens for `2 + 3 * 4`, the parser must know `*` binds tighter than `+` and build the tree accordingly (see **AST** example above), not just left-to-right.

### AST (Abstract Syntax Tree) `[parser]` (`2 + 3*4` → a tree, not text)
A tree representation of the program's structure, with irrelevant syntax (parentheses, semicolons) stripped away — only the meaningful structure remains.
Example: `2 + 3 * 4` becomes:
```
   (+)
   / \
  2  (*)
     / \
    3   4
```
not `2 + (3 * 4)` as text, but a tree where `*` is nested inside `+` because of precedence.

### Recursive descent `[parser]` (`parseExpr() -> parseAssign() -> ... -> parsePrimary()`)
A parsing strategy where each grammar rule becomes a function, and functions call each other (recursively) following the grammar's structure. The most common way to hand-write a parser.
Example: `parseExpression()` calls `parseAssignment()`, which calls `parseConditional()`, ... down to `parsePrimary()`, mirroring the precedence table directly.

### Precedence climbing `[parser]` (one loop instead of N nested functions)
A parsing technique (a variant of the Pratt parser idea) for handling expressions with operator precedence without writing a separate grammar rule/function per precedence level. The standard alternative is recursive descent with one function per level (as the C grammar's expression hierarchy literally suggests).
Example: instead of `parseAdditive() -> parseMultiplicative() -> parseUnary() -> ...` as nested function calls, one loop tracks the "minimum precedence" allowed and recurses only when it sees a tighter-binding operator.

### Dangling else `[parser]` (`if(a) if(b) x; else y;` — else binds to which `if`?)
The classic ambiguity where `if (a) if (b) x; else y;` could parse the `else` as belonging to either `if`. C resolves it by rule: **`else` always binds to the nearest unmatched `if`**. Your parser has to implement this explicitly (usually just falls out of straightforward recursive descent).
Example: `if (a) if (b) x; else y;` means `if (a) { if (b) x; else y; }`, *not* `if (a) { if (b) x; } else y;`.

---

## Parsing: expressions

See also the precedence cheat-sheet at the end.

### Expression `[parser]` (`a + b*c`, `x = 5`, `foo(1,2)`)
Anything that evaluates to a value. The broadest category in the grammar; built up from tighter sub-categories (primary → postfix → unary → ... → assignment), each level encoding one precedence tier.
Example: `a + b * c`, `x = 5`, `foo(1, 2)`, `arr[i]` are all expressions.

### Primary expression `[parser]` (`42`, `x`, `(a + b)`)
The tightest-binding, atomic building block of expressions: a literal, an identifier, or a parenthesized expression.
Example: `42`, `x`, `(a + b)`.

### Postfix expression `[parser]` (`arr[i]`, `foo()`, `p->x`, `x++`)
A primary expression followed by an operator that comes *after* it: indexing, calls, member access, post-increment/decrement.
Example: `arr[i]`, `foo()`, `p->x`, `x++`.

### Unary expression `[parser]` (`-x`, `!flag`, `*p`, `&x`)
An expression with one operand and a prefix operator.
Example: `-x`, `!flag`, `*p` (dereference), `&x` (address-of), `sizeof(int)`.

### Cast expression `[parser]` (`(int)3.14`)
An expression that explicitly converts a value to another type using `(type)`.
Example: `(int)3.14` truncates the float `3.14` to the integer `3`.

### Binary expression `[parser]` (`a + b`, `x * y`)
An expression with two operands and one operator between them.
Example: `a + b`, `x * y`. Contrast with **unary expression** (`-x`, one operand).

### Constant expression `[parser/semantic]` (`int a[10];` — the `10`)
An expression the compiler must be able to evaluate at compile time, not runtime — required anywhere the language demands a fixed value.
Example: `int a[10];` — the `10` must be a constant expression. `int a[n];` where `n` is a variable is *not* allowed in this context (that's a VLA, a different feature).

---

## Parsing: declarations

### Translation unit `[parser]` (one whole `.c` file)
A single preprocessed source file, and the top-level entry point of the grammar: a sequence of external declarations (functions and global variables).
Example: one `.c` file is one translation unit. Your parser's top-level function should be `parseTranslationUnit()`, not `parseStatement()` — the top level of a file is declarations, not arbitrary statements.

### Declaration `[parser]` (`int *p = NULL;`)
A statement that introduces a name (variable, function, type) and its type, optionally giving it an initial value. Structure: `specifiers declarator = initializer;`.
Example: `int *p = NULL;` declares `p` as a pointer to `int`, initialized to `NULL`.

### Type specifier `[parser]` (`int`, `char`, `struct X`)
The part of declaration-specifiers naming the actual base type.
Example: in `static const int x`, `int` is the type specifier; `static` is a storage-class specifier; `const` is a type qualifier.

### Storage-class specifier `[parser]` (`static`, `extern`, `typedef`)
Keywords controlling a variable's lifetime/linkage rather than its type: `static`, `extern`, `auto`, `register`, `typedef`.
Example: `static int counter = 0;` — `static` here means the variable retains its value across calls instead of being reinitialized each time.

### Type qualifier `[parser]` (`const`, `volatile`, `restrict`)
A modifier on a type that doesn't change its size/representation but changes how it can be used: `const`, `volatile`, `restrict`.
Example: `const int x = 5;` — `x` can be read but any later `x = 6;` is a compile error.

### Declarator `[parser]` (`*name`, `name[10]`, `name(int)`)
The part of a declaration that names the variable and describes its "shape" (pointer-ness, array-ness, function-ness) — separate from the base type.
Example: in `int *a, b;`, the declarators are `*a` and `b`. The `*` belongs to the declarator, **not** the type specifier — so `b` is a plain `int`, not `int*`. This trips up everyone at least once.

### Pointer `[parser/semantic]` (`int *p;`, `int **pp;`)
A declarator element (`*`) meaning "address of a value of this type," and by extension the type itself.
Example: `int *p;` — `p` holds the address of an `int`. `int **pp;` — `pp` holds the address of an `int*` (pointer to pointer).

### Parameter-type-list `[parser]` (`int add(int a, int b)`)
The list of a function's parameter types in its declaration/definition.
Example: in `int add(int a, int b)`, the parameter-type-list is `int a, int b`. A trailing `...` (as in `printf`) means variadic — skip this for a minimal subset.

### Initializer `[parser]` (`= 5`, `= {1, 2, 3}`)
The value assigned to a variable at the point of declaration, following `=`.
Example: in `int x = 5;`, `5` is the initializer. In `int arr[3] = {1, 2, 3};`, `{1, 2, 3}` is a brace-enclosed aggregate initializer.

---

## Parsing: statements

### Statement `[parser]` (`if (x) { y = 1; }`)
A unit of execution that doesn't (necessarily) produce a value — the building blocks of a function body. Six categories: labeled, compound, expression, selection, iteration, jump.
Example: `if (x) { y = 1; }` is a (selection) statement; `y = 1;` inside it is an (expression) statement.

### Compound statement `[parser]` (`{ int x = 5; x++; }`)
A `{ }` block, which can contain a mix of statements and declarations.
Example: `{ int x = 5; x = x + 1; return x; }` is one compound statement containing a declaration and two statements.

### Expression statement `[parser]` (`x = x + 1;`)
Any expression followed by `;`, used for its side effect rather than its value. The most common kind of statement. A lone `;` is a "null statement."
Example: `x = x + 1;` — the expression `x = x + 1` has a value (the new value of `x`), but it's used as a statement for its assignment side effect.

### Selection statement `[parser]` (`if`/`else`, `switch`)
A branch: `if`/`else` or `switch`.
Example: `if (x > 0) { y = 1; } else { y = -1; }`.

### Iteration statement `[parser]` (`while`, `do-while`, `for`)
A loop: `while`, `do-while`, `for`.
Example: `while (i < 10) { i = i + 1; }`. Note `for (init; cond; step)` is essentially sugar for a `while` loop with the init hoisted out and the step appended to the body.

### Jump statement `[parser]` (`return`, `break`, `continue`, `goto`)
A statement that transfers control non-locally: `return`, `break`, `continue`, `goto`.
Example: `return x + 1;` exits the function immediately, passing `x + 1` back to the caller.

---

## Semantic analysis

Checking the AST and annotating it with types.

### Semantic analysis `[semantic]` (does `int x = "hi";` type-check?)
The stage after parsing that walks the AST enforcing rules the grammar alone can't express: type checking, "is this variable declared before use," function signature matching. Annotates the AST with type info for codegen to use.
Example: the grammar happily parses `x = "hello";` if `x` is an `int` — it's syntactically valid. Semantic analysis is what catches "you can't assign a `char*` to an `int`."

### Symbol table `[semantic]` (`x -> {type: int, offset: -8}`)
A data structure (usually a stack of scopes/hash maps) mapping identifier names to their declared type and other info, used during semantic analysis and codegen to resolve what a name refers to.
Example: after seeing `int x = 5;`, the symbol table gains an entry `x -> {type: int, stack_offset: -8}` so that a later reference to `x` knows both its type and where to find it.

### Type checking `[semantic]` (`int x = "hello";` → error)
Verifying that operations are applied to compatible types, and computing the type of every expression.
Example: `int x = "hello";` fails type checking — you're trying to initialize an `int` with a `char*`.

### Lvalue / Rvalue `[semantic]` (`x = 5` ok, `5 = x` error)
An **lvalue** is an expression that refers to a storage location (can appear on the left of `=`). An **rvalue** is a value with no persistent storage location (can only appear on the right).
Example: in `x = 5`, `x` is an lvalue (it names a memory location). In `5 = x`, `5` is an rvalue — this is a compile error, you can't assign to a literal.

### Sequence point `[semantic]` (`i++ + i++` — undefined order)
A point in execution where all side effects of prior expressions are guaranteed complete before moving on. Expressions relying on evaluation order *between* sequence points are undefined behavior.
Example: `i++ + i++` is undefined — there's no sequence point between the two increments, so which one happens "first" isn't defined. You don't need to detect this, but your codegen shouldn't assume a particular left-to-right order where the standard doesn't guarantee one.

---

## IR generation

Optional in a first compiler.

### Intermediate Representation (IR) `[codegen]` (`t1 = load a; t2 = add t1, t2`)
A representation of the program between the AST and final machine code — simpler and flatter than the AST, but not tied to a specific CPU. Optional for a first compiler; you can go AST → assembly directly.
Example, IR for `x = a + b`:
```
t1 = load a
t2 = load b
t3 = add t1, t2
store x, t3
```

---

## Code generation

AST/IR → assembly.

### Codegen (Code Generation) `[codegen]` (`x = a+b` → `mov`/`add`/`mov`)
The stage that converts IR/AST into target assembly — deciding which instructions to emit, which registers to use, how to lay out memory.
Example: turning the AST for `x = a + b;` into `mov eax, [a] / add eax, [b] / mov [x], eax`.

### Backend `[codegen]` (AST/IR → `mov`, `add`, `push`)
The part of the compiler that turns IR (or AST) into target machine code. Contrast with **frontend**.
Example: the part of your compiler that walks the AST and emits `mov`, `add`, `push` instructions.

### ABI (Application Binary Interface) `[codegen]` (args in `rdi`, `rsi`, ... / return in `rax`)
The low-level contract for how compiled code interacts with the OS and other compiled code: register usage, stack layout, calling convention, data sizes. On Linux x86-64, this is the **System V AMD64 ABI**.
Example: the ABI says the first integer argument to a function goes in `rdi`, the second in `rsi` — so `foo(1, 2)` compiles to `mov edi, 1 / mov esi, 2 / call foo`.

### Calling convention `[codegen]` (arg 1 → `rdi`, return → `rax`)
The specific rules — part of the ABI — for how a function call passes arguments, returns a value, and which registers survive the call.
Example (System V x86-64): args 1–6 go in `rdi, rsi, rdx, rcx, r8, r9`; the return value comes back in `rax`; `rbx, r12–r15` are callee-saved (a called function must restore them if it uses them).

### Stack frame `[codegen]` (`push rbp; mov rbp,rsp; sub rsp,N`)
The chunk of stack memory allocated for one function call, holding its local variables, saved registers, and return address.
Example: on entry, a function typically does `push rbp / mov rbp, rsp / sub rsp, N` to set up a frame of `N` bytes for locals, then `mov rsp, rbp / pop rbp / ret` to tear it down on exit.

### Register allocation `[codegen]` (variable → `rax` vs. stack slot)
Deciding which values live in CPU registers (fast) versus memory/stack (slow) at each point in the program, given there are far fewer registers than variables. A hard problem in real compilers; a first compiler can cheat by spilling everything to the stack.
Example: naive strategy — every variable always lives on the stack, loaded into a register right before use and stored back right after. Correct but slow; real compilers keep hot variables in registers across many instructions.

---

## Assembling and linking

Done by external tools, not by the compiler.

### Assembler `[toolchain]` (`as file.s -o file.o`)
The tool that turns human-readable assembly text (`.s`) into a machine-code object file (`.o`). You don't write one — you emit `.s` and shell out to `as` (or `gcc`/`cc` acting as a driver for it).
Example: `as output.s -o output.o`.

### Linker `[toolchain]` (`cc output.o -o output`)
The tool that combines one or more object files (and libraries) into a final executable, resolving references between them (e.g. a call to `printf` gets linked against libc).
Example: `ld output.o -o output` or, more commonly, letting `gcc`/`cc` invoke the linker for you: `cc output.o -o output`.

---

## Quick precedence cheat-sheet

From loosest to tightest binding (top = evaluated last):

```
,                              comma
= += -= *= ...                 assignment
?:                              conditional (ternary)
||                              logical OR
&&                              logical AND
|                               bitwise OR
^                               bitwise XOR
&                               bitwise AND
== !=                           equality
< > <= >=                       relational
<< >>                           shift
+ -                              additive
* / %                            multiplicative
(type)                           cast
! ~ - + * & ++ -- sizeof         unary (right-to-left)
() [] . -> ++ --                 postfix
literals, identifiers, (expr)    primary
```
