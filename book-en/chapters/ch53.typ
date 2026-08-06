#import "../../book/lib.typ": *

= Variadic functions

#prereq(
  ([chapter 22, Output], [printf, a variadic function]),
  ([chapter 28, Implicit conversions], [the default argument promotions]),
)

#deepqa[
#idx("variadic arguments")  Chapter 28 taught that values crossing into variadic
  arguments undergo default promotions (float→double, small integers→int). But why
  is there a special promotion rule in that place alone?
][
  *Because the receiving side does not know the types.* An ordinary function has
  its argument types written in its prototype (chapter 24) and the compiler makes
  the two sides match. But the variadic position has only `...` in the prototype,
  so there is nothing to match against — hence the agreement "let us at least send
  everything unified into larger types" to reduce the confusion. This chapter is
  the story of how values are taken out at that "place that does not know types",
  and what it costs.
]

#organizer[
  We open the secret of how `printf` could take any number of arguments. The four
  tools of `<stdarg.h>`, why this device is not type-safe, its history (from K&R's
  `varargs` through C89's `stdarg` to C23's tidying), and today's alternatives.
  Why chapter 22's "the format is a contract" was such a strict contract is
  revealed here.
]

#chapter-questions()

== The four tools

What `<stdarg.h>` gives is one type and three macros — `va_list` (the reader),
`va_start` (begin), `va_arg` (take one out), `va_end` (close). Seen in a
demonstration.

#demo("examples-en/ch53/va.c")

The key is the second argument of `va_arg(ap, int)` — *the programmer states the
type of the value to take out.* The function does not know what arrived, so
somebody must say "what I take out now is an int", and that somebody is the
author who shares the agreement with the caller. Break the agreement — take out an
int when a double really arrived — and it is outside the contract (chapter 49).
It reads the wrong place in memory, so the value becomes rubbish or every
subsequent extraction is thrown out of step.

And the function *does not know how many arguments there are* either. So a way to
learn the count must be made into a calling convention, and the demonstration
shows two practices — receiving the count as the first argument (`sum_n`), or
agreeing on a marker value announcing the end (`sum_until_zero`). `printf` uses a
third road: *the format string is a specification announcing both count and
types.* That is the exact weight of chapter 22's "the format is a contract" —
break that contract and printf goes rummaging through memory taking out arguments
that do not exist (chapter 22's format string vulnerability was exactly this
mechanism).

== Default argument promotions — values crossing `...` get fatter

There is one more reason `va_arg` asks for a type. An argument crossing `...`
*does not go as its original type*. The default argument promotion seen in
chapter 28 really happens here.

#dtable(
  columns: 3,
  [*what you passed*], [*what actually arrives*], [*so*],
  [`char`, `signed char`, `unsigned char`], [`int` (★)], [`va_arg(ap, char)` is wrong],
  [`short`, `unsigned short`], [`int` (★)], [take it out with `va_arg(ap, int)`],
  [`bool`], [`int`], [the same],
  [`float`], [`double`], [`va_arg(ap, float)` is wrong],
  [integers at least as wide as `int`], [unchanged], [—],
  [pointers], [unchanged], [but a null constant needs a cast],
)

The rule in one line — *integers narrower than `int` undergo integer promotion, and
`float` becomes `double`.* That is why `printf` has no float-specific format
(chapter 56), and why writing `va_arg(ap, float)` is a contract violation, taking
out a type that never arrived.

One qualification attaches to the star in the table. The result of integer promotion
is *almost always* `int`, but exactly it is "`int` if an `int` can represent every
value of the original type, otherwise `unsigned int`" (chapter 28). On today's
mainstream machines an `unsigned short` also fits in an `int` and so becomes `int`, but
on an implementation where `short` and `int` have the same width an `unsigned short`
promotes to `unsigned int`. Take it out there with `va_arg(ap, int)` and the signedness
goes out of step — *the exact rule is that you must use the type actually promoted to.*

The trap when passing a null pointer comes from here too. On an implementation
where `NULL` is defined as the integer `0`, an integer 0 rides into the `...`
position, and if the receiving side takes it out with `va_arg(ap, char*)` the
widths are out of step. That is why C23's `nullptr` (chapter 35) or an explicit
cast (`(char *)0`) is used.

== The fundamental limit — the function knows nothing

A variadic function *knows neither the number nor the types of the arguments that
arrived.* The values are simply placed, with no information accompanying them to
explain what they are. So every variadic function must obtain that information
*from outside*, and there are only three ways in the end.

#dtable(
  columns: 3,
  [*method*], [*example*], [*how it breaks*],
  [a specification string tells it], [`printf("%d %s", ...)`], [format and arguments out of step is UB. if the format is a variable, no check is possible],
  [count and types are taken as arguments], [`sum_n(3, a, b, c)`], [miscount and it takes out arguments that are not there],
  [an end marker is agreed], [`execl(..., (char *)NULL)`], [omit the marker and it does not stop],
)

All three share the property that they hold only *if a human keeps the
agreement*, because the information for a compiler to check simply does not
exist. That is why chapter 74 counts this among "the five bugs C has been
shipping for fifty years."

== `_Generic` — catching the type at compile time

C11 added one tool for this problem. `_Generic` is the syntax that *chooses one of
several options at compile time according to an expression's type*.

```c
#define TYPE_NAME(x) _Generic((x), \
    int:          "int",           \
    double:       "double",        \
    const char *: "string",        \
    default:      "other")
```

Three things to watch. First, the criterion for choosing is *the type*, not the
value. Second, branches not chosen *are not even compiled* — so code valid only
for one type can sit beside code valid only for another. Third, giving a type not
in the list is *a compile error* unless there is a `default` — and that property
is the heart of the next section.

#demo("examples-en/ch53/generic.c")

The latter part of the example is this chapter's conclusion. `ARG(x)` uses
`_Generic` to learn the value's type and makes *a struct carrying the value
together with a type tag*. Putting those in an array to pass means the function
learns the count too — variadic arguments are not used at all. With no format
string there is no place for format and arguments to disagree.

== proven's `PROVEN_ARG` — the structure in the flesh

The `proven_println("{}", PROVEN_ARG(x))` used in chapter 80 is exactly this
structure. Taken apart, the real implementation is three pieces.

*① A bundle carrying a type tag and a value.* The library represents one argument
like this — an enumerated value saying what kind it is, and a union holding the
value itself (exactly chapter 45's tagged union).

```c
typedef enum {
    PROVEN_ARG_NONE, PROVEN_ARG_I32, PROVEN_ARG_U32,
    PROVEN_ARG_I64,  PROVEN_ARG_U64, PROVEN_ARG_F64,
    PROVEN_ARG_CSTR, PROVEN_ARG_STR_VIEW, PROVEN_ARG_PTR,
    PROVEN_ARG_CHAR, PROVEN_ARG_BOOL, PROVEN_ARG_CUSTOM, /* ... */
} proven_arg_type_t;
```

*② One construction function per type.* Small functions such as
`proven_arg_i32(int v)` that take a value, attach the tag and return it. All are
`static inline`, so there is no call cost.

*③ A macro choosing among those functions with `_Generic`.* Here is the real
header.

```c
#define PROVEN_ARG(x) _Generic((x),          \
    _Bool:              proven_arg_bool,     \
    char:               proven_arg_char,     \
    signed char:        proven_arg_i32,      \
    unsigned char:      proven_arg_u32,      \
    short:              proven_arg_i32,      \
    unsigned short:     proven_arg_u32,      \
    int:                proven_arg_i32,      \
    unsigned int:       proven_arg_u32,      \
    long:               proven_arg_i64,      \
    unsigned long:      proven_arg_u64,      \
    long long:          proven_arg_i64,      \
    unsigned long long: proven_arg_u64,      \
    double:             proven_arg_f64,      \
    float:              proven_arg_f64,      \
    const char*:        proven_arg_cstr,     \
    char*:              proven_arg_cstr,     \
    void*:              proven_arg_ptr,      \
    proven_u8str_view_t: proven_arg_str_view,\
    proven_arg_t:       proven_arg_identity  \
)(x)
```

The knack of reading it is the `(x)` on the last line. `_Generic` settles into
*one function name*, and the `(x)` after it calls that function. That is,
`PROVEN_ARG(n)` compiles to `proven_arg_i32(n)` if `n` is an `int`, and to
`proven_arg_f64(n)` if it is a `double`.

A few details of the design stand out.

- *Narrow integers are gathered towards the fat side* — `short` and `signed char`
  both go to `proven_arg_i32`. The same direction as the previous section's
  default argument promotions, but here it is *the library, not the compiler*,
  deciding explicitly.
- *`float` is gathered into `double`* — for the same reason.
- *`char` is kept separate* — there is a separate `proven_arg_char`, so
  `char c = 'Z'` prints as the letter `Z` rather than the number 90. But
  `PROVEN_ARG('Z')` still goes as 90, because `'Z'` is an `int` by C's rule — the
  header records this limit honestly in a comment.
- *`proven_arg_t` itself is in the list* — an identity branch, so that a value
  already made into a bundle passes through unchanged when wrapped again.
- *A type not in the list is a compile error* — there is no `default` branch. Pass
  a struct by mistake and the build fails. The exact opposite choice from
  `printf`, which accepted anything and collapsed at run time.

The rest is simple. `proven_println("...", PROVEN_ARG(a), PROVEN_ARG(b))` binds
the bundles into an *array literal* and passes it, with the count, to the real
function. Nothing passes through a variadic position, so there is no promotion, no
guessing at counts, and no format-argument mismatch.

#qa[
  So has `_Generic` given C generics?
][
  No. `_Generic` is a device for *choosing among functions that already exist*; it
  does not create code per type. A function must be written by hand for each type
  and the list maintained by hand. So it is used for *a finite, well-known set of
  types*, as in the standard library's `<tgmath.h>` (choosing maths functions by
  type) or this section's `PROVEN_ARG`. When real generics are needed, C still
  stamps code out with macros (chapter 81's container macros) or passes `void *`
  with a size (chapter 74's `qsort`).
]

== History — from varargs to stdarg

This device's history is a miniature of C growing into a language of contracts.

*Before the standard (the K&R days).* In the C of the era without prototypes
(chapter 12), effectively no function had its argument count checked — a function
like `printf` was nothing special. The way of taking arguments out was not
portable either: each person used tricks such as walking the stack from the
address of the first argument, and those tricks broke when machines appeared that
passed arguments in registers rather than on the stack. Hence the Unix family's
`<varargs.h>` — the first attempt at wrapping the tricks in macros to hide machine
differences.

*C89 — `<stdarg.h>`.* As the standards committee introduced prototypes
(chapters 10 and 24), variadic arguments got formal syntax too: write `...` in the
prototype and take values out with the `va_*` macros. The decisive difference from
the old `varargs` is that *at least one named argument is required* (`va_start`
uses it as the reference point) — which is why signatures with the format first,
like `printf(const char *fmt, ...)`, became standard.

*C23 — tidying.* The restriction requiring a reference argument was relaxed
(`va_start`'s second argument became optional), so a variadic function with no
named argument can be written, and the old `varargs.h` vanished entirely into
history. Over half a century it was refined from "unportable trick" through
"standard macros" to "the excess removed."

#misconception[
  "Variadic functions are convenient, so use them often"
][
  Convenient though they look, variadic arguments in C are *the place where type
  safety disappears*. The compiler does not check the arguments in the `...`
  position and the receiving side has no way to guess the types. That the `printf`
  family is even as safe as it is comes from compilers *specially* reading and
  checking the format string (chapter 17's warnings), not from any general rule of
  the language — a variadic function you write yourself has no such net. The
  practical advice is clear: *do not make one unless it is truly necessary.* The
  alternatives are usually better — if the count is fixed, just list the arguments;
  if there are many, take an array and a length (chapter 37's practice); if the
  kinds vary, take an explicit list of values with chapter 45's tagged union. The
  modern taste in C library design is "variadic only where a tool's checking
  attaches, as in logging and formatting."
]

#qa[
  How do I make my own function that wraps a formatting function like `printf` —
  a logging function, say?
][
  The standard has prepared `v`-prefixed partners for exactly that place —
  `vprintf`, `vfprintf`, `vsnprintf` and so on, the versions that *take a
  `va_list` directly*. Your function receives `...`, makes a reader with
  `va_start`, and hands that reader over whole. If you want the format-checking
  warning on your function too, the practice is to attach a compiler extension
  notation (gcc's and clang's `format` attribute) — the pattern seen several times
  in this book, of tools filling in the safety the language cannot give.
]

We know the identity of variadic arguments — it was how to pass several *values*
to a function. The next chapter is its mirror image: how to handle a function
itself as a value, the function pointer.
