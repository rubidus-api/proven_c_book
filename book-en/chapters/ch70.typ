#import "../../book/lib.typ": *

= Getting started — there is nothing to install

#organizer[
  The first chapter that actually uses proven. We first see why this library has no
  `configure`, no package manager and no shared library to link — and what that
  choice gives and takes away — and then run a first program. The third bug seen in
  chapter 69 (format mismatch) already disappears in this first program. Then we follow
  *the whole life of one object* (make it, use it, give it back) and set up the three
  rules needed to read the rest of this part.
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

== Three rules — the key to this whole part

The functions ahead number more than a hundred, but the rules for reading their
signatures are only three. Get these three into your hand and you can read half of any
function you have never seen, without the documentation.

+ *Only a function that takes an allocator as an argument takes memory.* If
  `proven_allocator_t` appears in the signature it means "this function may allocate",
  and if it does not, it takes *not one byte*. So which functions are usable in
  embedded work and which are not divide before your eyes (chapter 73).
+ *Failure comes as a value.* If there is no result to return it gives a single
  `proven_err_t`; if there is, an `{err, value}` bundle. Before checking `err` you do
  not look at `value` (chapter 71).
+ *Give a thing back with the allocator you made it with.* What was obtained with
  `_create` is let go with `_destroy`, and what has `view` in its name is borrowed and
  is not destroyed (chapters 72 and 73).

The naming rules have almost no exceptions either.

#dtable(
  columns: 3,
  [*shape of the name*], [*meaning*], [*example*],
  [`_create`], [obtain a new object from an allocator — returns a bundle], [`proven_u8str_create`],
  [`_borrow`], [lay an object over somebody's memory — no allocation], [`proven_u8str_borrow`],
  [`_destroy`], [give it back with the allocator it was made with], [`proven_u8str_destroy`],
  [`_as_`], [see the same thing through another eye — no copying], [`proven_u8str_as_view`],
  [`_view`], [borrowed. it is not destroyed], [`proven_u8str_view_t`],
  [`_checked`], [check the boundary and error if it is broken], [`..._slice_checked`],
  [`_unchecked`], [skip the check — for places the caller has already confirmed], [`..._slice_unchecked`],
  [`_grow`], [enlarge if short — which is why it takes an allocator], [`proven_u8str_append_grow`],
  [`_or_panic`], [panic on failure. for places with nobody to return to], [`proven_arena_alloc_or_panic`],
)

== The life of one object

Rather than reading three lines of rules, it is quicker to follow one real thing to
the end. The program below holds the whole course of *making, using and giving back* a
string object on one screen.

#demo("examples/ch70/first.c")

Six places to point at.

*① It took an allocator as an argument.* That `build_line`'s first argument is an
allocator is the declaration that "this function may take memory". The caller settles
whether to give it the heap or an arena (chapter 73).

*② Making returns a bundle.* `proven_u8str_create` gives a
`proven_result_u8str_t` (that is, `{err, value}`). Before checking `err` you do not
take `value` out — that order is the whole of chapter 71.

*③ The capacity is "by content".* The 64 of `create(alloc, 64)` is *the number of
bytes of content to hold*, and the library internally takes one more byte for the NUL.
That is how `as_cstr` can hand out a C string without copying.

*④ The failure path gives back too.* If formatting fails, the string taken so far is
returned with `destroy` before the error is raised. Grow this pattern and it becomes
chapter 71's `goto` cleanup idiom.

*⑤ The place where ownership passes is explicit.* `*out = line;` is that place. After
this line the string's owner is the caller, and the responsibility to destroy it is the
caller's too.

*⑥ Destroying empties the struct.* That the length prints as 0 after `destroy` is the
evidence. It is so that the returned buffer is not still pointed at, and the contract
that *a destroyed object is not used again* stands as it is.

#antipattern[
  The four mistakes a beginner meets on the first day
][
  ```c
  /* ① taking value out without checking */
  proven_u8str_t s = proven_u8str_create(alloc, 64).value;   /* rubbish on failure */

  /* ② destroying with a different allocator */
  proven_u8str_destroy(other_alloc, &s);                     /* contract violation */

  /* ③ holding a view longer than its original */
  proven_u8str_view_t v = proven_u8str_as_view(&s);
  proven_u8str_destroy(alloc, &s);
  proven_println("{}", PROVEN_ARG(v));                       /* reads a dead place */

  /* ④ forgetting PROVEN_ARG */
  proven_println("count={}", count);                         /* does not compile */
  ```
  Of the four only ④ is caught by the compiler. The other three are blocked *by a human
  keeping the rules*, which is why the previous section said to get the three rules into
  your hand. ③ in particular is met again in chapter 74, and once more when an arena is
  reset.
]

#qa[
  Must an object be made with `_create`? What about where there is no heap?
][
  No. Most objects come with *a borrowing edition* as well.
  `proven_u8str_borrow(buf, sizeof buf)` lays a string over a stack or static array —
  it takes no allocator, so it takes not one byte, and therefore needs no `destroy`
  either (the caller is already the owner). Embedded code handles strings this way
  (chapter 74), and several of this book's examples run so.

  There is a middle form too. Take the memory once in a large piece, lay an arena over
  it and hand out from there (chapter 73) — then `malloc` is never called once while the
  `_create` family can be used as it is.
]

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

== Attaching it to your own project — a minimal Makefile

To avoid typing the two lines above every time, use chapter 79's `make`. Supposing the
library has been put whole into `vendor/proven`, this much suffices.

```make
CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Werror -O2 -Ivendor/proven/include
VSRC    = $(wildcard vendor/proven/src/proven/*.c) \
          $(wildcard vendor/proven/platform/*.c)
VOBJ    = $(VSRC:.c=.o)

app: app.o $(VOBJ)
	$(CC) $^ -lm -o $@

clean:
	rm -f app app.o $(VOBJ)
```

Only three things need be known. *`-I`* tells it where to find `<proven.h>`
(chapter 48). *`platform/`* is the thin layer that calls the operating system, so when
going to bare metal only this line is removed (chapter 78). *`-lm`* joins the
mathematical functions that real-number formatting uses — take reals out of the
formatter (chapter 78's `PROVEN_FMT_NO_FLOAT`) and this is not needed either.

#platform[
  On Windows and in embedded work
][
  *MSVC* — this library requires C23. Recent updates of Visual Studio 2022 support a
  good deal of it with `/std:clatest`, but the surest road is to use `clang-cl` or
  MinGW-w64 (GCC) on Windows too (chapter 18's terrain).

  *Embedded* — leave out `platform/` and compile only `src/proven/*.c`. There being no
  heap, `proven_heap_allocator()` returns an unusable value (all zeros), and an arena
  laid over a static array is used instead (chapter 73). The detailed procedure is
  chapter 78.
]

#recap[
  This chapter in summary.

  #dtable(
  columns: 2,
    [*what*], [*how*],
    [header], [one `#include <proven.h>`],
    [build], [compile `src/proven/*.c` with the program (`-I` for the header path, `-lm`)],
    [OS dependence], [only in `platform/` (build without it if absent)],
    [rule ①], [only a function that takes an allocator takes memory],
    [rule ②], [failure comes as a value — check `err`, then `value`],
    [rule ③], [destroy with the allocator it was made with. a `view` is not destroyed],
    [making], [`_create` (allocates) / `_borrow` (over somebody's buffer, no allocation)],
    [output], [`proven_println("... {} ...", PROVEN_ARG(x))`],
    [format specification], [`{:>8}` `{:<8}` `{:.3}` — after the colon],
    [the price], [a `PROVEN_ARG` per argument, a syntax unlike the familiar `%d`],
)
]

The first program has run. Yet the `proven_println` just used can in fact fail too —
because the band going to the screen may break (chapter 10). This function returns an
error but *does not compel a check*, and that choice itself is a good entrance to
understanding this library's error model. The next chapter is that.
