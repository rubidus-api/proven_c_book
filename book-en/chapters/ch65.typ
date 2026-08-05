#import "../../book/lib.typ": *

= How to ask about overflow — `<stdckdint.h>`

#organizer[
#idx("checked arithmetic")  We learn the checked arithmetic C23 brought in. Why
  hand-written code that "checks after calculating whether it overflowed" is
  dangerous, exactly what `ckd_add`, `ckd_sub` and `ckd_mul` promise, and how to
  move the size calculations of existing code onto this tool.
]

#deepqa[
  Chapter 7 said unsigned overflow is defined wrap-round while signed overflow is
  outside the contract, and chapter 58 said `calloc` checks the product of the
  element count and the size. Then how do we write the check for whether a
  multiplication overflows ourselves?
][
  That very "checking ourselves" is the code that has been written wrongly for half
  a century. With signed values, *checking after it has overflowed is itself
  already outside the contract*, so the compiler may erase the checking code
  (chapter 13's editor), and with unsigned values the idiom that inserts a division
  is hard to read and easy to get wrong. C23 tidied this place up at the level of
  the language.
]

== The trap of checking afterwards

We start from the most common hand-written check.

#antipattern[
  Adding and then seeing whether it overflowed
][
  ```c
  int sum = a + b;
  if (sum < a) { /* it overflowed */ }        /* ← this check can vanish */
  ```
  If `a` and `b` are signed integers, the moment `a + b` overflows it is already
  undefined behaviour. The check after it has meaning only on the premise "if it
  did not overflow", so the compiler judges that `sum < a` can never be true and
  *erases the conditional entirely.* This pattern really did make checks vanish
  quietly in several projects, and among them were security checks.

  For unsigned values, wrap-round being defined, the check above *does work*. But
  going to multiplication makes even that awkward.
  ```c
  if (n != 0 && bytes / n != sz) { /* it overflowed */ }   /* correct but hard to read */
  ```
]

So compilers each put out extensions — GCC's and Clang's `__builtin_add_overflow`
family, MSVC's `SafeInt`, the home-made macros of many projects. They worked well
but *were not portable*, and to secure portability every project had to write the
same shell again. It is the same pattern as chapter 57's `strlcpy` story — reality
finds the answer first and the standard ratifies it belatedly.

== C23's answer — `ckd_add`, `ckd_sub`, `ckd_mul`

#demo("examples/ch65/ckdint.c")

The way to read it is this. All three macros have the same shape.

```c
bool overflowed = ckd_add(&result, left, right);
```

There are four promises.

+ *It calculates in infinite precision and then puts it in the vessel.* The
  judgement is "does the mathematical result fit in the result type", and there is
  no overflow in the intermediate calculation. So it judges exactly even when the
  arguments' types differ from each other, and even when the result type is
  narrower than the arguments — the example's `signed char <- 300` confirms it.
+ *The result type is settled by the first argument (the pointer).* It means the
  arguments' promotion rules (chapter 27) do not sway the result, so there is no
  need to fret over "in which type is it calculated".
+ *Even on overflow the result is stored.* That value is the value wrapped round
  into the result type. The example's `INT_MAX + 1` remaining as `-2147483648` is
  that — and it matters that even for a signed value *it is defined behaviour in
  this place*.
+ *Unsigned wrap-round is reported as "overflowed" too.* The example's `3u - 5u`
  confirms it. The value itself is a defined result (`4294967294`), but checked
  arithmetic reports that *it differs from the mathematical value*. In size
  calculations this is the property needed.

#qa[
  Are the `ckd_*` functions or macros? Do they not evaluate arguments several
  times?
][
  What the standard settles are macros, but they are pinned down not to evaluate
  their arguments several times (implementations mostly expand them into compiler
  builtins). So code such as `ckd_add(&r, i++, j)` is safe too.

  But there is *another* trap. The judgement and the result must not be mixed
  inside one expression.
  ```c
  printf("%s %d", ckd_add(&r, a, b) ? "overflow" : "fine", r);   /* dangerous */
  ```
  There is no settled order between the moment `r` is read and the moment `ckd_add`
  writes to `r`, so the old value may be printed (chapter 20's story of ordering).
  This mistake really did print a wrong value when this chapter's example was first
  written. *Take the judgement into a variable, and use the result on the next
  line* — that one line of discipline is the whole of it.
]

== Where it is used — size calculation comes first

The `alloc_array` in the latter part of the example is the type. Calculating an
allocation size is the place where checked arithmetic is most sorely needed. If
`n * sz` overflows you end up *obtaining a small vessel and using it believing it
big*, which leads straight to a buffer overflow accident. Several famous
vulnerabilities took exactly this route.

#dtable(
  columns: 3,
  [*place*], [*the old idiom*], [*now*],
  [array allocation], [the check `n && SIZE_MAX/n < sz`], [`ckd_mul(&bytes, n, sz)`],
  [growing a buffer], [just calculating `cap * 2`], [`ckd_mul(&cap2, cap, 2)`],
  [joining lengths], [`len1 + len2 + 1`], [`ckd_add` twice],
  [index calculation], [`base + off` just so], [`ckd_add` (compulsory for signed values)],
  [numbers from input], [using it straight after `atoi`], [`strtol` (chapter 58) + a range check],
)

The last line matters. Checked arithmetic only catches the overflow of a
*calculation*; it does not filter out a value that was too large to begin with. The
check at the input-parsing stage (chapter 58) and the check at the calculation
stage do not stand in for each other.

#misconception[
  "Use `ckd_*` and worry about integer overflow ends"
][
  Three things remain. First, *division* is not in this header — `INT_MIN / -1` is
  still an outside-the-contract case you must block yourself. Second, *conversions*
  are not checked. Assigning from a wide type to a narrow one is not arithmetic but
  conversion (chapter 7's truncation), so a value being wrecked there is not
  `ckd_*`'s business. Third, *floating point* is not its subject (chapter 60).

  In summary, checked arithmetic is a tool answering the narrow and clear question
  "did an addition, subtraction or multiplication overflow its vessel". It is a good
  tool precisely because it is narrow.
]

== Where this tool is absent

In an environment that cannot yet use the C23 header, prepare in two steps.

```c
#if defined(__has_include)
#  if __has_include(<stdckdint.h>)
#    include <stdckdint.h>
#    define HAVE_CKDINT 1
#  endif
#endif

#ifndef HAVE_CKDINT                      /* fill in with the GCC/Clang extensions */
#  define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#  define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#  define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif
```

Only beware that the argument order differs — the standard puts the result pointer
first, the compiler builtins put it last. Gathering such shells in one place is the
real shape of the portability layer spoken of in chapter 49, and Part XII's library
does the same work.

#platform[
  The road of leaving the checking to tools
][
  There is also a way of catching overflow without mending the code. GCC's and
  Clang's `-fsanitize=signed-integer-overflow` (UBSan) catches overflow during
  execution and reports it, and `-ftrapv` stops the program on overflow. Both are
  *for testing* — they show themselves only in a run in which an overflow actually
  happened, so they are different in character from checked arithmetic, which
  "blocks in code the places where it could happen". It is the same conclusion as
  chapter 17's story of debuggers: tools help observation but do not stand in for
  the contract.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [checking afterwards], [with signed values the check itself is outside the contract — it can be erased],
    [the shape], [`bool overflowed = ckd_add(&result, a, b)`],
    [the criterion], [does the mathematical result fit in *the result type*],
    [the result type], [settled by the first argument. not swayed by promotion rules],
    [even on overflow], [the wrapped value is stored (defined behaviour)],
    [unsigned values], [wrap-round too is reported as "overflow"],
    [one expression], [do not mix the judgement and the result],
    [first place to apply it], [allocation size calculation],
    [where it is absent], [make a shell with `__builtin_*_overflow`],
  )
]

We have learned how to ask about overflow according to the contract. The next
chapter is this part's last and the most conspicuous change C23 made to the
language — the story of things that were macros becoming keywords.
