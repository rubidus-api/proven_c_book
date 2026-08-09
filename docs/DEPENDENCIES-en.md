# Dependencies between chapters

This document is not written by hand but **extracted from the manuscript**.
Each chapter's `What to know first` entries declare "this chapter leans on that one",
and `scripts/make-depgraph.py` reads them to build this picture and these tables.
Re-run it whenever the manuscript changes.

![Dependencies between chapters](dependency-graph-en.svg)

- Chapters: **97**
- Dependencies: **199**
- Chapters leaning on nothing: 1

## Leaning on a later chapter

None. Every chapter leans only on chapters before it.

## The most leaned-on chapters — this book's pillars

| Ch. | Title | Chapters leaning on it |
|---|---|---|
| 38 | Arrays | 10 |
| 5 | Words and addresses — the archetype of C | 7 |
| 4 | A simple model of the machine — the birth of C | 6 |
| 9 | Characters and text — scars in the standard | 6 |
| 19 | The structure of a program | 6 |
| 51 | Errors and contracts | 6 |
| 2 | The regions of memory — where a program puts what | 5 |
| 16 | The general shape of compilation | 5 |
| 23 | Declaring variables | 5 |
| 24 | Declaring and defining functions | 5 |
| 46 | Structs | 5 |
| 61 | The terrain of the standard library | 5 |

## Full table by part

### Part I — Ground

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 1 | Setting the scene | — | 2, 4 |
| 2 | The regions of memory — where a program puts what | 1 | 3, 4, 44, 45, 82 |
| 3 | Programs and processes — what it is to be run | 2 | 53, 76 |

### Part II — How computing works

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 4 | A simple model of the machine — the birth of C | 1, 2 | 5, 7, 11, 12, 14, 16 |
| 5 | Words and addresses — the archetype of C | 4 | 6, 7, 11, 13, 23, 35, 48 |
| 6 | Special knowledge about addresses — address 0, alignment, low bits | 5 | 26, 36, 37 |
| 7 | Representing integers — sign, overflow, shift | 4, 5 | 8, 20, 27, 28 |
| 8 | Representing numbers — the contract called IEEE 754 | 7 | 9, 26, 50, 73 |
| 9 | Characters and text — scars in the standard | 8 | 10, 42, 67, 70, 72, 89 |
| 10 | The origin of streams — punched cards, line printers, printing terminals | 9 | 15, 22, 25, 63 |
| 11 | Memory divides — registers, caches, a ladder of layers | 4, 5 | 12, 38, 39, 79 |
| 12 | The machinery of speed — the birth of the standard | 4, 11 | 13 |
| 13 | Compiler optimisation — the abstract machine | 12, 5 | 14, 19, 48, 52 |
| 14 | And so C is an abstract language | 13, 4 | 37 |

### Part III — The first program

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 15 | Hello world | 10 | 16, 19 |
| 16 | The general shape of compilation | 15, 4 | 17, 18, 54, 57, 61 |
| 17 | Setting up a development environment | 16 | 18, 95 |
| 18 | The compiler landscape — C compilers in active service | 16, 17 | 96 |

### Part IV — A minimal toolbox

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 19 | The structure of a program | 15, 13 | 20, 21, 22, 23, 53, 57 |
| 20 | Expressions and constants — the things that become values | 7, 19 | 21, 30, 33, 49 |
| 21 | Using functions — how to call | 20, 19 | 24 |
| 22 | Output | 10, 19 | 25, 58, 63 |

### Part V — Declarations: how names are made

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 23 | Declaring variables | 5, 19 | 24, 26, 27, 34, 46 |
| 24 | Declaring and defining functions | 21, 23 | 33, 44, 54, 55, 59 |
| 25 | Input | 22, 10 | 43, 92 |

### Part VI — Values and flow

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 26 | The map of types — how the standard divides them | 23, 6, 8 | 27, 29 |
| 27 | Integers — a world of finite numbers | 26, 7, 23 | 28, 29, 67, 80 |
| 28 | Integer operations — division, bits | 7, 27 | 29 |
| 29 | Implicit conversions — promotion and the usual arithmetic conversions | 26, 27, 28 | 30, 34, 58 |
| 30 | Booleans and comparison | 20, 29 | 31, 81 |
| 31 | Deciding — if and switch | 30 | 32 |
| 32 | Repetition — loops and invariants | 31 | 41 |
| 33 | The meaning of a function — copying values and side effects | 24, 20 | 34, 35, 51 |
| 34 | Assignment and side effects | 23, 33, 29 | 49 |

### Part VII — Memory

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 35 | Objects, addresses, pointers | 5, 33 | 36, 37, 38, 40 |
| 36 | Null — the three siblings, formally | 6, 35 | — |
| 37 | The rules of pointers — alignment and provenance | 35, 6, 14 | 39, 87 |
| 38 | Arrays | 35, 11 | 39, 40, 42, 46, 47, 49, 59, 60, 65, 87 |
| 39 | Multidimensional arrays | 38, 37, 11 | 40, 41 |
| 40 | Arrays and pointers — when they are the same and when they are not | 38, 39, 35 | — |
| 41 | Loop techniques — nesting, escaping, and making a block | 32, 39 | — |
| 42 | Strings | 9, 38 | 43, 65, 84, 89 |
| 43 | Safe input — blocking overflow, handling failure | 25, 42 | 51, 61, 64 |
| 44 | Lifetime and storage duration | 24, 2 | 45, 77, 79, 82 |
| 45 | Dynamic memory | 44, 2 | 66, 83, 88, 91 |

### Part VIII — The shape of data

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 41 | Loop techniques — nesting, escaping, and making a block | 32, 39 | — |
| 46 | Structs | 23, 38 | 47, 48, 49, 74, 91 |
| 47 | Using structs — temporary values, named arguments, layout | 46, 38 | — |
| 48 | Unions and representation | 46, 5, 13 | — |

### Part IX — Deep corners

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 49 | Expressions and operators | 20, 34, 38, 46 | — |
| 50 | Real numbers — the mathematics of approximation | 8 | 73 |
| 51 | Errors and contracts | 33, 43 | 52, 75, 77, 84, 86, 97 |
| 52 | Undefined behaviour | 13, 51 | 75, 80, 97 |

### Part X — Structure

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 53 | The three faces of `main` — entry point and exit status | 3, 19 | 68, 76 |
| 54 | Several files — splitting and linking | 16, 24 | 55, 56, 85, 95 |
| 55 | The world of names — four name spaces and three axes | 24, 54 | 56 |
| 56 | Handling name collisions — from prefixes to `namespace` | 54, 55 | — |
| 57 | The preprocessor and the translation phases | 16, 19 | — |
| 58 | Variadic functions | 22, 29 | 90 |
| 59 | Functions as values — the function pointer | 24, 38 | 60 |
| 60 | How to read a declaration — two readings and `typedef` | 59, 38 | — |
| 61 | The terrain of the standard library | 43, 16 | 62, 66, 74, 84, 90 |

### Part XI — Reading the standard library

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 62 | The whole map of the standard library | 61 | 78 |
| 63 | Streams in reality — `<stdio.h>` ① | 10, 22 | 64, 69, 71, 92 |
| 64 | The traps of reading and writing — `<stdio.h>` ② | 63, 43 | 78 |
| 65 | Strings and memory — `<string.h>` | 42, 38 | — |
| 66 | The drawer of odds and ends — `<stdlib.h>` | 61, 45 | — |
| 67 | Character classification — `<ctype.h>` | 9, 27 | 68, 70 |
| 68 | Locales ① — a program's regional settings | 67, 53 | 69, 70 |
| 69 | Locales ② — numbers, money, time and sorting | 68, 63 | — |
| 70 | Wide characters ① — `wchar_t` and multibyte conversion | 9, 68, 67 | 71, 72 |
| 71 | Wide characters ② — the platforms, and wide I/O | 70, 63 | 72 |
| 72 | In practice — handling Unicode and multibyte encodings | 71, 70, 9 | — |
| 73 | Numbers — `<math.h>`, `<fenv.h>`, `<tgmath.h>` | 50, 8 | — |
| 74 | Time — `<time.h>` | 46, 61 | — |
| 75 | Diagnosis and control — `<errno.h>`, `<assert.h>`, `<signal.h>`, `<setjmp.h>` | 51, 52 | 76 |
| 76 | Signals — `<signal.h>` | 75, 53, 3 | 77 |
| 77 | Non-local jumps — `<setjmp.h>` | 76, 44, 51 | — |
| 78 | What the new standards added, and the `*_s` controversy | 62, 64 | 81 |
| 79 | Operations that do not split — `<stdatomic.h>` | 11, 44 | 93 |
| 80 | How to ask about overflow — `<stdckdint.h>` | 27, 52 | 87 |
| 81 | From macro to keyword — `bool`, `nullptr` and their companions | 78, 30 | — |
| 82 | A program's map of memory — operating systems and embedded | 44, 2 | 83, 93, 96 |
| 83 | Inside the allocator — the heap, alternative allocators, alternative standard libraries | 45, 82 | 88 |

### Part XII — proven — fundamentals, verified

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 84 | The five bugs shipped for fifty years | 51, 42, 61 | 85, 86 |
| 85 | Getting started — there is nothing to install | 54, 84 | — |
| 86 | Errors are values | 51, 84 | 94 |
| 87 | The foundation — bytes, views, and arithmetic that does not overflow | 37, 38, 80 | 91, 94 |
| 88 | Allocation is a parameter | 45, 83 | 94 |
| 89 | Strings and text | 42, 9 | 94 |
| 90 | Formatting and parsing — not writing the type twice | 58, 61 | — |
| 91 | Containers and algorithms | 46, 45, 87 | — |
| 92 | The outside world — files, streams, time, random numbers | 63, 25 | — |
| 93 | The boundaries — running things overlapped, and when there is no OS | 79, 82 | — |
| 94 | Writing it three times — a tiny JSON | 86, 87, 88, 89 | — |

### Part XIII — Closing

| Ch. | Title | Leans on | Leaned on by |
|---|---|---|---|
| 95 | C in practice — tools, projects, and territories | 54, 17 | 97 |
| 96 | The embedded toolbox — compilers and the tools beside them | 18, 82 | — |
| 97 | Modern C, gathered up | 52, 51, 95 | — |

## Chapters nothing leans on yet

Mostly the closing part. One near the front may be weakly connected.

36, 40, 41, 47, 48, 49, 56, 57, 60, 65, 66, 69, 72, 73, 74, 77, 81, 85, 90, 91, 92, 93, 94, 96, 97

---

Generated by `python3 scripts/make-depgraph.py`
