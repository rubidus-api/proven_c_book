#import "../../book/lib.typ": *

= The compiler landscape — C compilers in active service

#organizer[
#idx("compiler")  A map of which C compilers exist besides the gcc and clang
  installed in chapter 17, and where each is used. The big three of desktops and
  servers, the small tidy alternative on Windows (Pelles C), the vendor
  compilers that sell performance, and the doorway to the embedded world covered
  in detail next chapter. Only what is *currently in service* is listed — and
  the names that have retired, or are retiring, are named as such.
]

#deepqa[
  Chapter 14 said compilation is a four-step relay of preprocessing,
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

== The big three — GCC, Clang/LLVM, MSVC

Today most desktop and server C runs on these three.

*GCC* (GNU Compiler Collection). A free-software compiler continuing since
1987, and the default of the Linux world. Its greatest strength is the sheer
number of CPUs it can target — x86, Arm and RISC-V of course, but also embedded
chips such as AVR, MSP430, SuperH and PowerPC, all in one lineage. It is why
embedded vendors so often fill the inside of their own tools with gcc
(chapter 19).

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

== The tidy alternative on Windows — Pelles C

When you want to work in C on Windows without a heavy development environment,
a long-loved tool is *Pelles C*. Freeware carried on single-handedly by Pelle
Orinius of Sweden, it bundles an editor, compiler, linker, resource editor,
debugger and even an installer builder in one package. One download and you are
done, and it is small.

Notably its standard support is diligent — the latest edition at the time of
writing (14.50, July 2026) states support from C99 through *C23*. There have
been periods in which it followed new standards faster than much larger tools.
Its targets are limited to Windows (32-bit and 64-bit, including ARM64 Windows
hosts).

#dtable(
  columns: 2,
  [*where it fits*], [*where it does not*],
  [starting C lightly on Windows], [code to be ported off Windows],
  [small utilities and learning], [integration with huge build systems],
  [trying the newest standard syntax], [targeting Linux or macOS],
)

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

== Embedded — the doorway to the next chapter

Numerically, the largest branch of the world's C compilers is the embedded side.
Every company that makes chips has its own compiler, and where safety
certification is at stake (automotive, aerospace, medical) there are specialist
firms that sell that certification along with the tools. IAR, Arm's Keil family,
TI, Microchip, Renesas, Green Hills, Wind River — and the free-software SDCC for
8-bit chips. This terrain only makes sense together with the whole toolset (debug
probes, on-chip debugging, static analysis), so it gets its own chapter next.

== Others, and the retired names

*Small but alive* — `tcc` (Tiny C Compiler) is a tiny compiler of a few hundred
KiB, still used for compiling C source on the spot and running it immediately.
Small compilers such as `cproc` and `chibicc` have value as "teaching material
for compilers". They are not the workhorses of practice.

*Famous names no longer in service* — the Borland/Turbo C family of old
Windows, Watcom, and Intel's classic `icc` above. When you meet these names in
old textbooks or web posts, *read them with their era in mind.* In particular
the practices of the Turbo C days (`conio.h`, `void main()`) are not today's
standard C.

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
    [Pelles C], [a Windows-only freeware bundle. supports up to C23],
    [`icx`, AOCC, `nvc`, Open XL], [vendors' performance compilers. mostly LLVM-based],
    [`icc`], [retired (removed in oneAPI 2024.0)],
    [the embedded family], [the next chapter],
  )
]

The map of compilers is spread out. The next chapter enters its widest region —
embedded — and looks at its compilers together with the tools that sit beside
make and git.
