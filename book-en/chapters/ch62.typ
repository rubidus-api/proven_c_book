#import "../../book/lib.typ": *

= Characters and locales — `<ctype.h>`, `<locale.h>`, `<wchar.h>`

#prereq(
  ([chapter 9, Characters and text], [letters and encodings]),
)

#deepqa[
  Chapter 9 said a character is a number in a code chart and an encoding is the way
  of writing that number as bytes. Then what is a call like `isalpha('가')` asking?
][
  It is a wrongly thrown question. The functions of `<ctype.h>` judge *one byte*. In
  UTF-8 "가" is three bytes, so it cannot go in as an argument to begin with, and
  even if it did each byte would merely be looked at separately. This header is a
  tool of the ASCII days; to handle multibyte characters you need the wide-character
  family (`<wctype.h>`) or code that handles the encoding yourself.
]

#organizer[
  We look at the functions that handle a single character. It is the simplest
  header in appearance, yet two of C's subtlest traps are here — that *passing a
  `char` as it stands is outside the contract*, and that *the answer depends on the
  global state called the locale*. Chapter 9's encoding story returns at the level
  of functions.
]

#chapter-questions()

== The first trap — do not pass a `char` as it stands

#demo("examples-en/ch62/ctype.c")

Every function of `<ctype.h>` takes an `int`. And the argument value the standard
requires is *a value representable as an `unsigned char`, or `EOF`*.

The problem is that implementations where `char` is signed are common
(chapter 26). As the example's output shows, put a 0xC7 byte in a `char` and it
becomes −57, and passing that straight to `isalpha` is passing *a value that is not
permitted* — outside the contract. Real implementations are mostly built as array
indexes, so the accident of reading before the array with a negative index occurs.

The idiom is one.

```c
isalpha((unsigned char)c)
toupper((unsigned char)c)
```

That `EOF` is a valid argument is worth remembering too — that is why the argument
type is `int` and not `char` (the same root as chapter 58's `fgetc` story).

#antipattern[
  Passing a `char` as it stands
][
  ```c
  char *p = line;
  while (*p) { if (isspace(*p)) ... ; p++; }   /* outside the contract at bytes ≥ 0x80 */
  ```
  Unless there is a guarantee that only ASCII arrives, always convert.
  ```c
  while (*p) { if (isspace((unsigned char)*p)) ... ; p++; }
  ```
  In a program handling Korean, Japanese or European text, this one line divides a
  real accident from no accident.
]

#qa[
  If a program only ever handles ASCII, can locales and wide characters be ignored?
][
  It looks that way for a while, and then two places catch you. First, programs
  mostly receive other people's input — nothing stops a file name, a user name or
  a pasted string from carrying Korean or an emoji. Second, the locale changes
  *even the handling of ASCII*. The famous case is the Turkish locale, where
  `toupper('i')` becomes the dotted capital `İ`; in a locale whose decimal point
  is a comma, even `printf("%f")` prints differently.

  So this book's advice is not "you may ignore it" but *"be aware of the moment
  you step outside the default locale."* A design that handles bytes as they are,
  like chapter 79's UTF-8 views, shrinks the problem the most.
]

== The second trap — the locale

The judgements of `<ctype.h>` depend on *the current locale*. The default is the
`"C"` locale, and calling `setlocale(LC_ALL, "")` changes it to the locale the
environment variables settle. From that moment the set of bytes for which
`isalpha` returns true may differ.

What the locale changes is not only character classification.

#dtable(
  columns: 3,
  [*category*], [*what changes*], [*where to beware*],
  [`LC_CTYPE`], [character classification, case conversion], [`isalpha`, `toupper`],
  [`LC_NUMERIC`], [the decimal point character], [★ `printf("%f")`, `strtod`],
  [`LC_COLLATE`], [string sorting order], [`strcoll`, `strxfrm`],
  [`LC_TIME`], [date and time notation], [`strftime` (chapter 64)],
  [`LC_MONETARY`], [currency notation], [`localeconv`],
)

`LC_NUMERIC` is especially dangerous. In German and French locales the decimal
point is a comma, so `printf("%f", 3.14)` prints `3,140000` and
`strtod("3.14", …)` stops at 3. It means that *numbers to be shown to a human* and
*numbers to be written to a file or protocol* must follow different rules.

#realcase[
  The pattern in which a locale wrecked data
][
  It really has happened repeatedly that the same program writes `3.14` on the
  developer's machine (an English locale) and `3,14` on the user's machine (a
  German locale). A CSV file fails to parse column by column, a configuration file
  cannot be read back after being saved, or the JSON two countries' servers
  exchange goes out of step.

  The prescription is clear. *Numbers a machine will read are handled by a path
  that does not depend on the locale* — keep the `"C"` locale, handle the notation
  of the digits yourself, or use locale-independent functions. It is also why C11's
  `<uchar.h>` and several libraries lay a separate path to avoid this problem.
]

#misconception[
  "If `setlocale` is not called there is no locale problem"
][
  Broadly right, but there is an exception. *Another library* the program uses may
  call `setlocale` (GUI toolkits commonly do), and from that moment my code's
  `printf("%f")` is affected too. Moreover the locale is *process-global*, so in a
  program running along several strands it becomes a race. Thread-local locales
  such as POSIX's `uselocale` and `newlocale` arose for that reason, but they are
  not in standard C.
]

== Wide characters — `<wchar.h>`, `<wctype.h>`, `<uchar.h>`

C95 brought in `wchar_t` and its functions. The idea was "one character as one
unit", but reality betrayed the idea.

- The *size of `wchar_t` differs by implementation.* Linux has 4 bytes (close to
  UTF-32), Windows 2 bytes (it being UTF-16, characters of the supplementary
  planes become two units). That is, "one character = one `wchar_t`" does not hold
  everywhere.
- The conversion functions (`mbstowcs` and so on) *depend on the locale.* To handle
  a UTF-8 string the locale must be UTF-8.
- So portable programs often choose, instead of wide characters, to *handle the
  UTF-8 byte sequence as it is*.

The `char16_t` and `char32_t` of `<uchar.h>`, added by C11, have fixed sizes and so
avoid this problem. But library support is thin, so in the field a dedicated
Unicode library is still used, or it is handled by hand.

#platform[
  Windows and UTF-16
][
  The "W" family of Windows API functions takes UTF-16 (`wchar_t`). So a Windows
  program must go between UTF-8 and UTF-16 at the boundary, and it must not be
  forgotten that this conversion can fail (unpaired surrogates). It is the place
  where chapter 17's chain-of-encodings story is replayed at the API layer, and the
  `u16` family treated in Part XII's string chapter is exactly the tool that lays
  this bridge.
]

#recap[
  Characters and locales in summary.

  #dtable(
    columns: 3,
    [*situation*], [*rule*], [*if got wrong*],
    [calling `<ctype.h>`], [convert with `(unsigned char)`], [a negative argument — outside the contract],
    [handling `EOF`], [the argument type is `int`], [confusion with 0xFF],
    [multibyte characters], [judging byte by byte is meaningless], [a judgement on a split character],
    [numeric input and output], [a locale-independent path for machines], [the decimal point changes and parsing fails],
    [sorting], [`strcoll` for humans], [`strcmp` is not dictionary order],
    [`wchar_t`], [the size differs by implementation], [miscounting characters on Windows],
    [recommended], [UTF-8 byte sequences + conversion only at the boundary], [—],
  )
]

We have passed the world of the single character. The next chapter is the world of
numbers — how real-number calculation reports failure in the standard library.
