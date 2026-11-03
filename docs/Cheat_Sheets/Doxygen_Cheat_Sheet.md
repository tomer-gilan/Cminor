# Doxygen Cheat Sheet (C edition)

## ELI5: what is Doxygen?

Doxygen is a tool that reads **specially formatted comments** in your code and turns them into documentation: web pages, and the tooltip CLion shows you when you hover over a function (or press **Ctrl+Q**).

It works like labels on the drawers in a workshop. A normal comment is a sticky note only you will see. A Doxygen comment is a printed label with fixed slots ("what's inside", "handle with care", "who owns it") that a machine can read and show to anyone who opens the drawer.

The special formatting has two parts:
1. **The comment opener** (`/**` instead of `/*`) tells Doxygen "this comment is documentation".
2. **Commands** starting with `@` (or `\`, which means the same thing) fill in the slots: `@param`, `@return`, and so on.

> `@param` and `\param` are identical. Pick one style and stick to it. This sheet uses `@`.

---

## 1. Comment styles

| Style | Use for | Example |
|---|---|---|
| `/** … */` | A block comment **before** the thing it documents | see below |
| `///` | A one-line comment before the thing it documents | `/// Number of elements in the list.` |
| `///<` or `/**< … */` | A comment **after** a member, on the same line (for struct fields and enum values) | `int size; ///< number of used slots` |
| `/*! … */` / `//!` | Qt-style equivalents of the above | *(same thing, different spelling)* |

```c
/**
 * Creates a new, empty list.
 * @return a handle to the list. The caller owns it and must call list_destroy().
 */
List* list_create(void);

typedef struct {
    unsigned int line;    ///< 1-based line number
    unsigned int column;  ///< 1-based column number
} Position;

typedef enum {
    DIAGNOSTIC_SEVERITY_NOTE,    ///< informational, compilation continues
    DIAGNOSTIC_SEVERITY_ERROR,   ///< compilation fails at the end of the phase
    DIAGNOSTIC_SEVERITY_FATAL    ///< compilation stops immediately
} DiagnosticSeverity;
```

> ⚠️ `/*` (one star) and `//` are **ignored** by Doxygen. It has to be `/**` or `///`.

---

## 2. Brief vs. detailed description

| Command | Meaning |
|---|---|
| `@brief` | A one-sentence summary. It's what shows up in lists and tooltips. |
| `@details` | The longer explanation. (Optional: any paragraph after the brief counts as details.) |

```c
/**
 * @brief Peeks the character at an offset from the current position.
 *
 * Does not consume anything. Peeking past the end of the stream
 * returns the end-of-stream value instead of reading out of bounds.
 */
```

> 💡 With `JAVADOC_AUTOBRIEF = YES` in the config, **the first sentence becomes the brief automatically**, so you can skip writing `@brief`.

---

## 3. Function documentation

### `@param`: describes one parameter
Add a direction when it's not obvious: `[in]`, `[out]` or `[in,out]`.

```c
/**
 * Munches an identifier into a caller-provided buffer.
 * @param[in,out] stream  the stream to read from (advanced past the identifier)
 * @param[out]    buff    receives the identifier text, NUL-terminated
 * @param[in]     size    capacity of @p buff in bytes, including the terminator
 */
```

### `@return`: describes the return value

```c
 * @return the index of the added element
```

### `@retval`: describes one **specific** return value
Use it when some return values have special meanings.

```c
/**
 * Finds an element in the list.
 * @return the index of the first matching element
 * @retval -1 no element matched
 */
int list_index_of(const List* this, const void* element, ...);
```

### `@pre`: precondition (what the **caller** must guarantee)

```c
/**
 * @pre the opening quote has already been consumed from @p stream
 */
```

### `@post`: postcondition (what the **function** guarantees afterwards)

```c
/**
 * @post @p stream is positioned right after the closing quote
 */
```

### `@warning` / `@attention` / `@note`: highlighted paragraphs
They're ordered from most to least urgent:

| Command | When | Example |
|---|---|---|
| `@warning` | Ignoring this causes bugs (a leak, a crash, UB) | `@warning ownership of @p value transfers to the list; do not free it.` |
| `@attention` | Important, but not dangerous | `@attention peeking never advances the stream.` |
| `@note` | Background or a side detail | `@note C11 §5.2.4.1 requires at least 63 significant characters.` |

### `@see` / `@sa`: cross-reference ("see also")

```c
 * @see list_remove_at()
```

---

## 4. Inline formatting (inside a sentence)

| Command | Renders as | Use for | Example |
|---|---|---|---|
| `@p name` | `name` in code font | **Parameter names** (⚠️ CLion doesn't render it: it shows plain text and adds a line break, so use `@c` if you read docs in CLion) | `must be smaller than @p size` |
| `@c word` | `word` in code font | A single identifier or type | `returns @c NULL on failure` |
| `` `text` `` | `text` in code font | Any inline code (Markdown, Doxygen ≥ 1.8) | `` returns `LEXER_EOS` at the end `` |
| `@b word` | **word** | Bold (one word) | `@b never call this twice` |
| `@e word` / `@a word` | *word* | Italic (one word) | `the @e first match` |
| `func()` | a link to `func` | Automatic link: just write the name with `()` | `call list_destroy() when done` |
| `@ref name` | a link to `name` | An explicit link, e.g. to a type or group | `see @ref MunchClassification` |

> ⚠️ **`@code … @endcode` is NOT inline.** It opens a separate code block. Use `@p`, `@c` or backticks inside sentences.

---

## 5. Blocks

### `@code … @endcode`: a code example on its own lines
You can add a language hint for syntax colouring: `@code{.c}`.

```c
/**
 * Typical usage:
 * @code{.c}
 * List* list = list_create();
 * list_add(list, item);
 * list_destroy(list);
 * @endcode
 */
```

### `@verbatim … @endverbatim`: text shown exactly as written, with no command processing

```c
/**
 * Diagnostic output format:
 * @verbatim
 * file.c:3:14: error: unknown character '@'
 * @endverbatim
 */
```

### Lists (Markdown works)

```c
/**
 * Classification states:
 * - valid: the character belongs to the token
 * - invalid-continue: bad character, but keep munching
 * - terminate: the character ends the token
 *
 * Steps:
 * 1. skip whitespace
 * 2. record the start position
 * 3. dispatch on the first character
 */
```

---

## 6. Status and maintenance tags

Doxygen collects each of these into its own summary page (a "Todo List", a "Bug List", and so on).

| Command | Example |
|---|---|
| `@todo` | `@todo support hexadecimal constants` |
| `@bug` | `@bug unterminated literals loop forever` |
| `@deprecated` | `@deprecated use lexer_next() instead` |
| `@since` | `@since v0.2` |
| `@internal` | `@internal implementation detail, not part of the public API` (hidden unless `INTERNAL_DOCS = YES`) |

---

## 7. File-level documentation

Without a `@file` block, Doxygen **won't document free functions in that file** in the generated pages. That surprises a lot of people, and it matters in C, where almost everything is a free function.

```c
/**
 * @file Lexer.h
 * @brief Turns source text into a stream of tokens.
 * @author Tomer
 * @date 2026-09-22
 */
```

---

## 8. Grouping (optional, handy later)

This bundles related functions into a module page, e.g. "all List functions".

```c
/**
 * @defgroup list List
 * @brief A growable array of pointers.
 * @{
 */

List* list_create(void);
void  list_destroy(List* this);

/** @} */   // end of group "list"
```

Something in another file can join the group with `@ingroup list`.

---

## 9. Where do the docs go?

| Function kind | Document it in… |
|---|---|
| Public (declared in a `.h`) | the **header**, which is what users of the module read |
| Private (`static` in a `.c`) | the **`.c` file**, next to the definition |

Don't document the same function in both places: they'll drift apart.

---

## 10. Minimal config for a C project

Generate a config file once with `doxygen -g` (it creates a `Doxyfile`), then change these settings:

| Setting | Value | Why |
|---|---|---|
| `PROJECT_NAME` | `"Cminor"` | the title on the pages |
| `INPUT` | `git_code` | which folder to scan |
| `RECURSIVE` | `YES` | include subfolders |
| `OPTIMIZE_OUTPUT_FOR_C` | `YES` | C wording (e.g. "struct" instead of "class") |
| `JAVADOC_AUTOBRIEF` | `YES` | the first sentence becomes the brief |
| `EXTRACT_STATIC` | `YES` | also document `static` functions |
| `EXTRACT_ALL` | `NO` | only document what you actually documented |
| `WARN_IF_UNDOCUMENTED` | `YES` | warns about undocumented public things |
| `WARN_NO_PARAMDOC` | `YES` | warns about missing `@param`s |
| `GENERATE_LATEX` | `NO` | you only want HTML |
| `OUTPUT_DIRECTORY` | `docs` | where the output goes |

Then run `doxygen` and open `docs/html/index.html`.

> 💡 Even if you never generate HTML, the `WARN_*` options are useful: `doxygen` becomes a checker that flags `@param` names that don't match the real parameter names. That catches stale, copy-pasted docs.

---

## 11. CLion tips

> ⚠️ **CLion's Quick Documentation popup is not Doxygen.** It's JetBrains' own renderer and supports only a **subset** of commands ([CLion docs](https://www.jetbrains.com/help/clion/creating-and-viewing-doxygen-documentation.html); JetBrains tracks the unsupported list in [CPP-6723](https://youtrack.jetbrains.com/issue/CPP-6723)). Commands such as `@see`, `@sa` and `@ref` may show up raw or not at all in CLion, even though they're valid and work with the real `doxygen` tool. If a command looks broken in CLion, run `doxygen` to check whether it's your syntax or CLion. If you mostly read docs inside CLion, stick to `@brief`, `@param`, `@return`, `@note`, `@c` and backticks, and write cross-references as plain `func()` in the text. **Confirmed in practice:** CLion renders `@c` as inline code but **not `@p`**. Use `@c` for parameter names; Doxygen's HTML output looks the same either way.

- Type `/**` above a function and press **Enter**. CLion generates a stub with every `@param` and `@return` already filled in.
- **Ctrl+Q** (Quick Documentation) shows the rendered doc for whatever is under the cursor. It's the quickest way to check your formatting.
- CLion flags `@param` names that don't match the function signature (Settings → Editor → Inspections → "Documentation").

---

## 12. Common mistakes

| ❌ Mistake | ✅ Fix |
|---|---|
| `ownership of @code value @endcode is…` | `ownership of @p value is…` (Doxygen) or `ownership of @c value is…` (renders in CLion too) |
| `@param string string` (repeats the name) | Say something the signature doesn't: `@param string NUL-terminated keyword candidate` |
| `@return true iff …` on a function that returns an index | Keep `@return` in sync with the actual return type |
| An empty `/** */` block with no text | Delete it. An empty doc is noise. |
| "throws an error" in C docs | "issues a diagnostic" / "returns -1". C has no exceptions. |
| "@attention assumes X was already done" | That's a precondition: use `@pre` |
| A comment with `/*` (one star) | Doxygen ignores it: use `/**` |
| Documenting a function in both the `.h` and the `.c` | Pick one place (see §9) |
