#import "../../book/lib.typ": *

= Characters and text - scars in the standard

#organizer[
  How to put letters into the lockers — that a character is, in the end, a
#idx("UTF-8")  number; what history that numbering table (the character code)
  went through on its way to Unicode and UTF-8; and the scar that history left
  in C's syntax (trigraphs). For a reader who writes Korean this chapter is
  unusually vivid.
]

#deepqa[
  Chapter 6 said the decimal point is not in the bits but in an agreement. Then
  what about letters — is the shape "가" inside the bits?
][
  No; the principle is the same. Bits have neither shape nor sound. All there is
  is numbers, and there is a separate *agreed numbering table* saying which
  number is which letter. To store a letter is to store a number, and text is a
  sequence of numbers. This chapter is the story of those tables.
]

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
character* whose face we learned in chapter 4. A character meaning "no character
at all", which C later adopted as the mark for the end of a string
(chapter 36).

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

C was standardised in the middle of this confusion (chapter 10). Since C had to
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

Once the byte settled at 8 bits (chapter 2), the *ISO 8859* family extended the
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
`u8""` strings use as well (chapter 36).

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
  a "character" but a *byte* (chapter 36). In UTF-8 the English `A` is 1 byte
  but the Hangul `가` is 3. The two letters of "안녕" are six bytes. Character
  count and byte count are different objects, and code in which the distinction
  has collapsed will certainly cause an accident when handling Hangul.
]

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
becomes code. The format-string vulnerability of chapter 21, SQL injection in
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
  `printf("%s", input)` idiom of chapter 21 is the smallest practice of this
  principle). All three come from this book's refrain that representation and
  interpretation are separate.
]

== Sequences of letters — ways of holding a string

Now that we know how to hold one letter, a last question remains. When a
*sequence* of letters — a *string* — goes into the lockers, how do we know
"where does this string begin and end?" Chapter 3's refrain returns: memory does
not know the boundaries of a lump; what knows is the agreement of the reader.
There are several such agreements, so let us look at the main branches.

*Method 1 — write the length in front (length-prefixed).* Attach a number
meaning "this many characters follow" in front of the string. The Pascal family
used it, so it is also called a *Pascal string*. Asking for the length needs no
counting (read the number and you are done), and any byte at all — even
character 0 — can be held as content. The price is that the size of the length
field has to be fixed in advance — old Pascal held the length in one byte, so a
string could not exceed 255 characters (chapter 2's container story, again).

*Method 2 — plant a marker at the end (NUL-terminated).* Instead of writing the
length, plant a special character at the *end* of the sequence — the NUL
character of value 0 whose face we learned in chapter 4. *This is the method C
chose.* Its virtue is extreme simplicity — you need only the starting point, and
pointing anywhere in the middle of the sequence makes "from there to the NUL"
a string. The price is threefold: to know the length you must *count all the way
to the NUL*; you cannot hold a NUL in the content; and above all, forget to
plant the marker and the reader runs on into other people's land. That last
price is the terrain of more accidents than any other in C's history, and
chapter 36 is the scene.

*Method 3 — manage length and capacity together.* The dynamic strings of modern
languages usually manage three values as one bundle: [where the content is, the
current length, the capacity of the container] — length queries are immediate,
appending is free within the capacity, and when it runs out the content moves to
a larger container. C++'s `std::string` and Rust's `String` have this structure,
and proven's string handling, which we meet later, thinks in the same family by
treating length explicitly (chapter 37).

#realcase[
  The magic of fifteen characters — small string optimization in the MS STL
][
  Real implementations of method 3 have a clever double mechanism. In the
  Microsoft standard library implementation (MS STL) of neighbouring C++,
  `std::string` does not borrow a big warehouse (dynamic memory) for a short
  string — specifically *15 characters or fewer* (15 bytes plus the terminator) —
  but *simply holds it inside the body of the string object itself*. The
  technique is called SSO, small string optimization. Why 15 — because reusing,
  as a content buffer for short strings, the space of the pointer, length and
  capacity fields that must be in the object anyway comes out to exactly that
  size. The effect is explained by chapter 9's knowledge: most real-world
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
chapter 36.

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
