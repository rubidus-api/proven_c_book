#import "../../book/lib.typ": *

= Booleans and comparison

#organizer[
#idx("boolean")  The world of true and false as values — C23's `bool`, the
  comparison operators, and the logical operators with their special property
#idx("condition")  (short-circuit evaluation). The "condition" the next
  chapter's branching will ask about is made here.
]

#deepqa[
  Chapter 23 announced that `=` (putting) and `==` (comparing) use separate
  symbols. Then what is the *result* of a comparison like `3 < 5` — an
  expression becomes a value (chapter 20), so if a comparison is an expression,
  what value does it become?
][
  True or false — it becomes a value holding only those two. In C23 that type is
  named `bool` and its values `true` and `false`. That a comparison is not a
  "question" but *an expression computing a bool value* — that perspective is
  this chapter's centre, and in the next chapter branching consumes that value.
]

== bool — a type with two values

`bool` is a type whose value set is just {`false`, `true`}. It is the smallest
container beside chapter 26's family of integers. One line of history unravels
its odd corners — early C had *no* boolean type, and the practice of counting 0
as false and every nonzero value as true stood in its place. C99 brought one in
through the back door (via the `<stdbool.h>` box), and *only with C23* did
`bool`, `true` and `false` become proper keywords usable without the box. A
formal adoption half a century late — and as a trace of that practice, printing
a `bool` with `%d` gives 1 and 0 (confirmed in the demonstration).

== Comparison — expressions that make bools

There are six comparison operators — `==` (equal), `!=` (not equal), `<`, `<=`,
`>`, `>=`. All take two values as material and put out a bool.

#demo("examples/ch29/cmp.c")

The first line is the check — the value of the expression `10 > 3` (true) was
held in a variable and printed as 1 with `%d`. The instinct that judgement is not
the property of branching alone but *something that can be stored and passed as a
value* — the more complex conditions get, the more this instinct keeps code clean
(the practice of naming a judgement, as in `bool is_leap = ...`).

#misconception[
  "Writing `x = 3` instead of `x == 3` will behave about the same anyway"
][
  Here chapter 23's warning is collected head on — they are entirely different
  expressions, and the worst of it is that *both are legal*. `x = 3` is a putting
  expression, so it changes x to 3, and that expression's value 3 is read as
  "nonzero, therefore true" — slipped into a condition by mistake it becomes a
  double accident that is *always true and wrecks the variable as well*. So many
  accidents piled up on this trap historically that compiler warnings (`-Wall`)
  point specially at an `=` in a condition — the representative case for why
  chapter 17 said to keep warnings on.
]

== Logical operators — weaving judgements together

Three operators weave judgements: AND `&&` (both true), OR `||` (at least one
true), NOT `!` (invert). The demonstration's second line is an example of `&&` —
`3 < 5 && 2 + 2 == 4`, both judgements true, so 1.

And these operators have a property of particular importance in C —
*short-circuit evaluation*. If `&&`'s left side is false it puts out false
*without evaluating the right side at all* (`||` skips the right side if the left
is true). The demonstration's third line is the proof — the `printf` on the right
is an expression with a side effect (output), and because the left was 0 (false)
*the call itself never happened*.

This is not an optimisation but a *guarantee* — and a precious exception to
chapter 20's "the order of evaluation is mostly unspecified": `&&` and `||`
evaluate the left first and fix even whether the right is evaluated as part of
the contract. So C programmers use this guarantee as a *gatekeeper* — writing
"divide only if it is not zero" as `b != 0 && a / b > 10`, with the left-hand
judgement standing in front of the right-hand danger (chapter 27's division by
zero), is idiom.

Let us record the rule's name exactly — *short-circuit evaluation*. In the
standard's terms there is a *sequence point* (chapter 32) after the evaluation of
`&&`'s and `||`'s left operand, and the right is evaluated only when needed. So
these two are among the few operators in C that guarantee both the order and the
fact of evaluation (the conditional operator `?:` has the same property — the
branch not chosen is not evaluated).

The idioms of practice have hardened into three shapes.

- *Gatekeeper* — `if (p != nullptr && p->count > 0)`. If the left is false the
  dereference on the right never happens at all (this one line blocks chapter
  34's null dereference).
- *Filling a default* — `ok = load(&cfg) || load_default(&cfg);` If the left
  succeeds the right is not even attempted.
- *Deferring an expensive check* — `if (cheap_check(x) && expensive_check(x))`.
  Swap the order and the result is the same while only the cost grows.

The trap is the inverted face of this. *Put a side effect on the right and it may
not be executed.* In `if (init() && start())`, if `init()` returns false then
`start()` is never called — good code if intended, a hard-to-find bug if written
without thinking. And the bitwise operators `&` and `|` do *not* have this
guarantee — both sides are evaluated.

#qa[
  How do `&` (chapter 27) and `&&` really differ — both are called AND.
][
  They live on different layers. `&` is an arithmetic operator doing AND *bit by
  bit* — `5 & 3` puts the bits together and gets 1. `&&` is AND of *whole
  judgements* — it looks only at whether a value is zero or not, and it has the
  short-circuit guarantee. Swapping them by mistake is the more dangerous for
  often giving the same answer by accident — `2 && 4` is 1 (neither is zero) but
  `2 & 4` is 0 (no bits overlap). `&&` for judgement, `&` for bits — the rule is
  not to mix their places.
]

We can now make conditions. In the next chapter the program at last *forks* —
taking different roads according to a condition's value. The homework of
"checking input", deferred in chapter 25, can be done then too.
