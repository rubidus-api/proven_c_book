#import "../../book/lib.typ": *

= Formatting and parsing — not writing the type twice

#prereq(
  ([chapter 53, Variadic functions], [variadic arguments and the format string]),
  ([chapter 56, The terrain of the standard library], [the printf contract]),
)

#deepqa[
  Chapter 53 said type information does not ride along into variadic arguments, and
  chapter 56 said that is why the format string alone settles "how the stack is to be
  read". Then what is needed to take the type from the argument?
][
  Catch the type *at the call site* and send it along with the value. That is, do not
  pass the argument as it is but wrap it into a `{type tag, value}` bundle. The
  remaining problem is "how is the type tag attached automatically", and the answer is
  C11's `_Generic` — the device that chooses different code at compile time according
  to an expression's type. The run-time cost is zero, and unlike the implicit
#idx("implicit conversion")  conversion rules learned in chapter 28, here the type is
  *preserved*.
]

#organizer[
  The answer to chapter 74's third bug — the mismatch of format and argument. How the
  typeless placeholder `{}` obtains type safety, what `_Generic` does beneath it, and
  how failure appears as a value in the opposite direction, parsing. It is the
  alternative to the `printf` and `scanf` taken apart in chapter 56.
]

#chapter-questions()

== `{}` — the placeholder with no type

The rules are only three.

- `{}` — put the next argument here. The type is not written.
- `{:...}` — after the colon write the format specification (width, alignment,
  digits).
- `{{` `}}` — the brace characters themselves.

There being no `%d`, there is no place for a `%d` and a `double` to go out of step.
The possibility of mismatch seen in chapter 56 is removed at the level of syntax.

=== The whole grammar after the colon

The order of the specifiers is fixed, and every one may be omitted.

#align(center, block(inset: (y: 4pt))[
  `{:` `[fill][align]` `[sign]` `[#]` `[0]` `[width]` `[.precision]` `[type]` `}`
])

#dtable(
  columns: 3,
  [*place*], [*what may be written*], [*meaning*],
  [fill], [any character], [the character filling the spare places. it comes *before* the alignment symbol],
  [align], [`<` `>` `^`], [left, right, centre. the default is right for numbers, left otherwise],
  [sign], [`+`], [attach a sign to positives too],
  [alternative form], [`#`], [attach the `0x`, `0b`, `0` prefix],
  [zero fill], [`0`], [put before the width to fill with zeros (it goes after the sign)],
  [width], [a number], [the minimum number of characters. it does not cut if over],
  [precision], [`.number`], [decimal places for reals],
  [type], [`x X o b f e g`], [hexadecimal (lower, upper), octal, binary, fixed, exponential, shortest],
)

They correspond to chapter 56's `printf` formats but differ in three ways. *The
alignment symbol comes first* (`{:<10}` instead of `%-10s`), *the fill character can be
chosen* (`{:*>8}`), and *there is no type letter* (`%d`'s `d` has gone — the type comes
from the argument).

#demo("examples-en/ch80/spec.c")

The example shows this table in the flesh, a line at a time. A few points.

*① The fill is written before the alignment.* `{:*>8}` is "fill with asterisks, align
right". Swap the order (`{:>*8}`) and it does not mean anything.

*② The 0 of `{:08}` goes after the sign.* Zero-fill `-42` to a width of 8 and it is
`-0000042`, not `000000-42` — the same rule as `%08d` seen in chapter 56.

*③ The default notation for reals differs from `printf`'s.* Very large and very small
numbers are printed by `printf("%f")` as `100000000000000000000.000000` or `0.000000`,
while this library uses exponential notation, `1.000000e+20` and `5.000000e-07`. The
side that *does not lose information* was taken as the default, and it is a difference
to know before comparing two logs. If fixed notation is needed, force it with `{:f}`.

*④ `{:g}` gives the shortest notation that round-trips.* That `3.14159` comes out as it
stands is that result — the notation keeping the "read it back and it is the same value"
property seen in chapter 8.

=== Printing a type the library has never heard of

What can go into `{}` is only the types in the `_Generic` list. Then how is my own
struct printed — *give it one function that draws.*

```c
static proven_err_t render_frac(proven_fmt_sink_t out, const void *obj)
{
    const frac_t *f = obj;
    /* ... make it ... */
    return proven_fmt_put(out, view);      /* and send it out */
}

proven_arg_t a = proven_arg_custom(&half, render_frac);
proven_println("{} and |{:>8}|", PROVEN_ARG(a), PROVEN_ARG(a));
```

`proven_fmt_sink_t` is "a hole that receives bytes", and `proven_fmt_put` sends them
out. As the example's output shows, *width and alignment apply to a user type too*.

There is one contract to know here. *The drawing function is called twice per `{}`* —
once with a counting sink (because the width and alignment must be calculated) and once
for real. So this function must be *deterministic* and must not mutate its target. If
the two results disagree the library returns `INVALID_ARG` rather than print a misaligned
field. It is the price paid for aligning without allocating.

#demo("examples-en/ch80/fmt.c")

This example formats not to the screen but *into a string* — the `proven_println`
seen in chapter 75 is the edition connecting this machinery to standard output. Three
things can be pointed out.

*First, formatting too can fail.* `example.org:8080` cannot go into an 8-byte vessel,
so it was refused. Chapter 79's principle stands here too — rather than truncate, it
returns a failure. The opposite default from `snprintf`.

*Second, the format specification syntax is a little different.* The `>` of `{:>10}`
is right alignment, `{:<10}` left alignment, `{:.3}` three decimal places. They
correspond to chapter 56's `%10s`, `%-10s` and `%.3f`, differing in that *there is no
type letter*.

*Third, it rounds but keeps the number of digits.* `load=0.42` is what `{:.2}` made.

=== Which formatting function to use

Chapter 79's three kinds are here in formatting too. Organised in a table there is
nothing to choose over.

#dtable(
  columns: 4,
  [*function*], [*when short*], [*allocator*], [*where it is used*],
  [`proven_println(fmt, …)`], [—], [not needed], [one line to the screen],
  [`proven_print(fmt, …)`], [—], [not needed], [without a line break],
  [`proven_eprint(fmt, …)`], [—], [not needed], [to standard error],
  [`proven_u8str_append_fmt`], [refuses (the original stands)], [not needed], [a fixed buffer. the default],
  [`proven_u8str_append_fmt_trunc`], [as much as fits], [not needed], [places that may be cut, such as a log line],
  [`proven_u8str_append_fmt_grow`], [grows], [needed], [when the length is unknown],
  [`proven_u8str_append_fmt_with_scratch`], [grows], [needed (+ scratch)], [when the temporary memory is to be given separately],
)

All of them return a `proven_fmt_result_t`, and this bundle has two more numbers beside
`err`.

```c
typedef struct {
    proven_err_t  err;
    proven_size_t written;    /* the number of bytes actually written */
    proven_size_t required;   /* the number of bytes needed to write it all */
} proven_fmt_result_t;
```

`required` is the same information as `snprintf`'s return value seen in chapter 56. The
difference is *that it sits in a named slot* — with `snprintf` a human had to remember
the convention "if the return value is at least the buffer size it was truncated", while
here `err` already says that and `required` answers "so how much was needed". In the
output of the example `spec.c`, `written=7 required=10` is that use — how much to enlarge
the buffer by is known as it stands.

#qa[
  What exactly does `PROVEN_ARG` do?
][
  It chooses on the argument's type with `_Generic` and makes a small struct with a
  tag fitting that type attached. Carried over in concept alone it has this shape.

  ```c
  #define PROVEN_ARG(x) _Generic((x),          \
      int:          proven_arg_i32,            \
      double:       proven_arg_f64,            \
      const char *: proven_arg_cstr,           \
      bool:         proven_arg_bool            \
      /* ... */ )(x)
  ```

  `_Generic` chooses the branch *at compile time*, so there is no run-time cost of
  determining the type. And passing a type not in the list is *a compile error* — the
  exact opposite of chapter 56's `printf`, which accepted anything.
]

#antipattern[
  Passing a value without `PROVEN_ARG`
][
  ```c
  proven_println("count={}", count);        /* it does not compile */
  ```
  A raw value rather than a bundle was passed, so the types do not match and the build
  fails. It can feel tiresome, but this is the point of the design — *forget to wrap
  it and the program is not made.* Herein lies the difference from `printf`, which
  compiles even when you forget and goes strange during execution.
]

#misconception[
  "The placeholder has no type, so it must be slow"
][
  The opposite. `printf` interprets the format string letter by letter *during
  execution* and decides what to take out next. `{}` scans the format too, but each
  argument's type is already settled by its tag, so there is no guessing. Above all,
  there being no UB from type mismatch, no defensive code is needed either. The
  difference in cost is mostly negligible, and this library's real-number formatting
  has rather taken more care over accuracy — reproducing exactly the rounding rules of
  `%f` seen in chapter 56 is the more awkward task.
]

== The opposite direction — the scanner

Parsing is formatting's mirror, but the character of failure differs. Formatting
fails only when the vessel is too small, while parsing fails *whenever the input
differs from expectation*. And as seen in chapter 56, `sscanf` tells only "how many
succeeded" — it does not say where or why it stopped.

#idx("scanner")proven's scanner is an object with a cursor. It is placed over a view
and reads onward one at a time. Each read gives its result as a bundle.

```c
typedef struct {
    proven_u8str_view_t view;     /* the input being read (borrowed) */
    proven_size_t       cursor;   /* how far it has read */
} proven_scan_t;
```

That there are only two slots says two things. First, *the scanner does not own the
input* — it is only a cursor laid over a view, so making it allocates nothing and there
is no destroying. Second, *the cursor can be saved and restored by hand*. Copy the whole
struct, and on failure put it back, so a parser that "looks a few characters ahead to
judge" is not hard to write.

```c
proven_scan_t save = sc;         /* a mark to go back to */
proven_result_i64_t n = proven_scan_i64(&sc);
if (!proven_is_ok(n.err)) sc = save;   /* it failed, so let it never have happened */
```

There are six reading functions, and all of them push the cursor forward.

#dtable(
  columns: 3,
  [*function*], [*what it reads*], [*what it returns*],
  [`proven_scan_i64(&sc)`], [a signed integer], [`{err, val}`],
  [`proven_scan_u64(&sc)`], [an unsigned integer], [`{err, val}`],
  [`proven_scan_f64(&sc)`], [a real], [`{err, val}`],
  [`proven_scan_str(&sc)`], [a word up to whitespace], [`{err, view}` — *a view into the original*],
  [`proven_scan_skip_whitespace(&sc)`], [skips whitespace], [—],
  [`proven_scan_skip_until(&sc, t)`], [skips until `t` appears], [`err` (the cursor stands if absent)],
)

That `proven_scan_str` *returns a view without copying* matters — chapter 79's "text
handling without copying" holds in parsing too. In exchange that view is valid only
while the original input is alive (chapter 77).

#demo("examples-en/ch80/lines.c")

That three inputs divided into three branches is the heart of it. `bob thirty` had
its name read but stopped at the age, and the line of whitespace only failed from the
name. With `sscanf` both would have been lumped together as "one item succeeded" or
"0".

Having a cursor has another advantage. *How far it has read* can be known, so the
remaining part can be handled another way or the position can be carried in an error
message. It is the answer to the problem chapter 25 named when parsing a line with
`sscanf`: "you cannot know how many characters were consumed."

#qa[
  Does the scanner have a format string too?
][
  It does. It is written like this.

  ```c
  proven_scan_fmt_cursor(&sc, "{}:{}",
                         PROVEN_SCAN_ARG(&host), PROVEN_SCAN_ARG(&port));
  ```

  It is symmetrical with formatting, and the arguments are
  wrapped addresses of the places to hold the results. But there is one caution the
  header states honestly — if it fails in the middle of the format, *the values filled
  in up to that point have already been changed*. The failure atomicity learned in
  chapter 76 is not guaranteed here, so if it is really needed the cursor and the
  destinations must be saved beforehand and restored. Writing the contract in the
  documentation rather than hiding it is this library's way.
]

#realcase[
  What happens when a parser is lenient
][
  There is a problem that has come to light repeatedly in the handling of HTTP
  requests. If a server and a proxy interpret the same request *slightly differently*,
  an attacker can slip a second request in through that gap (request smuggling). The
  cause was differences such as one side generously letting odd whitespace in a header
  pass while the other refused strictly. The lesson is exactly the same as
  chapter 79's story of encodings — *do not read ambiguous input as something mended;
  refuse it.* A parser that returns failure as a value is also a parser equipped with
  the means to express that refusal.
]

#recap[
  Formatting and parsing in summary.

  #dtable(
  columns: 3,
    [*what it does*], [*API*], [*note*],
    [one line to the screen], [`proven_println(fmt, ARG…)`], [returns an error but does not compel],
    [format into a string], [`proven_u8str_append_fmt(&s, …)`], [refuses if short],
    [permitting truncation], [`…_append_fmt_trunc`], [states the intent in the name],
    [growing as it goes], [`…_append_fmt_grow(alloc, …)`], [needs an allocator],
    [wrapping an argument], [`PROVEN_ARG(x)`], [`_Generic` — a type not listed is a compile error],
    [starting a scanner], [`proven_scan_init(view)`], [an object with a cursor],
    [reading one at a time], [`proven_scan_i64/f64/str`], [result and failure as a bundle],
    [reading by format], [`proven_scan_fmt_cursor(…)`], [beware partial changes on mid-way failure],
)
]

The vocabulary for holding, making and reading back strings is equipped. Next are the
tools that *hold many* — growing arrays, lists, ring buffers, hash maps, and the
algorithms with "a guarantee even in the worst case" foretold in chapter 74.
