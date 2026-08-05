#import "../../book/lib.typ": *

= Unions and representation

#organizer[
#idx("union")  The device for seeing the same memory through a different eye —
  the union. And this is a chapter of representation too: we run the endianness
#idx("padding")  demonstration booked in chapter 5 and confirm with our own eyes
  the hidden gaps (padding) in a struct. It is where this book's refrain, the
  separation of representation from abstraction, rings out loudest for the last
  time.
]

#deepqa[
  Chapter 13 said the old technique of "reading a float's bits through uint32's
  eye" violates strict aliasing, and that the correct methods are memcpy or a
  union. Then what exactly is a union, that it stands in that place?
][
  *A type whose several members share the same memory.* If a struct lays members
  side by side (chapter 41), a union lays them *overlapping* — its size fits the
  largest member, and at any moment only one thing is really held. Chapter 5's
  perspective, "seeing the same bits through this eye and through that", made into
  syntax.
]

== The union — laying things over one another

The syntax is a twin of the struct's. Change `struct` to `union`:

```c
union bits32 {
    uint32_t as_int;
    float    as_float;
};
```

Member access is the same (`u.as_int`). The only difference is the layout of
memory — the two members share the same four bytes, so write to `as_float` and
read `as_int` and you see *the same bits under a different interpretation*. The C
standard permits this "read through a member other than the one written" (type
punning) for unions in particular — unlike chapter 13's pointer-cast approach, it
is inside the contract, which is the decisive difference (though the caution
remains that the value read may not be a valid value of that type).

== Representation with our own eyes — endianness and padding

Chapter 5 booked "actually doing this check in C is this chapter's
demonstration." Now we pay. Here, instead of a union, we use the most portable
method — the eye of bytes (`unsigned char`) learned in chapter 35, and `memcpy`.

#demo("examples/ch43/endian.c")

The first part is exactly chapter 5's picture. `0x12345678` sits in memory in the
order `78 56 34 12` — meaning this book's verification machine is little-endian,
and chapter 5's diagram is confirmed in the flesh. (Run this example on a
big-endian machine and `12 34 56 78` is printed and the verdict sentence changes
— the code stays the same.)

The second part is the struct's hidden circumstances. A struct holding one `char`
(1 byte) and one `int` (4 bytes) has size 8, not 5 — because chapter 6's
alignment rule inserted three bytes of *padding* after the `char`. The int member
must start at a multiple of four for the machine to grab it in one handful
(chapter 6). So one practical habit follows — *lay the large members first and
the gaps shrink.* In code handling millions of structs this one layout decision
governs memory and cache efficiency (chapter 11). The layout rules, and the ways
to remove gaps or force alignment (`pack`, `alignas`), were treated in detail in
chapter 42 — the purpose here is to confirm with our own eyes that the gap
*really exists*.

#realcase[
  The secret spilled by a gap — padding information leaks in kernels
][
  Padding looks harmless, being empty space nobody uses, but through the eye of
  security it is *uninitialised memory*. When an operating system kernel copies a
  struct whole to a user program, those gaps cross over too — and although every
  member was filled in, the gaps contain *the remains of other data* that happened
  to be there. An attacker can gather these crumbs to glimpse the contents or
  address layout of kernel memory, and that becomes the foothold for the next
  attack. Major kernels including Linux have fixed dozens of information-leak
  vulnerabilities of this class, and today's response is simple — a struct handed
  to userspace is *wiped to zero whole before its members are filled*. It is the
  moment chapter 23's rule of "initialise at the point of declaration" extends
  even to invisible blanks.
]

#misconception[
  "You can just write a struct to a file or send it over a network as it is"
][
  A tempting thought, and one much attempted — storing a struct's bytes whole
  makes the code short. But the two facts this chapter has just shown block it:
  byte order differs by machine (endianness), and the size and position of the
  gaps differ by compiler and platform (padding). A file written on one machine
  breaks on another — chapter 5's NUXI incident reproduced in the world of file
  formats. The right answer is *serialisation*: writing explicit code that writes
  and reads members one at a time, in an agreed size and byte order (network byte
  order — chapter 5). Representation is the machine's business and files and
  communication are a world of agreements — the bridge between the two worlds must
  be laid by hand.
]

#qa[
  When, then, is a union the standard thing to use?
][
  In two places. First, the *looking into representation* (type punning) just
  seen — low-level code inspecting floating-point bits or viewing a hardware
  register through several eyes. Second, and more common, the *tagged union*:
  putting into a struct both a union and a mark (a tag) saying "which member is
  valid now", to represent alternative data such as "this value is an integer, or
  a real number, or a string." It is the basic tool of interpreters' value
  representations and configuration-file parsers — and chapter 6's tagged pointer
  was the same idea at the bit level. Modern languages' enumerations (Rust's enum,
  Swift's associated values) lifted this pattern to the level of the language.
]

== Bit fields — cutting up one word

Write a colon and a number after a struct member and it becomes a *bit field* —
you specify directly how many bits that member occupies. Overlay a union on that
and you have both "the eye that sees it whole as one word" and "the eye that sees
it divided into fields" at once.

#demo("examples/ch43/bitfield.c")

The first part is the typical pattern for handling a hardware register. Write to
a field as in `r.f.mode = 5` and the value goes into the bit positions without
library help, and reading `r.raw` shows the result as one word. The reverse —
writing `r.raw` whole and reading the fields — works too; the output's third line
is the check.

Convenient though it looks, *the price in portability* is large, because much is
not fixed by the standard.

- *The order the bits are laid in* — whether they fill from the low end or the
  high end is implementation-defined. So the same declaration may produce
  different layouts on different compilers.
- *Fields crossing a boundary* — whether a field straddling a storage unit is
  allowed is also up to the implementation. So is how much padding is inserted.
- *Sign* — a plain `int x : 1;` is a signed one-bit field, so its values are 0 and
  −1. If that was not the intention, `unsigned` must be stated.
- *Its address cannot be taken* — `&` cannot be applied to a bit field. Nor can
  they be made into an array.
- *It is not atomic* — if two threads touch two fields sitting in the same word,
  an accident of the same family as chapter 12's false sharing occurs.

#misconception[
  "Bit fields can represent a file or network format directly"
][
  The commonest misunderstanding, and a fixture of portability accidents. A file
  format or protocol has *the layout of its bytes and bits fixed by
  specification*, whereas a bit field's layout is fixed by the implementation.
  Change compiler or move to another machine and the fields are read at the wrong
  places — worse still when endianness (the previous section) is layered on. The
  proper method for an external format is *laying out a byte array and extracting
  directly with shifts and masks* (chapter 7). Regard bit fields strictly as *a
  way of saving memory within one program*.
]

That is why bit fields are not recommended today. The reason to know the syntax
nonetheless is clear — you still meet them in the register definitions of embedded
SDKs, in the flag bundles of old codebases, and in kernel data structures.
*Be able to read them, but think twice before writing new ones* is the practical
instinct.

== The practical pattern of mixing structs and unions

The latter part of the example is a different story. It is a *tagged union* — a
struct holding a tag and a union together — the pattern named in the exchange
above.

```c
struct message {
    enum msg_kind kind;      /* the tag telling which eye to look with */
    unsigned      flags : 4; /* small states — bit fields earn their place here */
    unsigned      urgent : 1;
    union {                  /* an anonymous union (C11) */
        int  number;
        char text[16];
        struct { int x, y; } point;
    };
};
```

Three things are layered here. The tag, the state flags saved by bit fields, and
an *anonymous union* (C11). With no name, members can be used one step more
directly, as `m->number`, which makes a tagged union far more readable.

There is only one discipline and it is everything — *read only the member the tag
says.* If `kind` is `MSG_TEXT` and you read `number`, it becomes the "looking
through another eye" of this chapter's first section and gives a meaningless
value. So code handling such data is almost always made to pass through *a single
`switch` on the tag*, like the example's `show`. Gather the access in one place
and the place to keep the discipline is one place too.

That `sizeof(struct message)` came out as 24 bytes is worth reading as well — a
4-byte tag plus the word holding the bit fields plus the 16-byte union, with
padding (the previous section) added for alignment. Representation always takes
*a little more* than what was declared.

== Closing Part VIII

We have the two syntaxes for making types — the struct that lays things side by
side (chapter 41) and the union that lays them over one another (chapter 43). And
along the way we confirmed the realities of representation (endianness, padding)
with our own eyes. Part II's background knowledge has been fully collected into
syntax.

The next part is the part of precision — chapter 8's mathematics of approximation
comes down into C's floating types (chapter 44), the perspective of the contract
whose seed was planted in chapter 32 grows into error handling (chapter 45), and
we meet head on the world "outside the contract" that this book has foreshadowed
throughout — undefined behaviour (chapter 46).
