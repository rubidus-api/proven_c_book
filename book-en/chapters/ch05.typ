#import "../../book/lib.typ": *

= Words and addresses — the archetype of C

#prereq(
  ([chapter 4, A simple model of the machine], [the simple machine model that points at memory by address]),
)

#deepqa[
  Chapter 4 said that a single byte distinguishes only 256 cases, and that
  larger numbers are held by joining several bytes together. So how does the
  machine know that the joined bytes are "one lump"?
][
  It does not — and that is where this chapter starts. Memory itself records
  nothing about which slot belongs with which. What knows about the lump is not
  the memory but *the side doing the reading*. The program simply decides "from
  here I shall read four slots as one number." Chapter 4's misconception — the
  separation of what is stored from how it is read — gets its first real
  workout here.
]

#organizer[
  You will be able to draw memory as a long corridor of numbered lockers. That
#idx("word")  number is the address, and how many slots the machine handles at
#idx("endianness")  once is the word. You will learn to tell apart, in
  pictures, the two orders for storing a number that spans several slots —
  little-endian and big-endian.
]

#chapter-questions()

== The locker corridor

Drawn as a picture, memory looks like this. Along a long corridor, lockers of
identical size run in an endless row, and every locker carries a number. One
#idx("memory address")locker holds 1 byte. The numbers start at 0 and go up by one. That number is
the *address*.

#memrow(100, ("01001000", "01101001", "00100001", "00000000", "11111111"))

What is inside a slot is always just eight bits. Whether those eight bits are a
number, a character, or a fragment of something larger is known neither to the
slot nor to the corridor — the interpretation of the reader decides.

There is one important fact about addresses that is not visible at first
glance. *An address can be handled as a value.* A locker number is something
that can be written down, and being writable it can be put inside another
locker. "In slot 347, write the number 512" — nothing strange about it. This
ordinary idea later becomes C's most famous concept, the *pointer* (chapter 33).

Let us separate two layers here. In the *simple model* being drawn now, an
address may be pictured as a number, an integer — the corridor and the lockers
are drawn that way. But *in the C language a pointer value is not an integer
type.* It is a value that can be copied and compared, yet of its own kind, one
that drags along what type it points at and where it came from. The reason is
seen later in this chapter ("Is this number a real address?"), and treated
properly in chapters 33 and 35. What to take away now is one line — *an address
is a value, and there is a dedicated type for holding it.*

#qa[
  How long is the corridor — how many lockers are there?
][
  The number of bits used to write an address decides. If an address is an
  $n$-bit number, at most $2^n$ lockers can be distinguished (chapter 4's $2^n$
  is back already). In the days of 32-bit addresses the corridor was at most
  about 4.3 billion slots — 4 GiB — and today's 64-bit addresses make a
  corridor longer than anyone can practically fill. This is what phrases like
  "the 4 GB memory limit on 32-bit systems" really refer to.
]

== The word — the machine's natural handful

Lockers come one slot at a time, but if the machine picked up one slot at a
time as it worked it would be far too slow. So a CPU is built to grab several
slots in *one handful*. The size of that handful — the number of bits the
machine handles at once, most naturally — is called the *word*.

This is roughly what "a 64-bit computer" points at — that the representative
width of the general-purpose registers and of the arithmetic is 64 bits, that
is, 8 bytes. You might call the word the machine's "hand size": a machine with a
big hand grabs a bigger number at once.

Take it, though, as *a loose phrase*. Inside one CPU there are registers and
paths of different widths together (floating-point and vector registers of 128,
256 or 512 bits are common), the address width may differ from the register
width, and the width the language gives to `long` or to a pointer is a further
choice again (chapter 33 shows the cases). Moreover, *some instruction sets use
the word "word" differently* — in x86 documentation a word is traditionally 16
bits, 32 bits is a doubleword and 64 bits a quadword.

So "a 64-bit machine" is better received as an idiom meaning *it mostly works at
that width* than as an exact definition. Where this book says "word", it means
loosely "the width that machine handles comfortably in one grasp".

#qa[
  So on a 64-bit machine, is everything stored in 8-byte units?
][
  No. Storage is still free at byte granularity — a one-byte character and a
  four-byte number live in the same corridor. The word is "the size the machine
  picks up most comfortably", not "the size of everything." C really does have
  several number types of different sizes (chapter 25), and the question of
  where it is convenient to place data of a given size comes back in chapter 6
  (alignment).
]

== Endianness — two orders for putting a number in several slots

Now the highlight of the chapter. A four-byte number has to go into four
lockers. Say the number splits, in hexadecimal, into the four byte-pieces
`12 34 56 78` (two hex digits are one byte). If it starts at slot 100 — which
piece goes into slot 100?

There are two schools. *Big-endian* puts the big end first: exactly the order a
human writes a number on paper.

#memrow(100, ("12", "34", "56", "78"), highlight: (0,))

*Little-endian* puts the little end first. It looks reversed, but it is an
orderly rule in its own right: "the $i$-th slot holds the $i$-th least
significant piece."

#memrow(100, ("78", "56", "34", "12"), highlight: (0,))

The computer or phone you are reading this on is almost certainly little-endian
(Intel, AMD, and most ARM deployments). It is precisely the situation of date
notation — some countries write 2026-08-04 and others 04-08-2026, neither is
wrong, but *reading each other's letters verbatim causes accidents*.

#qa[
  Big-endian looks natural to the human eye, so why did most machines choose
  the "reversed" little-endian? What is the advantage?
][
  Two practical ones.

  First, *the starting slot does not move.* In little-endian the least
  significant piece of a number is always in the slot at the starting address.
  Whether you read the number above as "all four bytes" or narrow it to "only
  the low two bytes" or "only the low byte", the slot you start reading at is
  the same 100. In big-endian you must shift the starting slot and recompute
  every time you change the width you read. For a machine that often reads the
  same value through eyes of different sizes, "the start address is invariant"
  is a considerable simplification.

  Second, *it matches the direction of the arithmetic.* Think of adding by
  hand: you add from the ones place and carry upward. A machine adding
  multi-byte numbers likewise processes from the least significant end, and in
  little-endian that lines up exactly with "in increasing address order." Early
  small CPUs could start adding as soon as the first byte arrived, and this
  advantage soaked into early designs (the ancestors of the Intel line) and has
  carried through to today.

  To be fair to big-endian: a dump reads directly to the human eye, and
  comparing bytes whole from the front agrees with the numeric ordering. So
  big-endian survives where "humans and machines look at the same place",
  such as communication protocols. Neither is superior — they simply do
  different things often.
]

#misconception[
  "Read memory from the front and you see the number in the order it was
  written"
][
  Plausible — that is how a number on paper is read. But on a little-endian
  machine the number `12 34 56 78` sits in memory in the order
  `78 56 34 12`. Look at memory in slot order (which is exactly how a debugger
  or file dump shows it) and the number appears "reversed". It is not reversed;
  the convention for the order of storing is simply different. Remember: a
  memory dump is not a number written on paper.
]

#realcase[
  NUXI — the ghost born of ordering
][
  Engineers porting early Unix to another company's computer saw something odd
  on screen. Where `UNIX` should have been printed, `NUXI` appeared. The two
  machines differed in endianness, so the order of the characters within each
  two-byte bundle came out wholly reversed. The episode became known as "the
  NUXI problem", the byword for endianness accidents. The same class of accident
  still happens — saving a file on one machine and reading it on another, or
  passing numbers over a network, breaks the numbers unless the endianness is
  agreed. Which is why internet protocols settled on *network byte order*:
  "send big-endian on the wire."
]

#qa[
  Can I check with a program that my computer is little-endian?
][
  You can — put a multi-byte number in memory and read just its first slot as a
  single byte. If the first slot holds the least significant piece, it is
  little-endian. Actually doing this check in C is the demonstration in
  chapter 43 (unions) — that is where we get the tool for reading the same
  storage through different eyes.
]

== Is this number a real address?

One thing has to be shaken loose here in advance. The picture drawn so far — one
number attached to each locker, and that number *being* the place in memory — is
*a good picture to learn from*, not an exact drawing of today's computers.

The numbers a program sees today are mostly *virtual addresses*: numbers made
separately for each program by the operating system and the translation unit
inside the CPU (the MMU). Which slot of real DRAM such a number lands on is not
something the program knows. Two programs may use the same number at the same
time and reach different physical slots. The structure of that translation is
looked at properly in chapter 11. What is needed here is one sentence — *a
number is not the substance but an agreement.*

There were times when a number was not even one number. In the 8086's segmented
scheme an address was two pieces, "segment:offset", and several notations named
the same physical slot (`0x0000:0x0010` and `0x0001:0x0000` are the same place).
Some machines put program memory and data memory in altogether separate address
spaces (the Harvard architecture — the small chips of chapter 80 still do).

And sometimes what rides inside the number is not a number at all.

#realcase("What travels inside an address value")[
  (These cases are *not for memorising.* Keep only "an address is not always a
  single number"; the names can be looked up again when you meet such a machine.)

  *The spare low bits.* Thanks to alignment (chapter 6), the low bits of an
  address are always zero. Slipping a small mark into that space is a widely used
  trick — the tagged pointers of the next chapter.

  *The spare high bits.* No machine uses all 64 bits of an address. x86-64
  really uses 48 (57 on recent parts), with the remaining high bits tied down by
  a sign-extension rule. AArch64's TBI has the top byte ignored outright, so a
  tag can be carried there (memory tagging, MTE, uses this).

  *A number and a pointer in one container.* JavaScript engines hide pointers
  inside the spare bit patterns of a floating-point NaN (NaN boxing). One 64-bit
  value is a number at one moment and an address at another.

  *Machines where an address is not a number at all.* On CHERI and Arm Morello a
  pointer is a 128-bit bundle (a capability) carrying bounds and permissions
  along with the address. The hardware checks those bounds, so a value converted
  to an integer, played with and converted back has lost its permissions and
  cannot be used.
]

This is why C did not define an address as "just an integer". It made it a
*value* that can be copied and compared, but one that drags along what type it
points at and where it came from. Why that decision was necessary becomes clear
once pointers arrive in chapter 33 and provenance in chapter 35.

#qa[
  So is the locker picture we have learned so far wrong?
][
  Not wrong — it is *the picture with one layer folded away for now*. Through the
  program's eyes memory is still a run of numbered slots, neighbouring numbers
  are neighbouring slots, and C's arrays and pointer arithmetic work exactly on
  top of that. Virtual or physical, that property is built to hold.

  The moments for unfolding what was folded come separately: when measuring
  performance and reasoning about caches (chapter 11), when asking why programs
  cannot touch one another (chapters 3 and 11), and when asking "may I turn an
  address into an integer and make what I like of it" (chapter 35).
]

== The archetype of C is here

This chapter's corridor picture is also the blueprint of the C language. In the
days when C was born (chapter 4), the people designing the language did not
hide this shape of the machine but lifted it, as it was, into the concepts of
the language. Memory is a sequence of bytes — every piece of data in C has a
size in bytes. Every slot has an address — in C you can ask for the address of
almost anything. An address is a number too — the type that holds that number
(the pointer) sits at the centre of the language. This contrasts with the many
languages that seal the "locker corridor" away from the programmer.

This honesty cuts both ways. It is the power to work while seeing right through
the machine, and it is the danger that getting lost in the corridor is an
accident on the spot. Part VII of this book faces that power and that danger
head on.

The next chapter tours the interesting special cases in the corners of this
corridor. What is in locker 0, why a two-slot piece of luggage cannot go into
just any number (alignment), and even the trick of hiding information in the
fact that the last digit of a number is always 0 (tagged pointers) — you will
see that the world of addresses is far more like an inhabited neighbourhood
than you would expect.
