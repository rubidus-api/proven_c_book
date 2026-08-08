#import "../../book/lib.typ": *

= Strings

#prereq(
  ([chapter 9, Characters and text], [the representation of letters]),
  ([chapter 38, Arrays], [arrays and subscripts]),
)

#deepqa[
  Chapter 9 said the string representation C chose is NUL termination, "planting
  a marker at the end", and foreshadowed its three prices (the length must be
  counted, a NUL cannot be held as content, lose the marker and it runs away).
  Now that you have learned arrays (chapter 38) — how would you define a C string
  exactly, yourself?
][
  *A char array, somewhere in which there is a byte of value 0 (the NUL
  character, `'\0'`), plus the agreement that "the string" means from the start
  to just before that marker* — that is all. There is no separate string type. An
  array, and an agreement. This chapter treats the use and the cost of that
  agreement.
]

#organizer[
#idx("string")  C's string faced head on — its identity as a char array plus
  NUL termination, the special circumstances of string literals, the real cost of
  measuring length, and the practical matter of "character count ≠ byte count"
  that Hangul brings out. The last of the three nulls (the NUL character) gets
  its formal treatment here.
]

#chapter-questions()

== The identity — an array, and an agreement

`char greet[] = "안녕";` — initialise an array with a string literal and the
compiler builds the array by *appending `'\0'` after* the bytes of the
characters. The demonstration dissects it.

#demo("examples-en/ch41/str.c")

There are layers to read. `strlen` (the length measurement of the standard
`<string.h>`) answered 6 — "안녕" is *two characters, six bytes* (exactly
chapter 9's UTF-8 table: Hangul syllables live in the 3-byte range). The byte
dump `EC 95 88 EB 85 95` is the bare face of those six bytes, and `sizeof greet`
is 7 — the size of a container that also holds the NUL marker. This is the moment
chapter 8's misconception ("one character = one byte") is disproved by an
execution result.

`strlen`'s *cost* can now be stated exactly too — as chapter 9 foretold, NUL
termination does not write the length down, so strlen *walks one slot at a time
until it meets the marker*. The longer the string the longer it takes
(proportional to the slot count), which is why code calling strlen in a loop
condition every time is a classic performance trap (the demonstration's loop is
in fact that pattern — harmless for a short string, but for a long one the
practice is to take the length into a variable first).

== String literals — read-only ground

Instead of initialising an array you may hold a literal in a pointer — and here
lies an important difference:

```c
char buf[] = "you may change this";      /* copied into my array — modifiable */
const char *msg = "you must not change this";  /* points at the literal itself */
```

A string literal is itself *read-only data* baked into the program — attempting
to modify it is outside the contract, and in a modern environment it usually
collapses on the spot (literals being placed in a write-forbidden region —
chapter 6's protected zone doing another kindness). So the rule is to declare
pointers to literals as `const char*` — where chapter 23's `const` was
documentation saying "I will not change this", here it works as a lock that stops,
at compile time, the mistake of trying to change what must not be changed.

=== The type is not `const` — C's odd place

And here lies C's famous contradiction. *It must not be modified, and yet its type
carries no `const`.* The type of `"abcdef"` is not `const char[7]` but plain `char[7]`.

```c
char *p = "hello";     /* C: it passes without a warning */
p[0] = 'H';            /* but this is undefined behaviour */
```

That is, the compiler does not block it. "Not modifiable" exists *only as a contract,
not as a type*, and breaking it is chapter 51's undefined behaviour — mostly it dies on
the spot thanks to being placed in a write-protected region, but that is the
implementation's kindness, not the language's guarantee.

*C++ differs.* In C++ a string literal's type is `const char[N]`, and the first line
above is *a compile error*. This item belongs in the list of "differences between the
two languages" seen in chapter 94.

#qa[
  Why did C not attach `const`?
][
  *So as not to break existing code.* The word `const` itself entered the standard only
  with C89 (chapter 23), and by then the world already had mountains of code putting
  string literals into a `char *`. The moment the literal's type became `const char[N]`,
  all of that code would be subject to diagnosis.

  It is the same circumstance as `gets`'s funeral taking twenty years (chapter 63) —
  *the standard is an institution that must protect existing code*, so when "the right
  type" and "code already written" collide it leans towards the latter. C++, first
  standardised in 1998, carried no such burden and could attach `const` from the start
  (chapter 94's "siblings, not parent and child" is confirmed here too).

  So discipline stands in for the language. *A pointer at a literal is always declared
  `const char *`.* Then what the type cannot do has been written in by a human, and from
  there the compiler keeps it. GCC's and Clang's `-Wwrite-strings` is an option that
  changes a literal's type to `const char[N]` outright, but it is not the default
  because warnings pour out of old code.
]

#qa[
  What of the newest standard and the ones to come?
][
  *It stands as it is up to C23.* A string literal's type is still `char[N]` and
  modifying one is still undefined behaviour. C23 changed two things while leaving this
  place alone — settling `u8"..."`'s element type as `char8_t` and pinning down two's
  complement are both separate from this clause.

  Nor is there any sign of change in the next edition (C2y). The reason is the one above
  — changing the type now would make half a century of code subject to diagnosis. The way
  the standard tidies this place has been not to mend the type but *to let the tools
  warn*, and that is likely to remain so.

  In summary, remember it like this — *the type is `char[N]`, the contract is "do not
  modify", and the defence is `const char *` plus compiler warnings.*
]

== A literal is an array too

The previous section said a literal may be copied into an array or pointed at by a
pointer, and one step further in there is a surprising fact — *a string literal is
itself already an array.* Written with its type exactly, `"abcdef"` is `char[7]` (six
characters + the NUL).

#demo("examples-en/ch41/literal.c")

The output's first two lines are that confirmation. `sizeof "abcdef"` is 7 and
`sizeof ""` is 1 — not a pointer's size (8) but *the array's size*. A literal decaying
into a pointer is the decay seen in chapter 38, and `sizeof` being the exception to
decay is why the array's real size shows.

Something amusing follows from its being an array. Being an array, *a subscript can be
attached.*

```c
"abcdef"[3]     /* 'd' */
```

And recalling the identity `a[i] ≡ *(a + i)` learned in chapter 38, since addition
commutes, `*(a + i) ≡ *(i + a) ≡ i[a]`. That is,

```c
3["abcdef"]     /* 'd' as well — entirely legal grammatically */
```

That the example prints `d` for both is the evidence. There is no use for it in
practice (it merely bewilders the reader) and it is notation seen only in obfuscation
contests, but no example shows more clearly that *the array subscript is not a
decoration of the grammar but another notation for pointer arithmetic*.

== Concatenation — adjacent literals become one

Write two string literals side by side and the compiler joins them into one.

```c
"abc" "def"        /* -> "abcdef" — no comma, no operator */
```

That the example's `sizeof("abc" "def")` is 7 confirms it (3+3+NUL). This happens not
in the preprocessor but in *translation phase 6* (chapter 56's table) — so a string
fragment produced by a macro joins with the literal beside it too.

There are two uses.

*① Writing a long sentence over several lines.* It is cleaner than joining lines with
a backslash — because the indentation does not go inside the string.

```c
const char *help =
    "usage: tool [options] file\n"
    "  -v   verbose\n"
    "  -o   output file\n";
```

*② Keeping fragments under names and assembling them.* This is the use most often seen
in practice, and it appears with three faces.

*Assembling a version string.* The value is written in one place and woven in
elsewhere.

```c
#define MY_PROGRAM_VERSION  "v3.1.2"
#define PROGRAM_TITLE       "my_program " MY_PROGRAM_VERSION

puts(PROGRAM_TITLE);                       /* my_program v3.1.2 */
puts("build: " __DATE__ " " __TIME__);     /* with the compiler's own literals too */
```

The point is that raising the version means mending *one line*. It weaves just as well
with literals the compiler predefines (`__DATE__`, chapter 56), so many programs build
their banner and `--version` output this way.

*Keeping format fragments under names.* The same technique applied to `printf`'s
format.

```c
#define ID_FMT    "%d"
#define TEMP_FMT  "%.1f"

printf("id = " ID_FMT ", temp = " TEMP_FMT "\n", id, temp);
```

The demand that a format string *be a constant* (chapter 60's format string
vulnerability, and the compiler's format checking) is kept while the fragments are
managed under names. It contrasts with passing a variable, as in
`printf(fmt_string_variable, …)`, which makes all that checking vanish at once —
*concatenation finishes at compile time, so the result is still one literal.*

*The standard library uses the same technique.* The format macros of `<inttypes.h>`
seen in chapter 77 and appendix B stand exactly on this rule.

```c
printf("total = %" PRIu64 "\n", total);
```

`PRIu64` is defined as `"lu"` or `"llu"` (it differs by platform) and joins with the
fragments before and after into one format string. The problem that the format for a
fixed-width integer differs by platform was solved *by concatenation alone*.

The three cases come to the same one thing — *give literal fragments names and weave
them at the place of use.* And all three pair with chapter 56's `#` operator. If the
version is managed as numbers, the numbers can be turned into strings and woven.

```c
#define VER_MAJOR 3
#define VER_MINOR 1
#define STR_RAW(x) #x
#define STR(x)     STR_RAW(x)                     /* double expansion (chapter 56) */
#define VERSION    "v" STR(VER_MAJOR) "." STR(VER_MINOR)   /* "v3.1" */
```

#antipattern[
  Variables do not join
][
  ```c
  const char *unit = "°C";
  printf("temp = %.1f" unit "\n", t);   /* a compile error */
  ```
  Concatenation is a rule *between literals*. `unit` is a variable and so cannot join.
  To insert a variable's content the answer is one more placeholder —
  `printf("temp = %.1f%s\n", t, unit)`.

  For the same reason, when keeping a fragment to be joined in a `#define`, *that macro
  must expand to a string literal*. Define it as something that is not a literal and it
  will not join.
]

== Prefixes — a literal of which encoding

A letter may be attached before a literal to settle *which character type's array it
is*.

#dtable(
  columns: 4,
  [*notation*], [*element type*], [*encoding*], [*`sizeof "AB"`*],
  [`"AB"`], [`char`], [the execution character set (mostly UTF-8)], [3],
  [`u8"AB"`], [`char8_t` (C23) / `char`], [UTF-8], [3],
  [`u"AB"`], [`char16_t`], [UTF-16], [6],
  [`U"AB"`], [`char32_t`], [UTF-32], [12],
  [`L"AB"`], [`wchar_t`], [the implementation settles it], [12 on the example's machine],
)

The example really prints this table. That `L"AB"` is 12 bytes is because this Linux
machine's `wchar_t` is 4 bytes — *on Windows it is 2 bytes and this becomes 6.* It is
where chapter 66's "the size of `wchar_t` differs by implementation" shows itself at
the level of literals.

Character constants take the same prefixes (`L'A'`, `u'A'`). And where concatenation
meets prefixes there is one more rule — *if only one side has a prefix, the prefixed
side wins* (the example's `L"wide" " and narrow"` becomes a wide string), and
*joining two different prefixes is outside the contract.*

#realcase[
  gets — the function expelled from the standard
][
  Attached to the third price of NUL termination (lose the marker and it runs
  away) is the most famous funeral in the history of the C standard. `gets` in the
  early standard library was a function that "reads a line and puts it into an
  array" — and *it did not take the container's size*. If the input was longer
  than the container it overwrote the neighbouring memory as it went: a function
  with the boundary violation of chapter 38 built into its design. In 1988 the
  internet's first large-scale worm (the Morris worm) spread through defects of
  this class, and after decades of accidents the C11 standard *deleted* `gets` —
  a rare case of a standard formally burying one of its own functions. That is the
  reason this book taught `fgets` (the successor that takes the container's size)
  from the start in chapter 25 — and the next chapter faces this whole class of
  accident head on.
]

#qa[
  To handle Hangul properly — to work in units of characters — what must be done?
][
  Distinguishing two layers is the starting point. At the *byte layer* (the world
  of C strings) you may treat UTF-8 simply as a byte sequence — copying, joining,
  storing and transmitting are safe without knowing character boundaries (thanks
  to UTF-8's self-synchronising design, chapter 9). Work that needs the
  *character layer* (counting characters, cutting, case conversion) requires UTF-8
  decoding, and since the standard library's support is thin there, using a
  library is the practice (Part XII covers one such library). Keep one rule in
  mind and
  most accidents are prevented: *do not cut a string by byte index* — cut through
  the waist of a Hangul syllable and you get a broken byte sequence.
]

We know the string's identity and cost. The next chapter is this part's turning
point — a dissection of the class of accident called the boundary violation, and
five disciplines for handling a failed parse. The "safe input" seed planted in
chapter 25 is finally collected.
