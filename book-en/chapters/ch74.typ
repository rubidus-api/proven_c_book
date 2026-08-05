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

There are two roads to obtaining an owning string. Receiving it from an allocator
(`_create`), or borrowing somebody's buffer (`_borrow`).

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
  array cannot grow, so this is a design error. The habit of reading signatures
  becomes the defence here as it stands — *with an allocator it can grow, without one
  it cannot.*
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
