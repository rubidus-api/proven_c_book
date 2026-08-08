#import "../../book/lib.typ": *

= Expressions — the things that become values

#prereq(
  ([chapter 7, Representing integers], [the representation of integers]),
  ([chapter 19, The structure of a program], [where statements and expressions sit]),
)

#deepqa[
  Chapters 7 and 8 taught how to hold numbers *in the machine* (two's
  complement, IEEE 754). So when a number is written *in source code* — when you
  write `12`, say — when does it become the machine's bits?
][
  At compile time. The `12` in the source is just two characters (in the terms
  of chapter 9, the characters `1` and `2`), and the compiler reads it,
  translates it into a two's-complement bit pattern and plants it in the
  executable. A notation for a value written in source is called a *literal* —
  meaning "as the letters say." That the bridge between the world of letters and
  the world of bits is the compiler — chapter 16's relay is at work here too.
]

#organizer[
  We go inside the statement. How to write values in source code (literals), how
  to weave values into calculations (operators and expressions), and the order
  of calculation (precedence and parentheses) — but, following this book's
  principle, only the minimal set: addition, subtraction, multiplication and
  parentheses. That alone takes us surprisingly far.
]

#chapter-questions()

== Literals — values written in source

We have met literals several times already. The `0` of hello world is an integer
literal, and `"Hello, world!\n"` is a *string literal* — the characters inside
the quotation marks become a value "as they are." Add a decimal point to an
integer literal (`3.5`) and it becomes a floating-point value from chapter 8.

#qa[
  Then if I write `0.1` in the source, does the machine hold exactly 0.1?
][
  It does not — exactly as chapter 8 taught. The compiler translates the literal
  `0.1` into its *nearest neighbour* (`3FB9 9999 9999 999A`). A literal is "a
  notation writing the value as it is", but if that value is not on the grid of
  representable numbers, the approximation has already happened at translation.
  Writing it in the source does not make it exact — chapter 8's lesson holds in
  the world of source code too.
]

== Writing constants — the whole map

Having met literals in the previous section, this is the place to gather *every
notation for writing a value in source* at once. The standard scatters them through
its lexical clauses (§6.4.4, §6.4.5), and what trips people in practice is nearly
always one of three things: a *prefix, a suffix or an escape*.

This section is *for reference*. There is no need to memorise it now --- take away
the map that these notations exist and come back when you need them. Read the
listing in that spirit too: skimming the output and matching notation to result is
enough.

#demo("examples/ch20/constants.c")

== Integer constants (§6.4.4.1)

#dtable(
  columns: 4,
  [*Base*], [*Notation*], [*Example*], [*Note*],
  [decimal], [starts with `1`\~`9`], [`1234`], [],
  [octal], [starts with `0`], [`0755` = 493], [*the commonest trap* — `08` is an error],
  [hexadecimal], [`0x` or `0X`], [`0xFF` = 255], [],
  [binary], [`0b` or `0B`], [`0b1010` = 10], [*added in C23*],
)

Letters can follow too --- a *suffix*, as in `10L`, `1U`, `3ULL`. It marks not the
value but the *type*.

#dtable(
  columns: 2,
  [*Suffix*], [*What it marks*],
  [`u` `U`], [an unsigned type],
  [`l` `L`], [`long` or wider],
  [`ll` `LL`], [`long long` or wider],
  [`wb` `WB`], [`_BitInt` (*C23*)],
)

For now, *that a suffix changes the type* is all you need. The type names above, and
the exact rule for which type the compiler picks when there is no suffix, come in
*chapter 27 once integers have been met properly*, in "The type of an integer
#idx("constant")  constant". One thing in advance --- writing the same value in decimal or in
hexadecimal can give it a different type.

#qa[
  Why the advice never to use a lowercase `l`?
][
  Because in many fonts `1` (one) and `l` (ell) look almost identical. `10l` being
  read as `101` has really happened. *Suffixes in capitals* --- `10L`, `1UL` --- is the
  long-standing convention, and this book follows it. The `0x` prefix is conventionally
  lowercase, which looks like the opposite rule, but the reason is the same: whichever
  is easier to tell apart.
]

== The C23 digit separator `'`

A notation for breaking up long numbers arrived in C23. It has no effect on the
value --- in the standard's words it is *ignored when determining the value of the
constant.*

The standard's own example shows the traps along with the feature. Measured:

#dtable(
  columns: 3,
  [*Written*], [*Result*], [*Why*],
  [`12'34`], [`1234`], [a separator goes only between digits],
  [`0b11'10'11'01`], [`237`], [binary takes it too],
  [`0x1'2'3'4AB'C'D`], [`305441741`], [so does hexadecimal],
  [`0x'FF`], [*error* — "digit separator after base indicator"], [it may not follow `0x`],
  [`'1'2`], [*error*], [read as the character constant `'1'` followed by `2`],
)

The last row is this notation's one danger. *A separator is a separator only between
two digits*; at the front it is read as a single quote --- the start of a character
constant.

== Character constants (§6.4.4.4)

The type names read fully only after chapter 9's character sets and chapter 27's
integers --- for now just see that *the prefix settles the type*.

#dtable(
  columns: 4,
  [*Notation*], [*Type*], [*Measured size*], [*Note*],
  [`'a'`], [*`int`*], [4], [★in C a character constant is not a `char`],
  [`u8'a'`], [`char8_t`], [1], [*C23*. Must be one UTF-8 code unit],
  [`u'a'`], [`char16_t`], [2], [one UTF-16 code unit],
  [`U'a'`], [`char32_t`], [4], [one UTF-32 code unit],
  [`L'a'`], [`wchar_t`], [4 (Linux), 2 (Windows)], [the wide literal encoding (chapter 9)],
  [`'ab'`], [`int`], [4], [*value implementation-defined*. GCC warns with `-Wmultichar`],
)

#misconception[
  "`'a'` is a `char`, so its size is 1"
][
  True in C++ and *false in C.* Measured, the very same `sizeof('a')` is 4 in C and 1
  in C++.

  The standard's sentence is "an integer character constant has type `int`"
  (§6.4.4.4p11). Its value is "what results when an object of type `char` holding that
  character is converted to `int`", so *the value is what you expect and only the type
  is wider.*

  Where it shows is mostly `sizeof`, `_Generic`, and overloading on the C++ side. Write
  only C and the practical harm is near zero, but in a header used from both languages
  it must be known.
]

The escapes are exactly these (§6.4.4.4).

#dtable(
  columns: 3,
  [*Kind*], [*Notation*], [*Note*],
  [must be escaped], [`\'` `\\`], [the single quote and the backslash *must* take this form],
  [optional], [`\"` `\?`], [inside a string `\"` is needed],
  [non-graphic characters], [`\a` `\b` `\f` `\n` `\r` `\t` `\v`], [their meanings are defined in §5.2.3],
  [octal], [`\` + octal digits], [*at most three digits*],
  [hexadecimal], [`\x` + hex digits], [★*there is no digit limit*],
  [universal character names], [`\uXXXX` `\UXXXXXXXX`], [naming characters outside the basic set],
)

#antipattern[
  `"\x411"` --- a hex escape eats the letter after it
][
  The standard nails it: *each octal or hexadecimal escape sequence is the longest
  sequence of characters that can constitute the escape sequence* (§6.4.4.4p7). Octal
  stops at three digits; *hexadecimal does not stop.*

  ```c
  "\x411"      /* not 'A'(0x41) then '1', but a request for 0x411 */
  ```

  Measured, GCC warns `hex escape sequence out of range`. The fix is *to split the
  string and let it join* --- adjacent string literals concatenate (below), so
  `"\x41" "1"` is exactly `"A1"`.
]

== Floating constants (§6.4.4.3)

The decimal form must have *either a decimal point or an exponent part.* So `1.`,
`.5` and `1e3` are all valid, while `1` is an integer constant.

Floating constants take suffixes too (what the types are comes in chapters 8 and 49).

#dtable(
  columns: 3,
  [*Suffix*], [*Type*], [*Measured size*],
  [(none)], [`double`], [8],
  [`f` `F`], [`float`], [4],
  [`l` `L`], [`long double`], [16 (x86-64 Linux)],
  [`df` `dd` `dl`], [`_Decimal32` / `_Decimal64` / `_Decimal128` (*C23*)], [4 / 8 / 16],
)

There are also *hexadecimal floating constants* (C99) --- `0x1p-3` is exactly 0.125.

#qa[
  Why must a hex float have the `p` exponent, and why use one at all?
][
  Because `e` is unavailable: in hexadecimal `e` is *the digit 14* and cannot start an
  exponent. So `p`, meaning a binary exponent, was given its own place, and *`p` cannot
  be omitted* --- without it there is no telling where the significand ends. `0x1p-3` is
  "1 × 2#super[−3]".

  The reason to use one is *exactness*. As chapter 8 showed, decimal `0.1` does not sit
  exactly in binary, whereas the hex notation transcribes the binary representation
  itself, so *no rounding happens in translation.* Hence its use in floating-point
  tests' expected values, in the standard library's tables of constants, and in papers
  about floating point. `printf`'s `%a` prints in the same notation (appendix B).
]

Worth noting too that the suffix changes the value. Measured, `(double)0.1f == 0.1` is
*false* --- `0.1f` is the nearest value on the `float` grid and `0.1` the nearest on the
`double` grid, and those are different numbers (chapters 8 and 49).

== String literals (§6.4.5)

#dtable(
  columns: 4,
  [*Notation*], [*Element type*], [*Encoding*], [*Measured `sizeof`*],
  [`"가"`], [`char`], [the literal encoding (chapter 9)], [4 (3 UTF-8 bytes + NUL)],
  [`u8"가"`], [`char8_t`], [*always UTF-8*], [4],
  [`u"가"`], [`char16_t`], [UTF-16], [4 (1 code unit + NUL)],
  [`U"가"`], [`char32_t`], [UTF-32], [8],
  [`L"가"`], [`wchar_t`], [the wide literal encoding], [8 (Linux)],
)

Four properties go together.

+ *A NUL is appended.* `sizeof "abc"` is not 3 but *4*.
+ *Adjacent literals join into one* (translation phase 6). `"hello, " "world"` is one
  string. It is the standard way to split a long string across lines, and the way out
  of the `\x` trap above. But *the prefixes must not be mixed* --- `u"a" U"b"` is a
  compile error (a constraint violation).
+ *A NUL inside does not cut the array short.* `"a\0b"` has `strlen` 1 and `sizeof` 4
  --- the string functions stop, the data is all there.
+ *Modifying one is undefined behaviour.* Measured, it usually dies at run time (it is
  placed in a read-only section). So take string literals as *`const char *`.*

#antipattern[
  `char *s = "abc"; s[0] = 'X';`
][
  It compiles (in C the type of a string literal is `char[N]`, not `const`), and it
  dies when run --- SIGSEGV in the measurement.

  C++ closed this off entirely (a string literal is `const char[N]` there, so the
  assignment is an error). C left it open for compatibility with old code, so *the
  habit has to close it* --- take it as `const char *s = "abc";` and the compiler
  catches it. Turning on `-Wwrite-strings` is another way.
]

== Things that look like constants

#dtable(
  columns: 3,
  [*Notation*], [*What it really is*], [*More*],
  [`RED` (an enumeration constant)], [*an integer constant*, of type `int`], [chapter 54 — it lives in the ordinary-identifier yard],
  [`nullptr`], [a *keyword* of type `nullptr_t` (*C23*)], [chapter 36],
  [`true` `false`], [*keywords* yielding `bool` values (*C23*)], [chapter 30],
  [`(int[]){1,2,3}` a compound literal], [not a constant but an *object* — you can take its address], [chapter 46],
  [`#define N 100`], [not a constant but *token replacement*], [chapter 56],
  [`constexpr int n = 10;`], [*C23*'s real constant — usable in a constant expression], [chapter 23],
)

The last two rows pay off in practice. A macro has neither type nor scope
(chapter 56), and a `const int` is *not a constant expression* in C --- the place
where C and C++ part. But "cannot be used" is less accurate than *where* it cannot be,
so here it is, measured.

#dtable(
  columns: 2,
  [*Given `const int n = 10;`, writing*], [*Result (GCC, C23)*],
  [`int a[n];` inside a block], [accepted --- but as a *variable length array*, not a constant one],
  [`int a[n];` at file scope], [error --- `variably modified 'a' at file scope`],
  [`static int a[n];` inside a block], [error --- `storage size of 'a' isn't constant`],
  [`case n:`], [error --- `case label does not reduce to an integer constant`],
)

That is, it fails wherever *a real constant expression* is required. C23's `constexpr`
came in to fill that place.

== Expressions — the things that are evaluated into values

Weave literals together with operators and you have an *expression*, as in
`2 + 3 * 4`. The definition takes one sentence — *that which is calculated
(evaluated) into a value.* This property of "becoming a value" is the whole of
an expression, and half of the eye for reading C from here on. Wherever a value
is needed in code, an expression may go there — a single literal is the simplest
expression of all.

This chapter has only four operators — plus `+`, minus `-`, times `*`, and
parentheses `( )`. Following this book's spiral principle, the remaining
operators join in the chapters where they become necessary (comparison in
chapter 30, division in chapter 28 — why division was put off, in a moment).

Here is the demonstration. The `%d` in the example below is a mark meaning
"print an integer value here in decimal"; its formal explanation is in
chapter 22 — for now it is only a window for seeing results.

#demo("examples-en/ch20/expr.c")

== Order — precedence, and the practice of parentheses

The first line of the demonstration is 14 because the multiplication was
calculated *before* the addition. Exactly the convention of the mathematics
lesson, and C has fixed such a ranking of "who goes first" — *precedence* — for
every operator.

Here we state this book's recommendation in advance. *Do not memorise the
precedence table; use parentheses.* C has dozens of operators and the ranking
table runs to more than fifteen rows — even people who have memorised it all get
confused and cause accidents. Make the order explicit with parentheses, as in
the second line, and there is nothing to memorise and nothing to misread. The
full table is in the appendix as reference material; the text goes with
"arithmetic by the mathematical convention, parentheses everywhere else."

#qa[
  Why was division put off? Leaving one of the four operations out feels odd.
][
  Because integer division *differs from division in mathematics*. In C `7 / 2`
  is not 3.5 but 3 — division between integers is the quotient with the
  fractional part discarded, and it only makes sense understood as a set with
  its partner operator `%` (remainder). Including the exact rule of that
  discarding (which way it goes for negative numbers), it is a subject worth
  treating properly on top of chapter 7's world of integers, so it has been given
  a place in chapter 28. Better to treat it squarely, all at once, than to
  introduce it half-heartedly now and create the misconception "I thought 7/2
  was 3.5."
]

#qa[
  Is `printf("hello")` a value too? A function call is written in the position
  of an expression.
][
  Good eye — it is. Calling a function is itself an expression, and therefore
  becomes a value. What that value is and where it goes is exactly the next
  chapter's subject. That one question has already half-opened the next
  chapter's door.
]

== One caution planted early — the order in time may differ

About precedence there is one caution to plant as a seed. Precedence is a rule
of *binding* — who is whose *material* — not of *what the machine calculates
first in time*. Which of the left and right materials of an addition is
calculated first, for instance, is not fixed by the standard and *may differ
between compilers*. In an expression like `2 + 3` it does not matter at all, but
when calculating a material leaves a trace such as output, the order of those
traces may differ.

Be reassured on one point, though — the order *between statements* is guaranteed
absolutely. Chapter 19's "one statement at a time, top to bottom" is a contract.
What can waver is only the inside of a single statement, and the exact rules
there (side effects, the notion of sequence points) are faced head on in
chapter 33, once more material is in place. For now one practical rule suffices:
*do not cram order-sensitive work into one statement; split the statements.*

To summarise — a notation writing a value in source is a literal, that which is
evaluated into a value is an expression, and the order of binding is governed by
arithmetic convention and parentheses, while the order in time inside one
statement may differ. In the next chapter we learn the most important device
that *consumes and produces* values — how to call a function. Hello world's
heart, `printf(...)`, is finally treated head on.
