#import "../../book/lib.typ": *

= Wide characters ① — `wchar_t` and multibyte conversion

#prereq(
  ([chapter 9, Characters and text], [code points and encodings]),
  ([chapter 63, Locales ①], [`LC_CTYPE`]),
  ([chapter 62, Character classification], [the limit of per-byte judgement]),
)

#deepqa[
  Chapter 9 said one character in UTF-8 is one to four bytes. Then why not make a
  type that holds one whole character? Does C have one?
][
  It does. It is `wchar_t`, and C95 brought it in on exactly that thought — *hold
  one character in one unit.* The trouble is that the thought did not survive the
  next thirty years.

  The standard fixed *neither the size nor the encoding.* It said only "an integer
  type whose range of values can represent distinct codes for all members of the
  largest extended character set specified among the supported locales." The
  result: 4-byte UTF-32 on Linux, 2-byte UTF-16 on Windows — and on the latter,
  "one character = one unit" does not hold.

  This chapter takes the exact contract of the type, and the process by which
  bytes unfold into characters, one step at a time. The consequences of the
  mismatch are the next chapter.
]

#organizer[
#idx("wide character")  What `wchar_t` really is. The standard's definition and the macros an
  implementation uses to declare itself, the five kinds of character constant and
  string with their types, sizes and bytes, the difference between `MB_CUR_MAX`
  and `MB_LEN_MAX`, why `mbstate_t` has to exist, and a measured, step-by-step
  walk through `mbrtowc` eating bytes and producing characters.
]

#chapter-questions()

== What the standard fixes, and what it does not

`wchar_t` is an integer type defined in `<stddef.h>`. The standard's definition,
verbatim, is *"an integer type whose range of values can represent distinct codes
for all members of the largest extended character set specified among the
supported locales."*

Look at what is missing from it.

#dtable(
  columns: 2,
  [*The standard fixes*], [*It does not fix*],
  [That it is an integer type], [How many bytes],
  [That each character has a distinct value], [Whether it is signed],
  [That basic characters are non-negative], [Which encoding (not even that it is Unicode)],
)

So portable code *cannot assume* a `wchar_t` value is a Unicode code point.
Instead there are macros by which an implementation declares the fact.

#dtable(
  columns: 2,
  [*Macro*], [*If it is defined*],
  [`__STDC_ISO_10646__`], [`wchar_t` values equal ISO/IEC 10646 (Unicode) code points. Its value has the form `yyyymmL`],
  [`__STDC_UTF_16__`], [`char16_t` is UTF-16],
  [`__STDC_UTF_32__`], [`char32_t` is UTF-32],
  [`__STDC_MB_MIGHT_NEQ_WC__`], [Even for a basic character, `'x'` and `L'x'` may differ in value],
)

#demo("examples-en/ch65/wide_lit.c")

The last part of the demonstration is this machine's answer —
`__STDC_ISO_10646__` is defined, and `wchar_t` is four bytes. *On Windows that
macro is not defined.* Two bytes cannot hold every Unicode character as one
distinct value, so the condition is not met. That one line is the seed of the
whole of chapter 66.

== Five kinds of character constant and string

C23 has five ways to write a character. Each has its own type and encoding.

#dtable(
  columns: 4,
  [*Notation*], [*Type of the constant*], [*String element*], [*Encoding*],
  [`'A'`], [`int`], [`char`], [The execution character set (usually UTF-8 bytes)],
  [`u8'A'` / `u8"…"`], [`unsigned char`], [`char8_t`], [UTF-8],
  [`u'A'` / `u"…"`], [`char16_t`], [`char16_t`], [UTF-16],
  [`U'A'` / `U"…"`], [`char32_t`], [`char32_t`], [UTF-32],
  [`L'A'` / `L"…"`], [`wchar_t`], [`wchar_t`], [Implementation-defined],
)

The first part of the demonstration puts this table in the flesh. Three things
stand out.

*First, the type of `'A'` is `int`, not `char`.* In C a character constant is an
`int`, so `sizeof('A')` is 4 (a place where C and C++ differ).

*Second, the `u8` prefix applies to character constants too in C23* — but only for
characters representable as a single UTF-8 code unit, that is, ASCII.

*Third, the same "한" becomes entirely different bytes in each notation.* As
`char` it is the three bytes `ED 95 9C`; as `char16_t`, the single unit `D55C`;
as `char32_t` and `wchar_t`, the single unit `0000D55C`.

And with a character beyond the BMP the decisive difference appears. Holding
U+1F600, only `char16_t` needs *two elements* — `D83D DE00`. It does not fit in
16 bits, so it is split into a pair, and that pair is called a surrogate
(chapter 66).

#qa[
  What bytes does writing `"한"` in the source produce?
][
  It passes through two stages — *the source file's encoding* and *the execution
  character set* (chapter 17's chain of encodings). The compiler decides in which
  encoding to read the source (GCC defaults to UTF-8; `-finput-charset` changes
  it), then converts to the execution character set to make the bytes of the
  literal (`-fexec-charset`).

  If those two disagree, the code compiles and only the strings are broken. So
  code that must be portable writes non-ASCII characters as *universal character
  names* rather than directly in the source.

  ```c
  const char *s = "한";        /* U+D55C into the execution character set */
  const char32_t *t = U"\U0001F600";
  ```

  `\u` takes four digits, `\U` eight. The demonstration writes its emoji that way
  — that is how this book's examples produce the same bytes under any editor and
  any build setting.
]

== `MB_CUR_MAX` and `MB_LEN_MAX`

Two macros with similar names mean quite different things.

#dtable(
  columns: 3,
  [*Macro*], [*Header*], [*What*],
  [`MB_CUR_MAX`], [`<stdlib.h>`], [The maximum bytes per character *in the current locale* — it changes at run time],
  [`MB_LEN_MAX`], [`<limits.h>`], [An upper bound over any supported locale — a compile-time constant],
)

Chapter 63's demonstration showed `MB_CUR_MAX` changing with the locale: 1 (C),
2 (EUC-KR), 6 (UTF-8). Which to use when sizing a buffer splits here — *size the
array with `MB_LEN_MAX`, and judge inside the loop with `MB_CUR_MAX`.*

#qa[
  UTF-8 is at most four bytes; why is `MB_CUR_MAX` six?
][
  It is a scar from history. UTF-8's original definition (RFC 2279) allowed up to
  six bytes and could write codes of 31 bits. When Unicode stopped at U+10FFFF,
  RFC 3629 cut it to four — but glibc left `MB_CUR_MAX` at the roomier old value.

  The practical conclusion does not change — *size buffers by `MB_CUR_MAX` (or
  `MB_LEN_MAX`).* Sizing by 4 because UTF-8 never exceeds it will overflow under a
  locale with another encoding.
]

== Three layers of conversion function

Several functions move between byte strings and wide characters; they sort into
three layers.

#dtable(
  columns: 3,
  [*Layer*], [*Functions*], [*Character*],
  [The old layer (C89)], [`mbtowc`, `wctomb`, `mbstowcs`, `wcstombs`], [Hide the conversion state *inside the function* — not reentrant],
  [The state-exposed layer (C95)], [`mbrtowc`, `wcrtomb`, `mbsrtowcs`, `wcsrtombs`], [Take an `mbstate_t *`. The `r` is for restartable],
  [The Unicode layer (C11)], [`mbrtoc16`, `c16rtomb`, `mbrtoc32`, `c32rtomb`], [`<uchar.h>`. Between the locale's encoding and UTF-16/32],
)

New code uses the middle layer. The `r` in the name means restartable, and what
makes restarting possible is `mbstate_t`.

=== Why `mbstate_t` has to exist

Why does conversion need state? Two reasons.

*First, characters arrive cut in half.* Reading from a network or a file puts
buffer boundaries in the middle of characters. With two of "한"'s three bytes in
hand, the function must answer "not enough yet" rather than "wrong", and must
*carry on* when the next piece arrives.

*Second, some encodings have state.* Encodings such as ISO-2022-JP switch between
"kanji from here" and "ASCII from here" with escape sequences. Without
remembering which mode you are in, the same byte is a different character.

`mbstate_t` is the opaque object holding those two. Three rules: *filled with
zeroes it is the initial state*, *each conversion uses its own object*, and *you
do not look inside.*

```c
mbstate_t st;
memset(&st, 0, sizeof st);      /* to the initial state */
```

== `mbrtowc` — taken apart one step at a time

The key function of the chapter. The contract is dense, and reading the return
value exactly is the whole of it.

```c
size_t mbrtowc(wchar_t * restrict pwc, const char * restrict s,
               size_t n, mbstate_t * restrict ps);
```

#dtable(
  columns: 2,
  [*Return value*], [*Meaning*],
  [`0`], [A null character was completed (the result is the null wide character)],
  [1\~`n`], [This many bytes were consumed and one character was completed],
  [`(size_t)-2`], [All `n` bytes were examined and no character is complete yet — the state is updated],
  [`(size_t)-1`], [An invalid sequence. `errno` holds `EILSEQ` and the state is unspecified],
)

#demo("examples-en/ch65/mbrtowc_step.c")

The demonstration shows those four in turn. ASCII goes one byte at a time, Hangul
takes three at once, an emoji beyond the BMP takes four. A truncated piece gives
`(size_t)-2`, and a piece starting on a trail byte gives `(size_t)-1`.

*The final part shows exactly why `mbstate_t` exists.* Feeding "한"'s three bytes
as 2+1, the first call returned `(size_t)-2` and the second completed U+D55C. The
state object was holding "I have seen two bytes so far."

#misconception[
  "`(size_t)-2` is an error"
][
  It is not. It is *a normal intermediate state.* The only error is `(size_t)-1`.

  Lump the two together and a stream processor throws away perfectly good
  characters from fragmented input. Code reading 4096 bytes at a time from a
  network meets `(size_t)-2` at nearly every buffer's end — and what to do then is
  *to prepend the leftover bytes to the next buffer and read on*, not to stop with
  an error.

  That both values are large `size_t` numbers is a trap of its own. Received into
  an `int` and compared with `< 0`, the result is implementation-dependent
  nonsense. *Always receive them in a `size_t` and compare directly with
  `(size_t)-1` and `(size_t)-2`.*
]

=== A whole string at once — `mbsrtowcs`

To move a whole string rather than one character at a time, use `mbsrtowcs`. Two
knacks belong to its contract.

```c
mbstate_t st;
memset(&st, 0, sizeof st);
const char *p = utf8;                       /* a pointer holding the position */
size_t need = mbsrtowcs(NULL, &p, 0, &st);  /* (1) ask the length first */
if (need == (size_t)-1) { /* EILSEQ */ }

wchar_t *buf = malloc((need + 1) * sizeof *buf);
p = utf8;                                   /* (2) reset pointer and state */
memset(&st, 0, sizeof st);
mbsrtowcs(buf, &p, need + 1, &st);          /* (3) and convert for real */
```

*A null first argument counts only the length* — so the buffer size can be known
in advance. And the reason `s` is a `const char **` is to hand back *how far it
read* when the conversion stopped partway. The old `mbstowcs` lacked that, which
is why it could not be used for stream processing.

#platform[
  Conversion is tied to the locale
][
  One fact easily missed here. `mbrtowc` understands UTF-8 only *because the
  current locale is UTF-8.* With `LC_CTYPE` set to `"C"`, the same bytes are an
  invalid sequence.

  So a program using wide characters must call `setlocale(LC_ALL, "")` at once,
  and must cope with the fact that *the locale might not be UTF-8.* That is why
  the demonstration explicitly looks for a UTF-8 locale and skips if there is
  none.

  The constraint "these functions cannot choose the encoding" is a large part of
  why wide characters are avoided in practice. The premise that the program's
  encoding and the user's locale must agree is especially hard to keep in a
  server.
]

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [`wchar_t`], [An integer type. The standard fixes neither size nor encoding],
    [Unicode guarantee], [Only if `__STDC_ISO_10646__` is defined],
    [Character constants], [`'A'` is an `int`. Four prefixes: `u8`, `u`, `U`, `L`],
    [Beyond the BMP], [Only `char16_t` takes two elements (a surrogate pair)],
    [`MB_CUR_MAX`], [Changes with the locale. The bound is `MB_LEN_MAX`],
    [Conversion], [Use the ones with `r` (`mbrtowc`)],
    [`mbstate_t`], [Zero it to start. For cut characters and stateful encodings],
    [Return values], [`-2` is not an error but "more needed"],
    [The premise], [`LC_CTYPE` must be that encoding],
  )
]

We have seen `wchar_t`'s contract and the process of conversion. The next chapter
is how that contract split across real platforms — Windows's two bytes, Linux's
four, and what happened when UTF-16 could not hold all of Unicode.
