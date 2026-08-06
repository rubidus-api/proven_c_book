#import "../../book/lib.typ": *

= Writing it twice — a tiny JSON

#prereq(
  ([chapter 73, Errors are values], [returning failure as a value]),
  ([chapter 74, Foundations — byte, view, checked arithmetic], [borrowed slices and overflow checks]),
  ([chapter 75, Allocation — allocators, arenas, pools], [where memory comes from]),
  ([chapter 76, Strings], [refuse rather than truncate]),
)

#deepqa[
  Part XII has walked through five contracts one at a time — errors are values,
  a view is borrowed, the allocator is a parameter, state is not copied, refuse
  rather than truncate. So what changes in code when all five apply *at once,
  inside one program*?
][
  What changes is not the syntax but *where things are written down*. Written in
  plain C, container sizes, failure handling and the source of memory are
  scattered through the code as convention; written with proven, the same things
  come out in types, return values and parameters. Rather than describe the
  difference, this chapter writes the same program twice and shows it.
]

#organizer[
#idx("JSON")  The chapter that closes Part XII — an exercise without exercises. We write a
  very small JSON reader and writer in two editions and watch, line by line,
  where the two part company. No new syntax appears. What appears is *choice*.
]

#chapter-questions()

== What we are building

Build all of it and this becomes a parser textbook rather than this book. So the
scope is narrowed like this.

#dtable(
  columns: 2,
  keycol: false,
  [*In*], [*Out*],
  [One flat object — `{ "key": value, ... }`], [Nested objects and arrays],
  [Four kinds of value — string, integer, boolean, null], [Reals, exponent notation],
  [Reading and writing back (a round trip)], [`\u` escapes, comments],
  [Where a failure happened], [Recovery, partial parses],
)

Even narrowed, everything this part is about fits inside: the size of the
container, pointing into someone else's memory, integer overflow, how failure is
announced, and who provides the memory.

== The plain C edition

#demo("examples-en/ch81/json_plain.c")

It will read as familiar. This is *the most common shape* such a thing takes in
C: fixed-size arrays to hold it, `char` arrays to copy strings into, `-1` plus a
`char err[]` to report failure.

This is not *bad* code. It is code that needs someone to keep it. The last line
of the demonstration shows the price: given a value longer than the container,
127 of 159 characters survived and the rest was *cut silently.* The single line
`if (i + 1 < cap)` in `take_string` decided that, and the caller has no way of
learning it happened.

#dtable(
  columns: 3,
  [*Place*], [*What the code says*], [*What the code does not say*],
  [Length of a value], [`char str[128]`], [What happens past 128 characters],
  [Number of pairs], [`MAX_PAIRS 16`], [Who notices the seventeenth pair],
  [Failure], [`return -1` + `err[]`], [What happens if the caller does not check],
  [Numbers], [`strtol`], [That overflow must be read from `errno`],
  [Memory], [A static array], [Whether the parser's usage is visible outside],
)

The right-hand column is this code's *oral tradition*. It lives in comments, in
convention and in someone's memory — not in the types.

== The proven edition

#demo("examples-en/ch81/json_proven.c")

It reads the same grammar and writes the same result. But the right-hand column
of that table has moved to the left.

*Values are not copied.* A string value is a `proven_u8str_view_t` — a borrowed
slice pointing into the source buffer (chapter 74). With no container there is
nothing to overflow and nothing to truncate. In exchange one contract appears:
*it is valid only while the source lives.* That contract is written in the type's
name.

*Failure arrives as a value.* `jresult` returns a `proven_err_t` together with
*where it stopped*. The last two lines of the demonstration are that in the
flesh: when there is no room for another pair it refuses instead of trimming
(`err 1`), and a number too large to hold is refused rather than wrapped
(`err 9`).

*The source of memory is a parameter.* `json_parse` takes an arena
(chapter 75). It does not know where the memory comes from and does not need to.
Give it a static array and it runs without a heap; give it a heap allocator and
it runs on the heap. Neither requires touching the parser.

*Integer overflow is checked by hand.* `PROVEN_CKD_MUL` and `PROVEN_CKD_ADD`
check at every carry. Not "return, then look at `errno`" but failure as a value
at the moment of overflow.

#dtable(
  columns: 3,
  [*Place*], [*Plain C*], [*proven*],
  [String value], [Copied into `char str[128]` — cut if longer], [Borrowed as a `view` — no cut, a lifetime contract],
  [Number of pairs], [Fixed `MAX_PAIRS`], [A `cap` the caller chooses; `NOMEM` beyond it],
  [Failure], [`-1` plus a written reason], [`proven_err_t` plus the byte it stopped at],
  [Number parsing], [`strtol` + `errno` (easy to forget)], [Checked arithmetic at every digit],
  [Memory], [Static array (the parser decides)], [An arena (the caller decides)],
  [Writing], [`snprintf` — cut if it does not fit], [A growing `u8str` — failure if it does not fit],
)

== So what actually changed

#qa[
  The proven edition is the longer one. Where is the gain?
][
  What it is longer by is *the checking that should have been there.* The plain
  edition's brevity was bought by not checking, and that checking did not vanish
  — it moved onto a person.

  Count the difference and it comes to this. In the plain edition there are five
  places a person must remember to be careful: the container size, the maximum
  pair count, checking the return value, checking `errno`, and the size of the
  static array. In the proven edition those five moved into types, parameters and
  return values. *What had to be remembered became what can be read* — that is
  the gain.

  And it grows with the code. Five things can be remembered in a 200-line parser.
  They cannot be remembered in a 20,000-line program.
]

#qa[
  Is the plain edition useless, then?
][
  No — and this distinction is the most important thing in the chapter.

  The plain edition is excellent *when the conditions are narrow*: input you made
  yourself, sizes you know, code that never leaves this program. There its
  brevity is the virtue. What is dangerous is when that code *crosses a
  boundary*. The moment it reads a file someone else wrote, or bytes off a
  network, or runs inside a long-lived program, all five unwritten things become
  seeds of an incident.

  That is why chapter 71's five bugs have been shipping for half a century. Not
  because the code was bad, but because *code written for narrow conditions moved
  somewhere wide.*
]

#misconception[
  "Using a library stops you making these mistakes"
][
  A library does not *stop* mistakes. It *exposes* them. Ignore the `jresult` in
  the proven edition and the outcome matches the plain edition — except that
  writing it that way is more awkward, and where `[[nodiscard]]` is attached the
  compiler speaks up (chapter 73).

  What a tool can do ends at *making the correct path the easy path*. Beyond that
  it is always the user's part — which is also why this book explained the
  problems before it introduced the library.
]

#realcase("Where a real JSON parser gets harder")[
  What this example left out is where the real difficulty lives. Nesting needs a
  recursion depth limit (a malicious input otherwise blows the stack), `\u`
  escapes must handle UTF-16 surrogate pairs (chapter 9), and reals bring along
  the rounding problems of chapter 8 — read `0.1` and write it back, and do the
  same characters come out?

  That is why widely used parsers run to thousands of lines, and it is worth
  remembering that a good share of those lines are not features but *boundaries*.
]

#recap[
  #dtable(
    columns: 2,
    [*What to keep*], [*The point*],
    [One program, two editions], [Not the syntax but *what gets written down* differs],
    [Plain C], [Short. The price of that brevity is five things a person must remember],
    [proven], [Longer. It is longer by the checks moved into types, returns and parameters],
    [Cut versus refuse], [Failure comes back as a value instead of a silent trim],
    [Source of memory], [The caller provides it; the parser does not decide],
    [Boundaries], [Code written for narrow conditions gets dangerous somewhere wide],
  )
]

Part XII ends here. We have seen the five contracts one at a time, and finally
watched all five meet inside one program, written twice. The last part closes the
book — C in practice, the embedded toolbox, and everything gathered up.
