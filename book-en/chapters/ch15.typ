#import "../../book/lib.typ": *

= Hello world

#prereq(
  ([chapter 10, The origin of streams], [streams — output is something poured out]),
)

#deepqa[
  Chapter 10 said a computer's input and output is "a band of characters flowing
  a line at a time" (a stream), and that every program is given the flow called
  standard output by default. So what exactly is a program doing when it puts
  letters on the screen?
][
  It is letting letters flow onto the band called standard output. The screen is
  merely today's "paper" attached to the end of that band (remember that it is a
  descendant of the glass Teletype). What the first program does is exactly this
  — let one line of greeting flow onto standard output.
]

#organizer[
  At last, the first program. We run one six-line C program on the page and
  read it aloud, line by line. It is normal not to understand all of it yet —
  what counts as "on credit" and when the debt is paid is settled here.
]

#chapter-questions()

== The first program

By tradition, the first program greets the world.

#demo("examples-en/ch15/hello.c")

The upper box is the *source code* a person writes, and the lower box marked
"Output" is the *output* of actually compiling and running this program — as
with every result in this book, not decoration but what the machine really
produced.

== Reading it line by line

*`#include <stdio.h>`* — a declaration of what we need. It means "fetch the
standard input/output toolbox", and the `printf` used below lives in that box.
The name `stdio` takes the bold letters from #strong[st]andard
#strong[i]nput/#strong[o]utput — under the file-name length limits of early C
such compressed names became the practice, which is why most standard header
names are short.

The precise identity of lines beginning with `#` is revealed in the next
chapter (preprocessing).

*`int main(void)`* — the agreed starting point. A C program that runs on top of
an operating system, as the one we are making does (a hosted implementation),
starts at the function named `main`. To run the program is to run `main`. Where
there is no operating system, even the name of the starting point can differ —
that story is in chapter 47. The meanings of the
parentheses, of `int` and of `void` are unravelled in turn in Parts IV and V.

*`{` and `}`* — the fences of beginning and end. Inside the fence is the list of
things `main` will do.

*`printf("Hello, world!\n");`* — the substance of this program. It calls the
tool named `printf` and lets the letters inside the quotation marks flow onto
the band of standard output. `\n` is the very newline character we met in
chapter 10 — the typewriter's legacy is already in the first program. The
semicolon at the end is the full stop of a statement.

*`return 0;`* — the parting bow. It ends `main` and reports 0 to the operating
system, meaning "finished without trouble."

#qa[
  I do not understand half of it. Why is `int` there, what is `(void)`, and why
  of all things does 0 mean "without trouble"?
][
  It is correct not to know yet — and this book manages that "no need to know"
  honestly. There are only two things to take away from these six lines right
  now: that the host program we are making starts at `main`, and that `printf` lets letters flow onto
  standard output. All the rest is *deliberate credit* — Part IV takes as its
  goal the dissection of this one piece from beginning to end, and the debt is
  repaid for statements and blocks in chapter 19, for `printf` and calls in
  chapters 21–22, and for `int` and `(void)` in chapters 23–25. Not explaining
  everything now is not laziness but order.
]

#qa[
  Why "Hello, world" of all greetings? Who decided it?
][
  Tradition — and it has a lineage. This greeting was born beside C's fathers.
  Brian Kernighan used "hello, world" as the first example in a language
  tutorial at Bell Labs in the early 1970s, and when the 1978 K&R book (that
  "de facto standard" we meet in chapter 12) carried it as the first program it
  became a worldwide tradition. Since then the first example of almost every
  language imitates this greeting. It has become idiom enough that on meeting a
  new language or a new environment people say "start by getting hello world
  up" — and we have just joined that tradition.
]

== But what happened between source and execution?

We have just seen two objects: the *source code* a human reads and writes
(`hello.c`), and the result the machine produced. But as chapter 4 taught, what
a machine can execute is machine instructions only, and source code is just a
text file — in the terms of chapter 9, a sequence of letters held in UTF-8. So
somewhere in between there was a large event in which text became machine
instructions.

The name of that event is *compilation*, and the next chapter tours the scene
stage by stage. Seen from outside it is a single command; inside, a relay of
four stages turns — in one of which our "order" `#include` is filled, and in the
last of which it is revealed where the substance of `printf` comes from.
