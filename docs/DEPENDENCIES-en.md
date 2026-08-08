# Dependencies between chapters

This document is not written by hand but **extracted from the manuscript**.
Each chapter's `What to know first` entries declare "this chapter leans on that one",
and `scripts/make-depgraph.py` reads them to build this picture and these tables.
Re-run it whenever the manuscript changes.

![Dependencies between chapters](dependency-graph-en.svg)

- Chapters: **96**
- Dependencies: **196**
- Chapters leaning on nothing: 1

## Leaning on a later chapter

None. Every chapter leans only on chapters before it.

## The most leaned-on chapters — this book's pillars

| Ch. | Title | Chapters leaning on it |
|---|---|---|
| 38 | Arrays | 9 |
| 5 | Words and addresses — the archetype of C | 7 |
| 4 | A simple model of the machine — the birth of C | 6 |
| 9 | Characters and text — scars in the standard | 6 |
| 19 | The structure of a program | 6 |
| 50 | Errors and contracts | 6 |
| 2 | The regions of memory — where a program puts what | 5 |
| 16 | The general shape of compilation | 5 |
| 23 | Declaring variables | 5 |
| 24 | Declaring and defining functions | 5 |
| 41 | Strings | 5 |
| 45 | Structs | 5 |

## Full table by part

### Part I — Ground

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 1 | Setting the scene | — | 2, 4 |
| 2 | The regions of memory — where a program puts what | 1 | 3, 4, 43, 44, 81 |
| 3 | Programs and processes — what it is to be run | 2 | 52, 75 |

### Part II — How computing works

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 4 | A simple model of the machine — the birth of C | 1, 2 | 5, 7, 11, 12, 14, 16 |
| 5 | Words and addresses — the archetype of C | 4 | 6, 7, 11, 13, 23, 35, 47 |
| 6 | Special knowledge about addresses — address 0, alignment, low bits | 5 | 26, 36, 37 |
| 7 | Representing integers — sign, overflow, shift | 4, 5 | 8, 20, 27, 28 |
| 8 | Representing numbers — the contract called IEEE 754 | 7 | 9, 26, 49, 72 |
| 9 | Characters and text — scars in the standard | 8 | 10, 41, 66, 69, 71, 88 |
| 10 | The origin of streams — punched cards, line printers, printing terminals | 9 | 15, 22, 25, 62 |
| 11 | Memory divides — registers, caches, a ladder of layers | 4, 5 | 12, 38, 39, 78 |
| 12 | The machinery of speed — the birth of the standard | 4, 11 | 13 |
| 13 | Compiler optimisation — the abstract machine | 12, 5 | 14, 19, 47, 51 |
| 14 | And so C is an abstract language | 13, 4 | 37 |

### Part III — The first program

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 15 | Hello world | 10 | 16, 19 |
| 16 | The general shape of compilation | 15, 4 | 17, 18, 53, 56, 60 |
| 17 | Setting up a development environment | 16 | 18, 94 |
| 18 | The compiler landscape — C compilers in active service | 16, 17 | 95 |

### Part IV — A minimal toolbox

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 19 | The structure of a program | 15, 13 | 20, 21, 22, 23, 52, 56 |
| 20 | Expressions and constants — the things that become values | 7, 19 | 21, 30, 33, 48 |
| 21 | Using functions — how to call | 20, 19 | 24 |
| 22 | Output | 10, 19 | 25, 57, 62 |

### Part V — Declarations: how names are made

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 23 | Declaring variables | 5, 19 | 24, 26, 27, 34, 45 |
| 24 | Declaring and defining functions | 21, 23 | 33, 43, 53, 54, 58 |
| 25 | Input | 22, 10 | 42, 91 |

### Part VI — Values and flow

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 26 | The map of types — how the standard divides them | 23, 6, 8 | 27, 29 |
| 27 | Integers — a world of finite numbers | 26, 7, 23 | 28, 29, 66, 79 |
| 28 | Integer operations — division, bits | 7, 27 | 29 |
| 29 | Implicit conversions — promotion and the usual arithmetic conversions | 26, 27, 28 | 30, 34, 57 |
| 30 | Booleans and comparison | 20, 29 | 31, 80 |
| 31 | Deciding — if and switch | 30 | 32 |
| 32 | Repetition — loops and invariants | 31 | 40 |
| 33 | The meaning of a function — copying values and side effects | 24, 20 | 34, 35, 50 |
| 34 | Assignment and side effects | 23, 33, 29 | 48 |

### Part VII — Memory

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 35 | Objects, addresses, pointers | 5, 33 | 36, 37, 38 |
| 36 | Null — the three siblings, formally | 6, 35 | — |
| 37 | The rules of pointers — alignment and provenance | 35, 6, 14 | 39, 86 |
| 38 | Arrays | 35, 11 | 39, 41, 45, 46, 48, 58, 59, 64, 86 |
| 39 | Multidimensional arrays | 38, 37, 11 | 40 |
| 40 | Loop techniques — nesting, escaping, and making a block | 32, 39 | — |
| 41 | Strings | 9, 38 | 42, 64, 76, 83, 88 |
| 42 | Safe input — blocking overflow, handling failure | 25, 41 | 50, 60, 63 |
| 43 | Lifetime and storage duration | 24, 2 | 44, 78, 81 |
| 44 | Dynamic memory | 43, 2 | 65, 82, 87, 90 |

### Part VIII — The shape of data

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 40 | Loop techniques — nesting, escaping, and making a block | 32, 39 | — |
| 45 | Structs | 23, 38 | 46, 47, 48, 73, 90 |
| 46 | Using structs — temporary values, named arguments, layout | 45, 38 | — |
| 47 | Unions and representation | 45, 5, 13 | — |

### Part IX — Deep corners

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 48 | Expressions and operators | 20, 34, 38, 45 | — |
| 49 | Real numbers — the mathematics of approximation | 8 | 72 |
| 50 | Errors and contracts | 33, 42 | 51, 74, 76, 83, 85, 96 |
| 51 | Undefined behaviour | 13, 50 | 74, 79, 96 |

### Part X — Structure

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 52 | The three faces of `main` — entry point and exit status | 3, 19 | 67, 75 |
| 53 | Several files — splitting and linking | 16, 24 | 54, 55, 84, 94 |
| 54 | The world of names — four name spaces and three axes | 24, 53 | 55 |
| 55 | Handling name collisions — from prefixes to `namespace` | 53, 54 | — |
| 56 | The preprocessor and the translation phases | 16, 19 | — |
| 57 | Variadic functions | 22, 29 | 89 |
| 58 | Functions as values — the function pointer | 24, 38 | 59 |
| 59 | How to read a declaration — two readings and `typedef` | 58, 38 | — |
| 60 | The terrain of the standard library | 42, 16 | 61, 65, 73, 83, 89 |

### Part XI — Reading the standard library

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 61 | The whole map of the standard library | 60 | 77 |
| 62 | Streams in reality — `<stdio.h>` ① | 10, 22 | 63, 68, 70, 91 |
| 63 | The traps of reading and writing — `<stdio.h>` ② | 62, 42 | 77 |
| 64 | Strings and memory — `<string.h>` | 41, 38 | — |
| 65 | The drawer of odds and ends — `<stdlib.h>` | 60, 44 | — |
| 66 | Character classification — `<ctype.h>` | 9, 27 | 67, 69 |
| 67 | Locales ① — a program's regional settings | 66, 52 | 68, 69 |
| 68 | Locales ② — numbers, money, time and sorting | 67, 62 | — |
| 69 | Wide characters ① — `wchar_t` and multibyte conversion | 9, 67, 66 | 70, 71 |
| 70 | Wide characters ② — the platforms, and wide I/O | 69, 62 | 71 |
| 71 | In practice — handling Unicode and multibyte encodings | 70, 69, 9 | — |
| 72 | Numbers — `<math.h>`, `<fenv.h>`, `<tgmath.h>` | 49, 8 | — |
| 73 | Time — `<time.h>` | 45, 60 | — |
| 74 | Diagnosis and control — `<errno.h>`, `<assert.h>`, `<signal.h>`, `<setjmp.h>` | 50, 51 | 75 |
| 75 | Signals — `<signal.h>` | 74, 52, 3 | 76 |
| 76 | Non-local jumps — `<setjmp.h>` | 75, 41, 50 | — |
| 77 | What the new standards added, and the `*_s` controversy | 61, 63 | 80 |
| 78 | Operations that do not split — `<stdatomic.h>` | 11, 43 | 92 |
| 79 | How to ask about overflow — `<stdckdint.h>` | 27, 51 | 86 |
| 80 | From macro to keyword — `bool`, `nullptr` and their companions | 77, 30 | — |
| 81 | A program's map of memory — operating systems and embedded | 43, 2 | 82, 92, 95 |
| 82 | Inside the allocator — the heap, alternative allocators, alternative standard libraries | 44, 81 | 87 |

### Part XII — proven — fundamentals, verified

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 83 | The five bugs shipped for fifty years | 50, 41, 60 | 84, 85 |
| 84 | Getting started — there is nothing to install | 53, 83 | — |
| 85 | Errors are values | 50, 83 | 93 |
| 86 | The foundation — bytes, views, and arithmetic that does not overflow | 37, 38, 79 | 90, 93 |
| 87 | Allocation is a parameter | 44, 82 | 93 |
| 88 | Strings and text | 41, 9 | 93 |
| 89 | Formatting and parsing — not writing the type twice | 57, 60 | — |
| 90 | Containers and algorithms | 45, 44, 86 | — |
| 91 | The outside world — files, streams, time, random numbers | 62, 25 | — |
| 92 | The boundaries — running things overlapped, and when there is no OS | 78, 81 | — |
| 93 | Writing it three times — a tiny JSON | 85, 86, 87, 88 | — |

### Part XIII — Closing

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 94 | C in practice — tools, projects, and territories | 53, 17 | 96 |
| 95 | The embedded toolbox — compilers and the tools beside them | 18, 81 | — |
| 96 | Modern C, gathered up | 51, 50, 94 | — |

## Chapters nothing leans on yet

Mostly the closing part. One near the front may be weakly connected.

36, 40, 46, 47, 48, 55, 56, 59, 64, 65, 68, 71, 72, 73, 76, 80, 84, 89, 90, 91, 92, 93, 95, 96

---

Generated by `python3 scripts/make-depgraph.py`
