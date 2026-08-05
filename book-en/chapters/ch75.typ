#import "../../book/lib.typ": *

= Formatting and parsing — not writing the type twice

#organizer[
  The answer to chapter 69's third bug — the mismatch of format and argument. How the
  typeless placeholder `{}` obtains type safety, what `_Generic` does beneath it, and
  how failure appears as a value in the opposite direction, parsing. It is the
  alternative to the `printf` and `scanf` taken apart in chapter 53.
]

#deepqa[
  Chapter 50 said type information does not ride along into variadic arguments, and
  chapter 53 said that is why the format string alone settles "how the stack is to be
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

== `{}` — the placeholder with no type

The rules are only three.

- `{}` — put the next argument here. The type is not written.
- `{:...}` — after the colon write the format specification (width, alignment,
  digits).
- `{{` `}}` — the brace characters themselves.

There being no `%d`, there is no place for a `%d` and a `double` to go out of step.
The possibility of mismatch seen in chapter 53 is removed at the level of syntax.

#demo("examples/ch75/fmt.c")

This example formats not to the screen but *into a string* — the `proven_println`
seen in chapter 70 is the edition connecting this machinery to standard output. Three
things can be pointed out.

*First, formatting too can fail.* `example.org:8080` cannot go into an 8-byte vessel,
so it was refused. Chapter 74's principle stands here too — rather than truncate, it
returns a failure. The opposite default from `snprintf`.

*Second, the format specification syntax is a little different.* The `>` of `{:>10}`
is right alignment, `{:<10}` left alignment, `{:.3}` three decimal places. They
correspond to chapter 53's `%10s`, `%-10s` and `%.3f`, differing in that *there is no
type letter*.

*Third, it rounds but keeps the number of digits.* `load=0.42` is what `{:.2}` made.

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
  exact opposite of chapter 53's `printf`, which accepted anything.
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
  `%f` seen in chapter 53 is the more awkward task.
]

== The opposite direction — the scanner

Parsing is formatting's mirror, but the character of failure differs. Formatting
fails only when the vessel is too small, while parsing fails *whenever the input
differs from expectation*. And as seen in chapter 53, `sscanf` tells only "how many
succeeded" — it does not say where or why it stopped.

#idx("scanner")proven's scanner is an object with a cursor. It is placed over a view
and reads onward one at a time. Each read gives its result as a bundle.

#demo("examples/ch75/lines.c")

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
  It does — it is used as
  `proven_scan_fmt_cursor(&sc, "{}:{}", PROVEN_SCAN_ARG(&host),
  PROVEN_SCAN_ARG(&port))`. It is symmetrical with formatting, and the arguments are
  wrapped addresses of the places to hold the results. But there is one caution the
  header states honestly — if it fails in the middle of the format, *the values filled
  in up to that point have already been changed*. The failure atomicity learned in
  chapter 71 is not guaranteed here, so if it is really needed the cursor and the
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
  chapter 74's story of encodings — *do not read ambiguous input as something mended;
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
algorithms with "a guarantee even in the worst case" foretold in chapter 69.
