#import "../../book/lib.typ": *

= What the new standards added, and the `*_s` controversy

#prereq(
  ([chapter 60, The whole map of the standard library], [the whole map of the standard library]),
  ([chapter 62, The traps of reading and writing], [bounds and truncation]),
)

#deepqa[
  Chapter 60 said the speed at which headers grow is the same as the language's
  speed of change, and chapter 62 said `gets`'s funeral took twenty years. Then
  where are the "safe functions" the standard brought in to fill that place?
][
  Mostly *nowhere*. C11 brought in dozens of functions such as `gets_s` and
  `strcpy_s` as annex K, but it was *optional* and the major implementations
  refused to adopt it. What was confirmed on the machine that made this book is the
  same — the "annex K: not present in this implementation" the example printed. The
  latter part of this chapter is that story.
]

#organizer[
  The last chapter of this part. We skim the headers C99, C11 and C23 added to the
  standard library, and then see the whole story of this language's most famous
  failed attempt — annex K, which tried to bring "safe functions" into the
  standard. Why `gets_s` and `strcpy_s` are not widely used, and what is different
  about Microsoft's functions of the same names.
]

#chapter-questions()

== What C99 added

#dtable(
  columns: 3,
  [*header*], [*what*], [*its position today*],
  [`<stdint.h>`], [fixed-width integers (`int32_t` and so on)], [effectively compulsory. chapter 27],
  [`<inttypes.h>`], [the format macros for those types], [the `PRId32` family. appendix B],
  [`<stdbool.h>`], [`bool`, `true`, `false`], [unnecessary, having become keywords in C23],
  [`<complex.h>`], [complex numbers], [optional. support is uneven],
  [`<fenv.h>`], [the floating-point environment], [chapter 71],
  [`<tgmath.h>`], [type-generic mathematics], [chapter 71],
)

`<stdint.h>` is this list's winner. A standard way to express "exactly 32 bits"
finally arose, and the practice of every project keeping its own `typedef` until
then was tidied away.

#qa[
  If annex K's `*_s` functions failed, what fills their place now?
][
  Not one thing but three, sharing it out. *Compiler diagnostics* (warnings and
  hardened builds such as `_FORTIFY_SOURCE`), *sanitizers* (chapter 17), and
  *API designs that carry the length along*. The last is the direction this book
  has pushed, and chapters 85 and 87 are its implementation.

  The lesson is that safety does not arrive by appending `_s` to a function name.
  What actually worked was making failure impossible to ignore, putting the bounds
  inside the type, and making the checks something a tool can perform. That is how
  chapter 82 arranges the five bugs.
]

== What C11 added

#dtable(
  columns: 3,
  [*header*], [*what*], [*its position today*],
  [`<stdatomic.h>`], [atomic operations and memory orders], [the foundation of concurrency. chapter 12's story],
  [`<threads.h>`], [threads, mutexes, condition variables], [★ adoption is slow — pthreads are usually used],
  [`<stdalign.h>`], [`alignas`, `alignof`], [keywords in C23],
  [`<stdnoreturn.h>`], [`noreturn`], [to be retired in C23, in favour of `[[noreturn]]`],
  [`<uchar.h>`], [`char16_t`, `char32_t`], [chapter 65],
)

`<threads.h>`'s circumstance is interesting. Though it is in the standard, glibc
long did not provide it, so portable code still uses POSIX threads. It is a case
showing that *entering the standard and becoming usable are different things*.

== What C23 added

#demo("examples-en/ch76/newheaders.c")

`<stdckdint.h>` is this edition's practical winner. It reports the wrap-round of
the size calculations seen in chapter 63 *as a value* — `ckd_add`, `ckd_sub` and
`ckd_mul` return true on overflow, and the result may be treated as "unusable"
rather than as the wrapped value.

`<stdbit.h>` is new too. Bit manipulations such as counting leading zeros, counting
set bits and rounding up to a power of two have become standard functions — until
then a place that leaned on compiler builtins (`__builtin_clz` and the like).

Besides these, C23 promoted `bool`, `true`, `false`, `static_assert` and
`thread_local` to keywords, brought in `nullptr` (chapter 36), and effectively
retired compatibility headers such as `<stdbool.h>` and `<stdnoreturn.h>`.

== Annex K — the failed attempt at "safe functions"

Now the main business of this chapter.

In the early 2000s Microsoft put functions such as `strcpy_s` and `sprintf_s` into
its compiler and began raising warnings on use of the existing functions. The
proposal to make that design a standard entered C11 as *annex K*
(bounds-checking interfaces).

The core ideas were three.

+ The destination size is *compulsorily* taken as an argument.
+ When a problem arises it does not truncate and carry on but *returns an error*
  (`errno_t`).
+ When a contract violation is detected, the program's chosen *constraint handler*
  is called.

```c
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

char dst[8];
errno_t e = strcpy_s(dst, sizeof dst, src);   /* an error if it overflows */
```

The direction resembles what we organised in chapter 60 as "what is needed". Yet
the result was a failure.

#realcase[
  Why annex K was not adopted
][
  In 2015, C standards committee document N1967, "Field Experience With Annex K",
  surveyed the actual state. Its summary was cold — *it was not widely implemented,
  it behaved differently where it was implemented, and there was no evidence that
  it made real code safer*.

  Concretely these were the circumstances.

  - *Microsoft's functions and the standard's functions are not the same.* The
    names are the same while arguments and behaviour go out of step in places, so
    code fitted to one side broke on the other.
  - *Major implementations, glibc among them, did not adopt it.* That is still so
    today — the reason the earlier example printed "not present in this
    implementation".
  - *The global state called the constraint handler* caused conflicts between
    libraries.
  - It merely changed existing code mechanically, while the real defects remained
    *where the size is calculated wrongly*.

  The committee went as far as discussing removing annex K, and in the end it was
  settled to be kept but effectively not recommended. It is a representative case
  showing how the expectation that "putting it in the standard makes things safe"
  goes wrong in reality.
]

#misconception[
  "Using functions with `_s` attached is safe"
][
  Three things must be checked. First, *is that function there* — the standard's
  annex K is optional and is absent on most of the Unix family. Second, *which
  edition is it* — the standard's and Microsoft's may differ. Third, *what becomes
  safe* — it means the size is taken as an argument, not that the size you passed
  is right. The mistake of passing `strlen` instead of `sizeof` is just as much an
  accident in an `_s` function.

  The realistic choice in portable code is still this — within the standard,
  `snprintf` and explicit bounds checking; where the platform permits, the
  `strlcpy` family; and for the repeated danger zones, components with checking
  built in (Part XII).
]

#platform[
  The `_s` functions met on Windows
][
  MSVC has long provided `strcpy_s`, `sprintf_s`, `fopen_s` and so on, and raises
  the `C4996` warning when the existing functions are used. Defining
  `_CRT_SECURE_NO_WARNINGS` to turn the warning off is the practice.

  The point to beware of is that *these functions are not entirely the same as the
  standard's annex K*. For example the behaviour on argument-validation failure and
  the rules for return values may differ. So unless the code is Windows-only one
  does not lean on the `_s` family, and cross-platform projects mostly choose to
  keep a wrapper of their own.
]

== What this part leaves behind

We have walked the standard library across ten chapters. Memorising function names
was not the aim, so what remains to be kept is a few attitudes.

+ *Read the contract first.* Does it take a size, how does it report failure, who
  owns the pointer it returned.
+ *Do not throw away return values.* Especially `fclose`, `snprintf` and the
  `scanf` family.
+ *Suspect global state.* `errno`, the locale, static buffers, the rounding mode.
+ *Being in the standard does not make it safe.* `gets` survived twenty-two years.
+ *Weigh platform extensions between the gain and portability.*

#recap[
  A table of the editions.

  #dtable(
    columns: 3,
    [*edition*], [*representative addition*], [*is it actually used*],
    [C99], [`<stdint.h>`, `<inttypes.h>`], [yes — effectively compulsory],
    [C99], [`<complex.h>`], [rarely],
    [C11], [`<stdatomic.h>`], [yes — the foundation of concurrency],
    [C11], [`<threads.h>`], [rarely — pthreads prevail],
    [C11], [annex K (`*_s`)], [no — this chapter's story],
    [C23], [`<stdckdint.h>`], [yes — the right answer for size calculations],
    [C23], [`<stdbit.h>`], [growing],
    [C23], [keyword promotion (`bool`, `nullptr` and so on)], [yes],
  )
]

The bottom three lines of that table — `<stdatomic.h>`, `<stdckdint.h>` and
keyword promotion — have only shown their faces. The three remaining chapters of
this part treat those three in detail, one each. We begin with the foundation of
concurrency.
