= Preface

This book is an introduction to proven, a C library I wrote, and it is also an
introduction to the C language. There is a reason for that awkward double
stance.

Before you can recommend a tool, you have to show why it is needed. The problems
proven addresses live in the subtle corners of C — the limits of representation,
the rules of conversion, the lifetime of storage, the areas the standard
declines to promise anything about. To those who have not seen those corners,
proven looks like an unnecessary contraption. To those who know them well, on
the other hand, their own ways of handling them already exist, and there is
little reason to reach for proven.

So I decided to explain the problems first and the library afterwards. It was
written with a first-time reader in mind, so it begins with how a computer is
built and goes on through types and flow, pointers and arrays, the lifetime of
storage and the standard library. Even if proven turns out not to be what you
need, the earlier parts should be of use in themselves. Having read them you may
well decide you do not need proven — and that is a fine outcome too. It will
have helped you solve problems in C either way.

C has a long history, and in it are all sorts of scars and contortions and
serious deliberations, each an attempt to get past the real limits and
difficulties of its time. Tracing those marks has been an interesting and
enjoyable thing to do, so adding my own effort and my own marks to them was the
natural next step. That proven exists, and that this book exists, comes less
from necessity than from the affection of someone who likes C; I will not
pretend otherwise. That the affection has spilled into rather more explanation
and rather more pages than strictly needed — for that I apologise to the reader
in advance.

My thanks go in advance to everyone who reads this. That someone spends their
time sharing and discussing my work is always a happy thing.

== What this book intends, and how to use it

One thing must be clear first. *Reading alone will not grow your ability to
program.* This book has no exercises, no assignments, no "try it yourself". It
sets out a situation, asks a question, and gives an answer to it — that is all.
What it intends is that the reader end up with a firm grounding in C, and with
a feel and an eye for the traps and dangers built into it. That is somewhat
different from the ability to actually produce code: standing in front of an
empty file and breaking a problem into pieces, digging out the name of a
function that will not come to mind, clearing the errors a compiler has poured
out one by one, narrowing down the cause when the output is not what you
expected. That grows *only by writing it yourself.* This book does not do that
work for you, and I will not claim it can. That part has to continue in some
other course of practice, or in small programs of your own.

That is a flaw in the book, and also a deliberate choice. Exercises and
assignments swell the length and the reader's burden along with it. I chose
the side that gets read to the end without strain, and gave up drill in
exchange. *This is a book for understanding, not a book for making something
quickly.*

I should also say in advance that the first program comes late. Hello, world
does not appear until Part III. Until then the book lays the groundwork — how a
computer is built, how storage is divided, how numbers and letters are
represented. That order is deliberate, so that C is met as a set of ideas
rather than as syntax. A reader in a hurry to build something may find it
slow-going. Such a reader may read chapter 15, hello world, first and come back
to chapter 1. The order does not matter.

What the book intends is *a firm foundation*: not a quick result but an
understanding that lasts. Skills picked up from other books and courses sit far
more steadily on top of it. The gap between someone who knows why integers
wrap, why a pointer is not simply a number, and what a compiler does and does
not promise, and someone who does not, only widens with time.

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
