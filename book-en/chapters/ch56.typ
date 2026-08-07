#import "../../book/lib.typ": *

= The terrain of the standard library

#prereq(
  ([chapter 40, Safe input], [the traps of the standard functions]),
  ([chapter 16, The general shape of compilation], [a library is linked in]),
)

#deepqa[
  Chapter 16 said the linker finds `printf`'s body in the standard library and
  joins it up, and chapter 40 said `gets` was *removed* from the standard. Who
  settles the standard library, and how does it change?
][
  The language standard settles it along with the language — the C standard
  document pins down not only the grammar but also *the list of libraries that must
  be provided and the contract of each function* (hence "the C standard library").
  The speed of change is as slow as the language's, and removing something is far
  slower still — the reason `gets`'s funeral took decades, and the key to this
  library's character.
]

#organizer[
#idx("standard library")  We spread out a map of the standard library we have
  merely been using until now — what toolboxes there are, and what to trust and
  what to beware of. And why this library has its "thin and old" appearance, down
  to the historical reason.
]

#chapter-questions()

== The map — the toolboxes

Group the headers you meet often into families and the terrain comes into view at
a glance.

#dtable(
  columns: 2,
  [*family*], [*representative headers and content*],
  [input and output], [`<stdio.h>` — streams, the printf/scanf families, files],
  [strings and memory], [`<string.h>` — copying, comparing, searching; `<ctype.h>` — character classification],
  [numbers], [`<stdlib.h>` — conversion and random numbers, `<math.h>` — mathematical functions, `<stdint.h>`, `<limits.h>`, `<float.h>` — the limits of types],
  [memory management], [`<stdlib.h>` — the malloc/free family],
  [time and environment], [`<time.h>`, the environment access in `<stdlib.h>`],
  [contracts and diagnosis], [`<assert.h>`, `<errno.h>`],
  [the modern additions], [`<stdbool.h>` (C99, made unnecessary by C23), `<stdatomic.h>` and `<threads.h>` (C11), `<stdckdint.h>` (C23 checked arithmetic)],
)

Two things stand out in this list. First, *it is thin* — there are no data
structures (lists, hash tables), no regular expressions, no networking, no
graphics. Second, *it is old* — most of it was in C89, and what has newly entered
can be counted on the fingers.

== Why it is thin — design and history

The reason lies where C grew up. C was born as a language for making operating
systems (chapter 4), and it had to write in the same language not only programs
running *on top of* an operating system but the operating system *itself* and
embedded firmware too. In such places there may be no file system, no dynamic
allocation, not even an operating system — so the standard library was narrowed to
"the minimum that can exist anywhere" (the reason the standard separately defines
a freestanding environment). Abundance was given up and portability gained.

And the thinness came at a price — the world made what it needed in the end, and
the result is the forest of platform-specific APIs and third-party libraries. That
is the background of the saying "in C, choosing libraries is half the work" —
chapter 40's five disciplines are about what to demand of that choice.

== An anatomy of the output format string

This is the place to take apart head on the function used most. In chapter 22 we
learned only the minimal set (`%d`, `%s`, `%%`), but `printf`'s format is in fact
a small language of five pieces.

#align(center, block(inset: (y: 4pt))[
  `%` `[flags]` `[width]` `[.precision]` `[length]` `conversion`
])

Reading from the back is quicker to understand. The *conversion* settles "as what
shall it be printed" — this alone is required — and the rest is decoration laid on
top.

- *flag*: `-` left-align, `0` fill the spare places with zeros, `+` a sign even on
  positives, space a space before positives, `#` alternative form (`%#x` attaches
  `0x`).
- *width*: the minimum number of characters. If short it fills, if over it *does
  not cut* — width is a lower bound, not an upper one.
- *precision*: begins with `.`. For reals it is decimal places, for strings the
  *maximum* length, for integers the minimum number of digits.
- *length modifier*: tells the width of the argument. `l` (long), `ll` (long
  long), `z` (`size_t`), `h` (interpret narrowed to short).

Write `*` in the width or precision place and that value can be passed as an
argument. When printing a string that travels as "pointer and length", like
chapter 10's view, `%.*s` comes in handy.

#demo("examples-en/ch56/fmtspec.c")

There are several things to read here. `%06d` becoming `000042` is because the `0`
flag fills with zeros instead of spaces, and `%.3s` stopping at `pro` is because
for strings precision is *maximum length*. The `0x` of `%#x` was attached by the
`#`. `%g` shortening to `3.14159` is because that conversion automatically chooses
the shorter of `%e` and `%f`.

The length modifier is not decoration but *a contract*. As chapter 53 showed, type
information does not ride along into variadic arguments, so `printf` reads the
stack at exactly the width the format stated. That is why code printing a `size_t`
with `%d` quietly goes out of step on 64-bit — 8 bytes were put in and only 4 are
taken out.

#dtable(
  columns: 3,
  [*type*], [*output format*], [*note*],
  [`int`], [`%d` `%i`], [the basic form],
  [`unsigned int`], [`%u` `%x` `%o`], [hexadecimal when looking at bits],
  [`short`], [`%d`], [it is promoted, so `%hd` is optional],
  [`long`], [`%ld`], [],
  [`long long`], [`%lld`], [],
  [`size_t`], [`%zu`], [★ printing it with `%d` goes out of step],
  [`ptrdiff_t`], [`%td`], [],
  [`double`], [`%f` `%e` `%g`], [`float` is promoted and the same],
  [`long double`], [`%Lf`], [],
  [`char` (as a character)], [`%c`], [the argument is promoted to int],
  [`char *`], [`%s`], [it must be NUL-terminated],
  [`void *`], [`%p`], [the form is implementation-defined],
  [`bool`], [`%d`], [printed as 0 or 1],
)

#misconception[
  "`%f` when printing a `float`, `%lf` for a `double`"
][
  Half right. In *output*, a `float` goes over as a `double` by the default
  argument promotion (chapter 28), so both are `%f` — the standard permits `%lf`
  too, but it means the same. The real root of the confusion is in *input*. `scanf`
  takes addresses, so no promotion happens and `float *` and `double *` must be
  distinguished — `%f` is `float *` and `%lf` is `double *`. It is the most famous
  asymmetry of these two functions: the same letter meaning different things
  depending on the direction.
]

== The input format string — what differs

The formats of the `scanf` family overlap in their letters with output and so look
like the same language, but what they do is the opposite and their rules differ.
Five points of difference.

*First, the argument is not a value but the address of a place to hold it.* Leave
out the `&` and it mistakes an integer for an address and tries to write at that
address — the reason the `&` met in chapter 25 is compulsory here.

*Second, whitespace in the format means "any number of whitespace characters (none
is fine too)".* In output a space is simply one space, but in input it is *an
instruction to skip*. Most conversions skip leading whitespace by themselves — the
exceptions are `%c` and `%[`, which read whitespace as characters too.

*Third, ordinary characters in the format must match the input exactly.* `"x=%d"`
requires the input to begin with `x=`. If it does not match it stops with *a
matching failure*.

*Fourth, the return value is not the number of characters printed but the number
of items successfully assigned.* So when three were to be read and 2 comes back,
one was not filled, and that argument *is left untouched*. If there was no input
at all, EOF (a negative value) comes back — it must be distinguished from 0.

*Fifth, always give `%s` a maximum width.* A `%s` without a width writes without
knowing the destination's size, the same danger as chapter 40's `gets`.

#demo("examples-en/ch56/scanspec.c")

Going through the output line by line makes the rules visible. `mismatch` and
`nonnum` both have `k=0`, and what matters is that the argument remains as it was —
use it without checking the value and you mistake *the previous value* for new
input. `empty`'s `-1` means "the input has ended", not "the format was wrong".
`partial` is the case where only the leading integer was read and it stopped
after. `set`'s `%15[^,]` means "at most 15 characters that are not a comma", used
for cutting a line with separators.

#dtable(
  columns: 3,
  [*type*], [*input format*], [*argument*],
  [`int`], [`%d`], [`int *`],
  [`unsigned`], [`%u` `%x`], [`unsigned *`],
  [`long`], [`%ld`], [`long *`],
  [`long long`], [`%lld`], [`long long *`],
  [`size_t`], [`%zu`], [`size_t *`],
  [`float`], [`%f`], [★ `float *`],
  [`double`], [`%lf`], [★ `double *`],
  [`long double`], [`%Lf`], [`long double *`],
  [one character], [`%c`], [`char *` (it reads whitespace too)],
  [a word], [`%99s`], [`char[100]` — width compulsory],
  [a set of characters], [`%15[^,]`], [`char[16]`],
  [skipping], [`%*d`], [read but do not store],
)

#realcase[
  What one format brought down — the format string vulnerability
][
  We complete here the accident chapter 22 brushed past by name only. Code that
  puts a user-given string straight into the format position (`printf(user)`) lets
  an attacker input `%s` or `%x` and read the stack, and in the days when the old
  `%n` (which *writes* the number of characters printed to where the argument
  points) was still alive it led even to arbitrary memory writes. Around 1999 this
  class was discovered on a large scale and several servers including wu-ftpd were
  remotely compromised. The lesson is one line — *the format string must always be
  a constant written by the program, and user input goes in only as an argument*
  (`printf("%s", user)`).
]

#qa[
  Then may input parsing always be done with `sscanf`?
][
  For simple formats it is enough. But `sscanf` does not tell you *where and why it
  failed* — all it gives back is the number of successful items, so "the third
  field was not a number" and "the line ended early" cannot be distinguished.
  Moreover it does not detect integer overflow (give `99999999999` to a `%d` and it
  is outside the contract), and you cannot know how many characters were consumed
  up to the failing place either. When the format grows complex and the input came
  from somebody else, a tool is needed in which failure appears as a value and the
  remaining input can be held in the hand — Part XII's scanner is that answer.
]

== The places to beware

Gathering the traps this book has met along the way, from the library's point of
view, gives this.

- *Functions that do not take a size* — the `strcpy`, `strcat` and `sprintf`
  families do not know the size of the destination vessel (chapter 40's `gets` was
  the extreme). The alternatives are the editions that take a size (`snprintf`) or
  components that manage the boundary.
- *Functions whose return value reports failure* — `malloc`, `fopen` and `fgets`
  give null on failure (chapter 48's discipline: do not use without checking).
- *The global state called `errno`* — many functions leave the reason for failure
  in a global variable. Some do not clear it even on success, so the convention of
  "set it to 0 just before the call and read it just after" must be kept — a
  textbook case showing the inconvenience of global mutable state (chapter 41).
- *Locale- and culture-dependent functions* — character functions such as
  `toupper` and `strtod`'s interpretation of the decimal point behave differently
  according to the locale setting. When handling a data format it is safer to use
  locale-independent processing (the same grain as chapter 9's "do not guess the
  encoding").
- *Functions that return a static buffer* — the `asctime` and `strtok` families
  reuse a fixed place inside, so the next call overwrites the previous result. It
  is especially dangerous in a program running along several strands
  (chapter 12's multicore).

#qa[
  Then is the standard library so old that it is better not used?
][
  Not at all — *that it is everywhere* is an overwhelming virtue. Components
  existing with the same contract on every platform and every compiler are only
  these, and most of this book's examples ran with the standard library alone. The
  right attitude is not "do not use it" but *knowing which function has which
  contract and choosing accordingly* — choosing the editions that take a size,
  checking return values, and keeping the conventions when using functions that
  lean on global state. And laying components with checking built in over the
  repeated danger zones (string assembly, input parsing) — the next chapter is that
  practice.
]

The map is spread out. The next part (Part XI) walks the regions of this map one
by one and faces head on the places where accidents really happen in each header —
streams and files, strings, conversion and allocation, characters and locales,
numbers and time, and diagnosis and control.
