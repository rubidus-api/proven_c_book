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

The upshot is that this book *is also an introduction to the C language.* It is
written with a first-time reader in front of it, so it begins with how a
computer is built and goes on through types and flow, pointers and arrays, the
lifetime of storage and the standard library. A reader with no intention of
using proven can read Parts I to XI as a C primer and stop there — that stretch
stands on its own, without the library.

There is also, in these pages, a certain amount of affection for C. Explaining
why a language past its fiftieth year is still here, and what its design gave
up in order to gain what it gained, tends to produce that. I have not hidden
it.

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
