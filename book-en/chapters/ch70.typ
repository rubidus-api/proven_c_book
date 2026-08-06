#import "../../book/lib.typ": *

= From macro to keyword — `bool`, `nullptr` and their companions

#prereq(
  ([chapter 67, What the new standards added, and the `*_s` controversy], [what the standards added]),
  ([chapter 29, Booleans and comparison], [the type of true and false]),
)

#deepqa[
  Chapter 67 passed over in a table only that C23 made `<stdbool.h>` effectively
  unnecessary and that `nullptr` came in. But why put into the language what worked
  fine as macros — is it not merely a change of name?
][
  It is not merely a change of name. A macro is *the preprocessor changing letters*
  and so has neither type nor scope, and the user can `#undef` it or define it again
  with another meaning. A keyword is *a grammatical element of the language*, so it
  has a type, can be diagnosed, and nobody can redefine it. Where that difference
  really prevents accidents is this chapter's content — and `nullptr` in particular
  is not a renaming but *a new type*.
]

#organizer[
#idx("bool")  We treat the most conspicuous change C23 made to the language. Why
#idx("nullptr")  things long imitated with macros in headers — `bool`, `true`,
  `false`, `static_assert`, `alignas`, `thread_local` — rose to being keywords,
  what `nullptr` was newly made to prevent, what rules and limits come with them,
  and in what order to move existing code over.
]

#chapter-questions()

== What was promoted

#demo("examples-en/ch70/keywords.c")

We organise it in one table. On the left is the shape up to C17, on the right C23.

#dtable(
  columns: 3,
  [*before*], [*C23*], [*the header it required*],
  [`_Bool` + the `bool` macro], [the keyword `bool`], [`<stdbool.h>`],
  [the `true`, `false` macros (= 1, 0)], [the keywords `true`, `false` (of type bool)], [`<stdbool.h>`],
  [`_Static_assert`], [`static_assert`], [`<assert.h>`],
  [`_Alignas`, `_Alignof`], [`alignas`, `alignof`], [`<stdalign.h>`],
  [`_Thread_local`], [`thread_local`], [`<threads.h>`],
  [the `NULL` macro], [`nullptr` (a new type)], [`<stddef.h>` and others],
  [(none)], [`constexpr`], [—],
  [(a GCC extension)], [`typeof`, `typeof_unqual`], [—],
)

The headers still exist and including them is harmless — the principle that the
standard does not break old code (chapter 59's `gets` story) was kept here too. But
newly written code has no reason to include them.

== `bool` — what does it prevent

C long had no true-false type. It was imitated with `int`, and every project had
`typedef int BOOL;` and `#define TRUE 1` rolling about. C99 brought in `_Bool` and
`<stdbool.h>` attached a pretty name to it, and C23 raised that to a keyword.

What differs between `bool` and `int` is not the name but *the conversion rule*.

- *Every nonzero value is narrowed to 1.* The example's `bool b = 42;` printing `1`
  is that. `int i = 42` is 42 as it stands.
- So *comparing two truths is true.* The classic bug of the `int`-imitation days
  vanishes here.

#antipattern[
  Comparing truth with `1`
][
  ```c
  int a = isupper('A');      /* any nonzero value, depending on the implementation */
  int b = isupper('B');
  if (a == b) { … }          /* both are true, yet the values may differ and it be false */
  if (a == 1) { … }          /* worse */
  ```
  As seen in chapter 62, the classification functions of `<ctype.h>` promise only
  "a nonzero value". To compare truths, narrow to *truth values* rather than
  values.
  ```c
  bool a = isupper('A');     /* normalised to 1 here */
  bool b = isupper('B');
  if (a == b) { … }          /* safe */
  ```
]

#qa[
  What format is used when printing a `bool` with `printf`?
][
  There is no dedicated format. A `bool` goes over as a variadic argument and is
  promoted to `int` (chapter 53's promotion rule), so `%d` is used — the example did
  so. Printing words a human can read with `%s` and the ternary operator is a
  common practice too.

  The place to beware is the `scanf` side. There being no format that takes a
  `bool` directly, it must be received as an `int` and moved. And `sizeof(bool)` is
  usually 1, but *the standard does not promise it is 1* — do not assume this value
  when calculating a struct layout (chapter 43).
]

Two remaining traps of `bool`. First, using `bool` in a *bit-field* works fine even
with a width of 1, but the layout is implementation-defined (chapter 43). Second,
an array of `bool` uses one byte per element — it is not compressed into bits like
C++'s `vector<bool>`. To compress into bits, write the masks by hand (chapter 27)
or use the tools of `<stdbit.h>`.

== `nullptr` — not a renaming

Chapter 6 distinguished the null triplets. `NULL` is *a macro*, and its definition
is `0` or `((void *)0)` depending on the implementation. This freedom bore real
accidents.

*Accident 1 — the size goes out of step in variadic arguments.* A variadic function
does not know the arguments' types and so reads the bits as they came (chapter 53).
On an implementation where `NULL` is defined as `0`, writing
`execl("/bin/ls", "ls", NULL)` sends an *`int` 0*, and on a machine where pointers
are 8 bytes the upper 4 bytes remain as rubbish. The function, failing to recognise
the end of the list, runs away. That is why old code had to write `(char *)0`.
`nullptr` is always of pointer size, so it does not have this problem.

*Accident 2 — whether it is an integer or a pointer blurs.* On an implementation
where `NULL` is `0`, `foo(NULL)` is indistinguishable from passing the integer 0.
In code that branches by type with `_Generic` (chapter 53), this ambiguity becomes
an accident as it stands. `nullptr` has *a type of its own* called `nullptr_t`, so
the branch is clear.

*Accident 3 — going out of step with C++.* C++ brought in `nullptr` first, in 2011,
for the same reasons. There were places in headers crossing the two languages
(chapter 51) where `NULL`'s meaning divided, and C23's adopting the same word
narrowed that gap.

#dtable(
  columns: 3,
  [], [`NULL`], [`nullptr`],
  [identity], [a macro (implementation-defined)], [a keyword, of type `nullptr_t`],
  [variadic arguments], [dangerous (size goes out of step)], [safe],
  [`_Generic` branching], [ambiguous], [clear],
  [comparing with an integer], [`NULL == 0` may work], [not possible — diagnosed],
  [conversion to `bool`], [—], [possible (false)],
  [redefinition], [possible (`#undef`)], [not possible],
)

A few rules to pin down. `nullptr` converts to *any object pointer type* and to a
function pointer too (chapter 54). Two `nullptr`s, and a `nullptr` and any pointer,
can be compared with `==` and `!=`. Converted to `bool` it is false. But *it cannot
be compared with an integer* and does not convert to an integer — `nullptr == 0` is
subject to diagnosis. The language has, in effect, cut the old intuition that "a
null pointer is the same thing as the integer 0".

#misconception[
  "Using `nullptr` reduces null-dereference accidents"
][
  It is a different kind of problem. What `nullptr` prevents is accidents coming
  from *the way null is written* (size going out of step, type ambiguity), not
  accidents of dereferencing null. Confirming whether a pointer is null is still a
  human's part (chapter 35), and reducing that burden is the part not of the
  language but of design — data structures that do not make nulls in the first
  place, conventions that report failure through the return value (Part XII).
]

== The remaining promotions and the new words

*`static_assert`* — checks a condition at compile time and, if it is broken,
*translation fails*. The example's first line is that. It differs in character from
the run-time `assert` (chapter 65) — this one is a tool asking "does this code hold
on this machine", used for pinning assumptions about type sizes, alignment and
struct layout into the code. In C23 the message may be omitted.

*`alignas`, `alignof`* — the words for handling in the language the alignment seen
in chapter 6. They are used to lay things out to fit a cache line (chapter 11's
avoidance of false sharing) or to meet an alignment the hardware requires.

*`thread_local`* — makes a variable that exists separately per strand. It is a tool
for avoiding the problem of sharing seen in chapter 68 *by not sharing*. `errno` is
in fact implemented this way (chapter 65).

*`constexpr`* — as seen in the example, it makes a real constant. A `const int` is
not a constant *expression* and so could not be used for an array size or a `case`
label (chapter 23), and that place was long the part of `#define`. `constexpr`
reclaims that place with a typed name — this chapter's theme of macros being
promoted into the language is repeated here too.

*`typeof`* — takes an expression's type down as it is. What had been used as a GCC
extension for over thirty years became standard. It is especially handy when
declaring a temporary variable inside a macro.

#realcase[
  The story of the underscored names — why `_Bool` looks like that
][
  Why was it not called `bool` from the start? Because if the standard makes a new
  keyword, all existing code already using that word as a name breaks. The world had
  mountains of code containing `typedef int bool;` or `struct bool { … };`.

  So the standard uses *a name space reserved so that users cannot use it* — names
  beginning with one underscore and a capital letter. `_Bool`, `_Static_assert`,
  `_Alignas`, `_Atomic` (chapter 68) and `_Generic` are all products of this rule.
  And headers laid pretty names on top as macros, so that *only those who included
  them* used the short names. Old code that did not include them breaks in nothing.

  C23's promotion is the judgement that "enough time has passed that the short names
  may now be used". Even so the underscored names are still alive, and the two names
  point at the same thing. It is a case where the standard's habit of not breaking
  old code remains even in the names — the same character as chapter 59's `gets`
  story, and a decision in the opposite direction.
]

== How to move existing code over

There is no need to change it all at once. Fix an order and there is almost no
risk.

+ *First settle the compiler edition.* Check whether `-std=c23` (or `c2x`) can be
  used, and whether it works on all the target platforms. If even one does not, go
  to the shell strategy of number 5 below.
+ *Clear away your own `BOOL`, `TRUE`, `FALSE`.* Delete the project header's
  `typedef int BOOL;` and change it to `bool`. While doing so, look together for
  *places that were comparing values* (the counterexample above) — it is rather a
  place where bugs come to light.
+ *Change `NULL` to `nullptr`.* Mechanical substitution finishes most of it, but
  check two places by hand. Variadic calls (a real bug is mended here), and code
  that used `NULL` like the integer 0 (a compile error arises here — which is a good
  thing).
+ *Take `_Static_assert`, `_Alignas` and `_Thread_local` to the names without
  underscores.* They are different notations for the same thing, so there is no
  risk. Then tidy away the now unnecessary `<stdbool.h>` and `<stdalign.h>`
  includes.
+ *If old editions must be supported too, put the shells in one place.* The same as
  what was done in chapter 69.

  ```c
  #if __STDC_VERSION__ < 202311L
  #  include <stdbool.h>
  #  define nullptr ((void *)0)      /* not a complete substitute — see the caution below */
  #  define static_assert _Static_assert
  #endif
  ```

  This `nullptr` shell *cannot imitate the type as well.* In code doing `_Generic`
  branching or type checking it may behave differently from expectations, so if
  there is such code it is right to raise the edition rather than use the shell.

#qa[
  For a project that must keep compiling with an old standard, is this chapter
  somebody else's story?
][
  No — two things remain. First, *the ability to read*. New code and libraries have
  begun using these words, so when a `constexpr` or a `nullptr` appears you must
  know its meaning. Second, *what to mend now*. Places passing `NULL` into variadic
  arguments with the cast left out, places comparing truth with `== 1`, places
  making constants with `#define` and losing the type — these are already dangerous
  under the old standard too. C23's words merely have the language block that danger
  for you; the danger itself was there all along.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [the meaning of promotion], [macro (letter substitution) → keyword (type, diagnosis, no redefinition)],
    [`bool`], [it *normalises* nonzero values to 1 — comparing truths becomes safe],
    [printing a `bool`], [`%d` (promoted to `int` in variadic arguments)],
    [`nullptr`], [not a name but *a new type*. it prevents variadic and `_Generic` accidents],
    [`nullptr`'s limits], [no comparison with or conversion to an integer. false as a `bool`],
    [`constexpr`], [reclaims `#define` constants with a typed name],
    [underscored names], [a product of the reserved name space, to avoid breaking old code],
    [the order of moving], [check the edition → remove your own BOOL → substitute NULL → tidy underscores → shells],
  )
]

Across thirteen chapters we have walked the terrain of the standard library and the
newest standard. We have seen what the standard promises and what it does not,
where it is slippery and why.

The two remaining chapters of this part go one layer down. The place chapter 42
passed over saying only "it is expensive" — what map a program's memory is laid out
on in an operating system and in an embedded chip respectively (chapter 71), and
what an allocator actually does in the heap region of that map (chapter 72). Those
two chapters become the ground for understanding the next part's design.
