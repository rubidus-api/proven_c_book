#import "../../book/lib.typ": *

= Loop techniques — nesting, escaping, and making a block

#prereq(
  ([chapter 32, Loops and invariants], [the three loops `while`, `for`, `do-while`]),
  ([chapter 39, Multidimensional arrays], [rows and columns, and their layout in memory]),
)

#deepqa[
  Chapter 32 taught the three loops and chapter 39 the two-dimensional array.
  So the sum of every cell of `int a[1000][1000]` can be written two ways ---
  rows first, or columns first. The two give *the same answer*. What differs?
][
  *The speed --- by about eight times.* As chapter 39 showed, a 2-D array is laid
  out in memory *row by row* (row-major), and as chapter 11 showed, the cache
  brings in *neighbouring bytes together*. Walk along a row and you spend the
  line you fetched; walk down a column and every step calls for a new one.

  Having learned loops and knowing how to *drive* them are different things.
  This chapter fills that gap.
]

#organizer[
#idx("nested loop")  The practical techniques of loops, gathered --- nesting and traversal order
  (measured), the four ways of leaving nested loops and how they rank, the two
  places where `goto` is right and the discipline it demands, the
  `do { } while (0)` that makes a macro one statement, and the idioms and traps
  met over and over in practice. If chapter 32 was "what a loop is", this
  chapter is "how a loop is driven".
]

#chapter-questions()

== Nested loops — walking a multidimensional array

Chapter 39's `int a[R][C]` lies in memory *as one line*, in row-major order ---
`a[0][0] a[0][1] … a[0][C-1] a[1][0] …`. The order of the nested loops decides
*with what stride* you walk that line.

```c
for (int i = 0; i < R; i++)          /* rows first — one step to the neighbour */
    for (int j = 0; j < C; j++)
        sum += a[i][j];

for (int j = 0; j < C; j++)          /* columns first — a whole row skipped each step */
    for (int i = 0; i < R; i++)
        sum += a[i][j];
```

The same computation. Measured, not the same at all.

#dtable(
  columns: 3,
  [*Traversal*], [*Summing 4000×4000 `int`*], [*Why*],
  [rows first `(i, j)`], [11–14 ms], [it uses up the line the cache fetched, then moves on],
  [columns first `(j, i)`], [101–113 ms], [each step jumps 16 KB and calls for a new line],
)

*Eight times.* Same algorithm, same operation count, same result. Chapter 11's
"the cache governs speed" becomes a visible number here.

So practice's first optimisation is usually *changing the loop order* --- without
complicating the code or touching the algorithm, just swapping two lines so that
the inner subscript is *the axis that varies fastest.*

#qa[
  So should the last subscript always go innermost?
][
  Usually, yes. Two things go with it though.

  *1. The layout decides what is right.* C is row-major, but Fortran is
  column-major, so numerical code ported from Fortran may be right the other way
  round. If it is not an array but an *array of structs*, it changes again
  (chapter 45's layout story).

  *2. Some computations cannot have their order changed.* Where the previous cell's
  result feeds the next (accumulation, recurrences), the order is part of the
  algorithm. What is used then is *tiling* --- cutting the work into blocks that
  fit the cache --- which is beyond this book.

  One rule is worth keeping: *let the innermost loop move most tightly through
  memory.* That is a factor of several, for free.
]

== Leaving nested loops

Write a nested loop and you hit a problem at once.

#misconception[
  "`break` leaves the loop"
][
  It leaves *the innermost layer only*. The standard says so --- `break` ends the
  nearest enclosing `switch` or iteration statement, one of them.

  The listing measured it. Code that broke only in the inner loop visited 18 of the
  table's 20 cells --- it found the value and *the outer loop kept going.*

  And C has *no labelled `break`.* Here it parts from other languages.

  #dtable(
    columns: 2,
    [*Language*], [*Device for shedding several layers at once*],
    [C], [none --- `goto` or another way],
    [Java], [`outer: for (…) { break outer; }`],
    [Go], [the same label syntax; `continue` takes one too],
    [Rust], [`'outer: loop { break 'outer; }` --- it even returns a value],
    [Perl, PHP], [`last LABEL` / `break 2` (the depth as a number)],
  )
]

C offers four ways, each worth something different.

#demo("examples-en/ch40/nested_exit.c")

#dtable(
  columns: 4,
  [*Way*], [*How*], [*Good*], [*Bad*],
  [1. a flag variable], [put `&& !found` on the outer condition], [no `goto`], [the condition grows, and *where it ends is scattered over two places*. One more test per iteration too],
  [2. `goto`], [jump to a label the moment it is found], [it says "shed both layers at once" in one line], [some conventions forbid `goto`],
  [3. *a function and `return`*], [make the search a function], [`return` sheds any depth. No escape device at all], [one more function, and returning several values needs a struct],
  [4. condition on the outer loop], [`for (i = 0; i < R && !done; i++)`], [a variant of 1], [the same problem as 1],
)

*This book's recommendation is 3 → 2 → 1.*

*Three comes first* because the problem is not the escape but *the lump*. If a
nested loop has grown long enough that you are contriving a way out, its inside has
already become "something worth naming". Lift it into a function and it gains a
name, `return` solves the escape for free, and it becomes testable.

*Two beats one* because the intent shows. A flag makes the reader *reconstruct*
"when this variable becomes true, the loop ends somewhere"; `goto done;` says right
there that it ends. And a flag is easy to get wrong --- forget the inner `break` and
it quietly runs one more round.

== The two places `goto` is right

Clear away the misunderstanding first.

#realcase[
  What Dijkstra actually objected to
][
  Dijkstra's 1968 letter to CACM, "Go To Statement Considered Harmful", may be the
  most cited and *least read* piece in programming's history.

  Its argument is not "do not use the word `goto`". It is that *one must be able to
  map a point in the program's text onto the progress of its execution*, and that
  unrestrained jumping destroys that mapping. Even the title's "considered harmful"
  is understood to have come not from Dijkstra but from the editor, Niklaus Wirth.

  Seen from today, most of what that letter demanded *has already happened* ---
  `while`, `for`, functions, `break` and `return` replaced nearly every use of the
  `goto` of that era. What remains are the two places below, and in them `goto`
  makes the flow *easier* to read. That is why the Linux kernel's coding style
  explicitly permits it.
]

=== Place 1 --- shedding several loops at once

The pattern from the previous section. The rules are *downwards only, and nearby.*

```c
for (int i = 0; i < R; i++)
    for (int j = 0; j < C; j++)
        if (grid[i][j] == target) { found = (struct pos){ i, j }; goto done; }
done:
    …
```

=== Place 2 --- gathering the cleanup of error handling

A function that acquires several resources must *give back only what it acquired so
far* when it fails midway. Written with `if`s, the giving-back code gets copied
layer upon layer.

#demo("examples-en/ch40/macro_block.c")

The listing's `build` is that pattern. The labels are stacked *in reverse order of
release*, and a failure jumps to the label that matches it.

```c
    a = malloc(n);
    if (!a) goto out;          /* nothing to give back yet */
    b = malloc(n);
    if (!b) goto free_a;       /* give back only a */
    …
    free(b);
free_a:
    free(a);
out:
    return ok;
```

*There is exactly one copy of the cleanup code* --- that is what the pattern buys.
Add a third or fourth resource and only a label is added; nothing is duplicated. It
is how chapter 43's pairing of `malloc` and `free` is kept in practice.

#dtable(
  columns: 2,
  [*The discipline for `goto`*], [*Why*],
  [*jump downwards only*], [a jump upwards is a loop, and a loop should be written with loop syntax to be read],
  [*jump nearby* --- same function, within sight], [jump far and Dijkstra's problem really does appear],
  [*name labels for what they do*], [`done`, `cleanup`, `free_buf` --- `label1` is not a name],
  [*do not skip an initialisation*], [a variable whose initialisation was skipped holds an indeterminate value, and *jumping into the scope of a variable-length array is a constraint violation* the compiler diagnoses],
)

#qa[
  What about conventions (MISRA and the like) that forbid `goto` outright?
][
  As chapter 94 shows, rulebooks such as MISRA forbid or tightly limit `goto`. Three
  alternatives are used there.

  - *Lift it into a function.* Number 3 above. Much of the cleanup problem also
    dissolves once the work is split into small "acquire, use, release" functions.
  - *A `do { … } while (0)` run once.* A `break` inside it gives the same effect as
    "jump to the cleanup" --- the next section's pattern used for control flow.
  - *One state variable stepping through stages.* Chaining `if (ok) { … }`, which
    gets harder to read as the stages multiply.

  Judge it by chapter 12's ladder --- where a rulebook governs, follow the rulebook
  and pay its price (longer code) knowingly. Where none governs, there is no reason
  to bind yourself with "`goto` is always bad".
]

== Making a macro one statement — `do { } while (0)`

A macro holding several statements has an old trap. Wrapping it in braces is not
enough.

```c
#define SWAP_BAD(a, b)  { int t = (a); (a) = (b); (b) = t; }

if (x < y) SWAP_BAD(x, y); else puts("...");
```

Expand it and the reason shows --- it becomes `if (x < y) { … }; else …`, where the
semicolon after the braces is *an empty statement*. The `if` ends on that empty
statement and the `else` loses its partner. Measured, GCC says:

```text
error: 'else' without a previous 'if'
```

`do { } while (0)` solves exactly this, because it is *a braced block and, at the
same time, a single statement that wants a semicolon after it.*

#dtable(
  columns: 4,
  [*Macro body*], [between `if` and `else`], [as a `for` body (no braces)], [the semicolon],
  [bare statements], [breaks], [*only the first statement* repeats — a silent accident], [not needed (confusing)],
  [`{ … }`], [breaks], [fine], [adding one creates an empty statement],
  [`do { … } while (0)`], [*fine*], [*fine*], [*naturally required*],
)

In the listing, `SWAP` and `LOG` work between `if` and `else` and as a brace-less
`for` body alike --- that is the confirmation.

#qa[
  `while (0)` — doesn't that mean it never runs?
][
  It is a `do-while`, so it *runs the body first* and tests afterwards (chapter 32).
  The condition being false, it runs exactly once --- which is the point.

  Compilers know the pattern too. Even built without optimisation it folds into "a
  loop whose condition is always false", so *the run-time cost is zero*. It merely
  borrows the shape of a loop; no loop actually loops.
]

#antipattern[
  Using `do { } while (0)` for a macro that yields a value
][
  ```c
  #define MAX(a, b)  do { … } while (0)   /* cannot yield a value */
  int m = MAX(x, y);                       /* a statement cannot be assigned */
  ```

  `do { } while (0)` is a tool for making *statements*. When a value is needed, pick
  one of three.

  - *An expression macro* --- `#define MAX(a, b) ((a) > (b) ? (a) : (b))`. Parenthesise
    everything, and accept that an argument is *evaluated twice* (which is why
    `MAX(i++, j)` is an accident).
  - *A `static inline` function* --- it has types and evaluates each argument once.
    Since C99 this is the proper answer (chapter 24).
  - *`_Generic` choosing a function per type* --- when several types must be taken
    (chapters 26 and 56).

  GCC and Clang have a statement-expression extension `({ … })` that does both, but
  it is *not standard* (chapter 12's grey area). Where portability matters, use the
  three above.
]

#realcase[
  `do { } while (0)` in the standard library and the kernel
][
  The pattern is not a habit but a de facto standard. Open the Linux kernel's headers
  and `do { } while (0)` appears thousands of times; the coding-style document
  *requires* the form for multi-statement macros.

  There is an amusing variant. A macro that does nothing --- a log macro outside a
  debug build, say --- if defined as an empty `#define LOG(...)` leaves a bare
  semicolon, which breaks places like `if (x) LOG(…);`. So it is defined as
  `#define LOG(...) do { } while (0)`: *an empty do-while*, doing nothing while
  keeping the property of being a statement.

  As a bonus, unused macro arguments can raise "unused variable" warnings, so the
  habit of writing `do { (void)(x); } while (0)` settled in alongside it.
]

== Other idioms, and their traps

#demo("examples-en/ch40/loop_idioms.c")

=== Walking backwards --- `i >= 0` is unusable with `size_t`

The most frequent bite.

```c
for (size_t i = n - 1; i >= 0; i--)   /* an infinite loop */
```

An unsigned value is *always at least 0*, so the condition never fails, and `i--` at
0 wraps to `SIZE_MAX` (chapter 27). GCC says so through `-Wtype-limits` (included in
`-Wall -Wextra`): "comparison of unsigned expression in `>= 0` is always true".

There are two right shapes.

```c
for (size_t i = n; i-- > 0; )   /* the idiom — the test does the decrement too */
for (int i = (int)n - 1; i >= 0; i--)   /* a signed index */
```

The first is the common one. `i-- > 0` means "see whether the current value exceeds
0, then subtract one" (chapter 48's postfix), so the body sees `n-1` down to `0` in
turn.

=== The rest of the idioms

#dtable(
  columns: 3,
  [*Idiom*], [*What it is*], [*Watch out*],
  [`for (;;)` / `while (1)`], [an infinite loop; the two are identical], [GCC warns about neither (measured). Many codebases prefer `for (;;)` because MSVC used to warn on `while (1)`],
  [the comma operator], [`for (i = 0, j = m - 1; i < j; i++, j--)`], [closing in from both ends. *Outside this place* the convention is not to use the comma operator],
  [a sentinel loop], [`while ((c = getchar()) != EOF)`], [*the parentheses are required* — without them it is `c = (getchar() != EOF)`. And `c` must be an `int` (chapter 63)],
  [`strlen` in the loop condition], [`for (i = 0; i < strlen(s); i++)`], [it counts the string from the start every round. Take the length into a variable (chapter 41)],
  [a floating-point counter], [`for (double x = 0; x != 1.0; x += 0.1)`], [*it may never end* — 0.1 is not exact (chapter 49). Count in integers and divide],
  [an empty body `;`], [`while (*p++) ;`], [short, but the semicolon is invisible. See below],
)

#antipattern[
  An empty body nobody notices
][
  ```c
  for (int i = 0; i < n; i++);      /* one semicolon and the body is gone */
      total += a[i];                /* outside the loop — and i is not here */
  ```

  The indentation promises repetition; what happens is that the loop spins *empty*
  and ends.

  ★Measured, the warnings do not help. `-Wempty-body` catches `if (x);` with
  "suggest braces around empty body in an 'if' statement" but *does not catch
  `for (…);` or `while (…);`* --- those are a deliberate idiom.

  So discipline has to stop it --- *if an empty body is the intent, make it visible.*

  ```c
  while (*p++ != '\0')
      continue;                     /* "this is empty on purpose" */
  ```
]

#qa[
  In a `do-while`, where does `continue` go?
][
  *To the test* --- not to the top of the body. It is the same rule as in `while` and
  `for` (in a `for` it passes through the update expression first).

  It confuses people because a `do-while`'s condition is written *below*, giving the
  impression that `continue` "goes back up". The actual flow is "down to the test,
  and back up if it is true". Measured, the output matches the same code written as
  a `while`.
]

We have nesting, escaping, and the making of a statement out of a macro. But this
chapter's listings kept walking `char` arrays --- and the next chapter is the
*agreement* attached to such an array: the string.
