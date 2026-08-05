#import "../../book/lib.typ": *

= Getting started — there is nothing to install

#organizer[
  The first chapter that actually uses proven. We first see why this library has no
  `configure`, no package manager and no shared library to link — and what that
  choice gives and takes away — and then run a first program. The third bug seen in
  chapter 69 (format mismatch) already disappears in this first program.
]

#deepqa[
  Chapter 48 made a multi-file program and learned about headers, object files and
#idx("the compilation process")  linking, and chapter 16 saw the four runners of
  the compilation relay. Then what exactly is "using a library" in that picture?
][
  One of two things. *Compiling it together*, or *linking something compiled
  separately*. The former road is handing somebody else's source to the compiler
  along with mine; the latter is handing the linker a lump that has already become
  object code (a static `.a` or a shared `.so`/`.dll`). Either way the compiler makes
  the call from the *declaration* (the header) and the linker finds and joins the
  *definition* — exactly chapter 48's picture. proven took the former road, and the
  next section is why.
]

== The choice of having nothing to install

proven has no installation procedure. Compile the source you have obtained together
with your program and that is all. There are only two directories that matter.

- `src/proven/` — the portable body. The operating system is not called here.
- `platform/` — a thin layer that makes system calls. It is the only part that must
  be changed when moving to a new machine.

In an environment with no operating system (embedded) it is built without
`platform/`. This separation settled the shape of the whole library — the demand
"it must run anywhere" becomes the discipline "keep neither hidden allocation nor
hidden global state".

#qa[
  Why not distribute it as a package? Installing would be more convenient.
][
  It is a trade of price for gain. What is lost is convenience — it cannot be got
  through a system package, and updating becomes not "raising a version" but
  "fetching new source". What is gained is control. The library cannot differ from
  the source you are looking at now, compilation options you did not choose do not
  come attached, and links do not break because a distribution built it with
  different settings. Above all, it is *the only model that works both in a hosted
  environment and on bare metal* — embedded work has no package manager to begin
  with.
]

== How this book's examples are built

To state it honestly, this book's proven examples are compiled as follows. The
library's source is made into object files once and linked with the example.

```text
$ cc -std=c23 -O1 -Ivendor/proven/include -c vendor/proven/src/proven/*.c
$ cc -std=c23 -Wall -Wextra -Werror -Ivendor/proven/include \
     hello.c vendor-obj/*.o -lm -o hello
```

The first line handles the body, the second my program. `-I` tells it where to find
headers (chapter 48), `-lm` joins the mathematical functions. These two lines are
what this book's verification script really runs every time, and every execution
result printed on these pages is the output of a program made that way.

== The first program

One `#include <proven.h>` opens the whole library.

#demo("examples/ch70/hello.c")

We read it line by line. `proven_println` takes a format and arguments and prints
one line to standard output — so far the same as `printf`. What differs is the
*placeholder*.

- `{}` has no type in it. It is neither `%d` nor `%s` but simply `{}`.
- The type comes *from the argument*. `PROVEN_ARG(x)` looks at `x`'s type and wraps
  the value with a fitting tag attached.
- So chapter 69's third bug — the mismatch of format and argument — *structurally*
  cannot happen. The type is not written twice, so there is no place for them to go
  out of step.

What is written after the colon, as in `{:>8}`, corresponds to the width, alignment
and precision seen in chapter 53. `>` is right alignment, `<` left alignment, `.3` is
to three decimal places. That the alignment symbol comes first is what differs from
`printf`.

#qa[
  How does `PROVEN_ARG` find out the type? Does C not lack function overloading?
][
  It uses a device that came in with C11, `_Generic` — the syntax that chooses one
  of several things *at compile time* according to an expression's type.
  `PROVEN_ARG(x)` makes a small struct with an integer tag attached if `x` is an
  `int`, a real tag if a `double`, a string tag if a `const char *`. It is not
  determining the type at run time but *using as it stands what the compiler already
  knows*, so there is no cost. The syntax and the whole formatting rules are treated
  head on in chapter 75.
]

#misconception[
  "Using a library makes the program heavy"
][
  A frequently heard worry, and it depends on the character of the language and the
  library. In C, a library compiled together as source leaves *what is not used out
  of the executable* — because the linker does not put in an object file that is not
  referenced (chapter 16's linking stage). Moreover proven has no initialisation code
  running at startup, no global state being registered, and no thread quietly rising.
  Becoming heavy is not the price of using a library but what happens when a
  framework takes over the program's structure.
]

#realcase[
  The practice of distributing as source — SQLite in one file
][
  This distribution model is not a peculiar choice of proven's alone. SQLite, the
  most widely used database engine in the world, provides as its official
  distribution form an *amalgamation* joining dozens of source files into one huge
  `.c` file — fetch it, compile it with your program, and that is all. The `stb`
  family of libraries, famous for image and font handling, is a single header file
  entire. The reason is the same in every case. In a world where build environments
  are all different, *the most portable unit of distribution is source*.
]

#recap[
  This chapter in summary.

  #dtable(
  columns: 2,
    [*what*], [*how*],
    [header], [one `#include <proven.h>`],
    [build], [compile `src/proven/*.c` together with the program],
    [OS dependence], [only in `platform/` (build without it if absent)],
    [output], [`proven_println("... {} ...", PROVEN_ARG(x))`],
    [format specification], [`{:>8}` `{:<8}` `{:.3}` — after the colon],
    [the price], [a `PROVEN_ARG` per argument, a syntax unlike the familiar `%d`],
)
]

The first program has run. Yet the `proven_println` just used can in fact fail too —
because the band going to the screen may break (chapter 10). This function returns an
error but *does not compel a check*, and that choice itself is a good entrance to
understanding this library's error model. The next chapter is that.
