#import "../../book/lib.typ": *

= The drawer of odds and ends — `<stdlib.h>`

#prereq(
  ([chapter 53, The terrain of the standard library], [the terrain of the standard library]),
  ([chapter 40, Dynamic memory], [allocating and giving back]),
)

#organizer[
  A drawer named, exactly as it says, "the standard library". Functions that turn
  strings into numbers, random numbers, dynamic allocation, program termination,
  sorting and binary search are all in here together. In place of anything in
  common there are many traps — in particular *conversion functions with no way to
  report failure* and *the subtle differences between the terminating functions*
  are this chapter's two axes.
]

#deepqa[
  Chapter 25 read input with `fgets` and parsed it with `sscanf`, and chapter 53
  said `sscanf` does not tell you "where and why it failed". Then what is the most
  accurate way to turn one string into an integer?
][
  The `strtol` family. This function tells three things at once — the converted
  value, *where it stopped* (the end pointer), and whether the range was exceeded
  (`errno` being `ERANGE`). `atoi` tells none of them. Its name is short so it
  appears often in introductions, but it is a function not used in the field.
]

#chapter-questions()

== Conversion — why `atoi` is abandoned

#demo("examples-en/ch58/convert.c")

Put the output side by side and the difference is clear.

- `atoi("abc")` is 0. But `atoi("0")` is 0 too — *failure and success cannot be
  distinguished.*
- `atoi("42abc")` is 42. It quietly ignores the rubbish attached behind.
- `atoi("99999999999999999999")` came out as −1. The standard says only that this
  case is *undefined behaviour* — −1 came out by accident; any value at all could
  come out and nothing is guaranteed.

`strtol` distinguished, for the same inputs, "not a number", "characters left
over" and "out of range" separately. The checking code looks long, but that length
is precisely *the real complexity of handling a string that came from outside*.

#antipattern[
  Reading user input with `atoi`
][
  ```c
  int port = atoi(argv[1]);      /* "0" and "bad input" are the same value */
  ```
  Mended, it goes like this.
  ```c
  errno = 0;
  char *end;
  long v = strtol(argv[1], &end, 10);
  if (end == argv[1] || *end != '\0') { /* not a number */ }
  else if (errno == ERANGE || v < 1 || v > 65535) { /* out of range */ }
  else port = (int)v;
  ```
  *Setting `errno` to 0 just before the call* is the convention (chapter 62).
  Without it you read the value a previous call left behind.
]

Real conversion is the same. `atof` cannot report failure, while `strtod` gives an
end pointer and `ERANGE`. Moreover `strtod` may, according to the locale, read the
decimal point as `,` rather than `.` (chapter 59) — a point always to be remembered
when parsing a data format.

#qa[
  Why does `qsort` take its comparison through two `void *` — would knowing the type not be faster?
][
  Because the standard library must sort *an array of any type at all*. C has no
  generics, so the only passage that erases a type is `void *` (chapter 33), and
  the price is a cast and a dereference inside the comparator every time. The
  price is not only speed. Where the type has been erased, a mistake gets no help
  from the compiler: pass the wrong element size, or write a comparator that takes
  `int` where it must take `int*`, and it collapses quietly. That is why chapter
  76's `proven_array_sort` pins the type with a macro, and why chapter 69 counts
  "the unchecked callback" among the five bugs.
]

== Dynamic allocation — four functions and their contracts

We organise what chapter 40 taught, function by function.

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*contract and traps*],
  [`malloc(n)`], [allocate n bytes], [the content is undetermined. null on failure],
  [`calloc(k, n)`], [allocate k×n bytes + fill with zeros], [★ *the implementation* checks the multiplication for overflow],
  [`realloc(p, n)`], [change the size], [★ on failure the original is kept — assigning the return value straight to the original leaks],
  [`free(p)`], [release], [null is safe. freeing twice is outside the contract],
  [`aligned_alloc(a, n)`], [aligned allocation (C11)], [n must be a multiple of a],
)

`calloc`'s multiplication check is where it is useful. `malloc(k * n)` may have the
product wrap round as seen in chapter 57, but for `calloc(k, n)` the standard
requires the implementation to check for overflow and return null. *If sizes must
be multiplied, `calloc` is the safer choice.*

#antipattern[
  Assigning `realloc`'s return value straight to the original
][
  ```c
  buf = realloc(buf, new_size);   /* on failure the original address is lost → a leak */
  if (!buf) return -1;
  ```
  The correct idiom goes through a temporary variable.
  ```c
  char *tmp = realloc(buf, new_size);
  if (!tmp) { /* buf is still valid — it can be cleaned up or gone on using */ return -1; }
  buf = tmp;
  ```
  It is why chapter 56's line-reading example used this idiom.
]

`realloc` has two more peculiar rules. `realloc(NULL, n)` is the same as
`malloc(n)`, and *`realloc(p, 0)` is not to be used.* Its status changed from
edition to edition — up to C17 it was *implementation-defined* and marked as
deprecated, and in C23 it became outright *undefined behaviour* (proposal N2464).
It is a place where implementations diverged so far that the standard gave up on
settling it.

What this change leaves in practice is one line — *to release, use `free(p)`.* Code
that reallocates to a size that may become 0 must filter that case first.

```c
proven_err_t resize(char **buf, size_t n) {
    if (n == 0) { free(*buf); *buf = nullptr; return OK; }  /* never pass 0 */
    char *tmp = realloc(*buf, n);
    ...
}
```

== Termination — the difference between four ways

#dtable(
  columns: 3,
  [*way*], [*cleanup*], [*where it is used*],
  [`return` (in main)], [the same as `exit`], [normal termination],
  [`exit(status)`], [runs `atexit`, flushes and closes streams], [normal termination (from deep inside)],
  [`quick_exit(status)`], [runs only `at_quick_exit`, no flush], [quick termination (C11)],
  [`_Exit(status)`], [does nothing], [special places such as a child process],
  [`abort()`], [no cleanup, an abnormal-termination signal], [an unrecoverable error],
)

The heart of it is *the buffer*. `exit` empties the streams while `_Exit` and
`abort` do not — the "output vanishing" accident seen in chapter 56 happens here.
It is also why the last log of a program dying by `abort` is not seen.

Functions registered with `atexit` are called in the *reverse* order of
registration, and the standard guarantees registration of at least 32. Calling
`exit` again inside a registered function is outside the contract.

== Sorting and searching — `qsort` and `bsearch`

The functions chapter 54 foretold. Written out exactly, the contract is this.

- The comparator is `int cmp(const void *a, const void *b)` and returns a
  negative, zero or positive value. *Making it by subtraction can overflow* —
  `return *x - *y;` is wrong for large values.
  `return (*x > *y) - (*x < *y);` is the safe idiom.
- The comparator must be a *total order* (chapter 69). If it is inconsistent the
  result is not merely jumbled — it can trespass outside the array.
- `qsort` is *not a stable sort.* The relative order of equal values is not
  preserved. If it is needed, lay the original index on top in the comparator to
  break ties.
- Worst-case performance is settled by the implementation. The standard guarantees
  nothing — the reason chapter 54's complexity attack was possible.
- `bsearch` presumes *a sorted array*. If it is not sorted the result is
  meaningless.

== Random numbers — the limits of `rand`

`rand` returns a number from 0 to `RAND_MAX`. `RAND_MAX` is guaranteed only to be
at least 32767, so if a larger range is needed it must be composed.

#antipattern[
  Making a range with `rand() % n`
][
  ```c
  int dice = rand() % 6 + 1;      /* the values are not even */
  ```
  If `RAND_MAX + 1` is not a multiple of `n`, the values at the front come out
  more often. If the range is small the bias is small too, but as `n` grows it
  becomes noticeable. Moreover some old implementations had poor quality in the low
  bits, so `% 2` even came out alternating.

  If an even distribution is needed, use *rejection sampling* — throw away a value
  that exceeds the range and draw again. And *never use it for secrets* (Part XII's
  random number story).
]

Give no seed with `srand` and it is the same as `srand(1)` — the same sequence
every time. `srand(time(NULL))` is a common idiom, but two processes started in the
same second get the same sequence.

== The environment and processes

`getenv` returns an environment variable, but that string *must not be modified*
and may not be valid after a subsequent `setenv`-like call. If the value is needed,
copy it.

`system` raises a shell and executes a command. If user input is mixed into that
string it becomes *command injection* — the same class as the classic
vulnerability of web applications. Within the standard there is no alternative, and
the right answer is to use a platform API (`posix_spawn`, `CreateProcess`) and pass
the arguments as an array.

#realcase[
  A real bug made by subtraction in a `qsort` comparator
][
  A comparator of the form `return a - b;` is the most common mistake in sorting
  code. If the values are near `INT_MIN` the subtraction overflows and the sign
  flips, and the sort quietly gives a wrong result — since the signed overflow
  learned in chapter 7 is outside the contract, the symptom even changes with the
  compiler's optimisation.

  This pattern has been reported repeatedly in kernels, databases and game engines
  alike, common enough that static analysis tools catch it with a rule of their
  own. The mend is one line — do not subtract, compare.
]

#recap[
  `<stdlib.h>` in summary.

  #dtable(
    columns: 3,
    [*what you want to do*], [*what to use*], [*what to avoid*],
    [string → integer], [`strtol` + end pointer + `errno`], [`atoi`],
    [string → real], [`strtod` (mind the locale)], [`atof`],
    [allocate an array], [`calloc(k, n)`], [`malloc(k * n)`],
    [change the size], [`realloc` via a temporary variable], [assigning straight to the original],
    [normal termination], [`return`/`exit`], [`_Exit` (buffer loss)],
    [sorting], [`qsort` + a total-order comparator], [a subtracting comparator, assuming stability],
    [random numbers], [rejection sampling, a generator fit for the purpose], [`rand() % n`, using it for secrets],
    [external commands], [an argument array through a platform API], [joining input into `system`],
  )
]

The drawer is tidied. The next chapter is the functions that handle a single
character — and the fact that those functions are tied to the global state called
the locale.
