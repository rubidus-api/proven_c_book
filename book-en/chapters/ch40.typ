#import "../../book/lib.typ": *

= Dynamic memory

#organizer[
  Part VII's last place — memory whose size is settled at run time and which is
#idx("dynamic allocation")  kept alive as long as you wish. `malloc` and `free`,
  the notion of ownership, and this world's three representative accidents (leak,
  double free, use after free). The map of memory is completed here.
]

#deepqa[
  Chapter 39 taught automatic lifetime (dies with the function) and static
  lifetime (lives for the whole program). Then memory that is "sized according to
  input and must live only as long as needed" — in which of the two does it go?
][
  Neither fits. We need memory whose size is settled at run time and whose
  lifetime is decided by the program's logic — so there is a third place:
#idx("heap")  *allocated storage duration*, the warehouse commonly called the
  *heap*. The rule of this place differs decisively from the other two — *the
  programmer directly orders its birth and its death.*
]

== Borrowing, and giving back

The syntax is two functions. `malloc(number of bytes)` borrows that many
contiguous slots from the warehouse and returns their starting address, and
`free(address)` gives back what was borrowed.

#demo("examples/ch40/dyn.c")

Three practices are stamped into the demonstration.

*Compute the size as `sizeof *pointer`.* `count * sizeof *scores` is "the size of
one element × the count", asking for the size through *the target* rather than a
type name — so the line needs no fixing later if the type changes.

*Borrowing can fail.* `malloc` returns a null pointer when the warehouse is short
— so *check before writing* (chapter 34's rule becomes practice here). Write
without checking and it is a null dereference and collapse.

*Give back what you borrowed.* Forget `free` and those slots stay tied up until
the program ends — a *memory leak*. It does not show in a short program, but in a
long-running server it seeps away little by little and eventually eats the
machine.

== The alignment of the address returned — because it does not know what will go in

#demo("examples/ch40/alloc_cost.c")

The question the first part of the example answers is this. You borrowed a mere
one byte with `malloc(1)` — may that address sit just anywhere?

It may not. Because `malloc` hands out the place *without knowing what will go
in*. A `double` may be placed there, or a pointer, or a large struct. So the
standard promises this — *an address returned by the `malloc` family satisfies
the alignment suitable for any basic type.* The name given to that "strictest
basic alignment" is `max_align_t`, and on this machine it is 16 bytes (chapter
6's alignment rule made flesh).

Two things follow.

*First, borrowing small does not mean the place is small.* In the example eight
one-byte borrowings gave neighbouring blocks 32 bytes apart. It is because of the
space left over to satisfy alignment and the management information (size,
status) the allocator attaches to each block. It means *a program that borrows
countless small pieces uses far more memory than it asked for*, and that is why
the arena and pool approaches we see later were born (chapter 68).

*Second, stricter alignment must be requested separately.* SIMD instructions or
hardware DMA sometimes require 64-byte or 4096-byte alignment, and for that there
is C11's `aligned_alloc(alignment, size)`. Two rules must be kept — the alignment
must be a power of two, and in C11 *the size had to be a multiple of the
alignment* (C23 lifted that restriction). What it returns is still given back
with `free`.

#platform[
  The name of aligned allocation differs by platform
][
  The standard's `aligned_alloc` came in relatively recently (C11), and before
  that each platform used a different function — POSIX's `posix_memalign`,
  Windows's `_aligned_malloc` (and its partner `_aligned_free`; on Windows this
  must not be given back with `free`). When you meet these names in old code, read
  them as "an allocation with a stated alignment."
]

== The price of two cheap-looking lines — why allocation is expensive

The latter part of the example repeats the same work three ways, 300,000 times,
and measures the time. Borrowing and giving back every time is noticeably slower
than reuse or the stack — a little over ten nanoseconds per round on this
machine. The value itself differs by machine and allocator, but the fact of a
*two-orders-of-magnitude difference* is the same everywhere.

Why is it expensive? `malloc` looks like the one line "give me slots" but is in
fact one round trip to *the warehouse management office*.

+ *Find a free piece of the right size.* The allocator manages returned pieces in
  lists by size and picks a suitable one when a request comes. Searching the
  list, cutting a large piece when needed, putting the remainder back in the list
  — all of it is data-structure manipulation.
+ *Write management information.* The size and status must be written per block
  so that `free` can later know "how many bytes this was." So allocation involves
  *writing*.
+ *Ask the operating system when short.* When the warehouse is empty it obtains
  more address space with a system call (`brk` or `mmap`). That means going into
  the kernel and back, which is far more expensive. Fortunately it does not happen
  often — the allocator takes plenty and cuts it up.
+ *With several threads, locks appear.* The warehouse ledger is a shared resource,
  so contention between threads slows it (chapter 64's story of races). That is
  why modern allocators keep a small cache per thread.
+ *The cache is cold.* A freshly obtained address is usually not in the cache, so
  the first access is slow (chapter 11's ladder). Conversely a reused buffer is
  already up in the cache — half the reason the reuse side is fast in the example
  is here.

So a practical rule follows. *Do not allocate inside a hot loop.* Borrow once in
advance and reuse, put what has a known size on the stack, or borrow many at once
and cut them up. The last is the arena, and chapter 68 and Part XII are that
story.

#misconception[
  "`free` gives memory back to the operating system"
][
  Mostly it does not. `free` is *writing in the allocator's ledger that "this
  piece may be used again"*, not returning it to the operating system. So it is
  normal for a program's memory usage in the task manager to stay the same after
  releasing a large piece of data — the allocator is holding it for the next
  request (large blocks are sometimes returned).

  This fact explains two things. First, the common misunderstanding of "I freed
  the memory, so why does it not go down?" Second, the phenomenon of a
  long-running server holding memory *even with no leak* — pieces scattered so
  that a large lump cannot be formed: *fragmentation*. Chapter 68 faces it head
  on.
]

== Ownership — who is responsible for giving it back

The address `malloc` gave can be copied into several variables and can travel
between functions. And yet `free` must be called *exactly once* — from which
comes a core discipline of C programming: fixing one subject that at any moment
holds the responsibility for releasing that memory, the notion of *ownership*. C
has no syntax that enforces ownership — so ownership is expressed *in comments,
names and conventions* ("this function transfers ownership of the returned
pointer", and so on). That modern languages lifted ownership into the type system
(Rust's ownership, C++'s smart pointers) is the result of making the machine
enforce this discipline — the concern of the neighbouring languages seen in
chapter 1 arose exactly here.

#realcase[
  The three representative accidents — leak, double free, use after free
][
  Accidents with dynamic memory come with three faces. A *leak* is forgetting to
  give back — a slowly fatal disease. A *double free* is calling `free` twice on
  the same address, which wrecks the warehouse ledger so that every allocation
  after it is contaminated. The most dangerous is *use after free* — continuing to
  use the address of a slot that was given back. It is chapter 39's dangling
  pointer reproduced in the heap, and since the warehouse soon hands that slot to
  another request, *an attacker can put their own data in that place*. In the
  lists of severe vulnerabilities of browsers and kernels, use-after-free is a top
  fixture even today, which is why systems programming as a whole has moved in the
  direction of "let the language enforce ownership." The defence in C is
  discipline plus tools — the practice of assigning `nullptr` to a pointer right
  after `free`, and chapter 17's ASan (catching all three of these accidents is
  ASan's speciality).
]

#qa[
  Is it then best to avoid dynamic memory as far as possible?
][
  That really is the first strategy, and the reason this book has come this far
  without `malloc` — data of known size is fastest and safest kept in automatic
  variables (the stack), where there is nothing to release and the three accidents
  are sealed off at the source. In embedded and safety-critical fields, conventions
  banning dynamic allocation outright are common. But data whose size is settled at
  run time (a list the user gave, the contents of a file) needs the warehouse in
  the end — and then the practical answer is to make ownership clear, check with
  tools, and use well-made components (of the family that manages allocation and
  boundaries together, like chapter 38's proven).
]

== Closing Part VII

The map of memory is complete — on the ladder of registers and caches
(chapter 11) sit C's three storage durations (automatic, static, dynamic), and
pointers travel over them. We got the concept in chapter 33, the rules in
chapters 34–35, contiguous memory and strings in chapters 36–37, a safe component
in chapter 38, and lifetime and the warehouse in chapters 39–40. We have gone
once round the place where C's power and its danger live together.

We have seen dynamic memory's syntax, discipline and price. How an allocator
actually manages the warehouse, what alternative allocators and alternative
standard libraries are widely used today, and what map a program's memory is laid
out on in an operating system and in an embedded chip are treated in two chapters
at the end of Part XI (chapters 67 and 68).

The next part is short but long deferred — the structs and unions put off in
Part V with "declarations that make types come after we have a memory model."
That condition is now met.
