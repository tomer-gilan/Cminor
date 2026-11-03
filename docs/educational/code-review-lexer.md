# Cminor Code Review: Lexer and Supporting Modules

*Reviewed 2026-09-27. Covers everything under `git_code/` (Lexer, Token, Diagnostic, DataStructures, Utils, Globals, main).*

**How I did this review:** I read every file, then traced concrete inputs through the code by hand. No C compiler was on the PATH in my environment, so I haven't *executed* anything. Every bug below comes with the input I traced so you can check it yourself. Line numbers refer to the files as they were when I read them.

**Severity tags used below:**
- 🔴 **Critical**: wrong output, crash, infinite loop, or doesn't build.
- 🟠 **Moderate**: wrong in some cases, or a design choice that will hurt soon.
- 🟡 **Minor**: nits, hygiene, style.

---

## TL;DR

1. **The architecture is basically right.** You have a character stream with peek/pop, a token-producing layer on top of it, a tagged-union `Token`, interned identifiers, and data-driven diagnostic tables. That's the same shape real hand-written lexers have. The foundation is sound.
2. **The code doesn't build right now, and several core paths are broken.** Your whitespace skipping eats the first character of the next token. Punctuator munching eats the character after every punctuator. Every valid `'a'` character literal hits your internal-state check and exits. `List` can't grow, so the first `list_add` corrupts the heap. And `'\n'` is encoded as `0x10` instead of `0x0A`. Details are in [§B.1](#b1-critical-bugs).
3. **Nearly all of these bugs would have shown up within minutes of having a tiny lexer test.** This is the single highest-leverage thing to add ([§B.3.1](#b31-no-tests-and-no-warnings-or-sanitizers)).
4. **On your MunchResult question (#2): yes, and I'd go further.** Drop the fixed-size buffers as well. Tokens can be *slices* of the source (a start offset plus a length). Most of the buffer-size bookkeeping, and a whole class of bugs, goes away with them.
5. **On exit(1) (#6):** separate *internal bugs* (assert/abort right away) from *user errors* (report, keep going, count them). The driver then stops at the end of each phase if the count is non-zero.
6. **Overall design rating: about 6/10.** Architecture is about 7–7.5, how individual pieces work is about 4.5–5. The rating, the repeating habits behind it, and what to focus on are in [Part C](#part-c-where-you-stand-design-only).

---

# Part A: Your 7 Points

## A.1 Diagnostics and positions

**Verdict: the direction is right, but I'd change the design in two ways.**

### (a) Every token should carry a position, including valid ones

You planned "a position field that may be `POSITION_EMPTY`, populated with the invalid location if the token is invalid". I think that's backwards. The lexer is *not* the main consumer of token positions. The parser and the semantic analysis are, and they mostly complain about **valid** tokens:

```c
int main() { return x; }   // 1:21: error: 'x' undeclared
```

`x` is a perfectly valid identifier token. The error belongs to the semantic analysis, and it needs `x`'s position. So: **every token always gets its start position** (and ideally its length). Positions of invalid input belong to the *diagnostic*, not to the token.

### (b) Record the start position *before* you munch

Right now, every diagnostic inside a muncher uses `this->position`. By the time you report, that's where the stream currently is, which is usually the *end* of the bad token (or past it). Look at [Lexer.c:176](git_code/Lexer.c#L176): the "out of bounds" error for `99999999999` points after the last digit. The standard pattern is to take `Position start = this->position;` at the beginning of each token and use `start` for both the token and any diagnostic about it.

### (c) Use 1-based lines and columns, then 0 becomes a free sentinel

`POSITION_EMPTY = {0, 0}` is also the position of the very first character of the file, so the two are indistinguishable. Every mainstream compiler reports lines and columns starting at 1, and if you do the same, `{0, 0}` can never be a real position and becomes a clean "no position" value.

### (d) Name the two concepts separately

Your `struct Diagnostic` in [Diagnostic.c:7](git_code/Diagnostic.c#L7) is really a *descriptor*: "code X has severity Y and message Z". An actual reported diagnostic is a different thing, an *instance*: "code X happened at position P, about lexeme L". Giving them different names makes the design much clearer:

```c
// illustrative, not a patch
typedef struct { DiagnosticSeverity severity; const char* message; } DiagnosticDescriptor; // the static table
typedef struct { DiagnosticCode code; Position pos; /* + extra text */ } Diagnostic;        // one occurrence
```

You don't need to *store* instances yet, since printing immediately is fine. But once you have the name, "what fields does a diagnostic need?" gets a clear answer.

### (e) Use the conventional output format

At the moment your output looks like this:

```
[ERROR]:3:4	LEXER: Invalid numerical sequence.
Additional information: 01234.
```

The GCC/Clang convention is `file:line:col: error: message`, all on one line:

```
foo.c:3:4: error: invalid numeric literal '01234'
```

That format isn't just tradition. CLion, VS Code and most terminals recognise it and turn it into a **clickable link** to the source location, so you'd get that for free. (The `LEXER:` prefix also helps you more than the user. Users don't care which phase caught the problem.)

> **ELI5: descriptor vs. instance.** A descriptor is the *definition* in a dictionary ("sneeze: to expel air suddenly"). An instance is *a particular sneeze*: who sneezed, when, and where. Your table is the dictionary. Right now nothing represents the individual sneeze.

---

## A.2 MunchResult

**Verdict: I agree, and I'd go further.**

The real problem isn't the struct itself. It's that **the decision "is this token valid?" gets made in 3–4 different places**, and diagnostics get issued at more than one level:

| Token kind | Where validity is decided |
|---|---|
| number | `classify_numeric` → `valid_munch` (then **ignored**) → `strtol` + `*end` check |
| char literal | muncher issues `UNEXPECTED_EOS` **and** caller issues `INVALID_CHARACTER_LITERAL_SEQUENCE` for the same error (double report) |
| word | `valid_munch` can never be false (`classify_word` has no INVALID state), yet the caller `exit(1)`s on it with no message ([Lexer.c:540](git_code/Lexer.c#L540)) |
| punctuator | muncher reports, then the caller reports again |

So the information that goes through `MunchResult` is partly redundant, partly ignored, and partly contradicted by the caller. That's the "wobbly" feeling you picked up on.

**The standard shape is one function per token kind, and it does everything for that kind:**

```c
// illustrative shape (every hand-written lexer, e.g. chibicc and Crafting Interpreters, looks like this)
static Token lex_number(Lexer* L, Position start);      // scan + validate + report + build
static Token lex_identifier(Lexer* L, Position start);
static Token lex_char_literal(Lexer* L, Position start);
...
Token lexer_next(Lexer* L) {
    skip_whitespace_and_comments(L);
    Position start = L->pos;
    int c = peek(L);
    if (c == EOF_CHAR)  return make_eof(start);
    if (is_digit(c))    return lex_number(L, start);
    ...
}
```

Each `lex_xxx` has exactly one job and one place where it can go wrong. When it hits an error, it reports it *once* (with `start`) and returns a best-effort token, or an error token (see [§A.6](#a6-when-to-exit1)).

**Going further:** drop the `buff[4096]` too. You already have the entire source in memory (`this->data`). A token can simply *point into it*:

```c
// illustrative
typedef struct {
    TokenCategory category;
    Position      pos;
    int           offset;   // index into source
    int           length;   // how many bytes
    union { ... } as;
} Token;
```

This gets rid of:
- all the fixed-size buffers and their off-by-one checks,
- the silent truncation of identifiers and strings (see [§B.2.4](#b24-translation-limits-are-minimums-not-maximums)),
- the `'\0'`-inside-a-string-literal problem (see [§B.1.9](#b19-c-strings-cannot-hold-string-literal-contents)),
- most of the reason `MunchResult` existed ("how many bytes did I write?").

The generic `maximal_munch_text` + classifier function pointer is a neat abstraction, and I can see what you were going for. But it's shared by only 2 of the 5 token kinds, and it forces the other kinds into a structure they don't fit. Here, a little duplication beats the wrong abstraction.

> **ELI5: slice / string view.** Instead of photocopying a sentence out of a book (copying into `buff`), you write down "page 12, starting at word 5, 3 words long". You haven't copied anything, nothing can overflow, and you can always go back and read the original.

---

## A.3 `List*`

**Verdict: having a generic dynamic array is absolutely the right call. A compiler lives on them. The implementation, though, has two critical bugs and a leak.**

### 🔴 It never grows, and it reallocs the wrong size ([DataStructures.c:19–22, 29–35](git_code/DataStructures.c#L19))

- `list_create` sets `capacity = 0`. `list_expand` does `capacity *= 2`, and **0 × 2 = 0**.
- `realloc(this->elements, this->capacity)` takes a **byte** count, but `capacity` is an **element** count. Even if capacity grew, you'd allocate 1/8th of what you need on a 64-bit system.
- Put together, the very first `list_add` writes `elements[0]` into a 0-byte allocation. That's a heap buffer overflow. The program might crash right away, much later, or never, which is what makes it nasty.

(Your comment that `malloc(0)` isn't UB is **correct**, good. It's *implementation-defined*: it may return `NULL` or a unique pointer. It just doesn't help here.)

### 🟠 `list_destroy` leaks ([DataStructures.c:37–40](git_code/DataStructures.c#L37))

It frees every element, but never frees `this->elements` or `this` itself.

### 🟠 The list decides how to destroy its elements

`list_destroy_at` calls plain `free()` on each element. That's only correct for elements that are a single `malloc` block with no owned pointers inside. The day a `List` holds structs that own other allocations (AST nodes, anyone?), `free` will leak everything they point to. The usual C fix is to let the *creator* of the list say how elements are destroyed:

```c
// illustrative
List* list_create(void (*element_destroy)(void*));   // NULL = "list doesn't own elements"
```

That also forces you to *decide* ownership explicitly, which ties into your point #5.

### 🟡 Smaller things
- ~~`list_at` takes `List*`, but it could take `const List*`.~~ *Corrected:* taking `List*` is a defensible choice. Because the list owns its elements, you're treating them as part of its state ("deep const"), and `list_index_of` already passes `const void*` to its compare callback, which fits that view. The cost is that read-only code holding a `const List*` can't call `list_at` at all. If that becomes annoying, add a second accessor, `const void* list_at_const(const List*, int)`. Note that with a by-value list (below), a non-const `List*` becomes *required*, because the returned pointer points into the list's own storage.
- `list_remove_at` has no bounds check, while `list_at` does. Be consistent.
- `list_add` doesn't call `list_verify_existing`, but every other function does. Honestly, I'd replace *all* of those with `assert(this != NULL)`. See [§A.6](#a6-when-to-exit1) for why.
- The doc for `list_at` says "Retrieves the size of the list", and `list_index_of` says "@return true iff". Both are copy-paste leftovers.
- `realloc`'s result is assigned straight back to `this->elements`. If it fails, you lose the old pointer. (Low priority. See the `xmalloc` note in [§B.3.6](#b36-no-policy-for-malloc-failure).)

### Design option: store by value instead of `void*`

Right now each token gets its own `malloc` ([Lexer.c:553](git_code/Lexer.c#L553)) just so it can live in a `void*` list. For small value types like `Token`, a list that stores elements **by value** is simpler, faster, and has no ownership question at all:

```c
// illustrative
List* list_create(size_t element_size);
void  list_push(List* l, const void* element);   // memcpy's element_size bytes in
void* list_at(List* l, int i);                   // returns pointer into internal array
```

Either design is legitimate. `void*` lists are great for "a list of heap objects I own". A by-value list is great for "a growable array of small structs". Many compilers use both.

---

## A.4 Code documentation

**Verdict: I agree that obvious functions don't need docs. The problem isn't missing docs, it's that some docs are wrong or empty.**

Docs that are **wrong** are worse than docs that are **missing**, because the reader trusts them. Examples:
- `list_at`: "Retrieves the size of the list".
- `list_index_of`: "@return true iff element exists".
- All four munchers say "@return the number of bytes written", but they return `MunchResult`.
- The character-literal muncher says "we save only the first 63 bytes", but the constant used is 4096.
- The `peek_escaped` docs point to `lexer_is_synthetically_escaped`, which doesn't exist.
- `lexer_stream_create` calls the source text "the lexeme". A lexeme is *one token's* text, not the whole file.

Empty doc blocks like `@param string string` / `@param c character` are noise. Either say something the signature doesn't already say, or delete them.

**A rule of thumb:** document the **contract** and the **why**, not the **what**. For example, "assumes the opening quote was already consumed", "caller owns the returned list and its tokens", "returns -1 if not found". Your best comments already do this. The `MunchClassification` rationale, the "why `skip` loops over `pop`" note, and the `malloc(0)` note are all *why*-comments, and they're genuinely useful.

---

## A.5 Pointer ownership

**Verdict: you're thinking about it, and you document transfers on `list_add` / `list_remove_at`. That's more than most people do at this stage. Here's a framework and an audit.**

> **ELI5: ownership.** Every `malloc`'d block has exactly one **owner**, the piece of code that's responsible for eventually `free`ing it. Everyone else is just **borrowing**: they can look, but they must not free it and must not keep it after the owner frees it. Ownership can be **transferred**, like handing someone the keys ("here, it's yours now").

### A simple convention that answers 90% of ownership questions
| Pattern | Meaning |
|---|---|
| `const T*` parameter | **Borrowed.** The callee reads it and doesn't keep it past the call (or copies it if it needs to). |
| `T* x_create(...)` return | **Caller owns.** They must call `x_destroy`. |
| Function documented "takes ownership" | The caller must not touch or free it afterwards. |
| Pointer into another object's internals (e.g. `list_at`) | **Borrowed**, and only valid until that object changes or dies. |

Once you follow a convention, you only need to *document the exceptions*.

### Audit of the current code
| Where | Status |
|---|---|
| `list_add` / `list_remove_at` | ✅ Documented transfer in/out. Good. |
| `list_destroy` | 🔴 Leaks the array and the struct (see [§A.3](#a3-list)). |
| `list_destroy_at` | 🟠 Assumes the list owns elements and that `free` is enough. |
| `lexer_stream_create` | ✅ **Copies** the source (good, the stream owns its copy), but 🟠 copies with `strcpy`, which ignores `size`. See [§B.2.6](#b26-lexer_stream_create-ignores-size-when-copying). Undocumented. |
| `lexer_stream_extract_all_tokens` | 🟠 Returns a list of heap tokens, and ownership isn't documented. |
| Token factories | ✅ Copy the incoming string into the table. Good: the caller keeps its buffer. |
| `globals_*_table` | 🔴 Nobody creates them and nobody destroys them (see [§B.1.1](#b11-it-doesnt-build)). |
| `Diagnostic.info` | 🟡 Declared `char*` but points at string literals, so it should be `const char*`. |

### An honest industry note
Many real compilers (chibicc, for example) **never call `free`**. A compiler is a short-lived process, and the OS reclaims everything on exit. Others use **arenas**: one big allocation per phase, freed all at once. That's a perfectly respectable strategy, especially for AST nodes later. Since this is a learning project, I'd keep your current discipline for the lexer (it's good practice). But when you get to the parser, look up *arena allocation*. It will save you a lot of `_destroy` functions.

> **ELI5: arena.** Instead of borrowing individual books from the library and returning each one, you rent a whole shelf, put everything on it, and return the shelf when you're done. There's nothing to track individually.

---

## A.6 When to `exit(1)`

**Verdict: "exit on FATAL" is right, but you're mixing up two different kinds of failure.**

### Separate these three things

| Kind | Example | What to do |
|---|---|---|
| **Internal bug** (the compiler is broken) | index out of bounds in `List`, uninitialised table, "impossible" state in `extract_token` | `assert` / `abort()` **immediately**. It's *your* bug, not the user's. Don't dress it up as a user-facing diagnostic. |
| **Fatal user or environment error** | can't open the input file, out of memory | Report, then `exit(1)`. You can't continue at all. |
| **Normal user error** | `09`, `'ab'`, `@`, unterminated string | Report, **recover**, keep going, count it. |

Right now `INDEX_OUT_OF_BOUNDS`, `UNINITIALIZED_TABLE` and `INVALID_INTERNAL_STATE` are *diagnostic codes*, which means you're reporting your own bugs to the user as if they were the user's errors. Real compilers call these **ICEs** (Internal Compiler Errors) and handle them separately.

### Don't `exit` from deep inside library code

`list_verify_existing` calls `exit(1)`. So does `lexer_stream_extract_token` in five places. That makes the code impossible to unit-test, because the first bad input kills the test runner. It also scatters "should we stop?" decisions all over the codebase.

### The simple, common policy

1. `diagnostic_issue` itself increments an error counter. For `FATAL` it exits, so callers never have to remember to.
2. The **lexer never exits**. On an error, it reports once, skips the bad characters, and returns an error token (or just the next good token).
3. The **driver** checks the counter between phases:

```c
// illustrative driver shape
List* tokens = lex(source);
if (diagnostic_error_count() > 0) return 1;   // don't parse garbage
Ast*  ast    = parse(tokens);
if (diagnostic_error_count() > 0) return 1;
...
```

**Multiple errors vs. first error:** reporting all lexer errors is *easy* (just skip the bad character or token and continue), so do it. Parser error recovery is harder (the classic approach is **panic mode**: on an error, skip tokens until a `;` or `}` and resume). It's completely reasonable to **stop at the first parse error** at first and add recovery later.

> **ELI5: error recovery.** When you're proofreading an essay and find a typo, you mark it and keep reading. You don't throw the essay away at the first mistake. But if a whole paragraph is gibberish, you skip to the next paragraph (panic mode) instead of marking every word.

---

## A.7 `byte` vs. `char`

**Verdict: this is a real, well-known C friction point, not a sign you did something wrong. Here's how to resolve it cleanly.**

### Why the tension exists
- `char`'s signedness is **implementation-defined**. On x86 with GCC/MSVC it's *signed*, so the byte `0xFF` as a `char` is `-1`.
- libc's string functions (`strcmp`, `printf("%s")`, `strtol`) take `char*`.
- Raw bytes are most naturally `unsigned char`.
- Passing `byte*` to a `const char*` parameter is a **pointer-sign mismatch**. That's the warning CLion showed you (e.g. passing `buff`, a `byte[]`, to `token_identifier_create(const char*)` and `diagnostic_issue_string`).

### A clean resolution
1. **Store and pass source text as `const char*`** (the source *is* text).
2. **When you inspect a single character, convert to `unsigned char` / `int` at that moment** (e.g. `(unsigned char)src[i]`). That's the same rule `<ctype.h>` imposes.
3. **For "a byte or end-of-stream", return `int`, exactly like `getc`**. Bytes come back as 0–255 and EOS as `-1`, which is *outside* the byte range. That kills the `0xFF` sentinel problem (see [§B.2.1](#b21-in-band-signalling-structural-msb-and-0xff-as-eos)).
4. **For diagnostics, write one tiny "printable escape" helper** that shows a byte the way a human expects. Printing a raw `'\n'` or `0x07` into a terminal is garbled or invisible:

```
error: unknown character '\x07'
error: invalid escape sequence '\q'
```

With those four rules, `byte` mostly disappears from your API, and the casts live in exactly two places (peek and the printer).

> **ELI5: implementation-defined.** The C standard says "the compiler author picks, but must write down the choice". Whether `char` can go negative is one of those choices. On most PCs it can, which is why `0xFF` turns into `-1` behind your back.

---

# Part B: Other Points You Didn't Cover

## B.1 Critical bugs

### B.1.1 It doesn't build
- 🔴 **[main.c:8–10](git_code/main.c#L8)** still uses `Error`, `error_create`, `error_print`, `error_destroy` and `ERROR_CODE_…`, none of which exist anymore. (The include guard in [Diagnostic.h:5](git_code/Diagnostic.h#L5) is still `GIT_CODE_ERROR_H` from the same rename.)
- 🔴 **[DataStructures.c:43, 75](git_code/DataStructures.c#L43)** pass `{.line = 0, .column = 0}` as a function argument. A bare brace initializer isn't an expression in C, so this needs the type in front (a *compound literal*), or just use `POSITION_EMPTY`. Note that [Diagnostic.c:44](git_code/Diagnostic.c#L44) does it right.
- 🔴 **[Token.h:108–114](git_code/Token.h#L108)** declares the token factories `static` in the header.
  > **ELI5: `static` on a function = "private to this .c file".** The header gets pasted into `Lexer.c`, so `Lexer.c` now promises that it has its own private `token_keyword_create`. It never defines one. `Token.c` has its own private copy, which nobody can see. Result: an "used but never defined" warning, and then an "undefined reference" link error.
- 🔴 **[Globals.h:10, 12](git_code/Globals.h#L10)** defines variables in a header (`List* globals_identifiers_table;`, with no `extern`). Every `.c` file that includes it creates its own definition. On GCC ≥ 10 (the default `-fno-common`) that's a "multiple definition" **link error**. The header should *declare* (`extern`), and exactly one `.c` should *define*. On top of that, **nothing ever creates the tables** (your TODO #4 knows this), so they're `NULL`. Once the build works, the first identifier will immediately hit `UNINITIALIZED_TABLE` → `exit(1)`.
  > **ELI5: declaration vs. definition.** A declaration says "there exists a variable called X somewhere". A definition says "and here it is, this is where it lives". You can say the first thing many times, but the second thing only once.

### B.1.2 Whitespace skipping eats the first character of the next token ([Lexer.c:504–505](git_code/Lexer.c#L504))
`pop` returns the character it *just consumed*, so the loop consumes one character too many:

```
input: "int x"
after "int":  stream at ' '
c = peek  → ' '
c = pop   → ' '   (stream now at 'x')
c = pop   → 'x'   (stream now at EOS)  ← 'x' consumed!
c == 'x' → munch_word starts at EOS → empty identifier ""
```

So every token that follows whitespace loses its first character. Also, the EOS and structural checks happen *before* this loop, so a file that ends in whitespace or a newline (almost every file!) reaches the end of the function with `c == 0xFF` and reports `UNKNOWN_CHARACTER`.

### B.1.3 Punctuator munching eats the next character ([Lexer.c:377–390](git_code/Lexer.c#L377))
The loop `pop`s a character, checks whether the buffer is still a punctuator, and if not, removes it **from the buffer**. But it's already gone **from the stream**:

```
input: "a+b"
'+'  popped → "+"  is a punctuator, continue
'b'  popped → "+b" is not → buff = "+", but 'b' is consumed
next token starts at EOS → 'b' is lost
```

`x=1;` loses the `1`, and `((` loses the second `(`. The fix is conceptual: **peek** to decide whether the next character extends the token, and only pop when it does. (Side note: "keep extending while the prefix is still a punctuator" works for your subset. It fails in full C for `...`, because `..` isn't a punctuator, so keep that in mind if you ever add varargs.)

### B.1.4 Every valid character literal causes an exit ([Lexer.c:433–465](git_code/Lexer.c#L433), [513–516](git_code/Lexer.c#L513))
Trace `'a'`:
- `first = 'a'` → `buff[0] = 'a'`, index = 1.
- The loop pops the closing `'` (structural) → **stores it** at `buff[1]` → index = 2 → loop ends.
- The result is `bytes_munched = 2`, `valid_munch = true`.
- The caller then reports: `valid_munch && bytes_munched != 1` → `INVALID_INTERNAL_STATE` → `exit(1)`.

The same "store the closing delimiter" mistake exists in the string muncher ([Lexer.c:415–417](git_code/Lexer.c#L415)): `"abc"` gets interned as `abc` + byte `0xA7` (a structural `"`).

### B.1.5 Wrong escape values ([Lexer.c:288–299](git_code/Lexer.c#L288))
`\n` → `0x10`, `\v` → `0x11`, `\f` → `0x12`, `\r` → `0x13`. Those are **decimal** 10–13 written with a **hex** prefix. The correct values are `0x0A`, `0x0B`, `0x0C`, `0x0D`. (`\a`, `\b`, `\t` happen to be right because 7, 8 and 9 are the same in both bases.) Every `printf("hi\n")` compiled by Cminor would print a DLE control character instead of a newline.

### B.1.6 Unterminated literal → infinite loop with uninitialised memory ([Lexer.c:256–262](git_code/Lexer.c#L256))
When `peek_escaped_internal` sees EOS (`0xFF`, which has the MSB set), it takes the "structural character" branch, issues a diagnostic, and **returns 0 without writing `*ptr`**. So:
- `pop_escaped` skips 0 bytes (no progress), and
- returns an **uninitialised** `byte` (undefined behaviour), so the `lexer_char_is_EOS(current)` check in the muncher is comparing garbage.

Input `"abc` (no closing quote) most likely loops forever, printing a diagnostic each time. The same thing happens for `"abc\` (a backslash at EOF) and for the `default:` branch of the escape `switch` (invalid escape: `*ptr` is never set).

### B.1.7 Unknown character → infinite loop ([Lexer.c:546–547](git_code/Lexer.c#L546))
`@`, `$`, `#`, a backtick, or `\r` (see [§B.2.3](#b23-carriage-returns-windows-line-endings)): the lexer reports, returns `TOKEN_EMPTY`, and **doesn't consume the character**. `extract_all_tokens` sees "not EOF", `malloc`s a token, and calls again on the same character, forever, allocating until the machine runs out of memory.

### B.1.8 String literals go into the identifier table ([Token.c:24](git_code/Token.c#L24))
`token_string_literal_create` *searches* `globals_string_literals_table` but *adds to* `globals_identifiers_table`. It's a copy-paste slip. The returned id points into the wrong table.

### B.1.9 C strings can't hold string-literal contents
`"a\0b"` is a legal 3-character C string literal (4 bytes including the terminator). Your muncher writes `a`, `0`, `b` into `buff`, then it's passed around as a NUL-terminated `char*`, so everything sees just `"a"`. Also, `'\0'` as a character literal makes `buff` look empty in diagnostics. **Literal contents need to be stored as (pointer, length)**, which slices give you for free ([§A.2](#a2-munchresult)).

### B.1.10 `diagnostic_issue` can recurse forever ([Diagnostic.c:38–46](git_code/Diagnostic.c#L38))
`diagnostic_is_valid_code` checks `0 < code`, but code **0** is `DIAGNOSTIC_CODE_DIAGNOSTIC_UNKNOWN_DIAGNOSTIC_CODE`, the very code the fallback path issues. So an invalid code → issue code 0 → 0 is "invalid" → issue code 0 → … → stack overflow. Your own comment says "a bit funny, but could theoretically happen", and it would. 🙂

---

## B.2 Moderate issues

### B.2.1 In-band signalling: structural MSB, and `0xFF` as EOS
I want to give credit first: **you correctly identified a real problem.** Inside a string, you need to tell "the closing `"`" apart from "an escaped `\"`", and a plain `char` can't express that. Setting the high bit to mark "this is the structural delimiter" does solve it.

But it's **in-band signalling**: you're squeezing extra meaning into the same 8 bits that carry the data. The costs:
- `0xFF` (EOS) *also* has the high bit set, so EOS is simultaneously "the structural version of `0x7F`". That's exactly how bug [§B.1.6](#b16-unterminated-literal--infinite-loop-with-uninitialised-memory) happens.
- It bans every non-ASCII byte in the source. Deliberately, per your TODO, and that's fine for now. But it means a UTF-8 `é` inside a *comment* or *string* is fatal, even though C allows it there.
- Every caller has to remember to call `lexer_is_structural_version_of` instead of `==`.

**Out-of-band alternatives:** return an `int` (as in [§A.7](#a7-byte-vs-char)), where values ≥ 256 or < 0 mean "special". Or return a tiny struct `{ int ch; bool is_terminator; }`. Or the simplest option: have the string-lexing loop check `peek() == '"'` *before* decoding escapes. A `"` seen at that point is always the terminator, because escapes are decoded only after a `\`.

> **ELI5: in-band signalling.** Imagine agreeing that the word "STOP" in a telegram means "end of message". It works, until someone needs to send a message that contains the word "stop". Out-of-band means using a *separate* channel for control signals, like a light that turns on when the message ends.

### B.2.2 Number parsing uses `strtol(…, 0)`, which auto-detects the base ([Lexer.c:168](git_code/Lexer.c#L168))
Base `0` means "guess from the prefix". Your SUBSET.md says **base 10 only**, but:
- `0x1F` is munched fully (`x` and `F` are `INVALID_CONTINUE`, but `valid_munch` is ignored) and `strtol` happily returns **31**, so hex is silently supported.
- `010` returns **8** (octal).
- `09` gives "invalid suffix '9'".

For what it's worth, real C *does* treat leading `0` as octal. So pick one deliberately and document it: follow C (octal and hex are supported), or reject leading zeros. Right now it's accidental.

Also:
- The out-of-range diagnostic passes `end` (which is `""` there) instead of `string` ([Lexer.c:176](git_code/Lexer.c#L176)), so the "additional info" is empty.
- After an "invalid suffix" error, the function continues and returns a number.
- **A classic gotcha to know about:** `-2147483648` isn't a literal in C. It's unary minus applied to `2147483648`, which doesn't fit in `int`. With an int-only subset, `INT_MIN` can't be written as a literal. It's not a bug, just something you'll hit eventually.

### B.2.3 Carriage returns (Windows line endings)
[`lexer_is_char_parsing_indifferent`](git_code/Lexer.c#L93) accepts `' '`, `'\t'` and `'\n'`, but not `'\r'` (or `'\v'`, `'\f'`). **You develop on Windows.** Any test `.c` file saved with CRLF line endings produces an "unknown character" on every line, followed by the infinite loop from [§B.1.7](#b17-unknown-character--infinite-loop).

### B.2.4 Translation limits are *minimums*, not maximums
`LEXER_MIN_IDENTIFIER_DIFFERENTIATING_LENGTH` and `LEXER_MIN_STRING_LITERAL_DIFFERENTIATION_LENGTH` come from C11 §5.2.4.1, which lists what a conforming compiler **must at least** support. It's a floor, not a ceiling.
- **Identifiers:** the standard *does* allow ignoring characters past the 63rd ("significant initial characters"), so truncating is technically legal. But no modern compiler does it, and it silently merges two distinct names.
- **String literals:** truncating at 4095 **silently changes the program**, which is much worse than rejecting it.

With slices ([§A.2](#a2-munchresult)), you get no limits at all and never need these constants.

### B.2.5 Buffer contracts are violated (off-by-one)
[`maximal_munch_text`](git_code/Lexer.c#L339) writes at most `buff_size` bytes, **then** `buff[index] = '\0'` → one byte past the end. With the "`buff64`" contract, a 64-character identifier writes `buff[64]`. It doesn't crash today only because the real buffer is 4096 bytes. (Your TODO #6 already suspects this.)

### B.2.6 `lexer_stream_create` ignores `size` when copying
It takes a `size` but copies with `strcpy` (which stops at the first `'\0'`). If the file contains a NUL byte, `size` stays large while the copy is short, so `peek_at` reads past the allocation. Copy exactly `size` bytes with `memcpy` (and add a terminator if you like).

### B.2.7 Private details are in public headers
- [Lexer.h](git_code/Lexer.h) contains `static` tables, `static` helper *definitions*, the `MunchResult` typedef, and `#include <errno.h>`. All of that is implementation detail and belongs in `Lexer.c`.
- The **public** API of a lexer can be tiny: `create`, `destroy`, `next_token` (and maybe `tokenize_all`). `peek`, `pop`, `skip` and `peek_escaped` are internal machinery. Exposing them invites the parser to reach into the character stream.
- Headers should include only what their *declarations* need. `Diagnostic.h` pulls in `stdio`, `stdlib` and `string` for its users.

### B.2.8 Keyword and punctuator tables rely on manual ordering ([Lexer.h:20–37](git_code/Lexer.h#L20))
They're positional arrays that must match the enum order by hand. Insert an enum member and forget the table, and every lookup silently shifts by one. You already know the better pattern, since you use it in `Diagnostic.c`:

```c
// illustrative
static const char* const keywords[] = {
    [TOKEN_KEYWORD_ID_CHAR] = "char",
    [TOKEN_KEYWORD_ID_ELSE] = "else",
    ...
};
_Static_assert(sizeof keywords / sizeof keywords[0] == TOKEN_KEYWORD_ID__COUNT,
               "keyword table out of sync with enum");
```

(Leave the explicit `[COUNT]` size off, or the assert is trivially true.) One related trap: in `diagnostics_table`, a *missing* designated entry silently becomes `{severity = NOTE, info = NULL}`, and `printf("%s", NULL)` is UB. A startup self-check loop, or a unit test that asserts every entry has non-`NULL` `info`, catches that.

### B.2.9 Using `-1` as a sentinel for an enum
`lexer_get_keyword_id_by_string` returns `-1` through the type `TokenKeywordID`. An enum's underlying type is implementation-defined (GCC picks `unsigned int` when no enumerators are negative), so `id != -1` only works because of how the conversions happen to line up, and some compilers warn about it. Options: add an explicit `TOKEN_KEYWORD_ID_NONE = -1` enumerator, or return `bool` with an out parameter.

### B.2.10 `TokenPunctuator` has a redundant `category` ([Token.h:91–94](git_code/Token.h#L91))
The outer `Token` already has `category`. The copy inside the punctuator member is never set (the factory sets only `id`) and is inconsistent with the other token types.

### B.2.11 Diagnostic hygiene
- `Diagnostic.info` should be `const char*`.
- The `UNINITIALIZED_TABLE` message starts with `"LEXER:"`, but it's raised from `List` code.
- There's no error counter (see [§A.6](#a6-when-to-exit1)).
- `FATAL` severity has no *behaviour* attached. The callers decide ad hoc whether to exit.

---

## B.3 Things missing that matter

### B.3.1 No tests, and no warnings or sanitizers
**This is the single most important recommendation in this document.** Almost every critical bug above would show up in a 30-line test like this:

```c
// illustrative test idea: lex a string, compare categories/ids
expect_tokens("int x;",    KW_INT, IDENT, SEMI, END);
expect_tokens("a+b",       IDENT, PLUS, IDENT, END);
expect_tokens("'a'",       CHAR_CONST('a'), END);
expect_tokens("\"hi\\n\"", STRING("hi\n"), END);
expect_tokens("x \n",      IDENT, END);           // trailing whitespace
```

Also, turn the compiler into a bug-finder for you:
- **Warnings:** `-Wall -Wextra -Wpedantic` (and consider `-Wconversion`) through `target_compile_options` in CMake. Several of the issues above (static-in-header, pointer-sign, sign-compare, uninitialised use) are exactly what these flags catch.
- **Sanitizers:** `-fsanitize=address,undefined` on your WSL/Linux build. ASan would have pointed straight at the `List` overflow and the `buff[64]` write. UBSan flags the uninitialised reads.

> **ELI5: sanitizers.** The compiler adds invisible guards around every memory access. When your program touches memory it shouldn't, it stops *right there* and tells you the file and line, instead of silently corrupting something and crashing 10 minutes later somewhere unrelated.

### B.3.2 Comments aren't lexed
`// hello` currently lexes as `/`, `/`, then identifiers. Comments aren't a preprocessor feature: C replaces them with a space in translation phase 3, before tokens exist. You'll want them as soon as you write real test programs. They fit naturally into "skip whitespace", as "skip whitespace **and comments**".

### B.3.3 The punctuator table is missing things your subset needs
- **`,`** isn't in the table. You can't lex `int f(int a, int b)`, and the subset has functions with parameters.
- **`->`** is listed in SUBSET.md but not in the table (fine if it's deferred along with structs, but make the two agree).

### B.3.4 No EOF token at the end of the token list
`extract_all_tokens` stops *before* adding the EOF token. The parser will be much simpler if the list always ends with an `EOF` token. Then "peek the next token" never runs off the end, and you don't need bounds checks everywhere in the parser.

> **ELI5: sentinel.** It's like putting a "THE END" card after the last page. The reader never has to ask "is there another page?", because they just stop when they see the card.

### B.3.5 Global tables vs. a context object
Global identifier and string tables make testing painful: state leaks between tests, and nobody clearly owns them. The common alternative is a `Lexer` (or `CompilationContext`) struct that *owns* the tables and is passed around. It's a small change now and a bigger one later. There's also a layering smell: `Token.c` (a plain data type) now depends on global compiler state. Interning is really the *lexer's* job, which matches your instinct in point #2.

### B.3.6 No policy for `malloc` failure
No `malloc`, `realloc` or `strcpy`-copy result is checked. For a compiler, the standard idiom is one `xmalloc` / `xrealloc` wrapper that aborts with a message on failure. That's one check, done once, and never thought about again.

### B.3.7 A driver
`main` doesn't read a file or run the lexer yet. A minimal `main(argc, argv)` that reads a file, lexes it, and **dumps the tokens** (one per line, with positions) is a great debugging tool, and it's what your test harness will call.

### B.3.8 Small nits
- `int` / `unsigned` mixing in loops (`for (int i = 0; i < offset; …)` with `unsigned offset`) causes sign-compare warnings.
- `Position` lives in `DataStructures.h`. It's really a source-location concept, so it would fit better near diagnostics.
- `list_string_strcmp` sits in `Utils` but has a `list_` prefix.
- `lexer_char_to_digit` is unused.
- In `Lexer.h`, the Globals include is redundant (Token.h already includes it).
- `sizeof(char)` is always 1 by definition. That's harmless, just so you know.
- `this` as a parameter name is legal C, but it's a C++ keyword, so some tooling (and any future C++ test harness) will choke on it. Keep it if you like it.
- CMake: the project and target are called `git_code`. You might want a real name (e.g. `cmm`).

---

## B.4 Things you got right

This isn't padding, it's real. These are good decisions, and several of them are the *hard* part to get right:

- ✅ **The layering is right.** A character stream (`LexerStream` with `peek`, `peek_at`, `pop`, `skip`) under a token producer is exactly how it should be structured. `peek_at(offset)` is the right primitive for multi-character lookahead.
- ✅ **Opaque pointers** for `LexerStream` and `List` give you real encapsulation in C. Most beginners expose every struct.
- ✅ **A tagged-union `Token`** is the correct representation, and the `.as.` naming reads nicely.
- ✅ **Identifier interning** (a table of unique names, with tokens holding an id) is a classic compiler technique. It makes later symbol-table comparisons cheap. You got there without being told.
- ✅ **Data-driven diagnostics:** an enum + `__COUNT` sentinel + designated-initializer table is clean and extensible. Keep it.
- ✅ **Line and column tracking lives in exactly one place** (`pop`), and `skip` goes through `pop` *on purpose* to keep that invariant. That's genuine invariant-thinking, and the comment explains the reasoning.
- ✅ **"Keep munching past the error so the diagnostic can show the whole lexeme"** (the `MunchClassification` rationale) is a thoughtful UX idea. The mechanism needs simplifying, but the *goal* is exactly what good compilers do.
- ✅ **The lexer doesn't handle negative numbers.** `-5` is `MINUS` then `5`, which is correct. Plenty of first lexers get this wrong.
- ✅ **SUBSET.md defined against Annex A** of the standard is an excellent practice. Having a written contract of what you support is rare and valuable.
- ✅ **Consistent `module_verb_noun` naming** and `const`-correct parameters across the codebase.
- ✅ **You spotted the escaped-quote-vs-terminating-quote problem** yourself. The fix you chose has costs (see [§B.2.1](#b21-in-band-signalling-structural-msb-and-0xff-as-eos)), but seeing the problem is the hard part.
- ✅ **Your TODO list** already names three of the issues here (buffer overflow, globals init, 7-bit ASCII). Your instincts for where the problems are are good.

---

## B.5 Suggested order of work

This is a suggestion, not a prescription. You decide what to take.

1. **Make it build**, and turn on `-Wall -Wextra` plus ASan/UBSan in CMake.
2. **Write the tiny token-stream test harness** ([§B.3.1](#b31-no-tests-and-no-warnings-or-sanitizers)) and a token-dump `main`.
3. **Fix `List`** (growth, byte count, destroy). Everything sits on top of it.
4. **Restructure the lexer** around "one `lex_xxx` function per token kind, with start position captured up front, `int`-returning peek, and slices instead of buffers". That one change resolves your points #1, #2 and #7, plus bugs B.1.2–B.1.9 and B.2.1–B.2.5.
5. **Adopt the error policy** from [§A.6](#a6-when-to-exit1): asserts for internal bugs, an error counter, and the driver checking it between phases.
6. **Then** move on to the parser, with an EOF sentinel token at the end of the list.

---

# Part C: Where You Stand (Design Only)

*Added after a follow-up question. This part ignores simple leaks and one-off bugs and rates only the design choices. Problems you flagged yourself count as half-problems.*

**Overall: about 6/10 for the design of a first compiler's front end.** The big picture is solid. The weak spots are in how individual pieces work internally, and most of them come from a few repeating habits, not from many unrelated mistakes. Habits are easier to fix than scattered mistakes.

| Level | Rating | Why |
|---|---|---|
| **Architecture** (what the modules are and how they connect) | **7–7.5/10** | Character stream under a token producer, opaque types, a tagged-union `Token`, identifier interning, table-driven diagnostics, and a written subset spec. That's the shape real compilers have, and you got there without a template. |
| **Mechanisms** (how each piece does its job) | **4.5–5/10** | This is where the unflagged problems are. |

## C.1 Your self-assessment: good at symptoms, weaker at root causes

You flagged 7 things, and all of them are real. That's good judgment. But several of them are **symptoms** of problems you *didn't* flag:

| What you flagged (symptom) | The actual cause |
|---|---|
| "byte vs. char tension" | Using `0xFF` as EOS and the high bit as a "closing delimiter" flag, packed into the same byte as the data ([§B.2.1](#b21-in-band-signalling-structural-msb-and-0xff-as-eos)) |
| "MunchResult is wobbly" | Copying tokens into fixed-size buffers (so you have to report how many bytes were written), and deciding validity in 3–4 places ([§A.2](#a2-munchresult)) |
| "When to exit?" | Internal bugs going through the same diagnostic system as user errors ([§A.6](#a6-when-to-exit1)) |

You can tell when something feels wrong, and that instinct is worth a lot. The next step is asking *why* it feels wrong, and tracing it back to the decision that caused it.

## C.2 The repeating habits (unflagged, so they count as full problems)

**1. Special values that overlap valid values.** This is the biggest one, and it shows up everywhere:
- `0xFF` means EOS, but it's also a byte value.
- A set high bit means "closing delimiter", but it also means "non-ASCII character".
- `-1` is returned through an enum type that has no `-1` member.
- `POSITION_EMPTY` is `{0,0}`, which is also the first character of the file.
- `TOKEN_EMPTY` is returned as if it were a real token, which is what causes the infinite loop.

The rule: **a special state should be impossible to confuse with valid data.** Put it outside the valid range (`int` with `-1`, like `getc`), give it its own named enum member, or use a separate flag. Many of the bugs in Part B come from this one habit.

**2. Deciding the same thing in several places.** Whether a token is valid is decided by the classifier, the muncher, the caller, and `strtol`. Diagnostics are sometimes reported twice. `exit(1)` is called from five places plus `List`. When a decision lives in several places, those places drift apart. **Every decision should have a single owner.**

**3. Abstracting too early.** The generic `maximal_munch_text` with a classifier function pointer was designed before the concrete cases were written. Only 2 of the 5 token kinds fit it, and `MunchResult` exists partly to cover the gap. A useful rule: write it concretely twice, and abstract the third time, once you can see what the cases actually have in common.

**4. Unclear boundaries: what's public and who owns what.** `Lexer.h` exposes internal machinery (peek, pop, the tables, `static` definitions). The string tables are global, and `Token.c` depends on that global state. None of this breaks anything *yet*, but boundaries decide how painful the parser will be to write and test.

**5. Writing a lot before running anything.** This is a process issue, not a design issue, but it's the most important one. The lexer has never run end to end: `main` doesn't lex anything, and the tables are never created. Most of the critical bugs in §B.1 would have shown up the first time you ran `int x;` through it. Work in **thin end-to-end slices**: get `int x;` fully lexing and tested, then add the next feature.

## C.3 What to focus on, in priority order

1. **Fast feedback:** a tiny test harness, compiler warnings, and sanitizers, and build in thin slices. This multiplies everything else.
2. **Out-of-band special values:** make the states you don't want impossible to represent.
3. **One owner per decision:** one place decides validity, one place reports each error, one place decides to exit.
4. **Deliberate boundaries:** a small public API, private internals, and state owned by an object instead of globals.
5. **Concrete first, abstract later.**

## C.4 Calibration

A lot of people's first lexer is one 600-line function with a giant `switch`, globals everywhere, and no thought about ownership or diagnostics. You're well above that: you think about invariants, encapsulation, and user-facing error quality, and you write down *why* you made choices. What's missing isn't talent or effort. It's the habits above, and those come from writing code, running it, and seeing what breaks.

The parser will test you much harder on habits #2 and #4. If you fix the lexer with these habits in mind first, the parser will go noticeably smoother.

---

Happy to go deeper on any of these: slice-based tokens, how panic-mode recovery works, arena allocators, or how to structure the test harness.
