#import "../../book/lib.typ": *

= The preprocessor and the translation phases

#prereq(
  ([chapter 16, The general shape of compilation], [the first stage of translation]),
  ([chapter 19, The structure of a program], [where a macro sits]),
)

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

#organizer[
  We open head on the layer chapter 16 passed over as "a text tool that does not
  know C". The identity of the macro, the principle and idioms of the two
#idx("translation phases")  operators `#` and `##`, their traps, and the whole of
  the *translation phases* the standard pins down for source becoming a program.
  It is the grammar of the "second language" living inside a C source file.
]

#chapter-questions()

== Macros — turning a name into a stream of tokens

`#define NAME content` is the directive "from now on, when you meet the token
NAME, replace it with content." There is also a form taking arguments
(`#define MAX(a,b) ...`), which looks like a function but is not — it is
*substitution, not a call*, with neither type checking nor any guarantee about
evaluation order.

=== Parentheses — the first thing anyone using macros learns

With a function the argument arrives *after being calculated into one value*, but with
a macro *the tokens as written* are planted into the body. So the operators inside the
argument and the operators in the body meet in one place, and chapter 20's precedence
divides them. We see four traps together with their actual expansions.

#demo("examples-en/ch52/parens.c")

*① Without wrapping the argument, the operations inside it scatter.*

```c
#define SQ_BAD(x)  x * x
SQ_BAD(1+2)   →   1+2 * 1+2   →   1 + (2*1) + 2   =  5     /* 9 was expected */
```

`1+2` does not go in bound into one value; three tokens are planted as they are, and
over them the rule that multiplication is stronger than addition applies. Wrapping the
argument in parentheses solves it — `(1+2) * (1+2)` = 9.

*② Without wrapping the whole, the outer operator cuts in.* Wrapping the argument
alone is not enough.

```c
#define SQ_HALF(x)  (x) * (x)
100 / SQ_HALF(2)   →   100 / (2) * (2)   =  100            /* 25 was expected */
```

Division and multiplication have the same precedence and bind from the left
(appendix A), so `100/2` happens first and is then multiplied by 2. The whole body must
be wrapped for `100 / ((2) * (2))` = 25. *So the canonical form wraps both.*

```c
#define SQ(x)  ((x) * (x))
```

*③ With a signed argument the tokens can even fuse.*

```c
#define NEG_BAD(x)  -x
NEG_BAD(-3)   →   --3      /* it becomes the decrement operator — a compile error */
```

It is why the example prints this expansion *as text only* — written for real it does
not compile. Wrapped as `(-(x))` it goes properly to `(-(-3))` = 3.

*④ What parentheses cannot block — the argument is evaluated twice.*

```c
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
int i = 5, j = 3;
int m = MAX(i++, j);      →   ((i++) > (j) ? (i++) : (j))
```

The example's output is that result — `i` jumped from 5 *to 7*, and `m` is 6 rather
than the expected 5. Because `i++` was planted in the body *twice*. Parentheses solve
only the precedence problem, not this one. It is the place where chapter 32's "one
variable changed only once in one statement" is broken behind your back by a macro.

#dtable(
  columns: 2,
  [*rule*], [*reason*],
  [wrap every argument in parentheses], [so the operations inside an argument do not scatter],
  [wrap the whole body in parentheses], [so an outer operator does not cut in],
  [do not use an argument twice], [the side effects happen twice],
  [compute values with a `static inline` function], [then none of the three need be minded],
)

The last row is today's answer. The reason for computing values with `#define` was to
save the cost of a call, and today's compilers inline small functions by themselves
(chapter 13), so that reason has nearly vanished. *The parenthesis rules are needed
only where a macro must be used*, and those places are what the rest of this chapter
shows — assembling tokens, conditional compilation, capturing source location.

== `#` — turning a token into a string (stringize)

Put `#` before a macro argument and that argument becomes *a string literal of it
literally*. The key is that what becomes a string is not the value but *the shape
as written*.

#demo("examples-en/ch52/macro.c")

The demonstration's `SHOW(2 + 3 * 4)` is that in the flesh — `#expr` becomes the
string `"2 + 3 * 4"` while the `(expr)` beside it is calculated and becomes 14.
One argument was used *both as text and as a value*. This pattern is the basic
form of debugging and checking macros: printing the condition "x > 0" as it stands
when `assert(x > 0)` fails works on exactly this principle (the inner workings of
the assert met in chapter 48).

== `##` — joining two tokens into one (token paste)

`##` *joins* the tokens on either side *into one new token*. The demonstration's
`MAKE_NAME(value_, 1)` becoming the *identifier* `value_1` is the example — not a
string but a real name, so it can be used in a variable declaration.

Its use is *code generation that assembles names*. When functions or structs of
the same shape must be stamped out per type (C having no generics), joining a
prefix and a type name to produce names like `stack_int_push` and
`stack_double_push` from one macro is the classic technique.

== Double expansion — why one layer does not unfold

There is a rule you must meet when using `#` and `##`. *A macro argument that
becomes the operand of `#` or `##` is not expanded first.* The demonstration's
last two lines are the contrast — `STR_RAW(WIDTH)` becomes `"WIDTH"`, while
`STR(WIDTH)` becomes `"80"`.

=== The names first — four rules

What happens in this section is what the standard writes down in four clauses.
Knowing the names makes it far easier to put the order together in your head.

#dtable(
  columns: 2,
  [*the standard's name*], [*what the rule is*],
  [argument substitution], [an argument is *fully expanded first* and then put into the body],
  [the `#` operator (stringize)], [the operand argument is *not expanded* and becomes a string literally],
  [the `##` operator (token pasting)], [the arguments on either side are pasted into one token *without expansion*],
  [rescanning and further replacement], [the result of substitution is *scanned again* and the remaining macros expanded],
)

The heart of it is the *collision* between the first row and the middle two.
Normally an argument is expanded first (argument substitution), but an argument that
becomes the operand of `#` or `##` is the exception and the *unexpanded original
token* is used. So when the value is wanted, one more layer is added so that the
argument is expanded *before* it meets `#` or `##` — this is the idiom called
*double expansion*, or an *indirection macro*.

One more rule is needed. Its name is not the standard's but people's — the *blue
paint rule*. If a macro's own name appears again while it is being expanded, *that
name is not expanded again but left as it is* (the metaphor is that it is painted
blue to mark it). It is the device that prevents infinite recursion, and the `LOW`
of the standard example below turns on exactly this rule.

=== Following the expansion by hand — a contrast of two lines

What makes this hard to simulate in the head is that "when to expand and when not"
differs from place to place. Let us put two lines side by side and follow them step
by step. `WIDTH` is defined as `80`.

*One layer — `STR_RAW(WIDTH)`*

#dtable(
  columns: 3,
  [*step*], [*what is being done*], [*result*],
  [1], [`WIDTH` goes into `STR_RAW`'s parameter `s`], [`s` ← `WIDTH`],
  [2], [the body is `#s`, so it is *the exception to argument substitution* — not expanded], [`WIDTH` (as it is)],
  [3], [it is stringized], [`"WIDTH"`],
  [4], [rescanning — inside a string there are no tokens, so there is nothing more to see], [`"WIDTH"`],
)

*Two layers — `STR(WIDTH)`*

#dtable(
  columns: 3,
  [*step*], [*what is being done*], [*result*],
  [1], [`WIDTH` goes into `STR`'s parameter `x`], [`x` ← `WIDTH`],
  [2], [the body is `STR_RAW(x)` — neither `#` nor `##`, so *argument substitution* expands it first], [`x` ← `80`],
  [3], [it is put into the body], [`STR_RAW(80)`],
  [4], [rescanning — `STR_RAW` is a macro, so it is expanded], [`#80`],
  [5], [it is stringized], [`"80"`],
)

The only place a difference arises is *step 2*. Thanks to the extra layer outside,
the argument was expanded once before meeting `#`, and so the token `#` saw was not
`WIDTH` but `80`. `##` is in the same circumstance, so keeping a
`PASTE`/`PASTE_RAW` pair is idiom.

=== The standard's own example — `glue` and `xglue`

This idiom is also one the standard document gives directly as an example (in C17
it is the EXAMPLE of §6.10.3.5, "Scope of macro definitions"; C23 pushed the number
along by one as `#embed` came in at 6.10.3). We run that example as it stands.

#demo("examples-en/ch52/glue.c")

The third and fourth lines of the output are this section's conclusion. Given the
same arguments, `glue` produced `hello` and `xglue` produced `hello, world`. We
follow why, a step at a time. Three definitions are involved.

```c
#define glue(a, b)   a ## b
#define xglue(a, b)  glue(a, b)
#define HIGHLOW      "hello"
#define LOW          LOW ", world"
```

*`glue(HIGH, LOW)`*

#dtable(
  columns: 3,
  [*step*], [*what is being done*], [*result*],
  [1], [it takes the arguments], [`a` ← `HIGH`, `b` ← `LOW`],
  [2], [the body is `a ## b` — both are operands of `##`, so they are *not expanded*], [`HIGH`, `LOW` (as they are)],
  [3], [they are pasted], [`HIGHLOW` (one identifier token)],
  [4], [rescanning — `HIGHLOW` is a macro], [`"hello"`],
)

Notice that a macro called `HIGH` was never defined. The reason no error arises is
that `HIGH`, *before being used on its own*, was pasted with `LOW` and became a
different name, `HIGHLOW`. This is what `##` does — it makes one name that exists out
of two that do not.

*`xglue(HIGH, LOW)`*

#dtable(
  columns: 3,
  [*step*], [*what is being done*], [*result*],
  [1], [it takes the arguments], [`a` ← `HIGH`, `b` ← `LOW`],
  [2], [the body is `glue(a, b)` — no `#` or `##`, so *argument substitution* expands first], [2a and 2b below],
  [2a], [`HIGH` is not a macro], [`HIGH` (as it is)],
  [2b], [expanding `LOW` gives `LOW ", world"`, and the `LOW` inside is not expanded further, by the *blue paint rule*], [`LOW ", world"`],
  [3], [it is put into the body], [`glue(HIGH, LOW ", world")`],
  [4], [rescanning — `glue` is expanded. This time `a` is `HIGH` and `b` is `LOW ", world"`],
      [`HIGH ## LOW ", world"`],
  [5], [`##` pastes *only one token on each side* — `HIGH` and `LOW`], [`HIGHLOW ", world"`],
  [6], [rescanning — `HIGHLOW` is a macro], [`"hello" ", world"`],
  [7], [in translation phase 6 adjacent string literals are joined (this chapter's table)], [`"hello, world"`],
)

Step 5 is the place that confuses most. `##` is an operator that pastes *two tokens*,
not "the whole of the right-hand argument". So even though `b` has expanded into
several tokens (`LOW`, `", world"`), only the leading `LOW` is pasted and the rest
follows after it as it stands.

And it is worth pointing out that step 7 is the business not of the preprocessor but
of *translation phase 6*. What the preprocessor produced was, after all, the two
string literals `"hello" ", world"`, and their joining into one is the next stage.

#qa[
  How does `#include xstr(INCFILE(2).h)` become `"vers2.h"`?
][
  It is the same principle used on a header name. That the example really includes
  that header and prints `VERS_TAG` confirms it.

  + The argument is `INCFILE(2).h`. The outside is `xstr`, so it is *expanded first*.
  + `INCFILE(2)` → `vers ## 2` → `vers2`. So the argument becomes `vers2 .h`.
  + Put into `xstr`'s body `str(s)` and rescanned it is `str(vers2 .h)`.
  + This time it is the operand of `#`, so it is stringized without further expansion
    → `"vers2.h"`.
  + `#include` takes that string as the header name.

  Written with one layer as `str(INCFILE(2).h)` it would have become the useless name
  `"INCFILE(2).h"`. Code that chooses a header by version number or platform name uses
  this idiom.

  How stringizing *treats white space* appears in this example too. White space
  between tokens inside the argument shrinks to one space and the space at either end
  vanishes — so there is no need to worry that `vers2 .h` might become `"vers2 .h"`
  rather than `"vers2.h"`. In fact both the standard's result and this example's are
  `vers2.h`.
]

#antipattern[
  When you think it is two layers and it is one
][
  ```c
  #define CONCAT(a, b) a ## b
  #define VERSION 2
  int CONCAT(api_v, VERSION)(void);   /* api_vVERSION — not what was wanted */
  ```
  `CONCAT` itself is the macro that uses `##` directly, so `VERSION` is not expanded.
  One more layer must be wrapped round.
  ```c
  #define CONCAT_RAW(a, b) a ## b
  #define CONCAT(a, b)     CONCAT_RAW(a, b)
  int CONCAT(api_v, VERSION)(void);   /* api_v2 */
  ```
  There are two naming practices — attach `_RAW` or `_IMPL` to the inner one, or
  attach an `x` to the outer one (the standard example's `xstr` and `xglue` are the
  latter). Either way, only one rule need be kept: *the macro that uses `#` or `##`
  directly goes on the inside, and the outside merely calls it.*
]

It is the preprocessor trap that catches people most often, and also a problem solved
in two lines once known.

== The curious examples the standard gives

The standard document's macro replacement clauses hold several examples that make one
ask "was even this settled?". They are not things to use often in practice, but they
show *what happens when the expansion rules are pushed to their limit*, so they are
worth reading once. We pick three and really run them.

#demo("examples-en/ch52/odd.c")

The knack the example uses of printing the expansion as text is worth noticing too —
wrap something in `xstr(…)` and "so what did it become" can be seen as a string. The
double expansion just learned is, in effect, used as *a tool for debugging the
preprocessor*.

=== ① Placemarkers — pasting an empty argument

```c
#define t(x, y, z)  x ## y ## z
int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),
            t(10,,), t(,11,), t(,,12), t(,,) };
```

What happens if an argument is left empty and `##` is used? There being nothing to
paste, it looks like an error, but the standard settles that in an empty argument's
place there is an invisible token called a *placemarker*. This token leaves whatever it
is pasted with as it is, and vanishes when the expansion ends.

So `t(6,,7)` pastes `6` and `7` into `67`, and `t(,,)` *leaves no token at all*. It is
why the example's array ends with seven elements — the eighth `t(,,)` vanished whole
and left only a trailing comma (that comma being legal thanks to chapter 43's
initialiser syntax).

Where this rule pays in practice is in *a macro whose argument may or may not be
there*. A code-generating macro that chooses with one argument whether to attach a
prefix leans on this property.

=== ② The expansion the standard declares "unspecified"

```c
#define f(a)  a*g
#define g(a)  f(a)
f(2)(9)
```

These two lines call each other. `f(2)` becomes `2*g`, and with `(9)` following it it
could be read as `g(9)` — and expanding `g` gives `f(9)` again, whose `f` is already
being expanded and so is caught by the *blue paint rule*.

Here the standard did not settle on one answer. It writes into the example itself that
*"the result is either `2*9*g` or `2*9*f(9)`, and which it is is unspecified"*. It is a
textbook case of *unspecified behaviour*, one of the grey zones learned in chapter 49 —
it is one of two, but which is settled by the implementation.

The GCC on the machine that ran this example chose `2*9*g`. Another answer on another
implementation would not be a bug. The practical lesson is one — *do not make macros
call each other.*

=== ③ `__VA_OPT__` — when is "empty" judged?

The `__VA_OPT__` C23 brought in (chapter 70) puts something in "only when the variadic
arguments are not empty". But *when* it looks to see whether they are empty is subtle.

```c
#define LOG(...)  log(0 __VA_OPT__(,) __VA_ARGS__)
#define EMP                       /* a macro that expands to nothing */

LOG(1,2)   →  log(0 , 1,2)
LOG()      →  log(0 )
LOG(EMP)   →  log(0 )             ← an argument was passed and yet there is no comma
```

`LOG(EMP)` is the heart of it. One argument was plainly passed, but `EMP` expands to no
token at all. Because the judgement is made on the tokens *after* expansion rather than
before, it counts as "empty" and no comma attaches.

The `SDEF` side shows this property's practical use.

```c
#define SDEF(name, ...)  S name __VA_OPT__(= { __VA_ARGS__ })

SDEF(foo)         →  S foo                 /* without initialisation */
SDEF(bar, 1, 2)   →  S bar = { 1, 2 }      /* with it */
```

That a macro whose *very syntax changes* with the presence of arguments can be written
with standard syntax alone is C23's contribution. Before it one had to lean on a GCC
extension (`, ##__VA_ARGS__`).

#qa[
  I hear the standard has a worse example than these.
][
  It does. The last example of the clause on rescanning is the one — it puts down some
  nine lines of definitions (including things like `#define z z[0]` and `#define h g(~`,
  where *the parentheses do not even match*) and asks what the single line
  `f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);` becomes. The answer is
  `f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);`.

  That example's purpose is not to test people but *to test implementations*. It is put
  together so that whoever writes a preprocessor and gets one rule wrong will have it
  show up in this single line. We have no reason to memorise it; it serves only to
  confirm where the boundary of the defined world lies.
]

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

#demo("examples-en/ch52/xmacro.c")

Four sets came out of one list. Taking them one at a time.

*① The enumeration* — `AS_ENUM` unfolds to `ERR_##name = code,`. `##` joins
`ERR_` with the name and produces `ERR_NOT_FOUND = 2,`.

*② The table of name strings* — `#name` turns the name straight into a string.
Moreover it uses the *designated initialiser* `[code] = #name` (chapter 43) so
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
  (chapter 51). The macro line continuations (`\\`) disappear and the list reads
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
