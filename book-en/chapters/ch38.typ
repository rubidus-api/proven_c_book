#import "../../book/lib.typ": *

= Safe input, and the appearance of proven

#prereq(
  ([chapter 25, Input], [the danger of input]),
  ([chapter 37, Strings], [a string only marks its end]),
)

#organizer[
  Chapter 25 foreshadowed "why safe input is difficult", and chapters 33 and 37
  taught the roots of that danger (the boundary, NUL termination). This chapter
  completes the dissection of the accident — and the guest promised in chapter 1,
  this book's underlying library *proven*, finally appears. Exactly as promised:
  "at the point where the need has proved itself."
]

#deepqa[
  Chapter 37's gets was expelled for not taking the container's size, and
  chapter 25's fgets is safe because it takes one. Then does "taking the size"
  alone solve the whole safety problem of input?
][
  Only half of it. Passing the size blocks *overflow*, but input has a second
  problem — *the content cannot be trusted* (chapter 25). You expect a number and
  letters arrive; you expect a short line and a long one arrives; and a malicious
  counterpart deliberately crafts input aimed at the boundary (chapter 22's
  format-string attack was a taste). Safe input is the sum of [blocking overflow +
  handling failure explicitly], and this chapter's two pillars are those two.
]

#chapter-questions()

== Dissecting the accident — the epidemic called the boundary violation

Combine chapter 36's boundary rule with chapter 37's NUL termination and the most
expensive accident pattern in C's history assembles itself. Input longer than the
container overwrites neighbouring memory (a boundary violation) — and if what was
overwritten happens to be *the return ledger of a function call* (whose identity
we see in the next chapter), an attacker seizes the program's flow of execution
with input alone. That is the substance of the buffer overflow attack, an
epidemic running unbroken from the Morris worm (1988, chapter 37) to today's
security advisories. It is overwhelming statistically too — in the "most
dangerous software weaknesses" lists of major security bodies, the
boundary-violation family has been a top fixture for decades.

Because C chose *not* to do run-time boundary checking (chapter 36), defence must
be stacked in layers — functions that take a size (fgets), warnings and
sanitizers (chapter 17), and *components with checking built in from the start*.
That last layer is this chapter's guest.

== proven — fundamentals, verified, as a component

proven is a C library made by this book's author (exactly as disclosed in
chapter 1), and its design idea is one sentence — *provide the fundamentals where
accidents are commonest (strings, interpreting input, converting numbers) in a
form where failure shows up as a value and boundaries are always checked.* In the
language of chapter 9's representations, it builds a safe layer of the
length-and-capacity family on top of the NUL-terminated world.

For a first demonstration, chapter 25's homework — handling the failure of
interpreting input — solved with proven. *Both* the success path and the failure
path are shown:

#demo("examples-en/ch38/scan.c")

How to read it — `proven_scan_init` sets a scanner over a sequence of characters,
and `proven_scan_i64` interprets one integer. The return value is the heart of
this design: it returns *not a value but an `{err, val}` bundle*, and the caller
takes the value out only after confirming success with `proven_is_ok`. Compared
with sscanf (chapter 25) — instead of the conventional signal "the number of
successes", *failure is stated in the type*, and the function carries
`[[nodiscard]]` (C23 — the notation "warn if this return value is discarded"), so
*the compiler catches the mistake of forgetting to check for failure*. It is the
flesh of the "errors are values" discipline treated formally in chapter 45.

#qa[
  Why add a library for work the standard library can do — more components means
  more to learn.
][
  This book's answer is the distinction of layers. *The concepts were learned
  fully in standard C* — coming this far, through chapter 36, without proven is
  the evidence, and take proven away and this book's C knowledge still stands
  (chapter 1's promise). On top of that, when choosing *the fundamentals for real
  work*, choosing while knowing the standard library's historical traps (gets's
  funeral, scanf's tangles, unchecked string functions) is the modern choice —
  the safety Rust and Zig build into the language (chapter 1) is obtained in C by
  choosing components. proven is one of those options and this book's
  demonstration material; choose another library of the same philosophy and the
  principle is the same.
]

#qa[
  proven's function names are long — `proven_scan_i64` and the like. Why name them
  this way?
][
  Because C has no namespaces (chapter 24's scope belongs to blocks, and names
  that cross file boundaries live in one yard — chapter 48). If libraries use the
  same name they collide at the linking stage (chapter 16), so the practice of C
  libraries is for *a prefix to stand in for a namespace* — `proven_` is that
  fence. Long, but with the practical benefit that the name alone reveals which
  component it belongs to.
]

(This chapter, being a first meeting, writes only as much as is needed — the
library's whole design and use are treated across three chapters in Part XII.
That is why this book is at once an introduction to C and a guide to proven.)

We have safe input. But a phrase went past in this chapter without explanation —
"the return ledger of a function call". Where in memory does what live, and when
is it born and when does it die? The next chapter is that map — lifetime and
storage duration.
