#import "../../book/lib.typ": *

= Setting the scene

#organizer[
  What programming is, what kind of language C is and where it currently
  stands, and why a new introduction to C is needed right now. This chapter
  also settles how the book was written and how it is meant to be read.
]

== What is programming

A computer is a machine that does what it is told. The difficulty is in the
telling. A computer does not understand human speech; it moves only when it
receives brutally simple instructions in order. *Programming* is the work of
turning what we want into a list of such simple instructions, and the agreed
notation for writing that list down is a *programming language*.

There are hundreds of languages. Some sit close to the human side and are
comfortable to write; others sit close to the machine and let you handle it
down to its details. C, the subject of this book, is the leading example of
the latter — the language of systems, which has held the machine-side seat for
half a century.

== Where C is

C is not easy to spot. Other languages stand at the front of flashy apps and
websites. Lift the faces of those, though, and C is nearly everywhere
underneath.

#realcase[
  A day spent on top of C
][
  The alarm goes off on your phone in the morning — the core of that operating
  system (the kernel) is written in C. You send a message and the firmware of
  the radio module goes to work — C. You open the web and a server and a
  browser exchange data — the encryption library and transfer tool doing it
  (OpenSSL, curl) are C. The app stores something — SQLite, the most widely
  deployed database in the world, is C. The braking system in a car, the
  control panel of a washing machine, a pacemaker: most embedded devices are C.
  Even when you run machine learning from Python, a large part of the engine
  actually doing the arithmetic underneath is C (and its neighbours). Learning
  C means becoming able to read and write this invisible layer.
]

== The world of C is in motion right now

So why a new introduction *now*? If the language is old, surely an old book
will do. It will not — the landscape around C looks quiet but is shifting a
great deal, and that shifting is where this book starts.

*First, several eras of C coexist inside the language.* C is governed by an
official specification, the standard, and it has come through C89 (1989), C99,
C11 and C17 to the current C23. The trouble is that old standards never
retire. Industry is still full of code written exactly as it was in the C89
days, many textbooks are written in that era's idiom, and alongside them C23
has brought new vocabulary such as `bool` and `nullptr` into the standard.
Dialects thirty years apart live together under the single name "C".

#qa[
  If there is that much old-standard code about, is it not more practical to
  learn the old idiom first?
][
  Separate reading from writing and the answer settles. The day will come when
  you have to *read* old code — which is why this book covers history and the
  stories behind old idioms throughout. But there is no reason for code you
  *write* today to be in the old idiom. The newer standards are the result of
  decades of field experience filling in the traps of the older ones. The
  earlier you are in your learning, the more you gain by starting from today's
  safest idiom; the old idiom is quite enough as background knowledge of "why
  they wrote it that way."
]

*Second, neighbouring languages are encroaching on C's territory.* C++, which
started under the same roof, has gone through revisions at a rapid pace and
become an enormous language — vastly expressive, but grown large enough that
people say hardly anyone knows all of it. From another direction a new
generation of systems languages has appeared. Rust promises to seal off, at
the language level, the memory accidents C has long suffered; Zig marches
under the banner of "succeed C, but simpler and more honest." Each is pushing
into ground C used to hold alone. Rust code beginning to enter operating
system kernels was a symbolic moment.

*Third, under that pressure C itself is showing signs of change.* C23 was the
largest revision in recent memory, and the committee continues work on
tightening the rules for pointers (provenance) and on the safety discussion. C
is a slow-moving language, but the direction is clear — towards clearer
contracts and safer defaults.

#misconception[
  "C is an old language, so learning it will soon be pointless"
][
  It is a plausible worry — the new languages are arriving in force. But the
  actual numbers point the other way. As we saw above, the world's
  infrastructure stands on C, and this layer is so expensive to replace that it
  moves only on the scale of decades. Even the new languages are born with the
  ability to talk to C (a C interface) as a condition of survival — C is the
  lingua franca of the systems world, so whether you write Rust or Zig you end
  up having to read and understand C. Far from ageing out, C is the kind of
  language that grows *more* important as its neighbours multiply.
]

#qa[
  Then would it not be better to start with Rust or Zig instead?
][
  Honestly: it depends on your goal. If the goal is to build one safe
  application right now, other choices are good too. But if the goal is to
  understand *how a computer actually works* from the ground up, there is no
  better teaching material than C. C is the language that wraps the machine
  most thinly, so learning C turns out to be the same thing as learning about
  the machine, memory and the compiler. And that understanding is capital you
  can carry with you into Rust or Zig unchanged. This book is here to supply
  that capital in today's idiom.
]

== Why this book was made

That shifting landscape is the first reason. With several standards
coexisting, neighbouring languages competing, and C itself changing, what is
needed now is not an introduction written by the inertia of the old idiom but
one that *starts from today's C (C23) and today's practice*. So the table of
contents was rebuilt from scratch. Instead of listing grammatical items in
order, the book starts with the basics of the machine and meets C in the order
in which the concepts become necessary.

The second reason is best stated plainly. The author builds and publishes a C
library called *proven*. proven aims to provide, in verified form, the
fundamentals where C programming most often goes wrong — handling strings,
processing input, converting numbers. Most of C's famous traps go off in this
layer of fundamentals, and the standard library, for historical reasons,
carries those traps as they are. The later parts of this book show why the
traps are genuinely dangerous and then use proven as the basis of examples for
how they can be blocked. This book is also meant to make that library known —
but the order is respected. First the concepts are learned properly in
standard C alone; proven appears at the point where its necessity has proved
itself.

#qa[
  If it is a book promoting a library, does the content not lean that way?
][
  The structure is built to prevent it. The first thirty-odd chapters do not
  use a single line of proven — they proceed purely on the basics of computing
  and standard C. proven appears only after the text itself has made plain
  "why a tool like this is needed", and even then the standard approach is
  shown first and proven offered as a comparison. The C knowledge in this book
  stands entirely on its own without proven.
]

== How to read this book

This is a book to be read. That sounds obvious, but as a promise from a
programming book it is unusual.

*It was written to be readable straight through.* So there are no exercises
and no instructions to "try it yourself." You do not need to be sitting at a
computer — on a sofa, on the train, you can read it front to back like a
novel. Every piece of code is demonstrated on the page from beginning to end,
and the results of running it are printed on the page as well. Those results
are not decoration but the real thing: every example in this book carries the
output obtained by a machine actually compiling and running it.

If you want drills, another book is better. Designing good assignments and
walking you through them step by step is something the existing good C primers
do far better. This book's place is beside them — the reading side, the one
that explains why things are shaped as they are.

*It proceeds by question and answer.* Throughout the text, the question a
reader might have just formed is asked on the spot and answered immediately.
It is best to pause for a moment when you meet a question and try to answer it
yourself before reading on, but reading straight through is fine too — the
exchange is part of the explanation.

*It does not ask you to memorise.* Anything worth remembering comes back at
the head of a later chapter under the name "looking back", where you meet the
previous chapter's material again as a slightly deeper question. That is this
book's revision device. It is fine to have forgotten — the answer is right
next to it.

*It moves quickly in small chapters.* A chapter is short. It handles one idea
and moves on. Large subjects pass by several times across several chapters,
spiral-fashion, a little deeper each time. Not everything is said in one
chapter, so if you are left curious about something, that is usually
deliberate — you will meet that curiosity again a few chapters later.

Now we begin. The first step is not C but the computer itself — why C looks
the way it does only makes sense once you have seen the machine it was born
on. The next part starts by drawing the computer as a very simple picture.
