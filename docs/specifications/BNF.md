# C11 Grammar, Annex A.2

Written by Claude on 2026-10-13, from N1570 (the C11 draft), Annex A.2 "Phrase structure grammar".

Every phrase-structure rule of the standard, one row per alternative, with the slice that implements it (parser
roadmap Step 0b).

**How to read it**

- Each `###` heading is a rule's left side. Each row is one alternative of its right side.
- Links are nonterminals, defined in this file. *Italic* names are A.1 tokens that the lexer produces: *identifier*,
  *constant*, *string-literal*, *enumeration-constant*.
- `code` is a terminal: a keyword or punctuator, spelled exactly as written.
- <sub>opt</sub> means the symbol before it is optional.
- An alternative that starts with its own rule (`additive-expression + …`) is left-recursive. In recursive descent it
  becomes a loop, not a call.
- **SUBSET.md says** comes from `SUBSET.md`: ✅ in, ❌ out, ⚠️ partly, v2 = listed as a v2 candidate.
- **slice** is every slice (from the slice table in `parser-roadmap.md`) that adds to the row, one color per digit:
  0 ⚪, 1 ⚫, 2 🟡, 3 🟢, 4 🔴, 5 🟠, 6 🩶, 7 🟤, 8 🔵, 9 🟨. So ⚫🟡#12 is slice 12. A pass-through row gets the first
  slice where both its levels exist. Empty means not in Cminor v1.
- Cminor's lexer has no token for `.`, `->`, `++`, `--`, `?`, `:`, `...` or any compound assignment. `x -= 1` lexes as
  `x - = 1`, and `p->x` as `p - > x`.

## A.2.1 Expressions

### primary-expression

The atoms of an expression: names, literals, and parenthesized expressions.

| rule                            | example     | gotchas                                                                                                          | SUBSET.md says | slice       |
| ------------------------------- | ----------- | ---------------------------------------------------------------------------------------------------------------- | -------------- | ----------- |
| *identifier*                    | `x`         | Whether `x` is declared is sema's question, not the parser's.                                                    | ✅              | 🩶#6        |
| *constant*                      | `42`, `'a'` | In A.1, *constant* also covers floating and enumeration constants. Cminor has only integer and character constants. | ✅              | ⚫#1, ⚫🟢#13 |
| *string-literal*                | `"hi"`      | Its type is `char[N]`, which decays to `char *` in most uses.                                                    | ✅              | ⚫🟢#13      |
| `(`[expression](#expression)`)` | `(a + b)`   | The parentheses only shape the tree: no node of their own is needed.                                             | ✅              | 🟡#2        |
### postfix-expression

An operand followed by suffix operators: subscripts, calls, member access.

| rule                                                                                                                  | example           | gotchas                                                                                                                                         | SUBSET.md says | slice  |
| --------------------------------------------------------------------------------------------------------------------- | ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- | -------------- | ------ |
| [primary-expression](#primary-expression)                                                                             | `x`               | Pass-through: no node of its own.                                                                                                               | ✅              | 🟡#2   |
| [postfix-expression](#postfix-expression) `[` [expression](#expression) `]`                                           | `a[i]`, `m[1][2]` | Left-recursive: suffixes loop after the primary, so `m[1][2]` is `(m[1])[2]`. `i[a]` is legal: the grammar doesn't say which side is the array. | ✅              | ⚫🟡#12 |
| [postfix-expression](#postfix-expression) `(` [argument-expression-list](#argument-expression-list)<sub>opt</sub> `)` | `f()`, `f(a, b)`  | The callee is any postfix-expression, not just a name. What's callable is sema's decision.                                                      | ✅              | 🟨#9   |
| [postfix-expression](#postfix-expression) `++`                                                                        | `i++`             | v1 lexes it as two `+`, and the touching rule makes `i++` a syntax error. v2 adds the `++` token. | v2 |        |
| [postfix-expression](#postfix-expression) `--`                                                                        | `i--`             | v1 lexes it as two `-`, and the touching rule makes `a--b` a syntax error instead of silently `a - (-b)`. v2 adds the `--` token. | v2 |        |
### argument-expression-list

The comma-separated arguments of a function call.

| rule                                                                                                      | example       | gotchas                                                                                                                             | SUBSET.md says | slice |
| --------------------------------------------------------------------------------------------------------- | ------------- | ----------------------------------------------------------------------------------------------------------------------------------- | -------------- | ----- |
| [assignment-expression](#assignment-expression)                                                           | `f(a)`        | Arguments are assignment-expressions, not expressions: that's why `,` separates arguments here instead of being the comma operator. | ✅              | 🟨#9  |
| [argument-expression-list](#argument-expression-list) `,` [assignment-expression](#assignment-expression) | `f(a, b = 1)` | Left-recursive. The order in which arguments are evaluated is unspecified in C.                                                     | ✅              | 🟨#9  |
### unary-expression

An operand with prefix operators, and `sizeof`.

| rule                                                                  | example                   | gotchas                                                                                                                                     | SUBSET.md says | slice       |
| --------------------------------------------------------------------- | ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- | -------------- | ----------- |
| [postfix-expression](#postfix-expression)                             | `a[i]`                    | Pass-through.                                                                                                                               | ✅              | 🟡#2        |
| `++` [unary-expression](#unary-expression)                            | `++i`                     | v1 lexes it as two `+`, and the touching rule makes `++i` a syntax error. v2 adds the `++` token. | v2 |             |
| `--` [unary-expression](#unary-expression)                            | `--i`                     | v1 lexes it as two `-`, and the touching rule makes `--x` a syntax error; `- -x` is fine. v2 adds the `--` token. | v2 |             |
| [unary-operator](#unary-operator) [cast-expression](#cast-expression) | `-x`, `*p`, `!*p`         | The operand is a cast-expression so that `-(int)c` works. Without casts it's the same as a unary-expression. Right-recursive: `- -x` nests. | ✅              | 🟡#2, ⚫⚫#11 |
| `sizeof` [unary-expression](#unary-expression)                        | `sizeof x`, `sizeof a[0]` | `sizeof (x)` is this alternative: `(x)` is a primary-expression. The operand isn't evaluated, and arrays don't decay.                       | ✅              | ⚫🔴#14      |
| `sizeof` `(` [type-name](#type-name) `)`                              | `sizeof(int)`             | After `sizeof (`, the next token decides: a type keyword means this alternative. In Cminor v1 it's a syntax error.                             | v2             |             |


### unary-operator

The six prefix operator tokens.

One of:

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| `&` | `&x`, `&a[0]` | The operand must be an lvalue (sema). | ✅ | ⚫⚫#11 |
| `*` | `*p` | Same token as multiplication. Position decides: where an operand is expected, it's unary. | ✅ | ⚫⚫#11 |
| `+` | `+c` | Not a no-op on types: it promotes, so `+c` is an `int`. | ✅ | 🟡#2 |
| `-` | `-x` | `-2147483648` is `-` applied to `2147483648`, which doesn't fit in an `int`. | ✅ | 🟡#2 |
| `~` | `~x` | | ✅ | 🟡#2 |
| `!` | `!x` | The result is an `int`, 0 or 1. | ✅ | 🟡#2 |

### cast-expression

An explicit type conversion: `(type) operand`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [unary-expression](#unary-expression) | `x` | Pass-through. | ✅ | 🟡#2 |
| `(` [type-name](#type-name) `)` [cast-expression](#cast-expression) | `(char)x` | `(` starts both a cast and a parenthesized expression; the next token decides (a type keyword means a cast). With `typedef`, deciding that would need the symbol table. | v2 |  |

### multiplicative-expression

`*`, `/` and `%`, the tightest-binding binary operators.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [cast-expression](#cast-expression) | `x` | Pass-through. | ✅ | 🟢#3 |
| [multiplicative-expression](#multiplicative-expression) `*` [cast-expression](#cast-expression) | `a * b` | Left-recursive: `a * b * c` is `(a * b) * c`. | ✅ | 🟢#3 |
| [multiplicative-expression](#multiplicative-expression) `/` [cast-expression](#cast-expression) | `a / b` | Truncates toward zero. Division by 0 and `INT_MIN / -1` are UB. | ✅ | 🟢#3 |
| [multiplicative-expression](#multiplicative-expression) `%` [cast-expression](#cast-expression) | `a % b` | The sign follows the dividend: `-7 % 2` is `-1`. | ✅ | 🟢#3 |

### additive-expression

Binary `+` and `-`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [multiplicative-expression](#multiplicative-expression) | `a * b` | Pass-through. | ✅ | 🟢#3 |
| [additive-expression](#additive-expression) `+` [multiplicative-expression](#multiplicative-expression) | `a + b`, `p + 1` | Pointer + int is scaled by the pointee's size. | ✅ | 🟢#3 |
| [additive-expression](#additive-expression) `-` [multiplicative-expression](#multiplicative-expression) | `a - b`, `p - 1` | | ⚠️ no pointer − pointer | 🟢#3 |

### shift-expression

Bit shifts: `<<` and `>>`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [additive-expression](#additive-expression) | `a + b` | Pass-through. | ✅ | 🔴#4 |
| [shift-expression](#shift-expression) `<<` [additive-expression](#additive-expression) | `1 << n` | Shifting by a negative count, or by the type's width or more, is UB. | ✅ | 🔴#4 |
| [shift-expression](#shift-expression) `>>` [additive-expression](#additive-expression) | `x >> 1` | `>>` on a negative value is implementation-defined (see `implementation-defined.md`). | ✅ | 🔴#4 |

### relational-expression

Ordering comparisons: `<`, `>`, `<=`, `>=`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [shift-expression](#shift-expression) | `1 << n` | Pass-through. | ✅ | 🟠#5 |
| [relational-expression](#relational-expression) `<` [shift-expression](#shift-expression) | `a < b` | `a < b < c` is `(a < b) < c`: it compares 0 or 1 with `c`. | ✅ | 🟠#5 |
| [relational-expression](#relational-expression) `>` [shift-expression](#shift-expression) | `a > b` | | ✅ | 🟠#5 |
| [relational-expression](#relational-expression) `<=` [shift-expression](#shift-expression) | `a <= b` | | ✅ | 🟠#5 |
| [relational-expression](#relational-expression) `>=` [shift-expression](#shift-expression) | `a >= b` | | ✅ | 🟠#5 |

### equality-expression

Equality comparisons: `==` and `!=`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [relational-expression](#relational-expression) | `a < b` | Pass-through. | ✅ | 🟠#5 |
| [equality-expression](#equality-expression) `==` [relational-expression](#relational-expression) | `a == b` | Binds looser than `<`: `a < b == c < d` is `(a < b) == (c < d)`. | ✅ | 🟠#5 |
| [equality-expression](#equality-expression) `!=` [relational-expression](#relational-expression) | `p != 0` | A literal `0` is a null pointer constant, so comparing a pointer with it is fine (sema). | ✅ | 🟠#5 |

### AND-expression

Bitwise AND: `&`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [equality-expression](#equality-expression) | `a == b` | Pass-through. | ✅ | 🟠#5 |
| [AND-expression](#AND-expression) `&` [equality-expression](#equality-expression) | `x & 1` | Binds looser than `==`: `x & 1 == 0` is `x & (1 == 0)`. A classic bug. | ✅ | 🔴#4 |
### exclusive-OR-expression
Bitwise XOR: `^`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [AND-expression](#AND-expression) | `x & 1` | Pass-through. | ✅ | 🔴#4 |
| [exclusive-OR-expression](#exclusive-OR-expression) `^` [AND-expression](#AND-expression) | `a ^ b` | | ✅ | 🔴#4 |
### inclusive-OR-expression
Bitwise OR: `|`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [exclusive-OR-expression](#exclusive-OR-expression) | `a ^ b` | Pass-through. | ✅ | 🔴#4 |
| [inclusive-OR-expression](#inclusive-OR-expression) `\|` [exclusive-OR-expression](#exclusive-OR-expression) | `a \| b` | | ✅ | 🔴#4 |
### logical-AND-expression
Short-circuit logical AND: `&&`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [inclusive-OR-expression](#inclusive-OR-expression) | `a \| b` | Pass-through. | ✅ | 🟠#5 |
| [logical-AND-expression](#logical-AND-expression) `&&` [inclusive-OR-expression](#inclusive-OR-expression) | `a && b` | Short-circuits: codegen needs a jump, not an AND instruction. | ✅ | 🟠#5 |
### logical-OR-expression
Short-circuit logical OR: `||`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [logical-AND-expression](#logical-AND-expression) | `a && b` | Pass-through. | ✅ | 🟠#5 |
| [logical-OR-expression](#logical-OR-expression) `\|\|` [logical-AND-expression](#logical-AND-expression) | `a \|\| b` | Short-circuits, like `&&`. | ✅ | 🟠#5 |
### assignment-expression
Assignment, the loosest-binding operator except the comma.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [conditional-expression](#conditional-expression) | `a + b` | Pass-through. | ✅ | 🟡#2 |
| [unary-expression](#unary-expression) [assignment-operator](#assignment-operator) [assignment-expression](#assignment-expression) | `x = y = 0`, `a[i] = 1`, `*p = c` | The parser only knows it's an assignment once it sees `=`. Parse a conditional-expression, and if `=` follows, check that it's an lvalue. Right-recursive: `x = (y = 0)`. | ✅ | 🩶#6 |
### assignment-operator
`=` and the compound assignment tokens.

| rule  | example   | gotchas                                                 | SUBSET.md says | slice |
| ----- | --------- | ------------------------------------------------------- | -------------- | ----- |
| `=`   | `x = 1`   |                                                         | ✅              | 🩶#6  |
### expression

A full expression, which adds the comma operator.

| rule                                                                          | example        | gotchas                                                                                   | SUBSET.md says | slice |
| ----------------------------------------------------------------------------- | -------------- | ----------------------------------------------------------------------------------------- | -------------- | ----- |
| [assignment-expression](#assignment-expression)                               | `x = 1`        |                                                                                           | ✅              | 🟡#2  |
## A.2.2 Declarations
### declaration

Introduces names (variables, functions) and their types.

| rule                                                                                                              | example          | gotchas                                                                                                                            | SUBSET.md says | slice                   |
| ----------------------------------------------------------------------------------------------------------------- | ---------------- | ---------------------------------------------------------------------------------------------------------------------------------- | -------------- | ----------------------- |
| [declaration-specifiers](#declaration-specifiers) [init-declarator-list](#init-declarator-list)<sub>opt</sub> `;` | `int a, *p = 0;` | With the list omitted, `int;` is legal C (a useless declaration, usually a warning). SUBSET.md doesn't say whether Cminor accepts it. | ✅              | 🩶#6,<br>🟨#9,<br>⚫⚪#10 |


### declaration-specifiers

The words before the declarators: type, storage class, qualifiers.

| rule                                                                                                        | example           | gotchas                                                                                                              | SUBSET.md says | slice |
| ----------------------------------------------------------------------------------------------------------- | ----------------- | -------------------------------------------------------------------------------------------------------------------- | -------------- | ----- |
| [type-specifier](#type-specifier) [declaration-specifiers](#declaration-specifiers)<sub>opt</sub>           | `int`             | In C, `unsigned long int` is three specifiers combined. In Cminor there's exactly one, so the recursion is never taken. | ⚠️ exactly one | ⚫#1   |
### init-declarator-list

The comma-separated declarators of one declaration.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [init-declarator](#init-declarator) | `a` in `int a;` | | ✅ | 🩶#6 |
| [init-declarator-list](#init-declarator-list) `,` [init-declarator](#init-declarator) | `int a, *p;` | Each declarator has its own `*`s: in `int *p, q;`, `q` is an `int`. The type belongs to each declarator, not to the declaration. | ✅ | 🩶#6 |

### init-declarator

One declarator, with an optional initializer.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [declarator](#declarator) | `*p` | | ✅ | 🩶#6 |
| [declarator](#declarator) `=` [initializer](#initializer) | `x = 5` | The grammar allows any expression. Cminor's limit on global initializers is a check, not a grammar rule. | ✅ | 🩶#6, ⚫⚪#10 |

### type-specifier

The base type: `int`, `char`, a struct, a typedef name, and so on.

| rule                                                    | example                     | gotchas                                                                                                                                                           | SUBSET.md says | slice     |
| ------------------------------------------------------- | --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- | --------- |
| `void`                                                  | `void f(void);`, `void *p;` | Only as a return type, in `(void)`, and in `void *`. `void x;` is a sema error.                                                                                   | ✅              | ⚫#1, 🟨#9 |
| `char`                                                  | `char c;`                   | Plain `char` is a different type from `signed char`; whether it's signed is implementation-defined.                                                               | ✅              | ⚫🟢#13    |
| `int`                                                   | `int x;`                    |                                                                                                                                                                   | ✅              | ⚫#1       |
### specifier-qualifier-list
The type words of a member or a type-name, without storage classes.

| rule                                                                                                  | example        | gotchas                                                                               | SUBSET.md says     | slice |
| ----------------------------------------------------------------------------------------------------- | -------------- | ------------------------------------------------------------------------------------- | ------------------ | ----- |
| [type-specifier](#type-specifier) [specifier-qualifier-list](#specifier-qualifier-list)<sub>opt</sub> | `unsigned int` | Declaration-specifiers without storage classes. Also used by [type-name](#type-name). | v2 (via type-name) |       |
### declarator
The name being declared, wrapped in the `*`, `[]` and `()` that build its type.

| rule                                                                      | example                      | gotchas                                                                                                | SUBSET.md says | slice            |
| ------------------------------------------------------------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------------ | -------------- | ---------------- |
| [pointer](#pointer)<sub>opt</sub> [direct-declarator](#direct-declarator) | `**argv`, `a[3]`, `f(int x)` | `*` binds looser than `[]` and `()`: `int *a[3]` is an array of 3 pointers, not a pointer to an array. | ✅              | ⚫#1, 🩶#6, ⚫⚫#11 |
### direct-declarator
A declarator without its leading `*`s: the name and its `[]` and `()` suffixes.

| rule                                                                                                                                                                    | example                        | gotchas                                                                                                                                     | SUBSET.md says               | slice     |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------- | --------- |
| *identifier*                                                                                                                                                            | `x`                            |                                                                                                                                             | ✅                            | ⚫#1, 🩶#6 |
| [direct-declarator](#direct-declarator) `[` [type-qualifier-list](#type-qualifier-list)<sub>opt</sub> [assignment-expression](#assignment-expression)<sub>opt</sub> `]` | `a[3]`, `m[2][3]`              | Left-recursive: suffixes loop. In Cminor the size is required, and is one integer-constant token greater than 0.                               | ⚠️ `[N]` only                | ⚫🟡#12    |
| [direct-declarator](#direct-declarator) `(` [parameter-type-list](#parameter-type-list) `)`                                                                             | `f(int x, char *s)`, `f(void)` | The grammar allows `f(void)[3]` (a function returning an array) and `a[3](void)`: both are errors that a check must catch, not the grammar. | ✅                            | ⚫#1, 🟨#9 |
| [direct-declarator](#direct-declarator) `(` [identifier-list](#identifier-list)<sub>opt</sub> `)`                                                                       | `f()`, K&R `f(a, b)`           | In C11, `()` means "no prototype" (K&R style). Cminor accepts only the empty `()` and treats it as `(void)` (C23's rule).                      | ⚠️ `()` only, means `(void)` | 🟨#9      |
### pointer
One or more `*`s, each with optional qualifiers.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| `*` [type-qualifier-list](#type-qualifier-list)<sub>opt</sub> | `*p` | No qualifiers in Cminor, so it's just `*`. | ✅ | ⚫⚫#11 |
| `*` [type-qualifier-list](#type-qualifier-list)<sub>opt</sub> [pointer](#pointer) | `**pp` | Right-recursive, or a loop that counts the `*`s. | ✅ | ⚫⚫#11 |
### parameter-type-list
A function's parameters, with an optional trailing `...`.

| rule                                        | example                | gotchas                                                     | SUBSET.md says | slice     |
| ------------------------------------------- | ---------------------- | ----------------------------------------------------------- | -------------- | --------- |
| [parameter-list](#parameter-list)           | `int a, char *b`       |                                                             | ✅              | ⚫#1, 🟨#9 |
| [parameter-list](#parameter-list) `,` `...` | `const char *fmt, ...` | Variadic. v2 plans it in prototypes only, to call `printf`. | v2             |           |
### parameter-list
The comma-separated parameters.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [parameter-declaration](#parameter-declaration) | `int a` | | ✅ | ⚫#1, 🟨#9 |
| [parameter-list](#parameter-list) `,` [parameter-declaration](#parameter-declaration) | `int a, int b` | Unlike declarations, every parameter has its own type: `int f(int a, b)` is a syntax error. | ✅ | 🟨#9 |

### parameter-declaration

One parameter: a type and an optional name.

| rule                                                                                                        | example                 | gotchas                                                                                                                                                                                                             | SUBSET.md says     | slice            |
| ----------------------------------------------------------------------------------------------------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------ | ---------------- |
| [declaration-specifiers](#declaration-specifiers) [declarator](#declarator)                                 | `int x`, `char *s`      |                                                                                                                                                                                                                     | ✅                  | 🟨#9             |
| [declaration-specifiers](#declaration-specifiers) [abstract-declarator](#abstract-declarator)<sub>opt</sub> | `int`, `char *`, `void` | Unnamed parameters. `(void)` is this alternative on its own, and it means "no parameters" only as the sole parameter: `(void, int)` is an error. After `char *`, an *identifier* switches to the alternative above. | ⚠️ prototypes only | ⚫#1, 🟨#9, ⚫⚫#11 |
### type-name
A type written without a name, as in casts and `sizeof(type)`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [specifier-qualifier-list](#specifier-qualifier-list) [abstract-declarator](#abstract-declarator)<sub>opt</sub> | `int`, `char *`, `int (*)[3]` | A declaration without the name. Used by casts, `sizeof(type)`, `_Alignof`, `_Generic`, compound literals, `_Atomic(…)` and `_Alignas(…)`. | v2 |  |
### abstract-declarator
A declarator without the name: only `*`, `[]` and `()`.

| rule                                                                                        | example                 | gotchas                                      | SUBSET.md says        | slice |
| ------------------------------------------------------------------------------------------- | ----------------------- | -------------------------------------------- | --------------------- | ----- |
| [pointer](#pointer)                                                                         | `*` in `int f(char *);` | Cminor needs it for unnamed pointer parameters. | ⚠️ unnamed parameters | ⚫⚫#11 |
### initializer
The value after `=` in a declaration.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [assignment-expression](#assignment-expression) | `= 5`, `= x + 1` | An assignment-expression, not an expression, so `int a = 1, b;` declares two variables. | ⚠️ globals: one constant | 🩶#6, ⚫⚪#10 |
| `{` [initializer-list](#initializer-list) `}` | `= {1, 2}` | | v2 |  |
| `{` [initializer-list](#initializer-list) `,` `}` | `= {1, 2,}` | Trailing comma. | v2 |  |

### initializer-list

The comma-separated values inside an initializer's braces.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [designation](#designation)<sub>opt</sub> [initializer](#initializer) | `1`, `[2] = 5` | v2 plans brace initializers without designators. | v2 (no designation) |  |
| [initializer-list](#initializer-list) `,` [designation](#designation)<sub>opt</sub> [initializer](#initializer) | `1, 2` | | v2 (no designation) |  |
## A.2.3 Statements

### statement

Any statement, by kind.

| rule                                          | example                | gotchas | SUBSET.md says | slice |
| --------------------------------------------- | ---------------------- | ------- | -------------- | ----- |
| [compound-statement](#compound-statement)     | `{ x = 1; }`           |         | ✅              | 🟤#7  |
| [expression-statement](#expression-statement) | `x = 1;`               |         | ✅              | 🩶#6  |
| [selection-statement](#selection-statement)   | `if (x) y = 1;`        |         | ✅              | 🟤#7  |
| [iteration-statement](#iteration-statement)   | `while (x) x = x - 1;` |         | ✅              | 🔵#8  |
| [jump-statement](#jump-statement)             | `return 0;`            |         | ✅              | ⚫#1   |
### compound-statement
A block: `{ … }`, with its own scope.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| `{` [block-item-list](#block-item-list)<sub>opt</sub> `}` | `{}`, `{ int x; x = 1; }` | Opens a new scope. A function body shares its scope with the parameters, so `int f(int a) { int a; }` is an error. | ✅ | ⚫#1, 🟤#7 |
### block-item-list
The items inside a block, in order.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [block-item](#block-item) | `x = 1;` | | ✅ | ⚫#1 |
| [block-item-list](#block-item-list) [block-item](#block-item) | `int x; x = 1;` | Left-recursive: loop until `}`. | ✅ | 🩶#6 |
### block-item
One item in a block: a declaration or a statement.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [declaration](#declaration) | `int x = 1;` | The first token decides: a type keyword starts a declaration, anything else a statement. That's this easy only because there's no `typedef`. | ✅ | 🩶#6 |
| [statement](#statement) | `x = 1;` | | ✅ | ⚫#1 |
### expression-statement
An expression evaluated for its side effects, or an empty `;`.

| rule                                        | example      | gotchas                                               | SUBSET.md says | slice |
| ------------------------------------------- | ------------ | ----------------------------------------------------- | -------------- | ----- |
| [expression](#expression)<sub>opt</sub> `;` | `f(x);`, `;` | With the expression omitted, it's the null statement. | ✅              | 🩶#6  |
### selection-statement
Branching: `if`, `if`-`else`, `switch`.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| `if` `(` [expression](#expression) `)` [statement](#statement) | `if (x) y = 1;` | The body is a statement, never a declaration: `if (x) int y;` is a syntax error. | ✅ | 🟤#7 |
| `if` `(` [expression](#expression) `)` [statement](#statement) `else` [statement](#statement) | `if (a) if (b) x = 1; else x = 2;` | Dangling else: the grammar is ambiguous here. "`else` binds to the nearest `if`" falls out on its own if each `if` takes an `else` as soon as it sees one. | ✅ | 🟤#7 |
| `switch` `(` [expression](#expression) `)` [statement](#statement) | `switch (c) { case 1: x = 1; }` | | ❌ |  |
### iteration-statement
Loops: `while`, `do`-`while`, `for`.

| rule                                                                                                                                                                  | example                                        | gotchas                                                             | SUBSET.md says | slice |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------- | ------------------------------------------------------------------- | -------------- | ----- |
| `while` `(` [expression](#expression) `)` [statement](#statement)                                                                                                     | `while (i < n) i = i + 1;`                     |                                                                     | ✅              | 🔵#8  |
### jump-statement
Unconditional jumps: `goto`, `continue`, `break`, `return`.

| rule                                                 | example                | gotchas                                                                                                | SUBSET.md says | slice     |
| ---------------------------------------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------ | -------------- | --------- |
| `return` [expression](#expression)<sub>opt</sub> `;` | `return 0;`, `return;` | `return;` in a non-`void` function and `return x;` in a `void` one are sema errors, not syntax errors. | ✅              | ⚫#1, 🟨#9 |
## A.2.4 External definitions
### translation-unit
The whole source file: a list of top-level declarations.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [external-declaration](#external-declaration) | `int main(void) { return 0; }` | C requires at least one: an empty file isn't a valid translation unit. | ✅ | ⚫#1 |
| [translation-unit](#translation-unit) [external-declaration](#external-declaration) | `int g; int main(void) { return g; }` | Left-recursive: loop until end of file. | ✅ | 🟨#9 |
### external-declaration
One top-level item: a function definition or a declaration.

| rule | example | gotchas | SUBSET.md says | slice |
|---|---|---|---|---|
| [function-definition](#function-definition) | `int f(void) { return 1; }` | Both alternatives start with specifiers and a declarator. Parse that shared prefix, then the next token decides: `{` for a function definition; `=`, `,` or `;` for a declaration. | ✅ | ⚫#1 |
| [declaration](#declaration) | `int g = 1;`, `int f(int);` | | ✅ | 🟨#9, ⚫⚪#10 |
### function-definition
A function's signature together with its body.

| rule                                                                                                                                                                      | example                                   | gotchas                                                                                                                                                                      | SUBSET.md says         | slice     |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------- | --------- |
| [declaration-specifiers](#declaration-specifiers) [declarator](#declarator) [declaration-list](#declaration-list)<sub>opt</sub> [compound-statement](#compound-statement) | `int add(int a, int b) { return a + b; }` | The declaration-list is K&R parameter declarations, which Cminor doesn't have. The declarator must be a function declarator: `int x { }` parses up to `{` and must be rejected. | ⚠️ no declaration-list | ⚫#1, 🟨#9 |
