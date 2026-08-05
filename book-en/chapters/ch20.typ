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
chapter 29, division in chapter 27 — why division was put off, in a moment).

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
  a place in chapter 27. Better to treat it squarely, all at once, than to
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
chapter 32, once more material is in place. For now one practical rule suffices:
*do not cram order-sensitive work into one statement; split the statements.*

To summarise — a notation writing a value in source is a literal, that which is
evaluated into a value is an expression, and the order of binding is governed by
arithmetic convention and parentheses, while the order in time inside one
statement may differ. In the next chapter we learn the most important device
that *consumes and produces* values — how to call a function. Hello world's
heart, `printf(...)`, is finally treated head on.
