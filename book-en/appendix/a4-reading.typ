#import "../../book/lib.typ": *

= Appendix D — Further reading, tools, and the standard document

What to read after this book. Before answering, one circumstance of this language must
be pointed out. *In C the shelf right after the introductory books is thin — and what
stands on it is mostly old.*

On the C++ side things are different. Large applications are written in C++ in many
places, so above the introductory books intermediate books, design books and
collections of idioms have piled up layer by layer. In C, the books that have long held
that shelf are mostly classics of the 1990s, written before the changes that came with
C99 and after. And much of what actually fills the gap is not "intermediate C" at all
but *systems programming* — Unix system calls, operating systems, computer
architecture. That is generally what a reader picks up next, so the path onward does
exist; it simply has to be found rather than followed.

That this book took the trouble to begin from the computer itself and come as far as
contracts and library design is because it was conscious of that circumstance.

Happily, over the last few years books filling this shelf have been appearing. The
changes since C11 and the settling of C23 gave a reason to write new books. Below are
those among them worth reading.

#platform[
  How references are given in this appendix
][
  The bibliographic entries below follow the *ACM Reference Format*, widely used in
  computing — author. year. #box[title] (edition). publisher, place. identifier
  (ISBN, DOI or URL). Many venues use the IEEE style instead; the order differs, the
  items do not.

  Three things are worth noticing, and they train the eye. *The edition is stated* —
  the same title in another edition is another book, especially among C textbooks.
  *The year is that edition's year.* And *an identifier is given* — titles collide,
  ISBNs and DOIs do not.
]

== Jens Gustedt, #box[Modern C] — a textbook that takes the standard as its criterion

The author, Jens Gustedt, is a research director at France's INRIA and a member of the
C standards committee (ISO WG14) who took part as a co-editor of C17
(ISO/IEC 9899:2018). That is, it is *a textbook written from the side that makes the
standard document*.

Its content is divided into levels of difficulty from 0 to 3, beginning at the basics
and climbing to threads, atomic operations and the memory consistency model. In taking
today's standard rather than old practice as the default it goes in the same direction
as this book, and it goes far deeper. The place of "C's bible" has traditionally been
K&R's, but as *a single-volume textbook taking today's standard as its criterion* there
is at present nothing to match it.

Two things are worth knowing alongside. First, the Korean edition (Gilbut, 2022)
translates the original's second edition and so covers up to C17. What covers C23 is
the original's *third edition*, out in 2024. Second, the author has made an electronic
edition freely available — it can be obtained from INRIA's open repository (HAL). That
means that if a phrasing catches you while reading a translation, or if the newest C23
content is needed, the original can be opened alongside.

== K. N. King, #box[C Programming: A Modern Approach] — the orthodox textbook

The reason for including the only book in this list with no official Korean edition is
that it has been used widely enough to pass as the standard C textbook in the
English-speaking world. It is a book settled as a university text, and its
characteristic is a *spiral* structure that does not pour concepts out at once but
returns to them a little more deeply several times over (the same way this book chose).
The explanations are calm and the examples honest, so it is a good choice when, having
finished an introduction, you want to lay the foundations firmly again.

That the second edition (2008) takes C89 and C99 as its criterion should be known
before reading. But this "oldness" is sometimes an advantage, depending on the
situation — if in practice you must handle an old codebase or an embedded toolchain that
permits only up to C99, a book taking exactly that world as its criterion rather suits.

*References.*

- K. N. King. 2008. #box[C Programming: A Modern Approach] (2nd. ed.).
  W. W. Norton & Company, New York, NY. ISBN 978-0-393-97950-3.
- The author's page for the book: #link("http://knking.com/books/c2/")[`knking.com/books/c2/`] — source code and errata
  (at the time of writing it serves over HTTP, not HTTPS).

== Christopher Preschern, #box[Fluent C] — design and patterns

The original came out from O'Reilly in 2022 as #box[Fluent C: Principles, Practices,
and Patterns]. What this book treats is not syntax but *decisions about structure* —
how to return errors, how to mark ownership and lifetime, how to design flexible
interfaces and iterators. The problems this book's Part XII treated under the name of
contracts are organised there in the form of a list of patterns.

It suits especially a reader stuck at the place of "now that I have learned C, what and
how am I to write with it". But there are points where assessments divide. Reviews in
English commonly emphasise *that it is not for beginners* — that it pays off only after
you have written one non-trivial program. The review by the developers' association
ACCU gave a "conditional recommendation" while questioning that, though two chapters go
to error handling, testing is not treated, and questioning the appropriateness of some
patterns (such as deferred cleanup). Read the pattern list not as a norm but as *a list
of options* and there is much to gain even allowing for those points.

*References.*

- Christopher Preschern. 2022. #box[Fluent C: Principles, Practices, and Patterns].
  O'Reilly Media, Sebastopol, CA. ISBN 978-1-4920-9733-4.
- Publisher's page: #link("https://oreilly.com")[`oreilly.com`]

== Online references

The addresses first.

#dtable(
  columns: 2,
  [*What*], [*Address*],
  [cppreference's C section], [#link("https://en.cppreference.com/w/c")[`en.cppreference.com/w/c`]],
  [The GCC manual (options, warnings, extensions)], [#link("https://gcc.gnu.org/onlinedocs/")[`gcc.gnu.org/onlinedocs/`]],
  [Clang documentation], [#link("https://clang.llvm.org/docs/")[`clang.llvm.org/docs/`]],
  [MSVC documentation], [#link("https://learn.microsoft.com/cpp/")[`learn.microsoft.com/cpp/`]],
  [WG14 (the committee) documents], [#link("https://open-std.org/jtc1/sc22/wg14/")[`open-std.org/jtc1/sc22/wg14/`]],
  [Downloading the free drafts], [#link("https://open-std.org/jtc1/sc22/wg14/www/docs/")[`open-std.org/jtc1/sc22/wg14/www/docs/`] (e.g. `n3220.pdf`)],
  [The ISO online store], [#link("https://iso.org")[`iso.org`]],
  [Korea's KSSN], [#link("https://kssn.net")[`kssn.net`]],
)

The C section of #box[cppreference.com] is the de facto standard reference. For each
function it marks which edition of the standard it entered in and what changed, which
suits handling the "language with several editions" this book has emphasised. For
per-compiler support and warning options, GCC's and Clang's official documentation is
the most accurate.


== Running code straight in a browser

#dtable(
  columns: 3,
  [*Name*], [*What it gives*], [*Especially good for*],
  [Compiler Explorer (`godbolt.org`)], [Source and assembly side by side. Over a thousand C compiler builds, several comparable at once], [Seeing what optimisation deleted (chapter 13), checking version-dependent behaviour (chapter 16)],
  [OnlineGDB (`onlinegdb.com`)], [Editing, running and debugging. Breakpoints and variable views in the browser], [Learning a debugger before installing one (chapter 17)],
  [Wandbox (`wandbox.org`)], [Pick a compiler build and run. Several files too], [Finding code that only works on your own compiler],
  [Coliru (`coliru.stacked-crooked.com`)], [Write the command line yourself. No frills], [Watching warnings change as options change],
  [Replit (`replit.com`)], [A workspace with files and a terminal. Needs an account], [Examples split across files (chapter 51)],
  [TIO (`tio.run`)], [Dozens of languages in one place], [Comparing the same task in another language],
)

== Tools that show you

#dtable(
  columns: 3,
  [*Name*], [*What it shows*], [*Chapters it connects to*],
  [Python Tutor's C mode (`pythontutor.com/c.html`)], [Steps through execution and draws the stack, the heap and pointers], [Chapters 34–42 (pointers, arrays, lifetime, dynamic memory)],
  [Compiler Explorer], [Which machine-code lines a source line became], [Chapter 13 (optimisation), chapter 46 (what an operator becomes)],
  [`cdecl.org`], [A complicated declaration, put into words], [Chapter 55 (reading declarations)],
  [Godbolt's execution pane], [The output and exit status of the same code], [Chapter 50 (exit status)],
)

Visualisers *pay off on small code*. Past twenty or thirty lines the picture gets
too busy to read. The narrower the question — "where does this one pointer
point?" — the better they work.

== Places that hand you problems

What is needed after this book is *writing code yourself*. The sites differ
considerably in character, so choose by purpose.

#dtable(
  columns: 3,
  [*Name*], [*Character*], [*For someone learning C*],
  [Exercism (`exercism.org`)], [Small tasks with *human mentoring*. There is a C track], [The best fit right after the syntax],
  [Advent of Code (`adventofcode.com`)], [A puzzle a day through December, any language], [Good practice at input parsing and data structures],
  [Codewars (`codewars.com`)], [Short problems by difficulty; other people's solutions are visible], [Training at polishing one short function],
  [HackerRank, LeetCode], [Interview problem banks], [Algorithm-centred; a different thing from learning C],
  [Project Euler], [Mathematics through programs], [You will meet integer overflow in the flesh (chapters 26 and 70)],
)

#qa[
  Will solving problems on such sites make one good at C?
][
  Only half of you grows. Code on problem sites is mostly *short, written alone,
  fed fixed input, and thrown away tonight.* Under those conditions almost
  nothing this book emphasised — lifetime, ownership, error paths, interface
  design — is needed.

  So it is better to do two things together. One is *loosening your hands on
  short problems*; the other is *one small program carried all the way to the
  end.* Without the second you never meet the difference between "code that
  runs" and "code you can hand to someone." That second sense is what Part XII
  (proven) and chapter 88, C in practice, are about.
]

== What can be read for free

#dtable(
  columns: 3,
  [*Material*], [*What*], [*Note*],
  [cppreference's C section (`en.cppreference.com/w/c`)], [The de facto standard reference], [Each function shows which standard introduced it],
  [Beej's Guide to C Programming (`beej.us`)], [A light, free introduction], [The same author's network programming guide is better known],
  [Jens Gustedt, *Modern C*], [An intermediate textbook the author offers as a free PDF], [The book discussed in Appendix D],
  [The comp.lang.c FAQ (`c-faq.com`)], [Questions collected by a 1990s newsgroup], [Old, but full of answers to "why is it like that?" — pointers and arrays especially],
  [The SEI CERT C Coding Standard], [Rules from a security standpoint], [Each rule pairs a violation with a compliant version],
  [WG14 documents (`open-std.org`)], [Standard drafts and proposals], [See Appendix D, "How to obtain the standard"],
  [The GCC and Clang manuals], [The exact basis of warnings and extensions], [The primary source for "what does this warning mean?"],
)

== An eye for material — spotting what is out of date

C is a language of many editions (chapter 16), and writing from the 1990s is
still live on the internet. A few signals separate the good from the stale. If
any of the following shows up, *the material is at least twenty years behind*.

#dtable(
  columns: 2,
  [*If you see this*], [*Why it is stale*],
  [`void main()`], [A form the standard never defined (chapter 50)],
  [`gets(buf)`], [Removed outright in C11. There was no safe way to use it],
  [A cast on `malloc`'s return], [Unnecessary in C, and it hides a missing header (chapter 42)],
  [`#include <conio.h>`, `clrscr()`], [Belongs to old DOS compilers (Turbo C)],
  [Declarations with `int` omitted (`f(x) int x; { }`)], [K\&R-era syntax, gone entirely in C23 (chapter 24)],
  ["A pointer is just an integer"], [An explanation that ignores provenance and alignment (chapter 36)],
  [Assuming `char` is always signed], [It is implementation-defined (chapters 26, 62)],
)

There are *trustworthy* signals too — material that says which edition it speaks
of, calls undefined behaviour by its name, and shows output from a build with the
warnings turned on.

#realcase[
  When the first search result is the most dangerous
][
  Search for something like "C string input" and examples using `gets` still come
  up near the top. Those pages rank high not because they are right but *because
  they are old and have accumulated links.* In a language as old as C the
  correlation between ranking and correctness is especially weak.

  The practical knack: *look at the reference first (cppreference), then look for
  examples.* Reverse the order and you learn the stale practice first and correct
  it later. The same holds when asking an AI: checking the function's entry in the
  standard reference before using the answer is the faster route.
]

== How to obtain the standard document

This book has called the standard "the contract". That contract is a thing that can
really be bought, and there is a separate road to reading it without buying.

*The road of buying the official standard.* C's current standard is
*ISO/IEC 9899:2024* (C23), published in October 2024. It can be bought as a PDF
immediately from the ISO online store at a price in the region of 200 Swiss francs
(when a revision appears, the document number and the price change). There is also the
way of buying through each country's standardisation body — in Korea, ISO and IEC
standards can be searched for and bought at the Korean Standards Information Network
(KSSN) run by the Korean Standards Association. In the United States the ANSI webstore
plays the same role. By whichever route it is bought, the content is the same.

The previous editions are sold under their own numbers too — C17 is ISO/IEC 9899:2018,
C11 is 9899:2011, C99 is 9899:1999. They are useful when you must confirm the criterion
edition of an old codebase.

*The road of obtaining the free drafts.* This is the one used far more often in
practice. The standards committee (ISO/IEC JTC1/SC22/WG14) puts its working
documents in a public repository. *These drafts are not "the published
standard"* — let that be nailed down first.

For C99, C11 and C17 the situation is simple. The last working draft before
publication (N1256, N1570, N2176) is effectively the same in content as the final
standard, so unless you must cite the standard document itself the draft is
enough.

*C23 is different.* The widely used N3220 is not the draft immediately *before*
C23 was published: it is an early working draft of the next edition (C2y),
issued after C23 came out, with editorial corrections laid on C23. Its content is
very close to C23 and it is widely referred to in practice, but *where an exact
citation is needed the published ISO/IEC 9899:2024 must be the ground.*

#dtable(
  columns: 3,
  [*edition*], [*official number*], [*free draft*],
  [C23], [ISO/IEC 9899:2024], [N3220 (strictly, a working draft issued just after C23)],
  [C17], [ISO/IEC 9899:2018], [N2176],
  [C11], [ISO/IEC 9899:2011], [N1570],
  [C99], [ISO/IEC 9899:1999], [N1256 (the edition with corrigenda applied)],
)

The place to obtain them is WG14's document list — they are up under #link("https://open-std.org")[`open-std.org`] at
`jtc1/sc22/wg14/www/docs/` with the document number as the name (the C23 draft, for
example, is `n3220.pdf`). Defect reports and proposals are piled up in the same place,
which comes in handy when tracing "why did this clause end up like this".

*The knack of reading it.* The standard document is not a book read from the front but
a book looked things up in. Finding a clause through the contents and the index and
reading exactly that clause's sentences is the canonical way. There are a few
conventions that help in reading.

- "shall" is a requirement, "shall not" a prohibition. *Break a shall in a constraints
  clause and a diagnostic follows; break a shall outside the constraints and it is
  undefined behaviour* (chapter 49).
- Annex J gathers the *lists* of undefined, unspecified and implementation-defined
  behaviour. It is a good place for skimming all at once the three grey zones treated
  in chapter 49.
- The "EXAMPLE" at the end of each clause, and the footnotes, are explanation, not
  normative. Where there is a dispute, the sentences of the body are the criterion.

== The roads after this book

Three of them.

- *Read more* — the books of Appendix D and the free material above, plus the
  habit of opening the standard itself.
- *Write more* — loosen your hands on problem sites, and carry one small program
  all the way to the end.
- *Look deeper* — the fields chapter 88 points at (systems programming, embedded,
  performance) and the library design shown in Part XII.

What this book aimed at was the *floor* under all three. With a solid floor,
whichever road you take shakes less.
