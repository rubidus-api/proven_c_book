#import "../../book/lib.typ": *

= Expressions and operators

#prereq(
  ([Chapter 20, Expressions], [what becomes a value, and precedence]),
  ([Chapter 33, Assignment and side effects], [evaluation order and sequence points]),
  ([Chapter 37, Arrays], [subscripting and pointer arithmetic]),
  ([Chapter 43, Structures], [member access]),
)

#deepqa[
  Chapter 20 said "do not memorise the table, use parentheses", and since then
  operators have appeared piecemeal wherever they were needed — shifts in
  chapter 27, comparisons in chapter 29, assignment in chapter 33, pointer
  arithmetic in chapter 37. So what is left to learn here?
][
  *The same material through a different lens.* Until now the question was
  "how much of this operator do I need right here?" From here on we read each
  operator as a *contract*: what it accepts (constraints on the operands),
  what it hands back (result type and value category), and where the contract
  ends (the grey zones).

  That lens earns its keep in practice. Reading someone else's code and
  getting stuck; needing to know why the compiler optimised something the way
  it did; chasing a bug that only appears in one build — all of them come down
  to the contract of an operator.
]

#organizer[
#idx("expression")  Operators learned piecemeal, gathered in one place. We start with what an
  expression carries (value, type, value category, side effects), then the
  full precedence and associativity table, then operator-by-operator
  contracts, evaluation order and sequence points, and finally the grey zones
  gathered. After this chapter, appendix A is a lookup sheet and nothing more.
]

#chapter-questions()

== The four things an expression carries

Every expression in C carries four things at once. Keeping them apart is
most of what this chapter teaches.

#dtable(
  columns: 2,
  [*What it carries*], [*What that means*],
  [Value], [The result of the computation. `2 + 3` has the value 5],
  [Type], [Which container the value sits in — fixed at compile time (chapter 23)],
  [Value category], [Whether it is an *lvalue* (it designates a place). `x` is; `x + 1` is not],
  [Side effects], [Whether it changes an object or the outside world (chapter 33)],
)

*Value category* sounds like jargon, but you have been using it all along.
What may appear on the left of an assignment is an lvalue (chapter 33), and
what you may apply `&` to is an lvalue (chapter 34). An array name is an
lvalue that nonetheless cannot be assigned to — a special case (chapter 37).

#qa[
  Does "lvalue" simply mean "on the left"?
][
  Historically yes (left value). Today it is more accurate to read it as
  *an expression that designates a place* — appearing on the left of an
  assignment is one consequence of that property. `*p` is an lvalue even on
  the right-hand side, and a `const int c` is an lvalue that is not a
  *modifiable* lvalue, so it cannot be assigned to.

  That is why the standard says "modifiable lvalue" when it means the
  stricter thing. You will see that phrase in the operand column for
  assignment and for increment below.
]

The notations for writing a *constant* --- bases, prefixes, suffixes, escapes,
string literals --- are gathered in chapter 20, "Every way of writing a constant",
and the rule that settles an integer constant's type is in chapter 26. This chapter
deals with the operators that *join* those leaves.

== Precedence and associativity

The higher up, the more strongly it binds. *Associativity* decides which side
groups first when operators of the same strength stand side by side — `a - b - c`
is `(a - b) - c` because it is left-associative, and `a = b = 0` is
`a = (b = 0)` because assignment is right-associative.

#dtable(
  columns: 4,
  [*group*], [*operators*], [*assoc.*], [*why it associates that way*],
  [postfix], [`() [] . -> ++(post) --(post)`, compound literal], [L→R], [`a.b.c` only makes sense burrowing from the left],
  [unary], [`++ -- + - ! ~ (type) * & sizeof alignof`], [R→L], [the nearest one binds first: `- -x`, `*&x`],
  [multiplicative], [`* / %`], [L→R], [the convention of arithmetic],
  [additive], [`+ -`], [L→R], [subtraction only makes sense left-associative],
  [shift], [`<< >>`], [L→R], [`a << 1 << 2` pushes in turn],
  [relational], [`< <= > >=`], [L→R], [which is why `x < y < z` differs from mathematics],
  [equality], [`== !=`], [L→R], [],
  [bitwise AND], [`&`], [L→R], [],
  [bitwise XOR], [`^`], [L→R], [],
  [bitwise OR], [`|`], [L→R], [],
  [logical AND], [`&&`], [L→R], [short-circuiting only works from the left],
  [logical OR], [`||`], [L→R], [the same reason],
  [conditional], [`?:`], [R→L], [so `a ? b : c ? d : e` reads as a ladder],
  [assignment], [`= += -= *= /= %= &= ^= |= <<= >>=`], [R→L], [so that `a = b = 0` makes both zero],
  [comma], [`,`], [L→R], [the left is done first and discarded],
)

== Places where people slip

#dtable(
  columns: 3,
  [*what was written*], [*how it really groups*], [*if that was the intent*],
  [`a & b == c`], [`a & (b == c)`], [`(a & b) == c`],
  [`a << 1 + 2`], [`a << (1 + 2)`], [`(a << 1) + 2`],
  [`*p++`], [`*(p++)`], [`(*p)++`],
  [`*p.x`], [`*(p.x)`], [`(*p).x` or `p->x`],
  [`(int)x + y`], [`((int)x) + y`], [`(int)(x + y)`],
  [`a = b = 0`], [`a = (b = 0)`], [(as it is — right-associative)],
  [`x < y < z`], [`(x < y) < z`], [`x < y && y < z`],
  [`!x & y`], [`(!x) & y`], [`!(x & y)`],
  [`sizeof a + 1`], [`(sizeof a) + 1`], [`sizeof(a + 1)`],
  [`a ? b : c = d`], [`(a ? b : c) = d` (usually an error)], [`a ? b : (c = d)`],
)

The first two lines are counted among C's famous design scars — that the bitwise
operators bind *more weakly* than the comparisons is a trace of early C, before it
had `&&` and `||`. So parentheses are effectively mandatory around a bit test.
The standard itself notes in a footnote that `a<b<c` does not read as it does in
mathematics.

== Prefix and postfix — the same job, a different value

`++` and `--` can go before or after. Knowing exactly how the two differ is a good
part of the power to read C expressions, so they are gathered here.

=== The contract the standard sets

#dtable(
  columns: 4,
  [], [*prefix `++x` (§6.5.4.1)*], [*postfix `x++` (§6.5.3.5)*], [*the same?*],
  [Operand], [a modifiable lvalue of real or pointer type], [the same], [*the same*],
  [What it does to the object], [adds 1], [adds 1], [*the same*],
  [The value of the expression], [the value *after* the change], [the value *before* the change], [different],
  [Is the result an lvalue], [no], [no], [the same (C++ differs — below)],
  [How it is defined], [`++E` is equivalent to `(E += 1)`], [defined separately], [different],
)

*The only difference is the value the expression yields.* What happens to the object
is identical. So in a place where the value is not used --- the third slot of a
`for`, a statement that is just `i++;` --- the two mean *exactly* the same thing.

#misconception[
  "It says real type, so it cannot be used on integers"
][
  What the standard calls a *real type* is not "floating point". By the
  classification in §6.2.5 it is *integer types and real floating types together*,
  and the only thing left out is *complex*. So it applies to `int`, `char`, `bool`,
  `double` and pointers, and by the standard not to `double _Complex`.

  Measured, GCC lets `z++` (complex) through and only says
  "ISO C does not support `++` and `--` on complex types" when `-Wpedantic` is on ---
  a place it accepts as an extension (chapter 12's grey area).
]

=== When the value is settled, and when memory changes

This is the heart of the section. In `x++` the *event of settling the value* and the
*event of changing memory* are **two different events**, and the standard fixes only
their order.

#dtable(
  columns: 2,
  [*What*], [*The standard's sentence*],
  [Postfix (§6.5.3.5p2)], [the *value computation* of the result is *sequenced before* the side effect of updating the stored value of the operand],
  [Prefix (§6.5.4.1p2 → §6.5.17.1p3)], [`++E` is `(E += 1)`, and in an assignment *the side effect of updating the left operand is sequenced after the value computations of both operands*],
  [Every expression (§6.5.1p1)], [the value computations of the operands are sequenced before the value computation of the result],
)

How to read that matters. The standard nails down the *relative order*, not *the
moment*.

#misconception[
  "A postfix increment happens at the end of the statement (at the semicolon)"
][
  A very widespread belief. Nothing in the standard says it. What is settled is only
  the *order* --- "the result's value first, the store after" --- and when the store
  actually happens is *any time before the next sequence point*: possibly before the
  first instruction of the next statement, possibly in the middle of the same
  expression.

  The belief is dangerous because it invites the next thought: "so if I use it twice
  in one expression, the order must be settled". It is not.

  ```c
  i = i++ + 1;                 /* outside the contract — undefined behaviour */
  a[i] = i++;                  /* outside the contract */
  printf("%d %d\n", i++, i++); /* outside the contract */
  ```

  Section 6.5.1p2 nails it: if a side effect on a scalar object is *unsequenced*
  relative to another side effect on it or to a value computation using it, the
  behaviour is undefined. Prefix and postfix are caught alike. GCC reports
  "operation on 'i' may be undefined" through `-Wsequence-point` (included in
  `-Wall`) --- *though there are many shapes it cannot catch, so do not lean on the
  warning alone.*
]

#qa[
  Then when does the `i++` in `for (i = 0; i < n; i++)` happen?
][
  The third slot is evaluated *after the body of each iteration* --- that is the rule
  of the `for` statement, not of the postfix operator (chapter 31). Since nobody uses
  the result here, switching to prefix does not change one character of the meaning.

  The confusing place is where the *result is used*, as in `while (*d++ = *s++);`.
  There, "write what is pointed at now, and move the pointers on" sits in one
  expression. The two `++` operators touch *different objects* (`d` and `s`), so it is
  inside the contract. Touch the same object twice and it falls outside --- that is
  the boundary line.
]

=== The truth of "prefix is faster"

#realcase[
  `++` and `--` were not created for the PDP-11
][
  The explanation that "`++` was made to use the PDP-11's auto-increment addressing
  mode" still circulates. Dennis Ritchie, who made C, denied it himself. In "The
  Development of the C Language" he wrote that people often guess so but it is
  *historically impossible, inasmuch as there was no PDP-11 when B was developed.*
  The PDP-7 did have a few "auto-increment" memory cells, and that probably suggested
  the operators to Thompson --- yet *those cells were not used directly in
  implementing them*, and a stronger motivation was probably his observation that
  *the translation of `++x` was smaller than that of `x=x+1`*. Generalising them to
  both prefix and postfix was Thompson's own doing.

  So the "smaller translation" motive was *real* --- but it was a comparison of `++x`
  with `x=x+1`, not of `++x` with `x++`. Today's received wisdom is that fact bent
  once in the retelling.
]

What about today's compilers? Measuring settles it.

#dtable(
  columns: 3,
  [*Place*], [*Unoptimised (`-O0`)*], [*Ordinary build (`-O2`)*],
  [`for (…; i++)` vs `++i` --- value unused], [the generated assembly does not differ *by one byte*], [the same],
  [`a = (*b)++` vs `a = ++(*b)` --- value used], [9 instructions vs 11 --- *the postfix one was the shorter*], [3 vs 3, identical],
)

Two things to read out. *First, for C scalars there is no speed difference.* Where
the value is unused the compiler emits the same code. *Second, where the value is
used and the code differs, that is not "postfix is slower" but "the two compute
different things"* --- one needs the old value, the other the new.

=== Two things change in C++

#platform[
  Lvalue-ness, and user-defined types
][
  *1. Is the result an lvalue?* In C *neither* prefix nor postfix is. C++ made the
  prefix one an lvalue. Measured, they part like this.

  #dtable(
    columns: 3,
    [*Code*], [*C (GCC)*], [*C++ (G++)*],
    [`&++x`], [`lvalue required as unary '&' operand` --- error], [accepted],
    [`++x = 5`], [`lvalue required as left operand of assignment` --- error], [accepted],
    [`&x++`], [error], [error --- postfix is a value (prvalue) in C++ too],
  )

  So code like `++x = 5` *compiles in C++ and does not in C*. A place to watch in
  code that crosses between the two languages --- and even in C++ it is convention
  not to write it, being hard to read.

  *2. Postfix on a user-defined type makes a copy.* This is the real reason the
  "prefer prefix" convention took root in the C++ world. A postfix operator has to
  return *the value before the change*, so for a class it makes a copy of the old
  state, keeps it, and returns that.

  Attach a counter to the copy constructor and measure: advancing one iterator 1000
  times cost the prefix form *0 copies* and the postfix form *1000 copies* --- the
  same under `-O2` (a copy with an observable side effect cannot be optimised away).

  *C does not have this problem.* C's `++` attaches only to scalars, and a scalar's
  "copy" is one register, which is why the difference vanishes in the measurements
  above. Carry the advice "use prefix" straight into C and it becomes a *rule without
  a reason.*
]

#qa[
  What, then, should be used in C?
][
  This book's recommendation.

  - *Where the value is unused, make prefix the default.* Not for speed but for the
    *signal it gives the reader* --- "the value of this expression is not used". It is
    also the habit that keeps paying when you move to C++. That said,
    `for (i = 0; i < n; i++)` has been an idiom since K&R and plenty of codebases keep
    it. *Settle it as a team and hold to it.*
  - *Where the value is used, write the one you need.* Old value: postfix. New value:
    prefix. Here the computation chooses, not taste.
  - *And the one real rule --- never touch the same object twice in one expression.*
    Prefix or postfix, keep that and this operator will not hurt you. Chapter 32's
    "split statements when the side effects matter" says the same thing.
]

== Operator by operator

Now each family in turn. Every table has the same columns.

- *Operands* — what the standard requires. Violate it and the compiler must
  diagnose it (a constraint violation).
- *Result* — the type of the value, and whether it is an lvalue.
- *Grey zone* — in the three words of chapter 49. *UB* is undefined
  behaviour, *unspecified* means one of several possibilities with no rule
  saying which, and *implementation-defined* means the implementation chooses
  and documents it.
- *More* — the chapter that tells the story.

#platform("What this chapter rests on")[
  Checked against the expressions clause (§6.5) of ISO/IEC 9899:2024 (C23).
  Where an edition changed a rule, that is said in place. If you need to cite
  a rule, cite the published standard as appendix D explains.
]

== Postfix operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`a[i]` subscript], [one a pointer to a complete object type, the other an integer], [an lvalue of the pointed-at type. `a[i]` is `*(a+i)`], [*UB*: access outside the array (including following the one-past-the-end position)], [chapter 37],
  [`f(...)` call], [a function, or a pointer to one], [the function's return type; not an lvalue], [*unspecified*: the order in which arguments are evaluated. *UB*: arguments that disagree with the prototype], [chapters 21, 24, 55],
  [`s.m` member], [a struct or union value and a member name], [the member's type; an lvalue if the left side is one], [*UB*: reading a union member other than the one last written (the common initial sequence is an exception)], [chapters 43, 45],
  [`p->m` member], [a pointer to a struct or union, and a member name], [the member's type, an lvalue], [*UB*: a null or otherwise invalid pointer], [chapter 43],
  [`x++` post-increment], [a modifiable lvalue of real or pointer type], [*the value before the change*; not an lvalue], [*UB*: two modifications within one sequence point; signed integer overflow], [chapter 31],
  [`x--` post-decrement], [the same], [the value before the change], [the same], [chapter 31],
)

== Unary operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`++x` `--x`], [a modifiable lvalue of real or pointer type], [*the value after the change*], [`++E` is `(E += 1)` — the overflow rules are the same], [chapter 31],
  [`&x` address-of], [a function designator, the result of `[]` or unary `*`, or an lvalue that is *not a bit-field and not declared `register`*], [a pointer to it], [breaking the constraint is a compile error], [chapter 34],
  [`*p` indirection], [a pointer type], [an lvalue of the pointed-at type], [*UB*: null, an object whose lifetime has ended, a misaligned address, or one outside its provenance], [chapters 34, 36, 41],
  [`+x`], [arithmetic type], [the promoted value], [], [chapters 20, 28],
  [`-x`], [arithmetic type], [the promoted value], [*UB*: signed integer overflow (`-INT_MIN`)], [chapter 26],
  [`~x`], [integer type], [the bitwise complement after promotion], [it happens at the promoted width — mind narrow types], [chapter 27],
  [`!x`], [scalar (arithmetic or pointer)], [`0` or `1`, of type `int`], [], [chapter 29],
  [`(type)x` cast], [between scalars], [a value of that type; not an lvalue], [*implementation-defined*: pointer↔integer conversion. *UB*: following a misaligned pointer; converting between function and object pointers], [chapters 28, 36],
  [`sizeof`], [a complete object type or an expression. *Not a function type, an incomplete type, or a bit-field*], [a `size_t` value], [with a variable length array the operand *is* evaluated at run time; otherwise it is not evaluated], [chapters 34, 37],
  [`alignof`], [the *name* of a complete object type (not an expression)], [a `size_t` value], [], [chapter 36],
)

== Arithmetic operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`*` multiply], [arithmetic types], [the common type of the usual arithmetic conversions], [*UB*: signed integer overflow], [chapters 26, 28],
  [`/` divide], [arithmetic types], [the same], [*UB*: a zero divisor, `INT_MIN / -1`], [chapters 27, 47],
  [`%` remainder], [*integer types only*], [the same], [*UB*: a zero divisor, `INT_MIN % -1`], [chapter 27],
  [`+` add], [both arithmetic, or a pointer to a complete object type and an integer], [the common type, or the pointer type], [*UB*: integer overflow; pointer arithmetic beyond the array], [chapters 26, 37],
  [`-` subtract], [both arithmetic, a pointer and an integer, or *two pointers into the same array*], [`ptrdiff_t` for pointer difference], [*UB*: subtracting pointers into different arrays; a difference that does not fit `ptrdiff_t`], [chapters 26, 37],
)

Integer division truncates toward zero (settled since C99). So, as long as the
quotient is representable, `(a/b)*b + a%b == a` holds.

== Shift operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`E1 << E2`], [both of *integer type*], [the type of the promoted *left* operand], [*UB*: `E2` negative or at least the width of the promoted `E1`. *UB*: `E1` signed and negative, or signed and positive with `E1 × 2^E2` not representable in the result type], [chapters 7, 27],
  [`E1 >> E2`], [both of integer type], [the same], [*UB*: `E2` negative or at least the width. *implementation-defined*: the result when `E1` is signed and negative], [chapters 7, 27],
)

#misconception[
  "C23 mandated two's complement, so shifting negatives is defined now"
][
  Two's complement representation was indeed mandated (C23). The shift clause,
  however, is unchanged — *left-shifting a signed negative value is still UB in
  C23*, and *right-shifting a negative value is implementation-defined*. Most
  compilers do an arithmetic shift, but that is a promise of the implementation,
  not of the standard.

  The practical rule is one line: *shift on unsigned types*. If a signed value
  must be shifted, move it to an unsigned type, shift, and move it back. And
  always check that the count is within `0 <= n < width`.
]

== Relational and equality operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`< <= > >=`], [both real types, or *two pointers to compatible object types*], [`0` or `1`, of type `int`], [*UB*: ordering two pointers that do not belong to the same array (or object)], [chapters 29, 36],
  [`== !=`], [both arithmetic, compatible pointers, one a `void*`, one a null pointer constant or `nullptr_t`, and so on], [`0` or `1`, `int`], [*unspecified*: whether a one-past-the-end pointer compares equal to a pointer to the object that follows], [chapters 29, 35],
)

The two families have different contracts. *Equality may be tested between
different objects*, while *ordering only means something within one array.* An
object that is not an array is treated as an array of length one. For reals,
`+0.0` and `-0.0` compare equal (chapter 47).

== Bitwise and logical operators

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`&` `^` `|`], [both of integer type], [the common type of the usual arithmetic conversions], [they reach the sign bit, so use them on unsigned types], [chapter 27],
  [`&&`], [both scalar], [`0` or `1`, `int`], [*guaranteed*: if the left is 0 the right is not evaluated, and there is a sequence point between them], [chapter 29],
  [`||`], [both scalar], [`0` or `1`, `int`], [*guaranteed*: if the left is non-zero the right is not evaluated], [chapter 29],
)

== Conditional, assignment, comma

#dtable(
  columns: 5,
  [*operator*], [*operands*], [*result*], [*grey zone*], [*in detail*],
  [`c ? a : b`], [`c` scalar. `a` and `b` both arithmetic, or compatible structs or unions, or both `void`, or compatible pointers, or one a null pointer constant], [*one common type* for both branches], [*guaranteed*: a sequence point after the condition; the branch not chosen is not evaluated], [chapter 32],
  [`=`], [the left must be a modifiable lvalue], [the value converted to the left's type. *Not an lvalue*], [*UB*: assignment between overlapping objects (exact overlap with compatible types is allowed); two modifications within one sequence point], [chapter 23],
  [compound `op=`], [`E1 op= E2`], [as `E1 = E1 op E2`, except that *`E1` is evaluated once*], [the grey zones of the operation itself (overflow, zero divisor) still apply], [chapter 32],
  [`,` comma], [any two expressions], [the type and value of the right], [*guaranteed*: the left is evaluated and discarded, then a sequence point. The comma in an argument list is *not this operator*], [chapter 32],
)

== Evaluation order and sequence points

Precedence is a rule about *grouping*, not about the order in time (chapters 13
and 32). Order is guaranteed in exactly five places.

- between the left and right of `&&`
- between the left and right of `||`
- between the condition of `?:` and the branch chosen
- between the left and right of the comma *operator*
- between the evaluation of a call's arguments and the execution of the function
  body (though *the order among the arguments is unspecified*)

Nowhere else is any order guaranteed.

#dtable(
  columns: 2,
  [*expression*], [*verdict*],
  [`f() + g()`], [*unspecified* — which is called first is not settled],
  [`h(f(), g())`], [*unspecified* — argument evaluation order],
  [`i = i++`], [*UB* — `i` is modified twice within one sequence point],
  [`a[i] = i++`], [*UB* — the same reason],
  [`i++ + i++`], [*UB*],
  [`f(i++, i++)`], [*UB* — there is no sequence point between arguments],
  [`i++, i++`], [fine — the comma *operator* has a sequence point],
  [`(i++) && (i++)`], [fine — `&&` has a sequence point],
)

Since C11 the standard states these rules with a *sequenced-before* relation
rather than with sequence points, but the practical conclusion is the same —
*do not touch the same object twice within one expression.* GCC's
`-Wsequence-point` catches the common cases, but not all of them.

== The grey zones gathered

Only the operator-related entries, sorted by chapter 49's three words. The full
lists are in annex J of the standard.

=== Undefined behaviour (UB)

#dtable(
  columns: 2,
  [*place*], [*condition*],
  [`/` `%`], [a zero divisor; `INT_MIN / -1`, `INT_MIN % -1`],
  [`+ - *` `++ --`], [signed integer overflow],
  [`<<`], [a count that is negative or at least the width; left-shifting a signed negative; a signed positive whose result does not fit],
  [`>>`], [a count that is negative or at least the width],
  [`*` indirection], [null, an object past its lifetime, a misaligned address, a pointer outside its provenance],
  [`[]`], [access outside the array],
  [`+ -` pointer arithmetic], [a result outside the array (one past the end included)],
  [`-` between pointers], [pointers into different arrays],
  [`< <= > >=`], [ordering pointers that do not belong to the same array or object],
  [`.` `->` on unions], [reading a member other than the one last written (the common initial sequence excepted)],
  [expressions in general], [modifying the same object twice within one sequence point, or modifying it and reading it for another purpose],
)

=== Unspecified

#dtable(
  columns: 2,
  [*place*], [*what is not settled*],
  [subexpressions], [the evaluation order of `f() + g()`],
  [function arguments], [the order among arguments],
  [`==` `!=`], [whether a one-past-the-end pointer compares equal to a pointer to the next object],
  [padding bytes], [the values of a struct's padding — the reason not to compare with `memcmp` (chapter 44)],
)

=== Implementation-defined

#dtable(
  columns: 2,
  [*place*], [*what the implementation settles*],
  [`>>`], [the result of shifting a signed negative value (usually an arithmetic shift)],
  [integer conversion], [the result of converting a value that does not fit a signed type (still so in C23)],
  [pointer ↔ integer], [the result of the conversion and whether it round-trips (only the round trip through `uintptr_t`, where it exists, is guaranteed)],
  [`char`], [signed or unsigned — which splits `>>` and comparison (chapter 9)],
  [bit-fields], [the order of allocation and the padding],
)

== Things that are not operators

The same characters appear in the grammar without being operators.

#dtable(
  columns: 3,
  [*shape*], [*what it really is*], [*in detail*],
  [the comma in `f(a, b)`], [a separator of the call syntax — no sequence point], [chapter 32],
  [the comma in `int a, b;`], [a separator of declaration syntax], [chapter 23],
  [the comma in `{1, 2}`], [a separator in an initialiser list], [chapters 37, 43],
  [`(type){...}`], [a compound literal — not a cast but *syntax that makes an object*], [chapter 44],
  [the parentheses of `sizeof(int)`], [syntax wrapping a type name — not a call], [chapter 34],
  [`#` `##`], [preprocessor operators — they act in a different phase of translation], [chapter 54],
  [the dot in `{.x = 1}`], [designated-initialiser syntax, not member access], [chapter 43],
  [the star in `int *p;`], [declarator syntax, not indirection], [chapter 57],
)

#recap[
  #dtable(
    columns: 2,
    [*What to keep*], [*The point*],
    [What an expression carries], [Value, type, value category, side effects],
    [Precedence], [A grouping rule, not an order of computation],
    [Associativity], [Which side groups first among equals],
    [Where order is guaranteed], [Only `&&`, `||`, `?:`, the comma operator, and function calls],
    [Grey zones], [Read UB, unspecified and implementation-defined as distinct words],
    [Working rule], [Parenthesise, and never touch the same object twice in one expression],
  )
]

You can now read an operator as a contract. The chapters that follow go into
the places where those contracts get subtlest — the mathematics of
approximation (chapter 47), handling failure (chapter 48), and what happens
when a contract is broken (chapter 49).
