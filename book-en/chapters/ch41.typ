#import "../../book/lib.typ": *

= Structs

#prereq(
  ([chapter 23, Declaring variables], [declaring a variable]),
  ([chapter 36, Arrays], [several values under one name]),
)

#deepqa[
  Chapter 23 said "a type is a set of values plus an agreement about operations",
  and every type used so far has been one that already existed (int, double, char,
  pointers). Then what does it mean for a programmer to *make a new type*?
][
  It is settling a new shape of memory and giving it a name. Declare "I shall call
  a lump of two integers side by side a point", and from that moment point is a
  fully-fledged type from which variables can be made, which can be passed to
  functions and laid out as an array. If chapter 36's array was *a repetition of
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

#demo("examples-en/ch41/point.c")

*Declaration* is `struct point { int x; int y; };` — each item inside the braces
is called a *member*. The declaration itself takes no memory. It is only a
definition saying "a type of this shape exists"; a variable appears when you
write `struct point a;`.

For *initialisation* we recommend, as in the demonstration, writing the member
names — the *designated initializer* (C99): `{ .x = 3, .y = 4 }`. It reads better
than the order-dependent `{3, 4}` and stays safe if members are added or
reordered. Members not written are filled with 0.

*Access* has two notations — a dot for a value (`a.x`), an arrow for a pointer
(`p->x`). The arrow is in fact an abbreviation of `(*p).x` (chapter 33's
dereference plus dot). Handling structs through pointers is overwhelmingly common,
which is why it got its own notation.

*Compound literal* — the demonstration's
`(struct point){ .x = ..., .y = ... }` is the notation for "making one unnamed
struct value on the spot" (C99). It is useful for handing a struct over
immediately as a return value or an argument.

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
  `struct node { int value; struct node *next; };`. Let chapter 33's pointers and
  chapter 40's dynamic memory meet in that one line and structures such as linked
  lists and trees open up — a world of data structures beyond this book's scope,
  but worth knowing that the key that opens the door is here.
]

We have a way of binding values together. The next chapter is how to *use* it —
the temporary struct made and handed over on the spot, the order-free named
arguments obtained from it, and how to deal with the empty space that gets in
between members (padding).
