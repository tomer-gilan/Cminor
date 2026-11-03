
The following is the formal definition of the C standard features that are supported by the Cminor compiler.
Note that these features are categorized based on their respective definitions in the [C standard](https://www.iso-9899.info/n1570.html#A.).
Implementation-defined choices (sizes, signedness, evaluation order, ...) are in [`implementation-defined.md`](implementation-defined.md).
# A. \[Annex A (informative) Language syntax summary]
## A.1 \[Lexical grammar]
### A.1.2 \[Keywords]
char
else
if
int
return
sizeof
void
while
**Note: the only types are `int` and `char` (both signed), `void`, and pointers and arrays built from them.**
`void` is allowed only as a function's return type, as the `(void)` parameter list, and as the pointee of `void *`.
### A.1.3 \[Identifiers]
no change
### A.1.5 \[Constants]
integer-constant
* only in base 10.
character-constant
* including escaped characters
### A.1.6 \[String literals]
* no prefixes/suffixes.
* including escaped characters
### A.1.7 \[Punctuators]
punctuator: one of
	 \[] () {} , &
	 * + - ~  ! / %
	 << >> > <
	 <= >= == !=
	 ^  | && ||
	 ; =
**Notable Left Outs:**
	in-place arithmetic operations, e.g. +=, /=, etc.
	increment and decrement, ++ and -- (v2 candidate)
	ternary expression (condition ? true : false)
	... (ellipsis, variadic functions)
	ALL PREPROCESSOR PUNCTUATORS - no support for the preprocessor
### Comments and other lexical rules
* Comments: both `/* ... */` and `// ...` are in. Each comment becomes one space (translation phase 3).
* **Left out:** backslash-newline line splicing, adjacent string literal concatenation (`"a" "b"`).
### A.1.8 \[Header names]
Unimplemented, as it is a part of the unimplemented preprocessor
### A.1.9 \[Preprocessing numbers]
Unimplemented, as it is a part of the unimplemented preprocessor
## A.2 \[Phrase structure grammar]
Decided 2026-10-13. This section is the parser's **scope**. The trimmed EBNF itself is still to be written (parser
roadmap, Step 0b).
### A.2.1 \[Expressions]
Supported, from tightest to loosest binding:
* primary: identifier, integer-constant, character-constant, string-literal, `( expression )`
* postfix: array subscript `a[i]`, function call `f(a, b)`. `i[a]` is legal, as in C.
* unary: `- + ! ~ * &`, and `sizeof` *unary-expression*
* multiplicative `* / %`, additive `+ -`, shift `<< >>`, relational `< > <= >=`, equality `== !=`, `&`, `^`, `|`, `&&`, `||`
* assignment `=` (right-associative)
* *expression* is a single *assignment-expression*: there is no comma operator.

Rules:
* **`sizeof`, expression form only:** `sizeof x`, `sizeof *p`, `sizeof a[0]`. `sizeof(int)` is a syntax error
  (`sizeof (x)` is fine: it's `sizeof` applied to a parenthesized expression). The operand is never evaluated, and an
  array operand doesn't decay (`int a[3]; sizeof a` is 12). The result's type is `int`, not `size_t` (see
  `implementation-defined.md`).
* **No `--` or `++` spelled without a space.** Two `-` tokens (or two `+` tokens) whose ranges touch, as in `--x` or
  `a--b`, are a syntax error: in C they'd be the decrement/increment operator, so accepting them would make a Cminor
  program mean something different in C. `- -x` with a space is fine.
* Pointer arithmetic: pointer ± int only. Pointer − pointer is not supported (see `implementation-defined.md`).
* `void *` converts implicitly to and from any other object pointer type. It can't be dereferenced or used in arithmetic.

**Notable Left Outs:** comma operator, `sizeof(type-name)`, casts, `?:`, compound assignment, `++`/`--` (v2 candidate), `.` and `->`,
`_Generic`, compound literals, and constant expressions (nothing in v1 needs constant folding: array sizes and global
initializers are single tokens, see A.2.2).
### A.2.2 \[Declarations]
Supported:
* type specifier: exactly one of `int`, `char`, `void`.
* declarator: zero or more `*`, an identifier, then either
  * zero or more `[N]`, where `N` is a single integer-constant token greater than 0 (`int a[2][3]` is in), or
  * `( parameter-list )` for a function.
* **Multiple declarators:** `int a, *p, b[3];`. Each declarator has its own `*`s: in `int *p, q;`, `q` is an `int`.
* parameter-list:
  * `(void)`: no parameters.
  * `()` means the same as `(void)` (C23's rule, not C11's "no prototype").
  * a list of `type declarator`, separated by `,`. Parameter names are optional in prototypes (`int f(int, char *);`)
    and required in definitions.
* **Initializers**, `= ...` after a declarator:
  * locals: any expression.
  * globals: an integer constant or a character constant, optionally preceded by one `-`, or a string literal (for a
    `char *`). `int g = 1 + 2;` and `int *gp = &x;` are errors. Globals without an initializer are zero.
* Declarations can appear anywhere in a block (C99), but never as the body of an `if` or `while` (see A.2.3).

**Notable Left Outs:** storage-class specifiers (`static`, `extern`, `typedef`, `auto`, `register`), type qualifiers
(`const`, `volatile`, `restrict`), `struct`/`union`/`enum`, parenthesized declarators (`int (*p)[3]`, function
pointers), brace initializers (`int a[2] = {1, 2};`, `char s[] = "hi";`), unsized arrays (`int a[];`), array parameters
(`int f(int a[])`: write `int *a`), array sizes that aren't a single constant (`int a[2 + 1];`, VLAs), `...` in
parameter lists, designators, `_Static_assert`, `_Alignas`.
### A.2.3 \[Statements]
Supported:
* compound statement `{ ... }`: block items, each a declaration or a statement, in any order.
* expression statement `expr;`, and the null statement `;`.
* `if ( expr ) stmt` and `if ( expr ) stmt else stmt`. `else` binds to the nearest `if`.
* `while ( expr ) stmt`.
* `return expr;` and `return;`.

A declaration is a block item, not a statement, so `if (x) int y;` is a syntax error.

**Notable Left Outs:** labeled statements, `switch`, `for`, `do`-`while`, `goto`, `break`, `continue`.
### A.2.4 \[External definitions]
* A translation unit is one or more function definitions and declarations (global variables, prototypes), in any order.
* Function definitions use prototype-style parameters only (no K&R definitions). No nested functions.
* One translation unit per compilation.
* Calling libc: programs are linked against libc and declare what they call by hand, since there's no `#include`,
  e.g. `int putchar(int c);` or `void *malloc(int size);` (`int` stands in for `size_t`, see `implementation-defined.md`).
  Only non-variadic functions can be declared, since there's no `...`: `printf` is a v2 candidate.
## v2 candidates (optional)
Out of v1, but planned as optional additions for v2:
* full `sizeof`: `sizeof(type-name)`.
* casts: `(int)c`, `(char *)p`. (Both this and `sizeof(type-name)` need an abstract-declarator parser.)
* constant folding, and with it constant expressions as array sizes and as global scalar initializers
  (`int g = 1 + 2;`).
* brace initializers: `int a[2] = {1, 2};`, `char s[] = "hi";`, and unsized arrays whose size comes from the initializer.
* `...` in prototypes only, to call variadic libc functions such as `printf` (codegen sets `%al` before the call).
  Defining variadic functions stays out.
* `++` and `--`, prefix and postfix: `++i`, `i--`. The lexer gets `++` and `--` tokens (maximal munch, so `a+++b` is
  `a ++ + b`), and they replace v1's "no `--` or `++` spelled without a space" rule.
