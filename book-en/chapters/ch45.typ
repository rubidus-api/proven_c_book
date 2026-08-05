#import "../../book/lib.typ": *

= Errors and contracts

#organizer[
#idx("contract")  The seed of the contract planted in chapter 32 grows — what a
  function demands and what it promises (preconditions and postconditions), how
  failure is reported (C's way: errors are values), and the devices that force
  that value to be checked. The design idea of proven, seen in chapter 38, is
  organised here into principles.
]

#deepqa[
  At the end of chapter 32 the `fact` function was said to stand on an "implicit
  promise" (n at least 0; overflow at 13 or above). But that promise was written
  nowhere in the code — does it then exist?
][
  It exists, but *in an unkept state* — and that is the heart of the problem. Every
  function has an implicit contract: for what inputs it works properly
  (preconditions), and what it guarantees on success (postconditions). If that
  contract is in neither documentation nor code, a violation passes silently and
  goes off later somewhere unrelated. This chapter is the story of ways to *make
  the contract visible*.
]

== Errors are values — C's way

Many modern languages have a separate channel for failure (exceptions); C has
none. In C failure is *reported through the return value* — the function's result
itself carries "did it succeed?" out with it. There are three conventions.

- *A success/failure boolean* plus receiving the result through a pointer
  (chapter 33's `&` idiom).
- *A special value* marking failure — `malloc`'s null (chapter 40), `fgets`'s
  null, a negative return, and so on.
- *An error code* returned with 0 meaning success — the tradition of the system
  call family.

We see the first in a demonstration. It is the fact learned in chapter 27, that
division by zero is outside the contract, governed by a function's contract.

#demo("examples-en/ch45/errval.c")

Three things to read. *The contract is written in comments* — what is demanded
stands beside the code. *On failure the output argument is not touched* —
"nothing is changed on failure" is part of the contract too. And
*`[[nodiscard]]`* — the C23 notation by which the compiler warns if a call
discards this return value. It is a brake on the freedom of chapter 21's "it is
legal to discard a return value", saying "this one value must not be discarded."
That is exactly why chapter 38's proven functions wear this notation.

#misconception[
  "Error handling is an incidental chore that makes code untidy"
][
  A common impression for a beginner, and C's error handling really is
  conspicuously verbose — an `if` attaches to every call. But invert the
  perspective and it is exactly the opposite: *the error path is half of the
  program.* That a file may be missing, memory may run short, input may be
  nonsense, is not an exceptional situation but ordinary reality. The evidence is
  that the overwhelmingly common cause in real accident analyses is "the return
  value was not checked" — code written only for the success path is code half
  written. How to reduce the verbosity (gathering into a common cleanup point,
  using a type that wraps failure) is a matter of technique; the principle of
  *checking* is not a matter of compromise.
]

== The contract in code — assert and defence

#idx("assert")There is a tool for writing a precondition as *code* rather than
documentation — `assert(condition)` of `<assert.h>`. If the condition is false it
stops the program at once and reports the location. Distinguishing its use
exactly is important:

- *`assert` catches the programmer's mistakes* — an internal invariant meaning "if
  we have reached here, this condition must be true" (the same word as
  chapter 31's invariant). Practice is for it to be switched off in release builds
  (`NDEBUG`).
- *What came from outside is not for assert but for checking* — user input, file
  contents and network data are things for which "being wrong is normal", so they
  must always be checked at run time and handled with error values. Validating
  input with assert becomes the accident of the check disappearing entirely in the
  release build.

This distinction is the contract's two faces — the inside (invariants I must
keep) with assert, the outside (promises the other party may not keep) with checks
and error values.

== const — the cheapest contract

There is one more tool for writing a contract in code. It is `const`, introduced
in chapter 23 as "documentation saying I will not change this" and used in
#idx("const")chapter 41 as the mark "this function does not touch the original."
Seen again from this chapter's perspective, const is *the contract clause that
can be written most cheaply* — adding one word in one place in a function
signature promises the caller "your data is safe" and hands the compiler the job
of watching over that promise.

Its effect spans three layers.

*① For people — the burden of reading falls.* The moment you see the signature
`void render(const struct scene *s)`, it is settled that this function does not
change scene. Not having to read the function's body — in a large codebase there
is scarcely a more valuable saving. It is the substance of chapter 23's "the more
of a piece of code that does not vary, the easier it is to read."

*② For the compiler — it becomes grounds for optimisation.* Chapter 13 showed the
editor holding a value in a register, and the key to that judgement was "can this
value change in the meantime?". const is a signal helping that judgement — though
it must be stated exactly: *const is not itself a magic optimisation switch.*
Data arriving through a pointer may still be changed by another route (aliasing),
so const alone does not let the compiler be certain of everything. The definite
gain is on the side of *objects actually declared const* (global constants,
`static const` tables) — the compiler may plant the value directly in the code
(constant propagation) or place it in a read-only region, making it unmodifiable
outright (chapter 37's string literals lived in that place).

*③ For the layers of memory — sharing becomes safe.* Data that does not change
*has no reason to be copied.* Many places may read the same thing together, and
from chapter 11's cache perspective several cores may share and read the same
cache line without any contention — because the false sharing of chapter 12 is an
accident that requires *writing*. That is why the practice of passing large data
as a `const` pointer instead of by value (chapter 41) is both safe and fast.

So modern practice is simple — *make const the default and release only what must
change.* Widening a contract costs; narrowing it takes one word.

#qa[
  How does chapter 38's proven implement this principle?
][
  Three things follow this chapter's principles exactly. First, *failure appears
  in the type* — it returns an `{err, val}` bundle, so writing code that "takes the
  value out without asking whether it succeeded" becomes awkward instead. Second,
  *it forces the check with `[[nodiscard]]`* — discard it and the compiler warns.
  Third, *it takes boundaries and sizes as part of the contract* — blocking, at
  the API level, the root of the boundary violations seen in chapters 36 and 37.
  In summary: a design that writes the contract not in documentation but in
  *types and signatures*. Regard it as implementing, as a component inside C, the
  same direction as the concerns of Rust and Zig seen in chapter 1.
]

We can handle contracts and errors. But there remains a world this book has kept
deferring under the names "outside the contract" and "undefined behaviour". The
next chapter faces that world head on — the most misunderstood and most expensive
subject in C.
