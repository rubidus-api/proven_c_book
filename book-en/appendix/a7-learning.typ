#import "../../book/lib.typ": *

= Appendix F — Online tools and learning material

This is a book to be read, so it carries no exercises (see the preface). Here are
the places that take over that part — where you can run code straight in a
browser, where memory is drawn for you, where problems are handed to you, and
what can be read for free.

Addresses are given as bare domains rather than links. Sites change and
disappear, but the names usually still find them. The few chosen in chapter 17
are separated here by character as well.

== Running code straight in a browser

#dtable(
  columns: 3,
  [*Name*], [*What it gives*], [*Especially good for*],
  [Compiler Explorer (`godbolt.org`)], [Source and assembly side by side. Over a thousand C compiler builds, several comparable at once], [Seeing what optimization deleted (chapter 13), checking version-dependent behaviour (chapter 16)],
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
  [Compiler Explorer], [Which machine-code lines a source line became], [Chapter 13 (optimization), chapter 46 (what an operator becomes)],
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
