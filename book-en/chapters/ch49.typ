#import "../../book/lib.typ": *

= The preprocessor and the translation phases

#organizer[
  We open head on the layer chapter 16 passed over as "a text tool that does not
  know C". The identity of the macro, the principle and idioms of the two
#idx("translation phases")  operators `#` and `##`, their traps, and the whole of
  the *translation phases* the standard pins down for source becoming a program.
  It is the grammar of the "second language" living inside a C source file.
]

#deepqa[
  Chapter 19 said lines beginning with `#` are not statements and have no
#idx("preprocessor")  semicolon, and that the reason is "the preprocessor is a
  text tool working by lines, not by C's grammar." Then what does the preprocessor
  know of C?
][
  Almost nothing — it knows neither types nor scopes nor functions. What it knows
  is only *tokens* (word-pieces that cannot be split further: names, numbers,
  operators, strings), and what it does is delete, insert and join tokens. This
  ignorance is the preprocessor's power (it can produce any fragment of code
  whatever) and its danger (it upholds neither grammar nor meaning). This chapter
  sees the power and the danger together.
]

== Macros — turning a name into a stream of tokens

`#define NAME content` is the directive "from now on, when you meet the token
NAME, replace it with content." There is also a form taking arguments
(`#define MAX(a,b) ...`), which looks like a function but is not — it is
*substitution, not a call*, with neither type checking nor any guarantee about
evaluation order.

Two idioms follow from this difference. First, *wrap the arguments and the whole
in parentheses* — as in `#define SQ(x) ((x) * (x))`. Without them `SQ(1+2)`
unfolds to `1+2*1+2` and gives 5 (chapter 20's precedence applying to the
substituted token stream as it is). Second, *beware macros that use an argument
twice* — `SQ(i++)` increments `i` twice. Chapter 32's "one variable changed only
once in one statement" is broken behind your back by a macro.

== `#` — turning a token into a string (stringize)

Put `#` before a macro argument and that argument becomes *a string literal of it
literally*. The key is that what becomes a string is not the value but *the shape
as written*.

#demo("examples/ch49/macro.c")

The demonstration's `SHOW(2 + 3 * 4)` is that in the flesh — `#expr` becomes the
string `"2 + 3 * 4"` while the `(expr)` beside it is calculated and becomes 14.
One argument was used *both as text and as a value*. This pattern is the basic
form of debugging and checking macros: printing the condition "x > 0" as it stands
when `assert(x > 0)` fails works on exactly this principle (the inner workings of
the assert met in chapter 45).

== `##` — joining two tokens into one (token paste)

`##` *joins* the tokens on either side *into one new token*. The demonstration's
`MAKE_NAME(value_, 1)` becoming the *identifier* `value_1` is the example — not a
string but a real name, so it can be used in a variable declaration.

Its use is *code generation that assembles names*. When functions or structs of
the same shape must be stamped out per type (C having no generics), joining a
prefix and a type name to produce names like `stack_int_push` and
`stack_double_push` from one macro is the classic technique.

== The double indirection — why one layer does not unfold

There is a rule you must meet when using `#` and `##`. *A macro argument that
becomes the operand of `#` or `##` is not expanded first.* The demonstration's
last two lines are the contrast — `STR_RAW(WIDTH)` becomes `"WIDTH"`, while
`STR(WIDTH)` becomes `"80"`.

The reason lies in the order of substitution. A macro argument is normally
*expanded fully first* and then put into the body, but when it is the operand of
`#` or `##` the *unexpanded original token* is used. So when you want the value
you add another layer — the outer macro (`STR`) takes the argument ordinarily and
*after expanding it* passes it to the inner macro (`STR_RAW`), which makes the
string there. `##` has the same circumstance, so keeping a `PASTE`/`PASTE_RAW`
pair is idiom. It is the preprocessor trap that catches people most often, and
also a problem solved in two lines once known.

#realcase[
  A language made by one macro — the X macro
][
  There is a classic technique showing the power of `#` and `##`. It writes a list
  in one place and unfolds it in several forms — the *X macro*. The name comes
  from the conventionally used parameter `X`. We see it in the flesh in the next
  section.
]

== The X macro — several sets of code from one list

C has no code generation. So the problem arises of "having to write the same list
repeatedly in several forms" — take error codes: once in an enumeration, once in
a table of name strings, once in a `switch` returning messages. Miss one place
when adding an item and the tables are out of step from then on.

The X macro solves this by *writing the list only once*. The knack is two steps.

+ Write the list in the form of "calls to a macro `X` that is not yet defined."
+ Define `X` differently each time and unfold the list. The same list comes out as
  different code every time.

#demo("examples/ch49/xmacro.c")

Four sets came out of one list. Taking them one at a time.

*① The enumeration* — `AS_ENUM` unfolds to `ERR_##name = code,`. `##` joins
`ERR_` with the name and produces `ERR_NOT_FOUND = 2,`.

*② The table of name strings* — `#name` turns the name straight into a string.
Moreover it uses the *designated initialiser* `[code] = #name` (chapter 41) so
that the code value is itself the array index. Even with sparse values (0, 2, 5,
9) the table lines up by itself.

*③ Generating the `switch`* — `case ERR_##name: return text;` unfolds as many
times as there are items. The whole body of the function was made from the list.

*④ Counting* — the knack of making it unfold to `0 + 1 + 1 + 1 + 1`. Add items to
the list and `ERROR_COUNT` follows by itself.

*⑤ And in the body* — inside `main` the same list is unfolded once more to print a
table. Erasing the macro with `#undef` after use is the convention, because a
lingering name collides with the next use.

#qa[
  I hear there is a version that keeps the list in a separate file. What is
  different?
][
  It is the same idea in different packaging. Write the list in a file such as
  `errors.def`, and on the using side define `X` and unfold it with
  `#include "errors.def"` — using directly the fact that `#include` is text pasting
  (chapter 48). The macro line continuations (`\\`) disappear and the list reads
  better, at the cost of one more file and the inconvenience that an IDE does not
  recognise that file as code. The Linux kernel's system call table long used this
  approach.
]

#antipattern[
  Where it is better not to use an X macro
][
  ```c
  /* when there are only three items and only one derived form */
  #define COLOR_LIST(X) X(RED) X(GREEN) X(BLUE)
  ```
  At this size it reads better simply to write the enumeration and the string
  array side by side. The X macro's value appears when *the list is long, several
  sets of code derive from it, and items keep being added*. Use it on a short list
  and what is lost (the unfolded code being invisible in the debugger and in error
  messages, a first-time reader being unable to read it) outweighs what is gained
  (removing duplication).
]

#realcase[
  X macros as met in practice
][
  This technique is everywhere; only the name is unfamiliar. The Linux kernel's
  system call table and error code definitions, the register and pin definitions
  of many embedded SDKs, the asset-kind lists of game engines, and the state lists
  of state machines are representative. What they share is the same — places where
  *one list is used simultaneously as an enumeration, strings, checking code and
  serialisation code*. It is an invention created by the pressure to remove
  duplication in a language with no tooling for it, and so it is also a technique
  that shows C's character well.
]

#misconception[
  "A macro is the fast version of a function"
][
  In the old days it was half true — short functions were written as macros to
  save the cost of a call. But today's compilers inline small functions by
  themselves (chapter 14's editor), so reasons to use a macro for speed have
  almost vanished. What remains is only the price — no type checking, multiple
  evaluation of arguments, invisibility in the debugger, and error messages given
  against the unfolded code and thus hard to read. The modern guidance is clear:
  *compute values with functions (with `static inline` if needed), make constants
  with `enum` or C23's `constexpr`, and use macros only for what a function cannot
  do — conditional compilation, token assembly, capturing source location.*
]

== The translation phases — the eight steps the standard pins down

We knew preprocessing happens "first", but exactly what happens in what order?
The standard pins this down as eight steps under the name *translation phases*. A
real compiler need not perform these steps physically separately, but must behave
*as if it had done them in this order* (chapter 14's "as long as it looks the
same" applies here too).

#dtable(
  columns: 2,
  [*phase*], [*what it does*],
  [1], [read the source characters and map them into the internal character set (chapter 9's world of encodings). trigraph replacement used to be here — C23 removed it],
  [2], [join lines continued by a backslash at the end of a line],
  [3], [turn comments into a single space, and split the source into *tokens*],
  [4], [execute preprocessing directives — `#include` (repeating phases 1\~4 recursively), `#define` substitution, conditional compilation],
  [5], [convert the characters of character and string literals into the execution character set],
  [6], [join adjacent string literals into one (`"a" "b"` → `"ab"`)],
  [7], [interpret the tokens by C's grammar and translate — this is "compiling"],
  [8], [join the needed definitions and make the executable image — linking],
)

This table unravels several riddles at once. *Because comments become a space in
phase 3*, `a/**/b` is `a b`, not `ab`. *Because `#include` is processed
recursively in phase 4*, headers within headers unfold naturally. *Because string
joining is in phase 6*, the idiom of writing a long string over several lines
works, and string fragments made by a macro (the result of the `#` operator) also
join with a literal beside them. And *that grammatical interpretation is phase 7*
restates this chapter's core — when the preprocessor works, C's grammar does not
yet exist.

#qa[
  Then where is a syntax error inside a macro caught?
][
  In phase 7, against *the code after unfolding* — which is why the error message
  looks remote from the line you wrote. The standard diagnosis is the `-E`
  (preprocess only) learned in chapter 16: preprocess the file in question and look
  at how it really unfolded, and a riddling error usually resolves at once. Modern
  compilers help with this too by showing trace information such as "expanded from
  macro NAME".
]

We now know layer by layer the whole process of source becoming a program. The
next chapter is the secret of how `printf` takes any number of arguments — the
variadic function.
