#import "../../book/lib.typ": *

= Declaring variables

#prereq(
  ([chapter 5, Words and addresses], [addresses and slots]),
  ([chapter 19, The structure of a program], [where a declaration sits]),
)

#deepqa[
  Chapter 5 said memory is lockers with numbers (addresses). But no address has
  appeared even once in the C code we have written — so how has the program been
  using the lockers?
][
  Through names. To declare a *variable* in C is to "take some locker slots and
  attach a human-readable name to them." The numbers (addresses) are managed by
  the compiler and we call by name — a name is a human's nickname for an
  address. This chapter teaches how to make that name; meeting the real address
  behind the nickname again is Part VII (chapter 35).
]

#organizer[
#idx("variable")  Part V's subject is "how names are made." The first step is
  declaring a variable — agreeing a type and a name, holding a value, and
  changing it. One new operator joins: assignment `=`. And that this `=` is *not*
  the equals sign of mathematics is this chapter's most important sentence.
]

#chapter-questions()

== Declaration — type, name, and a first value

The syntax of a variable declaration is a shape we have already brushed past
several times:

```c
int apples = 12;
```

Read it in three parts — the *type* (`int`), the *name* (`apples`), and the
*initialisation* (`= 12`). The meaning: "take a slot that holds an integer, call
it apples, and put 12 in it as its first value."

The *type* promises two things. First, a *set of values* — `int` is a container
for signed integers, on common machines 32 bits, that is roughly ±2.1 billion in
chapter 7's two's-complement world. Write `double` and you get chapter 8's
64-bit floating-point container. Second, an *agreement about operations* — what
may be done to that name (integer arithmetic or approximate arithmetic) comes
from the type. Chapter 5 said "what knows about the lump is the side doing the
reading"; in C, *the type is exactly that reading eye*.

The *name* is yours to choose, within the frame the grammar fixes — made of
letters, digits and underscores, and not starting with a digit. A good name is
for people, not for the grammar: `apples` is kinder than `a` to yourself half a
year later.

*Initialisation* is not a mere practice but a rule of this book. *Always put a
first value in when you declare.* The slot of a variable declared without
initialisation holds whatever rubbish bits happened to be left there
(chapter 5 — a slot is always full of something), and reading that is a
representative path to an accident. Formal treatment is in chapter 41, but the
habit starts now.

== Assignment — the side effect that changes state

The value of a declared variable is changed by *assignment* (`=`). The operator
foreshadowed in chapter 20's allotment finally joins. The demonstration first:

#demo("examples-en/ch23/var.c")

Reading the third statement `apples = apples + 3;` exactly is the core of this
chapter. The order is: ① evaluate the right-hand expression `apples + 3` (read
the current value 12 and obtain 15), ② *put* that value into the slot of the
left-hand name (the 12 that was there is overwritten and gone). That is, `=` is
not "equals" but *an instruction: "calculate the right and put it in the
left."* The value changing — this is the *side effect* foreshadowed in
chapter 20, the second one we meet after output: assignment changes a
variable's *state*.

#misconception[
  "`x = x + 1` is a nonsensical equation — no number equals itself plus one"
][
  By the eye of mathematics, entirely right — read as an equation it has no
  solution. The root of this misconception is the reuse of the `=` symbol. C's
  `=` is not an equals sign but a *put instruction*, so `x = x + 1` is "add 1 to
  x's current value and put the result back into x" — a perfectly sensible
  command to increase x by one. The `x` on the right is *the value when read*
  (the old value) and the `x` on the left is *the place to put it* (the slot) —
  this distinction, the same name read as a value or as a location depending on
  its position, is a fundamental of reading C. Incidentally, the real comparison
  asking "are they equal?" is handled by a different symbol (`==`, chapter 30) —
  the symbols were separated precisely because of this confusion.
]

== const — the promise not to change

For a value that will never change, attach `const` to the declaration:

```c
const int max_floor = 63;    /* the value of this name will not change */
```

Assign to a name marked `const` and the compiler blocks it as an error. This is
less a lock than *documentation* — telling both the compiler and the next reader
that "this value does not vary." The more of a piece of code that does not vary,
the easier it is to read and the fewer accidents it has — so modern practice is
close to "declare it `const` first, and take that off only for what must
change." This book's examples follow that instinct.

#qa[
  In the discussion of types you said `int` is "roughly ±2.1 billion" — does
  that mean the exact size differs by machine? Where did chapter 7's 8-bit and
  16-bit stories go?
][
  An accurate observation. `int` is a type for which the standard fixes only "at
  least this much" and leaves the concrete size to the platform — in today's
  mainstream environments 32 bits is the de facto standard. Chapter 7's 8-bit and
  16-bit containers, and the types for "when you want the size pinned down
  exactly" (`int32_t` and the like), are organised in chapter 27 when the whole
  family of integer types is introduced. In this part `int` alone is enough — by
  the spiral principle, the family reunion happens when it is needed.
]

We have made a name, held a value and learned how to change it. But two lines
remain on the credit ledger carried since chapter 15 — `int main(void)` and
`return 0`. Both are the grammar of *functions*. Having given a name to a value,
in the next chapter we give a name to *work* — how to make a function yourself,
and the complete settling of the ledger.
