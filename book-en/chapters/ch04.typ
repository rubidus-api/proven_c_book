#import "../../book/lib.typ": *

= A simple model of the machine — the birth of C

#prereq(
  ([chapter 1, Setting the scene], [why C is still in use]),
  ([chapter 2, The regions of memory], [the map of memory regions]),
)

#deepqa[
  Chapter 1 said that C is still alive inside operating systems, embedded
  devices and the infrastructure of the internet. How is a language past fifty
  still in that position?
][
  Because the position C occupies is a special one. It is the lowest layer at
  which machine and human meet, and while machines have got faster over half a
  century, the skeleton of how they work is remarkably unchanged. This chapter
  looks straight at that skeleton. As long as the skeleton stays, the language
  of the skeleton stays too.
]

#organizer[
  You will be able to draw a computer as a simple picture of three parts — a
  place that calculates, a place that remembers, and something that keeps
#idx("bit")  time. You will learn what a bit, the smallest unit of information,
  is and why base two of all things. And you will see how a language called C
  was born in the days of that simple machine.
]

#chapter-questions()

== Drawing a computer as three parts

A computer is a complicated object. But the first step in understanding
anything complicated is always to draw a boldly simple picture. From here we
draw the computer as exactly three parts.

First, the *CPU*. The place that calculates. It adds, compares, and decides
what to do next.

Second, *memory*. The place that remembers. The material for the calculation
and the result of the calculation both sit here.

Third, the *clock*. The thing that keeps time. It ticks at a steady interval
like a metronome, and the CPU works one step per tick.

In this picture, what the computer does is this and nothing else: *on every
tick of the clock, the CPU fetches one thing to do from memory and does it.*
Endlessly repeated.

#qa[
  Is the gigahertz (GHz) in advertising copy — "four billion times a second" —
  this clock?
][
  It is. 4 GHz means the metronome ticks four billion times a second. The CPU
  advances one step per tick, so the faster the beat, the more steps in the
  same time. That said, the number of steps is not the whole story, as
  chapter 12 will show — doing several things in one step is the real weapon of
  a modern CPU.
]

#qa[
  How can games, video and artificial intelligence come out of nothing but
  "fetch it and do it", repeated?
][
  In the same way that a brick is simple but you can build a castle out of
  bricks. Even simple steps, taken billions of times a second and organised in
  layers — small jobs into functions, functions into programs, programs into an
  operating system — can build anything however complicated. This "stacking in
  layers" is *abstraction*, the heart of computing, and this whole book is the
  story of climbing that staircase.
]

What the CPU fetches as "the next thing to do" is called an *instruction*. An
instruction is nothing grand. "Add this number to that one", "put this value
in that slot", "if the last result was zero, jump somewhere else" — brutally
simple directions at about that level. A program is a list of such
instructions in order, and programming is the work of writing that list in a
way a human can bear.

== The bit — the smallest unit of information

Memory, we said, is the place that remembers. What does it remember, and in
what shape?

What an electronic circuit does best is distinguish two states. The voltage is
high, or it is low. The switch is on, or it is off. The smallest unit of
storage, holding one of those two, is called a *bit*. We give the two states
the names 0 and 1.

#misconception[
  "Somewhere inside the computer, 0s and 1s are written down like letters"
][
  It is a plausible thought — every book and every screen paints computers as
  full of ones and zeros. But there are no digit-shaped characters inside the
  machine. What is actually there is nothing but physical state: a high or low
  voltage, a charge present or absent. 0 and 1 are *names* we attach to those
  physical states so we can read them — an interpretation. This distinction is
  not idle pedantry. Separating "what is stored" from "how it is to be read" is
  a theme this book returns to again and again, and it is why the same string
  of bits can later be read as a number or as a character (chapters 8 and 9).
]

#qa[
  Why two states in particular? Would it not be far more efficient to put ten
  states, 0 through 9, into one switch?
][
  Decimal computers were in fact built early on. But distinguishing ten voltage
  levels means narrowing the gaps between them, and narrow gaps slip into a
  neighbouring level under noise or a change in temperature — that is, they are
  easy to get wrong. Two states give the widest possible gap, so the circuits
  are cheap, fast and robust against noise. Binary won not for mathematical
  elegance but for *engineering honesty*.
]

One bit distinguishes two things. Bundle several bits and the number of
distinguishable cases grows multiplicatively. Two bits give four (00, 01, 10,
#idx("byte")11), three give eight. A bundle of eight has a name — the *byte* — and one
byte distinguishes 256 cases.

#qa[
  Why eight, of all bundle sizes? Was it eight from the start, and everywhere?
][
  No — the size of a byte has never been fixed at 8 bits. Originally "byte"
  meant *the bundle that holds one character*, and its size differed from
  machine to machine. Early computers really did have 6-bit, 7-bit and 9-bit
  bytes, and some machines had 36-bit words that they cut into six 6-bit bytes.
  Eight became dominant when IBM's mainframes of the 1960s (System/360) adopted
  it and the industry followed. That is why communication specifications, where
  precision is everything, still say *octet* (exactly 8 bits) instead of byte —
  a trace of the history in which the word "byte" alone guaranteed no size.

  And this is not only an old story. Some signal-processing chips (DSPs) still
  have a smallest storage unit of 16 or 32 bits, so in the C running on them a
  byte is 16 or 32 bits. On machines you are likely to meet it is 8 bits
  essentially everywhere, but "a byte is always 8 bits" is custom, not promise.

  The definition in the C standard (C23) is correspondingly careful. In C23 a
  *byte* is "the smallest addressable unit of storage that can hold one
  character of the basic character set", and the number of bits in it is
  published under the name `CHAR_BIT` — pinned down only as *at least 8*, not
  exactly 8. This book, following the common machines, will proceed with byte =
  8 bits, but records here that this is the practice rather than the standard's
  guarantee. (The committee is discussing fixing it at 8 outright in a future
  standard — half a century of custom being promoted to a promise.)
]

#mathbox[
  Bundle size and number of cases
][
  The number of states a bundle of $n$ bits can distinguish is, since each
  position independently takes two values,

  $ 2 times 2 times dots.c times 2 = 2^n $

  So $2^8 = 256$, $2^16 = 65536$, $2^32 approx 4.3 times 10^9$. $2^n$ keeps
  appearing in this book — the size of storage, the range of representable
  numbers and the count of addresses are all of this form.

  Representing a *number* with a bundle works on the same principle as the
  decimal notation we use: positional notation. Just as the decimal number
  $304 = 3 dot 10^2 + 0 dot 10^1 + 4 dot 10^0$, the binary number
  $101_2 = 1 dot 2^2 + 0 dot 2^1 + 1 dot 2^0 = 5$. Each position simply carries
  a power of 2 instead of a power of 10.
]

#qa[
  How do we handle large numbers with a byte that only has 256 cases?
][
  Just as in decimal you add digits when one is not enough, bytes are joined
  together to hold larger numbers. How many it is natural to join is fixed per
  machine — and that "natural bundle" is the word, the subject of the next
  chapter.
]

== C was born in the days of that simple machine

The three-part picture drawn so far is far too simple for today's computers
(that is the story of chapters 11 and 12). But for a computer of the early
1970s the picture was *very nearly the literal truth*. The clock ticked a few
hundred thousand times a second, the CPU really did take one step per tick, and
memory really was a row of lockers.

C was born in exactly those days, on exactly that simple machine — not out of
nothing, though, but at the end of a short and distinct lineage. Knowing that
lineage explains several features of C's appearance at a stroke, so it is
summarised here.

The starting point was an ambitious British language of the 1960s called *CPL*
— so ambitious that machines of the day struggled to implement it, until
Martin Richards at Cambridge distilled its essence into a small, practical
language called *BCPL* (1967). BCPL's radical simplicity was this: *there is
only one type.* Every value is just one word-sized slot, and whether to use it
as a number or as an address is decided by the operation. It pushed "what is in
the slot is only bits, and interpretation decides" — the lesson of the next chapter —
across the entire language.

At Bell Labs, Ken Thompson, building early Unix on an ageing PDP-7, made *B*
(around 1969), BCPL trimmed further to his taste — still one type, and the
source of terse idioms that survive today (`++` among them). Then the labs
brought in a new machine, the PDP-11, and B's premise broke. The PDP-11
addressed memory in *bytes* rather than words (chapter 5's corridor is exactly
this shape), and data of different sizes — one-byte characters, two-byte
integers, and floating point — had to be handled distinctly. A language in
which "everything is one word" could not drive this machine properly.

So Dennis Ritchie brought *types* into B — distinguishing characters from
integers, creating pointers that know what they point at, and adding
structures. Those modifications accumulated between 1971 and 1973 and earned a
new name: C. In other words, C's type system was not a philosophical design but
a response to *the reality of a byte-addressed machine*. That C is
byte-oriented to the bone (the byte of chapter 4, the corridor of chapter 5),
that pointers sit at the centre of the language, and that its typing is
nonetheless on the loose side (an inheritance from a one-type ancestor) — all
are traces of that lineage.

Two footnotes. Idioms that look familiar today were not there from the start —
today's `+=`, for instance, was written `=+` in early C and only took its
current shape some years later (out of confusion: was `x=-1` a subtraction or
an assignment?). Languages are refined by eating the injuries of real use. And
in this era C had no standard and no specification document — the definition of
the language was effectively "whatever Ritchie's compiler accepts." What
commotion that looseness grew into, and how it summoned a standard (C89), is
picked up in chapter 12.

So early C was, with only slight exaggeration, *an honest nickname for the
machines of its day*. One C operation corresponded to one or two machine
instructions, and a programmer writing C could see clearly what the machine was
going to do. C's reputation as "a language close to the machine" comes from
here — and the story of how that reputation became only half true is in the
later chapters of this part (chapters 11–14).

#realcase[
  The portability revolution — rewriting Unix in C
][
  Around 1973 the heart of Unix was rewritten from assembly into C. It was
  radical at the time — the common sense being that an operating system had to
  be written at the machine-code level. The effect was overwhelming. Assembly
  Unix was tied to the PDP-11, but Unix written in C could be moved to another
  machine "as long as there is a C compiler." An operating system had stopped
  being the property of a particular machine. Today the kernels of Linux,
  Windows and macOS are all written in C (or its descendants), and the tradition
  that a new processor gets a C compiler ported to it first began here.
]

#qa[
  Is there no loss in learning, today, a language fitted to a machine of fifty
  years ago?
][
  Quite the opposite. The skeleton of the machine — memory, addresses, bytes,
  the sequential execution of instructions — has not changed in fifty years, and
  C remains the language wrapped most thinly around that skeleton. Learning C is
  therefore less about memorising one language's grammar than about learning the
  computer itself. Whatever language you use afterwards, C is almost always
  underneath it.
]

To summarise this chapter's picture: the machine repeatedly fetches an
instruction from memory on the beat of the clock and executes it, and memory is
made of bits, the two-state minimum unit. C was born when this picture was
nearly the truth, by translating this picture into a language.

The next chapter looks closely at memory, one of the three parts. We walk the
locker corridor and read the number attached to each slot — the *address* — and
measure the natural bundle the machine picks up at once — the *word* — and
inspect the two orders in which a multi-byte number can be put into the lockers
— *endianness*. C's famous notion of the pointer was born in this corridor.
