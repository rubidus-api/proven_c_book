#import "../../book/lib.typ": *

= Several files — splitting and linking

#organizer[
  Every program so far has been one file. Now it grows into several —
#idx("translation unit")  the division of labour between header and source, the
  notion of the translation unit, the linkage of names (external and internal),
  and how to read link errors. Chapter 16's relay and chapter 24's distinction
  between declaration and definition come together here.
]

#deepqa[
  Chapter 24 said "write prototypes at the top of the file and the definitions may
  be anywhere below — and, more importantly, they may be in another file."
  Explaining the grounds for that with chapter 16's relay?
][
  Because the compilation stage works from *declarations alone*, and it is the
  linker that finds and joins the bodies (chapter 16). So when compiling one file
  the contents of other files are not needed at all — all that is needed is the
  promise that "a function of this name with this signature exists somewhere", and
  the vessel that carries that promise is the header file. This chapter sees that
  division of labour in the flesh.
]

== Translation units and headers

One source file after preprocessing — the result of all the `#include`s being
spread out — is called a *translation unit*. The compiler always sees one
translation unit at a time. A multi-file program means making several translation
units into object files separately and having the linker join them into one.

We see the division of labour in a three-file example.

#demo("examples-en/ch48/main.c")

`greet.h` (the header) contains only the *declaration* — the contract signature.
`greet.c` has the *definition*, and `main.c` includes only the header, knows the
contract and calls. Compilation happens separately for each, and the linker joins
the call to `greet` with its definition.

The header's `#ifndef GREET_H ... #define ... #endif` is an *include guard*. It is
the idiom that makes the contents unfold only once even if the header is attached
twice through different paths, preventing things that error when declared twice
(struct definitions and the like). In practice it is often replaced by the single
line `#pragma once` (not standard, but supported by all the major compilers).

== Linkage — does a name cross file boundaries?

A name has one more property besides scope (chapter 24) — *linkage*, that is,
whether that name is visible from another translation unit.

- *External linkage*: visible outside the file. The default for functions and
  global variables.
- *Internal linkage*: visible only within this translation unit. Attach `static`
  to a file-level declaration — the example's `static int calls` is the case.

That the word `static` is used with different meanings in chapter 39 (static
lifetime) and here (internal linkage) is C's famous word recycling — *inside a
function* static means duration, *at file level* it means linkage.

Internal linkage is a basic weapon in practice. Chapter 38 said "C has no
namespaces, so a prefix stands in for the fence"; attach `static` to helper
functions and variables used only inside a file and those names never go out into
the yard at all — no collisions, and the compiler can optimise more aggressively
(knowing that name cannot be called from another file — an honest signal to
chapter 13's editor).

#misconception[
  "Putting a function definition in a header is convenient, so it is fine"
][
  The definition is copied into every file that includes it, so if two files
  include that header there are two definitions of the same function and the
  linker raises a *multiple definition* error ("multiple definition of ..."). A
  header's role is to carry the contract, not the implementation — declarations in
  headers, definitions in sources, is the basic form. (There are exceptions:
  `static inline` functions and macros are conventionally put in headers, and so
  are C23's `constexpr` constants. Knowing the exception and using it differs from
  breaking the rule in ignorance.)
]

#qa[
  How are link error messages read?
][
  Two sentences solve most of it. *"undefined reference to X"* — the declaration
  was seen but the definition could not be found (chapter 16). A source file was
  left out of the build, a library was not joined, or the spelling differs.
  *"multiple definition of X"* — there are two or more definitions. Common when a
  definition was put in a header, or a global variable was declared and initialised
  in a header. That both messages come from *the linker, not the compiler* is the
  starting point of the diagnosis — it means the syntax was fine.
]

We have learned how to cross file boundaries. But `#include` and `#ifndef`
appeared again in this chapter — the layer chapter 16 passed over with "the
preprocessor is a text tool that does not know C". The next chapter opens that
layer head on, and also sees the formal stages by which source code becomes a
program.
