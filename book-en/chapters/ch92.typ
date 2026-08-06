#import "../../book/lib.typ": *

= Modern C, gathered up

#prereq(
  ([chapter 49, Undefined behaviour], [undefined behaviour]),
  ([chapter 48, Errors and contracts], [contracts]),
  ([chapter 90, C in practice], [the tools of practice]),
)

#deepqa[
  Chapter 1 declared this book "not an introduction written by the inertia of old
  usage but an introduction setting out from today's C". Then, now that the book has
  been read through, how does the C the reader has learned differ from the old C?
][
  It is not that the list of syntax differs but that *the criterion differs.* A book of
  old usage teaches "write it this way and it works", while this book taught "write it
  this way and it is within the contract" — initialise before use (chapter 23), handle
  the boundary and the size together (chapter 37), confirm failure as a value
  (chapter 48), avoid shortcuts outside the contract (chapter 49), check with tools
  (chapter 17). C23's new syntax appeared as tools that make that criterion *easier to
  keep*. The retrospect below is that list.
]

#organizer[
  The last chapter. Chapter 90 looked round the terrain of practice, so now we gather
  up the road travelled. We look back over the faces of C23 this book has used
  naturally, organise the practices of modern C into a single sheet of guidance, and
  talk about where one can go from here. The book closes with a retrospective
  question and answer over the whole.
]

#chapter-questions()

== Retrospect — the C23 already used

This book did not introduce the new standard separately but used it naturally where it
was needed. Gathered together it comes to this much.

#dtable(
  columns: 3,
  [*what is C23's*], [*where first met*], [*what it made easy*],
  [the `bool`, `true`, `false` keywords], [chapter 29], [judgement as a value without a header],
  [`nullptr`], [chapter 35], [removing `NULL`'s ambiguity],
  [two's complement fixed], [chapter 7], [the end of worrying about the portability of sign representation],
  [`[[nodiscard]]`], [chapters 36 and 48], [the compiler blocks the mistake of forgetting to confirm failure],
  [`[[fallthrough]]`], [chapter 30], [the intent of switch fallthrough as documentation],
  [the `ckd_add` family], [chapter 49], [overflow as a value rather than UB],
  [`alignof`], [chapter 36], [querying alignment requirements at the level of the language],
  [the removal of trigraphs], [chapter 9], [tidying up a scar of the character-code era],
  [the relaxation of `va_start`], [chapter 53], [removing the excess of variadic arguments],
)

One direction is visible — *contracts more explicit, mistakes caught sooner.* This
table is the concrete content of chapter 1's "C itself is showing signs of change".

== The practices of modern C — one sheet of guidance

Compressing this whole book into practical precepts gives this.

*Building and tools.* Turn the warnings on (`-Wall -Wextra`). Run tests with
sanitizers (ASan, UBSan). Where possible, cross-check with two compilers
(chapter 17).

*Declarations and types.* Initialise at the point of declaration. Leave what will not
change as `const`. Where the size is a contract, use the fixed-width types of
`<stdint.h>`. Keep sizes and indexes consistently in the `size_t` family (chapter 28's
trap of mixing signedness).

*Memory.* Boundaries are always explicit — an array and its length travel together.
Confirm a pointer's validity before writing through it. For dynamic allocation, settle
the owner, and empty the pointer after freeing. Solve it with automatic lifetime where
possible (chapters 37 and 42).

*Errors.* Report failure as a value, and check return values. Internal invariants with
assert, what came from outside with checks at run time (chapter 48).

*Contracts.* Judge not by "it ran on my computer" but by "is it correct on the abstract
machine" (chapters 12 and 49).

*Components.* Know the contracts of the standard library and choose accordingly, and
lay components with checking built in over the repeated danger zones (chapter 56 and
Part XII).

#qa[
  Where is it good to go from here?
][
  Three branches are recommended. *Depth* — the places this book has left the door open
  to: data structures and algorithms (chapter 43's linked structures), concurrency
  (chapter 12's multicore and C11 atomics), systems programming (files and networks are
  the world of platform APIs). *Breadth* — the neighbouring languages: when looking at
  C++, Rust or Zig, the sense of memory, contracts and representation learned in this
  book translates over as it stands (as foretold in chapter 1). *Practice* — reading
  directly the source of the projects named in chapter 90. Well-written C code like
  SQLite or curl is a textbook in which every theme of this book is alive in the flesh.

  And it is worth remembering too that the standard document itself has now become a
  text you can read — seen through the viewpoint of "a contract", it becomes clear why
  that curt document was written as it was.
]

== Books to read next, and the standard document

For readers who will not stop here, further reading has been gathered separately —
that is *appendix D*. C is a language unusually empty in the shelf right after the
introductory books, but over the last few years books filling that shelf have been
appearing. The appendix notes those books' strengths and weaknesses, together with how
actually to obtain the standard document this book has all along called "the contract"
(buying it, and downloading the free drafts).

== Finally

This book began in chapter 4 by drawing the computer as three parts. That picture
promptly collapsed honestly — memory became a ladder, execution overlapped, an editor
cut in — and what remained in its place was *the contract*. That knowing C is not
memorising syntax but being able to read that contract is the one thing this book
wanted to say.

To the reader who has come this far by reading alone, only one thing now remains —
writing what was read as your own code. May the journey be a good one.
