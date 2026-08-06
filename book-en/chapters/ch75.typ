#import "../../book/lib.typ": *

= The foundation — bytes, views, and arithmetic that does not overflow

#prereq(
  ([chapter 36, The rules of pointers], [alignment and provenance]),
  ([chapter 37, Arrays], [arrays and bounds]),
  ([chapter 68, How to ask about overflow], [arithmetic that does not overflow]),
)

#deepqa[
  Chapter 36 said that pointers to a character type (`char*`, `signed char*`,
  `unsigned char*`) alone have the privilege of
  peering into any object byte by byte, and chapter 13 showed code that broke that
  rule quietly collapsing under optimisation. Then what, concretely, does "handling
  bytes safely" keep?
][
  Two things. First, *fix the type you peer with to the one the rule exempts* —
  `unsigned char`. Second, *do not lose the range you are peering into* — carry a
  pointer alone and you forget where your land ends, which is chapter 37's boundary
  trespass. proven's basic vocabulary is these two each made into a type.
]

#organizer[
  We see the four basic vocabularies the whole library stands on — the type that
#idx("view")  points at raw bytes, the *view* binding pointer and length into one,
  size calculation that does not wrap, and alignment. Chapter 72's sixth bug (the
  hidden type of bytes) and chapter 37's boundary problem obtain their answer at the
  level of types here.
]

#chapter-questions()

== Bytes have a name

`proven_byte_t` is an alias for `unsigned char`. It seems no great thing, but one
declaration writes down a contract — "this pointer is an eye that looks at
*representation*, not something pointing at a value of some type".

Why this distinction matters was already seen in chapter 13. Code that looks at the
same memory alternately through `uint32_t*` and `uint16_t*` breaks the aliasing rule,
and the compiler uses that premise in optimisation. Looking through `unsigned char`,
on the other hand, is explicitly permitted by the standard. It is why the library
always goes through this type when handling raw memory, and so through the library
that bug cannot be written.

== Views — pointer and length as one

The vocabulary for pointing at memory is only three, and all are structs of two slots.
The difference between the three is only *does it own* and *may it be changed*.

```c
/* ① an owned lump — "this memory is mine, and one day I give it back" */
typedef struct {
    proven_byte_t *ptr;
    proven_size_t  size;
} proven_mem_t;

/* ② a borrowed reading window — it only peers into somebody's memory */
typedef struct {
    const proven_byte_t *ptr;
    proven_size_t        size;
} proven_mem_view_t;

/* ③ a borrowed writing window — it may change somebody's memory but not return it */
typedef struct {
    proven_byte_t *ptr;
    proven_size_t  size;
} proven_mem_mut_t;
```

One `const` divides ② from ③, and *the name* divides ① from the rest. What this small
struct does is one thing — *making sure the pointer and the length never part*. It is
the answer to the problem chapter 39 called "the real problem of strings is carrying
the length separately".

#dtable(
  columns: 4,
  [*type*], [*owns*], [*writes*], [*where it comes from*],
  [`proven_mem_t`], [yes], [yes], [an allocator (chapter 76)],
  [`proven_mem_view_t`], [no], [no], [`_as_view`, slicing, literals],
  [`proven_mem_mut_t`], [no], [yes], [an allocation result, a stack array, slicing],
)

There are paired functions for obtaining a window from an owned lump too —
`proven_mem_view_from_owned` and `proven_mem_mut_from_owned`. The `from_owned` in the
name means "leave the ownership as it is and open only a window".

#demo("examples-en/ch75/mem.c")

The example runs the vocabulary of this section and the next once each. Five places to
point at.

*① A view is two words.* The output's `sizeof(mem_view_t)=16` is that (on 64-bit,
pointer 8 + length 8). That it is larger than a bare pointer (8 bytes) is a view's only
cost, and in exchange bounds checking becomes possible.

*② Only the writing window can change things.* The example changed the first letter
with `mut.ptr[0] = 'A'` and every later output begins with `A`. Try to change the same
place through the reading view and it is *a compile error* — that is what the `const`
does.

*③ Slicing has two editions.* We look at them closely in the next section.

*④ Copying and moving have boundaries too.* `proven_mem_copy(dst, dst_cap, src)`
*compulsorily* takes the destination's capacity and, if the source does not fit, writes
not one byte and returns `OUT_OF_BOUNDS` (the output's `copy 15 into 8`). If they may
overlap it is `proven_mem_move` — chapter 60's `memcpy`/`memmove` distinction as it
stands.

*⑤ You can ask which lump a pointer belongs to.*
`proven_range_contains_ptr` is that, and what is worth noticing is that *the
implementation compares as integers*. Comparing pointers from different allocations
with `<` or `>=` is outside the contract (chapter 36), so the library converts to
`uintptr_t` and then checks. It is the function chapter 76's arena uses when confirming
"is this pointer one I handed out".

This detour is not, however, *a portable check the standard guarantees.*
`uintptr_t` is an optional type — an implementation need not have it — and the
standard nowhere promises that converting pointers to integers preserves the
order of addresses. What it promises is only the round trip: pointer →
`uintptr_t` → the same pointer. On today's mainstream platforms, with their flat
address spaces, the ordered comparison does what one expects; on machines where
an address is not a single number — segmented addresses, or capability pointers
(the CHERI of chapter 5) — it is another story. So this function should be read
not as a contract but as *an assumption about the platforms proven supports*:
a flat address space in which the integer conversion preserves order (see the
support range in chapter 81).

== Slicing — the operation used most in this part

#demo("examples-en/ch75/view.c")

`slice 6+4` is this section's heart. From an 8-byte view we asked for 4 bytes from
the 6th, so two bytes are short. The library *does not cut off as much as there is* —
it returns an error (number 2 is `PROVEN_ERR_OUT_OF_BOUNDS`). Giving as much as there
is looks kinder, but then the caller cannot know whether what was received is what
was requested. It is blocking chapter 72's truncation problem from repeating here.

Four functions form pairs — reading/writing × checked/unchecked.

#dtable(
  columns: 3,
  [*function*], [*what it returns*], [*when*],
  [`proven_mem_view_slice_checked`], [an `{err, view}` bundle], [the default. when the boundary is unknown],
  [`proven_mem_view_slice_unchecked`], [a view (no check)], [a hot loop whose boundary is already confirmed],
  [`proven_mem_mut_slice_checked`], [an `{err, mut}` bundle], [the default (writing)],
  [`proven_mem_mut_slice_unchecked`], [a writing window (no check)], [the same],
)

The checked edition's contract is three lines. *If the length is nonzero and the
pointer is null*, `INVALID_ARG`. *If `offset` exceeds the size, or `offset + size`
exceeds it*, `OUT_OF_BOUNDS` — and it matters that this check is written not as
`offset + size > view.size` but as `size > view.size - offset`. The former can wrap in
the addition; the latter never can (the same spirit as the checked arithmetic later in
this chapter). *If the size is 0* it returns an empty view with a null pointer and size
0 — a safeguard against dereferencing a pointer to nothing.

#qa[
  I saw the slicing function in two editions, `_checked` and `_unchecked` — why does
  the latter exist?
][
  For places where the boundary has *already been checked*. Redoing the same check
  every turn inside a loop is waste, so one checks once before the loop and uses the
  unchecked edition inside.

  ```c
  /* read in 8-byte pieces — the loop condition already guarantees the boundary */
  for (proven_size_t off = 0; off + 8 <= buf.size; off += 8) {
      proven_mem_view_t chunk = proven_mem_view_slice_unchecked(buf, off, 8);
      process(chunk);
  }
  ```

  That the name carries `_unchecked` matters — the dangerous choice can be made only
  through *a visible name*, and the default is always the safe side. It is one form of
  chapter 48's "write the contract as code", with the practical benefit too that
  searching for `_unchecked` in review skims the dangerous places.
]

#antipattern[
  Holding a view longer than its owner
][
  ```c
  proven_mem_view_t get_view(void) {
      proven_byte_t local[16] = {0};
      return (proven_mem_view_t){ .ptr = local, .size = sizeof local };
  }   /* local dies here — the returned view is already invalid */
  ```
  A view is *borrowed*. When the owner vanishes it becomes invalid that instant, and
  use after that is the access to a dead automatic variable learned in chapter 41.
  That a view is safer than a pointer is about *boundaries*, not about *lifetime* —
  lifetime is still for a human to keep. That is why the next chapter's story of
  allocators becomes necessary.
]

== Size arithmetic that does not overflow

Chapter 26 taught the wrap-round of unsigned integers, and chapter 7 showed why it is
defined behaviour. The fact that wrapping is quiet becomes especially dangerous in one
place — *when calculating the number of bytes to allocate*.

```c
void *p = malloc(count * sizeof(item_t));   /* if count is large it wraps */
```

If `count` is large enough the product wraps into a very small number, and `malloc`
succeeds at that small size. Then the program begins writing as many items as it
originally intended — a typical heap overflow. This is the pattern chapter 26 gave as
the real accident case of overflow, "size calculation".

#idx("checked arithmetic")The library uses C23's checked arithmetic for size
calculation. The example's `PROVEN_CKD_MUL` is that: if the product overflows it
returns *true* (the return value is whether it overflowed, not the result of the
calculation). If it overflows, the allocation is not even attempted.

#misconception[
  "`size_t` is as much as 64 bits, so it cannot overflow"
][
  It can, and it does. Because multiplication grows values *on a squared scale* —
  four billion × four billion already exceeds 64 bits. Moreover on a 32-bit machine
  `size_t` is still 32 bits, and code multiplying two 32-bit fields read from a file
  format wraps on the spot. Most important of all is *who settles that number*. If the
  size is a constant inside the program you may rest easy, but if it is a number that
  came from a file or the network then it is *an operand chosen by the attacker*.
]

#realcase[
  The vulnerabilities one multiplication made
][
  This pattern is a regular in the CVE lists. Cases have been reported repeatedly of
  an image decoder wrapping while calculating
  `width * height * bytes_per_pixel` in 32 bits, of a font parser wrapping while
  multiplying the glyph count, of decompression code wrapping while multiplying the
  original size. What they share is that *the input file settles the size* — that is,
  the attacker can choose the multiplication's operands. So today's languages and
  libraries do size calculation with checked arithmetic, or handle it with types that
  cannot overflow at all.
]

== Alignment — pushing up to the next boundary

The alignment learned in chapter 6 becomes a practical tool here.
`proven_mem_align_up(13, 8)` returns 16 — because to fit an object starting at
address 13 to an 8-byte boundary it must be pushed to 16. This is exactly the
calculation the next chapter's arena does every time it lays objects out in a row.

#dtable(
  columns: 3,
  [*name*], [*what*], [*note*],
  [`proven_mem_align_up(a, n)`], [rounds `a` up to a multiple of `n`], [0 if it overflows or `n` is not a power of two],
  [`proven_uintptr_align_up(p, n)`], [rounds an address up], [as an integer, instead of pointer arithmetic],
  [`proven_is_pow2(n)`], [is it a power of two], [checking an alignment value],
  [`PROVEN_MAX_ALIGN`], [`alignof(max_align_t)`], [usually 16. holds any type],
  [`PROVEN_DEFAULT_ALIGNMENT`], [8], [the default for byte data such as strings and buffers],
)

*Reporting failure as 0* is these functions' peculiar contract. If the alignment is not
a power of two or the rounding overflows they return 0, so the result must be checked
for 0 before being used as a size. Inside the library the arena does that check for
you, so you will call these directly only when making a data structure of your own.

It is worth knowing too that the two constants have different uses. A byte array or a
string is content with `PROVEN_DEFAULT_ALIGNMENT` (8), while *general allocation that
does not know what type is coming* uses `PROVEN_MAX_ALIGN`. It is also why chapter 76's
heap allocator divides the two — requests at or below the default alignment go to
`malloc` (growth in place is then possible), and stricter requests to an aligned
allocation.

#recap[
  This chapter's vocabulary.

  #dtable(
  columns: 3,
    [*name*], [*what*], [*contract*],
    [`proven_byte_t`], [an alias for `unsigned char`], [the only legal window onto representation],
    [`proven_mem_view_t`], [a read-only view (ptr+size)], [borrowed — it cannot outlive its owner],
    [`proven_mem_mut_t`], [a writable view], [the same],
    [`..._slice_checked`], [making a sub-view], [an error if it exceeds the range, no truncation],
    [`PROVEN_CKD_MUL/ADD`], [checked arithmetic], [true if it overflows — the calculation is discarded],
    [`proven_mem_align_up`], [rounding up an alignment], [alignment is a power of two],
)
]

We are equipped with the vocabulary for *looking at* memory. Next is the story of
*obtaining* memory — and in that place we meet this library's most characteristic
decision.
