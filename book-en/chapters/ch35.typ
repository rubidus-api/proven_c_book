#import "../../book/lib.typ": *

= The rules of pointers — alignment and provenance

#prereq(
  ([chapter 33, Objects, addresses, pointers], [what a pointer is]),
  ([chapter 6, Special knowledge about addresses], [alignment and the low bits]),
  ([chapter 14, And so C is an abstract language], [the contract of the abstract machine]),
)

#organizer[
  Two deep rules that bind pointers — the constraint chapter 6's alignment places
  on pointer casts (and the privilege belonging to `char*` alone), and a
  practical feel for the provenance (the tag of origin) foreshadowed in
  chapter 14. Short, but the most "contract-like" of this part's safety rules.
]

#deepqa[
  Chapter 6 taught the alignment rule that "a four-byte load goes at a multiple
  of four." But a pointer is merely a variable holding a number — why can it not
  hold any number at all?
][
  Holding it is often not blocked at all — the accident happens *when it is
  followed*. Dereferencing through an `int*` means "read four bytes at that
  address through int's eye" (chapter 33), and if that address breaks int's
  alignment (a multiple of four) then chapter 6's consequences follow — slowness,
  or on some machines a bus fault on the spot. In the standard's eye, *the cast
  to a type with stricter alignment is already outside the contract*. A pointer's
  type is both an eye and a contract.
]

#chapter-questions()

== Alignment and casts — the privilege of char\*

Pointer casts (looking through a changed type) are grammatically free but
narrow by contract. Reduced to a practical sentence — *a cast to a type
demanding stricter alignment is dangerous.* Changing a `char*` (alignment 1) into
an `int*` (alignment 4) and following it is the typical violation. The opposite
direction is safe — and here lies a privilege C intended: *a pointer to a character
type (`char*`, `signed char*` and
`unsigned char*`) may look at any object byte by byte.* With alignment 1 it can
point anywhere, and the standard explicitly permits that "the representation of
any object may be read through the eye of bytes" (chapter 13's strict aliasing
has this exception carved into it too). The official channel for the perspective
of chapter 5, "inside a slot it is only bits", is `unsigned char*`.

The alignment requirement of each type can be asked with `alignof` (a C23
keyword):

#demo("examples-en/ch35/align.c")

== Provenance — the same number, a different origin

Let us collect chapter 14's foreshadowing as practical instinct. In the naive
picture a pointer is only a number, so if the number is right it should be
followable however it was made — but the contract of modern C is not like that.
*A pointer has an origin (provenance)*: an invisible tag saying which object it
derived from. Reduced to three practical rules.

- *Pointer arithmetic stays inside the object it was born in.* Taking a pointer
  that started from one object's address and pushing it (chapter 36) onto another
#idx("array")  object — even if the number really does coincide with that
  object — is outside the contract.
- *One past the end is permitted.* Pointing at the slot "after" the end of an
  array (one-past-the-end) is legal (as long as you do not follow it). It is so
  useful in a loop's ending condition that the standard carved out this place
  specially (in action in chapter 36).
- *To treat it as a number, use the official channel.* Converting a pointer to an
  integer to store or compute with it (such as chapter 6's tagged pointers) has a
  road provided by the contract: going through the dedicated type `uintptr_t`.

#qa[
  Is it not common for code that breaks these rules to "run fine on my computer"?
][
  It is common — and that is why these rules are dangerous. What collects the
  price of a violation is not the machine but *the compiler's optimisation*
  (chapter 13): it rearranges and caches on the premise that "a pointer does not
  point outside its origin", so violating code *silently* behaves differently
  when the optimisation level or the compiler version changes. That "it ran just
  now" is not evidence of keeping the contract — that the criterion is
  chapter 14's "is it correct on the abstract machine" — is this chapter's
  conclusion, and chapter 46 shows the whole of this subject.
]

We have the rules of pointers too. Now we take these tools to contiguous memory —
the array. The credit of `line[100]`, carried since chapter 25, is settled in the
next chapter.
