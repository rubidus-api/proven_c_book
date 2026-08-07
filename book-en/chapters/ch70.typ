#import "../../book/lib.typ": *

= In practice — handling Unicode and multibyte encodings

#prereq(
  ([chapter 69, Wide characters ②], [UTF-16 and the platform split]),
  ([chapter 68, Wide characters ①], [conversion functions and `mbstate_t`]),
  ([chapter 9, Characters and text], [code points and encodings]),
)

#deepqa[
  Through chapters 68 and 69 the advice "handle UTF-8 byte strings as they are"
  kept returning. But if you handle bytes as they are, how do you do something
  like "delete the third character"?
][
  *Because most programs never do that.* Count what they actually do: read,
  store, compare, concatenate, search, and hand back out. All six work on UTF-8
  byte strings *with no decoding at all.*

  The work that needs characters — moving a cursor, finding a line-break point,
  counting letters — belongs to an editor or a renderer, and at that layer even
  code points are not enough; you need *grapheme clusters*. This chapter draws
  that line: how far to go on bytes, where decoding begins, and what to watch for
  there.
]

#organizer[
#idx("UTF-8")  The prescriptions of practice. Why a UTF-8-first design wins, the three layers
  of "how many characters" and grapheme clusters, normalisation, case that
  depends on language, a UTF-8 validator written by hand, and the traps of the
  legacy two-byte encodings still in use.
]

#chapter-questions()

== The principle — UTF-8 inside, conversion only at the boundary

The conclusion first.

#dtable(
  columns: 2,
  [*Place*], [*What to use*],
  [Inside the program], [UTF-8 byte strings (`char *`, length in bytes)],
  [Files, networks, databases], [UTF-8],
  [Calling the Windows API], [Convert to UTF-16 at the boundary only],
  [Character-level editing and rendering], [Decode to code points or graphemes only where needed],
)

Why this design wins follows straight from UTF-8's properties.

#dtable(
  columns: 2,
  [*Property of UTF-8*], [*What it buys*],
  [Fully compatible with ASCII], [Existing code, protocols and file formats keep working],
  [Trail bytes are always 0x80 or above], [They never collide with separators like `'/'`, `'\\'` or `','`],
  [It is self-synchronising], [A character boundary can be found from any position],
  [Byte order equals code-point order], [`strcmp` gives a code-point-ordered sort],
  [It has no endianness], [No byte order mark is needed],
)

#demo("examples-en/ch70/utf8_scan.c")

The last part of the demonstration shows self-synchronisation. Point at any byte
and testing `(b & 0xC0) == 0x80` alone tells you whether you are inside a
character or at its start — which is why a buffer can be cut anywhere and
recovered, and why a scanner can move on to the next character in damaged data.

== Validation — what to reject even when the shape is right

Code that reads UTF-8 *must validate*. The rules are RFC 3629 (STD 63), and three
things are not caught by the shape alone.

#dtable(
  columns: 3,
  [*What to reject*], [*Example*], [*Why*],
  [Overlong encodings], [`C0 80`], [U+0000 written in two bytes — a classic way past a check],
  [Encoded surrogates], [`ED A0 80`], [U+D800 is not a character (chapter 69)],
  [Out of range], [`F5 80 80 80`], [Beyond U+10FFFF],
)

The demonstration rejects each of the three. The first row matters most —
*overlong encodings are a security problem.* There really were attacks that wrote
`..` or `/` in several bytes to slip past a path check, so that a later layer
would interpret them again. That is why "only the shortest form is valid" became a
requirement of the specification.

#misconception[
  "UTF-8 is just bytes, so it can be passed through without validation"
][
  Passing it through the middle untouched is mostly safe. The problem is *the
  moment of interpretation.*

  An unvalidated byte string can be interpreted differently by different layers,
  and that mismatch becomes a security hole — the first layer sees "odd bytes" and
  lets them by, the next reads them as `/`. This is especially so where a web
  server, a file system and a database meet.

  One discipline — *validate once at the input boundary and trust it afterwards.*
  Marking clearly in the code where that happened is part of the discipline. Part
  XII's `u8` family in proven is an example of enforcing it through the type.
]

== The three layers of "how many characters", and the fourth

Chapter 69 said length has three answers. Counting what a reader sees makes four.

#demo("examples-en/ch70/graphemes.c")

#dtable(
  columns: 3,
  [*Layer*], [*Unit*], [*Where it is used*],
  [Bytes], [`char`], [Storage, transmission, buffer sizes],
  [Code units], [A UTF-16 unit, …], [The `length` of Java and JavaScript],
  [Code points], [One Unicode value], [Normalisation, classification, conversion],
  [Grapheme clusters], [What a reader sees as one character], [Cursor movement, counting, truncation],
)

The demonstration measures the difference. One emoji family is *18 bytes, 5 code
points, 1 visible character.* An invisible character, ZWJ (U+200D), joined three
people into one. A flag is two regional indicators gathered into one.

The same happens in Hangul. "가" can be written as the precomposed U+AC00 or as
U+1100 (ㄱ) + U+1161 (ㅏ) — what appears on screen is the same letter.

#qa[
  How do you count grapheme clusters?
][
  The rules are Unicode Annex UAX \#29. It defines "where a character breaks" by
  combinations of character properties, and implementing it properly needs the
  Unicode database.

  The demonstration's count is a *simplified version* covering the four cases you
  meet most — combining marks, Hangul conjoining jamo, ZWJ joins and regional
  indicator pairs. That is enough to count most emoji and Hangul correctly, but it
  is not complete.

  The practical conclusion: *do not implement it yourself.* Where grapheme units
  are genuinely needed (an editor's cursor, display width), use ICU or its like.
  The purpose of this section is to know *when* they are needed: not for storage,
  comparison or transmission, but only when handling what a person sees.
]

== Normalisation — the same letter, different bytes

The two ways of writing "가" become a problem at once in practice, *because
different bytes make `strcmp` say different.* To the user it is the same letter,
yet the search misses, file names collide, and a login name is treated as another.

Unicode handles this with *normalisation* (UAX \#15). There are four forms.

#dtable(
  columns: 3,
  [*Form*], [*What*], [*Example*],
  [NFC], [Compose as far as possible (the usual recommendation)], [`ㄱ`+`ㅏ` → `가`],
  [NFD], [Decompose as far as possible], [`가` → `ㄱ`+`ㅏ`],
  [NFKC, NFKD], [Also unify compatibility characters that look different], [`㈜` → `(주)`, fullwidth `Ａ` → `A`],
)

#realcase[
  macOS file names and NFD
][
  macOS's HFS+ file system stored file names *normalised into something close to
  NFD*. So a Korean file name created on macOS and moved to Linux often appeared
  with its jamo pulled apart.

  For the same reason archives broke, web servers returned 404, and `git status`
  reported perfectly good files as modified. Git ended up adding a
  `core.precomposeunicode` setting to put names back into NFC on macOS.

  Two lessons. *One, normalise before comparing strings* — especially when using
  something a person typed (a file name, a user name, a search term) as a key.
  *Two, settle on one form across the whole system* — usually NFC.

  Standard C has no normalisation function. ICU or its like is required.
]

== Case and sorting depend on the language

The assumption that `toupper` turns one character into one character also
collapses under Unicode.

#dtable(
  columns: 3,
  [*Example*], [*What happens*], [*So*],
  [Turkish `i`], [Its capital is `İ` (dotted I)], [Without a locale it cannot be done right],
  [Turkish `I`], [Its lower case is `ı` (dotless i)], [The exact opposite of English],
  [German `ß`], [Its capital is `SS`, two letters], [Not a one-to-one mapping],
  [Greek `Σ`], [At the end of a word it is `ς`], [It depends on position],
)

#realcase[
  The Turkish `i` — code that really did break
][
  The common idiom "to compare case-insensitively, upper-case both and compare"
  breaks in a Turkish locale. Upper-casing `"file"` gives `"FİLE"`, which is not
  `"FILE"`.

  Because of this, extension checks failed, HTTP header names did not match and
  configuration keys went unrecognised in real software. Java's
  `toUpperCase(Locale.ROOT)` exists as a separate call because of this problem.

  The remedy is to separate the layers. *What a machine compares — protocols,
  identifiers, file extensions — is folded by ASCII rules only, independent of the
  locale.* The locale is used only for names shown to people.

  ```c
  /* for machines — ASCII only, independent of the locale */
  static char ascii_lower(char c)
  { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
  ```
]

== Still alive — the legacy two-byte encodings

The world has not all become UTF-8. Old files, old databases and the protocols of
old equipment still carry regional two-byte encodings.

#dtable(
  columns: 3,
  [*Encoding*], [*Region*], [*Underlying standard*],
  [EUC-KR / CP949 (UHC)], [Korea], [KS X 1001 (formerly KS C 5601)],
  [Shift_JIS / CP932], [Japan], [JIS X 0208],
  [Big5], [Taiwan, Hong Kong], [— (a de facto standard)],
  [GBK / GB 18030], [China], [GB 18030-2022 (a mandatory Chinese national standard)],
)

Their common structure is "lead byte + trail byte". And *in some of them the
trail byte reaches into ASCII* — which is where the famous accident happens.

#demo("examples-en/ch70/legacy_lead.c")

The demonstration reproduces it. In Shift_JIS, 表 is `95 5C` and ソ is `83 5C`,
and that second byte `0x5C` is the ASCII backslash. So
`strrchr(path, '\\')` *mistakes a character's trail byte for a path separator.*
In the demonstration it points at 7 instead of the correct 5, and cutting there
breaks the file name.

In Japan these characters even have a name — *dame-moji* (ダメ文字). 表, ソ, 十
and ダ caused accidents over and over in path handling, escaping and SQL.

#dtable(
  columns: 3,
  [*Encoding*], [*Trail byte range*], [`0x5C` as a trail?],
  [EUC-KR], [A1\~FE], [No — safe],
  [CP949], [41\~5A, 61\~7A, 81\~FE], [No — safe],
  [Shift_JIS], [40\~7E, 80\~FC], [★ Yes],
  [Big5], [40\~7E, A1\~FE], [★ Yes],
  [GBK], [40\~FE (not 7F)], [★ Yes],
  [UTF-8], [80\~BF], [No — structurally impossible],
)

*The Korean encodings escaped this accident by luck.* EUC-KR's trail bytes are
all 0xA1 or above and so never touch ASCII. Code handling Japanese or Chinese, by
contrast, must use a lead-byte-aware scan.

#antipattern[
  Scanning a legacy-encoded string byte by byte
][
  ```c
  char *ext = strrchr(filename, '.');    /* can misbehave in Shift_JIS */
  for (char *p = s; *p; p++)             /* mistakes a trail byte for a character */
      if (*p == ',') split(p);
  ```
  On meeting a lead byte you must step over two. The `safe_last_sep` of the
  demonstration is that shape. With standard functions, advance one character at a
  time with `mblen` or `mbrtowc` — but only if the locale is that encoding
  (chapter 68).

  The better prescription is not to create the problem — *convert to UTF-8 at the
  input boundary and handle only UTF-8 inside.*
]

== What does the converting

#dtable(
  columns: 3,
  [*Means*], [*Where*], [*Character*],
  [`iconv`], [POSIX], [Named by encoding. Outside standard C, but on every Unix],
  [The `MultiByteToWideChar` family], [Windows], [Named by code page number],
  [ICU], [Portable], [The most complete — normalisation, collation, graphemes],
  [The `mbrtowc` family], [Standard C], [*Only the locale's encoding* (chapter 68)],
  [Writing it yourself], [—], [UTF-8 ↔ UTF-16/32 is short. The demonstrations are the example],
)

Let us restate that standard C alone cannot handle an arbitrary encoding.
`mbrtowc` knows only "the encoding of the current locale". So a task like "read a
EUC-KR file and save it as UTF-8" is outside standard C and needs `iconv` or a
conversion table of your own.

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [Design], [UTF-8 byte strings inside, conversion at the boundary only],
    [Validation], [Once, at the input boundary. Reject overlong, surrogate and out-of-range],
    [Length], [Bytes, code units, code points, graphemes — settle which layer first],
    [Normalisation], [To NFC before comparing. Not in standard C],
    [Case], [ASCII rules for machines. Remember the Turkish `i`],
    [Legacy], [Shift_JIS, Big5 and GBK have trail bytes that reach into ASCII],
    [Scanning], [In legacy encodings, know the lead bytes and step over],
    [Conversion], [`iconv`, ICU, platform APIs. Standard C only does the locale's encoding],
  )
]

The story of characters that began in chapter 65 closes after six chapters —
from the judgement of one byte through locales and `wchar_t` to the prescriptions
of practice. The next chapter is what recent standards added, and the long
argument over "safe" functions.
