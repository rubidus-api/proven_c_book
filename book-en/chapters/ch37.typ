#import "../../book/lib.typ": *

= Arrays

#prereq(
  ([chapter 34, Objects, addresses, pointers], [addresses and pointers]),
  ([chapter 11, Memory divides], [memory is a run of adjacent slots]),
)

#deepqa[
  Chapter 11 said a cache line carries "sixty-four neighbouring slots as one
  box", and that this is why code scanning an array in order is fast. Then what
  exactly is an "array" through the eye of memory?
][
  Slots of the same type lined up *adjacent without gaps* — that is all. `int a[5]` is five ints laid side by side at consecutive addresses, so knowing only
  the first slot's address and the type lets every other position be calculated
  (element $i$ = start address + $i times$ slot size). This property of
  "calculable neighbours" is the root of both the array's power (immediate
  access, cache friendliness) and its danger (miscalculating the boundary).
]

#organizer[
  Contiguous memory — the array. Declaration and traversal, the real relationship
#idx("buffer overrun")  between arrays and pointers (the notorious "decay"), and
  the life-or-death rule of the boundary. Chapter 25's `char line[100]` credit is
  settled here.
]

#chapter-questions()

== Declaration, access, traversal

#demo("examples-en/ch37/arr.c")

`int a[5] = {3, 1, 4, 1, 5};` — type, name, slot count, and brace
initialisation. Access is `a[i]` and *numbering starts at 0* (the first element
is `a[0]`, the last `a[4]`). Why count from 0 was already answered by the opening
exchange — `a[i]` really means "the place $i$ slots *away* from the start", so the
first element is zero away.

== Arrays and pointers — the truth about decay

The latter part of the demonstration is this chapter's heart. Let us set out
exactly the relationship that has caused the most confusion in C — arrays and
pointers.

*The rule: in most contexts an array's name decays into "the address of its first
element."* That is why `int *p = a;` is legal — `a` was read as the pointer value
`&a[0]`. And the notation `a[i]` is itself sugar for `*(a + i)` — add an integer
to a pointer and you get the address "$i$ slots along, by that type's size"
(pointer arithmetic — chapter 36's rules apply here), and dereferencing that is
indexed access. The demonstration's `a[2] == *(a + 2)` is the check.

#misconception[
  "An array is a pointer"
][
  This famous sentence is false — an illusion created by how often decay happens.
  An array is *memory itself*, five slots of it; a pointer is *a different
  variable* holding one address. The demonstration's last line is the decisive
  evidence: `sizeof a` is 20 (the total size of five ints) and `sizeof p` is 8
  (the size of one address, chapter 34). The representative context in which decay
  does *not* happen is exactly `sizeof`, which is why the difference is visible —
  and it is thanks to this that chapter 25's `fgets(line, sizeof line, stdin)`
  measured the container correctly. One consequence matters: "pass" an array to a
  function and only the decayed *address* is copied (chapter 32), so the function
  side cannot learn the size with `sizeof` — which is why C functions
  conventionally take an array *and its length as a separate argument*. The
  concern of chapter 9's representations that "keep the length beside the data"
  reappears here at C's function boundary.
]

== Adding to a pointer is not ordinary addition

We have just said that `a[i]` is `*(a + i)`. Then what value, exactly, is
`a + 2`? If it were "2 added to an address" it would be two bytes along; in fact
it is *two `int` slots along*, that is eight bytes. Pointer arithmetic moves by
*multiplying by the size of the type pointed at*.

#figure-svg("ptrmath", caption: [The distance `+1` jumps is the size of the type pointed at.])

Set down exactly, the rule is this.

#dtable(
  columns: 3,
  [*expression*], [*meaning*], [*result type*],
  [`p + n`, `n + p`], [forward by `n` times the size of what `p` points at], [the same pointer type as `p`],
  [`p - n`], [backward by the same], [the same],
  [`p - q`], [*how many elements* fit between the two addresses], [`ptrdiff_t` (a signed integer)],
  [`p++`, `++p`, `p += n`], [moved by the same rule, then assigned], [the same],
)

So adding an integer to a pointer is not "adding a number to a number that is an
address" but *counting in slots*. Only on a `char *` is 1 one byte; on an
`int *` it is four, on a `struct point *` it is one whole struct.

#qa[
  Why was it settled this way — would plain byte arithmetic not be simpler?
][
  Three reasons overlap.

  *First, it gives arrays away for free.* `a[i]` can be defined as `*(a + i)`
  only because the addition counts elements. Had it counted bytes, every array
  access would have had to read `*(a + i * sizeof *a)`, and changing an element
  type would have meant editing every access. That one rule is how C has arrays
  without any special machinery for them.

  *Second, the type already knows the size.* Recall chapter 5's refrain — memory
  has no boundaries, and how many bytes count as one lump is decided by the
  *reading side*. A pointer's type is exactly that decision. Letting the value
  that already carries the decision carry the movement too is natural.

  *Third, the machine moves that way.* Most CPUs have an addressing mode that
  computes "base + index × size" in one step. Element-wise arithmetic maps
  straight onto it — another instance of chapter 4's "C did not hide the
  machine".
]

#demo("examples-en/ch37/ptrmath.c")

The first block of output shows the whole rule. The same `+ 1` moves a different
distance for each type, and the distance is exactly `sizeof`. So `(char *)p + 1`
and `p + 1` point at different places — which is where the idiom of casting to a
character pointer to move by bytes comes from (chapter 74's views do this).

The second block is *subtraction*. `&a[4] - &a[1]` is 3, not 12 — pointer
subtraction gives a count of *elements*, not of bytes. And that is why the result
has type `ptrdiff_t`: subtracting a later pointer from an earlier one can be
negative, so the type must be signed.

=== How far it may go — the contract of the arithmetic

Chapter 36's provenance applies to pointer arithmetic as it stands. Reduced to
practical sentences:

#dtable(
  columns: 2,
  [*what is done*], [*verdict*],
  [moving a pointer within the array it points into], [fine],
  [forming a pointer to the position *one past the last element*], [fine — but *dereferencing it is forbidden*],
  [*forming* an address further out than that], [outside the contract — even if it is only computed and never followed],
  [`+ 1` on a single object that is not an array], [fine — it is treated as an array of length one],
  [subtracting pointers into different arrays], [outside the contract],
  [ordering pointers into different arrays with `<`], [outside the contract (equality with `==` is allowed)],
  [adding an integer to a `void *`], [not in the standard — a gcc/clang extension (one byte per unit)],
)

"Outside the contract even when only formed" sounds strange, but it changes how
loop conditions are written. `p <= a + n` is safe because it goes no further than
one past the end; `p < a + n + 1` computes an address one further out and is
outside the contract. Sweeping backwards as
`for (p = a + n - 1; p >= a - 1; p--)` is dangerous for the same reason — `a - 1`
is an address before the array, and it is outside the contract the moment it is
computed.

#antipattern("a backward loop that steps in front of the array")[
  ```c
  for (int *p = a + n - 1; p >= a - 1; p--)   /* forms a - 1 — outside the contract */
      ...
  ```
  The safe shapes use an index, or keep the end condition inside the array.
  ```c
  for (size_t i = n; i-- > 0; )    /* stops when i is 0 — no address is formed */
      use(a[i]);
  ```
]

#realcase("optimisers really use this rule")[
  "Outside the contract even when only formed" looks excessive until you see what
  compilers do with the promise. If `p` is guaranteed to point within the array
  `a`, then `p >= a` is always true and the test may be deleted — exactly the
  logic of optimisation from chapter 13. Bounds checks written this way have
  disappeared from release builds more than once, in bugs reported in earnest.
]

== An array parameter is not an array

There is one more rule in the position of a function parameter. *Declare it as an
array and the compiler turns it into a pointer.* So the following three are
completely identical declarations to the compiler.

```c
void f(int *a);
void f(int a[]);
void f(int a[10]);   /* the 10 is documentation only; it is not checked */
```

For a multi-dimensional array only *the outermost (leftmost) dimension* is
stripped. The inner dimensions remain part of the type.

```c
void g(int m[3][4]);   /*  the real type is  int (*m)[4]  */
void h(int c[2][3][4]);/*  the real type is  int (*c)[3][4] */
```

There are three places where this fact becomes visible.

#demo("examples-en/ch37/param.c")

*① The value of `sizeof` differs.* On the caller's side `arr` is a real array, so
40 bytes (ten `int`s), but inside the function it is 8 — the size of one pointer.
So the idiom `sizeof a / sizeof a[0]` for counting elements *does not work inside
a function.* The count must be received separately.

gcc really does point at this mistake. Here is the diagnostic received when the
example was first written.

```text
error: ‘sizeof’ on array function parameter ‘a’ will return size of ‘int *’
       [-Werror=sizeof-array-argument]
note: declared here
```

*② It can be assigned to.* An array's name cannot appear on the left of an
assignment, but a parameter is just a pointer variable, so `a = a + 1` works. The
example's `second_of` does that — it moves the parameter along and reads `a[0]`,
yielding the original's second element — and the caller's `arr` was not affected
at all (exactly chapter 32's copy-by-value rule).

*③ In two dimensions only half remains.* `sizeof p` is 8 (a pointer) while
`sizeof p[0]` is 16 — that is, the size of "one row" survives. It is because the
inner dimension remains in the type, and that is what makes an access like
`m[1][2]` compute correctly. Put the other way round, *the size of the inner
dimension must be written* — `int m[][]` does not compile.

#misconception[
  "I wrote `int a[10]` for the parameter, so passing fewer than 10 will be
  caught"
][
  It will not. That `10` is closer to a comment with no meaning at all to the
  compiler — the type simply becomes `int *`. If you really want to make the array
  size a contract there are two roads. One is to *take the size as a separate
  parameter* (the commonest and surest method), the other is the `[static 10]`
  notation of the next section. The latter pins a minimum count down as a promise
  so that the compiler can warn.
]

== Arrays whose size is settled at run time — VLAs

Every array so far had its size settled at compile time. C99 added one more —
the *variable length array* (VLA). A variable, not a constant, may be written in
the size slot.

#demo("examples-en/ch37/vla.c")

There are two uses. The first is *the VLA as a local variable* (`int local[n];`),
where an array whose size is settled at run time is taken on the stack. That is
also the only time `sizeof` is computed at run time — the example's `sizeof local`
returning 16 is the evidence. The second is *the VLA as a parameter*
(`const int a[n]`, `const int m[n][n]`), and this side is far more useful. Its
value is greatest in two dimensions and above — you can write `m[i][j]` directly
instead of computing `m[i * n + j]` by hand.

Local VLAs, however, have hardened into something *not recommended* in practice.

- *Fail to control the size and the stack overflows.* Use a number that came from
  input directly as a size and an attacker can bring down the stack, and there is
  no way to check for that collapse.
- *There is no way to report failure.* `malloc` at least returns null
  (chapter 41); a VLA simply collapses when there is no room.
- *Its standing in the standard wavered too.* Mandatory in C99, it became an
  *optional* feature in C11 (if `__STDC_NO_VLA__` is defined, the implementation
  lacks it). MSVC does not support it.
- The Linux kernel removed VLAs from its entire codebase in 2018 — predictability
  of stack usage and performance were the reasons.

In summary: *the VLA notation in a parameter is usable; avoid local VLAs.* If you
need an array whose size is settled at run time, chapter 41's dynamic allocation
is the proper method.

== `[static N]` in a parameter — "at least this many will arrive"

There is one more peculiar syntax used only in array parameters.

```c
int sum3(const int a[static 3]);   /* a points at three or more elements */
```

Here `static` has nothing to do with storage duration (chapter 40). It means the
contract *"the pointer passed in this argument points at an array of at least N
elements."* Two things are gained — the compiler may optimise on that premise
(prefetching and the like), and it can point out with a warning code that passes
null or a shorter array.

Two things to remember.

- *The only place it may be written is an array declarator in a function
  parameter.* It cannot go on a local variable or a struct member declaration.
  And for a multi-dimensional array it attaches only to the outermost dimension.
- *Break the contract and you are outside it.* `sum3(nullptr)` or passing a
  two-element array is undefined behaviour — meaning it is a *promise*, not a
  check.

=== `[static 1]` — writing "not null" into the declaration

The form you meet most often has size 1.

```c
void use(int a[static 1]);   /* a points at one valid object */
```

Saying "an array with *at least one* element" is the same as saying *not a
null pointer, and the object it points at is valid*. Instead of a comment
reading "do not pass null", the promise sits in the declaration itself. That
is why this spelling is used as the idiom for expressing non-nullness in a
form a machine can read.

Tools really do read it. Here is what GCC 15 did while this book was being
written.

```c
static int sum1(int a[static 1]) { return a[0]; }
...
(void)sum1(NULL);
```

```text
warning: argument 1 null where non-null expected [-Wnonnull]
note: in a call to function ‘sum1’ declared ‘nonnull’
```

"Declared `nonnull`" is the phrase that matters — the compiler translates
`[static 1]` into a no-null property. With a size above 1 it looks at the
length as well. Passing a two-element array to a `const int a[static 3]`
parameter gets this:

```text
warning: ‘sum3’ reading 12 bytes from a region of size 8 [-Wstringop-overread]
```

Still, it is *a promise, not a check*. No run-time test is inserted, and the
compiler can only complain where it can see the problem — a literal null, an
array of known size. Once the pointer has flowed through a few functions it
usually says nothing at all. A call that breaks the promise is simply
undefined behaviour.

=== A qualifier inside the same brackets qualifies the *pointer*

The other thing that may go inside those brackets is a qualifier.

```c
void f(int a[const 4]);      /* == void f(int *const a); */
void g(int a[restrict 4]);   /* == void g(int *restrict a); */
```

Here `const` attaches to the *pointer itself*, not to the elements it points
at. It means `a` cannot be made to point elsewhere inside the function; it
does not mean `a[0]` is read-only. To protect the elements, the `const` goes
*outside* the brackets, as in `const int a[4]`.

#misconception[
  "`int a[const 4]` means the contents of the array cannot be changed"
][
  It does not. An array parameter has already decayed to a pointer (previous
  section), and the qualifier inside the brackets attaches to *that decayed
  pointer*. `int a[const 4]` is `int *const a`, so `a[0] = 1;` is fine and
  `a = other;` is not. To protect the contents write `const int a[4]`; for
  both, `const int a[const 4]`.

  That is exactly why the syntax exists — with array notation there is no
  other way to qualify the pointer. It becomes genuinely necessary when you
  want `restrict` on an array parameter.
]

The two can be combined. The grammar allows both `[static` qualifiers `N]`
and `[`qualifiers `static N]`, so `int a[const static 1]` and
`int a[static const 1]` are both valid — "points at one valid object, and the
pointer itself will not move."

#qa[
  Should every pointer parameter then be written `[static 1]`?
][
  For a function that does *not* accept null, there is a real case for it:
  the intent stays in the declaration, tools read it, and the optimiser may
  drop null checks.

  Two things weigh against it. First, never write it on a function that takes
  null as a legitimate input — `free`, or anything with an "absent is fine"
  argument. Second, it costs reading effort if your team does not know the
  syntax; it appears almost nowhere in the standard library's own
  declarations. Using it selectively in internal APIs, where null means a
  bug, is the practical compromise.
]

== The boundary — the life-or-death rule

An array's safety rule is one and admits no compromise — *valid numbers run from
0 to the slot count minus one.* Reading or writing `a[5]` (in a five-slot array)
is outside the contract, and that slot is *someone else's memory* — perhaps a
neighbouring variable, perhaps the ledger of a function call seen in chapter 39.
Read it and you get rubbish; write it and someone else's data is quietly broken —
the buffer overrun is the single cause of more accidents and security
vulnerabilities than anything else in C's history (the next two chapters are those
true stories).

There is exactly one place the standard specially permits at the boundary — as
foreshadowed in chapter 36, *making the address* of the slot one past the end
(`a + 5`) is legal (as long as you do not follow it). The traversal idiom stands
on that:

```c
for (int *it = a; it != a + 5; it += 1) { /* use *it */ }
```

#qa[
  Does the compiler not catch a boundary violation?
][
  Only some. Obvious violations with constants, like `a[7]`, are caught well by
  today's compiler warnings, but when the number is the result of a calculation it
  cannot be known at compile time — because doing boundary checks constantly at
  run time is something C *chose not to do* for performance (the price of that
  choice and the ways of making up for it are the remaining subjects of this
  part). The run-time net is chapter 17's ASan — run code that violates a boundary
  in an ASan build and it is caught at the moment of violation, with file and line
  number. And there is the road of using components with boundary checking built
  in from the start — chapter 39's proven is that road.
]

We can handle contiguous memory. The next chapter is the array's most famous
application — the string. The background of chapters 6 and 9 (a character is a
number; the choice of NUL termination) finally meets C's syntax in full.
