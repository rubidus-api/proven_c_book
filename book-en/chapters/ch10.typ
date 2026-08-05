#import "../../book/lib.typ": *

= The origin of streams - punched cards, line printers, printing terminals

#organizer[
  You will learn why a computer's input and output has the shape of "characters
  flowing one line at a time." The answer lies in the age of paper — punched
  cards, line printers, and the typewriter-like printing terminal. The origin of
  the two characters `\n` and of the odd name `tty` unravels here too. By the
  end of this chapter you will understand in advance what the `printf` of your
  first program is talking to.
]

#deepqa[
  Chapter 7 said that the control characters — instructions to a device, such as
  "advance the paper one line" — live in a corner of the character table. Why
  are device commands mixed into a table of letters?
][
  Because in the age that table was made, text *was* the movement of a device.
  A letter was not drawn on a screen but physically struck onto paper, and
  "next line" was an actual motion of mechanical parts. Look at the devices of
  that era and everything about the shape of today's input and output unravels.
]

== Punched cards — one card is one line

The input of early computing was the *punched card*. Holes punched in a stiff
paper card record letters — a combination of holes is one letter, and one card
holds *80 columns* of them. One line of a program is punched onto one card, and
the whole program becomes a deck of cards. Feed the deck to the machine and the
cards are drawn in and read one at a time.

Two legacies were born here. First, *the sense that "one card = one line."* The
notion that text is a sequence of lines, now as obvious as air, came from a
physical object. Second, *the number 80*. The card's 80 columns became the 80
columns of later terminal screens and survived half a century into today's
coding convention that "a line of code stays within 80 characters."

#qa[
  What happens if you drop the deck?
][
  Catastrophe — several hundred cards out of order is a scrambled program. So
  there was a trick of punching sequence numbers in a corner of the card and
  re-sorting them by machine. Cutting one corner of the deck diagonally, so a
  reversed card could be spotted by eye, was the same wisdom. It sounds like a
  joke, but the idea that "data with sequence numbers can be recovered after
  being scrambled" is still a fundamental of networking and database design.
]

== Line printers — output by the line

The physical object on the output side was the *line printer*. As the name
says, a printer that strikes *one whole line at a time* onto paper. Results
came out not on a screen but line after line on continuous paper. Input one
card (= one line) at a time, output one printer line at a time — a computer's
input and output was *a flow of lines* from birth.

== Printing terminals — a conversation on paper, and tty

With the age of time-sharing (many people using one computer at once) came the
need for a device in which human and computer could *converse*. The thing that
took that place was the telegraphic typewriter — the *Teletype*. Strike a key
on this typewriter-shaped machine and the letter goes to the computer; the
computer's reply is printed on the same machine's paper. There is no screen.
The entire conversation is printed on a roll of paper.

Unix was born beside exactly these devices (chapter 4), and took the name for a
terminal device from the abbreviation of Teletype — *tty*. The name is still
alive at the bottom of today's terminal windows.

There is another legacy from the typewriter's body. Changing lines on a
typewriter is really two motions — *returning* the type carriage to the left
end, and *advancing* the paper one line. The character table assigned a control
character to each: carriage return (CR, written `\r` in C) and line feed (LF,
`\n`). Changing one line needed two instructions because two mechanical parts
actually moved.

#realcase[
  CRLF — the hundred-year homework a typewriter left behind
][
  The two characters remained after the physical reason vanished, and the world
  never unified its line-ending notation. The Windows family writes both motions,
  `\r\n` (CRLF); the Unix family takes the single `\n` as the end of a line. The
  result is the everyday accidents of today — open a text file made on one side
  on the other and ghost characters appear at line ends or the whole file looks
  like one line, and version-control tools (git) print warnings about "CRLF will
  be replaced by LF." The motion of typewriter parts is still raising warnings at
  collaboration sites half a century after the parts disappeared.

  And these two characters show their face in security too. Internet protocols
  (HTTP, email) are text protocols that *separate fields by line breaks* — so if
  CR and LF get mixed into a value supplied by a user, an attacker can *create
  and insert a new field*. Put someone else's value straight into a response
  header and the headers can be wholly manipulated (HTTP response splitting);
  put it into a log line and fake log lines can be forged (log forging).
  Chapter 7's refrain repeats here — *do not mix data into the frame*, and
  filter control characters at the boundary.
]

== Screen terminals, and streams as the legacy

In time paper became screen — but the new screen devices were built to
*imitate* printing terminals. Today's terminal window, where letters are pushed
up from the bottom, is the glass version of the Teletype whose paper was pushed
upward. Only the appearance changed; the grammar of the conversation stayed.

This whole physical lineage condensed into a single notion. Input and output is
#idx("stream")*a band of characters flowing past in order, a line at a time* —
this is the *stream*. Input flows in as cards were drawn in one at a time;
output flows out as a printer struck one line at a time. C took this notion into
the language as its input/output model. The two flows given by default to every
program are *standard input* and *standard output*, and the `printf` we meet in
the next part of this book is the function that lets letters flow onto the band
called standard output.

== Swapping the band — redirection and pipelines

The notion of a stream hid an unexpected power. If a program *does not know
where the end of its output is* — then that end can be swapped from outside.

Unix turned this idea into an institution. A program merely reads and writes on
the bands called standard input and standard output, and what the other side of
those bands connects to is decided by *whoever runs the program*. Type
`> file.txt` after a command in the terminal and the output band leads to a file
#idx("redirection")instead of the screen (*redirection*); type `< input.txt`
and the input band flows from a file instead of the keyboard. Not one letter of
the program's code changes.

One step further is the *pipeline*: joining one program's output band directly
to another program's input band — the terminal's `|` symbol is that connection.

```text
$ list | filter | count
```

Three programs each work looking only at their own band, and joining the three
makes a new tool. The principle left behind by Doug McIlroy, who invented this
connection, and the Unix people became a maxim of software design ever after —
*"Write small programs that do one thing well, and have them cooperate through
text streams."* The grammar of assembling programs as components came out of
just this simplicity of swapping bands.

The price of that freedom is something the programmer must observe. You must
not assume your program's output always goes to a human's screen — decorative
characters or progress indicators can contaminate the input of the next program.
So the Unix tradition provides one more channel: the real results go to standard
output, and what is said to a human (warnings, errors) goes to a separate band
called *standard error*. That is why a third band exists.

#realcase[
  This book's printed output is itself a product of redirection
][
  This is not an abstract story — the book you are reading is the proof. Every
  code demonstration in this book is real output obtained by a machine running
  the example, and that output was not transcribed by a person but obtained by
  *turning the output band into a file* (redirection), which the typesetting tool
  then read and printed. Examples that need input likewise have a file joined to
  the standard input band — which is why the examples reproduce with no one
  striking a keyboard. When chapter 26 breaks the misconception that "input comes
  from the keyboard", the living proof is the making of this book.
]

#qa[
  Programs these days communicate with windows and buttons — is the stream not
  an old story?
][
  Looking only at the front of the screen it seems so, but round the back the
  stream is at its peak. The records (logs) of server programs are streams, so
  are the pipelines joining program to program, and so is an AI chatbot "flowing"
  its answer one character at a time. Above all, in the setting of learning to
  program, the stream is the simplest and most honest channel of communication —
  which is why every example in this book runs on top of one.
]

The ladder of representation is complete. How to put numbers in the lockers
(chapter 8), how to put letters in (chapter 9), and how to let letters flow
(chapter 10) — we can now put anything into the machine and get it out.

From the next chapter comes this part's final climb. How bold a lie the simple
picture of the machine has been so far — memory splitting apart (chapter 11),
execution overlapping (chapter 12), the compiler stepping in (chapter 13), and
therefore why C cannot help being "an abstract language" (chapter 14).
