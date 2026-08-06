#import "../../book/lib.typ": *

= Character classification — `<ctype.h>`

#prereq(
  ([chapter 9, Characters and text], [letters and encodings]),
  ([chapter 26, Integers], [the signedness of `char`]),
)

#deepqa[
  Chapter 9 said a character is a number in a code table and an encoding is a way
  of writing that number in bytes. What, then, is a call like `isalpha('가')`
  asking?
][
  It is the wrong question. The functions of `<ctype.h>` judge *one byte*. In
  UTF-8 "가" is three bytes, so it cannot even be passed as the argument, and if
  it were, each byte would be looked at separately. This header is a tool from
  the ASCII era.

  So this chapter is short. The contract of twelve functions, and the rule broken
  most often in all of C — *passing a `char` straight in is outside the
  contract* — is all of it. The two large subjects waiting behind it, locales and
  multibyte characters, belong to the next five chapters.
]

#organizer[
#idx("character classification")  The functions that judge a single byte. A map of the twelve, the exact range of
  values allowed as the argument, why `EOF` is mixed in among them, and the fact
  that even this judgement is changed by the locale.
]

#chapter-questions()

== A map of the twelve functions

`<ctype.h>` fixes eleven predicates and two conversions. The naming rule is
simple — `is` asks a true-or-false question, `to` changes something.

#dtable(
  columns: 3,
  [*Function*], [*True for*], [*In the "C" locale*],
  [`isalpha`], [Letters], [`A`\~`Z`, `a`\~`z`],
  [`isdigit`], [Digits], [`0`\~`9` — *these ten, regardless of locale*],
  [`isalnum`], [Letters or digits], [The two above together],
  [`isspace`], [Whitespace], [space, `\t`, `\n`, `\v`, `\f`, `\r`],
  [`isblank`], [Whitespace within a line], [space, `\t`],
  [`isupper`·`islower`], [Upper- and lower-case], [`A`\~`Z` / `a`\~`z`],
  [`ispunct`], [Printing, and neither alphanumeric nor space], [`!`, `,`, `\#`, …],
  [`isprint`], [Printing characters (space included)], [0x20\~0x7E],
  [`isgraph`], [Printing and not a space], [0x21\~0x7E],
  [`iscntrl`], [Control characters], [0x00\~0x1F, 0x7F],
  [`isxdigit`], [Hexadecimal digits], [`0`\~`9`, `A`\~`F`, `a`\~`f`],
  [`toupper`·`tolower`], [(conversion) to upper or lower case], [Returned unchanged if it does not apply],
)

`isdigit` is special. The standard nails the set of characters for which it is
true to the ten from `0` to `9`, *regardless of the locale*. That contrasts with
the other predicates, which may widen. It is the one thing code that parses
numbers can lean on.

== The first trap — never pass a `char` straight in

#demo("examples-en/ch62/ctype.c")

Every function in `<ctype.h>` takes an `int`. And the value the standard requires
of that argument is *one representable as an `unsigned char`, or `EOF`*.

The trouble is that `char` is signed on many implementations (chapter 26). As the
example prints, the byte 0xC7 held in a `char` becomes −57, and passing that
straight into `isalpha` passes *a value that is not allowed* — outside the
contract. Real implementations are usually built as array lookups, so it becomes
a read from before the start of an array.

There is one idiom.

```c
isalpha((unsigned char)c)
toupper((unsigned char)c)
```

#antipattern[
  Passing a `char` straight in
][
  ```c
  char *p = line;
  while (*p) { if (isspace(*p)) ... ; p++; }   /* outside the contract past 0x80 */
  ```
  Unless ASCII is guaranteed, always convert.
  ```c
  while (*p) { if (isspace((unsigned char)*p)) ... ; p++; }
  ```
  In a program that handles Korean, Japanese or European text, this one line is
  the difference between an accident and none.
]

#qa[
  If a program only ever sees ASCII, may the conversion be skipped?
][
  Programs in which "only ASCII arrives" actually holds are rarer than they look.
  A program mostly takes *someone else's input* — file names, user names, pasted
  strings, and there is no way to stop Korean or an emoji from being among them.
  One command-line argument is enough to bring in a byte above 0x80.

  And it costs nothing. An `(unsigned char)` conversion usually generates no
  instruction at all — one of the few places where staying inside the contract is
  free.
]

== The second trap — `EOF` is mixed in

There is a reason the argument type is `int` and not `char`. *`EOF` is a valid
argument too.* What `fgetc` returns must be passable straight in (chapter 58), and
that value may be a byte or may be `EOF`.

```c
int c;
while ((c = fgetc(f)) != EOF)
    if (isalpha(c)) ...          /* what fgetc gave is already unsigned char or EOF */
```

Receive `c` as a `char` here and two things break at once — `EOF` becomes
indistinguishable from the byte 0xFF, and the value can go negative and fall into
the first trap. Chapter 58's rule, "always receive what `fgetc` returns in an
`int`", comes back here.

== The judgement depends on the locale

The set of bytes for which `isalpha` is true is not fixed. *The current locale*
(precisely, the `LC_CTYPE` category) settles it. A program starts in the `"C"`
locale, so at first the basis is ASCII — but it can change the moment
`setlocale` is called.

And it is not only the predicates. `toupper('i')` becoming the dotted capital
`İ` in a Turkish locale is the famous case, and in a locale whose decimal point
is a comma even the output of `printf("%f")` changes.

*The locale is a subject in its own right.* By what rule its names are made,
which standards fix them, and how much it changes are the next two chapters
(chapters 63 and 64).

== It means nothing for multibyte characters

One thing remains. The functions of this header look at *one byte*. "한" in UTF-8
is three bytes, so no byte of it means anything on its own — passing 0xED to
`isalpha` asks "is this fragment a letter?", and that question has no answer.

To judge a character made of several bytes, there are two roads. Convert to *wide
characters* and use `<wctype.h>`'s `iswalpha` (chapters 65 and 66), or *work on
the byte string as it is* and do only the judgements you need yourself
(chapter 67). This book recommends the latter, and chapter 67 says why.

#recap[
  #dtable(
    columns: 3,
    [*Situation*], [*Rule*], [*If you get it wrong*],
    [Calling `<ctype.h>`], [Convert with `(unsigned char)`], [A negative argument — outside the contract],
    [Handling `fgetc`'s result], [Receive it in an `int`], [`EOF` confused with 0xFF],
    [`isdigit`], [`0`\~`9` regardless of locale], [—],
    [The other predicates], [They depend on `LC_CTYPE`], [Change the locale, change the answer],
    [Multibyte characters], [Per-byte judgement is meaningless], [Judging a fragment of a letter],
  )
]

The judgement of a single byte is done. Now for the thing that governed it — *the
locale*. The next chapter starts from what a locale is and by what rule a name
like `ko_KR.UTF-8` is made.
