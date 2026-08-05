= Preface

This book is an introduction to proven, a C library I wrote, and it is also a
piece of promotion for it.

But before you can recommend a tool, you have to show why it is needed. The
problems proven addresses live in the subtle corners of C — the limits of
representation, the rules of conversion, the lifetime of storage, the areas the
standard declines to promise anything about. To someone who has not seen those
corners, proven looks like an unnecessary contraption. So I decided to explain
the problems first, and the library afterwards. That is why most of this book
is written in plain standard C, and why proven does not appear until Part XII.
Having read the earlier chapters, you may well decide you do not need proven.
That is a fine outcome too.

The upshot is that this book *is also an introduction to the C language.* It was
written with a first-time reader in mind, so it begins with how a computer is
built and goes on through types and flow, pointers and arrays, the lifetime of
storage and the standard library. Even if proven turns out not to be what the
reader needs, the earlier parts should be of use in themselves.

== What this book gives, and what it does not

One thing must be clear first. *Reading alone will not grow your ability to
program.* This book has no exercises, no assignments, no "try it yourself".
What it gives you is an eye for reading code and naming the dangers in it.
Standing in front of an empty file and breaking a problem into pieces,
digging out the name of a function that will not come to mind, clearing the
errors a compiler has poured out one by one, narrowing down the cause when
the output is not what you expected — that is a different ability, and it
grows *only by writing*. This book does not do that work for you, and I will
not pretend it can. That part has to continue in some hands-on course, or in
small programs of your own.

That is a shortcoming of this book, but a deliberate choice as well. Put
exercises and assignments in and the volume swells, and the reader's burden
swells with it. I chose the side that gets read to the end without strain, and
gave up drill in exchange. *This is a book for understanding; it is not a book
for making something quickly.*

I should also say in advance that the first program comes late. Hello world
does not appear until Part III. Until then we build up how a computer is
shaped, how memory is divided, how numbers and letters are represented. That
order is deliberate: it is there so that pointers arrive as a concept rather
than as syntax. To a reader who wants to build something soon, the order may
feel slow. Such a reader may read chapter 15, hello world, first and come
back to chapter 1 — nothing is lost by breaking the order.

What the book gives in return is *a firm foundation*: not a quick result but
an understanding that lasts. Skills picked up from other books and courses
sit far more steadily on top of it. The gap between someone who knows why
integers wrap, why a pointer is not simply a number, and what a compiler does
and does not promise, and someone who does not, only widens with time.

There is also, in these pages, a certain amount of feeling from someone fond of
C. Even the dangerous and the inefficient parts — the ones that piled up as the
language compromised with the constraints and limits of an older age — have
always struck me as interesting and enjoyable. For the fact that this fondness
has found its way in as explanation and length somewhat beyond what was needed,
I apologise to the reader in advance.

Every piece of code in this book has actually been compiled and run. The
printed output is not transcribed by hand: it is what the machine produced when
the examples were run during the build, cross-checked with two compilers (gcc
and clang).

One more disclosure. *This book was written with AI as an assisting tool.* What
to cover and in what order, which principles the exposition follows, what to
include and what to leave out — all of that I decided; the manuscript was
written to those instructions and then reviewed by me. Technical claims were
checked against the standard and primary sources, and, as said above, the
examples are verified by a machine that actually runs them on every build —
which means that whether a human or an AI wrote it, *code that does not run
does not appear in this book*.

Errors will remain nonetheless. Those are mine, not the tool's. If you find
one, please tell me.

== Copyright and contact

The text (the prose and figures of this book) is licensed under
*CC BY-NC-SA 4.0*. You may share and adapt it freely with attribution, but not
for commercial purposes, and adaptations must carry the same license.

The example code is licensed under the *MIT license*. Anything you learn to
write here, you may take into your own programs without restriction.

Contact is by email. Error reports and correction requests are better made
through the GitHub repository — they leave a record, and other readers can see
them.

What I accept is *error reports only.* Incorrect statements, examples that do
not match, typos, stale information — tell me and I will gratefully fix them.
Manuscript contributions, on the other hand — a new chapter or section written
and sent in — I do not accept. The structure and voice of this book are better
kept under one person's judgement, and I would rather keep the copyright
situation simple.

#v(0.2cm)
#link("mailto:rubidus@gmail.com")[rubidus\@gmail.com] \
#link("https://github.com/rubidus-api/proven_c_book")[github.com/rubidus-api/proven_c_book]
