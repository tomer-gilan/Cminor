# Architecture Decision Records

A log of significant design decisions for the Cminor compiler: what was decided, why, and what it implies.
Entries are append-only. To change a decision, add a new entry that supersedes the old one and mark the old one's status accordingly.

Format per entry: **Status** (Proposed / Accepted / Superseded by ADR-NNN), **Date**, **Context**, **Decision**, **Consequences**.

---

## ADR-001: Build and run on a Linux host (Ubuntu), not natively on Windows

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context
The compiler's output is x86-64 assembly. That output has to be assembled and linked into an executable, and the executable has to run so its behavior can be tested. The development machine runs Windows, but the compiler targets the System V AMD64 ABI and ELF (see `overview.md`), which is Linux's world. Windows uses a different calling convention (Microsoft x64) and object format (PE/COFF).

### Decision
Build and run the compiler on Ubuntu via CLion's remote toolchain (`/usr/bin/cc`, GNU Make). The compiler emits x86-64 GNU assembly (`.s`), and `gcc` is used as the back-end driver to **assemble** (via `as`) and **link** (via `ld`, against libc) the result into an ELF executable. The OS loader then loads it at run time.

### Consequences
- Generated code follows the System V AMD64 ABI and produces ELF binaries. It does not run natively on Windows.
- Linux tooling is available for both the compiler itself and its output: `gdb`, `objdump`, `readelf`, `strace`, Valgrind, and the GCC/Clang sanitizers (ASan, LSan, UBSan).
- `gcc -O0 -S` on the same host serves as the reference output for any construct.
- A native Windows build of the compiler is not a goal; Windows-specific issues (MSVC, PE/COFF, Microsoft x64 ABI) are out of scope.

---

## ADR-002: Run the test suite as a CLion commit check

- **Status:** Accepted
- **Date:** 2026-09-30

### Context
Tests are only useful if they actually run. The options considered (see `testing.md`, "Automation"): run tests on every build (CMake `POST_BUILD`), before every program launch (CLion "Before launch"), before every commit (CLion commit check or a git `pre-commit` hook), or after push (CI).
Gating every build or launch gets in the way while debugging a failing test. A git `pre-commit` hook runs on the Windows side, where git runs, but the tests need the remote Linux toolchain.

### Decision
Use CLion's **before-commit check** ("Run Tests" in the commit dialog's commit checks) with the CTest run configuration. Tests are also run manually and often during development.

### Consequences
- Tests run through CLion, so they build and run with the remote Ubuntu toolchain: no cross-OS hook plumbing.
- A commit made outside CLion (e.g. plain `git commit` in a terminal) skips the check. CI can be added later as a second safety net.
- Builds and program runs stay fast and ungated, so a red test never blocks debugging it.

---

## ADR-003: Use golden-file tests for stage output

- **Status:** Accepted
- **Date:** 2026-09-30

### Context
A compiler's stages produce structured output (a token stream now, an AST and assembly later). Asserting on that output field by field in C is verbose and brittle.

### Decision
Add golden-file ("snapshot") tests: each stage can dump its output as stable text, and the tests compare that dump against a checked-in expected file. Details and workflow are in `testing.md`, "Golden-file tests".

### Consequences
- Needs a deterministic, stable text dump per stage (starting with the token dump).
- Expected files are part of the test suite and are reviewed like code when they change.
- Line endings must be pinned (`.gitattributes`) because git runs on Windows.

---

## ADR-004: Write the compiler in C11; self-hosting is not a goal, but is kept possible

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context
The implementation language shapes everything from data structures to debugging. `overview.md` lists C, C++, Rust, Python and others. Self-hosting (the compiler compiling itself) would additionally require the compiler's own source to stay inside the language subset it implements.

### Decision
Write the compiler in **C11** (`CMAKE_C_STANDARD 11`). Self-hosting is **not a goal**, but it isn't ruled out: where it costs nothing, conventions stay self-hosting-friendly.

### Consequences
- Manual memory management and ownership rules are needed (`conventions.md`, "Ownership").
- Self-hosting-friendly choices where they're free, e.g. `int` rather than `size_t` for sizes and indices (`conventions.md`, "Integer types").
- The subset (`SUBSET.md`) is designed for the language's sake, not for the compiler's own source. Reaching self-hosting later would require structs, function pointers and more.

---

## ADR-005: Emit x86-64 assembly directly, in AT&T syntax

- **Status:** Accepted
- **Date:** 2026-09-30

### Context
Codegen can target a backend (LLVM IR, QBE), which handles register allocation and instruction selection, or emit machine assembly directly. The project's purpose is to learn how compilers work, codegen included. Assembly can be written in AT&T syntax (the GNU `as` default) or Intel syntax (`.intel_syntax noprefix`).

### Decision
The compiler emits **x86-64 assembly itself**, with no LLVM or QBE: AST to assembly, possibly with its own IR later. The syntax is **AT&T**. The `.s` file is assembled and linked by `gcc` (ADR-001).

### Consequences
- Register allocation, instruction selection and the System V calling convention are the compiler's own work.
- `gcc -O0 -S` output is AT&T too, so it can be diffed against Cminor's output directly.
- Operand order is source, destination (`movl $1, %eax`), the reverse of the Intel manuals. Keep that in mind when reading the Intel SDM.
- Type sizes and the rest of the data model follow the ABI: see `implementation-defined.md`.

---

## ADR-006: No preprocessor; the input is one complete translation unit

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context
Translation phase 4 (directives and macro expansion) is a separate, largely independent text-processing layer. C programs normally get library declarations through `#include`.

### Decision
Cminor has **no preprocessor**. The input is a single, already-complete translation unit. The preprocessing punctuators, header names and preprocessing numbers are out of the subset (`SUBSET.md`).

### Consequences
- Test programs declare the libc functions they use by hand (`int puts(char* s);`).
- Line splicing (phase 2) and adjacent string literal concatenation (phase 6) are also out, since they mainly exist to serve macros (`SUBSET.md`).
- A backslash or `#` outside a literal is an *unknown character* error.

---

## ADR-007: All compilation-wide state lives in one `Context`

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context

Several pieces of state outlive a single stage: the source texts, the identifier table, the string-literal table, and
(soon) the diagnostics. They could be globals or live in an object that's passed around.

### Decision
They live in one **`Context`**, passed explicitly to every stage and to any function that needs them. There's no global mutable state.

### Consequences
- Tests can create an independent `Context` per test case, with no shared state between tests.
- Function signatures show what state they touch, which is why "deep const" on `Context*` is meaningful (`conventions.md`).
- New cross-stage state (diagnostics, ADR-012; later symbol tables) goes into `Context` too.

---

## ADR-008: Positions are (file, byte offset); every token carries a `Range`

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context
Diagnostics in every stage need to point at source locations, mostly at *valid* tokens (e.g. an undeclared identifier). Storing line and column everywhere is redundant and has to be kept correct during lexing.

### Decision

- A `FilePosition` is a (source-text index into the `Context`, byte offset) pair.
- Line and column are **computed on demand**, 1-based, by counting `'\n'` (`file_position_get_file_line_column`).
- Every token carries a **`Range`** (start position + length), so its spelling is a slice of the source text rather than a copy.

### Consequences
- `{0, 0}` is never a real line/column, so it can serve as a "no position" value.
- Computing line/column is O(offset). That's fine for printing diagnostics; cache it if it ever matters.
- The source text must outlive all tokens and `TextView`s into it (it's owned by `Context`).

---

## ADR-009: The lexer tokenizes the whole file before parsing

- **Status:** Accepted
- **Date:** 2026-09-30 (recorded; decision predates this log)

### Context
A lexer can hand tokens to the parser on demand (`next_token()`), or produce the complete token list first.

### Decision

`lexer_lex_source_text` produces the **complete token list up front**, which always ends with exactly one end-of-file
token. The parser consumes that list.

### Consequences
- The parser can peek any distance ahead for free.
- The lexer and parser are fully decoupled, which makes it easy to dump and golden-test the token stream (ADR-003).
- Lexer errors are all known before parsing starts, so the driver can stop after lexing (ADR-012).
- Memory is O(number of tokens), which doesn't matter at this scale.
- Character constants produce `TOKEN_CONST_INT` tokens, since in C their type is `int`.

---

## ADR-010: String literals are interned as bytes + length

- **Status:** Accepted, implemented (literals are `Text`, compared with `text_list_compare`)
- **Date:** 2026-09-30

### Context
Escape processing (phase 5) turns a literal's spelling into its byte contents, which are then interned in `Context`'s string-literal table. Today they're stored as NUL-terminated C strings. Since `\0` is in the subset, `"a\0b"` would be stored as `"a"`, and would also be merged with a genuine `"a"`.

### Decision
Decoded string literals are stored and interned as **(bytes, length)** pairs and compared by length and content, never with `strcmp`.

### Consequences
- Embedded NUL bytes work.
- Codegen emits each literal from its byte array, not as a C string, e.g. with `.byte` directives, or with `.ascii` plus explicit escapes.
- Interning and lookup need a length-aware comparison function.
- The decoded bytes live in a growable `Text`, so literals have no length limit (the old fixed 4096-byte decode buffer
  is gone).

---

## ADR-011: Error policy: asserts for compiler bugs, diagnostics for user errors

- **Status:** Accepted. Narrowed for the parser by ADR-020 (stop at the first syntax error in v1).
- **Date:** 2026-09-30 (recorded; stated in `Lexer.c`'s "general note" and in the lexer code review)

### Context
Something can go wrong for three different reasons: a bug in the compiler, a caller breaking a function's precondition, or an error in the user's source. These need different handling. A compiler must never crash on bad input, and it should report as many errors as it can in one run.

### Decision
- **`assert(cond && "message")`** only for compiler bugs and broken caller preconditions (internal invariants).
- **Errors in the user's source** become diagnostics, and processing **continues** (with a best-effort value so later code sees something sane).
- **Library code never calls `exit`.** Only the driver decides to stop, after a stage that reported errors. The sole exception is allocation failure (ADR-013).

### Consequences
- A crash on any input is a compiler bug, by definition.
- Each stage needs error recovery: the lexer produces an `UNKNOWN` token and moves on, and later the parser will need synchronization points.
- Environment errors (missing file, missing argument) are diagnostics too. `driver_create`'s `exit(1)` is a known violation (`TODO.md`).

---

## ADR-012: Diagnostics are collected in `Context`

- **Status:** Accepted
- **Date:** 2026-09-30

### Context

Diagnostics can be printed at the moment they're issued or collected and reported later. Tests need to check which
diagnostics were produced, and where (`roadmaps/testing-roadmap.md`, step 8).

### Decision
Diagnostic instances are **collected in a list in `Context`**, along with an error count. The driver prints them at the end of each stage, sorted by position, and stops the pipeline if the stage produced any errors. The output format is defined in `implementation-defined.md`.

### Consequences
- Tests inspect the list directly, instead of capturing `stderr`.
- Diagnostics from one stage come out sorted, regardless of the order they were found in.
- A diagnostic *descriptor* (the static code → severity + message table) is separate from a diagnostic *instance* (code + range + details).
- The parser never runs on a token stream that contains errors.

---

## ADR-013: Allocation failure aborts, via `xmalloc` / `xrealloc`

- **Status:** Accepted, implemented 2026-10-01 as `utils_xmalloc` / `utils_xrealloc` in `Utils` (prefixed per `conventions.md`; no `xstrdup`, `utils_allocate_and_copy_string` covers it)
- **Date:** 2026-09-30

### Context
`assert(ptr != NULL)` after `malloc` disappears under `NDEBUG`, and most call sites don't check at all. Running out of memory isn't a user error, and there's nothing sensible to recover to.

### Decision
All allocation goes through `xmalloc` / `xrealloc` (and maybe `xstrdup`) wrappers, which print a message and `abort()` on failure. This is the **only exception** to "library code never exits" (ADR-011).

### Consequences
- Call sites never check for `NULL` from allocation, which removes a lot of noise.
- The redundant `assert(... && "malloc failed")` lines go away.

---

## ADR-014: Punctuators are lexed by growing one character at a time, which requires a prefix-closed punctuator set

- **Status:** Accepted
- **Date:** 2026-09-30

### Context
`lexer_lex_punctuator` does maximal munch by extending the candidate spelling one character at a time. It keeps a character only if the longer spelling is still a punctuator (`token_kind_from_spelling`), and stops at the first one that isn't. It can't pop a character and check it afterwards, because there's no un-pop. `is_char_part_of_punctuator` decides whether to enter this path at all: it's true for any character that appears anywhere in any punctuator's spelling.

This only works if the punctuator set is **prefix-closed**: every prefix of every punctuator is itself a punctuator. In particular, every character that appears in a punctuator is a one-character punctuator.

The Cminor punctuator set (`SUBSET.md`, A.1.7) is prefix-closed today: `<<` `>>` `<=` `>=` `==` `!=` `&&` `||` are the only multi-character punctuators, and `<` `>` `=` `!` `&` `|` are all in the set.

### Decision
Keep the one-character-at-a-time munch, and treat "the punctuator set is prefix-closed" as an **invariant** of the lexer. It isn't checked in code yet, and that is accepted for now.

### Consequences
- Adding a punctuator whose prefixes aren't all punctuators breaks the lexer:
    - A missing **one-character** prefix (e.g. keeping `||` but dropping `|`) makes `lexer_lex_punctuator` return a
      **zero-length token without advancing**, so `lexer_lex_source_text` **loops forever**, even on valid input.
  - A missing **longer** prefix (e.g. standard C's `...` without `..`) produces **wrong tokens**: `...` lexes as three `.` tokens.
- So any change to the punctuator set in `SUBSET.md` or `Token.h` must re-check this invariant, or change the munching algorithm.
- Possible guards, when wanted: a unit test that checks every prefix of every punctuator spelling is in the table, and/or an assert that every non-EOF token has length > 0 (a progress guarantee that covers every lexing path). See `reviews/2026-09-30-code-review.md`, M5.

---

## ADR-015: The AST has one node type per family, each a tagged union

- **Status:** Accepted, implemented in slice 1. The name `ExternalDefinitionNode` is superseded by ADR-026
  (`ExternalDeclarationItem`, `FunctionDefinitionNode`), which also defines what counts as a node.
- **Date:** 2026-10-16 (recorded; decided in parser roadmap Step 0d)

### Context
An AST can use a single `Node` type with one big union for every construct, or a separate type per grammatical family.
With a single type, nothing stops a statement from ending up where an expression belongs.

### Decision
One type per family: `ExpressionNode`, `StatementNode`, `ExternalDefinitionNode`, and later a declaration node. Each is
a tagged union: a `kind` enum (`EXPRESSION_KIND_*`, ...) plus an `as` union with one member per kind. A place that can
hold more than one family gets its own small tagged wrapper, like `BlockItem` (a statement or, from slice 6, a
declaration).

### Consequences
- The compiler checks that a child belongs to the right family: a `ReturnStatement` holds an `ExpressionNode*`, and
  nothing else fits.
- Every switch over a `kind` has to handle each kind, and a new kind shows up as a `-Wswitch` warning wherever it's
  missing.
- Code that walks every node (the AST dump, later sema) needs one function per family, not one generic visitor.

---

## ADR-016: Every AST node carries a `Range` spanning the whole construct

- **Status:** Accepted, implemented in slice 1
- **Date:** 2026-10-16

### Context
Sema and later stages report errors at nodes (an undeclared identifier, a non-lvalue on the left of `=`), so nodes need
source positions, just like tokens (ADR-008). The question is how much of the source a node covers.

### Decision
Every node has a `Range` from its first token to its last, **punctuation included**: `return 2;` includes the `;`, a
compound statement goes from `{` to `}`, and a function definition from `int` to the closing `}`. A binary node spans
from its left operand's start to its right operand's end. A node's range is built with `range_union` over its tokens
and children.

### Consequences
- A diagnostic about a node can underline exactly the code the user wrote.
- Parse functions have to keep the tokens they consume (`(`, `;`, `}`), not only the children, to compute the range.
- Tests that check ranges by hand-counted offsets are fragile. Keep them in a few dedicated tests once the AST dump
  exists (parser roadmap Step 2).

---

## ADR-017: AST nodes live in `Context`'s arena; sequences are `next` pointers

- **Status:** Accepted, implemented in slice 1
- **Date:** 2026-10-16

### Context
The AST is built once, read by every later stage, and thrown away all at once at the end. Freeing it node by node
would need a destroy function per node kind, kept in sync with every new kind.

### Decision
- Every node is allocated from the **arena owned by `Context`** (ADR-007), and the whole AST is freed only by
  `context_destroy`. There's no per-node free.
- Sequences in the AST (a translation unit's definitions, a block's items) are **intrusive `next` pointers**, never a
  `List`.
- Nodes copy what they need from tokens (`Range`, interned id, constant value) and never point at a `Token`.

### Consequences
- The AST outlives the `Parser` and the token list: the driver can destroy both right after parsing.
- Nothing in the AST needs a destroy call, so a half-built AST after a syntax error leaks nothing.
- Every pointer into the AST becomes invalid when `Context` is destroyed. Nothing may keep one past that.
- Walking a sequence is a `for (x = head; x != NULL; x = x->next)` loop. There's no indexing and no size: count by
  walking.

---

## ADR-018: AST nodes name things by their interned identifier id

- **Status:** Accepted, implemented in slice 1
- **Date:** 2026-10-16

### Context
The lexer already interns every identifier in `Context`'s identifier table, and an identifier token carries its id.
Nodes could store that id, or the spelling (a `TextView` into the source).

### Decision
Nodes store the **interned `int` id** (e.g. `name_identifier_id`), never a string or a `TextView`.

### Consequences
- Two names are equal exactly when their ids are equal: comparing them is `==`, with no string comparison.
- Sema's symbol table can key on the id.
- Printing a name (diagnostics, the AST dump) goes through `Context`'s identifier table.

---

## ADR-019: The parser builds `Type`s for declarations

- **Status:** Accepted, to be implemented in slice 6 (slice 1 only has the `int` keyword). Clarified by ADR-026: a
  `DeclaratorNode` holds the declared name, its range and its `Type`, not declarator syntax.
- **Date:** 2026-10-16

### Context
Declarations carry types, some of them nested (`int *a[3]`: array of 3 pointers to `int`). The parser can build a
`Type` while it parses the declarator, or store the declarator syntax as written and leave the interpretation to sema.
Cminor has no `typedef` (`SUBSET.md`), so the syntax alone always determines the type.

### Decision
The parser builds `Type`s directly. There's one `Type` representation (`int`, `char`, `void`, pointer-to, array-of,
function), allocated from `Context`'s arena (ADR-017), and declaration nodes point at their `Type`.

### Consequences
- There are no declarator nodes in the AST, and only the parser has to understand declarator syntax.
- Sema works on `Type`s only: type checking, sizes and compatibility never look at syntax.
- The `Type` representation is needed in slice 6, before sema exists, so it's designed in the parser roadmap.

---

## ADR-020: The parser stops at the first syntax error (v1)

- **Status:** Accepted, implemented in slice 1. Narrows ADR-011's "processing continues" for the parser.
- **Date:** 2026-10-16

### Context
ADR-011 says errors in the user's source become diagnostics and processing continues. For a parser, continuing means
error recovery: after an error, skip tokens up to a synchronization point and resume ("panic mode"). That brings
half-built nodes and cascades of follow-up errors, which is about as much work as the parser itself.

### Decision
- In v1 the parser **stops at the first syntax error.** It reports one diagnostic and sets its error flag
  (`parser_set_error`). Once the flag is set, nothing else is parsed.
- **Failure contract:** a parse function that fails returns `NULL`, and its caller checks for that before using the
  result or calling the next parse function. A parse function's asserted preconditions (ADR-011) only hold on the
  no-error path, so the caller checks before it calls.
- **Panic mode is a possible later addition**, not part of v1. When it comes, the synchronization points are: in a
  block, the next `;`, or a `}` at the current brace depth; at top level, the next `int` at brace depth 0.

### Consequences
- One run reports at most one syntax error.
- A crash on bad input is still a compiler bug (ADR-011). With this contract it's usually a caller that didn't check
  for `NULL`, or that called a function whose precondition no longer holds.
- Error tests check the first diagnostic's code and position, **and that it's the only one** (count == 1). The count
  check is what catches cascades: a caller that didn't check after a failed `expect`, so a second call reports again
  (found 2026-10-16, when one stray `}` produced 3 diagnostics). If panic mode is added, the count check is relaxed
  for the tests where resynchronizing legitimately finds more errors.
- The driver doesn't run later stages after a parser error (ADR-012).

---

## ADR-021: Semantic analysis is a separate pass over the finished AST

- **Status:** Accepted, to be implemented in sema Step 1
- **Date:** 2026-10-16

### Context
Name resolution and type checking can happen while parsing (chibicc keeps a symbol table in its parser and types each
node as it builds it), or in a separate stage that walks the AST once the parser is done (`overview.md`'s pipeline).

### Decision
Sema is its **own pass**. The parser only builds the AST (ADR-015 to ADR-020). The driver runs sema on the finished
AST when the parser reported no errors, and runs codegen only when sema reported none (ADR-012).

### Consequences
- The parser stays purely syntactic: it never needs a symbol table, and every rule that isn't grammar is sema's.
- Sema sees the whole tree, so a check can look at anything already parsed.
- A program that parses but breaks a rule (`1 = x`, an undeclared name) reaches sema, and sema gives the diagnostic.
- `sema-roadmap.md` mirrors `parser-roadmap.md`, step for step.

---

## ADR-022: The symbol table is a stack of scopes

- **Status:** Accepted, to be implemented in sema Step 6 (block scopes). Until then (sema Step 1), file scope is the
  only scope and is a single table indexed by the interned identifier id: no stack and no linear search.
- **Date:** 2026-10-16

### Context
Sema needs to know which declaration each name refers to, following C's scope rules: an inner block can shadow an
outer name, and a name can't be declared twice in the same scope. chibicc uses one flat list and truncates it when a
block ends; the alternative is a stack of separate scopes.

### Decision
- A **stack of scopes**: file scope at the bottom, and one scope pushed at each `{` and popped at its `}`.
- Each scope holds its own symbols, keyed by the interned identifier id (ADR-018), searched linearly.
- Lookup searches from the innermost scope outwards. "Declared twice in the same scope" checks the top scope only.
- A symbol records what it is (function, variable, parameter), its `Type` (ADR-019) and the declaration node it came
  from. Symbols are allocated from `Context`'s arena (ADR-017), because sema's results point at them and outlive the
  sema pass (ADR-023). The scope stack itself only exists while sema runs.

### Consequences
- Shadowing and same-scope redeclaration are both direct checks, with no bookkeeping of list lengths.
- A function's parameters and its body's outermost block must be **one** scope (`int f(int a) { int a; }` is an
  error), so a function body doesn't push a second scope for its outermost `{`.
- Linear search is fine at this scale. A hash map per scope is a later optimization, not a design change.

---

## ADR-023: Sema never modifies the AST; its results live in a side table indexed by expression id

- **Status:** Accepted. Needs a parser change before sema Step 1 (expression ids). Revised 2026-10-18: one array of
  `ExpressionSemantics` instead of one array per fact.
- **Date:** 2026-10-16

### Context
Sema finds out things the source doesn't spell: the type of every expression and the declaration every identifier
refers to. Those can be written into the AST (fields the parser leaves empty and sema fills in, as clang does) or kept
beside it (as *Crafting Interpreters*' resolver does with a map from expression to result). Writing into the AST splits
responsibility for the tree between two stages and mixes what each stage's tests check.

### Decision
- **The AST is final once the parser returns.** Sema only reads it.
- Sema produces a separate result: **a side table indexed by expression id**. The parser numbers every expression node
  with an `int id` from 0 to n−1 (a counter, which carries no meaning beyond identity), and sema's result, a
  `SemaResult`, holds one array of length n, `expressions_semantics`, with `expressions_count` = n. Each entry, an
  `ExpressionSemantics`, is what sema learned about that expression:
  - `const Type* type`: every expression's type, after decay (ADR-024).
  - `const Symbol* identifier_symbol`: for an identifier expression, the declaration it refers to (ADR-022); `NULL`
    for every other expression.
- One array of entries, not one array per fact: sema and codegen always ask about one expression at a time, so its
  facts sit together, with one allocation and one count. A later fact (e.g. the value of a `sizeof`, sema Step 14) is
  a new field, not a new array.
- Facts that follow from the AST plus that table aren't stored at all: whether an expression is an identifier is its
  node's kind (no `is_identifier` field), and whether it is an lvalue is a function, `sema_is_lvalue`, over the node's
  kind and (for an identifier) its `identifier_symbol`.
- Declarations keep the `Type` the parser built for them (ADR-019). Sema adds nothing to them.

### Consequences
- Each stage owns its own data, and its tests check only that: parser tests check the AST, sema tests check the
  table.
- Lookups are O(1) array indexing. There's no map keyed by pointer, and no new data structure.
- Codegen reads two things: the AST and sema's result. The typed dump (sema roadmap) prints the AST with each
  expression's type from the table.
- The parser change: an `id` field in the expression node, a counter in the parser, and the final count n exposed to
  sema. Other node families get ids only if a later step needs per-node sema results for them.

---

## ADR-024: Sema decides array decay and pointer scaling; codegen derives widths from types

- **Status:** Accepted, used from sema Step 11 (pointers) and Step 12 (arrays)
- **Date:** 2026-10-16

### Context
C converts values implicitly: an array name becomes a pointer to its first element ("decay"), except as the operand of
`sizeof` or `&`; `p + 1` adds `1 × sizeof *p` bytes; `char` widens to `int` in arithmetic and `int` narrows to `char`
on assignment. Decay depends on context: the same identifier `a` is `int *` in `a + 1` and `int[3]` in `sizeof a`.
Sema has to apply that rule anyway to check `int *p = a;`. If codegen applied it again, the rule would live in two
places, which could disagree.

### Decision
- Sema records each expression's type **after** decay in its type table (ADR-023): `a` in `a + 1` is recorded as
  `int *`, and `a` under `sizeof` as `int[3]`. Codegen never re-derives decay.
- Pointer scaling, and every `char` ↔ `int` width, are derived by codegen from the recorded types: `p + 1` scales by the
  size of `p`'s pointee type, and a load or store uses the size of the recorded type.
- No conversion nodes are inserted (the AST is final, ADR-023).

### Consequences
- The context-dependent rule (decay) lives only in sema. The rules that follow directly from a type (sizes, scaling,
  sign extension) live in codegen, which needs the types to pick instructions anyway.
- Codegen tells "address of an array" from "load a scalar" by the recorded type: an identifier recorded as a pointer
  while its declaration's type is an array means "emit its address".
- The typed dump shows decay directly (`a:int*` vs `a:int[3]`), so it's testable at the sema level, without assembly.

---

## ADR-025: Sema stops at the first error (v1)

- **Status:** Accepted, from sema Step 1. Narrows ADR-011's "processing continues" for sema, as ADR-020 does for the
  parser.
- **Date:** 2026-10-16

### Context
Sema can keep going after an error more easily than the parser can, but it needs an *error type* to stop one bad
expression from causing an error at every later use (one undeclared `x`, one error per use of `x`).

### Decision
Like the parser (ADR-020), sema **stops at the first error** in v1: it reports one diagnostic and checks nothing more.
Reporting every error, with an error type that silences follow-up errors, is a possible later addition.

### Consequences
- One run reports at most one semantic error.
- Sema's error tests check the diagnostic's code, position, and count == 1, with the same kind of helper as the parser's.
- A note attached to an error (e.g. "previous definition is here") is a second diagnostic. If one is ever added, the
  count rule has to say whether notes count.

---

## ADR-026: What an AST node is, and what a node may contain

- **Status:** Accepted, to be applied before slice 6. Refines ADR-015 and ADR-017, supersedes ADR-015's
  `ExternalDefinitionNode` name, and clarifies ADR-019's "no declarator nodes".
- **Date:** 2026-10-21

### Context
ADR-015 names the node families but not what makes something a node, and slice 1 drifted in two ways:
- `ExternalDefinitionNode` is a list wrapper, yet it carries the `Range` and the `Node` name, while the function
  definition it wraps (`ExternalDefinitionFunction`) has no range.
- That wrapper holds the function definition **by value**, while `BlockItem` holds its statement **by pointer**.

From slice 6 (block declarations) and slice 10 (file-scope declarations) the same `DeclarationNode` sits under both
wrappers, so the two have to agree. This ADR is the rule to check every new AST type against.

### Decision
**A node** is a construct that a diagnostic or sema can point at. A node:
- has its own `Range` (ADR-016),
- is what a parse function returns,
- has an `int id` if sema keeps per-node results for it (ADR-023),
- has a type name ending in `Node`. Only node types end in `Node`.

**A wrapper** is a list cell, possibly also an "X or Y" choice (`BlockItem`: a statement or a declaration). It is not
a node: no range, no id. Its type name ends in `Item`, and only wrapper types end in `Item`. A choice wrapper is `kind`,
an `as` union of node pointers, and `next`; a plain list cell (`ArgumentItem`) is its node pointer(s) and `next`.

**A node's layout:** `range`, `id` (if any), `kind`, and an `as` union with one payload struct per kind. Payload structs
are inline: they are the node's own content, not children, and have no range of their own.

**The child rule.** Every field of a payload struct or a wrapper is one of:
1. **a terminal:** a scalar (`int`, `bool`), an interned identifier id (ADR-018), or a handle into a shared table such
   as `const Type*` (ADR-019);
2. **a pointer to a node**, where `NULL` means the child is absent (e.g. a declarator without an initializer);
3. **a pointer to the head of a list of wrappers** (ADR-017's intrusive `next`).

A node or a wrapper is **never held by value** inside another node or wrapper.

**Names.** Names say what a type is, even when that makes them long: someone re-reading the code months later shouldn't
have to guess.
- `ExternalDefinitionNode` becomes `ExternalDeclarationItem` (a wrapper) and `ExternalDefinitionFunction` becomes
  `FunctionDefinitionNode`. The standard's grammar is `external-declaration: function-definition | declaration`, and a
  prototype or a global (slice 10) is a declaration, not a definition.
- `InitDeclarator` becomes `DeclaratorItem` (see below).
- A call's arguments (slice 9) are a list of `ArgumentItem`: an `ExpressionNode*` and `next`.

**The AST stores what was declared, not how it was spelled.** `int a, b;` (one shared specifier) and a parameter list
`(int a, int b)` (one specifier each) differ only in syntax. The parser checks the syntax and builds each name's `Type`
(ADR-019); after that both are a list of declared names. So both use the same list cell, `DeclaratorItem`: a
`DeclaratorNode*`, an optional `initializer` (`ExpressionNode*`, `NULL` = none), and `next`. A parameter's `initializer`
is always `NULL`; the parser guarantees it (C has no default arguments). How a list is treated (variables in a block,
parameters filled from registers) belongs to the code that walks it, not to the list's type.

**Declarators are nodes.** `Declarator` becomes `DeclaratorNode`: the declared name, its range and its `Type`. It's held
by pointer, and a function's parameters are a list of `DeclaratorItem` instead of an array plus a count.
- Why: it has a range that diagnostics point at ("redefinition of `x`"), and codegen needs the `Symbol` behind
  `int x = 3;`, which is a per-declarator sema result.
- It gets an `id` when sema first needs that result (slice 6), per ADR-023's "other node families get ids only if a
  later step needs per-node sema results".
- This doesn't contradict ADR-019's "no declarator nodes": that's about declarator **syntax** (`*`, `[3]`, `()`), which
  still becomes a `Type` in the parser. A `DeclaratorNode` holds no syntax.

### The AST under this rule

| Type                      | Kind    | Contents                                                                                    |
|---------------------------|---------|---------------------------------------------------------------------------------------------|
| `TranslationUnitNode`     | node    | `range`; `head` → list of `ExternalDeclarationItem`                                         |
| `ExternalDeclarationItem` | wrapper | `FunctionDefinitionNode*`, or `DeclarationNode*` (slice 10); `next`                         |
| `FunctionDefinitionNode`  | node    | `range` (`int` to `}`); `DeclaratorNode*`; parameters → list of `DeclaratorItem`; `body` → `StatementNode*` (compound) |
| `DeclaratorNode`          | node    | `range`; `id` (from slice 6); `const Type*`; interned identifier id and its range           |
| `DeclaratorItem`          | wrapper | `DeclaratorNode*`; `initializer` → `ExpressionNode*` (`NULL` = none, always for parameters); `next` |
| `StatementNode`           | node    | `range`; compound (`head` → list of `BlockItem`) or return (`ExpressionNode*`)              |
| `BlockItem`               | wrapper | `StatementNode*`, or `DeclarationNode*` (slice 6); `next`                                   |
| `DeclarationNode`         | node    | `range`; `head` → list of `DeclaratorItem`                                                  |
| `ExpressionNode`          | node    | `range`; `id`; int constant (terminal), later unary/binary (`ExpressionNode*` each), call (slice 9: `callee` → `ExpressionNode*`, arguments → list of `ArgumentItem`) |
| `ArgumentItem`            | wrapper | `ExpressionNode*`; `next` (slice 9)                                                         |

### Consequences
- Drift in the code today, to fix before slice 6:
  - The renames above.
  - `ExternalDefinitionNode.as` holds the function definition by value: make it a pointer.
  - The `Range` moves from `ExternalDefinitionNode` down into the function definition.
  - `Declarator` held by value in `ExternalDefinitionFunction.declarator` and `InitDeclarator.declarator`: make it a
    `DeclaratorNode*`.
  - `ExternalDefinitionFunction.parameters` is an array plus `parameters_count`: make it a list of `DeclaratorItem`.
- Once the function definition has its own range, a function declarator's range can go back to covering the declarator
  only (`*f(void)`), instead of starting at the type specifier as the note in `Declaration.h` says today.
- One parse function and one serializer function per node type. `serialize_declaration` is shared by both list walkers,
  and one `DeclaratorItem` list walker serves both declarations and parameters.
- **Review check:** any of these is drift from this ADR:
  - a field whose type is a node or a wrapper, held by value;
  - a `Range` on a type whose name doesn't end in `Node`, or a `Node` type without a `Range`;
  - a wrapper with a `Range` or an `id`, or a wrapper whose name doesn't end in `Item`;
  - a `next` field in a node;
  - two wrappers with the same content (they should be one type);
  - a sequence that isn't a `next` list (an array plus a count).
