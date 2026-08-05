#import "../../book/lib.typ": *

= Arrays

#organizer[
  Contiguous memory — the array. Declaration and traversal, the real relationship
#idx("buffer overrun")  between arrays and pointers (the notorious "decay"), and
  the life-or-death rule of the boundary. Chapter 25's `char line[100]` credit is
  settled here.
]

#deepqa[
  Chapter 11 said a cache line carries "sixty-four neighbouring slots as one
  box", and that this is why code scanning an array in order is fast. Then what
  exactly is an "array" through the eye of memory?
][
  Slots of the same type lined up *adjacent without gaps* — that is all. `int
  a[5]` is five ints laid side by side at consecutive addresses, so knowing only
  the first slot's address and the type lets every other position be calculated
  (element $i$ = start address + $i times$ slot size). This property of
  "calculable neighbours" is the root of both the array's power (immediate
  access, cache friendliness) and its danger (miscalculating the boundary).
]

== Declaration, access, traversal

#demo("examples-en/ch36/arr.c")

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
(pointer arithmetic — chapter 35's rules apply here), and dereferencing that is
indexed access. The demonstration's `a[2] == *(a + 2)` is the check.

#misconception[
  "An array is a pointer"
][
  This famous sentence is false — an illusion created by how often decay happens.
  An array is *memory itself*, five slots of it; a pointer is *a different
  variable* holding one address. The demonstration's last line is the decisive
  evidence: `sizeof a` is 20 (the total size of five ints) and `sizeof p` is 8
  (the size of one address, chapter 33). The representative context in which decay
  does *not* happen is exactly `sizeof`, which is why the difference is visible —
  and it is thanks to this that chapter 25's `fgets(line, sizeof line, stdin)`
  measured the container correctly. One consequence matters: "pass" an array to a
  function and only the decayed *address* is copied (chapter 32), so the function
  side cannot learn the size with `sizeof` — which is why C functions
  conventionally take an array *and its length as a separate argument*. The
  concern of chapter 9's representations that "keep the length beside the data"
  reappears here at C's function boundary.
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

#demo("examples-en/ch36/param.c")

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

#demo("examples-en/ch36/vla.c")

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
  (chapter 40); a VLA simply collapses when there is no room.
- *Its standing in the standard wavered too.* Mandatory in C99, it became an
  *optional* feature in C11 (if `__STDC_NO_VLA__` is defined, the implementation
  lacks it). MSVC does not support it.
- The Linux kernel removed VLAs from its entire codebase in 2018 — predictability
  of stack usage and performance were the reasons.

In summary: *the VLA notation in a parameter is usable; avoid local VLAs.* If you
need an array whose size is settled at run time, chapter 40's dynamic allocation
is the proper method.

== `[static N]` in a parameter — "at least this many will arrive"

There is one more peculiar syntax used only in array parameters.

```c
int sum3(const int a[static 3]);   /* a points at three or more elements */
```

Here `static` has nothing to do with storage duration (chapter 39). It means the
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

Qualifiers such as `const` or `restrict` may go in the same place
(`void f(int a[const 8])`). It is not syntax you see often, but on meeting it in
someone else's code, recognise it as "a promise of a minimum count, not storage
duration."

== The boundary — the life-or-death rule

An array's safety rule is one and admits no compromise — *valid numbers run from
0 to the slot count minus one.* Reading or writing `a[5]` (in a five-slot array)
is outside the contract, and that slot is *someone else's memory* — perhaps a
neighbouring variable, perhaps the ledger of a function call seen in chapter 38.
Read it and you get rubbish; write it and someone else's data is quietly broken —
the buffer overrun is the single cause of more accidents and security
vulnerabilities than anything else in C's history (the next two chapters are those
true stories).

There is exactly one place the standard specially permits at the boundary — as
foreshadowed in chapter 35, *making the address* of the slot one past the end
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
  in from the start — chapter 38's proven is that road.
]

We can handle contiguous memory. The next chapter is the array's most famous
application — the string. The background of chapters 6 and 9 (a character is a
number; the choice of NUL termination) finally meets C's syntax in full.
