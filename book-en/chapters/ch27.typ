#import "../../book/lib.typ": *

= Integer operations — division, bits

#prereq(
  ([chapter 7, Representing integers], [bits and shifts]),
  ([chapter 26, Integers], [the finiteness of integers]),
)

#deepqa[
  Chapter 20 only announced that "7 / 2 is not 3.5." Answering for yourself with
  chapter 7's knowledge — why *can* the result of dividing integer containers
  not be 3.5?
][
  Because there is nowhere to hold 3.5. The value set of an integer type has 3
  and 4 and nothing in between — so division between integers must give an
  integer, and the fractional part must be *discarded* one way or the other. The
  question is "which way", and that rule is this chapter's first section.
]

#organizer[
#idx("division")  We keep the promise deferred in chapter 20 — the truth about
  integer division `/` and remainder `%`, including the rule for negative
  numbers. And the bit operations learned as concepts in chapter 7 join as C
  operators. The first step into conversions is taken here too.
]

#chapter-questions()

== Division and remainder — the direction of discarding

The demonstration first.

#demo("examples-en/ch27/divmod.c")

For positive numbers it is as intuition says — `7 / 2` is 3, remainder 1. Where
a rule is needed is negatives. C *truncates toward zero* — `-7 / 2` is $-3.5$
discarded towards zero, giving $-3$, and the remainder is correspondingly $-1$.
That is a different choice from "the quotient and remainder of mathematics"
(where the remainder is always non-negative), so code using negative remainders
causes accidents if it does not know the difference.

#mathbox[
  The division-remainder recovery invariant
][
  Whatever the direction, there is one equation C always keeps:

  $ (a / b) times b + (a % b) = a $

  Quotient and remainder are defined *as a pair* so as to satisfy this equation —
  the demonstration's last line is a check of it. That truncating toward zero
  makes the remainder's sign follow the dividend ($a$) is also a consequence of
  this equation. A line of history in addition — up to C89 the direction of
  discarding was allowed to differ by implementation, and C99 pinned it to
  "toward zero." Chapter 7's "where machines diverge the standard leaves a
  blank" is here another case of practice promoted to promise once the machines
  converged.
]

#qa[
  What happens if you divide by zero?
][
  Outside the contract — undefined behaviour. It is one of the rare cases where
  what is undefined in mathematics is undefined in C too, but the result is not
  as well-behaved as in mathematics: on many machines the program collapses on
  the spot, and under some optimisations stranger things happen (chapter 49).
  Checking for zero before dividing is the programmer's job — and having learned
  branching in chapter 30, we will be able to write that check in code.
]

== Bit operations — chapter 7's world, in C's syntax

The bit handling learned as concepts in chapter 7 joins as operators — AND `&`,
OR `|`, XOR `^`, complement `~`, and the shifts `<<` and `>>`. The latter part
of the demonstration is a taste: `5 & 3` is `101 & 011 = 001`, so 1; `5 | 3` is
`111`, so 7; and `1 << 4` is $2^4 = 16$, exactly as chapter 7 promised.

The basic pattern in practice is exactly the *shift plus mask* foreshadowed in
chapter 7 — push to the position you want (`<<`, `>>`) and keep only the bits
you need (`&`). But take two rules along with it. First, *do bit operations on
unsigned types* — shifts of signed numbers carry the traps seen in chapter 7 (at
least the width, left-shifting a negative = outside the contract), so playing on
`unsigned` or `uint32_t` is the safe practice. Second, `&` (bitwise AND) and
`&&` (logical AND, next chapter) are completely different operators — one
character changes the entire value.

== The contracts these operators make

Here the operators met so far are gathered under the eye of *contract*: what each
takes, and where the contract ends (the full table is in appendix A).

#dtable(
  columns: 3,
  [*operator*], [*what it demands of its operands*], [*outside the contract / grey zone*],
  [`/` `%`], [`%` takes *integers only*; `/` also takes reals], [a zero divisor is *outside the contract*. So are `INT_MIN / -1` and `INT_MIN % -1` (the quotient does not fit an int)],
  [`+` `-` `*`], [arithmetic types], [signed integer overflow is *outside the contract* (chapter 26); the unsigned side wraps],
  [`& | ^ ~`], [*integers only*], [on a signed type they reach the sign bit — use unsigned],
  [`<<` `>>`], [*both operands integers*], [see the table below],
)

Shifts have three grey zones, so they get their own table. These three have not
changed with the editions.

#dtable(
  columns: 3,
  [*situation*], [*verdict*], [*explanation*],
  [`x << n` or `x >> n` with `n < 0` or `n >= width`], [*outside the contract*], ["width" is the bit count of the promoted left operand. With a 32-bit `int`, `1 << 32` is already outside],
  [`x` signed and *negative* in `x << n`], [*outside the contract*], [still so in C23],
  [`x` signed and *positive* but the result does not fit], [*outside the contract*], [`1 << 31` on a 32-bit `int` — write `1u << 31`],
  [`x` signed and *negative* in `x >> n`], [*implementation-defined*], [usually an arithmetic shift (the sign preserved), but that is not the standard's promise],
)

#misconception[
  "C23 mandated two's complement, so the negative-shift problem is gone"
][
  Two's complement representation was indeed mandated (chapter 68). The shift
  clause, however, was left alone — *left-shifting a signed negative value is
  still outside the contract in C23*, and *right-shifting a negative value is
  still implementation-defined*. That gcc and clang do an arithmetic shift is a
  promise of those implementations, not of the standard.

  So this book's rule stands whatever the edition — *shift on unsigned types*.
  If a signed value must be shifted, move it to unsigned, shift, and move it
  back; and always check `0 <= n < width`, where the width is
  `sizeof(x) * CHAR_BIT`.
]

== Conversion — crossing between containers

With a family of types (chapter 26) comes a new question — what happens when
containers of different kinds are mixed in a calculation? C's answer is
*implicit conversion*: the value in a smaller container is automatically widened
#idx("sign extension")into a larger one before the calculation (chapter 7's sign
extension is exactly what happens then), and when an integer meets a
floating-point number the integer is promoted to floating point. Mostly this
does what you meant, but automatic also means *invisible* — comparisons mixing
signed with unsigned in particular are a classic trap (a negative number turns
into an enormous positive one), and a representative place where compiler
warnings (`-Wall`) protect you.

When you want the conversion *stated*, use the cast notation — `(double)7 / 2`
means "move 7 into a floating container and then divide", giving 3.5. The full
rules (integer promotion, the usual arithmetic conversions) are left as reference
material in the appendix; the text's rules are two: *state the intent of a mixed
calculation with a cast, and avoid comparisons that mix signs.*

We have the containers of integers (chapter 26) and their operations
(chapter 27). From the next chapter it is *flow* — starting with the values that
compare and decide, the booleans.
