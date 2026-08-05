#import "../../book/lib.typ": *

= The foundation — bytes, views, and arithmetic that does not overflow

#organizer[
  We see the four basic vocabularies the whole library stands on — the type that
#idx("view")  points at raw bytes, the *view* binding pointer and length into one,
  size calculation that does not wrap, and alignment. Chapter 69's sixth bug (the
  hidden type of bytes) and chapter 36's boundary problem obtain their answer at the
  level of types here.
]

#deepqa[
  Chapter 35 said that `char*` (and `unsigned char*`) alone has the privilege of
  peering into any object byte by byte, and chapter 13 showed code that broke that
  rule quietly collapsing under optimisation. Then what, concretely, does "handling
  bytes safely" keep?
][
  Two things. First, *fix the type you peer with to the one the rule exempts* —
  `unsigned char`. Second, *do not lose the range you are peering into* — carry a
  pointer alone and you forget where your land ends, which is chapter 36's boundary
  trespass. proven's basic vocabulary is these two each made into a type.
]

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

The read-only view looks like this. Two slots of a struct are all of it.

```c
typedef struct {
    const proven_byte_t *ptr;
    proven_size_t        size;
} proven_mem_view_t;
```

The writable edition (`proven_mem_mut_t`) is its twin with only the `const` removed.
What this small struct does is one thing — *making sure the pointer and the length
never part*. It is the answer to the problem chapter 37 called "the real problem of
strings is carrying the length separately".

#demo("examples/ch72/view.c")

`slice 6+4` is this section's heart. From an 8-byte view we asked for 4 bytes from
the 6th, so two bytes are short. The library *does not cut off as much as there is* —
it returns an error (number 2 is `PROVEN_ERR_OUT_OF_BOUNDS`). Giving as much as there
is looks kinder, but then the caller cannot know whether what was received is what
was requested. It is blocking chapter 69's truncation problem from repeating here.

#qa[
  I saw the slicing function in two editions, `_checked` and `_unchecked` — why does
  the latter exist?
][
  For places where the boundary has *already been checked*. Redoing the same check
  every turn inside a loop is waste, so one checks once before the loop and uses the
  unchecked edition inside. That the name carries `_unchecked` matters — the dangerous
  choice can be made only through *a visible name*, and the default is always the
  safe side. It is one form of chapter 45's "write the contract as code".
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
  use after that is the access to a dead automatic variable learned in chapter 39.
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

`PROVEN_MAX_ALIGN` is "the strictest alignment that can hold any type", and
`proven_is_pow2` confirms whether an alignment value is a power of two (alignment
must always be a power of two — only then can the rounding up be calculated with bit
operations).

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
