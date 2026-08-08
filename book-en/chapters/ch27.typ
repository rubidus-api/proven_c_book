#import "../../book/lib.typ": *

= Integers — a world of finite numbers

#prereq(
  ([chapter 26, The map of types], [where the integer and basic types sit]),
  ([chapter 7, Representing integers], [sign and overflow]),
  ([chapter 23, Declaring variables], [a type is the shape of the vessel]),
)

#deepqa[
  Chapter 7 said that overflow of unsigned integers is "defined wrap-around"
  while signed overflow is outside the contract. But is the `int` we made in
  chapter 23 signed or unsigned — which world have our variables been in all
  along?
][
  `int` is a *signed* integer — that is, our variables have been in the world of
  "outside the contract if it overflows." Fortunately the examples so far never
  came near the ±2.1 billion container, but it is time to draw that boundary
  exactly. The kinds and sizes of containers, where the boundary lies, and the
  rule when it is crossed — that is this chapter.
]

#organizer[
#idx("integer types")  Part VI forges values and governs flow. Its first chapter
  faces C's integers head on — how the world of representation learned in
  chapter 7 appears as C's family of types, what face overflow wears in
  practice, and the modern C practice (`<stdint.h>`).
]

#chapter-questions()

This chapter digs into one cell of the map opened in chapter 26 --- the family passed
over there in the single phrase "the signed and unsigned integer types".

== The family of integer types

C's integer types are not `int` alone but a family. The basic members on the
signed side, in order of size, are `char` (1 byte — the "byte = character" story
of chapter 4 survives in the name), `short`, `int`, `long` and `long long`, and
putting `unsigned` in front of each gives its unsigned partner (`unsigned int`
and so on). The standard fixes only each type's *minimum* size — `int` at least
16 bits, `long` at least 32 — so the exact size is the platform's business (on
mainstream systems `int` is 32 bits).

If "it differs by machine" sounds uneasy, that is an accurate instinct — which
is why modern C practice is to use types with the size fixed in the name. The
`<stdint.h>` toolbox's `int32_t` (exactly 32 bits, signed), `uint8_t`, `int64_t`
and the like. Wherever size is part of the contract — file formats,
communication, everywhere chapter 5's endianness matters — that side is standard
practice. C23 added `_BitInt(N)` on top: an integer whose bit width you write
yourself. This book's examples use `int` by default for simplicity on the page,
but switch to the `<stdint.h>` family in scenes where size matters.

== A map of the basic types — minimum and actual ranges

Here we gather in one place exactly what C's basic types are and how large each
is. This section is a place to look things up.

*Integer types* come in five tiers, each with a signed and an unsigned version —
`char`, `short`, `int`, `long`, `long long`. To these are attached `bool` (C99)
and the character-specific types (`char16_t`, `char32_t`, `wchar_t`).
*Floating types* are three — `float`, `double`, `long double`.

What the standard pins down is *not size but minimum range*. The common saying
"int is 4 bytes" is not a sentence of the standard but an observation that most
platforms are like that today.

#dtable(
  columns: 4,
  [*type*], [*minimum range guaranteed*], [*minimum width*], [*limit macros*],
  [`signed char`], [−127 to +127], [8 bits], [`SCHAR_MIN` `SCHAR_MAX`],
  [`unsigned char`], [0 to 255], [8 bits], [`UCHAR_MAX`],
  [`char`], [one of the two above (implementation-defined)], [8 bits], [`CHAR_MIN` `CHAR_MAX`],
  [`short`], [−32767 to +32767], [16 bits], [`SHRT_MIN` `SHRT_MAX`],
  [`unsigned short`], [0 to 65535], [16 bits], [`USHRT_MAX`],
  [`int`], [−32767 to +32767], [16 bits], [`INT_MIN` `INT_MAX`],
  [`unsigned int`], [0 to 65535], [16 bits], [`UINT_MAX`],
  [`long`], [−2147483647 to +2147483647], [32 bits], [`LONG_MIN` `LONG_MAX`],
  [`unsigned long`], [0 to 4294967295], [32 bits], [`ULONG_MAX`],
  [`long long`], [about ±9.2×10#super[18]], [64 bits], [`LLONG_MIN` `LLONG_MAX`],
  [`unsigned long long`], [0 to about 1.8×10#super[19]], [64 bits], [`ULLONG_MAX`],
)

Your eye will go to the minimum range being −127 (not −128). That is because the
old standard permitted all three sign representations (chapter 7); now that C23
has pinned two's complement down, it is effectively −128.

The standard also fixes *the order of sizes*. Widths cannot run against this
order.

```text
char  ≤  short  ≤  int  ≤  long  ≤  long long
```

And `sizeof(char)` is always 1 — because a byte is by definition the size of a
`char` (chapter 4). How many bits are in a byte, though, is told by `CHAR_BIT`,
and the standard guarantees only 8 or more.

The limits of *floating types* are the business of `<float.h>`. Picking only
those in frequent use:

#dtable(
  columns: 3,
  [*macro*], [*meaning*], [*for IEEE 754 double*],
  [`FLT_DIG` `DBL_DIG`], [trustworthy decimal digits], [6 / 15],
  [`FLT_MAX` `DBL_MAX`], [largest representable value], [about 1.8×10#super[308]],
  [`FLT_MIN` `DBL_MIN`], [smallest normalised value], [about 2.2×10#super[−308]],
  [`FLT_EPSILON` `DBL_EPSILON`], [smallest difference distinguishable from 1.0], [about 2.2×10#super[−16]],
  [`FLT_RADIX`], [the base of the exponent], [2],
)

`DBL_EPSILON` is met again in chapter 49 when comparing floating-point numbers —
it is the value used to set the criterion for judging "equal".

#demo("examples-en/ch27/limits.c")

What matters is that this output belongs to *the machine that made this book*.
Run it on another machine and different numbers may appear — and that is exactly
this section's point: *do not assume sizes; ask.*

== The type of an integer constant — the same value, typed by its notation

Chapter 20 showed the four bases and the suffixes for writing an integer constant.
What was deferred there --- *which type the compiler gives that constant* --- can be
faced now that integers have been met.

The rule is "walk a list and take the first type that fits". But *the list differs by
base.*

#dtable(
  columns: 3,
  [*Unsuffixed constant*], [*The candidate list, in order*], [*The point*],
  [decimal (`4294967295`)], [`int` → `long` → `long long`], [*unsigned types are not candidates*],
  [octal, hex, binary (`0xFFFFFFFF`)], [`int` → `unsigned int` → `long` → `unsigned long` → `long long` → `unsigned long long`], [unsigned types are *interleaved*],
)

Adding a suffix narrows the list by hand --- `u` walks only the unsigned ones, `l`
starts at `long`, `ll` at `long long`.

The difference shows up for real.

#dtable(
  columns: 3,
  [*Written*], [*Measured `sizeof`*], [*Type*],
  [`0xFFFFFFFF`], [4], [`unsigned int`],
  [`4294967295`], [8], [`long`],
)

*The same number, a different type.* And once the type differs, everything downstream
differs --- promotion and the usual arithmetic conversions (chapter 29) apply
differently, and comparisons can come out reversed.

#antipattern[
  Writing a bit mask in decimal
][
  ```c
  x & 4294967295      /* becomes an operation with a long */
  x & 0xFFFFFFFFU     /* visibly an unsigned 32-bit mask */
  ```

  Practice writes masks in hexadecimal not only because it reads better. *The type
  differs*, and above all *the number of bits is visible* --- `0xFFFF` is 16 bits and
  `0xFFFFFFFF` is 32, right there in the digit count. Adding `U` to pin the signedness
  as well is the convention (shifting a signed integer is chapter 28's grey area).
]

#qa[
  What happens if the value fits nothing in the list?
][
  If it does not fit even the widest candidate, it goes into an *extended integer type
  the implementation provides*, or, if there is none, it is a constraint violation and
  gets diagnosed. Meeting this in practice has one answer --- *use the fixed-width
  types and their macros* (`UINT64_C(…)`, `<stdint.h>`). "Do not leave the type to the
  compiler" is the same discipline as the rest of this chapter.
]

== Where size is the contract — fixed-width types

There are places where the width must be exactly determined: file formats,
network protocols, hardware registers. In such places use the fixed-width types
of `<stdint.h>`.

#dtable(
  columns: 3,
  [*kind*], [*examples*], [*meaning*],
  [exact width], [`int8_t` `uint16_t` `int32_t` `uint64_t`], [exactly that many bits. not provided if unavailable],
  [minimum width], [`int_least16_t`], [the smallest type that is at least that wide],
  [fastest], [`int_fast32_t`], [at least that wide and fastest for the machine to handle],
  [pointer-sized], [`intptr_t` `uintptr_t`], [an integer able to hold a pointer (mind chapter 14's provenance)],
  [largest], [`intmax_t` `uintmax_t`], [the widest integer],
  [size and difference], [`size_t` `ptrdiff_t`], [the types of a size and of a pointer difference],
)

Each has its limit macros too — `INT32_MAX`, `UINT64_MAX`, `SIZE_MAX`,
`PTRDIFF_MAX` and so on. Their format specifiers come from `<inttypes.h>`'s
`PRId32` family (appendix B).

The rule is simple. *If the meaning is "this machine's natural integer", `int`;
if "a size or an index", `size_t`; if "a width fixed by a format", a
fixed-width type.*

== Seeing the boundary with our own eyes

The edge of a container is told by the `<limits.h>` toolbox. And the
wrap-around of the unsigned world — the one learned on the page in chapter 7 —
can now be run and shown.

#demo("examples-en/ch27/wrap.c")

One new format has joined — unsigned integers are printed with `%u`, not `%d`
(chapter 22's format contract growing along with the family of types). And the
third line is exactly chapter 7's promise: add 1 to the maximum and you get 0 —
the clock has gone round once, and this is *defined* behaviour.

The signed side is another matter. Code that computes `INT_MAX + 1` is outside
the contract (undefined behaviour), so *it cannot even be put into this book's
verification pipeline* — the UBSan equipped in chapter 17 catches exactly this
kind of code at run time. That the same "+1" can be demonstrated on one side and
not on the other is itself eloquent about the difference between the two worlds.

#misconception[
  "Overflow is a problem for special programs that handle large numbers"
][
  Plausible, but the list of real accidents says the opposite. The commonest
  overflows happen in perfectly ordinary places — taking the average of two
  indices as `(a + b) / 2` and having *the intermediate sum* overflow (the famous
  binary search bug, which hid in standard libraries for nearly twenty years); a
  millisecond timer wrapping after 49 days (the Windows 95 49.7-day hang); a size
  calculation `count * size` overflowing and taking an absurdly small piece of
  memory (a classic of security incidents). What they share is that *the
  intermediate calculation overflows, not the final value* — worrying about the
  container is done for every intermediate step of an expression, not for the
  result, and that is why checked arithmetic of the kind proven provides exists
  (chapter 42).
]

#qa[
  It seems strange that `char` is a member of the integer family — is it not a
  character?
][
  In C a character and a small integer are the same thing — as chapter 8 taught,
  a character is a number, and `char` is merely a one-byte integer container
  holding that number. The value of the character literal `'A'` is simply 65. One
  trap in advance — whether `char` is signed or unsigned *differs by platform*
  (it is a third type, neither of the two). So `char` is not used for numeric
  work; when a byte is needed, modern practice uses `uint8_t`. The proper story
  of strings and `char` is in chapter 41.
]

The containers of integers are sorted. The next chapter takes the operations on
those containers — the truth about division, deferred in chapter 20, and the
operators of the bit world learned in chapter 7, joining C's syntax.
