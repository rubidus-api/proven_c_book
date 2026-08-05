#import "../../book/lib.typ": *

= The general shape of compilation

#prereq(
  ([chapter 15, Hello world], [the shape of the first program]),
  ([chapter 4, A simple model of the machine], [the machine executes only machine code]),
)

#organizer[
  We watch the four-stage relay by which the text `hello.c` becomes an
#idx("linking")  executable — preprocessing, compiling, assembling, linking.
  It is the general picture, true on any platform and with any compiler (gcc,
  clang). The two "orders on credit" from chapter 15 (what `#include` really is,
  where `printf` comes from) are settled here.
]

#deepqa[
  Chapter 4 spoke of the staircase of abstraction — layers stacked from
  transistors up to languages. Then from which floor to which floor is the
  compiler's translation?
][
  From the "C language" floor down to the "machine instruction" floor. But this
  translation is not one leap; it is a relay that steps down through landings —
  a layer that tidies text, a layer that turns C into assembly (a human-readable
  notation for machine code), a layer that stamps that into real machine
  instructions, and a layer that joins the pieces into one lump. This chapter is
  a tour of those four landings.
]

#chapter-questions()

== From outside — a single command

Appearances first. Give a source file to a compiler such as gcc or clang and an
executable comes out.

```text
$ cc hello.c -o hello      (cc stands for gcc or clang)
$ ./hello
Hello, world!
```

`-o hello` means "name the product hello". It looks finished in one command, but
behind that line four different workers run a relay. You can stop each runner
and inspect it with the options that ask the compiler to show intermediate
results (`-E`, `-S`, `-c`).

== Runner 1 — preprocessing: patchwork on text

The *preprocessor* does not know C yet. It is scissors and glue for *text*.
Lines beginning with `#` — chapter 15's orders — are directions to it.

The identity of `#include <stdio.h>` is settled here. The meaning of the
directive is astonishingly literal — *"find the file called stdio.h and paste
its contents in at this spot."* That is all. Ask for preprocessing only
(`cc -E hello.c`) and the six-line hello.c comes out swollen to *tens of
thousands of lines* — the *declarations* of the standard tools including
`printf` (the announcement that a function of this name exists — formally in
chapter 24) have all been pasted in.

The preprocessor also does other text work: substituting text (macros), and
including or excluding parts of the code by condition. Expanding the trigraphs
we met in chapter 8 was (in those days) this stage too — the preprocessor is
both the first runner of C compilation and a place where the scars of history
collect.

== Runner 2 — compiling: C into assembly

Now the main feature. The *compiler proper* reads the preprocessed C code —
checks the syntax, interprets the meaning, applies chapter 13's editing
(optimisation) — and renders it into the notation called *assembly*. Assembly is
that low-level language brushed past in chapter 4: almost one-to-one with
machine instructions, but with names so a human can read it. You can inspect
this intermediate result (`hello.s`) with `cc -S hello.c`, and it contains
fragments roughly like this (it differs by machine and compiler):

```text
        lea     rdi, [rip + .L.str]    ; prepare the address of the greeting
        call    printf                 ; call printf
        xor     eax, eax               ; make a 0
        ret                            ; return it and exit
```

The skeleton of the four statements we wrote is visible as it is — but as
chapter 13 taught, this correspondence does not always survive so neatly (turn
optimisation on and the sentence-by-sentence correspondence collapses).

== Inside runner 2 — the lexer and the parser

The single line "turns C into assembly" contains several stages. The
traditional arrangement has hardened into this, and today's gcc and clang keep
the same broad frame.

```text
source text
   → [lexer]           a stream of characters into a stream of tokens
   → [parser]          the token stream into a syntax tree
   → [semantic analysis]  type checking, name resolution
   → [intermediate form]  where optimisation happens
   → [code generation]    into the target machine's instructions
```

The *lexer* cuts characters into words. It turns the twelve characters
`int x = 42;` into five *tokens*: `int` / `x` / `=` / `42` / `;`. Whitespace and
comments disappear at this stage.

The *parser* checks whether those words are arranged according to the grammar
and, if so, weaves them into a *syntax tree*. It builds structure against rules
like "declaration = type + name + (initialiser)". Break the grammar and the
error comes out here — a message like `expected ';' before ...` is the parser's
voice.

The C standard divides tokens into six kinds.

#dtable(
  columns: 3,
  [*kind*], [*examples*], [*note*],
  [keyword], [`int` `if` `return` `sizeof` `struct`], [cannot be used as a name],
  [identifier], [`x` `main` `buffer_len`], [a name. the first character is not a digit],
  [constant], [`42` `0x1f` `3.14` `'a'`], [character constants belong here too],
  [string literal], [`"hello"`], [an array, not a constant (chapter 37)],
  [punctuator], [`+` `;` `{` `->` `<<=`], [operators, brackets, semicolons],
  [header name], [`<stdio.h>`], [used only in the preprocessing stage],
)

Two facts give you an eye for reading error messages.

*First, the lexer bites the longest thing first.* `a+++b` is cut as
`a ++ + b` — it reads `+` not one at a time but as many as can stick together.
This rule is called "maximal munch", and it startles people where a division
meets the start of a comment, as in `x/*p`.

*Second, the parser does not know what names mean.* C's grammar has a famous
ambiguity — `A * B;` may be "the product of A and B" or "a declaration of B as a
pointer to A". Which it is depends on *knowing* whether `A` is a type name, so a
C compiler solves this by passing information between the parser and the name
table (the symbol table). One root of C's reputation for hard-to-read
declaration syntax is here.

The *translation phases* covered in chapter 49 are the front half of this
picture pinned down in the standard's language — what remains after
preprocessing is the token stream above, and the compiler starts work from
there.

== Runner 3 — assembling: stamping out machine instructions

The *assembler* stamps assembly text into real machine instructions — bits. The
product is an *object file* (`hello.o`, if you make it with `cc -c`). Its
contents are now not human-readable text but the world of chapter 4: a bit
stream of machine instructions.

But an object file cannot be executed yet. *It has holes in it* — it stamped out
`call printf`, but this file does not know *where* the thing called `printf`
actually is. Only the declaration of `printf` (the announcement of existence)
came into our source; its body (the actual code) never did.

== Runner 4 — linking: joining pieces and filling holes

The last runner, the *linker*, fills that hole. The body of `printf` lives
inside the *standard library*, a bundle of object files already compiled and
placed on the system. The linker joins our object file to that bundle, fills in
the address saying "printf is here", and thereby completes a single
*executable*. This is the answer to chapter 15's second order — where does
`printf` come from: *code I never wrote is joined into my program at the
linking stage.*

#realcase[
  The strangest error — undefined reference
][
  The most bewildering error for a beginner comes from the linking stage.
  Misspell a function name or leave out a library and compilation passes
  perfectly, only for a message like `undefined reference to ...` to appear at
  the end. "The compiler accepted it, so why?" is the bewilderment — and now we
  can answer. The compilation stage is satisfied with a *declaration* (an
  announcement); actually finding and joining the body is the linker's job. When
  a different worker raises the error, the kind of error differs too — the habit
  of first asking "which of the four runners fell over?" is half of problem
  solving (formally, chapter 48).
]

#misconception[
  "The compiler translates source into an executable in one go"
][
  A natural picture planted by appearances (a single command), but in reality it
  is a relay of four tools of quite different character — patchwork on text
  (preprocessing), real translation and editing (compiling), stamping into bits
  (assembling), joining pieces (linking). This distinction is the root of
  practical instinct: you learn to read which stage an error came from, the
  build style of large projects — making object files separately and relinking
  only (chapter 48) — becomes natural, and you understand why the language rule
  of "declaration versus definition" (chapter 24) exists at all: because the
  compiler works from announcements and the linker joins the real things.
]

#qa[
  Must the four stages be run by hand every time?
][
  No — usually `cc hello.c -o hello` is enough, and the compiler runs the four
  runners in turn for you. The stage options (`-E`, `-S`, `-c`) are tools for
  learning and diagnosis. In large projects, though, it becomes standard practice
  to run only up to the third runner per file (each into a `.o`) and redo just
  the final link — there is no reason to retranslate everything because one file
  changed. That world opens in chapter 48.
]

We now have a map from source to execution. What remains is to equip the tools
so this relay can run *on your own computer* — in the next chapter we install a
compiler and also acquire the modern helpers that catch bugs like a net
(sanitizers).
