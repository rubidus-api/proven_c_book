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

#demo("examples-en/ch15/hello.c", show-output: false)

This is a kind of work plan written in the grammar of the C language, and it is
called *source code*. It becomes an *executable file* we can run only after a
translator called a *compiler* turns it into machine code and joins it with
already-made, pre-built machine code (called a *library*). Let us look at that
process, and at what the first program actually produces.

But one more thing is needed before that. *Where and how* is the executable run
once it is made? That place is the terminal.

== The terminal and the shell — where a program is run

Chapter 10 showed "a band of characters flowing one line at a time". Today's window
onto that band is the *terminal*. Open it and you can type characters; something
answers what you typed.

Two words must be told apart here. They are used loosely as one, and they do
different jobs.

#dtable(
  columns: 3,
  [*Word*], [*What it is*], [*What it does*],
  [terminal], [the *window* that shows and takes characters], [passes keystrokes on and paints the characters that come back],
  [shell], [the *program* running inside that window], [reads the line you typed, interprets it, runs it and returns the result],
)

That is, *the terminal is the glass, the shell is the worker behind it.* "Typing a
command into the terminal" is really "typing a command to the shell through the
terminal".

=== Which windows and which workers

#dtable(
  columns: 3,
  [], [*Terminal (window)*], [*Shell (worker)*],
  [Windows], [Windows Terminal, the old console window], [`cmd.exe` (Command Prompt), PowerShell (`powershell` / `pwsh`)],
  [Linux, Unix], [GNOME Terminal, Konsole, xterm and others], [`bash`, `zsh`, `dash`, `fish` and others],
  [macOS], [Terminal.app, iTerm2], [`zsh` (the default), `bash`],
)

The window may be chosen by taste and the result is the same. But *a different
worker means a different grammar* --- the same job is written differently, which is
why, when copying a command off the internet, the first question is "whose shell is
this written for?".

How to tell which worker you are talking to also differs.

#dtable(
  columns: 2,
  [*Shell*], [*How to tell*],
  [`bash`, `zsh`], [`echo $SHELL` or `echo $0`],
  [`cmd.exe`], [the prompt looks like `C:\Users\me>`],
  [PowerShell], [the prompt looks like `PS C:\Users\me>`; `$PSVersionTable` also tells],
)

=== What a shell does — read, split, run

Given a line, a shell always takes the same three steps.

+ *Read* --- take the line up to the Enter.
+ *Split* --- cut it into words at the spaces. The first word is *what to do*; the
  rest are the *arguments*.
+ *Run* --- if the first word is something the shell knows itself, do it; otherwise
  find an *executable file* of that name and launch it.

The "something it knows itself" in that third step is a *built-in* command. It is
fixed inside the shell, with no file of its own.

#dtable(
  columns: 4,
  [*To do this*], [`bash`, `zsh`], [`cmd.exe`], [PowerShell],
  [which directory am I in], [`pwd`], [`cd` (with no argument)], [`Get-Location` (`pwd`)],
  [change directory], [`cd name`], [`cd name`], [`Set-Location` (`cd`)],
  [list files], [`ls`], [`dir`], [`Get-ChildItem` (`ls`, `dir`)],
  [print to the screen], [`echo hi`], [`echo hi`], [`Write-Output hi` (`echo`)],
  [clear the screen], [`clear`], [`cls`], [`Clear-Host` (`cls`)],
)

The names in parentheses are *aliases* --- PowerShell attached familiar names like
`ls` and `cd` to the same jobs for people arriving from Unix. Just remember that the
same name is *a different program*.

#qa[
  Why must built-ins and executables be told apart? They look the same to the user.
][
  Mostly they need not be. Three places demand it.

  *1. They are found differently.* An executable is found by the shell searching the
  *path* (`PATH`); a built-in needs no searching, being inside the shell. So `cd`
  still works when `PATH` is broken.

  *2. Some jobs can only be built-ins.* `cd` is the example. Changing directory
  changes *the shell's own state*, and a separate program could only change its own
  state and exit (exactly chapter 3's story about processes).

  *3. Names can collide.* Where a built-in and an executable share a name the
  built-in usually wins. In `bash`, `type name` says whether a name is a built-in or
  a file.
]

=== Running the executable you made

Compile the first program and one executable file appears. How it is run differs a
little by platform, and *this is where beginners get stuck most often.*

#dtable(
  columns: 2,
  [*Situation*], [*What to type*],
  [Linux, macOS — `hello` in the current directory], [`./hello`],
  [Windows `cmd.exe` — `hello.exe` in the current directory], [`hello` or `.\hello.exe`],
  [Windows PowerShell], [`.\hello.exe` (`hello` alone will not do)],
)

#misconception[
  "I typed `hello` on Linux and got command not found"
][
  Not because the file is missing. *Because the shell does not search the current
  directory.*

  A shell looks for executables only in the directories listed in `PATH`, and the
  Unix family has a long-standing habit, for safety, of *not putting the current
  directory in `PATH`* --- with it there, a nasty program planted in a directory
  under the name `ls` would run.

  So you say "run this one, here" explicitly --- the `./` of `./hello` means "the
  current directory".

  PowerShell demands `.\hello.exe` for the same reason, while `cmd.exe` looks in the
  current directory first, so `hello` alone suffices. The three shells part company
  right here.
]

=== Passing arguments — how a program is handed values

The shell cuts a line into words, we said. What follows the first word is passed to
the program, and those are its *arguments*.

```sh
gcc -Wall -o hello hello.c
```

Here `gcc` is what runs, and the four that follow are arguments. Arguments come in
conventional kinds.

#dtable(
  columns: 3,
  [*Kind*], [*Shape*], [*Example*],
  [a value], [a bare word], [`hello.c` --- the file to work on],
  [a short option], [one `-` + one letter], [`-c`, `-g`; sometimes run together --- `-Wall`],
  [an option with a value], [the next word is the value], [`-o hello` --- "the output is named hello"],
  [a long option], [two `--` + a word], [`--version`, `--help`],
  [end of options], [`--` alone], [everything after is a value --- for file names starting with `-`],
)

*An argument containing a space is wrapped in quotes*, because the shell splits
words at spaces.

```sh
gcc -o "my program" hello.c     # when the name has a space
```

These are *the shell's rules*, not C's. How a C program receives those arguments ---
`main`'s `argc` and `argv` --- is treated properly in chapter 52.

#platform[
  Windows and Unix split arguments differently
][
  The Unix family has *the shell* cut the words and pass an array. Windows passes
  *one string* and the receiving runtime splits it (chapter 3). So quotes and
  backslashes are handled subtly differently, and a command line that worked on Linux
  can be cut differently on Windows.

  One thing to remember at this stage: *if a path contains a space, wrap it in
  quotes.* Windows' `C:\Program Files\...` is the classic place.
]

== Compiling and running from the command line

Now to turn that work plan into an executable. There are many compilers, but *the
shapes reduce to three* --- GCC, Clang and MSVC. The first two share almost all their
option syntax; only MSVC belongs to another lineage.

=== GCC (Linux, macOS, MinGW)

```sh
gcc -std=c23 -Wall -Wextra -o hello hello.c
./hello
```

Word by word: `gcc` is the program to call, `-std=c23` says "read this as C23",
`-Wall -Wextra` says "tell me everything suspicious" (chapter 17 covers why these
two are always on), `-o hello` says "name the result `hello`", and the final
`hello.c` is the material.

Drop the `-o` and the result comes out named `a.out` --- a default handed down from
early Unix, short for "assembler output".

=== Clang (the macOS default; Linux and Windows too)

```sh
clang -std=c23 -Wall -Wextra -o hello hello.c
./hello
```

*The options are GCC's.* It was built that way deliberately (chapter 18), so moving
between the two rarely means editing a command. It is also why this book can
cross-check its listings with both.

On macOS, typing `gcc` often calls Clang --- `gcc --version` reveals the truth.

=== MSVC (Windows)

MSVC differs from the option syntax up. And it *must be run in its own window.*

```bat
cl /std:c17 /W4 hello.c
hello.exe
```

#dtable(
  columns: 3,
  [*To do this*], [*GCC, Clang*], [*MSVC*],
  [mark an option], [one `-`], [`/` or `-` (`/W4`)],
  [choose the standard], [`-std=c23`], [`/std:c17`, `/std:c11`, `/std:clatest`],
  [turn warnings on], [`-Wall -Wextra`], [`/W4` (`/Wall` is far too noisy)],
  [name the output], [`-o hello`], [`/Fe:hello.exe`],
  [compile only], [`-c`], [`/c`],
)

The *own window* is needed because MSVC leans on several environment variables (the
paths to headers and libraries). The Start menu's "x64 Native Tools Command Prompt
for VS" opens a `cmd` with those set. Type `cl` in an ordinary `cmd` and you get
"not recognized as an internal or external command" --- not because the file is
missing but because *`PATH` was not prepared*, exactly the previous section's story.

#qa[
  Why is there no `/std:c23` for MSVC?
][
  Because, as this is written, MSVC's C support reaches `c11`, `c17` and `clatest`.
  Microsoft long treated C as an appendage of C++ and was late even to C99
  (chapter 18); it is catching up now but still trails GCC and Clang.

  So to run this book's listings on Windows, one of three is realistic ---
  *MinGW-w64's GCC*, *Clang for Windows*, or *GCC inside WSL*. To insist on MSVC,
  pass `/std:clatest` and be ready for the places where C23 features (`nullptr`,
  `constexpr` and the like) catch.
]

#platform[
  The three compilers in one line each
][
  #dtable(
    columns: 4,
    [], [*GCC*], [*Clang*], [*MSVC*],
    [where], [the Linux default; Windows via MinGW], [the macOS default; Linux and Windows], [Windows only],
    [option lineage], [Unix-style `-`], [same as GCC], [the `/` lineage],
    [C23 support], [broad], [broad], [catching up],
    [running it], [`./hello`], [`./hello`], [`hello.exe`],
  )

  All three aim at the same standard, so *this book's listings should give the same
  answer whichever builds them* --- and where they do not, that place is exactly what
  this book calls implementation-defined, or a grey area (chapter 12). In practice
  this book builds with GCC and cross-checks with Clang.
]

== And so the first program comes out like this

Here is the result of the two lines just typed --- compile, then run.

#demo-output("examples-en/ch15/hello.c")

As with every result in this book, it is not decoration but *what the machine really
produced* --- every build compiles and runs the listing and puts its output here.

== Reading it line by line

Having seen it run, we now read what each of those six lines says.

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
that story is in chapter 52. The meanings of the
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
