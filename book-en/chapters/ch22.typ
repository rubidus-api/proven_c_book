#import "../../book/lib.typ": *

= Output

#prereq(
  ([chapter 10, The origin of streams], [the origin of streams]),
  ([chapter 19, The structure of a program], [the skeleton of a program]),
)

#deepqa[
  Chapter 10 said that printing terminals naturally worked by *line buffering* —
  waiting until a line's worth had gathered before striking the paper. Does
  today's `printf` still carry that habit?
][
#idx("standard output")  It does — when standard output leads to a terminal,
  output usually gathers in an internal store (a buffer) until it meets a `\n`
  and then flows out a line at a time. In chapter 19's demonstration, the
  characters of `printf("Hello, ")` appearing on screen only when the second
  call's `\n` arrived was exactly this. Half a century after the circumstances of
  the paper typewriter disappeared, its rhythm lives on as software's default —
  the commotion this habit causes in practice ("my output isn't showing!") and
  how to govern it are met when we take up input and output properly.
]

#organizer[
#idx("format string")  `printf` faced head on — the frame called a format
  string, the blanks `%d` and `%s`, and matching the format against the
  materials. By the end of this chapter Part IV's goal is achieved: one piece of
  hello world becomes, apart from two lines still on credit, all known
  sentences.
]

#chapter-questions()

== printf — output with a format

The name `printf` is short for *print formatted* — "print it as the format
says." The first material is the *format string*, and the remaining materials
are the values to be fitted into the format's blanks:

```c
printf(format string, value1, value2, ...)
```

The format string is a *frame*. Ordinary characters flow straight out, and only
the places beginning with `%` — a *conversion specification* — are treated as
blanks, into which the following value is fitted, turned into characters. This
book's minimal set is three:

- `%d` — turns an integer value into decimal characters and fits it in
  (decimal).
- `%s` — fits a string in as it is (string).
- `%%` — not a blank; used when you want to print the percent character itself.

Here is the demonstration. Two values go into two blanks in order:

#demo("examples-en/ch22/fmt.c")

A string literal went into the `%s` slot and the value of the expression `6 * 7`
into the `%d` slot, completing the sentence. *The format is the frame, the
materials go in order* — that is the whole of reading `printf`. (Conversion
specifications have many side branches such as field widths, but they are
introduced one at a time as needed; the full list is reference material in the
appendix.)

#qa[
  Why is what was printed sometimes not visible at once — the program seems to
  hang and then everything pours out together?
][
  Because of the buffering seen in the looking-back exchange. When output goes to
  a terminal it flows by lines, but when it goes to a file or another program
  (chapter 10's redirection) it gathers until a larger lump has filled and then
  goes out all at once — a performance trick that reduces round trips. So a
  program printing progress, once joined to a pipe, appears to "hang and then
  print in a rush." When it must go out at once you can order the store emptied
  with `fflush(stdout)`, and when a program exits normally what remains is
  flushed automatically — but if it dies suddenly the contents of that store
  vanish, which is the common reason the log just before an accident is missing.
]

== Kinds of blank — a slot per kind of value

`%d` and `%s` carry you a fair way, but each kind of value has its own blank. Learn
the common ones first.

#demo("examples-en/ch22/convert.c")

#dtable(
  columns: 3,
  [*Blank*], [*What it takes*], [*Where the name comes from*],
  [`%d`], [an integer, in decimal], [decimal],
  [`%f`], [a real number in point notation --- six places by default], [floating],
  [`%c`], [one character], [character],
  [`%s`], [a string], [string],
  [`%x`], [an integer in hexadecimal --- `%X` for capitals], [hexadecimal],
  [`%%`], [not a blank but a percent sign itself], [---],
)

Integer, real, character, string --- chapter 20 taught four ways of writing a
constant, and here is one blank for each of the four. The remaining conversions
(unsigned integers, pointers, sizes) arrive with the types that need them, and the
full list is in appendix B.

== Lining things up --- width and precision

A blank can also say *how wide* and *how precise*. It pays off immediately when
making a table.

#dtable(
  columns: 3,
  [*Written*], [*Meaning*], [*Result*],
  [`%6d`], [reserve six columns, *right* aligned], [`|␣␣␣␣␣7|`],
  [`%-6d`], [a minus means *left* aligned], [`|7␣␣␣␣␣|`],
  [`%06d`], [fill the blanks with zeros], [`|000042|`],
  [`%.2f`], [two places after the point --- it rounds], [`0.67`],
  [`%8.2f`], [width and precision together], [`|␣␣␣␣0.67|`],
  [`%.3s`], [on a string it means *how many characters*], [`abc`],
)

Remember the reading order and it never confuses: after `%` come *alignment
(`-`, `0`) → width → dot and precision → the conversion letter.*

#misconception[
  "Give it a width of eight and eight characters fit"
][
  It counts *bytes*. The listing's last table shows it --- the English table is
  neat and the Hangul one is not. One Hangul syllable is three bytes in UTF-8
  (chapter 9), so the eight columns `%-8s` reserves hold barely two Hangul
  characters.

  Lining up a Hangul table in a terminal means *counting characters and padding by
  hand*, and even that raises another question: how many cells does one character
  occupy on screen (East Asian characters take two)? Chapter 71 meets this problem
  again.
]

== Output has more than one window

Besides `printf` there are three more, each for a different place.

#dtable(
  columns: 3,
  [*Function*], [*What it does*], [*When*],
  [`printf`], [prints by a format], [when values are slotted in],
  [`puts`], [prints one string *and the newline*], [a fixed line. Faster and safer than `printf`],
  [`putchar`], [one character], [when building output a character at a time],
  [`fprintf`], [prints *to a chosen stream*], [when it must not go to standard output],
)

The last matters. A program has *two* bands going out (chapter 10) --- *standard
output* (`stdout`), where results flow, and *standard error* (`stderr`), where error
messages flow.

```c
printf("result: %d\n", 42);                   /* standard output */
fprintf(stderr, "cannot open the file\n");    /* standard error */
```

They are separated because of *redirection* (chapter 10). Send the results to a file
and the error messages must still reach the screen; pipe the results to the next
program and error messages must not be mixed in. "Results to standard output, words
for a human to standard error" is the discipline of a program that behaves like a
tool (the same grain as chapter 52's exit status).

#qa[
  Why did the `stderr` line come out at the top of the listing's output?
][
  *Because standard error is not buffered.* Standard output, as the recall showed,
  gathers in a warehouse before flowing; standard error sends at once --- an error
  message must survive even if the program dies a moment later.

  So when both bands are captured into one file (as this book's listing verification
  does) *the order they came out in can differ from the order they were written in.*
  That is design, not accident, and worth knowing when reading logs.
]

== Matching — the contract between format and materials

A format string is a *contract*. The number and kinds of blanks promise the
number and kinds of materials that must follow. Write two `%d` and two integer
values must follow; a `%s` slot must receive a string.

Break the promise — put a string in a `%d` slot, or supply too few materials —
and it is outside the contract: undefined behaviour (the concept met in
chapter 7 appears in a function's manual too). Fortunately modern compilers
catch most of these mismatches through the warnings switched on in chapter 17
(`-Wall`) — `printf`'s format checking is among the oldest and most dependable
of the warning features.

#realcase[
  When a format string became a weapon — the format string vulnerability
][
  This matching contract appears in the history of security too. Wanting to print
  text a user typed, some programs put *the input into the format string slot*,
  as in `printf(user_input)`. If the input is ordinary text there is no problem,
  but when a malicious user sends in `%` signs — printf interprets them as blanks
  and starts rummaging through memory looking for materials that do not exist.
  Attacks that peeked into or wrecked a program's insides by this principle were
  epidemic in the 2000s and earned their own name, the "format string
  vulnerability." The correct code is `printf("%s", user_input)` — putting the
  input in as a *material*, not as the format. Do not mix data into the frame —
  this principle is a refrain of security in general.
]

#misconception[
  "printf is the function that prints to the screen"
][
  Practically it is mostly true, but strictly it is not — `printf` is the
  function that *lets characters flow onto the standard output stream*
  (chapters 10 and 15). If the end of that band is a terminal it appears on
  screen; turn the band towards a file with the operating system's facility
  (redirection) and the same program writes to a file. A program does not know
  where its output goes, and does not need to — that indifference is the power
  of the stream design. It is entirely thanks to this that one program serves,
  without being changed, for the screen, for a file, and as another program's
  input.
]

== Closing Part IV — rereading hello world

Time to keep the promise. Here are chapter 15's six lines again.

```c
#include <stdio.h>

int main(void)
{
    printf("Hello, world!\n");
    return 0;
}
```

Reading it: `#include` is a preprocessing directive pasting in the roster of the
stdio toolbox (chapters 16 and 19), `main`'s block binds a list of statements
(chapter 19), the expression statement of the `printf` call lets text flow onto
standard output with a string literal as material (chapters 20, 21 and 22), and
`\n` breaks the line (chapter 10). What was an incantation on first meeting has
become almost entirely known sentences.

Exactly two lines remain on credit — the `int` and `(void)` of
`int main(void)`, and `return 0`. Both are unravelled only once you know the
world of *declarations*, that is, how to make names and types yourself, and that
is the next part. At the end of it the debt is settled in full — and then, at
last, we get to take input too.
