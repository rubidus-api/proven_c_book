#import "../../book/lib.typ": *

= Characters and text — scars in the standard

#prereq(
  ([chapter 8, Representing numbers], [a representation is an agreement, and agreements leave scars]),
)

#deepqa[
  Chapter 8 said the decimal point is not in the bits but in an agreement. Then
  what about letters — is the shape "가" inside the bits?
][
  No; the principle is the same. Bits have neither shape nor sound. All there is
  is numbers, and there is a separate *agreed numbering table* saying which
  number is which letter. To store a letter is to store a number, and text is a
  sequence of numbers. This chapter is the story of those tables.
]

#organizer[
  How to put letters into the lockers — that a character is, in the end, a
#idx("UTF-8")  number; what history that numbering table (the character code)
  went through on its way to Unicode and UTF-8; and the scar that history left
  in C's syntax (trigraphs). For a reader who writes Korean this chapter is
  unusually vivid.
]

#chapter-questions()

== A character is a number

There is only one way to store "A" in a machine — make an agreement that "A
shall be number 65" and store the number 65. Such a table of agreements, the
#idx("character set")correspondence between letters and numbers, is called a
*character set* or character code.

The problem is that there were *several* such tables. Every company and every
country made its own. The same number meant a different letter in different
tables, and when two machines with different tables exchanged text the letters
came out tangled. The rest of this chapter is the history of that tangle, and
every turn of that history left a trace in today's C and computing.

== ASCII and EBCDIC — two tables

Two branches of table established themselves in the 1960s. IBM's mainframe
world used a table called *EBCDIC* — grown out of punched-card codes, so its
layout looks strange today (the alphabet is not in consecutive numbers, among
other things), but in IBM's world it was law.

On the other side, centred on the communications industry, came *ASCII*. Seven
bits, that is a small table of 128 slots, holding the English upper and lower
case, digits, punctuation, and the *control characters* that are invisible on
screen. A control character is not a letter but an instruction to a device —
"advance the paper one line", "sound the bell" — and their story comes in the
next chapter (streams). And sitting at *slot 0* of this table is the *NUL
character* whose face we learned in chapter 6. A character meaning "no character
at all", which C later adopted as the mark for the end of a string
(chapter 39).

ASCII became the winning table — nearly every character code today carries
ASCII inside it. But 128 slots were tight even for English and nowhere near
enough for the world's writing. Here the history of the scars begins.

== ISO 646 — national variants and C's trigraphs

The first stopgap was *ISO 646*, the international edition of ASCII. The idea
went: "keep one table, but allow each country to replace a few less important
slots with its own letters." The slots designated for replacement happened to
include symbols such as `[ ] { } \ | #`.

The result was a disaster for programmers. Open C code on a Danish terminal and
Danish letters appeared where the braces should be. Same number, different
table, different letter printed.

#realcase[
  The ₩ sign — ISO 646's scar left in Korea
][
  Korea is a party to this history too. The Korean table (KS X 1003) replaced
  the backslash `\` with the won sign `₩`. On Korean computers of that era file
  paths looked like `C:₩Program Files₩...` — and, astonishingly, this scar can
  still be touched. Even on today's Korean Windows some fonts draw the backslash
  code as ₩. Same number, same bits, and the table (the font) draws ₩. This
  book's refrain, "what is stored and how it is read are separate things", is
  something Korean readers have witnessed on screen every day.
]

C was standardised in the middle of this confusion (chapter 12). Since C had to
be usable in countries where braces were invisible, C89 provided a grotesque
detour — the *trigraph*. Write `??<` and it was read as `{`; write `??/` and it
was read as `\`; a three-character cipher. A scar of the character-code wars,
carved into the grammar of the language.

#qa[
  Are trigraphs still used?
][
#idx("Unicode")  No — and their end is a symbolic scene in recent C history. As
  the Unicode era arrived trigraphs lost their reason to exist and survived only
  as a trap in which a string like `??!` was unexpectedly transformed. C23
  *removed* trigraphs from the standard. A scar carved in 1989 was erased only
  in 2023 — which also shows how much slower a standard is at *removing*
  something than at admitting it.
]

== A hundred schools of eight bits, and Hangul

Once the byte settled at 8 bits (chapter 4), the *ISO 8859* family extended the
table to 256 slots — the first 128 exactly ASCII, the second 128 holding the
letters of one language region (Latin-1 for Western Europe, and so on). But
since the upper 128 were used differently per region, the problem of tangled
letters when you picked the wrong table remained.

And there were writing systems for which 256 slots were nowhere near enough.
Hangul, Han characters, kana — East Asian scripts run to thousands and tens of
#idx("encoding")thousands. The solution was multi-byte encodings using *several
bytes per character* (Korea's EUC-KR among them). With lengths varying per
character and methods varying per country, a Korean document read with the wrong
table breaking into a parade of meaningless symbols — the so-called "broken
characters" — was daily life for Korean computer users of that era.

== Unicode and UTF-8 — one table, a clever way of holding it

In the end there was only one fundamental solution. *Put every letter in the
world into one table.* That is *Unicode*, from the 1990s. Each letter gets a
unique number (a code point), written like `U+AC00` (가). The table has more
than a million slots.

The remaining problem was *how to hold* those large numbers in bytes. Spending
4 bytes on every letter is wasteful and, above all, incompatible with the
mountains of existing ASCII text. The answer was the variable-length encoding
*UTF-8* — letters in the ASCII range stay 1 byte as they are, and everything
else takes 2–4 bytes. Thanks to the exquisite design by which an existing ASCII
file is already valid UTF-8, UTF-8 has become effectively the only standard for
the web and for source code today. It is what this book's examples and C23's
`u8""` strings use as well (chapter 39).

#mathbox[
  The byte structure of UTF-8
][
  The length varies with the size of the code point. The leading bits of the
  first byte announce how many bytes this character has, and every continuation
  byte starts with `10`:

  #dtable(
  columns: 3,
    [*range*], [*bytes*], [*bit layout*],
    [U+0000 – U+007F], [1], [`0xxxxxxx` (= ASCII)],
    [U+0080 – U+07FF], [2], [`110xxxxx 10xxxxxx`],
    [U+0800 – U+FFFF], [3], [`1110xxxx 10xxxxxx 10xxxxxx`],
    [U+10000 – U+10FFFF], [4], [`11110xxx 10xxxxxx ×3`],
)

  Because continuation bytes always start with `10`, you can recover the
  character boundary even if you wake up in the middle of the text
  (self-synchronisation). Hangul syllables (around U+AC00) live in the 3-byte
  range.
]

#misconception[
  "One character is one byte"
][
  A misconception left over from the intuitions of the ASCII era, and especially
  dangerous when learning C — because C's `char` type is, despite the name, not
  a "character" but a *byte* (chapter 39). In UTF-8 the English `A` is 1 byte
  but the Hangul `가` is 3. The two letters of "안녕" are six bytes. Character
  count and byte count are different objects, and code in which the distinction
  has collapsed will certainly cause an accident when handling Hangul.
]

== The two character sets C keeps apart — source and execution

So far this has been the world's story of character sets. Now for *how C handles
them.* The core is one sentence --- **C treats the character set the source code is
written in and the character set of the running program as two different things.**

Start with the standard's exact names (§5.2.1).

#dtable(
  columns: 3,
  [*The standard's term*], [*What it is*], [*Note*],
  [source character set], [the set of characters the source file *is written in*], [mapped in translation phase 1],
  [execution character set], [the set *interpreted in the execution environment*], [its members' values are implementation-defined],
  [basic character set], [the common denominator both must have], [52 Latin letters, 10 digits, 32 graphic characters, space and a few controls],
  [extended characters], [the locale-specific rest, absent from the basic set], [Hangul lives here],
  [extended character set], [the basic set and the extended characters together], [],
)

The standard does not say the two must share an encoding. Section 5.2.2 splits them
explicitly --- the execution character set may also contain multibyte characters,
and those are *not required to have the same encoding as for the source character
set.* And §5.2.1 drives in one more nail: *the values of the members of the
execution character set are implementation-defined.*

=== Where the crossing happens — translation phases 1 and 5

The place the two sets meet is pinned down in the translation phases (chapter 54).

#dtable(
  columns: 2,
  [*Phase*], [*What the standard makes it do (§5.1.1.2)*],
  [1], [physical source file multibyte characters are mapped, *in an implementation-defined manner*, to the source character set],
  [5], [each source character set member and escape sequence in character constants and string literals is *converted to the corresponding member of the execution character set*],
)

Phase 5's second sentence matters most --- where there is *no* corresponding member,
it is converted, in an implementation-defined manner, to some member other than the
null character. That is: *it quietly becomes a different character.* Not an error.

#misconception[
  "The letters I typed in the source go into the executable unchanged"
][
  For the basic character set --- letters, digits, symbols --- effectively yes. But
  an *extended character* such as Hangul is converted once, in phase 5. Write the
  source in UTF-8 and, if the execution character set is EUC-KR, EUC-KR bytes are
  what get embedded --- chapter 17 measures exactly this.

  So "what encoding the source file is in" and "what bytes the program emits" are
  *different questions*. Treat them as one and you will never narrow down which link
  to fix when Korean text breaks.
]

=== The more precise name C23 added — the literal encoding

C23 added §6.2.9, "Encodings", tidying up one more name.

#dtable(
  columns: 2,
  [*The standard's term (§6.2.9)*], [*Definition*],
  [literal encoding], [an implementation-defined mapping of the characters of the execution character set to *the values in a character constant or string literal*],
  [wide literal encoding], [the same mapping for `wchar_t` character constants and string literals],
)

If the "execution character set" is *which characters exist*, the "literal encoding"
is *what byte values write them.* The standard requires only that both map the whole
basic execution character set, and leaves the rest to the implementation.

#platform[
  C++ renamed all of this
][
  C++20 and C++23 overhauled the same concepts. C++ uses *translation character set*
  in place of "source character set" and pins it to Unicode, while using the same
  names, *literal encoding* and *wide literal encoding*, for the execution side.

  So when reading both standards together, know that the vocabulary diverges ---
  *C has no term "translation character set" yet.* C23 (N3220) §5.2.1 still says
  source character set and execution character set, and only §6.2.9 adds the name
  literal encoding.
]

=== Prefixes that nail the encoding down

There is a way to fix an encoding *regardless* of the execution character set ---
the literal prefixes (§6.4.5).

#dtable(
  columns: 4,
  [*Literal*], [*Element type*], [*Encoding*], [*Who guarantees it*],
  [`"가"`], [`char`], [the literal encoding --- implementation-defined], [nobody. Options change it],
  [`u8"가"`], [`char8_t` (C23)], [*always UTF-8*], [the standard],
  [`u"가"`], [`char16_t`], [UTF-16], [if `__STDC_UTF_16__` is 1],
  [`U"가"`], [`char32_t`], [UTF-32], [if `__STDC_UTF_32__` is 1],
  [`L"가"`], [`wchar_t`], [the wide literal encoding], [Unicode if `__STDC_ISO_10646__` is defined],
)

Where the bytes must not move --- protocols, file formats, a test's expected value
--- write `u8"…"`. *Two characters that turn "the implementation decides" into "the
standard decides."* The fuller story of the wide side returns in chapters 67 and 68.

== Same letter, several representations — the security terrain hidden in text

Unicode unifying the table did not end the problems. If anything, as the table
grew and the representations layered up, new traps appeared. It is terrain any
program handling text meets eventually, so let us survey it as a map. The root
is single — *if the same thing can be written in two or more ways, the side
checking it and the side interpreting it can disagree.*

#idx("normalization")*① Normalization — Hangul's NFC and NFD.* The letter "각"
can be written two ways in Unicode: as one composed syllable (NFC, the single
code point `U+AC01`) and as a sequence of jamo (NFD, the three code points
`ᄀ`+`ᅡ`+`ᆨ`). They look identical, but the byte sequences are entirely
different — so a naive byte comparison answers "각 ≠ 각". For Korean users this
is not a textbook story: because macOS stored file names in the NFD family, it
was common for Hangul file names crossing to another OS to appear with the jamo
undone, or for names to go wrong inside archives. The solution is to *normalise
to one form at the boundary* — and in a security context the order of that
normalisation is decisive (see ④ below).

*② Overlong encodings — the back door UTF-8 once left open.* By the UTF-8 rules
in the maths box above, `/` (U+002F) is the single byte `2F`. But early
implementations generously accepted the same character written "long", in 2
bytes (`C0 AF`) or 3 — an *overlong* representation. From this came a classic
attack: the security check looked only for `2F` to filter out path traversal
(`../`), while the file system interpreted the overlong `C0 AF` as `/` too. A
disagreement between checking and interpreting — that was the identity of the
directory-traversal vulnerability that swept IIS servers in 2001. Today's
standard therefore *declares overlong forms illegal and requires decoders to
reject them*.

*③ The ghost of a vanished encoding — UTF-7.* UTF-7, made to send Unicode
through 7-bit channels (early email), represents other characters using
ordinary-looking ASCII letters (`+ADw-` becomes `<`). In the days when browsers
guessed encodings automatically, attackers used this property to get through
filters — a harmless-looking string at checking time that resurrects as a tag
the moment the browser interprets it as UTF-7. UTF-7 was effectively retired
because of these incidents, and today's norm has hardened into *do not guess the
encoding; state it*.

*④ Traps in Unicode itself — homoglyphs and direction controls.* A large table
also means there are *different letters that look identical*. Latin `a` and
Cyrillic `а` are indistinguishable on screen — using this to build a fake domain
that looks exactly like the real one is a *homograph attack* (which is why
browsers show suspicious mixed-script domains back in their original notation).
More cunning are the *directional control characters*: Unicode has invisible
characters that reverse the display order of text, making it possible for *the
order a human reads in the source and the order the compiler reads to differ* —
"Trojan Source", published in 2021, used this technique to show that code
looking perfectly fine to a reviewer could compile with different logic.
Compilers began warning about such characters afterwards.

*⑤ When layers stack — escaping and multiple interpretation.* To display `<` as
a letter on the web you write `&lt;` (HTML escaping). Stack several such layers
and accidents follow — decode once-escaped text in another layer and the
original symbol resurrects; conversely, check but forget to escape and the input
becomes code. The format-string vulnerability of chapter 22, SQL injection in
databases and XSS on the web are all the same pattern — *making data be mistaken
for the frame (the code)*. C's trigraphs were an ancestral case of the pattern:
if `??/` happened to appear inside a string, the preprocessor turned it into a
backslash, creating an escape the programmer never wrote (one of the reasons
C23's removal of trigraphs is welcome).

#qa[
  If the principle a programmer should take from these traps were reduced to
  one, what would it be?
][
  *Do not let the representation change between checking and interpreting.*
  Unfolded into practical rules there are three — first, *do not guess the
  encoding; state it* (pin UTF-8 at the input/output boundary). Second,
  *normalise and decode first, and check afterwards* — reverse the order and the
  overlong and UTF-7 accidents come back. Third, *do not mix data into the
  frame* — a value always goes in the place for values (the
  `printf("%s", input)` idiom of chapter 22 is the smallest practice of this
  principle). All three come from this book's refrain that representation and
  interpretation are separate.
]

== Sequences of letters — ways of holding a string

Now that we know how to hold one letter, a last question remains. When a
*sequence* of letters — a *string* — goes into the lockers, how do we know
"where does this string begin and end?" Chapter 5's refrain returns: memory does
not know the boundaries of a lump; what knows is the agreement of the reader.
There are several such agreements, so let us look at the main branches.

*Method 1 — write the length in front (length-prefixed).* Attach a number
meaning "this many characters follow" in front of the string. The Pascal family
used it, so it is also called a *Pascal string*. Asking for the length needs no
counting (read the number and you are done), and any byte at all — even
character 0 — can be held as content. The price is that the size of the length
field has to be fixed in advance — old Pascal held the length in one byte, so a
string could not exceed 255 characters (chapter 4's container story, again).

*Method 2 — plant a marker at the end (NUL-terminated).* Instead of writing the
length, plant a special character at the *end* of the sequence — the NUL
character of value 0 whose face we learned in chapter 6. *This is the method C
chose.* Its virtue is extreme simplicity — you need only the starting point, and
pointing anywhere in the middle of the sequence makes "from there to the NUL"
a string. The price is threefold: to know the length you must *count all the way
to the NUL*; you cannot hold a NUL in the content; and above all, forget to
plant the marker and the reader runs on into other people's land. That last
price is the terrain of more accidents than any other in C's history, and
chapter 39 is the scene.

*Method 3 — manage length and capacity together.* The dynamic strings of modern
languages usually manage three values as one bundle: [where the content is, the
current length, the capacity of the container] — length queries are immediate,
appending is free within the capacity, and when it runs out the content moves to
a larger container. C++'s `std::string` and Rust's `String` have this structure,
and proven's string handling, which we meet later, thinks in the same family by
treating length explicitly (Part XII).

#realcase[
  The magic of fifteen characters — small string optimisation in the MS STL
][
  Real implementations of method 3 have a clever double mechanism. In the
  Microsoft standard library implementation (MS STL) of neighbouring C++,
  `std::string` does not borrow a big warehouse (dynamic memory) for a short
  string — specifically *15 characters or fewer* (15 bytes plus the terminator) —
  but *simply holds it inside the body of the string object itself*. The
  technique is called SSO, small string optimisation. Why 15 — because reusing,
  as a content buffer for short strings, the space of the pointer, length and
  capacity fields that must be in the object anyway comes out to exactly that
  size. The effect is explained by chapter 11's knowledge: most real-world
  strings are short, and those now travel attached to the object with no
  warehouse round trip (allocation), moving together on the same cache line.
  Other implementations (the GCC and Clang standard libraries) use the same
  technique with different numbers — a modern textbook case of "the choice of
  representation is speed."
]

The terrain is wider still — representations for editors that manage a large
document as a tree of pieces (ropes), views that point at [start, length]
without copying the original, and so on: each use has its own fitting
representation. One thing to remember: *the shape a string takes in memory is
not one thing but a choice made by a language and a library, and that choice
determines the character of its performance and safety.* What character C's
choice (NUL termination) has, and how it must be handled, is faced head on in
chapter 39.

#qa[
  Why does a beginner need to know this messy history now? Is it not enough to
  use UTF-8 only?
][
  For anything new, UTF-8 alone is enough — that is today's practice and this
  book's premise. But the history is needed for two reasons. First, old
  encodings still flow through the world's files and systems, so when you meet
  broken characters you must be able to diagnose "ah, the wrong table." Second,
  C itself is a product of this history — the name `char`, NUL termination, the
  wreckage of trigraphs — so knowing the history makes all of C's strange
  corners read as wounds with a story. There is nothing to memorise. "A character
  is a number; there were many tables until Unicode unified them; and UTF-8 won
  as the way of holding them" — those three sentences are enough.
]

The next chapter is the last rung on the ladder of representation. Now that we
can hold letters, it is the story of letters *flowing* — why a computer's input
and output is shaped as "characters going past one line at a time", an answer
that lies in the age of paper cards and typewriters.
