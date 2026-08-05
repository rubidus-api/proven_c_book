#import "../../book/lib.typ": *

= Objects, addresses, pointers

#prereq(
  ([chapter 5, Words and addresses], [an address is a value]),
  ([chapter 32, The meaning of a function], [copying values alone cannot change the original]),
)

#organizer[
  Part VII is this book's second mountain — memory. We walk chapter 5's locker
#idx("pointer")  corridor again, this time in C's syntax. The first chapter
  faces head on the type that handles addresses as values, the *pointer* — `&`,
  `*` and `sizeof`, settling every credit carried since chapter 25.
]

#deepqa[
  Chapter 32 said "the proper method for a function to change a variable outside
  is to copy and pass the variable's *address* as a value." Unfolding that with
  chapter 5's knowledge — on what grounds can an address be "copied as a value"?
][
  On the ground that an address is just a number (chapter 5) — a locker number is
  an integer, and being an integer it can be written down in another locker and
  copied and handed over. In C the type of that "variable holding an address" is
  the pointer. What we learn today is not a new concept but dressing chapter 5's
  idea in syntax.
]

#chapter-questions()

== Three notations — declaration, &, \*

Pointer syntax is three pieces and no more.

*Declaration* — `int *p;` declares "a variable p holding the address of an int."
The knack of reading it is from the back: "p is a pointer (`*p`), and at the place
it points there is an int."

*Taking an address* — `&n` is the value "n's address." The `&` brushed past in
chapter 25's `sscanf(..., &n)` has finally got its formal name — the operator
that detaches the number tag of the locker called n.

#idx("dereference")*Dereference* — `*p` means "go to the address written in p
and *that slot*." In a reading position it reads that slot's value; on the left
of an assignment it writes to that slot. That the `*` of a declaration, the `*`
of an expression (dereference) and the `*` of multiplication use the same
character is C's notorious thrift — position is the only way to tell them apart.

The demonstration shows all three at once, including chapter 32's homework — the
proper method by which a function changes an original.

#demo("examples-en/ch33/ptr.c")

Read the route of `set_to(&n, 99)` exactly and half of pointers is done — ① `&n`
(the value that is an address) is *copied* into the parameter `target`
(chapter 32's copy-by-value rule is untouched!), and ② inside the function
`*target = value` goes to that address and writes into the original slot. The
world of copying values was not broken at all — *what was copied happened to be
an address.*

== sizeof — asking the size of a container

The last deferred credit — `sizeof` is the operator that puts out the *size in
bytes* of a type or an object (its value is of type `size_t`, an unsigned integer
just for sizes, printed with `%zu`). Chapter 25's
`fgets(line, sizeof line, stdin)` now reads completely: "tell fgets the number of
bytes in the container called line." Being an operator rather than a function,
its value is mostly settled at compile time, and it works on a type
(`sizeof(int)`) as well as on an object (`sizeof line`).

#qa[
  How large is a pointer variable itself — what is `sizeof p`?
][
  Being a container holding one address, it is exactly chapter 5's answer: the
  *word size* — 8 bytes in today's 64-bit environments. Whatever it points at
  (`int*` or `double*`) a pointer's size is the same — because what it holds is
  always one "number that is an address." Then why distinguish the types? Because
  when dereferencing, the type decides "how many bytes at that address, and
  through what eye, to read." Chapter 5's refrain (what knows the lump is the
  reading side) is the reason pointer types exist.
]

#misconception[
  "Pointers are a difficult and dangerous advanced feature"
][
  The notoriety is real but half of it is misunderstanding. The concept itself is
  as we just saw — "write down a number and go by the number" — an ordinary story
  for anyone who knows the locker corridor. The real source of the notoriety is
  not the concept but *the price of breaking the rules*: code that follows an
  empty pointer (next chapter), or invents a number in someone else's land
  (chapters 35 and 36), or keeps the number of a slot that has vanished
  (chapters 39 and 40),
  causes accidents quietly, late and largely. That is why all the remaining
  chapters of this part are chapters of "rules" — the concept ended today, and
  now we learn the safety code.
]

#qa[
  Printing a variable's address would be interesting — why did the demonstration
  not print address values?
][
  It can be done — `printf("%p", (void *)&n)` is the format. It was left out for
  honesty's sake: address values *differ on every run*. Modern operating systems
  place a program at a different position each time for security (address space
  layout randomisation, ASLR — a defence so that an attacker cannot know
  addresses in advance). Every result printed in this book is a real capture, and
  putting in a value that changes every time would make "the same result"
  impossible to guarantee. The *value* of an address does not matter; the
  *relations* between addresses (same, different, adjacent) do — which is why the
  demonstration printed `p == &n`.
]

The concept of the pointer is in place. The next chapter is this part's first
safety rule — how to handle "points nowhere." The three nulls whose faces we
learned in chapter 6 get their formal treatment, this time in syntax and
practical rules.
