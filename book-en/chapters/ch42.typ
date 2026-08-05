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

#demo("examples-en/ch42/point.c")

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

== Zeroing the whole thing — `{ 0 }` and `{ }`

The previous section passed over "members you leave out are filled with zero"
in a single clause. That clause is the foundation of an idiom used every day,
so it is worth a section of its own.

```c
struct config c = {0};   /* the old idiom */
struct config c = {};    /* C23 onwards — the empty initializer */
```

#demo("examples-en/ch42/zeroinit.c")

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

  The same goes for floating point: `{0}` promises the value 0.0, `memset`
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
  Traditionally an alias has been made with `typedef` — `typedef struct point
  point_t;` and the like. But this is a point where taste and schools divide
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
  chapter 41's dynamic memory meet in that one line and structures such as linked
  lists and trees open up — a world of data structures beyond this book's scope,
  but worth knowing that the key that opens the door is here.
]

We have a way of binding values together. The next chapter is how to *use* it —
the temporary struct made and handed over on the spot, the order-free named
arguments obtained from it, and how to deal with the empty space that gets in
between members (padding).
