# Proven C Book — An Introduction to Modern C with the Proven C Library

**Learn how C works, then use that understanding to design C programs.**
For a first-time programmer, the book builds a foundation in the machine and the
language. For readers who have finished an introductory text, it connects that
foundation to memory management, error handling, libraries and project structure.

*[한국어판 README](README.md)*

> You have learned pointers and structs. Who keeps track of string lengths now?
> Who frees the memory? When a function fails, what happens to the original data?
>
> This book develops the principles needed to answer those questions. Using C23
> throughout, it connects machine representations to C's rules, then uses the
> [proven C library](https://github.com/rubidus-api/proven_c_lib) and comparative
> examples to show **how those rules inform program design**.

- **Current edition**: v0.92.0 — **draft**
- **Download the PDF** — [English PDF](https://github.com/rubidus-api/proven_c_book/releases/download/v0.92.0/proven_c_book-v0.92.0-en.pdf) · [Korean PDF](https://github.com/rubidus-api/proven_c_book/releases/download/v0.92.0/proven_c_book-v0.92.0-ko.pdf)
- **Read on the web** — [English](https://rubidus-api.github.io/proven_c_book/en/) · [한국어](https://rubidus-api.github.io/proven_c_book/ko/)
- **Bundles (zip)** — [en](https://github.com/rubidus-api/proven_c_book/releases/download/v0.92.0/proven_c_book-v0.92.0-en.zip) · [ko](https://github.com/rubidus-api/proven_c_book/releases/download/v0.92.0/proven_c_book-v0.92.0-ko.zip) · [all](https://github.com/rubidus-api/proven_c_book/releases/download/v0.92.0/proven_c_book-v0.92.0-all.zip)
- Copies inside the repository: [en PDF](dist/proven_c_book-v0.92.0-en.pdf) · [ko PDF](dist/proven_c_book-v0.92.0-ko.pdf)
- **Style specimen** — every device the book uses, gathered in one place, with each
  element labelled by its own name (CSS selector on the web, function name in the
  typeset edition). Always current, independently of releases:
  [web](https://rubidus-api.github.io/proven_c_book/style-specimen.html) ·
  [PDF](https://rubidus-api.github.io/proven_c_book/style-specimen.pdf)
- 13 parts, 105 chapters, appendices A–Q and an index — 1,261 pages in English, 1,190 in Korean.
- The change log lives in [CHANGELOG.md](CHANGELOG.md).

## Beyond the first C textbook

Knowing the syntax leaves practical decisions to make. Should a full buffer reject
input or grow? How long is a returned pointer valid? When a program spans several
files, what should its interfaces expose and what should stay private?

The book connects **machine representations, C types and lifetimes, errors and
contracts, and library and project design**. A contract here means a function's
input conditions, its result and its promises when it fails.

| A question after the basics | What the book covers |
| --- | --- |
| Who allocates memory, and who frees it? | Lifetimes, ownership, allocators and arenas that group allocations with a shared lifetime |
| How do strings and arrays keep track of their size? | Length and capacity, views that refer to data without copying it, and containers |
| What remains when a function fails? | Returning errors as values and preserving the original state on failure |
| What changes when the same problem is designed differently? | [Three small JSON implementations](docs/en/ch100.html): plain C, proven, and an explicit stack for nesting |
| How do separate files become a maintainable project? | [Builds and tests](docs/en/ch102.html): headers, dependencies, public and internal boundaries, and regression tests |

For readers who want to keep developing in C, these connections provide a next
step. C++ is a separate language to choose according to a project's needs and
tools, not a required next stage of learning C. This book explores how to design
memory management, error handling and modules within C.

## Who it is for, and how to read it

- **New to programming:** read from the beginning. The first two parts introduce
  the machine and its representations; [Hello world](docs/en/ch16.html) in Part III
  introduces the first program.
- **Finished a first C book:** revisit pointers, lifetimes and conversions, then
  move to library design in Part XII and [project structure](docs/en/ch102.html).
  Each chapter states its prerequisites.
- **Already using C:** look up a rule or a design choice you want to understand
  better. The dependency map and glossary help you find the background you need.

Every chapter opens with prerequisites, a look back, learning goals and the
questions it will answer. Explanations are interspersed with questions and answers,
common misconceptions, real cases and counter-examples, so readers see both the
conditions of a rule and what changes when those conditions are broken.

**This is a reading-focused book, with no exercises.** Reading in order means
fifteen chapters of background before the first program. Readers who prefer to see
code first can start with Hello world and return to earlier chapters for unfamiliar
concepts.

It is not a project course for building a complete game, GUI application or server.
It covers the foundations those projects need in memory management, error handling,
builds and tests; domain-specific tools and experience building programs come next.

## Design habits illustrated with proven

Part XII uses the [proven C library](https://github.com/rubidus-api/proven_c_lib) to
show how recurring C problems can be handled through a common API. Alongside C23
features, it explores **making memory origins, value lifetimes and the state left
after failure explicit in code**.

- **Make allocators explicit.** Pass one to a function or store it in an object to
  specify where memory comes from.
- **Distinguish ownership from borrowing.** Use length-carrying views to reduce
  copying, and explain how long the original data must remain alive.
- **Separate failure policies.** Give callers distinct APIs for rejecting an
  operation at fixed capacity, processing part of it or growing the buffer.
- **Compare the same problem.** The JSON examples show how bounds checks, error
  handling and copying change, and which lifetime obligations take their place.

These habits also apply to code that does not use proven. The aim is to help readers
define the contracts of their own functions and data structures, beyond learning
one library's API.

proven cannot prevent every lifetime violation or use of the wrong allocator.
The verification scope of the bundled snapshot and considerations for adoption
are covered in [the introduction to proven](docs/en/ch91.html) and
[the library's boundaries](docs/en/ch99.html).

## What is inside

| Parts | What it does | Chapters like these |
| --- | --- | --- |
| I–II | How a computer is actually built | The ladder of registers and caches / IEEE 754 as a contract / Characters and text — scars inside the standard |
| III–IV | The first program and a minimal toolbox | Hello world / The landscape of working C compilers |
| V–VI | Declarations, values, control flow | **The map of types — how the standard divides them** / Implicit conversions — promotion and the usual arithmetic conversions / Loops and invariants |
| VII | **Memory** | Pointers / Null — the three siblings handled properly / The rules of pointers: alignment and provenance / Multidimensional arrays / Lifetime and storage duration |
| VIII–IX | The shape of data, and the deep corners | Unions and representation / Floating point as approximation / **Undefined behaviour** |
| X | Composition | **The world of names — four name spaces and three axes** / **Handling name collisions — from prefixes to `namespace`** / The preprocessor and translation phases / Functions as values |
| XI | **The standard library, closely read** | Streams in practice / Signals / Non-local jumps / Locales (two chapters) / Wide characters (two chapters) / Unicode in practice / Inside the allocator |
| XII | **Library design with proven** | Five bugs still shipping after 50 years / Errors are values / Allocation is a parameter / Writing it three ways — a small JSON reader |
| XIII | Continuing into practice | C in practice / Builds and tests — the structure of a shared project / How the work gets done / The embedded toolbox / Modern C, gathered up |

The seventeen appendices include reference material on operators, `printf`/`scanf`
formats, implicit conversions, further reading and the C grammar in EBNF, plus
a standard-library reference and a C23 summary. The hardware-oriented group covers
reading dumps, executable formats, C without an OS, booting, interconnects, disk
layout, measurements and memory-management hardware.

## What makes it different

- **Output comes from the examples.** All 223 listings have their execution output
  captured by verification scripts. The book records full verification under GCC 14
  and cross-checks under Clang 19, with reasons for the 7 cross-check skips in
  [the skip list](docs/example-cross-skip.tsv).
- **Standard rules and measurements are distinguished.** Code demonstrates the
  effects of optimization levels and the cost of memory access, with the measured
  environment identified. The book explains why an observation on one machine is
  not a promise made by every C implementation.
- **C23 is the default.** `bool`, `nullptr`, `[[noreturn]]` and
  `<stdckdint.h>` are introduced where they are useful, alongside comparisons with
  established practices in existing code.
- **Two editions, published together.** Korean and English, with the listings
  split per edition too (the English edition uses `examples-en/`, where comments
  and printed output are in English). Both trees are verified in full on every
  build.
- **Written with AI as an assisting tool.** The author decided and reviewed the
  structure, policies and contents. Executable examples and printed code fragments
  have separate checks; the author is responsible for the prose's accuracy and for
  correcting errors.

> **What that verification covers.** It reaches exactly this far: *the examples
> build and run in this environment, and the output on the page came from that
> run* (x86-64 Linux, all of them under GCC 14.2, cross-checked with Clang 19.1
> where 7 are skipped). It is not an
> audit of the book's prose against the standard, not a security audit, and not
> validation by large-scale real use. The prose is grounded separately, in clauses
> of the standard and primary sources; the verified scope and the limits of the
> bundled proven library are discussed in the introduction and boundaries chapters
> linked above.

## Dependencies between chapters

Each chapter opens with "What to know first", which is itself a declaration of
dependency, so those entries are extracted and drawn as a graph. It is not a
hand-drawn picture but one *generated from the manuscript* and regenerated when its
dependency declarations change.

[![Dependencies between chapters](docs/dependency-graph-en.svg)](docs/DEPENDENCIES-en.md)

The horizontal axis places the chapters in order, and each arc is one dependency ---
from the chapter leaned on (left) to the chapter itself (right). The longer the arc,
the further back it reaches; the bigger the dot, the more chapters lean on it. Any
chapter leaning on a later one shows up as a red dashed arc.

- As tables — [DEPENDENCIES-en.md](docs/DEPENDENCIES-en.md) · [한국어](docs/DEPENDENCIES.md)
- A finer grain — [the section map](docs/SECTIONS.md): forward references, difficulty, kind and size by section
- Terms — [the Korean–English glossary](docs/TERMS.md): concept terms and where each is defined
- To regenerate — `python3 scripts/make-depgraph.py`, `scripts/section-map.py`, `scripts/check-terms.py`

## Running the listings yourself

Every listing in the book is in this repository and can be built and run in one
go.

```sh
scripts/verify-examples.sh              # build + run + capture output, Korean tree
scripts/verify-examples.sh examples-en  # the English tree
CC=clang scripts/verify-examples.sh     # cross-check with another compiler
```

- A C23 compiler is required. What was actually measured is GCC 14.2 (all pass) and
  Clang 19.1 (7 skipped); older versions were not tested, so nothing is promised for them.
- Listings that `#include <proven...>` build `vendor/proven` alongside them
  automatically.

> The manuscript (Typst sources) and the typesetting scripts are not published.
> The `scripts/` directory here holds only the verification and checking tools.
> What this repository carries is the finished book (PDF and HTML) and the
> listing code.

## Layout

```
dist/        Distribution — PDFs (ko, en) and zip bundles
docs/        The HTML edition served by GitHub Pages (ko/, en/)
examples/    Executable examples, including demonstrations with several files
examples-en/ The same listings in English (comments, strings, output)
scripts/     Listing verification scripts
vendor/      A snapshot of the proven library (for linking the listings)
```

## Questions and feedback

Ask about the content on the [Q&A board (Discussions)](https://github.com/rubidus-api/proven_c_book/discussions/categories/q-a),
or report an incorrect explanation or typo in an [issue](https://github.com/rubidus-api/proven_c_book/issues).
Korean and English are both welcome. I will respond as time allows and keep a record
of answers and corrections.

## Licence

- **The text** (the generated PDF and HTML): **CC BY-NC-SA 4.0** —
  [LICENSE](LICENSE) in this repository is authoritative. You may share and
  adapt it with attribution, but not commercially, and adaptations must carry
  the same licence.
- **The listing code** (`examples/`, `examples-en/`, `scripts/`): **MIT** —
  [LICENSE-CODE](LICENSE-CODE). Code learned from the book may be reused without
  restriction.
- `vendor/proven/`: under the licence of the original work.

Details are in [LICENSE-NOTICE.md](LICENSE-NOTICE.md).

---

This is a draft, so errors remain. Wrong statements, listings that do not run and
typos are gratefully received as issues. Contact: rubidus@gmail.com.
