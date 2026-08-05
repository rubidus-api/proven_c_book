#import "../../book/lib.typ": *

= Using structs — temporary values, named arguments, layout

#prereq(
  ([chapter 41, Structs], [defining a struct]),
  ([chapter 36, Arrays], [arrays and passing by value]),
)

#deepqa[
  Chapter 41 said a struct is a value, so assigning copies it whole and passing it
  to a function sends a copy. But chapter 36 said that passing an array to a
  function makes it decay into a pointer so that *the original is touched*. Do the
  two not collide — which is it when a struct contains an array?
][
  The struct wins. An array decaying into a pointer is the rule for *when the
  array itself is written as an argument*; an array that has gone in as a struct
  member is part of the value that is the struct and is therefore copied. So the
  only way in C to pass an array truly *like a value* is "wrapping it in a
  struct." This chapter's second example shows that contrast by measurement — the
  side passed by value leaves the original untouched, and the side that decayed
  into a pointer changes it.
]

#organizer[
  If chapter 41 was the syntax of the struct, this chapter is *how to use it*.
#idx("compound literal")  The notation for referring to a struct inside a
  struct, the temporary struct made and handed over on the spot (the compound
  literal), the idiom of "order-free named arguments" built from it, and the
#idx("padding")  padding that gets in between members and how to remove it or
  force it. Finally we measure the only road for passing an array by value, and
  its price.
]

#chapter-questions()

== Nesting and access — reading dots and arrows mixed

A struct's member may itself be a struct. The notation is simply layered.

```c
struct point { int x, y; };
struct rect  { struct point origin; struct point size; };
struct scene { struct rect *frame; const char *name; };

struct rect  r  = { .origin = { .x = 1, .y = 2 }, .size = { .x = 30, .y = 40 } };
struct scene s  = { .frame = &r, .name = "main" };

r.origin.x        /* value inside value        : dot + dot */
s.frame->size.y   /* pointer inside value      : dot + arrow + dot */
(&r)->origin.y    /* the arrow is only an abbreviation of (*p). */
```

There is only one rule. *If the left is a value, a dot; if a pointer, an arrow.*
`p->x` is an abbreviation of `(*p).x` (chapter 41), and the abbreviation exists
because handling structs through pointers is overwhelmingly common. Indeed, code
written as `(*p).x` is usually old code or a place explaining operator
precedence.

#qa[
  Where do you break a long chain like `s.frame->size.y` when reading it?
][
  Left to right, one step down at a time. `s` (the scene) → `.frame` (the pointer
  inside it) → `->size` (the size of the rectangle pointed at) → `.y` (that
  point's y). Each arrow is a mark that *one dereference happens*, so it also
  means there are as many pointers needing a null check as there are arrows. If
  `s.frame` is null this notation collapses on the spot — a long chain, as easy as
  it is to read, also hides the checks.
]

== The temporary struct — the compound literal

The notation for making one struct value *on the spot*, without making a named
variable, is the compound literal (C99).

```c
draw_((struct draw_opts){ .width = 40, .title = "chart" });   /* straight as an argument */
return (struct point){ .x = a.x + dx, .y = a.y + dy };        /* as a return value */
```

Take away three properties.

*First, it is an lvalue.* It merely has no name; it is a real object, so its
address can be taken and its members assigned to. The example's
`&(struct draw_opts){ … }` is the check. It is easy to think "being a temporary
(an rvalue) its address cannot be taken", but C's compound literal is not like
that — a difference from C++'s temporary objects.

*Second, its lifetime is the end of the block, not of the statement.* A compound
literal written inside a block lives until that block ends (automatic storage
duration). So within the same block it is safe to carry its address about.

*Third, therefore, sending its address out of the function is a dangling
pointer.*

#antipattern[
  Returning the address of a compound literal
][
  ```c
  struct point *make(int x, int y)
  {
      return &(struct point){ .x = x, .y = y };   /* it vanishes when the function ends */
  }
  ```
  Exactly the same accident as returning the address of a local variable in
  chapter 35. To return a value, return it *by value*
  (`struct point make(...)`), fill a place the caller provided, or use dynamic
  allocation (chapter 40). A compound literal written at file scope has static
  storage duration and does not have this problem, but in that place it is usually
  better to give it a name.
]

== Named arguments — passing one struct

#demo("examples-en/ch42/opts.c")

From here comes the idiom that changes code most in practice. Consider a function
with five or six arguments.

```c
draw(40, 20, false, true, 3, "chart");    /* what is the third true? */
```

C has neither other languages' named arguments nor default values. But overlay
*designated initialisers with a compound literal* and you effectively get the
same thing.

```c
struct draw_opts { int width; int height; bool grid; const char *title; };
static void draw_(struct draw_opts o);
#define draw(...) draw_((struct draw_opts){ __VA_ARGS__ })

draw(.title = "chart", .height = 20, .width = 40);   /* order-free */
draw(.grid = true);                                   /* the rest are defaults */
draw();                                               /* all defaults */
```

Four things are gained.

+ *Freedom from order.* A designated initialiser fixes the slot by name, so the
  caller writes in whatever order suits.
+ *What is left out is 0.* The standard's promise that unwritten members are
  filled with 0 (null for pointers) becomes the "default value". So the knack is
  to design the fields *so that 0 makes sense as the default* — the example
  reading `width == 0` as "the default 80" is that.
+ *The call site is self-explanatory.* You need not ask what the `false, true, 3`
  above are.
+ *Adding a field later does not break existing calls.* Change an argument list
  and every call site must be fixed, but adding one member to a struct has no
  effect on existing calls at all (that member becomes 0). In an API maintained
  for a long time this property is especially valuable.

#qa[
  Are there no traps in this idiom?
][
  Beware of three.

  First, *the order of evaluation between initialiser items is not fixed.* Mix in
  side effects, as in `draw(.width = i++, .height = i)`, and the result is
  unpredictable (chapter 20). Write only values in the arguments.

  Second, if there is *a field for which 0 is a valid value*, "left out" cannot be
  told from "0 was specified". Design such a field with its meaning inverted
  (`grid` rather than `no_grid`), or add a separate presence field.

  Third, *the cost of building and passing a large struct every time*. Option
  structs are usually small enough not to matter, but when they grow, use the
  variant of receiving `const struct opts *` and passing
  `&(struct opts){ … }` at the call site — the property above, that an address can
  be taken, works here.
]

#realcase[
  Named arguments as met in practice
][
  This pattern is widespread. Various initialisation functions in the Linux
  kernel, the way standard and POSIX APIs take options as a struct (such as
  `struct sigaction`, chapter 62, or `struct timespec`), and the `..._desc`
  structs of graphics libraries (the `..._DESC` of several GPU APIs, say) are all
  the same idea. "When arguments grow numerous, bind them into a struct" is
  practically an idiom in C, and C99's designated initialisers made it read well.
]

== Padding — the empty space between members

#demo("examples-en/ch42/layout.c")

The sizes of the three members `char`, `int`, `char` sum to 6, and yet the struct
is 12 bytes. Six bytes went in as *padding*. The reason is chapter 6's alignment —
`int` must sit at an address that is a multiple of 4, so three bytes are left
empty after the first `char` to push `b` to offset 4. And three bytes attach after
the last `char` too (*trailing padding*): because when this struct is laid out as
an array, the next element's `int` must keep its alignment as well.

It comes down to three rules.

+ Each member sits at *an offset that is a multiple of its own alignment*.
+ The whole struct's alignment is *the maximum of its members' alignments*.
+ The struct's size is *rounded up to a multiple* of that alignment (trailing
  padding).

So *merely changing the order of members shrinks the size.* The example's `tight`
lays the large one first and reduces 12 bytes to 8. In a program laying out
millions of structs, this one line of reordering changes both memory and cache
hit rate (chapter 11).

#misconception[
  "Padding bytes contain 0"
][
  They do not. The value of padding is *unspecified*. Initialisation may put 0
  there, or whatever previously used that place may remain. Three practical traps
  come from this.

  - *Do not compare structs with `memcmp`* (chapter 57) — equal values may come
    out "different" because the padding differs. Compare member by member.
  - *Do not hash a struct whole* — for the same reason, the same value gives
    different hashes.
  - *Do not send a struct as it is to a file or a network* — the rubbish in the
    padding goes with it (and can be an information leak), and if the receiving
    side's layout differs the interpretation goes wrong too.

  Whether struct assignment (`b = a;`) copies the padding as well is not promised
  by the standard either. Remember that *only the members are meaningful* and it
  is all explained.
]

== How to remove padding, how to force alignment

There are certainly places where padding is inconvenient — when *the byte layout
is fixed from outside*, as in a file format or a communication protocol. So
implementations provide devices for turning padding off.

#platform[
  packed and pragma pack — not standard
][
  ```c
  #pragma pack(push, 1)          /* widely used, common to MSVC, GCC and Clang */
  struct header { char kind; int length; };
  #pragma pack(pop)              /* always put it back */

  struct header2 { char kind; int length; } __attribute__((packed));  /* GCC and Clang */
  ```
  Neither is *standard C*. In the example `#pragma pack(1)` made a struct of 6
  bytes with alignment 1. Fail to pair `push` with `pop` and the layout of structs
  in headers included afterwards changes too, giving the nasty bug of a layout that
  disagrees with a library — leaving pack open inside a header without closing it
  is the representative accident.
]

#antipattern[
  Passing the address of a packed struct's member
][
  ```c
  struct __attribute__((packed)) h { char k; int len; };
  void take(int *p);
  take(&s.len);          /* an unaligned address — outside the contract */
  ```
  A packed struct's member may sit at a misaligned place. Pass its address as an
  ordinary `int *` and the receiving side accesses it assuming alignment — on a
  tolerant machine (x86) merely slower, on a strict machine dead on the spot
  (chapter 6). GCC and Clang issue the warning
  `-Waddress-of-packed-member` here.

  Reading the value (`int n = s.len;`) is safe, because the compiler gathers the
  bytes for you. It is *leaking the address* that is the problem.
]

There is a tool in the opposite direction, *forcing alignment*, and this one is a
standard word of C23 (chapter 66).

```c
struct cacheline { alignas(64) int counter; };   /* on a 64-byte boundary */
```

In the example this struct became size 64, alignment 64. Its uses are clear —
putting each thread's counter on a different cache line to avoid the *false
sharing* seen in chapter 11, or meeting a hardware requirement such as DMA or
SIMD. It is not free, though: the struct above uses 64 bytes to hold one `int`.

#qa[
  So when handling a file format or a communication protocol, is a packed struct
  the right answer?
][
  There is a safer right answer: *moving between the byte sequence and the struct
  by hand.*

  ```c
  /* reading: take the fields out of the buffer one at a time */
  uint32_t len;
  memcpy(&len, buf + 1, sizeof len);
  len = le32toh(len);            /* state the endianness too (chapter 5) */
  ```

  Laying a packed struct over a buffer (`struct h *p = (struct h *)buf;`) assumes
  three things at once — no padding, correct alignment, matching endianness. Get
  one of them wrong and it breaks silently, and besides, access through a swapped
  type runs into the aliasing rules (chapter 46). Field-by-field `memcpy` is longer
  but exposes all three assumptions in the code. What Part XII's library does is
  exactly to gather this tedious work into one place.
]

== Passing an array by value — can it be done, and should it?

The latter part of the example is that contrast. `total(struct row r)` received a
copy and changed it, and the original was untouched (`cell[0] = 1`).
`total_raw(int cell[8])` decayed into a pointer and changed the original
(`cell[0] = 999`). The assignment `struct row copy = r;` likewise copies the array
member whole.

So it comes to this. *If you want to handle an array with value semantics in C,
wrap it in a struct.* It is the only way, and there are places where it is really
used.

#dtable(
  columns: 2,
  [*where it is worthwhile*], [*why*],
  [small fixed-size vectors and matrices (`struct vec3`, `struct mat4`)], [calculating with them like values is natural],
  [fixed-size identifiers and keys (`struct uuid { unsigned char b[16]; }`)], [copying is cheap and leaves no room for mistakes],
  [when an array must be *returned*], [an array cannot be returned but a struct can],
  [when you want to pass it immutably], [being a copy, the callee cannot touch the original],
)

The price is clear too.

*Stack usage.* The copy is usually placed in the called function's stack frame.
Pass a 16 KiB struct by value and that much more is piled on per call — measure
it and the stack position before and after the call really does widen by the
struct's size. The stack is usually about 8 MiB (chapter 35), and can be far
smaller in recursion or on a per-thread stack (chapter 63). Recursion passing
large structs by value is the shortest road to stack overflow.

*The cost of copying.* But saying "a copy always happens" would be inaccurate.
The calling convention decides — a small struct (usually up to two words) crosses
*in registers* with nothing worth calling a copy, and for a large struct it is
common for the caller to build it in memory and pass its address hidden. Moreover,
if the function is inlined the compiler may remove the copy itself. So the
accurate sentence is: *the meaning is always a copy, and the real cost is decided
by size, calling convention and optimisation.*

#misconception[
  "Passing a struct by value is always slow"
][
  It depends on size. Something small like `struct point { int x, y; }` is if
  anything faster passed by value, and reads better too — pass a pointer and a
  dereference appears, and the compiler must suspect "someone may change the value
  through this pointer", which reduces optimisation.

  The rough practical rule: *up to a couple of words by value, larger than that by
  `const` pointer.* And this choice is a matter not only of performance but of
  contract — receive by value and "the original is not touched" is guaranteed by
  the syntax; receive by pointer and it is only promised with `const`.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [access], [dot if the left is a value, `->` if a pointer (= `(*p).`)],
    [compound literal], [`(struct T){…}` — an lvalue whose lifetime is *the block's end*],
    [returning an address], [do not send a compound literal's address out of the function],
    [named arguments], [an options struct + designated initialisers. omitted members are 0],
    [designing defaults], [fix the fields so that *0 makes sense as the default*],
    [padding], [it arises from alignment. its value is unspecified],
    [member order], [lay the large ones first and the size shrinks],
    [`memcmp`, hashing, serialising], [do not handle a struct whole],
    [`pack`], [not standard. pair `push`/`pop`, do not leak member addresses],
    [`alignas`], [standard. avoiding false sharing, hardware requirements],
    [array by value], [wrap it in a struct. watch the stack and the size together],
  )
]

We have learned how to use structs. The next chapter is the device for seeing the
same memory *through a different eye* — the world of unions and representation.
The endianness demonstration booked in chapter 5 finally opens.
