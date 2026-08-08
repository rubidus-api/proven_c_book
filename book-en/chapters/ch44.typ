#import "../../book/lib.typ": *

= Dynamic memory

#prereq(
  ([chapter 43, Lifetime and storage duration], [the limits of automatic lifetime]),
  ([chapter 2, The regions of memory], [the warehouse (the heap)]),
)

#deepqa[
  Chapter 43 taught automatic lifetime (dies with the function) and static
  lifetime (lives for the whole program). Then memory that is "sized according to
  input and must live only as long as needed" — in which of the two does it go?
][
  Neither fits. We need memory whose size is settled at run time and whose
  lifetime is decided by the program's logic — so there is a third place:
#idx("allocated")#idx("heap")  *allocated storage duration*, the warehouse commonly called the
  *heap*. The rule of this place differs decisively from the other two — *the
  programmer directly orders its birth and its death.*
]

#organizer[
  Part VII's last place — memory whose size is settled at run time and which is
#idx("dynamic allocation")  kept alive as long as you wish. `malloc` and `free`,
  the notion of ownership, and this world's three representative accidents (leak,
  double free, use after free). The map of memory is completed here.
]

#chapter-questions()

== Borrowing, and giving back

The syntax is two functions. `malloc(number of bytes)` borrows that many
contiguous slots from the warehouse and returns their starting address, and
`free(address)` gives back what was borrowed.

#demo("examples-en/ch44/dyn.c")

Three practices are stamped into the demonstration.

*Compute the size as `sizeof *pointer`.* `count * sizeof *scores` is "the size of
one element × the count", asking for the size through *the target* rather than a
type name — so the line needs no fixing later if the type changes.

*Borrowing can fail.* `malloc` returns a null pointer when the warehouse is short
— so *check before writing* (chapter 36's rule becomes practice here). Write
without checking and it is a null dereference and collapse.

*Give back what you borrowed.* Forget `free` and those slots stay tied up until
the program ends — a *memory leak*. It does not show in a short program, but in a
long-running server it seeps away little by little and eventually eats the
machine.

#misconception[
  "Memory from `calloc` comes with its pointers initialised to null"
][
  What `calloc` promises is one thing: *every bit is set to zero.* That is not
  the same as "a null pointer" or "0.0" — chapter 36's distinction between
  spelling and representation catches you here too.

  #dtable(
    columns: 2,
    [*What `calloc` promises*], [*What it does not*],
    [Every bit of the memory is zero], [That those bits mean a null pointer],
    [The same bits as an integer 0], [That they mean the floating value 0.0 (with IEEE 754 they happen to)],
  )

  On today's mainstream machines all three coincide, so no accident follows.
  Still, *code that allocates an array of pointers with `calloc` and says "they
  are all null, so a check is enough" is standing on that coincidence* — better
  to know it. Where portability matters, fill them with `nullptr` in a loop
  after allocating, or mark "empty" by something other than zero in the first
  place.
]

== How to write a `malloc` call — an old argument

There are several ways to write the same allocation, and the C world has argued for
thirty years about which is right. Find a piece on this subject online and it is
usually heated, because both sides have grounds. Let us measure and decide.

#dtable(
  columns: 3,
  [*Form*], [*Shape*], [*Who recommends it*],
  [1. take the size from the target], [`p = malloc(n * sizeof *p);`], [the practice since K&R, the Linux kernel, the C FAQ],
  [2. name the type and cast], [`p = (T *)malloc(n * sizeof(T));`], [CERT MEM02-C; codebases that value C++ compatibility],
  [3. the compromise], [`p = (T *)malloc(n * sizeof *p);`], [cast where a cast is needed, without letting the size drift],
)

#demo("examples-en/ch44/malloc_style.c")

=== Four reasons for form 1

*First, when the type changes there is only one place to fix.* This is the biggest
reason. The Linux kernel's coding style puts it this way --- spelling the type name
out "*hurts readability and introduces an opportunity for a bug when the pointer
variable type is changed but the corresponding sizeof that is passed to a memory
allocator is not*".

Think of the common act of changing an `int *` into a `struct node *`. Form 1 needs
only the declaration changed and the size follows; form 2 needs *two* places changed,
and forgetting one compiles cleanly with too small a block.

*Second, the type name never appears, so it cannot drift.* This goes one step beyond
"only one place to fix" --- in form 1 *there is no way to write it wrongly.*

*Third, `sizeof *p` does not read `p`.* At first sight it looks like dereferencing a
pointer that has not been allocated yet. It is not: the operand of `sizeof` is *not
evaluated* (chapter 48). The listing confirms it --- `sizeof *p` computes fine with
`p` null, because the size comes from the *type*, not the value.

*Fourth, the cast used to hide a bug.* Before the standard and in the C89 era,
calling `malloc` without including `<stdlib.h>` made the compiler assume "a function
returning `int`". Putting that `int` into a pointer draws a diagnostic --- and *the
cast wipes that diagnostic away.* Reproduced by measurement:

#dtable(
  columns: 2,
  [*Called without a declaration (C89, builtins off)*], [*What the compiler says*],
  [`p = (int *)f(4);`], [`cast to pointer from integer of different size` --- blamed on the cast],
  [`p = f(4);`], [`initialization of 'int *' from 'int' makes pointer from integer without a cast` --- *exactly* right],
)

Honesty requires adding that this fourth reason has *nearly lost its force today*.
C99 removed implicit function declarations, and in C23 it is an outright error ---
measured, `error: implicit declaration of function 'malloc'` appears with or without
the cast. "The cast hides a bug" is now *a historical explanation*, not a present
reason. When a piece still wields it as the clincher, check which decade it is
describing.

=== The case for form 2 --- CERT MEM02-C

The other side has standards behind it too. The SEI CERT C Coding Standard's
*MEM02-C* is titled "*Immediately cast the result of a memory allocation function
call into a pointer to the allocated type*", and its rationale is this --- "casting
the result of `malloc()` to the appropriate pointer type *enables the compiler to
catch subsequent inadvertent pointer conversions*".

The claim holds. Compile CERT's own example and the difference is plain.

#dtable(
  columns: 2,
  [*Code*], [*What GCC 14 emits*],
  [`widget *p = malloc(sizeof(gadget));`], [*a warning* --- `allocation of insufficient size '1' for type 'struct widget' with size '40'`],
  [`widget *p = (gadget *)malloc(sizeof(gadget));`], [*an error* --- `assignment to 'struct widget *' from incompatible pointer type`],
  [`widget *p = malloc(sizeof *p);`], [nothing --- *that bug cannot be written*],
)

That the cast promotes a warning into an error is true. But the third row shows the
heart of the argument --- **form 1 needs no diagnostic, because there is no mistake
to diagnose.** And CERT itself rates this recommendation *severity low, likelihood
unlikely, priority P3*.

=== Form 3 and the "C++ compatibility" reason --- is it real?

The most frequently heard reason for the cast is "because it must build with a C++
compiler too". C++ does not allow an implicit conversion from `void *` to another
pointer type, so without a cast it genuinely fails to compile --- measured,
`error: invalid conversion from 'void*' to 'int*'`.

Three things deserve weighing before accepting that reason.

*First, C++ does not recommend `malloc` in the first place.* The way to obtain memory
in C++ is `new`, and more properly containers and smart pointers. So "a `malloc` call
that also builds as C++" means *keeping code C++ does not recommend conformant to C++
syntax*. It is worth asking what that buys.

*Second, the compatibility is not actually obtained.* This is decisive. A modern C
file with the cast dutifully applied was handed to a C++ compiler:

#dtable(
  columns: 2,
  [*What the file contains*], [*What the C++ compiler says*],
  [`(struct pt *)malloc(n * sizeof *p)`], [passes --- thanks to the cast],
  [`struct pt a = { .z = 3, .x = 1 };`], [*error* --- `designator order for field 'pt::x' does not match declaration`],
  [`_Generic(n, int: 1, default: 0)`], [*error* --- `'_Generic' was not declared in this scope`],
  [`int vla[n];`], [not in standard C++ (it passes only as a GNU extension)],
)

That is, **the cast does not get the file into C++.** The ordering constraint on
designated initialisers, `_Generic`, variable length arrays, `restrict`, compound
literals, flexible array members --- the two languages have diverged steadily since
1989, and C23 and C++23 are further apart than ever (chapter 94). *What one cast buys
is not compatibility but the illusion of it.*

*Third, what is really shared is the header, not the `.c` file.* The realistic place
where C and C++ meet is a header (`extern "C"`, chapter 55) --- and **headers do not
call `malloc`.** The calls live in implementation files, and those are compiled by a
C compiler.

#qa[
  Is there no place, then, where the cast is right?
][
  There is --- but where there is *an actual circumstance*, not "just in case".

  - *Projects that really do build `.c` files as C++.* Rare, but they exist ---
    some embedded toolchains, old MSVC practice, builds that pull C code wholesale
    into a C++ translation unit. There it is a requirement, not a choice.
  - *Under a convention that follows CERT.* As chapter 94 showed, where a rulebook
    governs, the rulebook wins.

  In such a place, prefer form 3, `(T *)malloc(n * sizeof *p)` --- satisfy C++ with
  the cast while keeping form 1's safety by taking the size from the target. Measured,
  this form passed in both C and C++. *If one of the two must go, let it be writing
  the type name twice.*
]

#antipattern[
  The real danger both forms share --- overflow in the multiplication
][
  The cast argument tends to hide something. *The multiplication in `n * sizeof *p`
  can overflow.*

  ```c
  p = malloc(n * sizeof *p);   /* with a large n the product wraps */
  ```

  As the listing showed, when the wrapped result is a tiny number **`malloc`
  succeeds** --- and writing `n` elements into that tiny block brings the heap down.
  The hole is equally open with or without the cast, and it has become a real
  vulnerability more than once.

  Three ways to close it.

  - *Check the count first* --- `if (n > SIZE_MAX / sizeof *p) return NULL;`
  - *Use `calloc(n, sizeof *p)`* --- the standard requires it to check the
    multiplication. Measured, `calloc` refused the overflowing request (zeroing is a
    bonus and a cost).
  - *Use checked arithmetic* --- C23's `<stdckdint.h>` (chapter 79).

  This is a far more valuable argument than whether to write a cast.
]

#qa[
  So which does this book use?
][
  *Form 1.* `p = malloc(n * sizeof *p);`

  The reason in one line --- **a form that cannot be written wrongly beats a form that
  catches you when you write it wrongly.** A cast is a device that turns a mismatch
  into a diagnostic; form 1 removes the mismatch. In the language of chapter 12's
  ladder, *better than rung 3 (let the build tell you) is a design where the state
  never arises.*

  This is not offered as a rule to enforce. In a codebase that must build `.c` as C++,
  or under a convention that follows CERT, forms 2 and 3 are the right choice.
  *Choosing knowingly differs from inheriting a habit* --- and the commonest mistake
  in this argument is not which form you pick but attaching the cast without knowing
  why, because that is how you were taught.
]

== The alignment of the address returned — because it does not know what will go in

#demo("examples-en/ch44/alloc_cost.c")

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
the arena and pool approaches we see later were born (chapter 82).

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
  so contention between threads slows it (chapter 78's story of races). That is
  why modern allocators keep a small cache per thread.
+ *The cache is cold.* A freshly obtained address is usually not in the cache, so
  the first access is slow (chapter 11's ladder). Conversely a reused buffer is
  already up in the cache — half the reason the reuse side is fast in the example
  is here.

So a practical rule follows. *Do not allocate inside a hot loop.* Borrow once in
advance and reuse, put what has a known size on the stack, or borrow many at once
and cut them up. The last is the arena, and chapter 82 and Part XII are that
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
  that a large lump cannot be formed: *fragmentation*. Chapter 82 faces it head
  on.
]

== Borrowing in one dimension, using it as two

Chapter 38 showed real multidimensional arrays such as `int m[3][4]`. But when
the size is settled at run time that syntax cannot be used, and so the commonest
shape in practice is *to borrow one run and read it along two axes*.

The heart of it is one line of arithmetic. Borrow `rows × cols` slots at once and
find slot `(i, j)` as `i * cols + j`. That is exactly the layout of a real
two-dimensional array (row-major, chapter 38) — so the performance is the same
and the cache behaves the same way (chapter 11).

#demo("examples-en/ch44/flat2d.c")

Three things need care here.

*First, do not scatter the subscript arithmetic by hand.* Write
`p[i * cols + j]` all over the code and the moment one place forgets `cols` it
quietly reads a different slot. Shut it inside one macro, as the example does,
and there is one place to fix. When writing the macro, keep chapter 56's rules —
*wrap every argument in parentheses*, and raise the product to `size_t` to avoid
overflow.

```c
#define AT(p, cols, i, j)  ((p)[(size_t)(i) * (size_t)(cols) + (size_t)(j)])
```

*Second, the size computation itself can overflow.* `rows * cols * sizeof(int)`
is a product of three numbers, easy to overflow, and an overflow means *borrowing
a small vessel and using it as a large array* — the worst kind of accident. The
example checks with `ckd_mul` (chapters 51 and 79) first and does not even attempt
the allocation if it overflows.

*Third, nail down the order of rows and columns in the documentation.*
`AT(g, cols, 1, 2)` and `AT(g, cols, 2, 1)` are different slots. Half the mistakes
come from here, so name the parameters `rows` and `cols` plainly and write the
order down.

#qa[
  Could an array of pointers (`int **`) not be used, keeping the `m[i][j]` syntax?
][
  It can, and it is a common method — allocate each row separately and hold their
  addresses in an array. The price is high, though.

  *The memory is scattered.* With rows far apart, chapter 11's locality breaks and
  the cache hit rate falls. *There are many allocations.* One `malloc` per row,
  that many failure paths, and freeing must run in reverse just as many times.
  *There is one more indirection.* `m[i][j]` follows an address twice.

  And decisively, *`int[3][4]` and `int **` are different types.* Pass the name of
  a real two-dimensional array to a function taking `int **` and it is a compile
  error; force it through with a cast and it is outside the contract — because,
  as chapter 38 showed, `int m[3][4]` decays to `int (*)[4]`, not to `int **`.
  This misunderstanding is the most frequent accident with multidimensional
  arrays.

  In short — *a real two-dimensional array when the size is fixed; a
  one-dimensional allocation plus a subscript macro (or a VLA parameter,
  chapter 38) when it is settled at run time*; and `int **` when the rows have
  genuinely different lengths (a ragged array).
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
  use the address of a slot that was given back. It is chapter 43's dangling
  pointer reproduced in the heap, and since the warehouse soon hands that slot to
  another request, *an attacker can put their own data in that place*. In the
  lists of severe vulnerabilities of browsers and kernels, use-after-free is a top
  fixture even today, which is why systems programming as a whole has moved in the
  direction of "let the language enforce ownership." The defence in C is
  discipline plus tools — the practice of assigning `nullptr` to a pointer right
  after `free`, and chapter 17's sanitizers.

  Neither is a cure-all, though. Assigning `nullptr` stops reuse and double free
  *through that one variable* only; any other alias holding the same address is
  left as it was — take it as a local defence for when there are no aliases.
  ASan catches use-after-free and double free very well, but detecting leaks
  needs LeakSanitizer to be on with it, and that depends on the platform and the
  settings (on a default build for Linux x86-64 the two usually come together).
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
  boundaries together, like chapter 42's proven).
]

== Closing Part VII

The map of memory is complete — on the ladder of registers and caches
(chapter 11) sit C's three storage durations (automatic, static, dynamic), and
pointers travel over them. We got the concept in chapter 35, the rules in
chapters 36–37, contiguous memory and strings in chapters 38–41, a safe component
in chapter 42, and lifetime and the warehouse in chapters 43–44. We have gone
once round the place where C's power and its danger live together.

We have seen dynamic memory's syntax, discipline and price. How an allocator
actually manages the warehouse, what alternative allocators and alternative
standard libraries are widely used today, and what map a program's memory is laid
out on in an operating system and in an embedded chip are treated in two chapters
at the end of Part XI (chapters 81 and 82).

The next part is short but long deferred — the structs and unions put off in
Part V with "declarations that make types come after we have a memory model."
That condition is now met.
