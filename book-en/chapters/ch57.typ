#import "../../book/lib.typ": *

= Strings and memory — `<string.h>`

#organizer[
  The header in which the most accidents have happened in C. Functions that do not
  take a size, `strncpy` which is not safe despite its name, copying that touches
  overlapping regions, `strtok` which destroys the original and hides state — the
  dangers of strings learned in chapter 37 take concrete shape here function by
  function. We also see the real portability of the non-standard alternatives (the
  `strlcpy` family).
]

#deepqa[
  Chapter 37 said a C string is "up to the NUL", so its length must be counted
  every time, and chapter 54 said functions that do not take a size are the first
  chronic illness. Then does using `strncpy` instead of `strcpy` cure that illness?
][
  It does not. Contrary to the impression the name gives, `strncpy` *was not
  designed as a safe copying function.* Its original purpose was filling the
  fixed-length fields of old Unix (a 14-byte directory entry name, say) — so it
  fills all the spare places with zeros, and if there is not enough room it does
  not attach a NUL. Both properties go against the expectations of "string
  copying". This chapter's first example shows it before your eyes.
]

== The truth about `strncpy`

#demo("examples/ch57/strncpy.c")

Two things come out.

*First, it fills all the spare places with zeros.* Put 3 characters into a 10-byte
buffer and it writes zeros over the remaining seven. The bigger the buffer, the
bigger the waste.

*Second, when it fits exactly or overflows it does not attach a NUL.* Put `abcd`
into a 4-byte buffer and there is no room for the NUL, so what remains is *a byte
array, not a string*. Print that with `%s` or pass it to `strlen` and it reads
outside the buffer — the typical route of "I used the safe function and it blew
up."

gcc really does catch this mistake. Here is the diagnosis received when first
writing the example.

```text
error: ‘strncpy’ output truncated before terminating nul copying 4 bytes
       from a string of the same length [-Werror=stringop-truncation]
```

But as seen in chapter 54, the compiler catches *only what it can see*. If the
source's length is settled during execution, this warning does not appear.

#antipattern[
  Copying "safely" with `strncpy`
][
  ```c
  char dst[32];
  strncpy(dst, src, sizeof dst);      /* there may be no NUL */
  printf("%s\n", dst);                 /* it reads outside the buffer */
  ```
  It must be mended at least like this.
  ```c
  strncpy(dst, src, sizeof dst - 1);
  dst[sizeof dst - 1] = '\0';          /* close it by hand */
  if (strlen(src) >= sizeof dst) { /* truncated — handle it */ }
  ```
  Three lines are needed, and leaving out even one of them is an accident. That is
  why this function is assessed as "a safety device that is hard to use."
]

== Then what is used

Within the standard, the most practical tool for safely joining strings is in fact
in `<stdio.h>`.

```c
int need = snprintf(dst, sizeof dst, "%s", src);
if (need < 0 || (size_t)need >= sizeof dst) { /* truncated */ }
```

There is the criticism that it is slow (the cost of interpreting the format), but
it is the only standard function that keeps the boundary while letting you *know
about truncation*.

#dtable(
  columns: 4,
  [*function*], [*status*], [*boundary*], [*can truncation be known*],
  [`strcpy`, `strcat`], [standard], [none], [—],
  [`strncpy`], [standard], [yes], [no (must be measured by hand)],
  [`strncat`], [standard], [yes (but the argument is the *remaining room*)], [no],
  [`snprintf`], [standard], [yes], [yes (the return value)],
  [`strlcpy`, `strlcat`], [the BSD family, a C23 annex], [yes], [yes (the return value)],
  [`strcpy_s`, `strcat_s`], [C11 annex K (optional)], [yes], [yes (an error return)],
)

`strncat`'s argument is a particular trap — the second argument is not *the
destination's size* but *the number of bytes that may additionally be written*.
`strncat(dst, src, sizeof dst)` is almost always wrong, and
`sizeof dst - strlen(dst) - 1` is right.

#realcase[
  Why `strlcpy` was not standard
][
  OpenBSD put out `strlcpy` and `strlcat` in 1998. They take the destination's
  size, always close with a NUL, and return *the length of the source* so that
  truncation can be known. They spread through the BSD family and several
  libraries, but glibc long refused to adopt them — the counter-argument being that
  "an API that quietly permits truncation only moves the problem."

  So code using `strlcpy` was long unportable on Linux, and every project came to
  have its own edition. In 2023 glibc 2.38 finally added them and C23 brought in
  functions of similar intent as an annex, but *the state in which you must check
  the target platform's edition before saying "it can be used"* persists. It is a
  representative case showing the gap between the standard and reality.
]

== Overlapping regions — `memcpy` and `memmove`

#demo("examples/ch57/overlap.c")

`memcpy`'s contract includes "the two regions must not overlap". Calling it with
them overlapping is undefined behaviour, and in an optimised implementation values
really do get scrambled — because there is no guarantee that bytes are moved in
order (several bytes may be moved at once with SIMD, or moved from the back).

If they may overlap, it is `memmove`. Contrary to the impression its name gives,
it does not mean "moving" but *copying that is safe even when overlapping*.

#misconception[
  "`memcpy` is always faster than `memmove`"
][
  An old saying. Today the performance difference between the two is mostly
  negligible, and in some implementations they converge on the same code. The
  reason `memcpy` can be faster is that it can use the premise "they do not
  overlap" in optimisation, and if that premise is set wrongly, what is lost (a
  bug that is hard to find) is far greater than what is gained (a few nanoseconds).
  *If there is the slightest possibility of overlap, `memmove`* — that is the
  modern default.
]

== `strtok` — it destroys the original and hides state

The last part of the example. `strtok` has two sins.

*First, it destroys the original.* It makes tokens by overwriting the separators
with NUL. So it cannot be used on a read-only string (a string literal) — using it
there is outside the contract — and if the original is needed it must be copied
first.

*Second, it hides state inside the function.* That is why `NULL` is passed from
the second call onward. That state is *singular*, so if another function calls
`strtok` in the middle of cutting tokens the two wreck each other's traversal. In
a program running along several strands it gets worse.

There are three alternatives. Use an edition in which the caller holds the state,
such as `strtok_r` (POSIX) or `strtok_s` (annex K); cut it yourself with `strcspn`
and `strchr`; or use a tool that *does not touch the original*, like Part XII's
view-based splitting.

== The traps of the remaining functions

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*to beware of*],
  [`strlen`], [length], [with no NUL it runs away. $O(n)$ every time],
  [`strcmp`], [comparison in dictionary order], [only the sign of the return value means anything. 0 is "equal"],
  [`strncmp`], [compare the first n bytes], [if n exceeds the length it stops at the NUL],
  [`strchr`, `strrchr`], [find a character], [if the sought character is `'\0'` it points at the end],
  [`strstr`], [substring], [worst-case performance differs by implementation],
  [`strspn`, `strcspn`], [length by a set of characters], [the heart of the cutting idiom],
  [`memset`], [fill with a byte], [★ for erasing secrets it may vanish under optimisation],
  [`memcmp`], [compare bytes], [★ it compares padding too. it must not be used to compare structs],
)

The two starred entries are especially dangerous in practice.

*Erasing a secret with `memset`* — the `memset(key, 0, len)` that erases after use
may, if `key` is not read afterwards, be seen by the compiler as a "useless write"
and deleted (chapter 13's optimisation story). C11 put `memset_s` in annex K for
this, and each platform has a function such as `explicit_bzero` or
`SecureZeroMemory`.

*Comparing structs with `memcmp`* — because of the padding seen in chapter 41.
Even for two structs holding the same values, if the padding bytes differ `memcmp`
answers "different". The members must be compared one by one.

#qa[
  I hear `memcmp` is dangerous for comparing passwords too?
][
  Correct, for a different reason. `memcmp` returns the instant it meets a
  differing byte, so *the time the comparison took leaks how much of the front
  matched.* That means an attacker can measure time and get it right one byte at a
  time (a timing attack). When comparing a secret, use a constant-time comparison
  function that always takes the same time regardless of length — it is not in the
  standard; cryptographic libraries provide it.
]

#recap[
  `<string.h>` in summary.

  #dtable(
    columns: 3,
    [*what you want to do*], [*what to use*], [*what not to use*],
    [copy a string], [`snprintf` (or the platform's `strlcpy`)], [`strcpy`, a careless `strncpy`],
    [join], [`snprintf` in one go], [`strcat`, `strncat` with its confusing argument],
    [copy that may overlap], [`memmove`], [`memcpy`],
    [cut tokens], [`strcspn`/`strchr` or `strtok_r`], [`strtok`],
    [compare structs], [compare member by member], [`memcmp` (padding)],
    [compare secrets], [constant-time comparison], [`memcmp` (time leak)],
    [erase secrets], [the platform's explicit function], [`memset` (vanishes under optimisation)],
  )
]

We have passed the strings. The next chapter is the drawer of odds and ends and a
treasury of accidents — `<stdlib.h>`.
