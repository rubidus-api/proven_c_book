#import "../../book/lib.typ": *

= Special knowledge about addresses — address 0, alignment, low bits

#prereq(
  ([chapter 5, Words and addresses], [an address is a number attached to a slot of memory]),
)

#deepqa[
  Chapter 5 said "an address is a number too, so it can be written down in
  another locker." But suppose you want to record that there is *no address
  yet* — how do you indicate "points nowhere"?
][
  You agree on one special value as the mark for "none." And almost every
  system chose 0 for that mark — leaving locker 0 empty, used by nobody, and
  reading "if 0 is written there, it points nowhere."
#idx("null pointer")  That agreed value is the *null pointer*. But why was locker 0 free to be
  left empty in the first place — that story is the first section of this
  chapter.
]

#organizer[
  Three pieces of corner knowledge about the locker corridor. Why locker 0 is
#idx("alignment")  special, why a large piece of luggage cannot go into just any
  number (alignment), and the trick of hiding information in the last digits of
  a number (tagged pointers). The three different things all called "null" are
  first told apart here as well.
]

#chapter-questions()

== What is in locker 0

The answer first: *it differs by machine*. And the difference is interesting.

On desktops, servers and phones, where the operating system runs in protected
mode, the OS deliberately makes the area around address 0 a *forbidden zone*.
A program that tries to read or write address 0 is caught and terminated on the
spot. This is a kindness — mistakenly using the "none" mark (null) as if it
were a real address is a very common accident, and because address 0 is
forbidden, that mistake does not slip by quietly but is caught loudly right
where it happens.

The embedded world is different. On small chips running without an operating
system, address 0 is often perfectly good, even important, memory — many
microcontrollers place the interrupt vector table (a sort of emergency contact
list) near address 0. On such a machine, reading address 0 is legal and
meaningful.

#qa[
  On a machine where address 0 is real memory, how do you tell the "none" mark
  from a genuine address 0?
][
  The distinction does blur, and embedded programmers work conscious of that
  boundary. More interesting is history's answer — machines really did exist
  that used a value other than 0 as the "none" mark. The following case is that
  story.
]

#realcase[
  Machines where the null pointer was not zero
][
  The C standard was careful from the start. That the constant 0 written in
  source code means a null pointer is the standard's promise, but *what bit
  pattern* that null pointer actually has inside the machine was left to the
  machine's freedom. And there were machines that used that freedom. The Prime
  50 series used segment 07777, offset 0 as null; the CDC Cyber 180 used the
  special pattern 0xB00000000000; Lisp-optimised Symbolics machines used a
  nonzero value, "the address of the NIL object." Look at the bits of a null
  pointer on such a machine and not one of them is 0 — and yet comparing a
  pointer against 0 in the source still comes out true, exactly as the standard
  promises. The 0 in the source is a *symbol*; the representation in the machine
  is an *implementation* — the "separation of symbol and representation" carried
  along since chapter 4 repeats here too.

  Today's mainstream machines (Intel, AMD, ARM and so on) all use "all bits
  zero" as null, so you rarely feel this distinction. But in the world of tagged
  pointers, coming up shortly, the situation of "the value meaning none is not
  all zeros" is alive and well right now.
]

#misconception[
  "A null pointer is zero"
][
  Half right. The sentence has to be read in two layers.

  #dtable(
    columns: 2,
    [*The source-code layer (spelling)*], [*The in-memory layer (representation)*],
    [You may write `p = 0;`], [Nothing promises the stored bits are all zero],
    [You may ask whether `p == 0`], [The compiler compares against that machine's own null],
    [The standard promises it], [The machine decides it],
  )

  So *the 0 you write in the source is a symbol meaning "nothing"*, and the
  compiler translates it into that machine's real null representation. That is
  why both assignment and comparison hold everywhere.

  *Filling the bits with zero yourself* is work in the other layer. Pushing a
  pointer to zero with `memset`, or trusting memory from `calloc` to be null, is
  a story that holds only on machines where "all bits zero" and "null" coincide.
  On today's mainstream machines they really do — but that is not the contract.
  Chapter 36 shows the difference in the flesh, and chapter 44 explains why the
  two ways of emptying a struct mean different things.
]

== The three nulls — three things alike only in name

There are three things called "null" around C. Telling them apart once, now,
saves great confusion later. They get formal treatment in Part VII
(chapter 36); here we merely learn their faces.

- *The null pointer* — what we just saw. The agreed pointer value meaning
#idx("NUL character")  "points nowhere." It lives in the world of pointers.
- *`NULL`* — the name (a macro) used to write a null pointer in C source code.
  The latest C (C23) also brought in a clearer spelling, `nullptr`. It too
  lives in the world of pointers.
- *The NUL character* — an entirely different object. It is a single
  *character* of value 0, at position 0 of the character table (chapter 9),
  written `'\0'` in C. It is one byte of data and is used to mark the end of a
  string (chapter 40). It lives in the world of characters.

All three have "null" in the name and all three have a 0 tangled up in them
somewhere, so they are easy to mix up, but the pointer's null and the
character's NUL live in different worlds. Remember them as "strangers who
resemble each other only because the value happens to be 0."

== Alignment — a two-slot load cannot go just anywhere

The second piece of corner knowledge about the corridor. Chapter 5 said the
machine grabs several slots in one handful (the word). But the machine's hand
is not built to take a handful at any position — usually it grabs comfortably
only *at numbers that are multiples of its own size*. A four-byte number is
grabbed in one go when it sits at an address that is a multiple of 4 (100, 104,
108, …); an eight-byte one, at a multiple of 8. This rule is *alignment*.

#memrow(100, ("78", "56", "34", "12", "  ", "  ", "  ", "  "), highlight: (0, 1, 2, 3))

Four bytes starting at 100 (a multiple of 4), as above, are aligned. Put the
same number starting at 102 and the alignment is off. What happens then also
differs by machine. A tolerant machine (the Intel line) merely *gets slower*,
splitting it into two handfuls, but does the job. A strict machine (old SPARC,
some ARM eras) raised an error (a bus fault) on the spot and stopped the
program. This is one of the classic sources of the portability problem of "code
that works only on the machine where it works."

#qa[
  Why can the machine not grab anywhere? In the locker metaphor, what gets in
  the way?
][
  Because the machine's hand (the bus) sees the corridor as a grid of four-slot
  cells and moves accordingly. 100–103 is one cell, 104–107 the next. Four
  bytes starting at 102 straddle two cells, so grabbing them at once means
  opening both cells and stitching the needed pieces together — work done twice,
  or refused outright. It is the same as a locker-room rule that two-slot
  luggage must start at an even number.
]

== The trick of the low bits — tagged pointers

The alignment rule has an unexpected by-product. Write a multiple of 4 in
binary and the last two bits are *always 0*; for a multiple of 8, the last
three bits are always 0. That is, the low bits of an aligned address are always
zero — so they can be used as *free space to carry information*.

This trick is called a *tagged pointer*. While writing down the address, you
tuck a small tag into the trailing digits that are zero anyway. It is genuinely
widespread — the runtimes of Lisp-family languages and OCaml distinguish "is
this value an integer or an object address?" by a low-bit tag, and JavaScript
engines and garbage collectors put status marks on pointers by the same trick.
It is not free, of course — the tag must be stripped off (the low bits set back
to 0) before the address is actually used.

Here we meet the earlier null story again. In a tagged world, even the special
value meaning "none" carries a tag, so *not all of its bits need be 0*. Lisp's
"none" (NIL), for instance, is the address of a real special object. Even today,
when C's null pointer is uniformly all-zero on mainstream machines, "nonzero
none" is still in active service one layer up, in the world of runtimes.

#qa[
  May a programmer play the tagged-pointer trick directly?
][
  It is technically possible in C and is a genuinely used technique, but it is
  an advanced one with many traps — you need a guarantee of alignment, you must
  strip the tag before use without fail, and it tangles with the pointer rules
  to come (provenance in chapter 14, and chapter 37). One conclusion suffices at
  this stage: the low bits of an address are not "just a number" but a special
  place that alignment created.
]

The three pieces of corner knowledge in this chapter — the specialness of
address 0, alignment, and the low bits — all come from a single fact. An
address is a number, but *not every number is treated alike*. The corridor has
a geography.

From the next chapter we turn our eyes to the contents put into the lockers.
The first rung on the ladder of representation is the integer — the story of
three competing ways to hold negative numbers in bits and C23's decision,
numbers that overflow, and shifting bits wholesale left and right.
