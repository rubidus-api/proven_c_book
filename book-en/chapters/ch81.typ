#import "../../book/lib.typ": *

= C in practice — tools, projects, and territories

#prereq(
  ([chapter 50, Several files], [a project of several files]),
  ([chapter 17, Setting up a development environment], [setting up the tools]),
)

#deepqa[
  Chapter 50 said that for a multi-file program the practice is to make each file
  into an object file and then only link again. In a project with hundreds of files a
  person cannot do that work by hand — what does it instead?
][
  *A build tool.* Write down the dependencies between files and it looks at what has
  changed and makes only what needs remaking. The classic of the C world and still its
  standard is `make`, and this chapter begins with that tool.
]

#organizer[
  Before closing the book we look round the world beyond these pages. The two
  essential tools of the field (make and git), the representative projects that run
  in pure C even today and why they chose C, what C does in embedded work, and how
  C's limits are crossed in practice. It is a map of where the language this book has
  taught actually stands.
]

#chapter-questions()

== make — what is to be remade

`make`'s idea is one line — *if the product is older than its materials, make it
again.* The syntax for writing rules is that idea as it stands:

```text
hello: hello.c greet.c greet.h      # target: materials
	cc -Wall -Wextra -o hello hello.c greet.c    # how to make it (begins with a tab)
```

Call `make hello` and it compares the files' times and runs the command only when
needed. Not recompiling everything when one file has been mended — this is how
chapter 16's relay and chapter 50's division save time in practice.

make was made in 1976 and is used still. Even the eccentricities of its syntax remain
(a command line must begin with a tab — a mistake its maker himself looked back on
with "at the time there were only twelve users"), but the idea that "write down the
dependencies and only what is needed is remade" became the root of every later build
tool. Today's large projects lay tools such as CMake, Meson and Ninja on top, but what
those tools produce in the end is mostly make-like rules too.

== git — what changed and when

The second essential tool is *version control*. A tool that records every change to
the code, lets you go back at any time, and lets several people mend the same code and
merge it — today's de facto standard is `git`.

Everyday commands can be started with five.

```text
$ git init                 # make this folder a repository
$ git add hello.c          # choose the change to record
$ git commit -m "first program"   # record it with a reason
$ git log                  # see the record travelled
$ git diff                 # what has changed now
```

There is one place where git connects with this book's theme — *git itself is a
program written in pure C.* In 2005 Linus Torvalds made the first version in a few
days for the development of the Linux kernel, and it is now the most widely used
development tool in the world. That tool giving the CRLF warning seen in chapter 10 is
itself made in the language this book has taught.

== The rest of the toolbox — building, observing, checking

make and git are only the beginning. A C project in the field mostly layers several
tools, and grouping them by *what question each answers* leaves nothing to memorise.

=== ① What to make and how — building and dependencies

#dtable(
  columns: 3,
  [*tool*], [*what it does*], [*note*],
  [`make`], [looks at dependencies and remakes], [the classic and the ground],
  [`CMake`], [*generates* build files], [the de facto standard. exports to several platforms and IDEs],
  [`Ninja`], [runs generated rules very fast], [the back end of CMake and Meson],
  [`Meson`], [a readable generator], [paired with Ninja. popular in Linux projects],
  [Autotools], [the `./configure` family], [still met in old Unix projects],
  [`Bazel`, `xmake`], [large-scale, reproducible builds], [big organisations, monorepos],
  [`ccache`], [does not redo the same compilation], [the effect is keenly felt in large projects],
  [`pkg-config`], [tells the compile and link options of a library], [the Unix family],
  [`vcpkg`, `Conan`], [fetching external libraries], [C has no standard package manager],
)

The last row is C's long-standing weakness — there being no official package manager
attached to the language, how dependencies are brought in differs by project (system
packages, putting the source in whole (vendoring), submodules, tools such as vcpkg and
Conan). That this book's examples use proven by *putting it into the repository whole*
is because of that reality too.

=== ② What has gone wrong — debuggers and record-replay

Chapter 17 learned `gdb` and `lldb`. In practice there are two more families beside
them.

- *Platform debuggers* — Windows' `WinDbg` and the Visual Studio debugger. WinDbg's
  *time travel debugging* (TTD) in particular records the execution whole and lets you
  turn it *backwards* as you look.
- *Record and replay* — Linux's `rr` does the same. Record a bug once reproduced and
  that execution can be turned back identically as many times as you like, which is
  especially strong against bugs that "reproduce only sometimes". It is the frontal
  method against the heisenbug seen in chapter 17.
- *Post-mortem crash analysis* — open a core dump (Linux) or a minidump (Windows) in a
  debugger and retrace the scene. The practice of building release builds with `-g` too
  and keeping the debug information separately (chapter 17) pays here.

=== ③ Where is it slow — profilers

"Do not guess, measure" is the first rule of performance work.

#dtable(
  columns: 3,
  [*tool*], [*what it measures*], [*where*],
  [`perf`], [CPU time, cache misses and branch failures by sampling], [Linux. effectively the default],
  [Valgrind `callgrind`], [counts instructions exactly (slow but precise)], [Linux, macOS],
  [Valgrind `cachegrind`], [simulates cache behaviour], [chapter 11's ladder before your eyes],
  [`massif`, `heaptrack`], [heap usage over time], [leaks and over-allocation (chapter 41)],
  [Intel VTune, AMD uProf], [microarchitecture-level analysis], [vendor tools],
  [the Visual Studio profiler, WPA], [CPU, memory, ETW tracing], [Windows],
  [`uftrace`, `Tracy`], [function tracing, per-frame visualisation], [games, real-time code],
)

Use a profiler and the stories of chapters 11 and 12 are confirmed in numbers — loops
with many cache misses, conditionals where branch prediction misses, and where
expensive allocations (chapter 41) are crowded, all become visible as they are.

=== ④ Is the code right — checking and testing

- *Static analysis* — `clang-tidy`, `cppcheck`, and the compiler's own `-fanalyzer`
  (GCC). Commercially, `PC-lint Plus`, `PVS-Studio`, `Coverity`.
- *Dynamic checking* — chapter 17's sanitizers (ASan, UBSan, TSan) and
  `valgrind --tool=memcheck`. The former must be recompiled and is fast, the latter can
  be run as it stands and is slow.
- *Coverage* — `gcov`/`lcov` (GCC), `llvm-cov` (Clang). They show "which lines the
  tests actually passed through".
- *Fuzzing* — `libFuzzer`, `AFL++`, `honggfuzz`. Pour in random input and find the
  places it dies. The effect is great in code that *eats outside input*, such as parsers
  and decoders.
- *Unit testing* — C having no standard test framework, libraries such as `Unity`,
  `CppUTest` and `Criterion` are used.

=== ⑤ Is it good for people to read — formatting and documentation

`clang-format` (automatic formatting), `uncrustify`, `Doxygen` (generating
documentation from comments), and `compile_commands.json` (a list of the build
commands). The last is the file an editor's autocompletion (`clangd`) and static
analysis tools read in order to know "with what options is this file compiled", so
turning it on with one line in CMake is today's practice.

#platform[
  Observation tools of embedded work — how to measure a machine with no screen
][
  Over the tools seen in chapter 18 (probes, OpenOCD, RTT, static analysis) there is
  one more layer that *measures performance and behaviour*.

  - *Cycle counters* — read a counter inside the chip, such as the DWT cycle counter
    of Arm Cortex-M, and count the clocks of a stretch directly. The most accurate and
    the cheapest.
  - *GPIO toggling* — raise and lower one pin before and after the stretch you want to
    measure and look at it with chapter 18's logic analyser. It is the frontal method
    still widely used on chips with no software profiler.
  - *RTOS tracing* — tools such as SEGGER `SystemView` and Percepio `Tracealyzer` draw
    task switches, interrupts and queue waits on a time axis. It is the way to see with
    your eyes "why is this task late".
  - *Measuring stack usage* — the stack painting seen in chapter 69, and static
    analysis tools' worst-case depth calculation.
  - *Tracking code size* — it is common practice to hang chapter 18's map files and
    `bloaty` on CI and watch how much the size has grown with each commit.
  - *Hardware in the loop (HIL)* — attaching a real board to test equipment and running
    it automatically. With simulators (QEMU, Renode) it is one of the two axes of
    embedded CI.
]

#qa[
  When should all these tools be brought in?
][
  There is an order. Only three are put in place *from the start* — version control
  (git), a build tool (make or CMake), and warnings at maximum (`-Wall -Wextra`). After
  that it is one at a time as the need arises.

  - bugs are frequent → sanitizers and unit tests
  - it is slow → a profiler (no guessing)
  - people have multiplied → formatting tools and CI
  - it eats outside input → fuzzing
  - it reproduces only sometimes → record and replay (rr, TTD)

  Install a heap of tools first and you mostly end up not using them. *Letting the
  problem call for the tool* is the order of the field.
]

== What runs in C even today — and why

Chapter 1 said "C is everywhere". Now the concrete names and reasons can be written
down.

#dtable(
  columns: 3,
  [*project*], [*what*], [*why C*],
  [the Linux kernel], [the heart of an operating system], [it must handle hardware directly, must have no runtime dependency, and every architecture has a compiler],
  [SQLite], [the most widely deployed database in the world], [it must be ported everywhere (phones, aircraft, browsers) and needed the extreme simplicity of distributing as a single source file],
  [FFmpeg], [the standard tool of video and audio processing], [codec work where performance is everything, direct connection with hardware acceleration APIs],
  [Redis], [an in-memory data store], [predictable latency and control of memory — it manages chapter 11's ladder itself],
  [curl], [the universal tool of internet transfer], [it must go onto every platform and every device (cars, TVs, even spacecraft)],
  [CPython, Ruby and others], [implementations of other languages], [a language runtime needs the lowest layer in the end, and the C interface is the ecosystem's common tongue],
  [OpenSSL, zlib], [cryptography and compression libraries], [because they must be called from every language — the C ABI is the de facto universal interface],
)

Draw out the common threads and the reasons for choosing C organise into four.

- *When you must stand at the lowest layer* — operating systems, drivers, runtimes.
- *When it must be moved everywhere* — when a new chip appears, a C compiler is made
  first (chapter 4's portability revolution has held for half a century).
- *When performance and resources must be controlled by hand* — memory layout, latency,
  executable size.
- *When it must speak with every other language* — from Python, Java or Rust alike, a C
  function can be called. The C ABI is the international common tongue between
  languages.

== C and C++ — siblings, not parent and child

We settle the most common misunderstanding here. *C is not a subset of C++.* C++ is
not "C with object orientation and templates added" but *a different language* that
split off from C and grew separately.

A short look at the history makes the relation clear. In 1979 Bjarne Stroustrup made
"C with Classes", laying classes on C, and in 1983 the name changed to C++. Up to here
it is indeed an extension. But after that the two languages diverged, *each making its
own standard in its own committee* — C obtained its first standard as C89 in 1989, C++
as C++98 in 1998, and afterwards they referred to each other without being subordinate
to each other.

So today each has things the other does not.

#dtable(
  columns: 2,
  [*in C and not in C++*], [*in C++ and not in C*],
  [the `restrict` qualifier], [classes, inheritance, virtual functions],
  [variable-length arrays (chapter 37)], [templates and the standard library upon them],
  [flexible array members], [references (`&`) and operator overloading],
  [`_Generic` (chapter 52)], [exceptions and RAII],
  [order-independent designated initialisers], [namespaces],
  [compound literals (chapter 43)], [`auto` type deduction, lambdas],
)

== Put C code into a C++ compiler

It is seen not in words but in the flesh. Below is *entirely correct C code*.

#demo("examples-en/ch81/ccompat.c")

Compile the same file with `g++ -std=c++20` and this happens.

```text
error: invalid conversion from ‘void*’ to ‘int*’ [-fpermissive]
   15 |     int *buf = malloc(4 * sizeof *buf);
      |                ~~~~~~^~~~~~~~~~~~~~~~~
      |                      void*
error: designator order for field ‘point::x’ does not match
       declaration order in ‘point’
   23 |     struct point p = { .y = 2, .x = 1 };
```

C++ refuses the implicit conversion of `void *` that C permits — so in C++ a cast is
always attached to `malloc`'s result (and that is a practice *not recommended* in C.
That is where the "must malloc's return value be cast" debate divides). Designated
initialisers too, though C++20 accepted them, *cannot have their order changed*.

There are places besides where the meaning quietly differs. The representative is the
character constant.

```text
C  : sizeof('a') = 4      (the type of a character constant is int)
C++: sizeof('a') = 1      (the type is char)
```

It compiles on both sides and the result differs — such places are the most dangerous.
Code that used C++ reserved words such as `class`, `new`, `template` and `this` as
variable names in C does not pass over as it is either.

#realcase[
  The standard writes the differences down separately
][
  The C++ standard treats this problem formally too. The C++ standard document has a
  section called *annex C (Compatibility)* that lists "the points differing from C"
  clause by clause — the type of character constants, `void *` conversion, tag name
  spaces, the type of string literals and so on. If you want to know the differences,
  opening that annex is more accurate than guessing. That the annex exists at all
  proves this section's argument — *the two languages are different enough that
  compatibility must be cared about.*
]

== Time flows faster on the C++ side

We write a little more here of the circumstance mentioned briefly in chapter 1. Since
2011 C++ has put out a standard on a *three-year cycle* — C++11, 14, 17, 20, 23, 26.
Each edition brings large features into the language and the standard library, and the
look of "modern C++" changes correspondingly fast. C's revision cycle, by contrast, is
far longer (C89 → C99 → C11 → C17 → C23), and what enters is mostly small and
conservative.

This difference of speed is not a matter of taste but comes from *a difference of
role*. C is the language of operating systems and firmware, of the floor layer other
languages lean on, so not changing is itself a feature. That thirty-year-old code
compiles today, and that every platform has a C compiler, are the assets obtained in
exchange.

#qa[
  Then with what attitude should a learner look at this difference?
][
  Three things are recommended.

  *First, abandon the thought that "learn C++ and you know C automatically".* The
  reverse likewise. It is true that much overlapping syntax lets you pick each up
  quickly, but the idioms and ways of thinking are quite different. Bring C++'s habits
  (exceptions, RAII, templates) into C as they are and the code becomes awkward; bring
  C's habits (manual management, macros) into C++ and the code becomes dangerous.

  *Second, always be conscious of which language it is being compiled as.* `.c` and
  `.cpp` differ not in extension but in *language*. Merely knowing that the same file
  can mean different things in the two languages prevents half the accidents.

  *Third, most of what was learned in this book holds on either side.* The ladder of
  memory, the distinction of representation and abstraction, contracts and undefined
  behaviour, ownership and lifetime — these are not syntax but *the realities of
  machines and of language design*. Go off to learn C++ and that eye is used just the
  same.
]

== How to mix them — `extern "C"`

Mixing the two languages is very common in practice. The representative case is a C++
program calling a library written in C. The problem is *name mangling* — a C++
compiler, in order to support overloading, mixes parameter type information into the
function name it plants in the object file (recall chapter 50's linking), while a C
compiler leaves the name as it is. So joining them just so leaves the linker unable to
find the pair.

The solution is to mark the declarations "handle this by the C convention".

```c
/* mylib.h — the header of a library made in C */
#ifndef MYLIB_H
#define MYLIB_H

#ifdef __cplusplus
extern "C" {
#endif

int mylib_add(int a, int b);
void mylib_reset(void);

#ifdef __cplusplus
}
#endif

#endif
```

`__cplusplus` is a macro only a C++ compiler defines. So this header, read in C, is
two ordinary declarations, and read in C++ becomes declarations wrapped in
`extern "C" { ... }` — the standard idiom for *satisfying both languages with one
header*, and nearly every C library header in the world has this shape.

A few rules are worth remembering with it.

- `extern "C"` concerns *the name and the calling convention* only. It has nothing to
  do with the function's contents or with type checking.
- Overloaded functions cannot be bound in `extern "C"` — because there must be only one
  name.
- When using a C header from C++ it is courteous not to use C++ reserved words
  (`class`, `new`, `template`, `namespace`, `this`) as parameter names.
- Data crossing the boundary is kept in *a shape both languages understand* — plain
  structs and pointers, and explicit sizes. Do not throw C++ classes or exceptions
  across the boundary.

== Embedded — C's home ground

Unnoticed in daily life but, by volume, C's largest territory is embedded work. The
control panels of washing machines and air conditioners, the dozens of control units in
a car, medical devices, the low-level control of drones and robots, the firmware of
communication modules — most of it is written in C.

The reason lies in that environment's constraints. Memory is in kilobytes, there may be
no operating system (that world of chapter 6 where address 0 is the interrupt vector),
execution time must be predictable (unpredictable pauses such as garbage collection are
not permitted), and power is limited. C fits naturally in such an environment because it
is a language that *charges no cost for what is not executed* — it can be used without
the standard library (chapter 55's freestanding) and the code generated is predictable.

In embedded C the knowledge of this book is used unusually directly — handling a
particular address as a device register (chapter 6), blocking optimisation with
`volatile` (chapter 13), manipulating hardware flags with bit operations (chapters 5
and 27), not using dynamic allocation at all (chapter 41), handling communication
protocols conscious of alignment and representation (chapters 6 and 44).

== How to cross the limits — the knack of practice

C's lacks are clear — there are no namespaces, no generics, resource cleanup is not
automatic, and safety is not enforced. Practice fills these with discipline and tools
outside the language. Gathering what this book has passed through into practical
guidance gives this.

- *No namespaces* → the prefix convention (chapter 39's `proven_`) and file-level
  `static` (chapter 50's internal linkage).
- *No generics* → code generation with macros (chapter 51's `##`, the X macro), or
  generalisation taking `void *` with a size and a comparison function (the standard's
  `qsort` is that way), or C11's `_Generic` selection.
- *Resource cleanup is not automatic* → state the ownership convention in names and
  documentation (chapter 41), and gather one function's cleanup points into one place
  (the pattern of `goto` to a cleanup label is standard practice in the kernel).
- *Safety is not enforced* → layer the nets (chapter 17): maximum warnings, sanitizers,
  cross-checking with two compilers, static analysis tools, and components with checking
  built in (Part XII).
- *The standard library is thin* → choose or make the layer you need. Bringing it in as
  a vendored copy to keep the build simple is the canonical way for a small project.
- *The complexity of large projects* → divide the translation units small and reveal the
  contracts through headers (chapter 50), and automate the tests and the build (this
  chapter's make and git).

#realcase[
  The reality of mixing — C does not vanish but becomes a layer
][
  Today's large systems are mostly not written in one language. It is common to have a
  layered structure with the core needing performance in C (or C++ or Rust) and the
  logic above it in Python or JavaScript. The reason Python's numerical libraries are
  fast is that beneath them is C and Fortran, and the reason a browser plays video is
  that beneath it is a C codec. Chapter 1 said Rust and Zig are encroaching on C's
  territory, but what actually happens is closer to *coexistence* than to replacement —
  the new languages too must speak with existing C assets, so they are born equipped
  with a C interface. To be able to read and write C means, therefore, that whatever
  layer you work at, you can look into the layer below.
]

#qa[
  What is worth making next for someone who has read this book through?
][
  Something small and finite is good. A tool that reads a text file and produces
  statistics, a simple configuration-file parser (chapter 77's scanner is used as it
  stands), implementing one data structure yourself (chapter 42's linked structures),
  reading the source of a small C project you like and mending one place. The last is
  especially recommended — the ability to read somebody else's code takes up most of the
  time spent in practice, and this book spent a good deal of itself on growing the eye
  for that reading.
]

We have looked round the terrain beyond these pages too. The next chapter enlarges one
region of it — in embedded work, where C is most deeply rooted, what compilers and what
tools one works with. Then in the last chapter we retrace the road this book has
travelled and close it.
