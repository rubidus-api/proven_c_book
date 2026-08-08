#import "../../book/lib.typ": *

= Safe input — blocking overflow, handling failure

#prereq(
  ([chapter 25, Input], [the danger of input]),
  ([chapter 41, Strings], [a string only marks its end]),
)

#deepqa[
  Chapter 41's `gets` was expelled for not taking the container's size, and
  chapter 25's `fgets` is safe because it takes one. Then does "taking the size"
  alone solve the whole safety problem of input?
][
  Only half of it. Passing the size blocks *overflow*, but input has a second
  problem — *the content cannot be trusted* (chapter 25). You expect a number and
  letters arrive; you expect a short line and a long one arrives; and a malicious
  counterpart deliberately crafts input aimed at the boundary (chapter 22's
  format-string attack was a taste).

  Safe input is the sum of [blocking overflow + *handling failure explicitly*].
  Chapters 25 and 41 built the first half, so this chapter builds the second.
]

#organizer[
  Chapter 25 foreshadowed "why safe input is difficult", and chapters 35 and 41
  taught the roots of that danger (the boundary, NUL termination). This chapter
#idx("parsing input")  finishes the dissection of the accident and gathers *how to handle a failed parse*
  into five disciplines. They can be built in standard C alone, and the libraries
  met later stand on them.
]

#chapter-questions()

== Dissecting the accident — the epidemic called the boundary violation

Combine chapter 38's boundary rule with chapter 41's NUL termination and the most
expensive accident pattern in C's history assembles itself. Input longer than the
container overwrites neighbouring memory (a boundary violation) — and if what was
overwritten happens to be *the return ledger of a function call* (whose identity
we see in the next chapter), an attacker seizes the program's flow of execution
with input alone. That is the substance of the buffer overflow attack, an epidemic
running unbroken from the Morris worm (1988, chapter 41) to today's security
advisories. The statistics are lopsided too — in the major security bodies' lists
of "most dangerous software weaknesses", the boundary-violation family has been a
fixture at the top for decades.

Because C decided *not* to check bounds at run time (chapter 38), the defence has
to be built in layers — functions that take a size (`fgets`), warnings and
sanitizers (chapter 17), and *the discipline of the parsing code itself.* That
last layer is this chapter's subject.

== How do the standard tools report failure

First, gather how the tools already in hand announce a failure. That they are all
different is itself this chapter's starting point.

#dtable(
  columns: 3,
  [*Function*], [*How you know it succeeded*], [*Where it catches you*],
  [`fgets`], [It returns a non-null pointer], [*Whether the line was cut* must be checked separately — by looking for `\n` at the end],
  [`scanf`, `sscanf`], [It returns *how many conversions succeeded*], [You do not know where it stopped. Code that ignores the count is common],
  [The `strtol` family], [You look at `endptr` and `errno` together], [Three things must be read together — the return value, `endptr` and `errno`],
  [`atoi`], [There is no way to know], [Failure is indistinguishable from "read a zero". Not used],
)

The `strtol` family has the soundest contract of these, but using it correctly
means checking *three things at once* — which is why practice does not call it
directly but *wraps* it. That wrapping is this chapter's five disciplines.

== Five disciplines for handling a failed parse

#demo("examples/ch42/parse.c")

The listing's `parse_int` puts all five into one function. One at a time.

=== 1. Return failure as a value

Return "did it succeed" and "what is the value" *kept apart*. Return only a value,
as `atoi` does, and there is no room to express failure; report a count, as `scanf`
does, and *what* failed is not preserved.

```c
struct parse_i64 { bool ok; long long value; const char *rest; const char *why; };
```

Returning a struct by value may look costly, but a struct this size usually rides
in a couple of registers (chapter 46). *Writing failure into the type* is worth far
more than that.

=== 2. Let the compiler speak when a check is forgotten

Attach C23's `[[nodiscard]]` and *code that throws the return value away gets a
warning.*

```c
[[nodiscard]] static struct parse_i64 parse_int(const char *text);
```

This moves "you must check" out of the documentation and into *the compiler's job.*
A rule a person has to remember is eventually forgotten, and the place it is
forgotten is exactly the place the accident happens.

=== 3. Say how far it read

`strtol`'s `endptr` is the good precedent. Return *where parsing stopped* as well
and two things become possible — carrying on (the listing's `10,20,30` sum), and
pointing a person at where it went wrong ("it stopped at the third character").

=== 4. Do not count truncation as success

This is the most frequently broken discipline. Return "it did not fit, so I cut it
to size" as a success and what follows is looking up a file under a truncated name
and connecting to a truncated address.

The listing's `copy_line` *does nothing and returns failure* when the text will not
fit. That is the lesson of the long-standing problem of `strncpy` not reporting
truncation (chapter 63).

=== 5. On failure, touch no output

If a half-filled value is left in the result on failure, code will end up using it.
Make *change nothing on failure* the contract and write it in the documentation —
the last part of the listing checks exactly that.

#misconception[
  "Checking `scanf`'s return value is enough to be safe"
][
  Half right. Checking the count tells you *how many conversions succeeded*, and
  nothing more; it does not substitute for disciplines 3, 4 and 5 above.

  - You cannot tell *where it stopped* — there is no good way to re-read the rest.
  - Give `%s` *no width* and it writes without knowing the container's size
    (overflow). Always write the width, as in `%9s`.
  - On failure the contract for *which arguments were already filled* is vague.

  So the practice in the field is "read a line (`fgets`) and parse that line
  yourself". This chapter's five disciplines are the skeleton of that "yourself".
]

== Casting the discipline into a component

Build the same discipline by hand in every function and something is eventually
left out. So practice's next step is *to cast the discipline into a component* —
one where failure surfaces as a value, bounds are always checked, and the compiler
speaks up when a check is forgotten.

#qa[
  Does that mean the standard library is not enough?
][
  Less that it is not enough than that *you have to rebuild it every time.* The
  standard library carries the practice of the 1970s and 80s intact (chapters 63
  and 64), and it gives none of the five disciplines above as a default. So every
  real codebase, without exception, lays *its own thin layer* on top — a layer that
  puts a safe shell around strings, parsing and number conversion.

  Make that layer a library and a whole team stands on the same discipline. This
  book has one such example ready — `proven`, written by the author, and *Part XII*
  covers its design and use. Chapter 85, "Errors are values", takes up disciplines
  1 and 2 at the library level; chapter 88, "Strings and text", takes up 3 and 4.

  What matters is not which library you use but *whether the discipline is carved
  into the component.* Choose another with the same idea, or build your own.
]

We have safe input. But a phrase went past in this chapter without explanation —
"the return ledger of a function call". Where in memory does what live, and when
is it born and when does it die? The next chapter is that map — lifetime and
storage duration.
