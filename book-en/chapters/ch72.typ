#import "../../book/lib.typ": *

= Numbers — `<math.h>`, `<fenv.h>`, `<tgmath.h>`

#prereq(
  ([chapter 49, Real numbers], [the mathematics of approximation]),
  ([chapter 8, Representing numbers], [IEEE 754]),
)

#deepqa[
  Chapter 49 said not to compare reals with `==`, and chapter 8 said 0.1 is not
  exactly representable. Then how does a mathematical function tell you when it
  receives "input it cannot calculate"?
][
  There are two paths. It gives NaN or an infinity as the *return value*, and at
  the same time leaves the reason in *`errno`* — `EDOM` for outside the domain,
  `ERANGE` when the result exceeds the representable range. But an implementation
  may choose to report through the floating-point exception flags (`<fenv.h>`)
  instead of `errno`, so to check portably you must be ready to look at both. In
  the field it is usually simpler to check the return value with `isnan` and
  `isinf`.
]

#organizer[
  We look at how real-number calculation reports failure. The mathematics of
  approximation learned in chapters 8 and 49 becomes the contract of functions here
  — calls outside the domain, results beyond the range, the properties of NaN and
  infinity, and the hidden global state called the rounding mode.
]

#chapter-questions()

== The properties of NaN and infinity

#demo("examples-en/ch72/math.c")

Four things to point out in the output.

*① NaN is not equal to itself.* IEEE 754 settled it so. Hence the old idiom that
if `x != x` is true then `x` is NaN, while the standard function is `isnan(x)`.
Because of this property, sorting an array containing NaN with `qsort` breaks the
comparator's total order and the result collapses (chapter 65).

*② Dividing a real by zero is not outside the contract.* Unlike integer division
(chapter 28), in an IEEE 754 environment it yields an infinity or a NaN. But the
same holds that *the very fact of dividing by zero is usually a bug*.

*③ `sqrt(-1)` is `EDOM`, `exp(1000)` is `ERANGE`.* The former is outside the
domain, the latter a case where the result exceeded the representable range. If you
mean to look at `errno`, set it to 0 just before the call (chapter 74).

*④ 0.0 and −0.0 are equal under `==`.* But the sign bit differs, and `1/0.0` and
`1/-0.0` are +∞ and −∞ respectively. If the sign must be distinguished, use
`signbit`.

#misconception[
  "Comparing reals is safe if you use an epsilon"
][
  The epsilon comparison learned in chapter 49 is not omnipotent. Absolute error
  (`fabs(a-b) < eps`) becomes meaningless when the values are large — near 1e9,
  1e-9 is not even representable — and relative error collapses near zero. The
  prescription in the field is *settling a tolerance that fits the situation*, not
  using a universal constant. And it is better to ask first whether it can be
  handled with integers or fixed point so that the comparison is not needed at all
  (chapter 8's story of calculating money).
]

#qa[
  How do the functions of `math.h` report failure — the return value alone cannot say?
][
  In three ways. *Outside the domain* (say `sqrt(-1)`) they return NaN and set
  `errno` to `EDOM`. *Beyond the range* (say `exp(1000)`) they return infinity and
  set `ERANGE`. And the floating-point exception flags of `<fenv.h>` are raised.

  The trouble is that *how far each of the three is honoured varies between
  implementations*. So the practical idiom is to clear `errno = 0` before the call
  and check immediately after (chapter 74). To inspect the value itself use
  `isnan` and `isinf` — they say what they mean, unlike tricks such as `x != x`.
]

== Functions often got wrong

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*trap*],
  [`pow(x, y)`], [raising to a power], [used for an integer power it can be slow and inexact],
  [`round`, `nearbyint`], [rounding], [`round` goes away from zero, `nearbyint` follows the current mode],
  [`floor`, `ceil`, `trunc`], [cutting to an integer], [the direction differs for negatives],
  [`fmod`, `remainder`], [the remainder], [their sign rules differ from each other],
  [`abs`, `fabs`], [absolute value], [★ `abs` is for integers. used on a real it truncates],
  [`atan2(y, x)`], [angle], [the argument order is `y, x`],
  [`isnan`, `isinf`], [classification], [they are macros — they cannot be used as function pointers],
)

`pow(x, 2)` is widely used, but for an integer square `x * x` is faster and exact.
The compiler often optimises it, but not always.

The mistake of using `abs` on a real is especially quiet. `<stdlib.h>`'s `abs`
takes an `int`, so `abs(-1.5)` turns −1.5 into 1. Today's compilers warn, but it is
easy to miss in a file that does not include `<math.h>`.

== Rounding modes and floating-point exceptions — `<fenv.h>`

Floating-point operations have two pieces of *hidden global state*.

*The rounding mode* — the default is "to the nearest value, ties to even". It can
be changed with `fesetround`, and once changed every subsequent real operation is
affected.

*The exception flags* — flags are raised when division by zero, overflow,
inexactness and so on occur. They are read with `fetestexcept` and cleared with
`feclearexcept`. They are finer than `errno`, but to use this facility
`#pragma STDC FENV_ACCESS ON` must be turned on — and then the compiler refrains
from reordering real operations, so optimisation is reduced.

#antipattern[
  Turning on `-ffast-math` and checking for NaN
][
  ```sh
  cc -O2 -ffast-math app.c        # tells the compiler "take it that NaN and infinity do not exist"
  ```
  ```c
  if (isnan(x)) { /* this branch can vanish entirely */ }
  ```
  Options of the `-ffast-math` family tell the compiler it may assume
  associativity and ignore the existence of NaN and −0.0. Speed is gained, but *the
  checking code can vanish under optimisation* — the real-number edition of the
  "bug that appears only in release" seen in chapter 17. In a program where
  numerical accuracy matters, not turning it on is the default.
]

== Type-generic — `<tgmath.h>`

`sqrt` is for `double`, `sqrtf` for `float`, `sqrtl` for `long double`. Include
`<tgmath.h>` and the edition fitting the argument's type is chosen by `sqrt(x)`
alone — the representative case of the `_Generic` seen in chapter 57 being used in
the standard library.

It is convenient but has a price. Being macros, they cannot be passed as function
pointers, and there may be implementations that evaluate the argument twice, so
putting in an expression with side effects is dangerous.

#realcase[
  The same calculation, a different answer — the history of excess precision
][
  x86's old floating-point unit (x87) calculated internally in 80 bits. So it
  happened that the same `double` operation differed depending on whether it was
  still in a register or had been stored to memory — change the optimisation level
  and the result changed minutely, and `x == y` that had been true could become
  false.

  C99 made this circumstance explicit with `FLT_EVAL_METHOD`, and today's 64-bit
  x86 uses SSE so the problem has greatly diminished. But the possibility of "the
  same code, a different answer" still remains in compilation options and the
  target machine — the reason chapter 49 said "real-number calculation needs
  reproducibility looked after separately."
]

#recap[
  Numbers in summary.

  #dtable(
    columns: 3,
    [*situation*], [*what to use*], [*what to beware of*],
    [checking for NaN], [`isnan`], [`x == NaN` is always false],
    [checking for infinity], [`isinf`], [dividing a real by zero is not UB],
    [the kind of a value], [`fpclassify`], [the existence of subnormal numbers],
    [domain and range errors], [the return value + `errno`], [`errno = 0` just before the call],
    [integer squares], [`x * x`], [`pow(x, 2)`],
    [absolute value of a real], [`fabs`], [`abs` (for integers)],
    [per-type functions], [`<tgmath.h>`], [macros — no arguments with side effects],
    [fast-math options], [off by default], [checking code vanishes],
  )
]

We have passed numbers. The next chapter is time — a place with unusually much
that the standard does not settle for you.
