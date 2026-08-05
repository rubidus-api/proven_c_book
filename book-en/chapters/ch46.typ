#import "../../book/lib.typ": *

= Undefined behaviour

#organizer[
  We face head on the world this book has kept deferring under the name "outside
  the contract". What UB exactly is and why it exists, why it is not "a little
  dangerous" but "anything at all is possible" — and how to avoid and catch it in
  practice. The contract narrative begun in Part II is completed here.
]

#deepqa[
  Chapter 13 said that violating strict aliasing is "the act of telling the
  compiler something untrue as if it were true." Then what has the standard
  permitted the implementation by leaving some behaviour "undefined"?
][
  It decided *to demand nothing at all.* The standard's sentence is cold — for
  undefined behaviour the standard "imposes no requirements". That is, such a
  program has no correct execution result whatever. The common misunderstanding is
  "dangerous behaviour, but it mostly works as expected", whereas in the eye of the
  contract the whole program *loses its meaning*. That difference is this chapter
  entire.
]

== Three grey zones — UB, unspecified, implementation-defined

#idx("implementation-defined")#idx("unspecified")#idx("undefined behaviour (UB)")Let
us first separate three confusable words. Organised through the cases this book
has met so far:

#dtable(
  columns: 3,
  [*kind*], [*the standard's attitude*], [*examples met in this book*],
  [implementation-defined], [the implementation decides and *documents it*], [the size of `int` (chapter 26), the signedness of `char`],
  [unspecified], [one of a fixed set of choices, with no duty to document], [the order of evaluating subexpressions in one expression (chapter 32)],
  [undefined behaviour (UB)], [it imposes no requirements at all], [signed overflow (chapter 7), boundary violation (chapter 36), null dereference (chapter 34)],
)

The first two are worlds where "there are several answers but there is an answer."
Only UB is a world where there is no answer at all.

== Why it exists

There are two reasons for leaving an outside-the-contract region. First, *because
machines differ* — chapter 7's shift of at least the width is the representative
case. x86 and older ARM respond differently, and had the standard chosen one side
the other machine would have to insert correction code every time. Instead of
taking sides it said "do not write that code." Second, *to obtain premises for
optimisation* — as chapter 7 showed, the premise that "signed integers do not
overflow" is what lets the compiler analyse and reorder loops (chapter 13). These
clauses are the price of C remaining the language of speed for half a century.

== The real face of "anything at all"

UB's result is not only a collapse. The pattern seen in chapter 13 is more
frightening — *the compiler reads UB as "a thing that cannot happen" and deletes
code.* A null check disappears entirely (if the pointer was already dereferenced,
it infers "it cannot be null"), an overflow check disappears (since signed
overflow is premised not to happen), a loop becomes infinite or vanishes
altogether. So UB's representative symptom is not "dying on the spot" but *a bug
that appears somewhere unrelated, disappears when the optimisation level changes,
and is hard to reproduce.*

#realcase[
  The vanished null check — Linux kernel CVE-2009-1897
][
  There was an incident in which this pattern really went off in the kernel. The
  code went roughly like this — the pointer `tun` was dereferenced first to take a
  value out, and below that a null check `if (!tun) return ...;`. The order was a
  mistake, but to a human eye it looks like "the check is still there, so a null
  will be caught." The compiler's inference was different: *it was already
  dereferenced → had it been null that would have been UB at that moment → UB is
  premised not to happen → therefore tun is not null → the null check below is dead
  code.* The check was removed entirely by optimisation and, combined with an
  environment in which the null page could be mapped, became a
  privilege-escalation vulnerability. The compiler worked by the rules; what
  collapsed was the contract.
]

== How to avoid it — discipline, tools, and components

Defence in practice is three layers.

*Discipline* — the rules this book has passed through are the list: initialise
before use (chapter 23), keep boundaries (chapter 36), check for null
(chapter 34), change one variable only once in one statement (chapter 32), do not
take shortcuts outside the contract (pointer casts, assumptions about
representation) (chapters 11 and 35).

*Tools* — the nets equipped in chapter 17. Compiler warnings catch at compile
time; UBSan and ASan catch at run time. The *checked arithmetic* brought in by
C23 is a tool of this layer too — functions that report overflow as a value
instead of making it UB:

#demo("examples/ch46/checked.c")

`ckd_add` returns "did it overflow?" as its return value — the standard's answer
to the trap learned in chapter 7 ("signed overflow is outside the contract"),
governed by the discipline learned in chapter 45 ("errors are values").

*Components* — using an API in which violating the contract is difficult to begin
with (chapter 38's proven is that layer). Chapter 17's metaphor — tools are nets,
good components are footholds — is completed here.

#qa[
  Must all the UB be memorised? I hear the standard has hundreds of them.
][
  Memorising is not the goal — the list is vast and continually refined. What works
  in practice is *an instinct*: the habit of asking "are my grounds for saying this
  code is correct in the contract, or is it that it ran on my computer?" And
  backing that instinct with tools — turning warnings on, running tests under
  sanitizers, cross-checking with two compilers (chapter 17). The reason this book
  has repeated "is it correct on the abstract machine" since Part II is precisely
  to plant that instinct.
]

== Closing Part IX

The part of precision is over — how to handle approximation (chapter 44), how to
handle failure (chapter 45), and how to know the world outside the contract
(chapter 46). The three chapters share one theme: *C is a language that entrusts
much to the programmer, and the person who knows what has been entrusted writes
safe code.*

The last parts remain. Every program so far has been a single file — now it grows
into several files (chapter 48), we face the layer of preprocessing and
translation (chapter 49), we learn the terrain of the standard library
(chapter 53), we treat proven head on (Part XII), and we close the book with the
practices of modern C (chapter 81).

The next part is the story of *composing* a program. Its first chapter is the
place we have used only as a six-line convention until now — `main` itself. We see
its three forms, and where the value it returns goes.
