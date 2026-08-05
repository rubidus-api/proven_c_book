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
  On the ground that an address is *a value that can be handled like a number*
  (chapter 5) — a locker number can be written down elsewhere, copied and handed
  over. In C the type of that "variable holding an address" is the pointer. What
  we learn today is not a new concept but dressing chapter 5's idea in syntax.

  One thing should be nailed down in advance. That it can be copied and compared
  does not make *a pointer an integer.* C keeps pointers as a different kind of
  value from integers and attaches a contract to them: the type pointed at, and
  the provenance — which object it came from. Chapter 6's remark that "the same
  number need not be the same pointer" begins here, and chapter 35 finishes it
  properly under the name of provenance.
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
  In this book's verified environment (x86-64 Linux) it is *8 bytes*, and so it
  is in most of today's mainstream 64-bit environments. In that environment, too,
  every object pointer has the same size whatever it points at (`int*` or
  `double*`).

  *That, however, is not a guarantee of the standard.* The standard settles
  neither the size nor the representation of a pointer. There is no rule that
  different object pointer types must share a size and a representation
  (machines really existed on which `char*` was larger than other pointers), and
  a function pointer is not required to have the same representation as an
  object pointer either. "Word size" is not a word of the C standard but a
  matter of the machine. So code that needs the size never writes a number: it
  asks with `sizeof p`.

  Then why distinguish the types? Because when dereferencing, the type decides
  "how many bytes at that address, and through what eye, to read." Chapter 5's
  refrain (what knows the lump is the reading side) is the reason pointer types
  exist.
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

== The size of a pointer is not one number

Here is why the answer above said "8 bytes on this machine" so carefully. The
widespread belief runs: *"a pointer is just an address, so within one machine
they are all the same size."* On the x86-64 Linux we use today that is true,
which is why the belief is so rarely contradicted. But the C standard never
promised it, and real machines have broken it more than once.

What the standard actually promises is much narrower.

- *`void *` and the character-type pointers (`char *`, `signed char *`,
  `unsigned char *`) have the same representation and alignment requirements as
  each other.* Those four go together.
- Pointers to structures share a representation with one another; so do pointers
  to unions.
- *Beyond that, nothing is promised.* There is no rule that `int *` and
  `double *` must be the same size, and none that an object pointer and a
  function pointer must share a representation.

Why leave it so loose — because machines really were different.

#realcase("Machines where pointer sizes differed within one program")[
  *Word-addressed machines.* On the Cray, the Data General Nova, the Prime 50,
  Honeywell hardware and their like, an address named a *word* rather than a
  *byte*. Which byte within the word had to be carried separately, so `char *`
  was larger than `int *`, or the same size with a different bit layout. C's
  narrow rule — that only `void *` and `char *` share a representation — was
  written to accommodate that world.

  *Segmented x86 (16-bit DOS and Windows).* `near` pointers (a 16-bit offset)
  and `far`/`huge` pointers (32-bit segment:offset) lived in the same program.
  Which was which was decided by the *memory model* — in the medium model code
  pointers were far and data pointers near; in the compact model the reverse.
  That is, `sizeof(void (*)())` and `sizeof(void *)` differed inside one program.

  *Harvard-architecture microcontrollers.* On AVR, PIC and some DSPs, code and
  data live in entirely separate address spaces. Function pointers have a
  different width from data pointers, and reading a constant out of flash needs
  separate instructions and a separate kind of pointer (AVR's `__flash`, the
  `PROGMEM` idiom). Chapter 80's embedded story stands on this ground.

  *IBM AS/400 (OS/400).* Pointers were 128 bits with a tag inside them, and the
  hardware refused forged ones. Code that converts an address to an integer and
  plays with it does not work here.
]

Nor is this only in the past tense.

#realcase("Now, and ahead")[
  *CHERI and Arm Morello.* Real hardware in which a pointer is a bundle of
  address, bounds and permissions (a capability). The address is 64 bits but the
  pointer is *128*, and `sizeof(void *)` is 16. A pointer converted to an integer
  and back loses its permissions and cannot be used — the place where chapter
  35's provenance stops being a standards quibble and becomes a rule of the
  hardware.

  *Machines that use the high bits of an address.* AArch64's TBI (Top Byte
  Ignore) and MTE, and x86-64's LAM, leave the pointer's *size* alone and carry a
  tag in the top byte. A modern demonstration that the same size does not make
  "pointer = address value" true (chapter 6's tagged pointers).

  *Same CPU, different ABI.* x86-64's x32 ABI uses 64-bit instructions while
  keeping pointers at 32 bits. WebAssembly has wasm32 and wasm64 side by side.
  Even with the machine fixed, the *build target* changes the size.
]

#misconception[
  "Every machine is 64-bit these days, so `sizeof(void *)` is 8"
][
  In the environment that runs this book's examples it is indeed 8. But the
  moment that 8 is *written into the code*, that code can no longer travel to any
  of the worlds above. Two accidents are common — declaring the slot that holds a
  pointer in a struct as `long` or `int`, and computing the size of an array of
  pointers as `n * 8`. Both run quietly on x86-64 and collapse on a 32-bit build
  or on CHERI.

  The rule is simple. *Always ask `sizeof` for a size*, use `uintptr_t` if a
  pointer must be held as an integer (chapter 35), and do not put a function
  pointer into a `void *`. The C standard does not guarantee that conversion —
  POSIX demands it separately, for the sake of `dlsym`.
]

#qa[
  Then why can `void *` be used as "the container that holds any pointer"?
][
  Because the standard promised exactly that one thing. *Any object pointer
  converted to `void *` and back to its original type compares equal to the
  original.* That is why `malloc` returns a `void *` and why `qsort` takes its
  comparator through `void *`. Two cautions: the promise covers *object*
  pointers only (function pointers are left out), and a `void *` knows nothing of
  the size of what it points at, so it can be neither dereferenced nor used in
  arithmetic (some compilers allow addition, but that is an extension).

  `char *` holds a different privilege — it may look into the representation of
  any object, byte by byte. Put the two side by side: *`void *` is the passage
  that "forgets what is pointed at", and `char *` is the passage that "reads
  anything as bytes".* That the standard nails these two to the same
  representation is no accident either — on word-addressed machines these were
  precisely the pointers that had to carry the widest address. The detailed rules,
  and their price, continue in chapter 35.
]

The concept of the pointer is in place. The next chapter is this part's first
safety rule — how to handle "points nowhere." The three nulls whose faces we
learned in chapter 6 get their formal treatment, this time in syntax and
practical rules.
