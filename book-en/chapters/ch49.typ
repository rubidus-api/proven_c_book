#import "../../book/lib.typ": *

= Real numbers — the mathematics of approximation

#prereq(
  ([chapter 8, Representing numbers], [the contract called IEEE 754]),
)

#deepqa[
  Chapter 8 said `0.1 + 0.2` and `0.3` are neighbours one final bit (1 ulp) apart,
  and that comparison must change into "are they close enough?". Then how is the
  criterion of "enough" — the epsilon — decided?
][
  That it must vary with the size of the values is the lesson of chapter 8's third
  incident. Near 0 the ticks are dense so a small fixed value will do, but around
  $10^16$ the tick spacing itself exceeds 1 and the same criterion becomes
  meaningless. So practice keeps two — *absolute error* (for near 0) and
  *relative error* (proportional to size). This chapter's demonstration shows both
  side by side.
]

#organizer[
  The world of approximation learned on the page in chapter 8 finally comes down
#idx("comparing reals")  into C code. Choosing between `float` and `double`, the
  correct way to compare (epsilon — absolute and relative), and the special values
  (infinity, NaN). Chapter 8's three incidents are confirmed by execution results.
]

#chapter-questions()

== Choosing a type, and comparing

#demo("examples-en/ch49/eps.c")

The first three lines are the execution check of chapter 8's first incident —
`==` is false, and printed to twenty digits the two numbers diverge at the end.
Use `near_abs` (absolute error) and it becomes true.

The latter part checks the third incident (absorption) — add 1 to $10^16$ and the
value is unchanged, because at that size the gap between representable neighbours
is already wider than 1 (chapter 8's tick calculation). Here the right tool is not
absolute error but `near_rel` (relative error).

Reduced to practical rules there are three. *The default is `double`* — as
chapter 8 showed, the room in precision is of a different order, and `float` is
chosen only where memory and bandwidth are tight. *Do not use `==`* — code asking
whether two reals are equal is almost always suspect (there are exceptions, such
as comparing against an integer or against 0, but they must be judged
consciously). *The tolerance comes from the problem* — it is decided by looking at
the nature of the calculation and the size of the values; there is no magic
constant.

== Special values — infinity and NaN

IEEE 754 (chapter 8) defines special values besides ordinary numbers. *Infinity*
(positive and negative) comes out of overflow or division by zero, and *NaN* (Not
a Number) means "not a number" — the result of an undefinable operation such as
$0/0$ or the square root of a negative.

One thing has to be stated exactly here. Integer division by zero is outside the
contract (chapter 28), and floating-point division by zero is commonly said to
"be defined and give infinity". *That, however, is not a promise of the C
language itself.* The standard's rule for division leaves the behaviour
undefined when the second operand is zero — for reals as much as for integers.
Infinity appears in implementations that support IEC 60559 (that is, IEEE 754)
semantics. In such an implementation, dividing a finite non-zero value by zero
gives a signed infinity and raises the divide-by-zero exception, while $0/0$
falls on the NaN side. Today's mainstream compilers on x86-64 and AArch64 behave
that way, but it is not something the standard forces on every implementation.

#platform("Where this distinction actually bites")[
  The mark by which an implementation declares that it follows IEC 60559 for
  binary floating point is `__STDC_IEC_60559_BFP__` (BFP = binary floating
  point). Where that macro is defined, annex F semantics are a contract and the
  behaviour above may be expected. Decimal floating point is marked separately,
  by `__STDC_IEC_60559_DFP__`.

  It must not be confused with the similarly named
  `__STDC_IEC_60559_BF16_TYPES__` — that is a *separate* feature mark, about
  whether bfloat16 types are provided, and has nothing to do with the semantics
  of ordinary binary floating-point operations. And note further that *a type
  having the same format as IEC 60559 and the operations following annex F are
  two different questions* — the former is what `__STDC_IEC_60559_TYPES__`
  speaks to; what is needed here is the latter. Where it does not — some embedded
  toolchains, and builds that deliberately switch annex F semantics off with
  something like `-ffast-math` — the guarantee of infinity and NaN goes away.
  *Portable code screens out a zero divisor first.*
]

=== Opening the bits directly

Having seen the layout in chapter 8, we now print the bits of real values and
check them. To move a representation we use `memcpy` rather than a union — by
chapter 47's rule that is the safest passage for "moving a value", and compilers
mostly make the copy disappear.

#demo("examples-en/ch49/bits.c")

Five things from the output are worth pointing at.

*First, `1.0` is remarkably tidy.* The exponent field holds 1023 (the bias
itself, so the actual exponent is 0) and the fraction is all zeros — the hidden
bit alone makes $1.0 times 2^0$. `2.0` raises the exponent by one, `0.5` lowers
it by one, and flipping the sign bit gives `-1.0`.

*Second, `0.1` shows the cut mark of an unending fraction.* Its fraction ends in
`999999999999a`, and that final `a` is the trace of *rounding*. It is chapter 8's
mathematics box — "it does not come out even in binary" — laid bare in bits.

*Third, `0.1 + 0.2` and `0.3` differ by one last bit.* The two bit patterns end
`...3334` and `...3333`, exactly one apart. That is why the `==` comparison is
false, and why this chapter talks about tolerances.

*Fourth, the identity of one ULP becomes visible.* Adding the integer 1 to the
bits of `1.0` gives the very next real number, and the difference is
`DBL_EPSILON` ($2^{-52}$). "The smallest distinguishable difference near 1.0"
turns out to be a single bit.

*Fifth, the subnormals appear at the floor.* Halve the smallest normal number and
the exponent cannot go lower, so *zeros begin to fill the front of the fraction*
instead — that state, with the exponent field all zeros, is a subnormal. Precision
is given up little by little on the way down to zero, and when the last bit
disappears the value becomes zero. This design, fading out instead of falling
abruptly to zero, is called *gradual underflow*.

#platform("subnormals can be slow")[
  Arithmetic on subnormals is far slower than on normal numbers on some hardware
  (tens of times, on some machines). So signal processing and game engines
  sometimes switch on a mode that flushes subnormals to zero — a trade of a
  little accuracy for the removal of a worst-case stall. Standard C has no
  portable way to switch that mode on (it is a compiler option or a platform API).
]

NaN has one famous property — *it is not even equal to itself.* If `x != x` is
true then x is NaN, and that is the classic idiom for detecting NaN (today one
uses `isnan()`). Being a value that breaks the basic property of the relation
"equality", NaN mixed into sorting or searching algorithms produces strange
results — which is why checking for NaN at the boundary is the practice when
handling real-number data.

#realcase[
  The accumulation of 0.1 seconds — the Patriot missile incident
][
  There is an event in which chapter 8's "small discrepancies accumulate" led
  directly to human lives. In the 1991 Gulf War a Patriot air-defence system
  failed to intercept an incoming missile and 28 people died, and the heart of the
  cause analysis was floating-point error. The system counted time in units of 0.1
  seconds — and as chapter 8 showed, 0.1 is an infinite fraction in binary, so a
  minute error arises each time it is held. Because that system used a 24-bit
  container the error was relatively large, and after 100 hours of continuous
  operation without a reboot the accumulated error reached about 0.34 seconds. In
  those 0.34 seconds the target moved more than 500 metres, and the tracking window
  was looking at the wrong piece of sky. "Approximation is faithful but not
  harmless" — the heaviest confirmation of chapter 8's lesson.
]

#qa[
  Should real numbers then not be used for things like money?
][
  Not using them is the standard — this is exactly the place for the fixed point
  learned in chapter 8. Handle amounts as real numbers in units of won and
  discrepancies at the 0.1-won level accumulate until the ledger does not balance,
  so the practice of financial software is to compute in *integers of the smallest
  unit* and put the decimal point in only when displaying. Reduced to a rule —
  *integers (fixed point) where the exact decimal value matters, floating point
  for physical quantities and scientific computation.* It is fitting the tool to
  the problem, and the grounds for that judgement are the nature of representation
  learned in chapter 8 and here.
]

We can handle the world of approximation in C. The next chapter is this part's
central subject — how a program deals with the fact that a computation can fail:
the story of errors and contracts.
