#import "../../book/lib.typ": *

= Wide characters ② — the platforms, and wide I/O

#prereq(
  ([chapter 68, Wide characters ①], [`wchar_t`'s contract and conversion]),
  ([chapter 61, Streams in practice], [streams and buffers]),
)

#deepqa[
  Chapter 68 said the standard fixes neither the size nor the encoding of
  `wchar_t`. If it does not, the implementation picks — is that really such a
  problem?
][
  *That it split into two camps* is the problem. Linux and macOS chose four
  bytes; Windows chose two. And that choice stood on one fact of the 1990s —
  "sixteen bits are enough for Unicode" — which broke in 1996.

  Once Unicode went past U+FFFF, a two-byte `wchar_t` could no longer keep its
  original promise of *holding one character in one unit.* The patch was the
  surrogate pair, and most of the string traps in Windows programming today come
  from it.

  This chapter is the exact shape of that split, and what each platform and
  toolkit chose to build on top of it.
]

#organizer[
#idx("UTF-16")  What the same `wchar_t` became on each platform. The limit of UTF-16 and the
  arithmetic of surrogate pairs, Windows's `W` functions and their exact
  encoding, glibc's choice, how GTK and Qt hold strings, and the little-known
  rule of stream orientation.
]

#chapter-questions()

== A `wchar_t` split into two camps

#dtable(
  columns: 4,
  [*Platform*], [`sizeof(wchar_t)`], [*Encoding in practice*], [`__STDC_ISO_10646__`],
  [Linux (glibc), macOS], [4], [UTF-32 (UCS-4)], [Defined],
  [Windows (MSVC, MinGW)], [2], [UTF-16], [Not defined],
  [Some other Unixes (AIX, …)], [2 or 4], [Varies], [Varies],
)

Chapter 68's demonstration gave this machine's answer — four bytes, and
`__STDC_ISO_10646__` defined. One `wchar_t` is one code point. On Windows that
equation does not hold.

#qa[
  Why did Windows choose two bytes?
][
  The circumstances of the age. In the early 1990s, while Windows NT was being
  designed, Unicode was *a fixed-width 16-bit character set.* The design of the
  day was to hold every character in the world within 65,536, and on that premise
  "two bytes is one character" was a reasonable choice. Java's `char`,
  JavaScript's strings and Qt's `QChar` all made the same choice at the same time.

  Unicode 2.0 broke the premise in 1996. Chinese characters alone exceeded 65,536,
  and opening the range to U+10FFFF created *characters that sixteen bits cannot
  hold.* Systems already built on 16 bits could not change the type, so they
  changed the encoding instead — UTF-16, writing one character as two units.

  So Windows's `wchar_t` today is *not "one character" but "one UTF-16 code
  unit."* Name and reality have been at odds for thirty years.
]

== The limit of UTF-16 and the surrogate pair

Unicode's code point space runs from U+0000 to U+10FFFF. The first part,
U+0000\~U+FFFF, is the *Basic Multilingual Plane* (BMP), and that is as far as
sixteen bits reach.

To write characters above it in 16-bit units, Unicode set aside *a region that is
never used for characters* inside the BMP itself.

#dtable(
  columns: 3,
  [*Region*], [*Range*], [*What*],
  [High surrogates], [U+D800\~U+DBFF], [The first unit of a pair (1024 of them)],
  [Low surrogates], [U+DC00\~U+DFFF], [The second unit of a pair (1024)],
  [(The range they express together)], [U+10000\~U+10FFFF], [1024 × 1024 = 1,048,576 characters],
)

#demo("examples-en/ch69/surrogate.c")

The demonstration shows the arithmetic itself. For U+1F600:

+ Subtract 0x10000 from the code point → 0x0F600 (it fits in 20 bits)
+ Add the top ten bits to 0xD800 → 0xD83D (high)
+ Add the bottom ten bits to 0xDC00 → 0xDE00 (low)

*Surrogate values are not characters.* U+D800\~U+DFFF are assigned to no letter,
and appearing alone in UTF-8 or UTF-32 they are invalid data (we meet them again
in chapter 70's validation). That is why trying to hold U+D800 as UTF-16 printed
"cannot be represented".

#misconception[
  "The length of a string is the number of characters"
][
  Which layer that sentence is about decides between three answers. The latter
  part of the demonstration measures them — one emoji is *4 bytes in UTF-8, 1 code
  point, 2 UTF-16 units.*

  So "length" means different things in different languages. C's `strlen` counts
  bytes; the `length` of Java, JavaScript and C\# counts UTF-16 units; Python 3's
  `len` counts code points. JavaScript's famous surprise — putting in one emoji
  and getting a length of 2 — is exactly this place.

  And none of the three is *the number of characters a reader sees.* That fourth
  layer is chapter 70.
]

== Windows — the `W` functions and their exact encoding

The Windows API offers nearly every function that takes a string in two versions.

#dtable(
  columns: 3,
  [*Suffix*], [*String type*], [*Encoding*],
  [`A` (ANSI)], [`char *`], [The process's *active code page* (CP949 on a Korean install)],
  [`W` (Wide)], [`wchar_t *` (`WCHAR`)], [UTF-16LE],
)

`MessageBoxA` and `MessageBoxW` are the pair, and the name `MessageBox` is a
macro — it expands to `W` if `UNICODE` is defined and to `A` otherwise. Names like
`TCHAR` and `_T()` are relics of the same era.

#platform[
  Three roads for strings on Windows
][
  *① Use the `W` functions and convert at the boundary (the classic road).* Keep
  the program's insides in UTF-8 and convert to UTF-16 only when calling the API,
  with `MultiByteToWideChar(CP_UTF8, …)`, and back with `WideCharToMultiByte`.
  Both are two-call functions — ask the required size first (pass 0 for the
  length and it returns the count), then fill.

  *② Write the whole program in UTF-16.* The old way for Windows-only programs.
  Every literal carries an `L`, and you use `wcslen` and the `wprintf` family.
  Portability is given up.

  *③ Make the active code page UTF-8 (the modern prescription).* Since Windows 10
  1903, putting `<activeCodePage>UTF-8</activeCodePage>` in the application
  manifest makes the `A` functions take UTF-8. Then UTF-8 code written for Linux
  runs almost unchanged. For a new program this is the shortest road.

  The console is separate again. To get Unicode out of a console, either turn on
  wide mode with `_setmode(_fileno(stdout), _O_U16TEXT)` or change the code page
  with `SetConsoleOutputCP(CP_UTF8)`. "Korean comes out as question marks" is
  usually this.
]

#realcase[
  Unpaired surrogates — the shadow over Windows file names
][
  A Windows file name is "an array of UTF-16 units", not "a valid Unicode
  string". Because nothing checks, *a name holding a lone high surrogate* can
  exist in the file system.

  Converting such a name to UTF-8 causes trouble — the value cannot be represented
  in valid UTF-8. `WideCharToMultiByte` either fails or substitutes U+FFFD, and
  then *the file can no longer be opened.*

  Because of this some programs use an extension called "WTF-8" internally (a
  non-standard encoding that writes unpaired surrogates in UTF-8 style). Rust's
  `OsString` is implemented that way on Windows.

  The lesson: *do not assume that a string the platform hands you is valid
  Unicode.* File names, command-line arguments and environment variables
  especially.
]

== Linux and glibc — large, and unused

glibc's `wchar_t` is four-byte UCS-4 and `__STDC_ISO_10646__` is defined; it is
the implementation closest to the picture the standard drew. And yet Linux code in
practice hardly uses `wchar_t`. There are four reasons.

#dtable(
  columns: 2,
  [*Reason*], [*Explanation*],
  [It uses four times the memory], [For mostly-ASCII text, 4× is a large waste],
  [You must convert at every boundary], [Files, sockets and APIs are all bytes],
  [It is tied to the locale], [Conversion fails unless `LC_CTYPE` is UTF-8 (chapter 68)],
  [UTF-8 already does most of the work], [`strlen`, `strcmp` and `strstr` still work],
)

The last line is decisive. UTF-8 is *ASCII-compatible, self-synchronising, and
its byte order matches code-point order*, so the existing byte functions remain
mostly useful (chapter 70). So the Linux camp took the road of "UTF-8 inside
too."

== What the toolkits chose — GTK and Qt

The same split appears at the application layer.

#dtable(
  columns: 4,
  [*Toolkit*], [*String type*], [*Encoding*], [*Code point type*],
  [GTK / GLib], [`gchar *` (= `char *`)], [UTF-8], [`gunichar` (32-bit), `gunichar2` (16-bit)],
  [Qt], [`QString`], [UTF-16], [`QChar` (a 16-bit unit)],
  [Windows API], [`WCHAR *`], [UTF-16], [—],
)

GLib set the rule "every string is UTF-8" and laid functions on top of it —
`g_utf8_strlen` (characters), `g_utf8_next_char`, `g_utf8_validate`. It does not
use `wchar_t` at all: a decision not to put a type that changes per platform into
an API.

Qt went the other way. Its insides are fixed at UTF-16, and it crosses the
boundary with `QString::fromUtf8` and `toUtf8`. The gain is no conversion when
meeting the Windows API; the cost is the trap that `QString::size()` counts UTF-16
units.

#qa[
  So what should my program choose?
][
  *For new code, UTF-8 byte strings.* The table above says why — smaller, at home
  with existing C functions, the same as the representation on files and networks,
  and less tied to the locale.

  The places `wchar_t` is needed are narrow: *calling the Windows API directly*
  (and only at the boundary), and *fitting an existing library that only takes
  wide characters.*

  If you genuinely need fixed-width code points, use `char32_t` rather than
  `wchar_t`. It is four bytes everywhere and its being UTF-32 is guaranteed by a
  macro — two things `wchar_t` cannot give you.
]

== Streams have an orientation

Wide characters come with I/O functions of their own — `wprintf`, `fputws`,
`getwc`, `fgetws`, and `WEOF` in place of `EOF`. But there is a rule that is not
widely known.

*A stream has an orientation, and once settled it cannot be changed.*

#dtable(
  columns: 2,
  [*State*], [*Meaning*],
  [No orientation], [A freshly opened stream. Neither yet],
  [Byte-oriented], [Settled by the first use of a byte function such as `fputs` or `fprintf`],
  [Wide-oriented], [Settled by the first use of a wide function such as `fputws` or `fwprintf`],
)

Using the opposite family after it is settled is *undefined behaviour*. The
`fwide` function asks (pass 0) or settles it in advance while there is none.

#demo("examples-en/ch69/wide_io.c")

The demonstration shows the rule plainly. A freshly opened stream has no
orientation; `fwide(f, 1)` can make it wide; and afterwards `fwide(f, -1)` *does
not move it back.* And this program's `stdout` is already byte-oriented because
it began with `printf` — calling `wprintf` here would be outside the contract.

#antipattern[
  Mixing `printf` and `wprintf`
][
  ```c
  printf("name: ");
  wprintf(L"%ls\n", name);      /* wide output on an already byte-oriented stdout */
  ```
  At best the output interleaves; at worst nothing appears. On Windows, calling
  `printf` after turning on `_O_U16TEXT` can kill the program outright.

  One discipline — *one family per stream.* If you decide on wide output, do it
  throughout the program; otherwise do not use it at all. This book recommends the
  latter: what wide functions gain is smaller than the portability they cost.
]

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [The split], [Linux 4-byte UTF-32, Windows 2-byte UTF-16],
    [The cause], [The 1990s premise "Unicode = 16 bits" broke in 1996],
    [Surrogates], [U+10000 and above as a D800\~DBFF + DC00\~DFFF pair. The values themselves are not characters],
    [Length], [Bytes, code points and UTF-16 units all differ],
    [Windows], [`W` functions are UTF-16LE. New code: a UTF-8 code page via the manifest],
    [File names], [May not be valid Unicode (unpaired surrogates)],
    [Toolkits], [GTK = UTF-8 `char*`, Qt = UTF-16 `QString`],
    [If you need fixed width], [`char32_t`, not `wchar_t`],
    [Stream orientation], [Once settled it cannot change. Mixing is undefined behaviour],
  )
]

We have seen the platforms as they are. The next chapter is the conclusion —
*how*, in practice, to handle Unicode and multibyte encodings. The three layers of
length, normalisation, UTF-8 validation, and the traps of the legacy two-byte
encodings that are still with us.
