#import "../../book/lib.typ": *

= Strings

#organizer[
#idx("string")  C's string faced head on — its identity as a char array plus
  NUL termination, the special circumstances of string literals, the real cost of
  measuring length, and the practical matter of "character count ≠ byte count"
  that Hangul brings out. The last of the three nulls (the NUL character) gets
  its formal treatment here.
]

#deepqa[
  Chapter 9 said the string representation C chose is NUL termination, "planting
  a marker at the end", and foreshadowed its three prices (the length must be
  counted, a NUL cannot be held as content, lose the marker and it runs away).
  Now that you have learned arrays (chapter 36) — how would you define a C string
  exactly, yourself?
][
  *A char array, somewhere in which there is a byte of value 0 (the NUL
  character, `'\0'`), plus the agreement that "the string" means from the start
  to just before that marker* — that is all. There is no separate string type. An
  array, and an agreement. This chapter treats the use and the cost of that
  agreement.
]

== The identity — an array, and an agreement

`char greet[] = "안녕";` — initialise an array with a string literal and the
compiler builds the array by *appending `'\0'` after* the bytes of the
characters. The demonstration dissects it.

#demo("examples/ch37/str.c")

There are layers to read. `strlen` (the length measurement of the standard
`<string.h>`) answered 6 — "안녕" is *two characters, six bytes* (exactly
chapter 9's UTF-8 table: Hangul syllables live in the 3-byte range). The byte
dump `EC 95 88 EB 85 95` is the bare face of those six bytes, and `sizeof greet`
is 7 — the size of a container that also holds the NUL marker. This is the moment
chapter 8's misconception ("one character = one byte") is disproved by an
execution result.

`strlen`'s *cost* can now be stated exactly too — as chapter 9 foretold, NUL
termination does not write the length down, so strlen *walks one slot at a time
until it meets the marker*. The longer the string the longer it takes
(proportional to the slot count), which is why code calling strlen in a loop
condition every time is a classic performance trap (the demonstration's loop is
in fact that pattern — harmless for a short string, but for a long one the
practice is to take the length into a variable first).

== String literals — read-only ground

Instead of initialising an array you may hold a literal in a pointer — and here
lies an important difference:

```c
char buf[] = "you may change this";      /* copied into my array — modifiable */
const char *msg = "you must not change this";  /* points at the literal itself */
```

A string literal is itself *read-only data* baked into the program — attempting
to modify it is outside the contract, and in a modern environment it usually
collapses on the spot (literals being placed in a write-forbidden region —
chapter 6's protected zone doing another kindness). So the rule is to declare
pointers to literals as `const char*` — where chapter 23's `const` was
documentation saying "I will not change this", here it works as a lock that stops,
at compile time, the mistake of trying to change what must not be changed.

#realcase[
  gets — the function expelled from the standard
][
  Attached to the third price of NUL termination (lose the marker and it runs
  away) is the most famous funeral in the history of the C standard. `gets` in the
  early standard library was a function that "reads a line and puts it into an
  array" — and *it did not take the container's size*. If the input was longer
  than the container it overwrote the neighbouring memory as it went: a function
  with the boundary violation of chapter 36 built into its design. In 1988 the
  internet's first large-scale worm (the Morris worm) spread through defects of
  this class, and after decades of accidents the C11 standard *deleted* `gets` —
  a rare case of a standard formally burying one of its own functions. That is the
  reason this book taught `fgets` (the successor that takes the container's size)
  from the start in chapter 25 — and the next chapter faces this whole class of
  accident head on.
]

#qa[
  To handle Hangul properly — to work in units of characters — what must be done?
][
  Distinguishing two layers is the starting point. At the *byte layer* (the world
  of C strings) you may treat UTF-8 simply as a byte sequence — copying, joining,
  storing and transmitting are safe without knowing character boundaries (thanks
  to UTF-8's self-synchronising design, chapter 9). Work that needs the
  *character layer* (counting characters, cutting, case conversion) requires UTF-8
  decoding, and since the standard library's support is thin there, using a
  library is the practice — proven's u8str family takes exactly this place (first
  met in the next chapter, in earnest in chapter 53). Keep one rule in mind and
  most accidents are prevented: *do not cut a string by byte index* — cut through
  the waist of a Hangul syllable and you get a broken byte sequence.
]

We know the string's identity and cost. The next chapter is this part's turning
point — a dissection of the class of accident called the boundary violation, and
the appearance of this book's underlying technology, proven. The "safe input"
seed planted in chapter 25 is finally collected.
