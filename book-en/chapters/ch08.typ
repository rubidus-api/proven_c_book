#import "../../book/lib.typ": *

= Representing numbers — the contract called IEEE 754

#prereq(
  ([chapter 7, Representing integers], [the limits that appear when numbers are held in fixed bits]),
)

#deepqa[
  Chapter 7 got as far as signed integers in bits — including three competing
  ways of holding negatives, settled on two's complement. But they are all
  still integers. A number like 1.5 — how is that held in bits?
][
  The bits themselves have no decimal point. So the decimal point too is made by
  *agreement* — the same principle by which negative numbers were made by an
  agreement (two's complement). The way of deciding "where shall we pretend the
  point is when we read this?" splits into two: pinning the point down (fixed)
  and carrying the point along in the data (floating). This chapter is the story
  of those two branches.
]

#organizer[
  The next rung on the ladder of representation. You will be able to tell apart
#idx("floating point")  the two ways of putting a number with a decimal point
  into the lockers — fixed point and floating point — and you will meet
#idx("IEEE 754")  IEEE 754, the contract that unified a chaos of floating-point
  formats. One of the most important facts in this book — that a computer's
  numbers are finite and approximate — takes its place here.
]

#chapter-questions()

== Fixed point — pinning the decimal point down by agreement

The first method is surprisingly simple. *Just use an integer, and change the
unit.*

Money is a good example. To store 1,500.25 won, agree that "our ledger is
written entirely in units of 0.01 won" and store the integer 150025. The
decimal point is stored nowhere — everyone reading and writing merely shares
the agreement "pretend there is a point two digits from the end." This is
*fixed point*: the position of the point is fixed by agreement.

The virtue of fixed point is that it *is* an integer. Addition and subtraction
are as fast and exact as integer operations, with no approximation. So it is
still in active service for money calculations and on small embedded chips with
no decimal hardware. Its weakness is stiffness — one agreement cannot cover both
very large and very small numbers. You cannot write the mass of a galaxy and the
mass of an atom in the same ledger of hundredths.

== Floating point — carrying the point in the data

The second method transfers *scientific notation*, learned in science class, to
the machine. That is, writing a number as $6.02 times 10^23$ — split into "the
meaningful digits" and "how many times ten was multiplied in." Store both the
kernel (the significand) and the multiplication count (the exponent) as data,
and the decimal point *floats* along with the exponent — hence *floating point*.
The machine uses 2 instead of 10: it holds numbers in the form
$"significand" times 2^"exponent"$.

The power of this method is that it covers *an enormous range with the same
number of bits*. Galaxy and atom fit in one format. The price is precision —
the number of digits in the kernel is finite, so anything beyond them is
*discarded by rounding*.

#misconception[
  "Computers are good at maths, so of course decimal arithmetic is exact"
][
  Plausible — you have never seen a calculator get it wrong. But the numbers
  floating point can hold are only *finitely many*, and a number it cannot hold
  is rounded to its nearest neighbour. Astonishingly, that includes 0.1 — 0.1 is
  an infinite fraction in binary (see the box below), so the "0.1" inside the
  machine is really a different number very close to 0.1. Small discrepancies
  can accumulate through a calculation. This is not a defect of the computer but
  a necessity of finite representation, and there are proper ways to handle it
  (chapter 46). One thing to remember for now: *floating point is not an exact
  number but a faithful approximation.*
]

#mathbox[
  Why 0.1 does not terminate in binary
][
  A fraction terminates only when its denominator is made solely of factors of
  the base. In decimal (base $10 = 2 times 5$), $1/10$ ends after one digit. But
  binary's base is only 2, so $1/10$, with a 5 in the denominator, never
  terminates:

  $ 0.1_10 = 0.0001100110011..._2 $

  It is the same as $1/3$ being the endless $0.333...$ in decimal. A finite
  significand has to cut this infinity off somewhere, and that cut is the source
  of the approximation.
]

== How the bits are really divided

Let us take "the significand and the exponent are stored together" all the way
down to the bit layout. IEEE 754 divides a value into three pieces — the *sign*
(1 bit), the *exponent*, and the *fraction*.

#dtable(
  columns: 5,
  [*format*], [*total*], [*sign*], [*exponent*], [*fraction*],
  [`float` (single)], [32 bits], [1], [8], [23],
  [`double` (double)], [64 bits], [1], [11], [52],
)

#figure-svg("ieee754", caption: [The bit layout of `float` and `double`. The width of each field is the ratio of its bit count.])

The rules for reading the three pieces are these.

- *Sign*: 0 for positive, 1 for negative. It is a separate bit, unrelated to the
  magnitude, which is why *`-0.0` exists* — a zero whose sign bit alone is 1.
- *Exponent*: negative exponents must fit too, so a *bias* is added before
  storing. `float` adds 127, `double` adds 1023. A stored 1023 means an actual
  exponent of 0; a stored 1019 means $-4$.
- *Fraction*: in a normalised number the leading digit is always 1, so *that 1 is
  not stored* (the hidden bit). Hence `double` stores 52 bits and yet carries 53
  bits of precision.

So the value of a normal number is recovered as

$ (-1)^"sign" times 1."fraction" times 2^("stored exponent" - "bias") $

The two ends of the exponent range are reserved for special meanings.

#dtable(
  columns: 3,
  [*exponent bits*], [*fraction*], [*meaning*],
  [all zero], [0], [zero (`+0.0` or `-0.0` by the sign bit)],
  [all zero], [non-zero], [*subnormal* — the hidden bit is taken as 0, filling in densely near zero],
  [all one], [0], [infinity ($plus.minus infinity$ by the sign)],
  [all one], [non-zero], [NaN — "not a number"],
  [anything else], [anything], [a normal number — the formula above],
)

This table is the origin of the properties met in chapter 46. That NaN is not
equal to itself, that infinity comes out of overflow, that `+0.0 == -0.0` while
their bits differ — all of it comes from this layout. Chapter 46 confirms each of
them by *printing the actual bits*.

#qa[
  Why bias the exponent — why not store it in two's complement?
][
  It could be stored that way. The bias has a practical advantage, though — *read
  the bits (apart from the sign) as an integer and the ordering of the reals comes
  out right.* The exponent sits in the high bits, and thanks to the bias a smaller
  exponent gives a smaller bit pattern, so two positive numbers of the same sign
  compare correctly even when their patterns are read as unsigned integers. The
  hardware's comparison circuits get simpler and work like sorting gets faster. A
  two's complement exponent would break that property.
]

== Three incidents caused by approximation

Let us look ahead, numerically, at the faces "faithful approximation" wears in
practice. (Shown here by calculation on the page; running it in C is
chapter 46.)

*Incident 1 — 0.1 + 0.2 ≠ 0.3.* The most famous non-equation in the programming
world. As we just saw, 0.1, 0.2 and 0.3 are all infinite in binary, so the
machine holds each one's "nearest neighbour" instead. And the sum of 0.1's
neighbour and 0.2's neighbour happens to round the *other way* from 0.3's
neighbour. Written out in the 64-bit format —

- $0.1 + 0.2$ in the machine = `0.30000000000000004440...`
- $0.3$ #h(2.4em) in the machine = `0.29999999999999998889...`

Both are faithfully close to 0.3, but *they are not each other*. So when you
ask "are they equal?" (`==`), the machine answers honestly: no.

Look at the internal representation and the whole incident is visible at a
glance. The 64-bit format splits into [1 sign bit | 11 exponent bits |
52 significand bits], and writing these four numbers' 64 bits in hexadecimal —

#dtable(
  columns: 2,
  [*number*], [*64-bit internal representation (hex)*],
  [0.1], [`3FB9 9999 9999 999A`],
  [0.2], [`3FC9 9999 9999 999A`],
  [0.3], [`3FD3 3333 3333 3333`],
  [0.1 + 0.2], [`3FD3 3333 3333 333`#text(fill: rgb("#b0483c"), weight: "bold", raw("4"))],
)

There are three layers to read here. First, the `9999...` in the significands
of 0.1 and 0.2 is the repeating binary `0011` seen in hexadecimal, and the fact
that *the last digit is `A` and not 9* is the trace of rounding up while cutting
the infinity off at 52 bits — the "cut" of the maths box is stamped right into
the bits. Second, 0.3's significand `3333...` is the same repetition at a
different phase, and this one rounded down at the end. Third — the crux — 0.3
and 0.1+0.2 differ by *exactly one final bit*. They are the immediate
neighbours, one tick apart (1 ulp, in the jargon). `==` answers "different" even
to that one-bit difference, so floating-point "equality" breaks under a
discrepancy of a single tick.

*Incident 2 — so how do you compare?* In the floating-point world the question
of equality itself has to change — not "are they exactly equal?" but *"are they
close enough?"* You settle on a tolerance (traditionally called *epsilon*,
$epsilon$) and count the two numbers as equal if their difference falls inside
it:

$ |a - b| < epsilon quad "(e.g. " epsilon = 10^(-9) ")" $

That is the basic form; in practice there is one more layer — as numbers grow,
so does the gap between neighbours (incident 3 below), so instead of a fixed
$epsilon$ it is safer to use a tolerance *proportional to the size of the
numbers* (relative error). The concrete use of both, and their traps, is
covered in C code in chapter 46. What to remember now is one sentence: *code
that compares floating-point numbers with `==` is almost always suspect.*

*Incident 3 — beside a large number, a small one disappears.* That the
significant digits are finite also means that when two numbers of very different
sizes meet in one container, the smaller one *vanishes entirely*. The place
where this shows most cleanly in the 64-bit `double` (about 15–16 significant
decimal digits) is around $10^16$.

- $10^16 + 1$ is *$10^16$ again.* The 1 that was added simply disappears. And
  yet $10^16 + 2$ becomes $10000000000000002$ and grows properly. There is one
  reason for this strange contrast — at that magnitude the gap between
  representable neighbours is exactly *2*. With ticks 2 apart, 1 is half a tick
  and is swallowed by rounding, while 2 is exactly one tick and survives. This
  phenomenon of a small value being eaten by a large one is called
  *absorption*.
- So the limit up to which a `double` can hold *every* integer without omission
  is $2^53 = 9007199254740992$. Above that the tick spacing exceeds 1 and even
  integers begin to be skipped — try to hold $2^53 + 1$ and you get $2^53$
  again. The practical rule "do not put money or identifiers in floating point"
  comes from here (JavaScript, which handles integers as `double`, hit this same
  wall and brought in `BigInt` for the same reason).
- There is an accident in the opposite direction too. *Subtracting two nearly
  equal numbers* erases all the leading digits and leaves only the approximation
  in the last ones — a calculation that started with sixteen significant digits
  produces an answer with two or three. This is called *cancellation*, and it is
  the chief culprit in eroding precision in numerical work.

The three incidents have a single root — the representable numbers are a
*discrete set of ticks*, and the spacing between ticks widens as numbers grow.
Near 0 the ticks are dense enough to distinguish something like $10^(-300)$, but
as we just saw, around $10^16$ the spacing exceeds 1 and *even integers get
skipped*. It is no
exaggeration to say that a feel for "how many digits can I trust?" (significant
digits) is the whole of using floating point.

== The chaos, and the contract called IEEE 754

The idea of floating point was old, but *how* to hold it — how many bits each
for significand and exponent, which way to round — differed by company for a
long time. IBM mainframes, DEC's VAX and Cray supercomputers each used a
different format. The same program moved to another machine gave *different
answers*, and numerical programmers had to learn each machine's arithmetic
habits afresh.

In 1985 a standard called IEEE 754 unified the chaos. It is a precise contract
that pins down the format (1 sign bit + exponent + significand), the rounding
rules, and the treatment of special values such as infinity and "not a number"
(NaN). Today essentially every CPU and GPU follows this contract — the 32-bit
format (C's `float`) and the 64-bit format (C's `double`) being the
representatives.

The dates are worth noticing. The contract for floating point (IEEE 754, 1985)
and the contract for the C language (ANSI C, 1989) were born within a few years
of each other. It was an era in which the industry, unable to bear
vendor-by-vendor arbitrariness any longer, turned to "let us write contracts" —
and we meet this "age of contracts" again in chapter 12.

#qa[
  How do you choose between the 32-bit `float` and the 64-bit `double` in
  practice?
][
  The default is `double`. The 64-bit format handles about 15–16 significant
  decimal digits, which leaves room in most calculations. `float` (about 7
  digits) is chosen for its size advantage where memory and bandwidth matter —
  large numbers of coordinates, graphics, machine learning. "double for
  precision, float for volume" is roughly right. Actual use in C is covered in
  chapter 46.
]

#qa[
  Integer overflow in chapter 7 and rounding in this chapter are both problems
  of "a finite container" — are they the same kind of problem?
][
  The root is the same, the symptoms differ. The integer container is finite in
  *range*, so it overflows at the end (and is perfectly exact until it does);
  the floating-point container is finite in *precision*, so it approximates a
  little everywhere (but its range is enormous). An exact but narrow container
  and a wide but approximating one — a feel for which container to use is a
  fundamental of handling numbers, and choosing types in C (chapters 26 and 46)
  is exactly that choosing.
]

To summarise this rung of the ladder: the decimal point is not in the bits but
in an agreement. Pin the agreement down and you have fixed point; carry it in
the data and you have floating point, whose agreement is unified by the
contract IEEE 754. And numbers under that contract are not exact numbers but
faithful approximations.

The next rung is characters. What has to be agreed in order to put "hello" in
the lockers — the world of characters and text, and the story of the scars left
where those agreements failed to match.
