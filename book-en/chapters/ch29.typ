#import "../../book/lib.typ": *

= Implicit conversions — promotion and the usual arithmetic conversions

#prereq(
  ([chapter 27, Integers], [integer types differ in width]),
  ([chapter 28, Integer operations], [an operation happens between one type]),
)

#deepqa[
  Chapter 7 taught sign extension (widening 8 bits to 16), and the end of
  chapter 28 brushed past "the value in a smaller container is automatically
  widened before the calculation." Then is the result type of `char + char`
  char?
][
  No — it is int. And that fact is this chapter's starting point. C's arithmetic
  *does not happen on small integer types*. Everything is widened to int (or
  something larger) before the calculation, and the result is that wider type.
  Why that is, and how far it goes, is this chapter.
]

#organizer[
  When C makes values of different types meet, it converts them *silently*. This
#idx("integer promotion")  chapter gathers those invisible conversions in one
  place — integer promotion, the usual arithmetic conversions, and the default
  promotions of variadic arguments. Scattered, each is a riddle; gathered, they
  are one system of rules.
]

#chapter-questions()

The words this chapter leans on throughout --- *arithmetic type*, *integer type*,
*what integer promotion acts on* --- are as defined in chapter 26. If they blur,
open that map again.

== Rule 1 — integer promotion

*Integer promotion*: when a value of an integer type smaller than int — `char`,
`short`, `bool`, a bit-field — takes part in an arithmetic operation, it is
*widened to int* before the calculation (to int if int can hold all its values,
otherwise to unsigned int).

#demo("examples-en/ch29/conv.c")

The first line is the check — two `signed char` values of 100 were added and the
result is 200. The reason a value that does not fit in a char did not overflow is
that the addition happened not in char's world but in int's.

The reason lies in the machine (chapter 11). A CPU's arithmetic circuits and
registers are built to handle integers around the word size, so having separate
arithmetic just for small types would be inefficient. C took that reality into
the language as a rule — small types are *units of storage*, not units of
calculation.

== Rule 2 — the usual arithmetic conversions

#idx("usual arithmetic conversions")If the two operands still differ in type
after promotion, the *usual arithmetic conversions* settle on one common type.
The order to remember in practice:

- If one side is floating, go to the floating side (`long double` > `double` >
  `float`).
- If both are integers — go to the wider one. At equal width, *the unsigned side
  wins*.

Spelled out in the standard's own order, the integer rules are four steps.

#figure-svg("conversions", caption: [Follow the four in order. Most accidents happen at step 3.])

That last line is the source of the trap. The second part of the demonstration
is it in the flesh — read `-1` through unsigned eyes and it becomes an enormous
positive number over four billion (exactly chapter 7's modular world). So a
comparison like `-1 < 1u` comes out *false*, against intuition. That is why
mixing signed values into array indices or size calculations (the result of
`sizeof` is the unsigned `size_t`!) causes silent accidents.

Fortunately the compiler guards this trap well — the warnings switched on in
chapter 17 point at sign-mixed comparisons (one example in this book was caught
by that warning and rewritten). Reduced to a rule: *do not mix signed and
unsigned in one comparison.* Use the `size_t` family consistently for sizes and
indices, or state the intent with an explicit cast.

== Rule 3 — default promotions for variadic arguments

The third conversion happens in functions whose argument count is not fixed —
*variadic functions* such as `printf`. Arguments passed into a position where the
prototype states no type undergo *default argument promotions*: `float` becomes
*`double`*, and small integer types become *int*.

The demonstration's last line is the check — a `float` value printed with `%f`
comes out fine. The format `%f` in fact expects a double, and the float argument
arrived as a double after promotion (which is why printf has no float-specific
format at all). The "contract between format and materials" learned in
chapter 22 has this promotion rule as a hidden clause — the full contract, and
how to write variadic functions yourself, is faced head on in chapter 56.

#realcase[
  The conversion that destroyed a rocket — Ariane 5, 1996
][
  There is an event that shows how heavy implicit and explicit conversions can
  be. In 1996 the European Space Agency's Ariane 5 rocket exploded 37 seconds
  after its first launch. The heart of the investigation's finding was one line
  of conversion — the inertial navigation unit computed horizontal velocity as a
  64-bit floating-point number, and there was code moving that value into a
  *16-bit signed integer*. On the predecessor Ariane 4 that velocity never
  exceeded the 16-bit range and it was safe, but on the faster Ariane 5 the value
  overflowed its container. The failure of the narrowing conversion (chapter 7's
  truncation) raised an exception, and with that exception unhandled the
  navigation computer stopped, whereupon the rocket lost attitude and
  self-destructed. The loss ran to hundreds of millions of dollars. Reused code
  meeting *a new range of values* broke the contract — the most expensive
  confirmation of this chapter's sentence, that a conversion changes the value.
]

#misconception[
  "A cast does not change the value, only the interpretation"
][
  For pointer casts (chapter 37) that is broadly true, but *a cast between
  arithmetic types changes the value itself*. `(int)3.9` becomes 3 (the
  fractional part discarded), `(char)300` does not fit the container and is
  truncated (chapter 7's narrowing), and `(unsigned)-1` becomes an enormous
  positive number. C's cast means not "read these bits as that type" but
  "*convert* this value into a value of that type" — if you want to leave the
  bits alone and change only the eye, chapter 46's union or `memcpy` is that
  channel. Not writing the two demands with the same syntax is one of C's few
  kindnesses.
]

#qa[
  Must all these rules be memorised?
][
  Three lines are enough — *small integers are promoted to int; when mixed, the
  wider and the unsigned side wins; in variadic arguments float becomes double.*
  Leave the rest of the detail to the appendix's tables, and in practice two
  habits stand in for memorising rules: keeping warnings on (chapter 17), and
  *stating a deliberate conversion with a cast*. The danger of implicit
  conversion lies not in the complexity of the rules but in their being
  *invisible*, so making them visible is the best defence.
]

We have the map of conversions. From the next chapter come the tools of flow —
beginning with the booleans and comparisons that turn judgement into a value.
