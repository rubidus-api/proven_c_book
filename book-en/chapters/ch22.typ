#import "../../book/lib.typ": *

= Output

#organizer[
#idx("format string")  `printf` faced head on — the frame called a format
  string, the blanks `%d` and `%s`, and matching the format against the
  materials. By the end of this chapter Part IV's goal is achieved: one piece of
  hello world becomes, apart from two lines still on credit, all known
  sentences.
]

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
