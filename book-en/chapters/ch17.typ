#import "../../book/lib.typ": *

= Setting up a development environment

#prereq(
  ([chapter 16, The general shape of compilation], [the four stages: preprocess, compile, assemble, link]),
)

#deepqa[
  Chapter 16 worked at the command line, asking the compiler for things like
  `-E` and `-S`. These days everything can be done by clicking — so why the
  command line?
][
  Because it is the general account. Graphical development tools each look
  different and change every few years, but the sentence
  `cc hello.c -o hello` has worked on every platform for half a century. And
  whatever graphical tool you use, what actually runs underneath its "build"
  button is exactly these commands — someone who knows the bottom does not get
  lost when switching tools. That is why this book takes the command line as its
  reference.
]

#organizer[
  We equip the tools to run chapter 16's relay on our own computer. The general
  account, true on any platform, is the main text; concrete installation steps
  tied to one operating system are isolated in "platform note" boxes — the boxes
  use Windows as this book's example platform, and the text continues if you
#idx("debugger")  skip them. After the compiler and the editor come the
  debugger, which stops a program to look inside, its blind spots, and the net
#idx("sanitizer")  that catches bugs while the program runs (sanitizers).
]

#chapter-questions()

== Only three things are needed

"Development environment" sounds grand, but C programming needs only three
things.

- A *compiler* — chapter 16's four runners in one. This book takes the gcc and
  clang families as its reference (both are free and exist on every major
  platform).
- A *text editor* — source code is just a text file (chapter 9), so anything
  that can write characters will do. Which editor you use is a matter of taste,
  and this book does not decide it.
- A *terminal* — the window in which you type commands. Every operating system
  has one.

The shape of the working day is simple too — *edit, compile, run.* That
three-beat cycle is the basic rhythm of programming, and every example in this
book turns on it (readers may of course simply read on the page — as chapter 1
promised, even this chapter's installation is a demonstration, not homework).

#platform[
  Installing a compiler on Windows
][
  This book's example platform is Windows. Pick one of two routes (or both) —
  this book's examples build identically either way.

  *Route 1 — LLVM clang (recommended).* Install the LLVM project's official
  Windows distribution. One line in the terminal (PowerShell) is enough:

  ```text
  > winget install LLVM.LLVM
  ```

  (Downloading the installer from llvm.org's releases page is the same thing.)
  If `clang --version` answers with a version number in a new terminal, you are
  ready. From then on, write `clang` where this book writes `cc`:

  ```text
  > clang hello.c -o hello.exe
  > .\hello.exe
  Hello, world!
  ```

  *Route 2 — MinGW-w64 gcc.* The Windows port of gcc. Install the package
  environment MSYS2 (msys2.org), then in its terminal:

  ```text
  $ pacman -S mingw-w64-ucrt-x86_64-gcc
  $ gcc --version
  ```

  *A third road — about Visual Studio (MSVC).* Microsoft's own compiler and
  integrated environment is an excellent choice too, but its command and option
  spellings differ from this book's (the gcc/clang family), so it is not covered
  here. Interested readers can follow Microsoft's official documentation
  (#link("https://learn.microsoft.com/cpp")[`learn.microsoft.com/cpp`]), which guides from installation to the first
  program in detail.
]

#qa[
  Is there a reason for having two families of compiler? Life would be less
  confusing with one.
][
  Because competition is quality — and because for a programmer it is a free
  verification tool. Compile the same code with both gcc and clang and it is
  common for one to catch a warning the other missed. Code that two independent
  implementations both accept is far more likely to be written to the standard
  (the contract) — in chapter 12's terms, cross-evidence that you are speaking
  the standard language rather than a dialect. The examples printed in this book
  are cross-checked with both compilers.
]

== If installing is too much — try it straight in a browser

Getting the three things above is the proper way, but *when you want to run one
line right now*, a browser is enough. Several places compile and run code with no
installation, and some of them show what this book could only describe on paper —
the optimised machine code, and where a pointer points.

#dtable(
  columns: 3,
  [*What*], [*Character*], [*Where it helps in this book*],
  [Compiler Explorer (#link("https://godbolt.org")[`godbolt.org`])], [Source and *assembly* side by side. Over a thousand C compiler builds], [Chapter 13 (optimisation), chapter 16 (compilation), chapter 46 (operators)],
  [OnlineGDB], [Editing, running and a *debugger* in the browser], [Following this chapter's debugger section without installing],
  [Wandbox, Coliru], [Running the same code under several compiler builds], [Spotting "works only on my compiler"],
  [Replit], [A workspace with files and a terminal], [Examples split across files (chapter 51)],
  [Python Tutor (C mode)], [Steps through execution and *draws memory*], [Chapters 34–42, pointers and memory],
  [#link("https://cdecl.org")[`cdecl.org`]], [Puts a complicated declaration into words], [Chapter 57, reading declarations],
)

#figure-svg("explorer", caption: [How a screen that shows one source line becoming several machine-code lines is laid out.])

=== Study options — the same on the web

Whichever tool you use, the knack is *not to leave the options box empty*. The
warning options of the next section work here too.

```text
-std=c23 -Wall -Wextra -Werror -g -O0
-fsanitize=address,undefined        <- only where the site supports it
```

- Compiler Explorer takes them in the *Compiler options* box next to the compiler
  name.
- OnlineGDB takes them under the gear icon, in *Extra Compiler Flags*.
- Sites often default to an old compiler. *If `-std=c23` is refused, the compiler
  is old* — pick a newer one from the list (some middle-generation builds accept
  `-std=c2x`).

#figure-svg("viz", caption: [Values as boxes, pointers as arrows — the picture a visualiser draws.])

Visualising memory pays off most in Part VII (pointers and memory). What this
book explained in pictures and words, you can move around yourself in code you
wrote.

#platform[
  The limits of online tools — easier once you know them
][
  The browser's place is as narrow as it is convenient. Knowing the limits saves
  wasted effort.

  - *There are time and memory limits.* Long loops and large arrays get cut off.
  - *Files and networking are usually blocked.* Run the file-I/O examples
    (chapter 60 onwards) on your own machine.
  - *Standard input goes in a box you fill in advance.* Interactive input is only
    imitated.
  - *It is someone else's server.* Do not paste company code or anything that must
    not be public. A Compiler Explorer share link puts that code into a URL.
  - *Compiler builds and libraries differ per site.* Half of "it works here but
    not there" is a version difference (chapter 16).

  So this book's advice: *the web while you are learning, your own machine once
  you start building something.* Getting the three things above can be postponed,
  but not skipped.

]

Places that hand you problems, material that can be read for free, and a knack for
spotting out-of-date writing are collected in *Appendix D*. Since this book has no
exercises (see the preface), the hands-on part carries on from there.

== Warnings — leave the free review switched on

There is one habit to adopt immediately after installation: asking the compiler
for generous *warnings*.

```text
$ cc -Wall -Wextra hello.c -o hello
```

#idx("warning options")`-Wall -Wextra` means "tell me about anything
suspicious" (despite the name, `-Wall` is not all of them, so the two are
conventionally used together). A warning is not an error but *a free code review
by the compiler* — it points at patterns that invite accidents even where the
contract (the standard) is not violated. Every example in this book is verified
with these two options on and zero warnings.

== When Korean text breaks — the chain of encodings

Here we note in advance a problem Korean-speaking readers almost always meet.
It is also why this book's examples print mostly English strings — the moment
you print Hangul, it is not one program but *four layers of encoding settings
that must all agree* for the letters to come out right.

The four links in the chain:

- *① the source file's encoding* — source is, in the end, a byte sequence
  (chapter 9). Whether the editor saved it as UTF-8 or as CP949 (the EUC-KR
  family, the old default on Korean Windows) makes the same "안녕" different
  bytes.
- *② the encoding the compiler reads it as* — the compiler *guesses, or assumes
  a default*. If the source is UTF-8 and the compiler reads CP949, the bytes of
  string literals are mangled right there.
- *③ the execution character set* — what bytes the compiler embeds in the
  executable (stage 5 of chapter 9; the conversion seen in the translation
  phases of chapter 54).
- *④ the encoding the terminal interprets* — which table the window reads the
  program's bytes with. Get this wrong and the screen shows broken characters
  even if the first three are perfect.

One broken link out of four produces the same symptom — "the letters are
broken" — which makes the cause unusually hard for a beginner to pin down. The
knack of diagnosis is *narrowing down which link broke*: open the source in hex
(separating ① from ②), redirect the output to a file (separating ④), and the
culprit appears.

=== Nailing ② and ③ down for the compiler

As chapter 9 showed, C treats the *source character set* and the *execution
character set* as different things, and both are settled by the implementation.
There are options that let a person state what "the implementation settles".

#dtable(
  columns: 3,
  [*What it settles*], [*GCC, Clang*], [*MSVC*],
  [② the encoding the compiler reads the source in], [`-finput-charset=…`], [`/source-charset:…`],
  [③ the execution character set (the literal encoding)], [`-fexec-charset=…`], [`/execution-charset:…`],
  [③ the wide literal encoding], [`-fwide-exec-charset=…`], [(no separate option)],
  [②③ both to UTF-8 at once], [already the default], [`/utf-8`],
)

Knowing the defaults prevents accidents. GCC and Clang default to *UTF-8 on both
sides*; MSVC defaults to *the current system code page* (CP949 on a Korean Windows).
That single `/utf-8` removes half the Windows-side trouble for this reason.

#demo("examples/ch17/charset.c")

The listing was run *with the defaults*. Rebuild the same source with different
options and the first line changes --- measured, it runs like this.

#dtable(
  columns: 4,
  [*Source file encoding*], [`-finput-charset`], [`-fexec-charset`], [*the bytes of the literal*],
  [UTF-8], [(default UTF-8)], [(default UTF-8)], [`EA B0 80`],
  [UTF-8], [(default)], [`EUC-KR`], [`B0 A1`],
  [EUC-KR], [`EUC-KR`], [(default UTF-8)], [`EA B0 80`],
  [EUC-KR], [*not given*], [(default)], [`B0 A1` — the bytes simply passed through],
)

Two things can be read out of it.

*First, the same source file becomes a different program.* Rows three and four are
*byte-identical files* with different results. What separated them was whether the
compiler was told "this file is EUC-KR".

*Second, telling it wrongly usually raises no error.* In row four the compiler
believed the file was UTF-8, said nothing, and put the bytes into the executable as
they were. The screen may even look fine by accident --- if the terminal is CP949.
*All four links wrong and the result looking right* is the nastiest state of all,
because it breaks the moment one machine or one person changes.

#qa[
  When, then, is `-fexec-charset` used?
][
  Almost never is the answer. For a program written today the right move is
  *everything in UTF-8*, and then these options are unnecessary (that is already the
  GCC and Clang default).

  There are only two places you reach for them. *Reviving old code* --- when an old
  project stored in CP949 must be built without touching it, `-finput-charset=CP949`
  tells the compiler the truth. And *a counterpart you cannot change* --- if output
  must go to old equipment that accepts only EUC-KR, match the execution character
  set to it.

  Both are *places where an exception is being declared*, so follow chapter 12's
  ladder and write the reason in the build file. One option changes every byte of
  the program; if that is recorded nowhere, the next person spends a day finding it.

  And where the bytes must not move, write `u8"…"` from the start (chapter 9) --- the
  standard guarantees UTF-8 whatever the options. Measured: build the listing with
  `-fexec-charset=EUC-KR` and `u8"가"` alone stayed `EA B0 80`.
]

#realcase[
  `__STDC_ISO_10646__` does not only tell the truth
][
  The standard says that if some other encoding is used, this macro *shall not* be
  defined (§6.10.10.3). Yet build with `-fwide-exec-charset=EUC-KR`, making the wide
  literal encoding something other than Unicode, and --- while `L"가"[0]` becomes
  `U+A1B0` (EUC-KR bytes) --- `__STDC_ISO_10646__` was still defined, as `201706L`.

  The lesson is not to distrust macros but that *a feature-test macro is "what the
  implementation said", which need not be "what is true in this particular build"* ---
  especially in a build given options away from the defaults. In the language of
  chapter 12's ladder, this is a place to confirm with a *test* rather than lean on
  a macro alone.
]

#platform[
  When Hangul breaks on Windows
][
  Windows is the main battleground for this problem. The default code page of
  Korean Windows was CP949 for a long time, and as the UTF-8 world was laid on
  top, settings diverged link by link. Practical prescriptions, per tool:

  *Common — save the source as UTF-8.* Save as "UTF-8" in your editor. For
  compatibility with Windows tools, *UTF-8 with a BOM* (UTF-8 with signature) is
  sometimes recommended — MSVC reads it as UTF-8 with confidence when a BOM is
  present. gcc and clang read BOM-less UTF-8 well by default.

  *Visual Studio (MSVC)* — the surest route is to save the file as UTF-8 (with
  BOM) via "Save with Encoding", or to give the compile option `/utf-8`, pinning
  both the source and the execution character set to UTF-8. Details are in
  Microsoft's official documentation.

  *VS Code* — the current file's encoding appears in the status bar at bottom
  right. Change it there with "Save with Encoding → UTF-8", and set
  `"files.encoding": "utf8"` as the default to pin ①. Automatic detection
  (`files.autoGuessEncoding`) looks convenient, but a wrong guess can damage the
  file, so in teamwork it is safer to turn it off and be explicit.

  *The terminal (④)* — the Windows console interprets bytes with the current
  code page. `chcp 65001` switches to the UTF-8 code page and UTF-8 output shows
  correctly. Windows Terminal handles UTF-8 close to by default and has fewer
  accidents than the old console window. Conversely, running a UTF-8 program in
  a console set to CP949 shows broken jamo — the program is not wrong; *the
  table doing the reading is different* (chapter 9's story reproduced on screen).

  *MinGW/MSYS2 and WSL* — these are mostly UTF-8 by default and have fewer
  accidents. Still, a MinGW-built program run in an old Windows console can have
  ④ out of step, just the same.
]

#qa[
  So is the answer to avoid printing Hangul in examples altogether?
][
  At the learning stage it helps — and this book made that choice. If you are
  trying to learn about pointers and get stuck on encoding settings, you are
  taken away from what you meant to learn. But *avoidance is not a solution.*
  Real programs end up handling Hangul, and then the straightforward method is
  to pin each link of the chain — UTF-8 source, UTF-8 stated to the compiler,
  UTF-8 terminal. Pin those three and it stays quiet thereafter. The rules for
  *handling* strings (character count ≠ byte count, do not cut at a boundary)
  are as already learned in chapters 9 and 39.
]

== The debugger — stopping to look inside

When a program gives a wrong answer, the first thing most people do is plant a
`printf`. That is not a bad method — it works anywhere and needs no
preparation. But you have to know *in advance* where to print, you must
recompile for each print, and what you get is only that variable at that moment.

The *debugger* removes those constraints. It stops the program at a point of
your choosing, lets you look at every variable at that instant, walk one line at
a time, and trace back the chain of calls that got you here. The standard tools
are GNU's `gdb` and LLVM's `lldb`; only the command names differ.

#dtable(
  columns: 3,
  [*what you want*], [*gdb command*], [*meaning*],
  [stop here], [`break sum_all`], [breakpoint],
  [start], [`run`], [run the program],
  [what is this value], [`print n`], [evaluate a variable or expression],
  [advance one line], [`next` / `step`], [step over / step into],
  [how did I get here], [`backtrace`], [the call stack],
  [stop when this changes], [`watch s`], [watchpoint],
  [until this function returns], [`finish`], [shows the return value too],
)

The program we will use looks like this. The sum should be 150, but it comes out
100.

#demo("examples-en/ch17/bug.c")

Caught in the debugger it looks like this (really run with gdb 17.2).

```text
$ gcc -std=c23 -g -O0 bug.c -o bug
$ gdb -q ./bug
(gdb) break sum_all
Breakpoint 1 at 0x1154: file bug.c, line 4.
(gdb) run
Breakpoint 1, sum_all (a=0x7fff5302c8a0, n=5) at bug.c:4
4	    int s = 0;
(gdb) print n
$1 = 5
(gdb) next
5	    for (int i = 0; i < n - 1; i++)   /* bug: stops one short */
(gdb) next
6	        s += a[i];
(gdb) print a[4]
$3 = 50
(gdb) finish
Run till exit from sum_all (a=..., n=5) at bug.c:4
Value returned is $4 = 100
```

`n` arrived correctly as 5, the last slot of the array `a[4]` holds 50 intact,
and yet the return value is 100. The materials are right and the calculation is
wrong, so the only place left is the boundary of the loop — `i < n - 1`. That
process of narrowing down is the debugger's usefulness.

== Debug-build options — `-g` and `-O0` are different switches

For the debugger to know variable names and line numbers, the compiler has to
put that information into the executable. That is `-g`.

- `-g` — embeds *debug info*. Variable names, types, line numbers and function
  boundaries ride along inside the executable.
- `-O0` — turns optimisation off. Source statements correspond almost one to one
  with machine instructions, so stepping and inspecting variables work exactly.
- `-Og` — optimises "only as much as remains good for debugging". Faster than
  `-O0` and far more visible than `-O2`. A good default for debugging.
- `-g3` — includes macro definitions too. Things like `print MAX_LEN` become
  possible.
- `-fno-omit-frame-pointer` — leaves the thread for tracing back the call chain.
  It also helps profilers and crash reports.

There is one switch related to `assert` (chapter 48). Give `-DNDEBUG` and every
`assert` disappears entirely — release builds are usually made that way. Erasing
the checks means *what was caught in the debug build passes silently in
release*, so a check that must not disappear should generally be written as an
`if`, not an `assert`.

#misconception[
  "Adding `-g` makes the program slower"
][
  Plausible, but no. `-g` merely *attaches information*; it does not change the
  machine instructions produced — the executable grows, the speed is the same.
  What slows things down is the `-O0` used alongside. They are different
  switches and can be used together, as in `-O2 -g`. In practice the standard
  approach is to build even release with `-g` and keep the debug info in a
  separate file — the shipped executable stays light while crash reports from
  the field can still be decoded with that information. Throw the information
  away and from that moment the accident scene is a list of address numbers.
]

== The debugger's blind spot — optimised builds

The trouble is the release build. Build the same program with `-O2 -g` and catch
it, and this happens (again, a real capture).

```text
$ gcc -std=c23 -g -O2 bug.c -o bug
$ gdb -q ./bug
(gdb) break sum_all
Function "sum_all" not defined.
(gdb) break main
Breakpoint 1 at 0x1040: file bug.c, line 12.
(gdb) run
Breakpoint 1, main () at bug.c:12
12	    printf("sum = %d (should be 150)\n", sum_all(data, 5));
(gdb) info locals
data = <optimised out>
```

The function we were looking for *is not there*. The compiler spread `sum_all`
out at the call site (inlining) and computed the sum of a constant array at
compile time, so no such function remains in the executable. The array `data`
likewise "was optimised out". The editor's rights, learned in chapter 13, appear
here in the form of blocking the observer's view.

The blind spots commonly met in optimised builds:

- `<optimised out>` — the variable lives only in a register, or not at all, and
  its value cannot be seen (chapter 11's registers return here).
- *line numbers jump* — stepping goes 12 → 15 → 12. The compiler reordered the
  instructions, and the debugger is not lying but showing the real, shuffled
  order.
- *the call chain is short* — inlined functions and tail calls leave no frame on
  the stack.
- *breakpoints do not hit* — deleted code has nowhere to stop.

== "It works in debug but is wrong only in release"

The most vexing situation, and most of the causes converge on one — *the program
was already broken, and the debug build happened to be hiding it.* Optimisation
did not break sound code; code outside the contract (chapters 13 and 49) was
taken as a premise, and only then did the symptom appear. The common roots:

- *uninitialised local variables* — under `-O0` that stack slot happens to be 0
  and all goes well; change the layout and garbage arrives.
- *writes past a boundary* — what sits in the overrun place differs per build.
  What overwrote spare space in the debug build overwrites something important in
  release.
- *optimisation premised on the outside of the contract* — signed overflow does
  not happen (chapter 7), strict aliasing is respected (chapter 13), a pointer
  cannot be null after being dereferenced — such premises turn into real code
  transformations only at `-O2`.
- *a polling loop missing `volatile`* — exactly as in chapter 13: read every
  time at `-O0`, read once and frozen at `-O2`.
- *vanished `assert`s* — `-DNDEBUG` removes the checks wholesale and bad input
  flows straight through.
- *races revealed by changed timing* — in a program running on several threads
  (chapter 12) faster code reorders things. Attach a debugger, it slows down and
  the symptom disappears: the classic Heisenbug.
- *tiny differences in floating-point arithmetic* — options of the
  `-ffast-math` family assume associativity and reorder, so the last digits can
  change (chapter 8).

#realcase[
  "Our code works in debug" — the kernel surrendering with a flag
][
  Strict aliasing, seen in chapter 13, is the representative of this class.
  Hand-written parser code that looks at a byte buffer through pointers of
  different widths runs perfectly at `-O0` and silently gives a different answer
  at `-O2`. Rather than fixing the code case by case, the Linux kernel chose to
  compile the entire build with `-fno-strict-aliasing` — turning off one flag
  wholesale because of one rule. It is not to be imitated in a personal project,
  but it is the most famous demonstration of how real a "release-only bug" is.
]

== Tools for when the debugger cannot be trusted

There really are places where a debugger cannot be used or trusted — bugs that
reproduce only in optimised builds, timing bugs that vanish the moment you
attach, production environments where no debugger can be launched, and embedded
boards with neither screen nor keyboard. The order of attack is roughly this.

+ *Rebuild with `-Og -g`.* The first attempt to regain visibility without
  turning optimisation fully off. If the symptom survives here, the debugger
  becomes usable again.
+ *Turn on the sanitizers.* The tools of the next section. They catch an
  accident *at the moment it happens*, so the cause of a release-only bug is
  usually found here. Importantly, they can be layered on the same `-O2` as
  release.
+ *Test a hypothesis with a flag.* If turning on `-fno-strict-aliasing` makes
  the symptom vanish, you know the cause is on the aliasing side. But this is
  *diagnosis*, not treatment — once the cause is confirmed, fix the code.
+ *Turn warnings and static analysis up to maximum.* Beyond `-Wall -Wextra`,
  `-fanalyzer` (GCC) or `clang --analyze` catch things without running.
+ *Leave logs and core dumps.* In production these are the only window. If you
  built release with `-g` (see the misconception box above) you can open that
  dump in a debugger afterwards and trace the crash site back to source lines.
+ *Narrow by bisection.* Lower the optimisation level (`-O2` → `-O1` → `-O0`) to
  see where behaviour diverges, and halve the version-control history to find
  when the bug entered (chapter 92's `git bisect`).
+ *Make a minimal reproduction.* Reducing the problem to the smallest program
  that reproduces it often exposes the cause by itself, and turns it into
  something you can ask others about.

#platform[
  Installing a debugger, and the Windows situation
][
  *Linux and macOS* — `gdb` comes from the distribution's package manager,
  `lldb` with the LLVM package. On macOS `lldb` is the default.

  *Windows (MSYS2/MinGW)* — get it with
  `pacman -S mingw-w64-ucrt-x86_64-gdb`. Executables built with MinGW are caught
  with this `gdb`.

  *Windows (LLVM)* — `lldb` is installed alongside. How well it gets on with the
  PDB debug-info format used by the MSVC family depends on the tool combination.

  *Visual Studio* — its integrated debugger is highly polished, with watch
  windows, memory views, and even time travel debugging (WinDbg). You do the
  same things as in the table above with F5, F10 and F11, without memorising
  commands.

  Either way the concepts are the same — breakpoints, stepping, inspecting
  variables, the call chain. When the tool changes, look for those four.
]

#qa[
  So should `printf` debugging now be given up?
][
  No. They are not rivals; they are used in different places. The debugger is a
  tool for looking *deeply at one instant*, and logs are for looking *broadly
  over a long time*. For timing problems, long-running servers, embedded work
  and accidents that are hard to reproduce, logs are often the only window. The
  practical knack is to have *switchable logging* in the code from the start,
  rather than a `printf` planted and deleted — then the same window serves both
  during development and in production.
]

== Sanitizers — a net cast while the program runs

Modern C development has one more essential piece of equipment: a tool that
catches accidents not at compile time but *while running*, the *sanitizer*.

The principle: give a special option at compile time and the compiler plants
*watch code* throughout the program. Run that program and the watch code
observes memory accesses and operations, and when something goes wrong it
points at the scene *at the moment it happens* and stops the program. There are
three main players.

- *ASan* (AddressSanitizer) — in charge of memory accidents. It catches, on the
  spot, the dangers we meet in Part VII: accesses past a boundary, touching
  freed storage again.
- *UBSan* (UndefinedBehaviorSanitizer) — in charge of contract violations. When
  *undefined behaviour* such as signed overflow (chapter 7) or a shift at least
  as wide as the type (chapter 7) actually happens at run time, it says so on
  the spot. It is the protagonist of chapter 49.
- *TSan* (ThreadSanitizer) — in charge of race accidents. It catches several
  cores (chapter 12) fighting over the same data. Outside this book's scope, but
  worth knowing by name.

Using them is one compile option:

```text
$ cc -fsanitize=address,undefined -g hello.c -o hello
$ ./hello
```

(`-g` is the extra-information option that asks for the accident site to be
reported as a source line.) A program built this way runs somewhat slower but
does not pass over accidents quietly — these tools are the modern answer to
chapter 7's "overflow is silent". Later in this book (Parts VII and IX), when we
handle dangerous code, we will see the sanitizers catching accidents on the page
several times.

#platform[
  Using sanitizers on Windows
][
  Support differs by platform, so let us record the Windows situation honestly.

  - *ASan and UBSan*: *usable with route 1's clang* — the option
    `-fsanitize=address,undefined` above works as it is. Route 2's MinGW-w64 gcc
    does not support sanitizers on Windows — to use them, choose clang
    (installing both routes and using either normally, clang when checking, is a
    good arrangement).
  - *TSan*: does not support Windows at all (Linux and macOS only). If you must
    have it on Windows there is the route of WSL (the Linux environment inside
    Windows, an official Microsoft feature) — though it is not needed within this
    book's scope.

  For reference, MSVC also supports its own ASan (`/fsanitize=address`) —
  details are in the Microsoft documentation mentioned earlier.
]

#qa[
  With warnings on and sanitizers available, why do bugs still remain?
][
  Because each tool sees a different range. Warnings look at the shape of the
  code *at compile time*; sanitizers look at what actually happened *at run
  time* — which means a sanitizer knows nothing about accidents on paths never
  executed. So modern C development layers its nets: warnings (always) +
  sanitizers (test runs) + crossing two compilers (habit) + and using components
  that are hard to have accidents with in the first place. That last item is
  where chapter 40's disciplines stand — tools are nets, and good components are
  footholds you do not fall off to begin with.
]

== Closing Part III

We read the first program (chapter 15), watched the relay that turns it into an
executable (chapter 16), and equipped our desk with the tools and nets to run
that relay (chapter 17). The preparation is done.

The next chapter is this part's finish and a map: which C compilers, other than
the gcc and clang we just installed, are in active service in the world
(chapter 18). What embedded developers carry beside make and git is a wide
enough subject that it waits until the end of the book (chapter 93).

From the part after that, the real study of the language begins — not a list of
new syntax, but the goal of Part IV: *reading one piece of chapter 15's hello
world completely.*
