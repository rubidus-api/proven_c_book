#import "../../book/lib.typ": *

= Locales ① — a program's regional settings

#prereq(
  ([chapter 62, Character classification], [that the judgement depends on the locale]),
  ([chapter 50, The three faces of `main`], [environment variables]),
)

#deepqa[
  Chapter 62 said the answer `isalpha` gives depends on "the current locale". But
  I have never set a locale. What does it mean for an answer to depend on
  something I never set?
][
  *Because one is already settled even if you set nothing.* The standard fixes
  that too — at program startup the state is as if `setlocale(LC_ALL, "C")` had
  been called (§7.11.1.1p4). So a program that does nothing runs in the minimal
  environment called the C locale.

  A locale is *the device by which settings outside the program change behaviour
  inside it.* It is what makes one executable print `2026년 8월 6일` in Seoul,
  `06.08.2026` in Berlin, and `3,14` from `printf("%f")`. This chapter is about
  the names and rules of that device; the next is about what it changes.
]

#organizer[
#idx("locale")  What a locale is, from the ground up. Why it exists, what the six categories
  divide, the exact contract of `setlocale`, by what rule and which international
  standards a name like `ko_KR.UTF-8` is built, the precedence of the environment
  variables, and where on the machine that data lives.
]

#chapter-questions()

== Why it exists — conventions differ by country

When a program shows something to a person, there is not one "correct" way to
write it.

#dtable(
  columns: 3,
  [*What*], [*Korea*], [*Germany*],
  [Decimal point], [`3.14`], [`3,14`],
  [Thousands], [`1,234,567`], [`1.234.567`],
  [Date], [`2026년 8월 6일`], [`06.08.2026`],
  [Currency], [`₩1,234`], [`1.234,00 €`],
  [Sorting], [Hangul order], [`ä` filed with `a`],
)

Writing this out by hand in every program multiplies the code by the number of
countries. So Unix and C took another road — *make the conventions data, keep
them outside the program, and let the program choose only which set to work
with.* That bundle of data is a locale.

#qa[
  Is a locale translation?
][
  No, and the distinction matters. A locale deals with *conventions* — decimal
  points, grouping, the order of a date, sorting, case, currency symbols.
  Turning the messages a program prints into another language, that is,
  *translation*, is outside standard C.

  Unix has a separate `LC_MESSAGES` category and tools such as `gettext` for
  translation, but it is not among the six categories C fixes. What this book
  covers stops at conventions too.
]

== A locale is process-global state

The nature of a locale in one line: *one locale per process.*

#demo("examples-en/ch63/locale_probe.c")

The first line of the demonstration shows the standard's rule in the flesh.
Before anything happens, `LC_ALL` is `C`. And one `setlocale(LC_ALL, "")` moves
it to whatever the environment says.

Two things follow from its being global.

*First, it can change without you calling anything.* If a library you linked
calls `setlocale`, your `printf("%f")` is affected from that moment. GUI toolkits
commonly do.

*Second, it is a race between threads.* C23 states this explicitly — a call to
`setlocale` may introduce *a data race* with other `setlocale` calls or with
functions affected by the locale (§7.11.1.1p5). If you want different locales in
different threads, standard C has no road; POSIX's `uselocale` family is needed
(chapter 64).

== The six categories — what governs what

A locale is not one lump; it divides into *categories*. The standard fixes six,
and it also enumerates what each one affects.

#dtable(
  columns: 3,
  [*Category*], [*What it governs*], [*Functions affected*],
  [`LC_CTYPE`], [Classification, case, *multibyte conversion*], [`isalpha` family (chapter 62), `mbrtowc` family (chapter 65)],
  [`LC_NUMERIC`], [The decimal point and grouping of plain numbers], [★ `printf`, `scanf`, `strtod`],
  [`LC_MONETARY`], [Monetary formatting information], [`localeconv`],
  [`LC_COLLATE`], [String comparison order], [`strcoll`, `strxfrm`],
  [`LC_TIME`], [Date and time formatting], [`strftime`, `wcsftime`],
  [`LC_ALL`], [(the name for all of the above at once)], [—],
)

You need not memorise the table, but one line is worth keeping: *`LC_NUMERIC`
governs `printf`.* Nearly every case of this device corrupting data in practice
comes from that line (chapter 64).

#platform[
  The categories POSIX added
][
  On top of standard C's six, POSIX and glibc laid more. `LC_MESSAGES`
  (translation), and glibc's `LC_PAPER` (paper size), `LC_NAME` (the order of
  name parts), `LC_ADDRESS`, `LC_TELEPHONE`, `LC_MEASUREMENT` (metric or not),
  `LC_IDENTIFICATION`. The long semicolon-separated list the demonstration
  printed when asking `LC_ALL` is exactly those.

  The standard leaves the door open: names beginning with `LC_` and an upper-case
  letter may be defined by the implementation (§7.11p3). So these names work on
  Linux and may not elsewhere.
]

== The exact contract of `setlocale`

One function, simple in shape, dense in contract.

```c
char *setlocale(int category, const char *locale);
```

#dtable(
  columns: 2,
  [*Second argument*], [*Meaning*],
  [`"C"`], [The minimal environment the standard fixes. Where a program starts],
  [`""` (empty string)], [*The locale the environment says* — it reads the environment variables],
  [`NULL`], [Do not change anything; only report the present value],
  [Any other string], [An implementation-defined name (`"ko_KR.UTF-8"`, …)],
)

The return value splits two ways. On success it returns a string holding *the
name of the locale that was set (or is in force)*; on failure it returns null.

#misconception[
  "`setlocale` does not fail"
][
  The quietest accident starts here. If the requested locale is *not installed*
  on that machine, `setlocale` returns null and *changes nothing.* Without
  looking at the return value the program carries on believing the locale
  changed, dates come out in English and Korean comes out broken.

  The `no_SUCH.locale` line of the demonstration is that case. Linux
  distributions often ship a minimum of locales to save space (container images
  especially), which makes this the classic place where what worked on the
  developer's machine does not work in production.

  ```c
  if (!setlocale(LC_ALL, "")) {
      fprintf(stderr, "warning: could not apply the locale; continuing in C.\n");
  }
  ```
]

The returned pointer has a rule too. That string points into static storage that
*a later `setlocale` call may overwrite*. If you intend to restore it later, copy
it, as the demonstration does.

#qa[
  May categories be mixed?
][
  They may, and doing so is the standard practice. The last part of the
  demonstration is the idiom.

  ```c
  setlocale(LC_ALL, "");        /* follow the environment for everything, then */
  setlocale(LC_NUMERIC, "C");   /* put numbers back into "C" */
  ```

  Dates, sorting and currency shown to a person follow the environment, while
  *numbers a machine will read and write are pinned* so the locale cannot move
  them. These two lines prevent most of the data corruption seen in chapter 64.

  Once categories are mixed, `setlocale(LC_ALL, NULL)` returns a long string of
  the form `LC_CTYPE=…;LC_NUMERIC=…;…`. That is the demonstration's last line —
  the rule is "one name if they all agree, a list if they do not."
]

== The grammar of a locale name

`ko_KR.UTF-8`. Take the name apart and there are four international standards
inside it.

#dtable(
  columns: 3,
  [*Position*], [*Example*], [*Fixed by*],
  [Language], [`ko`], [ISO 639-1 (two letters), or ISO 639-2/-3 (three)],
  [`_` + territory], [`_KR`], [ISO 3166-1 alpha-2 country code],
  [`.` + codeset], [`.UTF-8`], [A character-set name (IANA registry, ISO 8859 family, …)],
  [`@` + modifier], [`@euro`], [A variant convention for the same language and place],
)

The whole grammar can be written like this.

```text
language[_TERRITORY][.codeset][@modifier]

ko_KR.UTF-8      Korean, Republic of Korea, UTF-8
de_DE@euro       German, Germany, the euro variant
sr_RS@latin      Serbian written in the Latin script
C   or  POSIX    the minimal locale the standard fixes
```

#idx("ISO 639")#idx("ISO 3166")What matters is that the language and the territory are *codes borrowed from
other standards*. `ko` is the code ISO 639-1 gave Korean and `KR` is the one
ISO 3166-1 gave the Republic of Korea. A locale name is not something C invented;
it is an assembly of code systems that already existed.

#qa[
  Which standard fixes this grammar?
][
  Not the C standard. C fixes only `"C"` and `""`, and says the rest are
  *implementation-defined strings* (§7.11.1.1p3).

  But one footnote points the way — "*ISO/IEC 9945* specifies locale and charmap
  formats that can be used to specify locales for C." ISO/IEC 9945 is *POSIX*
  (IEEE Std 1003.1). The `language_TERRITORY.codeset@modifier` grammar, the format
  of locale definition files, and the precedence of the environment variables are
  all fixed by POSIX.

  So the naming rules of this chapter are *the rules that hold on Unix-like
  systems.* Windows uses another system, and the web uses a third — compared
  below.
]

=== Codeset names and normalisation

`.UTF-8`, `.utf8`, `.UTF8` — all three name the same thing. glibc compares names
ignoring case and `-`. So even when `locale -a` prints `ko_KR.utf8`, a program may
ask for `"ko_KR.UTF-8"`.

The codeset names themselves come from yet another registry. `UTF-8`, `EUC-KR`
and `ISO-8859-1` are registered in IANA's character-set registry, and behind them
stand the ISO/IEC 8859 family or Unicode (ISO/IEC 10646).

*The codeset part often matters more than the rest of the name.* `ko_KR.UTF-8`
and `ko_KR.EUC-KR` share a language and a territory but differ in *how many bytes
a character takes*. In the demonstration their `MB_CUR_MAX` values split, 6
against 2.

#realcase[
  One country, two encodings — the era of `ko_KR.EUC-KR`
][
  Korean Unix environments of the 1990s and early 2000s defaulted to
  `ko_KR.eucKR` (or `ko_KR.EUC-KR`) — a world in which one Hangul syllable is two
  bytes. Code written then has the assumption "Hangul is two bytes" embedded
  everywhere: string lengths divided by two, cursors moved by two, truncation
  rounded to an even byte count.

  Moving to UTF-8 broke all of it. Hangul became three bytes, and that code began
  cutting letters in half. This is what the encoding transition actually felt like
  in Korea, and it is also why the three layers of chapter 67 — bytes, code
  points, characters — have to be told apart.
]

=== Names in other worlds — BCP 47 and Windows

The same "Korean (Republic of Korea)" is named differently by each system.

#dtable(
  columns: 3,
  [*System*], [*Notation*], [*Basis*],
  [POSIX and C], [`ko_KR.UTF-8`], [ISO/IEC 9945 (POSIX)],
  [BCP 47 (web, XML, HTTP)], [`ko-KR`], [RFC 5646 (tags), RFC 4647 (matching)],
  [Windows (Vista onwards)], [`ko-KR`], [Follows BCP 47],
  [Windows (the old way)], [`Korean_Korea.949`], [Windows-specific],
  [Unicode CLDR], [`ko_KR`], [UTS \#35 (LDML)],
)

#idx("BCP 47")BCP 47 is the internet standard — the tag in HTML's `lang="ko-KR"` and in HTTP's
`Accept-Language`. It uses a hyphen instead of an underscore and has no codeset
part, because the web handles encoding separately. When the script must be
stated, an ISO 15924 code goes in the middle, as in `sr-Latn-RS`.

*A program that spans both worlds must translate names.* To pass a browser's
`ko-KR` to `setlocale` it has to become `ko_KR.UTF-8`, which means holding a
mapping table somewhere. Mistakes are frequent at that seam.

#platform[
  The modern source of locale data — CLDR
][
  Who maintains the actual convention data — how a country writes dates, what its
  currency symbol is? Today's answer is the Unicode Consortium's *CLDR* (Common
  Locale Data Repository), in the format UTS \#35 (LDML) fixes. ICU, Java, Android
  and browsers all take their data from there.

  glibc's locale definitions come from an older line — ISO/IEC TR 14652
  (a specification method for cultural conventions) and ISO/IEC 15897 (procedures
  for registering cultural elements) — and the files themselves sit in
  `/usr/share/i18n/locales/` in a human-readable form.

  Since the two lines differ, *the same locale may carry slightly different
  values.* That is where a Java program and a C program printing the same date
  differently comes from.
]

== The precedence of the environment variables

We said `setlocale(LC_ALL, "")` uses "the locale the environment says". Exactly
what environment — POSIX fixes the precedence.

#dtable(
  columns: 3,
  [*Rank*], [*Variable*], [*Meaning*],
  [1], [`LC_ALL`], [If present, it overrides every category],
  [2], [`LC_CTYPE`, `LC_TIME`, …], [Sets only that category],
  [3], [`LANG`], [The default for categories not set above],
)

So `LANG=ko_KR.UTF-8 LC_NUMERIC=C ./program` runs mostly with Korean conventions
but with numbers in C conventions. The scripts that launch server programs pin
`LC_ALL=C` for the same reason — *to insulate logs and parsing from the
environment.*

```sh
$ locale                 # what the environment is setting right now
$ locale -a              # the locales installed on this machine
$ locale -k LC_NUMERIC   # the values of one category in detail
```

== Where locale data lives

A locale is not inside the program. It has to be *installed on the machine*.

#dtable(
  columns: 2,
  [*Step*], [*What*],
  [Definition file], [`/usr/share/i18n/locales/ko_KR` — human-readable],
  [Charmap], [`/usr/share/i18n/charmaps/UTF-8.gz`],
  [Compile], [`localedef -i ko_KR -f UTF-8 ko_KR.UTF-8`],
  [Installed under], [`/usr/lib/locale/` (or `locale-archive`)],
  [Search path], [Changeable with the `LOCPATH` environment variable],
)

Knowing this structure lets you solve the "missing locale" problem yourself. If a
container has no `ko_KR.UTF-8`, put the definition file in and build it with
`localedef`.

#realcase[
  How this book checked its locale tables
][
  The machine this book is built on also started with only `C`, `C.UTF-8` and
  `POSIX`. So, to check the tables of the next chapter, the necessary locales were
  built from glibc 2.41's definitions.

  ```sh
  localedef -i ko_KR -f UTF-8 <path>/ko_KR.UTF-8
  localedef -i de_DE -f UTF-8 <path>/de_DE.UTF-8
  export LOCPATH=<path>
  ```

  So the values from other locales printed in this book are not guesses; they are
  *what came out of running it that way.* Conversely, if the reader's machine
  lacks those locales the examples print "not on this machine" — they are written
  so as not to assume any locale exists.
]

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [What it is], [*Conventions* set outside the program. Not translation],
    [Starting value], [Always `"C"` — the standard says so],
    [Scope], [Process-global. A library can change it; threads race over it],
    [Categories], [`LC_CTYPE`, `LC_NUMERIC`, `LC_MONETARY`, `LC_COLLATE`, `LC_TIME` (+`LC_ALL`)],
    [`setlocale`], [`""`=environment, `NULL`=query. *Null return means failure*],
    [Names], [`language_TERRITORY.codeset@modifier` — ISO 639, ISO 3166, charset registry],
    [Basis], [The grammar and formats are POSIX (ISO/IEC 9945). The web uses BCP 47],
    [Environment], [`LC_ALL` > `LC_`category > `LANG`],
    [In practice], [`LC_ALL, ""` followed by `LC_NUMERIC, "C"`],
  )
]

We have seen what a locale is and how one is chosen. The next chapter is *what it
changes and how* — numbers, money, time and sorting, one at a time.
