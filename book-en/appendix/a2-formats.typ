#import "../../book/lib.typ": *

= Appendix B — printf and scanf formats in full

Chapter 22 learned the minimal set (`%d`, `%s`, `%%`) and chapter 58 dissected the
format string. This appendix is a place *for looking things up* — read only the line
you need. One rule goes first: if the format and the argument's type go out of step it
is outside the contract (undefined behaviour) (chapters 44 and 58).

== The skeletons of a format specification

Output and input have similar syntax but different meanings. First the two skeletons
side by side.

#dtable(
  columns: 2,
  [*output (the `printf` family)*], [*input (the `scanf` family)*],
  [`%` `[flags]` `[width]` `[.precision]` `[length]` `conversion`],
  [`%` `[*]` `[max width]` `[length]` `conversion`],
  [width = the *minimum* number of characters — it fills if short and does not cut if over],
  [width = the *maximum* number of characters to read — the means of protecting the buffer],
  [there is a precision (`.`)], [there is no precision],
  [`*` takes the width or precision as an argument], [`*` reads but does not store (suppression)],
)

== Output conversions — the whole list

#dtable(
  columns: 4,
  [*conversion*], [*argument type*], [*output form*], [*note*],
  [`%d` `%i`], [`int`], [a decimal signed integer], [in output the two are the same],
  [`%u`], [`unsigned int`], [a decimal unsigned integer], [],
  [`%o`], [`unsigned int`], [octal], [attach `#` and a `0` goes in front],
  [`%x` `%X`], [`unsigned int`], [hexadecimal lower and upper case], [attach `#` and `0x`/`0X` goes in front],
  [`%f` `%F`], [`double`], [`123.456000`], [default precision 6],
  [`%e` `%E`], [`double`], [`1.234560e+02`], [exponential notation],
  [`%g` `%G`], [`double`], [automatically the shorter], [it removes trailing zeros],
  [`%a` `%A`], [`double`], [`0x1.edp+6`], [hexadecimal floating point (C99) — the bits seen exactly],
  [`%c`], [`int` (as a character)], [one character], [the argument comes promoted to `int`],
  [`%s`], [`char *`], [the string up to the NUL], [the maximum length can be limited by precision],
  [`%p`], [`void *`], [implementation-defined notation], [usually a hexadecimal address],
  [`%n`], [`int *`], [(no output)], [★ it *writes* the number of characters printed — not used, for security],
  [`%%`], [—], [one `%` character], [],
)

== Flags, width, precision

#dtable(
  columns: 3,
  [*place*], [*notation*], [*meaning*],
  [flag], [`-`], [left alignment (the default is right)],
  [], [`0`], [fill the spare places with zeros (ignored together with `-`)],
  [], [`+`], [attach a sign even to positives],
  [], [space], [one space before positives],
  [], [`#`], [alternative form — `%#o`→`0`, `%#x`→`0x`, `%#f`→always a decimal point],
  [width], [`%8d`], [at least 8 places],
  [], [`%*d`], [take the width as an argument: `printf("%*d", 8, n)`],
  [precision], [`%.3f`], [to three decimal places (rounded)],
  [], [`%.5d`], [an integer to at least 5 digits (zeros in front)],
  [], [`%.4s`], [a string to at most 4 characters],
  [], [`%.*s`], [the maximum length as an argument: `printf("%.*s", len, p)`],
)

`%.*s` is a form used often in this book — because a string that carries its length
separately (chapters 55 and 86's views) can be printed as it is, without NUL
termination.

One thing must be kept, though. *The width or precision argument that `*` takes is of
type `int`.* Pass a length carried as a `size_t` (as views mostly do) straight in and
the variadic argument's type goes out of step — the very mismatch seen in chapter 55.
The canonical form checks the length and then casts.

```c
if (v.size <= INT_MAX)
    printf("%.*s", (int)v.size, (const char *)v.ptr);
```

== Length modifiers — the place that tells the type's width

Type information does not ride along into variadic arguments (chapter 55), so this
letter is itself the contract. Get it wrong and the stack is read wrongly.

#dtable(
  columns: 4,
  [*modifier*], [*conversions used with*], [*output argument*], [*input argument*],
  [(none)], [`d i u o x X`], [`int` / `unsigned int`], [`int *` / `unsigned *`],
  [`hh`], [`d i u o x X`], [`int` (interpreted at char width)], [`signed char *`],
  [`h`], [`d i u o x X`], [`int` (interpreted at short width)], [`short *`],
  [`l`], [`d i u o x X`], [`long`], [`long *`],
  [`ll`], [`d i u o x X`], [`long long`], [`long long *`],
  [`j`], [`d i u o x X`], [`intmax_t`], [`intmax_t *`],
  [`z`], [`d i u o x X`], [`size_t`], [`size_t *`],
  [`t`], [`d i u o x X`], [`ptrdiff_t`], [`ptrdiff_t *`],
  [`L`], [`f e g a`], [`long double`], [`long double *`],
  [`l`], [`c` / `s`], [`wint_t` / `wchar_t *`], [`wchar_t *`],
)

Reals need particular care. In *output* a `float` becomes a `double` by the default
promotion, so `%f` alone suffices and `%lf` means the same. In *input* there is no
promotion, so `%f` must be `float *` and `%lf` `double *` without fail (chapter 58's
misconception box).

== Formats for fixed-width integers

For a type such as `int32_t` the real type differs by platform, so writing the format
by hand can go out of step. The standard put macros in `<inttypes.h>`.

```c
#include <inttypes.h>

uint64_t total = 1234567890123ULL;
printf("total = %" PRIu64 "\n", total);   /* it is string concatenation */

int32_t n = 0;
sscanf(line, "%" SCNd32, &n);
```

The naming rule is simple — for output `PRI` + conversion + width (`PRId32`, `PRIu64`,
`PRIx16`), for input `SCN` + conversion + width (`SCNd32`, `SCNu64`). Besides
`8`, `16`, `32`, `64`, the width place also takes `MAX` (`PRIdMAX`) and `PTR`
(`PRIxPTR`).

== Input conversions — the whole list

#dtable(
  columns: 4,
  [*conversion*], [*what it reads*], [*argument*], [*note*],
  [`%d`], [a decimal integer], [`int *`], [it skips leading whitespace],
  [`%i`], [an integer (base automatic)], [`int *`], [`0x` means hex, a leading `0` octal],
  [`%u`], [unsigned decimal], [`unsigned *`], [it accepts a minus sign too and wraps],
  [`%o` `%x`], [octal, hexadecimal], [`unsigned *`], [],
  [`%f` `%e` `%g`], [a real], [`float *`], [★ for `double` it is `%lf`],
  [`%c`], [the character as it is], [`char *`], [★ it does not skip whitespace. give a width and it reads that many],
  [`%s`], [up to whitespace], [`char[]`], [★ always give a maximum width (`%63s`)],
  [`%[...]`], [characters belonging to a set], [`char[]`], [`%[^,]` is characters that are not a comma. it reads whitespace too],
  [`%p`], [pointer notation], [`void **`], [only what was printed with `%p` is read back],
  [`%n`], [(reads nothing)], [`int *`], [it writes the number of characters consumed so far],
  [`%%`], [the `%` character], [—], [],
)

Insert the suppression character `*` and it reads without storing —
`sscanf(s, "%*d %d", &n)` throws away the first number and takes only the second. A
suppressed item is not counted in the return value either.

The set specifier is close to a small tool of its own. `%[abc]` gathers only a, b and
c; `%[^,\n]` gathers characters that are neither a comma nor a newline. Range notation
(`%[a-z]`) is widely used but not guaranteed by the standard.

== Rules that hold in input alone

- *One space in the format* means "any number of whitespace characters (none is fine
  too)". A newline is whitespace too.
- *An ordinary character in the format* must match the input exactly. If it goes out of
  step it stops on the spot with *a matching failure*.
- *The return value* is the number of items successfully filled. If the input has
  already ended it is `EOF` (a negative value), and that differs in meaning from "the
  format was wrong" (0).
- *A failed argument is not touched.* Use it without checking and you mistake the
  previous value for new input (chapter 81's counterexample is that).
- *Only `%c` and `%[` do not skip whitespace.* The accident of a newline left from the
  previous line going into the next `%c` arises here — put a space in front, as in
  `" %c"`, and it skips.
- *It does not report overflow.* Giving `99999999999` to a `%d` is outside the
  contract. If a range check is needed, use the `strtol` family or chapter 87's
  scanner.

== The return values organised

#dtable(
  columns: 3,
  [*function*], [*on success*], [*failure and boundaries*],
  [`printf` `fprintf`], [the number of characters printed], [negative],
  [`sprintf`], [the number of characters written (excluding the NUL)], [negative],
  [`snprintf`], [the number of characters *that would have been needed*], [negative. if the return value is at least the buffer size it was truncated],
  [`scanf` `sscanf`], [the number of items filled], [0 (matching or conversion failure) or `EOF`],
  [`fgets`], [the buffer pointer], [null (end of file or an error)],
  [`fputs` `puts`], [a non-negative value], [`EOF`],
)

`snprintf`'s return value rule matters especially — being *the number of characters
needed, not the number written*, the truncation check is
`need >= (int)sizeof buf` (chapter 81).

== A collection of common mistakes

#dtable(
  columns: 3,
  [*wrong code*], [*what happens*], [*the mend*],
  [`printf("%d", sz)` — a `size_t`], [the width goes out of step on 64-bit], [`%zu`],
  [`printf("%s", 42)`], [it reads an integer as an address — collapse], [`%d`],
  [`printf(user)`], [the format string vulnerability (chapter 58)], [`printf("%s", user)`],
  [`scanf("%d", n)`], [a value was passed, not an address], [`&n`],
  [`scanf("%s", buf)`], [it writes past the boundary], [specify a width, as in `%63s`],
  [`scanf("%f", &d)` — a `double`], [only half is filled], [`%lf`],
  [`scanf("%c", &c)` in succession], [it reads the leftover newline], [`" %c"`],
  [ignoring the return value], [failure passes by quietly], [check the item count],
  [using `%n`], [it becomes a passage for memory writes], [do not use it],
)
