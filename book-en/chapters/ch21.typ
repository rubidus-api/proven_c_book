#import "../../book/lib.typ": *

= Using functions — how to call

#prereq(
  ([chapter 20, Expressions], [an expression becomes a value]),
  ([chapter 19, The structure of a program], [the lump of work called a function]),
)

#organizer[
  How to *make* a function is still some way off (chapter 24). This chapter is
  how to *call* one — learn the three words call, argument and return value, and
  hello world's heart, `printf("Hello, world!\n");`, reads as a complete
  sentence. Employing workers other people made — half of programming is really
  this.
]

#deepqa[
  At the end of chapter 20 we said "calling a function is itself an expression,
  and therefore becomes a value." Then the value 14, calculated from the
  expression `2 + 3 * 4` inside `printf("%d\n", 2 + 3 * 4)` — where exactly does
  it go?
][
  Into the function's mouth. When a function is called, the values of the
  expressions written inside the parentheses are passed to it as *materials*,
  and the function does its work with them and puts out a *result value*. The
  materials are called arguments, the result the return value — those two words
  are the whole of this chapter.
]

#chapter-questions()

== The call — say the name and a worker runs

A function is *a worker with a name*. Somewhere a way of working (a body) has
been made, and we need only say the name. The syntax of calling is already
familiar to the eye:

```c
name(material1, material2, ...)
```

This is a *call*. The order in which a call executes is: ① the expressions
inside the parentheses are evaluated into values first, ② those values are
passed to the function, ③ the function's body executes, and ④ the *return
value* the function puts out becomes the value at the call site. That last item
collects chapter 20's foreshadowing: *a call is an expression* — read the place
where a call is written as turning into a single return value once execution
finishes.

Since a call is an expression, a call can be put in as the material of another
call. Here is a demonstration — `abs` is a worker from the standard library that
takes one integer and returns its absolute value (it lives in the `<stdlib.h>`
toolbox, which is why one more `#include` has appeared).

#demo("examples-en/ch21/call.c")

The order is exactly as it reads — the inner expression `2 - 10` first becomes
$-8$, that enters `abs` as material and becomes the return value $8$, and that
$8$ becomes `printf`'s material and is printed. *From the inside out, values are
relayed* — this is the basic reading of C code in which expressions and calls
are stacked layer upon layer.

== The return value — used, or discarded

Every call becomes a value, we said. Then in hello world's
`printf("Hello, world!\n");`, where did the return value go?

*It was discarded* — and that is legal. Put a semicolon after an expression and
you get a statement (an expression statement) that "evaluates the expression and
throws the value away." `printf` has a return value too (it returns the number
of characters printed). But usually the printing *side effect* is the point and
the character count is of no interest, so the value is discarded. "A statement
that calls a function" was in fact "a call expression + throwing the value
away."

#qa[
  If the value may be discarded, why return it in the first place?
][
  Because someone else uses it. For the same worker, the caller's circumstances
  may require the result or only the side effect — the function puts out a
  result regardless, and whether to use or discard it is the caller's decision.
  That is C's division of labour. But this generosity has a shadow: if the return
  value is what reports "did it succeed?", discarding it means missing the
  failure. C's way of reporting errors as values, and the story of "return values
  you must not discard", is the subject of chapter 45.
]

#qa[
  Does `abs` always return a positive number? Being an absolute value, it seems
  obvious.
][
  Almost always — and yet there is a trap in exactly one place. Chapter 7 taught
  the one asymmetry of two's complement: the most negative number (−128, or
  about −2.1 billion in 32 bits) has *no* positive partner. So what happens when
  you ask for that number's absolute value — there is no container to hold it, so
  it is outside the contract (undefined behaviour). The trap sits exactly one
  step outside "surely this just works" — a case where chapter 7's asymmetry left
  its trace even in a function's manual. This kind of boundary hunting becomes
  chapter 46's speciality.
]

== The standard library — workers made in advance

Neither `printf` nor `abs` was made by us. They are workers of the *standard
library* that comes with a C installation — the very bundle joined to our
program at chapter 16's linking stage. Each toolbox (`<stdio.h>`, `<stdlib.h>`
and so on) contains a roster (declarations) of the workers you may use, and
`#include` fetches the roster so that box's workers can be called.

Which boxes the standard library has and what is in them is surveyed with a map
in chapter 48. What to take away now is one instinct — *half of programming is
calling functions other people made.* Recognising a well-made worker and
employing it correctly decides half of your code, which is why "which worker
shall I use?" becomes a large subject again in the later part of this book
(chapter 38).

Only two slots of hello world remain — exactly how the format inside `printf`'s
quotation marks works (next chapter), and the identity of `int main(void)` and
`return 0` (Part V). The next chapter faces output head on and brings us close
to Part IV's goal.
