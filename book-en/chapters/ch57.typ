#import "../../book/lib.typ": *

= The whole map of the standard library

#prereq(
  ([chapter 56, The terrain of the standard library], [the character of the standard library]),
)

#deepqa[
  Chapter 56 said the standard library is "thin and old", and that the standard
  pins down not only the grammar but the list of libraries and the contract of each
  function. Then exactly how many headers are there, and how has that list grown?
][
  Thirty-one as of C23. C89 began with fifteen, C95 added two concerning wide
  characters, C99 nine, C11 five, and C23 a few more. That the speed of growth is
  almost the same as the language's speed of change tells the character of this
  list — *once something enters it stays effectively forever, and so it takes long
  to let anything in*. It is the other side of the story of `gets`'s funeral taking
  decades (chapter 40).
]

#organizer[
  We spread into a table every header the C standard settles, without missing one.
  Which header entered in which edition, what can be used even without an
  operating system, and in what order the remaining chapters of this part walk
  those regions. It is the chapter that redraws chapter 56's terrain map at a
  larger scale.
]

#chapter-questions()

== Freestanding and hosted — two worlds

The standard divides implementations in two. A *hosted implementation* is the
ordinary environment running on top of an operating system, and a *freestanding
implementation* is an environment with no operating system — firmware, a kernel, a
bootloader.

The difference between the two is exactly the difference in the header list. The
headers a freestanding implementation must provide are only the following; the
rest may or may not be there.

#dtable(
  columns: 3,
  [*header*], [*what*], [*note*],
  [`<float.h>`], [the limits of real types], [defines values only],
  [`<limits.h>`], [the limits of integer types], [defines values only],
  [`<stdarg.h>`], [variadic arguments], [chapter 53],
  [`<stdbool.h>`], [`bool`], [C99. effectively unnecessary in C23],
  [`<stddef.h>`], [`size_t`, `NULL`, `offsetof`], [the most basic of the basics],
  [`<stdint.h>`], [fixed-width integers], [C99],
  [`<stdalign.h>`], [`alignas`, `alignof`], [C11. keywords in C23],
  [`<stdnoreturn.h>`], [`noreturn`], [C11. to be retired in C23],
  [`<iso646.h>`], [alternative spellings such as `and`, `or`], [C95],
  [`<stdbit.h>`], [bit manipulation], [added in C23],
  [`<stdckdint.h>`], [checked arithmetic], [added in C23],
)

That C23 lengthened this list is worth noticing. The two that newly entered are *pure
computation needing no operating system*, so they can be provided in a freestanding
environment too, and what they do (counting bits and checking overflow) is especially
handy in embedded work. That the list grows in the direction of "what works without an
OS" shows this division's character too.

And a whole header being required differs from only some of its declarations being
required — `<string.h>`, for example, is not freestanding-required, but if an
implementation provides it the contracts inside must follow the standard.

That this list is short is the background of this whole book — in embedded work
neither `printf` nor `malloc` is a given (Part XII's freestanding story begins
here).

== The whole list

Every header the standard settles. "Edition" is the edition in which that header
entered the standard, and the chapter of this part that treats it is written
alongside.

#dtable(
  columns: 4,
  [*header*], [*edition*], [*what it holds*], [*in this part*],
  [`<assert.h>`], [C89], [`assert` — the diagnosis that catches contract violations], [chapter 65],
  [`<complex.h>`], [C99], [complex arithmetic], [chapter 63],
  [`<ctype.h>`], [C89], [character classification and conversion], [chapter 62],
  [`<errno.h>`], [C89], [the error-number global], [chapter 65],
  [`<fenv.h>`], [C99], [the floating-point environment (rounding, exceptions)], [chapter 63],
  [`<float.h>`], [C89], [the limits of real types], [chapters 26, 63],
  [`<inttypes.h>`], [C99], [formats and conversions for fixed-width integers], [appendix B, chapter 68],
  [`<iso646.h>`], [C95], [alternative spellings of operators], [chapter 68],
  [`<limits.h>`], [C89], [the limits of integer types], [chapter 26],
  [`<locale.h>`], [C89], [locale settings], [chapter 62],
  [`<math.h>`], [C89], [mathematical functions], [chapter 63],
  [`<setjmp.h>`], [C89], [non-local jumps], [chapter 65],
  [`<signal.h>`], [C89], [signal handling], [chapter 65],
  [`<stdalign.h>`], [C11], [specifying and querying alignment], [chapter 68],
  [`<stdarg.h>`], [C89], [variadic arguments], [chapter 53],
  [`<stdatomic.h>`], [C11], [atomic operations], [chapter 69],
  [`<stdbit.h>`], [C23], [bit manipulation (counting, rotating and so on)], [chapter 68],
  [`<stdbool.h>`], [C99], [`bool`, `true`, `false`], [chapter 68],
  [`<stdckdint.h>`], [C23], [arithmetic that checks for overflow], [chapters 49, 70],
  [`<stddef.h>`], [C89], [`size_t`, `ptrdiff_t`, `NULL`, `offsetof`], [chapter 68],
  [`<stdint.h>`], [C99], [fixed-width integer types], [chapters 26, 68],
  [`<stdio.h>`], [C89], [stream input and output, files], [chapters 51, 59],
  [`<stdlib.h>`], [C89], [conversion, random numbers, allocation, sorting, program termination], [chapter 61],
  [`<stdnoreturn.h>`], [C11], [`noreturn`], [chapter 68],
  [`<string.h>`], [C89], [strings and memory blocks], [chapter 60],
  [`<tgmath.h>`], [C99], [type-generic mathematical functions], [chapter 63],
  [`<threads.h>`], [C11], [threads, mutexes, condition variables], [chapter 68],
  [`<time.h>`], [C89], [time and the calendar], [chapter 64],
  [`<uchar.h>`], [C11], [UTF-16 and UTF-32 character types], [chapter 62],
  [`<wchar.h>`], [C95], [wide-character input, output and strings], [chapter 62],
  [`<wctype.h>`], [C95], [wide-character classification], [chapter 62],
)

#qa[
  How many of these are actually used often?
][
  Most programs live on about five — `<stdio.h>`, `<stdlib.h>`, `<string.h>`,
  `<stdint.h>`, and as needed `<math.h>` or `<time.h>`. The rest are things you
  "know exist and look up when needed". So this part's aim too is not memorising
  but *keeping the map in your head* — roughly where what is, and which regions are
  slippery.
]

#misconception[
  "If it is in the standard library it is a safe and portable function"
][
  Being in the standard means *it is everywhere*, not *it is safe*. `gets` was in
  the 1989 standard and was deleted only in 2011 (chapter 59). `strncpy`, contrary
  to its name, is not a safe copying function (chapter 60), and `atoi` has no way
  at all to report failure (chapter 61). Some functions of *the same name even
  behave differently according to the locale* (chapter 62). Using the standard
  library means not "using what has been verified" but *"using what has a stated
  contract"*, and reading that contract is still our part.
]

#realcase[
  The accident one header called down — `<strings.h>` is not standard
][
  There is a place confusable by similarity of name. `<string.h>` is standard but
  `<strings.h>` (plural) is POSIX. Functions such as `strcasecmp` and `bzero` are
  in there, so code using it does not compile on Windows. Conversely `strlcpy` and
  `strlcat` came out of OpenBSD and spread to several Unixes but *were not
  standard* — only in C23 were functions of similar intent so much as discussed.
  The guess "it is used a lot, so it must be standard" is a common beginning of
  portability accidents.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [number of headers], [thirty-one as of C23. grown from C89's fifteen],
    [freestanding], [only eleven are guaranteed without an operating system (C23 added two)],
    [speed of entering], [slow. the speed of leaving is slower],
    [standard = safe], [no. standard = *the contract is written down*],
    [`<strings.h>`], [not standard (POSIX). do not be fooled by the name],
  )
]

The map is spread out, so we walk. The next two chapters are the region used the
most and slipped in the most — stream input and output.
