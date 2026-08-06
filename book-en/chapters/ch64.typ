#import "../../book/lib.typ": *

= Locales ② — numbers, money, time and sorting

#prereq(
  ([chapter 63, Locales ①], [categories and `setlocale`]),
  ([chapter 58, Streams in practice], [`printf` formats]),
  ([chapter 69, Time], [`strftime`]),
)

#deepqa[
  Chapter 63 said a locale is data holding conventions. What exactly is that data
  made of, and how does a program get at it?
][
  The window is surprisingly narrow — one function (`localeconv`) and one struct
  (`struct lconv`). Every convention about numbers and money sits in that struct's
  twenty-four members.

  For the rest there is *no window at all.* `strftime` writes dates by itself and
  `strcoll` compares by itself — the standard gives a program no way to look at
  that data. This asymmetry is the map of the chapter: what you *fetch* (numbers
  and money) and what you *delegate* (time and sorting).
]

#organizer[
#idx("localeconv")  What a locale actually changes, one at a time. All twenty-four members of
  `struct lconv`, how the grouping string is encoded, the rule by which a
  monetary form is assembled, the pattern in which `LC_NUMERIC` corrupts data,
  `strftime`'s locale-dependent and locale-independent formats, `strcoll` and
  sort keys, and locales in threads.
]

#chapter-questions()

== One window — `localeconv`

```c
struct lconv *localeconv(void);
```

What it returns is the address of a struct filled with *the numeric and monetary
conventions of the current locale.* Two rules attach. A program *must not modify*
the contents, and a later `localeconv` or a `setlocale` with `LC_ALL`,
`LC_MONETARY` or `LC_NUMERIC` *may overwrite* them. It is a value to read on the
spot, not to hold on to.

#demo("examples-en/ch64/lconv.c")

=== A map of the twenty-four members

The standard says the struct shall contain *at least* the following members, in
any order. So reach them by name, never by position or designated order.

#dtable(
  columns: 3,
  [*Member*], [*What*], [*"C" locale*],
  [`decimal_point`], [Decimal point, plain numbers], [`"."`],
  [`thousands_sep`], [Group separator, plain numbers], [`""`],
  [`grouping`], [Grouping rule, plain numbers], [`""`],
  [`mon_decimal_point`], [Decimal point, money], [`""`],
  [`mon_thousands_sep`], [Group separator, money], [`""`],
  [`mon_grouping`], [Grouping rule, money], [`""`],
  [`positive_sign`], [The string for a non-negative amount], [`""`],
  [`negative_sign`], [The string for a negative amount], [`""`],
  [`currency_symbol`], [The local currency symbol], [`""`],
  [`frac_digits`], [Digits after the decimal point], [`CHAR_MAX`],
  [`p_cs_precedes`, `n_cs_precedes`], [Symbol before the value? (1/0)], [`CHAR_MAX`],
  [`p_sep_by_space`, `n_sep_by_space`], [A space between symbol and value (0/1/2)], [`CHAR_MAX`],
  [`p_sign_posn`, `n_sign_posn`], [Where the sign goes (0\~4)], [`CHAR_MAX`],
  [`int_curr_symbol`], [International symbol + one separator character], [`""`],
  [`int_frac_digits` and six more `int_*`], [The same items for the international form], [`CHAR_MAX`],
)

Two conventions must be read. A string member of `""` means *this locale does not
specify that value* (`decimal_point` alone always has one), and a `char` member of
`CHAR_MAX` means the same. The `CHAR_MAX` values the demonstration printed in the
`"C"` locale are exactly that — the C locale says nothing at all about money.

#idx("ISO 4217")For `int_curr_symbol` the standard reaches straight into another international
standard. *The first three characters are the alphabetic international currency
symbol of ISO 4217*, and the fourth is the character separating that symbol from
the amount. That is why `"KRW "` and `"EUR "` came out with a trailing space.

=== How the grouping string is encoded

`grouping` is not a string for people to read; it is *an array of numbers*. The
standard fixes the reading.

#dtable(
  columns: 2,
  [*Element value*], [*Meaning*],
  [`CHAR_MAX`], [No further grouping is to be performed],
  [`0`], [Repeat the previous element for the remaining digits],
  [Anything else], [The size of the group at this position; the next element sizes the group before it],
)

So `"\3"` looks as though it should mean "one group of three and no more", yet
real locales produce `1,234,567` from it, because glibc treats the last element as
repeating. To be explicit, write `"\3\0"`.

What the demonstration measured is en_IN's `3 2`. Three digits from the right,
then two at a time — `12345678` becomes `1,23,45,678`. It is India's lakh-crore
system, and living proof that *the assumption of fixed groups of three is wrong.*

=== A monetary form is assembled from three values

Why does printing one amount need six members? Because the real forms differ that
much. The combination of `p_cs_precedes` (symbol first?), `p_sep_by_space`
(a space?) and `p_sign_posn` (where the sign goes) decides the form.

#dtable(
  columns: 2,
  [`p_sign_posn`], [*Meaning*],
  [0], [Parentheses surround the quantity and the currency symbol],
  [1], [The sign string precedes the quantity and the symbol],
  [2], [The sign string follows the quantity and the symbol],
  [3], [The sign string immediately precedes the currency symbol],
  [4], [The sign string immediately follows the currency symbol],
)

The standard carries the resulting table itself. With `$` as the symbol and `+`
as the sign, printing `1.25` splits like this (an excerpt).

#dtable(
  columns: 4,
  [`p_cs_precedes`], [`p_sign_posn`], [`p_sep_by_space`=0], [`p_sep_by_space`=1],
  [0], [0], [`(1.25$)`], [`(1.25 $)`],
  [0], [1], [`+1.25$`], [`+1.25 $`],
  [0], [3], [`1.25+$`], [`1.25 +$`],
  [1], [0], [`($1.25)`], [`($ 1.25)`],
  [1], [1], [`+$1.25`], [`+$ 1.25`],
  [1], [4], [`$+1.25`], [`$+ 1.25`],
)

That *the accountant's parentheses* (a `p_sign_posn` of 0) are in the standard is
worth noticing — the convention of writing a negative as `(1,234)` rather than
`-1,234`.

#qa[
  Then which standard function prints an amount?
][
  *There is none in standard C.* `localeconv` hands over the materials; the
  assembly is the program's job. You must look at those six members and build the
  string as the table above says.

  POSIX has `strfmon` to do it for you (`strfmon(buf, n, "%n", 1234.5)`). Windows
  has `GetCurrencyFormat`. Neither is standard C, so where portability matters you
  assemble it yourself or use a library.

  A more important discipline, in passing: *do not hold money in a `double`*
  (chapter 47). Keep it as an integer number of the smallest unit and insert the
  decimal point only when displaying.
]

== `LC_NUMERIC` — where data is quietly corrupted

The most practical warning in this chapter. `LC_NUMERIC` changes not only what
`localeconv` reports but *the decimal-point character `printf`, `scanf` and
`strtod` themselves use.*

The last part of the demonstration is that. The same `printf("%.2f", 1234.5)`
prints `1234.50` in one place and `1234,50` in another. And `strtod("3.14", …)`
stops after `3` in a locale whose decimal point is a comma.

#realcase[
  How one decimal point stopped a server
][
  The same program writing `3.14` on the developer's machine (English locale) and
  `3,14` on the user's (German locale) has happened over and over.

  - *A configuration file cannot be read back* — written as `3,14`, expected as
    `3.14`.
  - *A CSV loses its columns* — the comma inside a value collides with the column
    separator.
  - *JSON between two servers disagrees* — the JSON standard nails the decimal
    point to `.`, and `printf` writes `,`.
  - *Numbers in the logs cannot be aggregated.*

  They have one thing in common. All of them sent *a number a machine will read*
  down the path meant for people. There is one prescription — pin `LC_NUMERIC` to
  `"C"` with chapter 63's idiom, and format separately when showing a person.
]

#misconception[
  "Our service is only used inside one country, so locales are not our problem"
][
  It catches you in two places. First, *the locale is settled by the user's
  machine.* The same program runs under a different locale on someone else's
  computer — the desktop's settings, or a container image's environment
  variables.

  Second, *a Korean locale is not `"C"` either.* `ko_KR.UTF-8` happens to use `.`
  as its decimal point, but its grouping, dates and sorting all differ. Try to
  parse back a date printed with `%c` and it catches you there.
]

== `LC_TIME` — writing dates and times

The time arithmetic itself has nothing to do with the locale (chapter 69). What
the locale changes is *the writing*, and the window is `strftime`'s conversion
specifiers.

#demo("examples-en/ch64/time_locale.c")

The demonstration prints one instant in six locales. The knack is to split the
specifiers into two groups.

#dtable(
  columns: 3,
  [*Group*], [*Specifiers*], [*Nature*],
  [Locale decides], [`%c` `%x` `%X` `%A` `%a` `%B` `%b` `%p` `%r`], [Differs by country — *only for showing people*],
  [Locale-independent], [`%Y` `%m` `%d` `%H` `%M` `%S` `%j` `%F` `%T`], [The same everywhere — *for recording, sending, parsing*],
)

`%F %T` produces ISO 8601 (`2026-08-06 15:04:05`) exactly. Using only these in
logs, file names and API responses is the discipline. In the demonstration these
two are the only lines identical across all six locales.

#platform[
  The time zone is not the locale
][
  A common mix-up. What `%Z` (zone name) and `%z` (offset) show is settled by the
  *`TZ` environment variable* and `tzset`, not by `LC_TIME`. "I changed the locale
  to Korea and the time is still wrong" usually means `TZ` was not changed.

  Locale and time zone are different axes — *the locale says how to write it, the
  zone says when it is.* Printing Seoul time with a German locale is a perfectly
  normal combination.
]

== `LC_COLLATE` — `strcmp` is not dictionary order

`strcmp` compares *byte values*. So `"Zebra"` sorts before `"apfel"`
(`Z`=0x5A \< `a`=0x61), and Hangul lines up in code-point order. That is not what
a reader expects.

#demo("examples-en/ch64/collate.c")

The result shows the difference plainly. In the `"C"` locale `strcoll` and
`strcmp` give the same answer, but elsewhere they part — in German, `Äpfel` comes
right after `apfel`, and case is interleaved rather than separated.

=== `strxfrm` — why such a function exists

`strxfrm` turns a string into a *sort key*. Keys compared with an ordinary
`strcmp` come out in the same order as `strcoll`. The last part of the
demonstration confirms it.

Why is it needed? One `strcoll` is not cheap — the locale's rules must be applied
every time. Sorting `n` items takes roughly $n log n$ comparisons, so it is better
to *transform `n` times and compare cheaply.*

```c
/* build the keys before sorting */
size_t need = strxfrm(NULL, s, 0);   /* ask for the size first */
char *key = malloc(need + 1);        /* take that much */
strxfrm(key, s, need + 1);           /* and fill it */
```

The key sizes the demonstration printed reveal the function's character. In the
`"C"` locale the key is the same 6 bytes as the original; in the German locale it
is 43 — locale collation stacks *several levels of weight* (base letter, then
accent, then case).

#qa[
  Is `strcoll` enough for Korean sorting?
][
  For a simple list, yes. Hangul syllables are already arranged in dictionary
  order in Unicode, so `ko_KR.UTF-8`'s `strcoll` gives the expected order.

  Real-world sorting adds rules, though — natural number order ("file2" before
  "file10"), grouping by initial consonant, folding Chinese characters by their
  reading, ignoring case and spaces. That is beyond `strcoll` and belongs to
  Unicode's collation algorithm (UTS \#10) and its implementation, ICU.

  Draw the line like this — *`strcoll` for what locale collation covers, a
  dedicated library beyond it.* Only avoid sorting a human-facing list with
  `strcmp`.
]

== Locales and threads

Chapter 63 said the locale is process-global. In a program with several threads
that becomes a problem — one thread changing the locale to print a date for a
user shakes another thread's `printf("%f")`.

Standard C has no remedy. What exists are extensions.

#dtable(
  columns: 3,
  [*System*], [*Means*], [*Shape*],
  [POSIX], [Thread-local locale], [`newlocale`/`uselocale`/`freelocale`],
  [POSIX], [Functions taking a locale], [`strtod_l`, `strcoll_l`, `strftime_l`, …],
  [Windows], [Per-thread locale mode], [`_configthreadlocale`, `_locale_t` and `_l` functions],
)

The `_l` family is the real remedy — it names the locale *for that call only*,
touching no global state. Where portability matters you end up writing a thin
layer over the two.

#antipattern[
  Calling `setlocale` inside a library
][
  ```c
  /* a library function */
  double parse_number(const char *s) {
      setlocale(LC_NUMERIC, "C");     /* changes someone else's program state */
      return strtod(s, NULL);
  }
  ```
  These three lines quietly wreck the application's date and currency formatting.
  And between threads it is a data race.

  A library has one discipline — *read the locale, never change it.* If you need
  parsing that the locale cannot shake, use `strtod_l` or handle the digits
  yourself.
]

== Prescriptions

#dtable(
  columns: 2,
  [*What you want*], [*How*],
  [Dates, sorting and money in the user's own way], [`setlocale(LC_ALL, "")`],
  [Writing numbers into files and protocols], [Pin `LC_NUMERIC` to `"C"`],
  [Timestamps in logs], [`strftime` with `%F %T` (ISO 8601)],
  [Sorting a list for people], [`strcoll` (or `strxfrm` keys)],
  [Strings a machine compares], [`strcmp` — it must not be shaken by the locale],
  [A different locale per thread], [`uselocale` / the `_l` family (outside the standard)],
  [Writing a library], [Do not call `setlocale`],
)

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [The window], [`localeconv` alone. Do not modify it, do not keep it],
    [`struct lconv`], [Twenty-four members. `""` and `CHAR_MAX` mean "not specified"],
    [`int_curr_symbol`], [The first three characters are an ISO 4217 code],
    [`grouping`], [An array of numbers. `CHAR_MAX`=stop, `0`=repeat. Not always three],
    [Money], [Assembled from `cs_precedes`, `sep_by_space`, `sign_posn`. No standard function],
    [`LC_NUMERIC`], [★ Changes the decimal point of `printf` and `strtod` — the chief corrupter],
    [`LC_TIME`], [`%c %x %X %A %B %p` follow the locale; `%F %T %Y-%m-%d` do not],
    [Time zone], [`TZ`, not the locale],
    [`LC_COLLATE`], [`strcmp` ≠ dictionary order. Repeated comparison → `strxfrm` keys],
    [Threads], [Nothing in the standard. `uselocale`, the `_l` family],
  )
]

We have followed what a locale changes to the end. One axis remains — the
*multibyte and wide characters* governed by `LC_CTYPE`. The next chapter takes up
what `wchar_t` really is and how a byte string unfolds into characters, one step
at a time.
