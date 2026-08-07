#import "../../book/lib.typ": *

= Structs

#prereq(
  ([chapter 23, Declaring variables], [declaring a variable]),
  ([chapter 37, Arrays], [several values under one name]),
)

#deepqa[
  Chapter 23 said "a type is a set of values plus an agreement about operations",
  and every type used so far has been one that already existed (int, double, char,
  pointers). Then what does it mean for a programmer to *make a new type*?
][
  It is settling a new shape of memory and giving it a name. Declare "I shall call
  a lump of two integers side by side a point", and from that moment point is a
  fully-fledged type from which variables can be made, which can be passed to
  functions and laid out as an array. If chapter 37's array was *a repetition of
  the same type*, a struct is *a bundle of different types* — and the moment that
  bundle gets a name, the program's vocabulary grows.
]

#organizer[
  We keep the promise put off in Part V with "declarations that make types come
#idx("struct")  after we have a memory model." The type that binds several
  values into one — the struct. Declaration and initialisation, the two access
  notations (`.` and `->`), and how a struct travels as a value.
]

#chapter-questions()

== Declaration, initialisation, access

#demo("examples-en/ch43/point.c")

*Declaration* is `struct point { int x; int y; };` — each item inside the braces
is called a *member*. The declaration itself takes no memory. It is only a
definition saying "a type of this shape exists"; a variable appears when you
write `struct point a;`.

For *initialisation* we recommend, as in the demonstration, writing the member
names — the *designated initializer* (C99): `{ .x = 3, .y = 4 }`. It reads better
than the order-dependent `{3, 4}` and stays safe if members are added or
reordered. Members not written are filled with 0.

*Access* has two notations — a dot for a value (`a.x`), an arrow for a pointer
(`p->x`). The arrow is in fact an abbreviation of `(*p).x` (chapter 34's
dereference plus dot). Handling structs through pointers is overwhelmingly common,
which is why it got its own notation.

*Compound literal* — the demonstration's
`(struct point){ .x = ..., .y = ... }` is the notation for "making one unnamed
struct value on the spot" (C99). It is useful for handing a struct over
immediately as a return value or an argument.

== The first surprise of `sizeof` — not the sum of the members

Make a struct, ask for its size, and the guess is usually wrong.

#demo("examples-en/ch43/sizeof_first.c")

`char` + `int` + `char` looks like six bytes; it is twelve. The extra six are
*padding* — empty space between the members and at the end.

#figure-svg("padding", caption: [The hatched cells are padding. Each member sits at a multiple of its alignment, and space is added at the end too.])

The reason is chapter 6's alignment. An `int` must sit at an address that is a
multiple of four, so three bytes go empty after the first `char`, and three more
after the last one — because when this struct is laid out as an array, *the next
element's `int`* must be aligned too.

There are only three rules.

+ Each member sits at an *offset that is a multiple of its own alignment*.
+ The struct's alignment is the *maximum of its members' alignments*.
+ The struct's size is *rounded up* to a multiple of that alignment (tail padding).

So *changing only the order can shrink it.* The demonstration's `tight` puts the
big one first and turns twelve bytes into eight. With a million elements that is
11 MiB against 7 MiB — and not only memory: the number of elements that fit in
cache changes with it (chapter 11).

The tool for seeing the layout is `offsetof` from `<stddef.h>`; the demonstration
uses it to print where each member starts. "If it differs from what you thought,
ask" is the knack here.

#qa[
  Should the biggest member always come first, then?
][
  It makes a fine default but a poor rule. *A readable order* often matters more
  (keeping related members together), and where only one struct is ever made, a
  few bytes are nothing.

  The places to think about order are clear — *when very many of the same struct
  are laid out* (arrays, pools, nodes), and *where memory is tight* (embedded).
  Elsewhere it is enough to print the size once and not be surprised.

  The devices for removing padding (`#pragma pack`, `packed`) and for raising
  alignment (`alignas`) are in chapter 44. What to know first is that *they are
  either non-standard or have a price.*
]

== Zeroing the whole thing — `{ 0 }` and `{ }`

The previous section passed over "members you leave out are filled with zero"
in a single clause. That clause is the foundation of an idiom used every day,
so it is worth a section of its own.

```c
struct config c = {0};   /* the old idiom */
struct config c = {};    /* C23 onwards — the empty initializer */
```

#demo("examples-en/ch43/zeroinit.c")

=== What is actually guaranteed

The standard (C23 §6.7.11) gives this a name: *default initialization*.
Anything not initialised explicitly is filled in as follows.

#dtable(
  columns: 2,
  [*Type of the member*], [*What it is filled with*],
  [Pointer], [*A null pointer*],
  [Arithmetic type (integer, floating)], [(positive or unsigned) zero],
  [Decimal floating type], [Positive zero; the quantum exponent is implementation-defined],
  [Aggregate (struct, array, union)], [The same rules again, *recursively*],
)

That answers this section's central question: *pointer members are
initialised to null* — recursively, including pointers inside nested structs.
In the demonstration both `path` and `in.note` come out null.

#qa[
  Is "filled with zero" not the same thing as "made null" for a pointer?
][
  On the overwhelming majority of implementations the result is the same, but
  *the promise is a different promise.*

  What the standard guarantees is "becomes a null pointer value", not
  "becomes all-bits-zero" (chapter 35, on what null really is). Implementations
  where the representation of null is not all-bits-zero have existed, and the
  standard still leaves room for them. So `{0}` and `{}` give you null
  everywhere, while `memset(&c, 0, sizeof c)` only ever gives you all-bits-zero.
  On an implementation where those two promises come apart, the latter is not
  null.

  Chapter 35's demonstration empties this very struct both ways and prints the
  bytes side by side — on this machine the results agree, and the promises do
  not. The same goes for floating point: `{0}` promises the value 0.0, `memset`
  promises a bit pattern. The working rule is simple — *use an initializer to
  empty a struct, and keep `memset` for other purposes* (such as the padding
  question below).
]

=== The fine difference between `{0}` and `{}` — padding

They are nearly the same, and they part company in one place. For an
aggregate subject to default initialization, C23 states that *any padding is
initialized to zero bits*. With `{}` the *whole object* is subject to default
initialization, so the gaps between members are zero too. With `{0}` the
first member is initialised explicitly, so what gets default initialization
is *the remaining members* — the struct's own padding bytes are not covered,
and their values are unspecified.

In the demonstration all 48 bytes come out zero, but that is this
implementation's behaviour, not a promise.

The distinction is usually irrelevant, and then suddenly matters when you
compare whole structs with `memcmp` or write them out byte-wise to a file or
a socket. The rule for those cases:

- You only need the values to be right → `{0}` or `{}`.
- The padding must be zero too (comparison, serialisation) → `{}` in C23;
  otherwise `memset` first and then assign the members you need.

#misconception[
  "`{0}` only zeroes the first member"
][
  It does not. `{0}` spells out one member, but *everything left out is
  default-initialised* (§6.7.11). A struct with a hundred members is fully
  zeroed and nulled by that one `{0}`.

  The inverted misconception is just as common: "if I write only
  `{ .retries = 3 }`, the rest is garbage." Also false. Designated or
  positional, *if there is any initializer at all*, the members you leave out
  are default-initialised — the third line of the demonstration is the check.
  Garbage is what you get when there is no initializer whatsoever
  (`struct config c;`).
]

#platform("Can you use `{}`?")[
  The empty initializer `{}` became standard in C23. GCC and Clang accepted
  it as an extension before that, but such code was not portable. If you must
  also support C17 and earlier, use `{0}` — bearing in mind that when the
  first member is itself a struct or an array, some compilers warn and you
  end up writing `{ {0} }`. Not having that annoyance is another point in
  favour of `{}`.
]

== A struct is a value

In C a struct is treated *like a value* — assign it and it is copied whole, pass
it to a function and it crosses over copied, exactly by chapter 32's rule, and it
can be returned whole with `return`. The demonstration's `moved(a, 10, -1)` is
the check: a is unchanged and a new value b came out.

This copying is a *shallow copy* that transcribes the members as they are. If all
the members are numbers there is no problem, but if a member is a pointer the
address is duplicated as it is, so original and copy point at the same place —
this fact becomes a decisive trap later when handling data that points at itself.

In practice, though, rather than passing large structs by value it is common to
*pass a pointer* — to save the cost of copying (recall chapter 11's ladder of
memory and it is clear that copying a large lump is not free). When only reading,
the practice is to receive it as a const pointer, as in
`const struct point *p` — chapter 23's `const` working as a contract mark saying
"this function does not touch the original."

#qa[
  Writing `struct point` with `struct` every time is a nuisance — can it not be
  shortened?
][
  Traditionally an alias has been made with `typedef` — `typedef struct point point_t;` and the like. But this is a point where taste and schools divide
  (there is the counter-argument that an alias hides the information "this is a
  struct"), so this book writes `struct` so the identity is visible on the page.
  Either way, consistency is what matters.
]

#qa[
  Can a struct hold itself as a member — it seems necessary for making something
  like a list.
][
  It cannot hold itself *by value* (the size would be infinite). But it can hold
  *a pointer to itself*, and that is precisely the seed of linked data structures:
  `struct node { int value; struct node *next; };`. Let chapter 34's pointers and
  chapter 42's dynamic memory meet in that one line and structures such as linked
  lists and trees open up — a world of data structures beyond this book's scope,
  but worth knowing that the key that opens the door is here.
]

=== Why assignment works but comparison does not

A struct is a value, so `b = a;` copies the whole thing in one line. Yet `a == b`
does not exist — it is a compile error. Why does assignment work and comparison
not?

*Because of padding.* Assignment can be defined as "move the members' values",
but comparison has to answer "are they equal", and *the value of the padding is
not specified.* Two structs holding the same values may hold different rubbish in
their padding, and comparing bit by bit then says "different".

#misconception[
  "Then compare them with `memcmp`"
][
  The commonest substitute, and quietly wrong. `memcmp` compares
  *representations* — it looks at the padding as well as the members.

  Chapter 44's demonstration shows this in the flesh: two structs whose members
  are all equal, and `memcmp` reports "different". The opposite accident exists
  too — if the padding happens to match, it says "equal", but that is luck, not a
  contract.

  For the same reason *a struct must not be hashed whole* (equal values give
  different hashes) and *must not be written whole to a file or a socket*
  (chapter 44 goes into it).

  There is one prescription — *write a function that compares member by member.*

  ```c
  bool point_eq(struct point a, struct point b)
  { return a.x == b.x && a.y == b.y; }
  ```
]

== Header and data in one block — the flexible array member

A struct followed by data of no fixed length is a very common shape — messages,
packets, nodes holding a string. C99 made the pattern official.

#idx("flexible array member")A *flexible array member* is the *last* member of a struct: an array with its
size left empty.

#demo("examples-en/ch43/flexible.c")

Three things are the contract.

- *It must be last, and at least one other member must precede it.*
- *It is not included in `sizeof`.* That `sizeof(struct msg)` and
  `offsetof(struct msg, data)` printed the same value says exactly that.
- *The size is decided when allocating.* `malloc(offsetof(…, data) + length)` is
  the standard form.

There is a reason for using `offsetof` rather than `sizeof`: `sizeof` includes the
tail padding, so it is counted twice — not wrong, merely a little more than
needed. And the length arithmetic *must be checked for overflow* (chapter 72) — a
large length that wraps around means writing large data into a small vessel, which
is precisely a heap overflow.

#realcase[
  From "the struct hack" to official syntax
][
  Before C99, people who wanted this wrote the last member as `char data[1]` and
  balanced the arithmetic when allocating, as in
  `malloc(sizeof(struct msg) + len - 1)`. This practice was known as *the struct
  hack*.

  It worked, but it was *outside the contract* — it touched the second element of
  an array with only one. A compiler optimising on that fact could break it.

  C99 removed the grey area by making `char data[]` official. Read `[1]` in old
  code as a trace of that era, and write `[]` in new code.
]

We have both a way of binding values together and the shape those values take in
memory. The next chapter is how to *use* them — the temporary struct made and
handed over on the spot, order-free named arguments, the devices for dealing with
padding, and *why a struct must not be stored or sent whole*.
