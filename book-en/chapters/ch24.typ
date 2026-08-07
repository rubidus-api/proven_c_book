#import "../../book/lib.typ": *

= Declaring and defining functions

#prereq(
  ([chapter 21, Using functions], [how a function is called]),
  ([chapter 23, Declaring variables], [the split between declaration and definition]),
)

#deepqa[
  Chapter 16 said "the compilation stage is satisfied with a *declaration* (an
  announcement), and it is the linker that finds and joins the body." That
  "declaration" and the "variable declaration" of chapter 23 use the same word —
  is that a coincidence?
][
  It is not. In C a *declaration* is always the same job — "telling the compiler
  in advance a name and its shape (its type)." A variable declaration announces
  the name of a value; a function declaration announces the name of a piece of
  work. Once we have made the second kind in this chapter, even the identity of
  what `#include` was fetching (a bundle of declarations) is threaded onto one
  line.
]

#organizer[
  Chapter 21 taught how to *call* a function — now how to *make* one. The
#idx("function prototype")  distinction between definition and declaration (a
  prototype), parameters and `return`, and the first step into scope. And at the
  end of this chapter the credit carried since chapter 15 — `int main(void)` and
  `return 0` — is settled in full.
]

#chapter-questions()

== Definition — the syntax for making a worker

We now make workers like chapter 21's `abs` or `printf` ourselves. The syntax of
a function *definition* is:

```c
return-type name(parameter list)
{
    body — a list of statements
}
```

The demonstration makes it quick. A worker that takes an integer and returns its
square:

#demo("examples-en/ch24/fn.c")

Reading `square`'s definition part by part.

- The *return type* `int` — the promise that what this worker puts out is an
  integer.
- The *parameter* `int n` — a *variable declaration* that receives the material
  (exactly chapter 23's syntax). When called, the body starts with the material's
  value held in this variable. Call `square(12)` and 12 is put in `n` and the
  body runs.
- The *`return` statement* — puts out the result and *leaves immediately*.
  `return n * n;` evaluates the expression and returns its value to the caller —
  the supplier's side of "the call site turns into the return value", learned in
  chapter 21.

The picture of a call is now complete. Chapter 21 was the consumer's eye
(calling) and this chapter the producer's (making) — in
`printf("%d\n", square(12))` the whole relay is now visible: the value 12 into
`n`, the 144 of `n * n` to the call site, and on as `printf`'s material.

== Declaration — signing the contract in advance

In the example above, `square`'s definition is *above* `main`. That is no
accident — the compiler reads a file top to bottom (chapter 16), so by the time
it meets `square(12)` inside `main` it must already know *what* square is (the
types of material and result) in order to check that the call is right.

But there is no need to show the whole definition in advance. It is enough to
announce *the signature alone* — that is a function *declaration*, idiomatically
a *prototype*:

```c
int square(int n);    /* declaration: such a worker exists (somewhere) */
```

No body, just the signature and a semicolon. Write prototypes at the top of the
file and the definitions may be anywhere below — and, more importantly, they may
be *in another file*. Chapter 16's riddle is now fully solved: the identity of
the tens of thousands of lines pasted in by `#include <stdio.h>` is exactly such
*a bundle of prototypes*. Showing the compiler `printf`'s contract signature in
#idx("header file")advance — that is the header file's job, and the linker joins
the body (the story of that division of labour growing into multi-file projects
is chapter 52).

We can now also state the age of this invention, the prototype — as chapter 12
showed, it came in with C89's standardisation. Before that C did not check the
types of materials, and a call made with the wrong materials passed silently and
caused accidents. The prototype was C's first step towards being a language of
contracts: "check the contract at compile time."

#realcase[
  The wound left by calls without prototypes — implicit declaration
][
  The C of the era before prototypes (chapter 12) had one generous rule. Call a
  name never declared as if it were a function and the compiler would *make up a
  declaration*, assuming "presumably a function returning an integer", and let it
  pass (implicit declaration). It looks convenient; the result was grim — a
  function that really returned a floating-point number was mistaken for an
  integer function, and passing an integer to a function taking a pointer
  compiled without a single warning. Code that collapsed silently when moved to a
  machine where integers and pointers differ in size poured out — a great many
  bugs of this class surfaced during the move from 32 bits to 64. C99 finally
  removed implicit declaration from the standard, and today's compilers catch it
  as an error. That the rule "declare before use" is not tiresome formality but a
  barrier against accidents — that is the lesson of half a century of wounds.
]

== Scope — the range in which a name is visible

Will `square`'s `n` collide with some name in `main`? No need to worry. *A name
declared inside a block is visible only inside that block.* This range of
visibility is called *scope*. `n` exists only in `square`'s body, and `main` does
not know that name at all. It is as if each function has its own workbench, so
workers name things freely without worrying about touching one another's tools —
half the secret of a program not collapsing as it grows is this partition. (The
deeper circumstances of the workbench — the lifetime of names, the precise
picture of how values travel between functions — continue in chapters 30 and
42. And a name has two further properties besides scope — *linkage* and its
*name space* — which chapters 52 and 53 set side by side with it.)

== Settling the credit — int main(void)

Now for the thing saved up for this moment. The two lines left as an
"incantation" to the last in chapter 15's hello world, reread with the grammar
learned today.

```c
int main(void)
{
    ...
    return 0;
}
```

This is simply a *function definition*. The name is `main`, the `void` in the
parameter list is the notation stating "no materials", and the return type is
`int` — that is, "a worker that works without materials and puts out one
integer." What is special is not the grammar but the *caller*: what calls `main`
is not our code but the *operating system*. Running a program is the operating
system calling `main`, and `return 0;` returns a result value to that caller —
by convention 0 means "finished without trouble" and a nonzero value "there was
a problem." That value is not discarded — the operating system and the shell
receive it, and automation that joins programs together uses it to tell success
from failure (like chapter 10's streams, this too is a device for joining
programs as components).

Chapter 15's credit ledger is with this *entirely settled*. There is now nothing
unknown in hello world's six lines — preprocessing directive, function
definition, block, call expression statement, string literal, format, and
return. The first mountain of the introduction has been crossed.

#qa[
  If `main` is a function too, may we call `main()` ourselves?
][
  Grammatically possible, but the practice is not to, and there is no reason to —
  `main`'s essence is its role as "the entrance the operating system calls", so
  calling it again inside the code is like building the front door of a building
  inside a room. If there is work you want to reuse, take that work out as a
  separate function and call it — which is in fact the whole reason for making
  functions: *giving a name to work worth naming.*
]

We have named values (chapter 23) and named work (chapter 24). Now the last
promise deferred in chapter 22 can be kept — with somewhere to hold a value, at
last it is *input*.
