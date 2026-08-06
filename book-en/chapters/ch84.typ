#import "../../book/lib.typ": *

= Writing it three times — a tiny JSON

#prereq(
  ([chapter 76, Errors are values], [returning failure as a value]),
  ([chapter 77, Foundations — byte, view, checked arithmetic], [borrowed slices and overflow checks]),
  ([chapter 78, Allocation — allocators, arenas, pools], [where memory comes from]),
  ([chapter 79, Strings], [refuse rather than truncate]),
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
  difference, this chapter writes the same program several times and shows it.
]

#organizer[
#idx("JSON")  The chapter that closes Part XII — an exercise without exercises. We write a
  very small JSON reader and writer in three editions — plain C, proven, and an
  extended proven that takes nesting — and watch where they part company. The
  last one shows how to handle depth without recursion. No new syntax appears.
  What appears is *choice*.
]

#chapter-questions()

== What we are building

Build all of it and this becomes a parser textbook rather than this book. So the
scope is narrowed like this.

#dtable(
  columns: 2,
  keycol: false,
  [*In*], [*Out*],
  [One flat object — `{ "key": value, ... }`], [Nesting (in the first two editions; the third solves it)],
  [Four kinds of value — string, integer, boolean, null], [Reals, exponent notation],
  [Reading and writing back (a round trip)], [`\u` escapes, comments],
  [Where a failure happened], [Recovery, partial parses],
)

Even narrowed, everything this part is about fits inside: the size of the
container, pointing into someone else's memory, integer overflow, how failure is
announced, and who provides the memory. Nesting is taken up in the last section
by a *third edition* — without recursion, on an explicit stack.

== The plain C edition

#demo("examples-en/ch84/json_plain.c")

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

#demo("examples-en/ch84/json_proven.c")

It reads the same grammar and writes the same result. But the right-hand column
of that table has moved to the left.

*Values are not copied.* A string value is a `proven_u8str_view_t` — a borrowed
slice pointing into the source buffer (chapter 77). With no container there is
nothing to overflow and nothing to truncate. In exchange one contract appears:
*it is valid only while the source lives.* That contract is written in the type's
name.

*Failure arrives as a value.* `jresult` returns a `proven_err_t` together with
*where it stopped*. The last two lines of the demonstration are that in the
flesh: when there is no room for another pair it refuses instead of trimming
(`err 1`), and a number too large to hold is refused rather than wrapped
(`err 9`).

*The source of memory is a parameter.* `json_parse` takes an arena
(chapter 78). It does not know where the memory comes from and does not need to.
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

== One step further — nesting, without recursion

The two editions so far read one flat object. Real JSON nests. How is that
usually written? *Recursive descent*: when a value turns out to be an object,
call yourself again to read what is inside. It is short and it reads well.

That brevity has a price attached. *The input decides the depth.* Nest a
thousand deep and a thousand frames pile up; nest a hundred thousand deep and
the stack gives way. It is chapter 40 exactly — the stack is narrow (a few MiB
usually), and when it overflows the program dies with no way to check for it.
For a parser reading files other people wrote, that is an *attack surface*.

So the extended edition uses no recursion. Both parsing and output are loops
driven by an explicit stack. Depth becomes the length of an array, so crossing
the limit can be *refused as a value* instead of collapsing the stack.

#demo("examples-en/ch84/json_nested.c")

The last two lines of the output are the point of the design. Given the same
200-deep input, a limit of 32 refuses at the 32nd level (`err 2`), and a limit
of 256 reads it through and builds 200 nodes. *Neither run dies* — depth is a
setting, not an incident.

=== What was used where

This edition draws on the tools of Part XII across the board. Gathered in one
place, each takes on one problem.

#dtable(
  columns: 3,
  [*Tool*], [*What it takes on*], [*Without it*],
  [Arena (chapter 78)], [Takes the memory of one parse in a lump and drops it in a lump], [Every node needs a matching `free`],
  [Pool (chapter 78)], [Recycles slots of exactly one `jnode`], [Same-size allocations fragment the heap],
  [Intrusive list (chapter 77)], [Hooks a child onto its parent — through a link inside the node], [A separate child array must be allocated and grown],
  [Dynamic array], [Stacks the open containers — the *explicit stack*], [You end up leaning on the call stack (that is, recursion)],
  [`view` (chapter 77)], [Borrows keys and strings from the source], [Every character needs a copy and a container],
  [Checked arithmetic (chapter 77)], [Watches overflow at every carry], [One forgotten `errno` and it wraps],
  [`proven_err_t` (chapter 76)], [Depth exceeded, no room, bad syntax — all as values], [Either death, or a silent trim],
)

*The intrusive list* earns its keep especially here. Rather than allocating an
array for the children, each node carries one link (`proven_list_node_t link`)
that threads it onto the parent's list. The link was created along with the node,
so *adding a child costs no new allocation* — and one more place that could fail
disappears.

#qa[
  Does dropping recursion not make the code longer and harder to read?
][
  Longer, yes. What would be ten lines in a recursive version becomes thirty of
  stack frames and state transitions. Recursion also reads more easily — it is
  closer to the model in a person's head.

  It is still written this way for one reason: *the input must not decide how
  much resource is consumed.* In the recursive version depth eats the call
  stack, an *invisible* resource that can be neither checked nor capped. With an
  explicit stack, depth is `stack.len` — a *number you can see* — and the cap is
  a parameter.

  Out of that comes the working rule for code at a boundary (files, networks,
  plug-ins): *do not read a format with depth using recursion.* If you do, put a
  limit on the depth and count it.
]

#realcase("Deep nesting is a real attack")[
  "Depth bombs" are an old class of attack on JSON and XML parsers. Send a few
  kilobytes with a hundred thousand brackets in it and a recursive parser
  overflows the stack while reading it and the process dies — denial of service.
  Stopping a server with a few dozen bytes of input is a good return on effort.

  That is why widely used parsers nearly all impose a depth limit. It is also why
  this example takes `max_depth` as a parameter — and why the limit is set by the
  *caller rather than the library*, since what counts as reasonable differs from
  one place of use to another.
]

#misconception[
  "Removing recursion removes stack overflow"
][
  It does not remove it — it *moves* it. An explicit stack eats memory too. The
  difference is that this memory sits on the heap (or in an arena), its length
  can be counted, and a cap can be placed on it.

  The point is not "recursion is bad" but *keep the resource where you can see
  it*. If the depth is a constant you chose (as in code reading your own config
  file), recursion is the better choice. If someone else chooses the depth, it is
  better to hold that resource in your hand and count it.
]

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

  That is why chapter 74's five bugs have been shipping for half a century. Not
  because the code was bad, but because *code written for narrow conditions moved
  somewhere wide.*
]

#misconception[
  "Using a library stops you making these mistakes"
][
  A library does not *stop* mistakes. It *exposes* them. Ignore the `jresult` in
  the proven edition and the outcome matches the plain edition — except that
  writing it that way is more awkward, and where `[[nodiscard]]` is attached the
  compiler speaks up (chapter 76).

  What a tool can do ends at *making the correct path the easy path*. Beyond that
  it is always the user's part — which is also why this book explained the
  problems before it introduced the library.
]

#realcase("Where a real JSON parser gets harder")[
  What all three editions left out is where the real difficulty lives. `\u`
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
    [Nesting and depth], [An explicit stack instead of recursion — depth becomes a setting, not an incident],
  )
]

Part XII ends here. We have seen the five contracts one at a time, and finally
watched all five meet inside one program, written three times. The last part closes the
book — C in practice, the embedded toolbox, and everything gathered up.
