#import "../../book/lib.typ": *

= The compiler landscape — C compilers in active service

#prereq(
  ([chapter 16, The general shape of compilation], [the translation process]),
  ([chapter 17, Setting up a development environment], [setting up the tools]),
)

#deepqa[
  Chapter 16 said compilation is a four-step relay of preprocessing,
  translation, assembling and linking, and chapter 17 installed gcc or clang as
  the tool that walks those steps for us. So what changes when the compiler
  changes — it is the same standard C, is it not?
][
  Three things change. First, *the quality of the machine code produced* (speed
  and size). Second, *which machines can be targeted* — there are CPUs in the
  world that gcc and clang simply do not know, and that place is filled by the
  compiler of the company that made the chip. Third, *which edition of the
  standard is supported* and *the character of the diagnostics*. The standard may
  be the same, but the tools are many and each is good in a different place —
  mapping that is this chapter.
]

#organizer[
#idx("compiler")  A map of which C compilers exist besides the gcc and clang
  installed in chapter 17, and where each is used. The big three of desktops and
  servers, the small tidy alternative on Windows (Pelles C), the vendor
  compilers that sell performance, the doorway to the embedded world covered at
  the end of the book, and, as an aside, the tidy Windows alternative
  (Pelles C). Only what is *currently in service* is listed — and
  the names that have retired, or are retiring, are named as such.
]

#chapter-questions()

== The big three — GCC, Clang/LLVM, MSVC

Today most desktop and server C runs on these three.

*GCC* (GNU Compiler Collection). A free-software compiler continuing since
1987, and the default of the Linux world. Its greatest strength is the sheer
number of CPUs it can target — x86, Arm and RISC-V of course, but also embedded
chips such as AVR, MSP430, SuperH and PowerPC, all in one lineage. It is why
embedded vendors so often fill the inside of their own tools with gcc
(chapter 82).

*Clang/LLVM*. A later arrival from the 2000s, built on LLVM, which was designed
so that a compiler could be *used as components*. It made its name with friendly
diagnostics, and today, thanks to that componentry, a whole ecosystem stands on
it — the static analyser (`clang-tidy`), the formatter (`clang-format`), the
autocompletion server for editors (`clangd`), and many vendors' new compilers,
all branched off LLVM. It is also the default compiler on macOS.

*MSVC* (Microsoft Visual C++). The standard tool of Windows. The name says C++
but it compiles C too; for a long time it was said to be incomplete even for
C99, but in recent years it supports C11 and C17 and is moving towards C23. Its
reason for existing is that it meshes best with the Windows API.

#qa[
  Is it all right to learn just one?
][
  Start with one, but adopt the habit of *occasionally compiling with another*.
  Different compilers catch different mistakes — it is common for one to let
  code pass that the other flags with a warning. That is why this book's
  examples are cross-checked with two compilers (see the preface). They are free
  and easy to install, so keep gcc and clang together on Linux and macOS, and
  MSVC and clang together on Windows.
]

== The compilers that sell performance — vendor compilers

Some compilers sell the promise that the same source will run faster on their
own company's CPU. You meet them mostly in large-scale computing (HPC) and on
servers.

#dtable(
  columns: 3,
  [*name*], [*whose*], [*status now*],
  [`icx` (oneAPI DPC++/C++)], [Intel], [in service. LLVM-based],
  [`icc` (C++ Compiler Classic)], [Intel], [*retired* — removed in oneAPI 2024.0],
  [AOCC], [AMD], [in service. LLVM-based],
  [NVIDIA HPC SDK (`nvc`)], [NVIDIA], [in service, alongside GPU-accelerated code],
  [IBM Open XL C/C++], [IBM], [in service (AIX, z/OS). moved onto LLVM],
  [Cray/HPE CCE], [HPE], [in service (supercomputers)],
)

One current runs through the table — *they all converged on LLVM.* Maintaining a
compiler of one's own from scratch became too expensive, so taking the common
parts (parsing the syntax, optimisation) from LLVM and attaching only a back end
suited to one's own chip became the standard approach. Intel's classic `icc`
stepping back and the LLVM-based `icx` taking its place is the most symbolic
scene of that current.

#misconception[
  "They say code gets faster if you use the Intel compiler (icc)"
][
  A long-circulated remark, but stale on two counts. First, *that `icc` no
  longer exists.* Intel announced its deprecation and removed it in oneAPI
  2024.0; what you download now is the LLVM-based `icx`. Second, the gap itself
  has narrowed. There was a period when Intel's automatic vectorisation was well
  ahead, but GCC and Clang now do the same job well. What has not changed is
  that *the few per cent gained by changing compiler* is far smaller than the
  several-fold gained by laying data out to suit the ladder of memory in
  chapter 11.
]

== Embedded — the doorway to the last chapter

Numerically, the largest branch of the world's C compilers is the embedded side.
Every company that makes chips has its own compiler, and where safety
certification is at stake (automotive, aerospace, medical) there are specialist
firms that sell that certification along with the tools. IAR, Arm's Keil family,
TI, Microchip, Renesas, Green Hills, Wind River — and the free-software SDCC for
8-bit chips. This terrain only makes sense together with the whole toolset (debug
probes, on-chip debugging, static analysis), so it gets its own chapter at the
end of the book (chapter 82).

== An aside — Pelles C, the tidy alternative on Windows

If the names so far were "what is widely used", this section is a little
different. It holds no large share of the market, but its character is
distinct enough to be worth introducing.

*Pelles C* is Windows-only freeware carried on single-handedly by Pelle Orinius
of Sweden. Editor, compiler, assembler, linker, resource editor, debugger and
even an installer builder come in one package, and one download is the whole
setup. The name comes from its author's.

Its standard support is notably diligent: the latest edition at the time of
writing (14.50, July 2026) states support for C99, C11, C17 and C23. There have
been periods in which it followed new standards faster than far larger tools.

Gathering users' assessments, they divide roughly like this.

#dtable(
  columns: 2,
  [*frequently praised*], [*frequently noted limits*],
  [installs in one go and is small], [Windows only — not for code you will port],
  [compiles fast], [essentially no C++ support (treat it as a C-only tool)],
  [good standards conformance, follows new editions], [not as full-featured as a large IDE (Visual Studio)],
  [Windows API and resource editing fit smoothly], [a thin user base, so fewer materials turn up in a search],
  [a low barrier for beginners], [weak integration with build systems and outside tools],
)

In short: it suits *working on Windows, in pure C, lightly*. Good for learning,
for small Windows utilities, and for testing whether some new standard
construct really works. Conversely, for code to be ported across platforms, or
a project to sit on CMake and CI, the big three above are the fit.

#qa[
  Can this book's examples be followed with Pelles C?
][
  They can — the examples use standard C only, so any compiler supporting C23
  gives the same results. Two things are worth knowing. First, the commands and
  option spellings this book uses in the text (`cc -Wall -Wextra …`) belong to
  the gcc/clang family, so you must find the corresponding settings in Pelles
  C's IDE. Second, the sanitizers of chapter 17 are not here — for the passages
  that need checking tools, having clang or a Linux environment alongside helps.
]

== Others, and the retired names

*Small but alive* — `tcc` (Tiny C Compiler) is a tiny compiler of a few hundred
KiB, still used for compiling C source on the spot and running it immediately.
Small compilers such as `cproc` and `chibicc` have value as "teaching material
for compilers". They are not the workhorses of practice.

*Famous names no longer in service.* When you meet these in old textbooks or
web posts, *read them with their era in mind.*

#dtable(
  columns: 2,
  [*name*], [*situation*],
  [Borland/Turbo C], [the power of the DOS and early Windows years. its practices (`conio.h`, `void main()`) are not today's standard C],
  [Watcom], [a power in DOS and game development. an open-source edition (Open Watcom) survives but is effectively stalled],
  [Intel `icc`], [removed in oneAPI 2024.0. succeeded by `icx`],
  [Dev-C++], [Bloodshed (stopped around 2005) → the Orwell fork → an Embarcadero fork in 2020, but updates are very sparse],
  [TDM-GCC], [a Windows GCC distribution long paired with Dev-C++; its versions stopped advancing, and MSYS2/MinGW-w64 or LLVM is used now],
)

The *Dev-C++ with TDM-GCC* combination in particular was used for years in
schools and academies, so material about it is still plentiful. The problem is
that the combination carries a compiler several versions behind — syntax from
C11 onwards, and the C23 keywords (chapter 65), often do not work. If you are
starting now, the two routes of chapter 17 (LLVM clang, or gcc from MSYS2) are
what to take. A good share of the walls met while following an old textbook —
"why does this syntax not work?" — are the age of the tool.

#realcase[
  Why this book verifies its examples with two compilers
][
  As stated in the preface, this book's examples are compiled and verified with
  both gcc and clang. Several times during writing there were mistakes only one
  of them caught — the possibility of format-string truncation
  (`-Wformat-truncation`), truncation by `strncpy`
  (`-Wstringop-truncation`), comparison of signed with unsigned
  (`-Wsign-compare`), applying `sizeof` to an array parameter
  (`-Wsizeof-array-argument`). One compiler's silence is not evidence that the
  code is right — it may simply mean *that tool has no eye for that mistake*.
  Keeping one more tool is like keeping one more reviewer.
]

#qa[
  What is the practical criterion for choosing a compiler, in one line?
][
  *The target machine chooses first, and the human chooses the rest.* Once the
  chip is fixed the options usually narrow to a couple (only the compilers that
  know that chip remain), and within those you decide by certification, price,
  diagnostic quality and the team's familiarity. At the learning stage it is the
  opposite — write standard C that runs anywhere, and verifying with two common
  tools is enough.
]

#recap[
  #dtable(
    columns: 2,
    [*name*], [*in one line*],
    [GCC], [free software. targets the most chips],
    [Clang/LLVM], [a compiler used as components. the base of the tool ecosystem],
    [MSVC], [the Windows standard. C11 and C17, C23 in progress],
    [`icx`, AOCC, `nvc`, Open XL], [vendors' performance compilers. mostly LLVM-based],
    [the embedded family], [chapter 82],
    [Pelles C], [a Windows-only freeware bundle, up to C23. an aside],
    [retired names], [`icc`, Turbo C, Watcom, Dev-C++, TDM-GCC],
  )
]

One last word for the learner. *Any of the compilers in active service in this
chapter will do for study.* This book's examples use standard C only, so gcc,
clang, MSVC and Pelles C all give the same results. Choose by what is to hand,
what installs easily, what your school or workplace uses — what you learn does
not change.

One thing, though, is not recommended. *Dev-C++ is still widely used because it
installs easily, but the compiler it carries is too old.* Syntax from C11
onwards and the C23 keywords do not work, which makes it easy to hit the wall of
"I wrote it as the book says, so why does it not work?" For the same reason it
is better not to follow the Turbo C screens of old textbooks. If ease of
installation is the appeal, the two routes of chapter 17 (a one-line LLVM clang
install, or gcc from MSYS2) are easy enough — and they come with the newest
standard and the checking tools.

The map of compilers is spread out. Its widest region — the compilers of
embedded work and the tools beside them — only makes sense together with the
whole toolset, so it gets its own chapter at the end of the book (chapter 82).

Part III is over. We read the first program (chapter 15), watched the relay that
turns it into an executable (chapter 16), equipped the tools (chapter 17) and
spread out the world's map of compilers (chapter 18). From the next part the
real study of the language begins — not a list of new syntax, but the goal of
Part IV: reading one piece of chapter 15's hello world completely.
