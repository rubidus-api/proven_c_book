#import "../../book/lib.typ": *

= The map of types — how the standard divides them

#prereq(
  ([chapter 23, Declaring variables], [a declaration — type, name and first value]),
  ([chapter 6, Memory and addresses], [size and alignment]),
  ([chapter 8, Representing numbers], [how integers and floating point are represented]),
)

#deepqa[
  Chapter 23 kept using the word "type" while writing `int x = 0;`. So what
  exactly does a type *settle*? Name three.
][
  Four can be named.

  - *Size* --- how many bytes this value occupies (chapter 6).
  - *Representation* --- through what eye those bytes are read (chapter 7 and 8's
    two's complement and IEEE 754).
  - *Permitted operations* --- what may be done (`%` only on integers, `++` only on
    real types and pointers).
  - *Contract* --- what the compiler checks and what it guarantees.

  This chapter sets out *what branches exist* under that word "type", exactly as
  the standard divides them.
]

#organizer[
#idx("type")  This book has been using words like "arithmetic type", "scalar type",
  "aggregate" and "complete type" already. This is where they get defined. The whole
  classification of §6.2.5 gets built --- object and function, complete and
  incomplete, basic types, integer, real and arithmetic, derived, scalar and
  aggregate, qualifiers --- and on top of it the types of `<stdint.h>` that write
  their width into their name. The next chapter's integers, and the one after that's
  promotion, stand entirely on this vocabulary.
]

#chapter-questions()

== The four things a type settles

Given the same eight bytes, reading them as a `double` gives 3.14 and as a `long`
gives 4614253070214989087 (exactly as chapter 8 showed). Bytes do not know what they
are; *only the type knows.*

#dtable(
  columns: 3,
  [*What a type settles*], [*Example*], [*More*],
  [size], [`sizeof(int)` = 4], [chapter 6],
  [representation], [`-1` as an `int` is `FF FF FF FF`], [chapters 7, 8],
  [permitted operations], [`%` only on integers, `/` on arithmetic types], [chapters 28, 47],
  [contract], [`const` says "I will not change this"], [chapters 23, 49],
)

So "knowing a type" is not knowing syntax but *knowing those four*. And the standard
settles those four branch by branch.

== The first division — object types and function types

The topmost division is in two.

#dtable(
  columns: 3,
  [*Branch*], [*What it is*], [*Examples*],
  [*object type*], [describes a place that holds a value], [`int`, `double`, `struct point`, `int[10]`, `char *`],
  [*function type*], [describes something that works — by return type and parameters], [`int(void)`, `void(int, char *)`],
)

This division shows itself in practice. *`sizeof` cannot be applied to a function
type*, and being no object it has neither size nor alignment. That is why treating a
function as a value means turning it into *a pointer to a function* (chapter 57).

=== Complete and incomplete types

Object types divide again --- *is the size known?*

#dtable(
  columns: 3,
  [*State*], [*What it cannot do*], [*Example*],
  [complete type], [(no restriction)], [`int`, `struct point` once its definition is seen],
  [incomplete type], [`sizeof` is unusable, and no object of it can be made], [an array with no size `int a[]`, a `struct node` declared by tag only],
  [`void`], [an incomplete object type *that cannot be completed*], [its set of values is empty],
)

An incomplete type is not a defect but *a tool*. It can express "I know this type
exists and nothing of its insides", which is what makes an *opaque type* possible ---
hiding the insides in a header and passing only pointers (`FILE *` is the archetype).
Hiding the insides means callers cannot depend on them, and that is the design gain
(chapter 45).

#qa[
  `void` means "nothing at all", so why is it a type?
][
  Because it does three different jobs in three places.

  - *`void f(void)`* --- "there is no value to return" and "there are no parameters".
  - *`(void)printf(…)`* --- a cast that writes down "I am discarding this value".
  - *`void *`* --- "what kind of object it points at is not settled yet". This one
    alone is *a complete object type* (being a pointer, it has a size).

  The standard's definition is "an incomplete object type whose set of values is
  empty and *that cannot be completed*". So no `void` variable can be made and
  `sizeof(void)` is not usable in the standard --- GCC gives 1 as an extension
  (chapter 12's grey area).
]

== The basic types

What the standard calls the *basic types* is exactly these three groups together.

#dtable(
  columns: 2,
  [*What the basic types are (§6.2.5p18)*], [*Note*],
  [`char`], [just one. One of the three character types below],
  [the signed and unsigned integer types], [`short`, `int`, `long`, `long long` and their `unsigned` partners],
  [the floating types], [`float`, `double`, `long double`, and complex],
)

*Enumerated types are not basic types.* They are integer types, but they do not
appear in the list of basic types --- a commonly confused spot.

The standard nails down one more sentence about them: *"even if the implementation
defines two or more basic types to have the same representation, they are
nevertheless distinct types."* The character types of the next section are the
example of that sentence.

=== There are three character types

#dtable(
  columns: 3,
  [*Type*], [*Signedness*], [*Where it is used*],
  [`char`], [the implementation settles it --- *the same range, representation and behaviour* as either `signed char` or `unsigned char`], [for holding characters],
  [`signed char`], [signed], [a small signed integer],
  [`unsigned char`], [unsigned], [*for looking at bytes* (chapter 46)],
)

The heart of it is the sentence a footnote nails down --- *whichever choice was made,
`char` is a separate type from the other two and is compatible with neither.* There
are three, not two.

#misconception[
  "`char` is just `signed char`"
][
  It depends on the platform. On x86 Linux it is signed; on ARM Linux and many
  embedded toolchains it is unsigned. `CHAR_MIN` in `<limits.h>` being 0 or
  `SCHAR_MIN` tells you which.

  The difference bites quietly in one place. Put a byte of 128 or more into a `char`
  and compare, and on the signed side it is *negative*.

  ```c
  char c = 0xFF;
  if (c == 0xFF) { … }      /* does not hold with a signed char */
  ```

  So the discipline is simple --- *`char` for letters, `unsigned char` for bytes,
  `int8_t` for small numbers.* Separate the three uses by name and this trap never
  appears.
]

#platform[
  C23: `bool` is an unsigned integer type
][
  C23's §6.2.5p8 says that *`bool`* together with the unsigned types corresponding to
  the standard signed integer types are collectively the *standard unsigned integer
  types*. So `bool` is an integer type, an arithmetic type, and a scalar.

  The measurement confirms it --- the type of `bool + 0` is `int` (it was promoted).
  All that is special is the value: it is always 0 or 1, since putting any scalar into
  a `bool` gives 0 for zero and 1 for anything else. C99's `_Bool` gained the keyword
  `bool` in C23, and `<stdbool.h>` became a header you may or may not include
  (chapter 79).
]

== Integer, real, arithmetic — the collective names

From here comes the source of the vocabulary this book has been using. These names
are not *branches* of the tree but *words that gather several branches.*

#dtable(
  columns: 3,
  [*Name*], [*What it gathers (§6.2.5)*], [*Where the name is used*],
  [integer types], [`char` + signed integer + unsigned integer + *enumerated types*], [the operands of `%` and `<<`],
  [real types], [integer types + *real* floating types (complex excluded)], [the operands of `++` and `--` (chapter 47)],
  [arithmetic types], [integer types + floating types (complex included)], [the operands of `+`, `-`, `*`, `/`; what promotion acts on],
  [scalar types], [arithmetic types + pointers + *`nullptr_t`* (C23)], [the condition of `if` and `while`, the operand of `!`],
  [aggregate types], [array + struct --- ★*not union*], [the rules for initializer lists],
)

In chapter 47's tables of operator contracts you will meet cells such as "operand: a
real type or a pointer", and now they can be read exactly.

#misconception[
  "A union is an aggregate too"
][
  Not in C. Section 6.2.5p26 says only *"array and structure types are collectively
  called aggregate types"*. The union is missing.

  The reason is that "aggregate" means *holding several things at once*. A union has
  only one member alive at a time (chapter 46's active member), so it does not fit
  that definition.

  *C++ differs* --- there a union that meets the conditions is an aggregate. The
  difference shows when comparing the two languages' initialisation rules.
]

#figure-svg("type-tree", caption: [How the standard divides types. A dashed box is a name that gathers several branches.])

== Derived types — made out of what is there

New types can be *constructed* from object and function types, and what is so
constructed is a *derived type*.

#dtable(
  columns: 3,
  [*Derivation*], [*From what to what*], [*More*],
  [array], [from element type T to "array of T"], [chapter 38],
  [structure], [holding several types *in sequence*], [chapter 44],
  [union], [holding several types *overlapping*], [chapter 46],
  [function], [from return type T to "function returning T"], [chapter 24],
  [pointer], [from referenced type T to "pointer to T"], [chapter 35],
  [atomic], [`_Atomic(T)` --- a conditional feature], [chapter 77],
)

*These constructions apply recursively.* "An array of 10 pointers to int" and "a
pointer to a function returning int" are both built that way. That recursion is
exactly why chapter 58, "Reading declarations", is hard, and the standard gathers
three of them --- *array, function and pointer* --- under the name *derived declarator
types*. Those three are precisely what must be unwrapped from the inside out when
reading a declaration.

== Qualifiers — a qualified edition of the same type

`const`, `volatile` and `restrict` do not make new types; they make *qualified
versions*.

#dtable(
  columns: 3,
  [*Qualifier*], [*What it promises*], [*More*],
  [`const`], [I will not change it through this name], [chapter 23],
  [`volatile`], [it may change without my knowing, so do not optimise it away], [chapters 13, 75, 77],
  [`restrict`], [this object is reached only through this pointer], [chapter 38],
)

A qualified type and an unqualified one have *the same size, representation and
alignment* --- what changes is only *what may be done*. So `const int` is the same
four bytes as `int`, and adding `volatile` does not make a value bigger.

#demo("examples-en/ch26/type_map.c")

`_Generic` is the device that picks, *at compile time*, on "what is this expression's
type" (C11). Instead of learning the classification in words, we asked the compiler
directly. Three things in the output are worth pointing at.

- *`uint8_t` came out as `unsigned char`* --- an alias, not a new type.
- *The constant `RED` is `int` while the enum variable is `unsigned int`* --- which
  integer type an enumeration is paired with is implementation-defined.
- *Both `char + 0` and `uint8_t + 0` are `int`* --- the promotion of the
  chapter-after-next showing its face early.

== Types with their width in the name — `<stdint.h>`

That was the types the language gives; now the ones *the standard library names for
us*. They are the direct answer to the problem that `int`'s size differs by
implementation (the next chapter).

#demo("examples-en/ch26/stdint_kinds.c")

=== What separates the three families is "what they demand"

#dtable(
  columns: 4,
  [*Family*], [*Names*], [*What it guarantees*], [*Is it there?*],
  [exact width], [`int8_t`, `uint32_t`, …], [*exactly* N bits, *no padding bits*, two's complement], [*optional* --- defined only by implementations that have such a type],
  [minimum width], [`int_least8_t`, `uint_least32_t`, …], [the smallest with *at least* N bits], [8, 16, 32, 64 are *required*],
  [fastest], [`int_fast8_t`, `uint_fast32_t`, …], [*usually the fastest* with at least N bits], [8, 16, 32, 64 are *required*],
  [holding a pointer], [`intptr_t`, `uintptr_t`], [a `void *` put in and taken back out compares equal], [*optional* (chapter 35)],
  [widest], [`intmax_t`, `uintmax_t`], [holds the value of any integer type], [*required*],
)

The measurement shows the difference --- on this machine `uint_fast32_t` was *eight
bytes*. Thirty-two bits would have sufficed and it took sixty-four; that is what
"fast" means (matching the register width is faster). *It is not a type for saving
space.*

#qa[
  "Optional"? Is there really a machine without `uint32_t`?
][
  Rare, but yes. An exact-width type demands a type with *no padding bits*, and some
  DSPs have a 16-bit `char` or padding bits in their integers, so they can offer no
  type that is "exactly 8 bits". Such an implementation *does not define* `uint8_t`.

  Practice judges it this way. If you face only desktops, servers and mainstream
  embedded targets, use the exact-width types freely (rungs 1–2 of chapter 12's
  ladder). If you write a library facing *every* C implementation, use the
  minimum-width types and, where exact width is genuinely required, let the build say
  so with something like `static_assert(sizeof(uint8_t) == 1)` (rung 3).
]

=== The three traps of `uint8_t`

The most used, and the most injuring, so it gets its own treatment.

#antipattern[
  1. A number goes in and a letter comes out
][
  ```c
  uint8_t age = 65;
  printf("%c\n", age);      /* 'A' comes out */
  putchar(age);             /* the same trap */
  ```

  `uint8_t` is usually an *alias* for `unsigned char`, so it slots straight into a
  place expecting a character. The types match, so the compiler says nothing.

  *`uint8_t` is safer used as "a byte" than as "a small number".* For a small number
  a person will read, use `int` or `int16_t`; if it must be printed, use `%u` (it is
  promoted, so it arrives as `unsigned`) or `PRIu8`.
]

#antipattern[
  2. Thinking it is 8-bit arithmetic
][
  ```c
  uint8_t a = 200, b = 100;
  a + b     /* 300, not 44 */
  ```

  The measurement shows it. `uint8_t` is narrower than `int`, so before arithmetic it
  is *promoted to `int`* (the chapter after next). The computation happens in 32 bits,
  and to make it wrap you must *put it back* --- `(uint8_t)(a + b)` is 44.

  This is a safeguard rather than a defect: multiply two narrow types and the
  intermediate result is not cut off. Only *the expectation that "I used an 8-bit
  variable so it will compute in 8 bits" must be abandoned.*
]

#antipattern[
  3. Thinking it is a new type
][
  A `typedef` makes an alias, not a new type (chapter 58). So `uint8_t` and
  `unsigned char` are *the same type*, and neither `_Generic` nor overloading can tell
  them apart. The measurement's first output is the evidence.

  For the same reason the compiler will not catch this.

  ```c
  void send(uint8_t port, uint8_t value);
  send(value, port);        /* swapped, and it stays silent */
  ```

  To distinguish them by type, *wrapping in a struct* is C's way ---
  `struct port { uint8_t v; };`. The price is clumsier syntax; what you buy is the
  compiler's checking.
]

=== So which do you use

#dtable(
  columns: 3,
  [*In this place*], [*use this*], [*why*],
  [protocols, file formats, hardware registers], [exact width `uint32_t`], [the byte count is the contract (chapter 46)],
  [portable code facing every implementation], [minimum width `uint_least16_t`], [exact width is optional],
  [loop counters, local computation], [fastest `uint_fast32_t`, or simply `int`], [where speed is worth more than width],
  [sizes, indices, byte counts], [`size_t` (`<stddef.h>`)], [it is `sizeof`'s type and covers any array],
  [the difference of two pointers], [`ptrdiff_t` (`<stddef.h>`)], [a sign is needed (chapter 38)],
  [a small number a person will read], [`int`], [it gets promoted to `int` anyway],
)

Emphasise that last row as practice's default. *With no particular reason, use
`int`.* The types with a width in their name are a tool for *places where the width
is the contract*, not something good to sprinkle about for thrift.

#realcase[
  Where does `size_t` live?
][
  A frequently confused spot, so it is written down. `size_t` and `ptrdiff_t` belong
  not to `<stdint.h>` but to **`<stddef.h>`**. `<stdint.h>` is the home of the types
  *with a width in their name*; `<stddef.h>` is the home of the types *the language
  itself uses* (`size_t`, `ptrdiff_t`, `nullptr_t`, `max_align_t`).

  In practice another header such as `<stdio.h>` often drags `size_t` in for you,
  which blurs the distinction --- and *the day that header changes, it breaks.* The
  discipline is to include the header that defines the type you use (the same grain
  as chapter 53's story about names).
]

== Summary — the standard's words and this book's chapters

#dtable(
  columns: 3,
  [*The standard's word*], [*What it is*], [*The chapter that faces it*],
  [object type / function type], [what holds a value / what works], [here, chapters 24, 57],
  [complete / incomplete type], [is the size known], [here, chapter 45],
  [basic types], [`char` + integer + floating], [here, chapters 27, 47],
  [character types], [the three `char`, `signed char`, `unsigned char`], [here, chapters 9, 40],
  [integer types], [`char` + integer + enumerated], [chapter 27],
  [real types], [integer + real floating], [chapter 47],
  [arithmetic types], [integer + floating], [chapter 29 (promotion)],
  [derived types], [array, struct, union, function, pointer, atomic], [chapters 35–38, 44–46],
  [scalar types], [arithmetic + pointer + `nullptr_t`], [chapters 30, 36],
  [aggregate types], [array + struct], [chapters 38, 44],
  [qualified types], [`const`, `volatile`, `restrict`], [chapters 23, 38, 75],
)

The map is open. From the next chapter we dig into its cells one at a time --- first
*integers*. The family passed over here in the single phrase "the signed and unsigned
integer types" gets its insides, its ranges, its representation and its accidents.
