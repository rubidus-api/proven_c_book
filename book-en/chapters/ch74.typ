#import "../../book/lib.typ": *

= Strings and text

#organizer[
  The answer to chapter 69's first bug — string functions that do not know the size
  of the vessel. Strings that carry their length, the distinction between owning and
  borrowing, and this library's most contentious decision, *refuse rather than
  truncate*. Chapter 9's story of encodings and chapter 37's story of boundaries
  gather here into one.
]

#deepqa[
  Chapter 37 said that for a C string "up to the NUL" is the length, so to know the
  length it must be counted every time. Then what improves if the length is carried
  along — and what is lost?
][
  Three things are gained. The cost of counting the length vanishes ($O(n)$ becomes
  $O(1)$), data with a zero byte in the middle can be handled, and above all *the
  boundary can be checked* — without the length there is nothing to check. Two things
  are lost. One string becomes two words rather than one, and conversion becomes
  necessary when meeting a world that expects NUL termination, as `printf("%s")` does.
  This library keeps a NUL internally as well in order to remove that conversion cost
  — a compromise that has both.
]

== Two types, one rule

The string vocabulary is only two.

- `proven_u8str_t` — an *owning* string. It has a buffer, a length and a capacity.
- `proven_u8str_view_t` — a *borrowed* string. Only a pointer and a length.

If the name has `view` in it, it is borrowed, and what is borrowed is not destroyed.
The rule set up in chapter 73 applied to strings as it is. `u8` means UTF-8 — that
encoding seen in chapter 9 is this library's default text representation.

Open them up and why they were divided in two becomes clear.

```c
/* the borrowed string — the same shape as chapter 72's mem_view_t */
typedef struct {
    const proven_byte_t *ptr;
    proven_size_t        size;
} proven_u8str_view_t;

/* the owning string — one buffer and an "is it borrowed" flag */
typedef struct {
    proven_buf_t internal;   /* { ptr, len, cap } */
    bool         borrowed;
} proven_u8str_t;
```

The three slots of `proven_buf_t` state the string's character as they are. *`len` is
the number of bytes of content held now*, *`cap` is the whole buffer's size*, and the
difference between them is the spare room including the NUL's place. So
`create(alloc, 64)` really takes 65 bytes — 64 is the limit of the *content* and one
byte is the NUL's share. Thanks to that one byte `proven_u8str_as_cstr` can hand out a
C string with neither copying nor allocation.

The `borrowed` flag marks a string made with `_borrow`. When this flag is on,
`_destroy` *releases nothing* and merely empties the struct — somebody else's memory
cannot be given back.

There are three roads to obtaining a string.

#dtable(
  columns: 4,
  [*function*], [*allocates*], [*destroy*], [*where it is used*],
  [`proven_u8str_create(alloc, limit)`], [yes], [`_destroy` needed], [starting empty and filling it],
  [`proven_u8str_create_from_view(alloc, v)`], [yes], [`_destroy` needed], [owning a copy of existing content],
  [`proven_u8str_borrow(buf, cap)`], [no], [unnecessary (harmless)], [over a stack or static array. embedded],
)

Only beware that `_borrow`'s `cap` is *the whole capacity including the NUL* — give it
`buf[64]` and the content goes to 63 bytes.

Making a view from a literal is done with one macro.

```c
PROVEN_LIT("hello")        /* becomes { ptr, 5 } at compile time — no strlen */
proven_u8str_view_from_cstr(p)   /* counts the length at run time */
```

`PROVEN_LIT` draws the length from `sizeof("...") - 1`, so its *run-time cost is zero*.
Using it for string literals is the practice, and `_from_cstr` for a C string whose
length is unknown.

#demo("examples/ch74/ops.c")

The first line shows chapter 73's rule again. `proven_u8str_borrow` has no allocator —
therefore it does not allocate. It is doing string operations on a 16-byte array taken
on the stack, and this is exactly how strings are handled in embedded work without a
heap.

== Refuse, rather than truncate

The second `append` is the most important single line of this part. On trying to
attach `" world, and more"` after `"hello"` in a 16-byte vessel, the library *wrote
nothing* and returned `PROVEN_ERR_OUT_OF_BOUNDS`. And as the next line shows, the
original content `hello` stands as it was — the failure atomicity learned in
chapter 71.

It is the exact opposite of `snprintf`'s choice seen in chapter 69. Why not truncate?

*Because a truncated value is not a short value but a different value.* A truncated
path points at a different file, a truncated command is a different command, a
truncated user name is a different person. Truncate and declare success and the caller
cannot know whether what was received is what was requested. So this library *gives
the decision back to the caller* — "there is not enough room. What shall we do?"

#qa[
  Still, are there not places where truncation is fine, such as one line of a log?
][
  There are. So there is a separate function that *explicitly writes only part* —
  `proven_u8str_append_partial` returns the number of bytes that went in. That the
  name is long and the return value different is the heart of it. Truncation is still
  possible, but to do it you must *write it that way*. The principle that the default
  is always the safe side and the dangerous choice can be made only through a visible
  name (the same as chapter 72's `_unchecked`) is kept here too.
]

#antipattern[
  Confusing growth with fixed capacity
][
  ```c
  proven_u8str_t s = proven_u8str_borrow(buf, sizeof buf);
  proven_err_t e = proven_u8str_append_grow(alloc, &s, view);  /* dangerous */
  ```
  `_append` writes only within the capacity, while `_append_grow` asks the allocator
  for more if there is not enough — which is why only the latter has an allocator
  argument. The problem is demanding growth on a borrowed buffer (`_borrow`). A stack
  array cannot grow, so this is a design error.

  In this case the library *does not quietly reallocate somebody else's memory* but
  returns `OUT_OF_BOUNDS` — it succeeds while things fit and refuses the moment they
  would not. The habit of reading signatures becomes the defence here as it stands —
  *with an allocator it can grow, without one it cannot.*
]

== Three kinds of writing — refuse, truncate, grow

This library's string operations are made so that the kind can be known from the name
alone. That kind is "what happens when there is not enough room".

#dtable(
  columns: 4,
  [*kind*], [*shape of the name*], [*when short*], [*failure atomicity*],
  [fixed capacity, atomic], [`_append`, `_insert`, `_replace_at`], [refuses (`OUT_OF_BOUNDS`)], [yes — the original stands],
  [best effort, truncating], [`_append_partial`, `_append_fmt_trunc`], [writes as much as fits and reports], [no (deliberately)],
  [growing], [`_append_grow`, `_insert_grow`, `_replace_at_grow`], [asks the allocator for more], [yes — the original stands if allocation fails],
)

All three kinds are needed because the right answer differs by place. Where a file path
is being built truncation must not happen, so *refusing* is right; where a log line is
fitted to the screen *truncating* is right; where the length is unknown *growing* is
right. That the default (the short name) is refusal shows this design's attitude.

== The operations that mend a string

Appending alone is not enough. There are operations that mend the middle, delete, and
rewrite.

#demo("examples/ch74/edit.c")

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*contract*],
  [`_insert(&s, i, v)`], [inserts at position `i`], [`i` ≤ length. it pushes the tail out],
  [`_remove(&s, i, n)`], [deletes `n` bytes from `i`], [an error if it exceeds the range],
  [`_replace_at(&s, i, old, v)`], [a range with other content], [the lengths may differ],
  [`_replace_first(&s, off, t, r)`], [the first `t` found, into `r`], [★ if absent it returns *success*],
  [`_reset(&s)`], [empties only the content], [the buffer and capacity stand],
  [`_reserve(alloc, &s, n)`], [capacity up to `n` in advance], [it pays especially on an arena],
  [`_append_byte(alloc, &s, b)`], [appends one byte], [grows if needed],
)

The starred contract of `_replace_first` needs care. *Not finding it is not an error but
a success* — to distinguish "there was nothing to replace" from "it was replaced" you
must first check with `proven_u8str_view_find`. That the example's "replacing what is
not there" comes back as `err=0` is that confirmation.

`_reset` and `_reserve` are a pair for performance. In code that builds a string afresh
every frame or per request, *reusing the buffer instead of throwing it away* is
`_reset`, and taking it in advance when you roughly know how large it will grow is
`_reserve`. As seen in chapter 73, `_reserve` pays especially on an arena — take it
large before another allocation intervenes and growth in place becomes possible.

#qa[
  Which should be the default, `_insert` or `_insert_grow`?
][
  *`_insert` where I settle the capacity*, *`_insert_grow` where I do not know how much
  content there will be*. The criterion is simple — "is running short here *a bug*, or
  *a thing that can happen*?"

  If a protocol header is being assembled in a fixed-size buffer, running short is a bug,
  and refusal is right there (and that error reveals the bug). If user input is being
  appended, it can grow to any length, so growing is right. It is also why embedded code
  uses only the editions without `_grow` — there, *unpredictable growth itself is
  forbidden* (chapter 78).
]

== Finding and cutting — text handling without copying

The example's ③ and ④ are a view's real usefulness. Obtaining a substring needs no
copying — it merely calculates a new pointer into the original and a new length.
While one CSV line was divided into three fields, not one allocation happened.

This pattern is especially powerful in parsers. When parsing a line with `sscanf` in
chapter 25 a buffer had to be prepared in advance to hold the result, whereas cutting
with views leaves only *marks upon the original*. In exchange one more thing must be
kept — as seen in chapter 72, *it is valid only while the original is alive*.

There is a convention too in the value `find` returns when it does not find. It is
not 0 or a negative number but a sentinel with a name, `PROVEN_INDEX_NOT_FOUND`.
Chapter 69 spoke of the danger of sentinel values, and what differs here is that *it
has a name and is documented* — a nameless magic number and a named contract are
different things.

#misconception[
  "Carrying the length means knowing the number of characters"
][
  No. The length is *the number of bytes*. As learned in chapter 9, one character in
  UTF-8 is 1\~4 bytes, so the Korean "가" is 3 bytes and an emoji is 4. It is why the
  example's output takes the trouble to state "(5 bytes)". So handling "the nth
  character" is still delicate, and *cutting anywhere at a byte boundary* gives the
  broken characters seen in chapter 9. What a string library solves is boundary
  trespass, not the essential difficulty of encodings.
]

== The boundary of two worlds — NUL termination and UTF-16

Conversion is needed at every place where the outside world is met.

- `proven_u8str_as_cstr` — obtains a NUL-terminated pointer from an owning string.
  A NUL being kept internally, there is neither copying nor allocation. It is used
  when passing to `printf("%s")` or to a file API.
- `proven_u8str_view_to_cstr` — makes a NUL-terminated string from a view. This one
  *takes an allocator* — because a view may point into the middle of the original and
  a NUL cannot be written in that place. The signature states the fact once again.
- `proven_u16str_t` / `proven_u16str_view_t` — the bridge to the UTF-16 world. The
  Windows API uses this encoding so conversion is needed, and conversion can fail
  (unpaired surrogates and the like). So the result comes as a bundle.

#realcase[
  "Do not guess, refuse" — the principle of handling encodings
][
  The root of the text security problems seen in chapter 9 is mostly *the lenient
  decoder*. Implementations that "read a wrong UTF-8 as something similar" have
  several times been the passage to security incidents — decoders that permitted
  overlong encodings in particular had their checks bypassed. So today's norm is one
  sentence. *Do not read invalid input as something mended; refuse it.* proven's
  encoding conversion stopping with `PROVEN_ERR_INVALID_ENCODING` is that norm
  implemented, and this principle is exactly the same spirit as this chapter's "do not
  truncate".
]

#recap[
  The string vocabulary in summary.

  #dtable(
  columns: 3,
    [*function*], [*what it does*], [*failure and ownership*],
    [`u8str_create(alloc, cap)`], [create an owning string], [returns a bundle, needs `_destroy`],
    [`u8str_borrow(buf, cap)`], [a string over somebody's buffer], [no allocation, no destruction],
    [`u8str_append(&s, v)`], [append within the capacity], [refuses if short (the original is preserved)],
    [`u8str_append_partial`], [as much as fits], [returns the number of bytes put in],
    [`u8str_append_grow(alloc,…)`], [grow it if short], [allocation may fail],
    [`u8str_view_find`], [the position of a substring], [`PROVEN_INDEX_NOT_FOUND` if absent],
    [`u8str_view_slice`], [a sub-view], [no copying — tied to the original's lifetime],
    [`u8str_as_cstr`], [a NUL-terminated pointer], [no copying],
    [`u8str_view_to_cstr`], [view → a C string], [needs an allocator],
)
]

We can now hold and cut strings safely. But the work of *making* them remains —
turning numbers and values into letters, and turning letters back into numbers. It is
the place where chapter 69's third bug waits, and the place where this library's most
conspicuously different syntax appears.
