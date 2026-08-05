#import "../../book/lib.typ": *

= Input

#prereq(
  ([chapter 22, Output], [the two roads of output]),
  ([chapter 10, The origin of streams], [a stream runs both ways]),
)

#deepqa[
  Chapter 10 said input, like output, is "a band of characters flowing a line at
  a time", and that human input naturally comes in lines ending with Enter (line
  buffering). Then is the natural unit in which a program takes input also — the
  line?
][
  Exactly so — and that is this chapter's design principle. A person writes a
  line and presses Enter. So it follows the grain of the flow for a program to
  receive *a whole line* and then find the values it needs inside it. Lifting a
  line out of the band of characters, and interpreting the lifted line — split
  those two and each becomes simple.
]

#organizer[
  We complete chapter 22's promise — with somewhere to store a value (a
  variable), we take input. The way this book teaches from the start is exactly
  modern practice: *read a whole line, then interpret that line.* Separating
  reading from interpreting — that two-stage structure is the source of both
  convenience and safety. Part V is completed here.
]

#chapter-questions()

== Two stages — read, then interpret

The demonstration first. A program that takes one integer from standard input
and reports its square. (What was given on standard input for this run is shown
in the middle box.)

#demo("examples-en/ch25/read.c", stdin: true)

There are two new faces — one per stage.

*Stage 1, `fgets` — read a whole line.*
`fgets(line, sizeof line, stdin)` reads one line from standard input (`stdin` is
its name) and holds it in a space called `line`. The first line's
`char line[100];` is a declaration meaning "take a space of 100 character slots
and call it line" — but the formal syntax of a multi-slot space (an array)
belongs to chapter 36, so for now know it only as "a container for a line" (a
deliberate credit). The second material, `sizeof line`, is "the number of slots
in the container" — a safety device telling fgets the container's size so that
*nothing is ever put in past its edge* (formal treatment of the `sizeof`
operator is chapter 33).

*Stage 2, `sscanf` — interpret the line we fetched.*
`sscanf(line, "%d", &n)` is the partner of chapter 22's `printf` — it uses the
same format language in *the opposite direction*. Where `printf` turned values
into characters and sent them out, `sscanf` finds in the characters (the `"7"`
inside `line`) a value matching the format (`%d`) and *puts it into a variable*.
The `&` before the name is "a mark telling it the *place* of the variable to
put the value in" — the moment addresses, learned in chapter 5, first show their
face in the grammar; formal treatment is in chapter 33 (this is Part V's last
deliberate credit).

#qa[
  Why split into two stages? I hear there is also a function (`scanf`) that
  reads `%d` straight from the input.
][
  There is, and many primers teach it first — this book takes a different road
  for two reasons. First, *the aftermath of failure is clean.* Even if
  interpretation fails (something that is not a number arrives), the line is
  already in our own container, so the input band never stops half-way and gets
  tangled — read directly from the band and fail, and the leftover characters
  stay caught on the band and contaminate the next read, which is the classic
  headache of using `scanf` directly. Second, *the grain of safety is the same.*
  Reading a line is a structure in which you tell the container's size as you
  read, so overflow is blocked at the source — what accidents the old ways that
  do not state a size caused, and how this two-stage practice seals them off, is
  the story of chapter 37. Getting the safe grain into your hands from the start
  — that is this book's choice.
]

== Interpretation can fail

Unlike output, input has one essentially new problem — *you do not know what the
other side will send.* You expected `7` and `seven` may arrive. So `sscanf`'s
return value is *the number of values successfully interpreted* — in the example
above, 1 on success and 0 on failure.

As we are now, we have no syntax — branching — for checking that return value and
taking one road on success and another on failure. So the example above
discarded the return value and laid a floor for the failing case by initialising
`n` to 0 (chapter 23's habit). The fork appears in the next part (chapter 29),
and the discipline of "report failure as a value and always check it" is
established formally in chapter 45 — checking input will be that discipline's
first real exercise.

#misconception[
  "Input comes from the keyboard"
][
  Mostly it does, but that is not its essence — input comes from the *standard
  input stream* (chapter 10). In fact the demonstration just given is the proof:
  there is nobody striking a keyboard at this book's example-verification machine
  — the input `7` above was held in a file and flowed onto the band of standard
  input (redirection). Just as chapter 22's output is not screen-bound, input is
  not keyboard-bound, so the same program serves conversation with a person, file
  processing, and joining programs together, unchanged. The power of the stream
  design repeats on the input side too.
]

== Closing Part V

We have learned all of how to make names — the name of a value (chapter 23), the
name of work (chapter 24), and how to take a value from the outside world and
hold it in a name (chapter 25). Chapter 15's credit ledger is settled, and the
credit newly taken is exactly two — the line container (`char line[100]`, in
chapter 36) and the place marker (`&`, in chapter 33) — and both have their due
dates written down.

Above all, a program can now *converse*. Take in, calculate, answer — in the
next part we forge the calculation itself: facing the world of integers
(chapters 26 and 27), comparing and deciding (chapters 29 and 30), repeating
(chapter 31), and completing the meaning of functions (chapter 32). It is the
part of values and flow.
