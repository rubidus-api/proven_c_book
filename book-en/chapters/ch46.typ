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

== Before the computation even begins — the UB of a file's shape

Undefined behaviour usually brings to mind an accident *during execution*, such as
an overflow or a null dereference. Yet read the standard's list (annex J.2) from the
top and something surprising appears — *the second entry is about the last character
of a file*.

#dtable(
  columns: 2,
  [*what the standard requires*], [*break it and*],
  [a non-empty source file must end in a new-line character], [undefined behaviour],
  [that new-line must not be one preceded by a backslash], [undefined behaviour],
  [the file must not end in a partial preprocessing token or comment], [undefined behaviour],
)

That is, *a file whose last line has no new-line at the end* is outside the contract
however perfect its grammar. This provision has been there since C89 and remains in
C23 (ISO/IEC 9899:2024) — it is the second entry of annex J.2. C++, for reference,
dropped the clause in 2011 (deciding that a missing new-line counts as one appended).
It is a rare place where the two languages parted.

Why should such a thing be UB? Recall the *translation phases* seen in chapter 49 and
the answer appears. The preprocessor works by lines, and one directive is complete
only when a new-line ends it. If the file ends with no new-line, the last line is left
*unfinished*, and what happens next differs by implementation. The third row's
"partial token" is the same circumstance — if the file ends with an unclosed string
literal or a comment with no `*/`, the preprocessor has no ground on which to judge
whether to keep reading into the next file.

Today's compilers mostly append a new-line quietly (older GCC gave
`warning: no newline at end of file`). So the place this clause makes trouble in
practice is not the compiler but *the other tools that handle the file*.

#realcase[
  The practical noise one new-line makes — git and the Unix tools
][
  POSIX defines a *line* as "a string ending in a new-line". So a file missing the
  final new-line becomes, in the eyes of the tools, "a file whose last line is
  unfinished", and the following happens.

  - *A mark is left in git's diff* — that famous `\ No newline at end of file` line.
    If somebody later adds the new-line, a line whose content did not change is *caught
    as a changed line*, making the diff dirty and making conflicts likely at that place
    when branches are merged. The red mark on the last line in GitHub's web view is the
    same thing.
  - *Joining files runs lines together* — with `cat a.txt b.txt`, `a`'s last line and
    `b`'s first line become one line. It is especially tiresome in builds that make
    source or configuration by joining fragments.
  - *Tools that count lines miss one* — `wc -l` counts new-lines, so an unfinished last
    line is not counted.

  So today's practice is one line — *end a text file with a new-line.* An editor
  setting (add a final new-line automatically), `.editorconfig`'s
  `insert_final_newline`, and the formatting tools seen in chapter 79 do that work for
  you. The C standard's clause is, in effect, the oldest ground for that practice.
]

== Other curious pieces of UB

The same list holds several entries that make one ask "even this?". They are things
that happen in *the world of characters and names*, unrelated to computation at run
time. A few, picked out.

#dtable(
  columns: 3,
  [*this code*], [*what is wrong*], [*the standard's place*],
  [`#include "dir\file.h"`], [a `\` inside a header name is UB — writing a Windows path as it stands hits this], [6.4.7],
  [`#include <a//b.h>`], [`//`, `/*`, `'` and `"` likewise are UB], [6.4.7],
  [`#define defined(x) …`], [using `defined` as a macro name], [6.10.9],
  [using `assert` after `#undef assert`], [erasing a standard library macro and then using it], [7.1.3],
  [`int _Value;`, `int __x;`], [trespassing on the reserved name space (chapter 66)], [7.1.3],
  [`memcpy(p, q, 0)` with `p` null], [even at size 0 a null pointer is outside the contract], [7.26.2],
  [`printf("%s", NULL)`], [passing null as a string], [7.23.6.1],
)

The first two rows are especially practical. Writing `#include "utils\str.h"` on
Windows is *undefined behaviour as far as the standard goes* — in reality MSVC handles
it for you, but it becomes a problem the moment you port. What the standard guarantees
is `/` alone, and happily the Windows compilers accept `/` too. Hence the advice always
to use `/` in header paths.

The sixth row surprises people often too. "The size is 0 so nothing will happen — what
does it matter whether the pointer is null?" one thinks, but the standard requires
`memcpy`'s two pointers to be *valid* regardless of the size. Sanitizers (chapter 17)
really do catch this, and the compiler gains the premise "this pointer is not null" and
sometimes erases a null check that follows — exactly the pattern seen in chapter 13.

#misconception[
  "These are theoretical quibbles; nothing actually happens"
][
  In most places nothing really does happen. But that is precisely this chapter's
  theme — *nothing happening is not a guarantee.* A file with no new-line is quiet at
  the compiler and makes noise down the tool chain, and `memcpy(NULL, NULL, 0)` is fine
  until the day the optimisation level is raised and the null check vanishes.

  The practical attitude is this. *Keep the clauses that can be kept for free.* Put a
  new-line at the end of a file, use `/` in header paths, do not begin a name with two
  underscores — the cost of these is zero, and in exchange you gain one thing: "in this
  place I need not suspect anything."
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
