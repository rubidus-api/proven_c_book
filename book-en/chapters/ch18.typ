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
(chapter 95).

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
#idx("icc")  suited to one's own chip became the standard approach. Intel's classic `icc`
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
end of the book (chapter 95).

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

Its lineage is interesting too. Pelles C did not appear from nowhere; it is a
heavily reworked *LCC* (the small, portable teaching compiler of Fraser and
Hanson). Wikipedia carries it not as an article of its own but as a section of
"LCC (compiler)", describing it there as a heavily modified LCC with C11, C17 and
C23 support, amd64 support, optimisations such as inline expansion, and an IDE.
Knowing this saves effort when hunting for material — *searching Wikipedia for a
"Pelles C" article finds nothing.*

Downloads are at the official home #link("https://pellesc.se")[`pellesc.se`], while questions and release
announcements gather on a separate forum (#link("https://forum.pellesc.de")[`forum.pellesc.de`]). The author
answering there himself is a long-standing feature of the tool.

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

=== Writing a Windows app in C

One more place for this tool is worth marking. It fits *writing a desktop app
against the Windows API (Win32)* particularly well — the resource editor and the
linker come in one package, and what comes out is a single executable with no
runtime dependency.

"Why write a Windows app in C in this day and age?" is a fair question. Stated
without exaggeration, the grounds are these.

#dtable(
  columns: 2,
  [*What Win32 has*], [*What it costs*],
  [It is the *foundation* API of Windows — the UI stacks above it come and go, this remains], [The code is long; even opening a window needs a body of set-up code],
  [Its backward compatibility is remarkable — executables built twenty years ago commonly still run], [High-DPI, dark mode and accessibility must be handled by hand],
  [No runtime or framework to install], [Some newer UI features exist only on the WinRT/WinUI side],
  [Documentation and examples are vast, and Microsoft keeps maintaining them], [Little help from a visual designer],
)

=== What comes in the one package — the resource editor and dialog designer

The real reason this tool suits Win32 work is *the editors*. The official site
lists what is inside.

#dtable(
  columns: 2,
  [*Kind*], [*What is included*],
  [Build tools], [An optimising C compiler, a macro assembler, a linker, a resource compiler, a message compiler, make, a library manager],
  [IDE], [Project management, a debugger, a *profiler*, a source editor],
  [Resource editors], [*Dialogs*, menus, string tables, accelerator tables, bitmaps, icons, cursors, animated cursors, AVI, version information, manifests],
  [Shipping], [A code-signing utility, an install builder],
  [Targets], [x64, x86 and ARM64. C99, C11, C17 and C23, OpenMP 3.1, SSE\~AVX-512 and part of NEON],
)

The *dialog editor* is what is usually meant by a GUI designer. You drag buttons
and text boxes onto a window, the layout is saved as an `.rc` resource script, and
the resource compiler puts it inside the executable. In code you bring the dialog
up with `DialogBox` or `CreateDialog` and reach controls by their numeric IDs —
the long-standing way of working in Win32.

#realcase[
  What Visual C++ 6.0 taught people to expect, and where we are now
][
  The standard tool of late-1990s Windows development was *Visual C++ 6.0* (1998),
  and its strength was exactly this: editor, compiler, debugger and resource editor
  in one package, a dialog you could draw and show at once, and nothing else to
  prepare once it was installed. The memory of a "light integrated environment that
  just works" comes from there.

  Today's Visual Studio is incomparably more powerful, and also very much larger —
  tens of gigabytes installed, a wait before it opens, and mostly features someone
  writing plain C will never touch.

  The place Pelles C is loved is in between: *a tool that keeps the feel of VC6's
  small, quick integrated environment while its compiler has followed C up to C23.*
  One download, a resource editor attached, and what comes out is an executable
  with no runtime.

  In fairness, note both directions. *What VC6 had and Pelles C does not* —
  productivity tools built on a C++ framework, such as MFC and its class wizard;
  this is a C-only tool by design. *What VC6 never had* is simply the difference of
  an era — the standards after C99, 64-bit, ARM64, modern SIMD. VC6 went out of
  support long ago and is not a tool for writing new code.

  In short, for "I want to make one small Win32 app as lightly as in the VC6 days",
  this is the closest answer available today.
]

*What Microsoft recommends for new apps is not Win32 but the current stack (the
Windows App SDK and WinUI 3)* — let that be clear. But Windows's UI stacks have
been torn up and replaced several times, MFC → WinForms → WPF → UWP → WinUI, and
each time the previous generation moved to the "maintained, but no longer
recommended" shelf. What stood outside that churn is Win32, and for *a small tool
meant to last*, that stability is itself the gain.

One thing has changed as well. The cost in the table's first row — "the code is
long" — no longer weighs as much, because *boilerplate is exactly the kind of code
today's AI produces well.* The shell that creates a window and runs a message loop
looks the same everywhere, and there is less reason to memorise it. This book's
attitude holds here too, though: *code you were handed still has to be readable
before it can be fixed.* Even if someone else writes the shell, what runs inside
it — the message loop, handles, the lifetime of resources — is yours to know.

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
C11 onwards, and the C23 keywords (chapter 77), often do not work. If you are
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

== Where the official material lives

Compiler talk is tied to versions and options, which makes *the habit of opening
the primary source* especially valuable. Here are the addresses that were live as
this book was written (domains only — paths change, domains last).

#dtable(
  columns: 3,
  [*What*], [*Official material*], [*English Wikipedia article*],
  [GCC], [#link("https://gcc.gnu.org")[`gcc.gnu.org`] — manual, options, release notes], [`GNU Compiler Collection`],
  [Clang / LLVM], [#link("https://clang.llvm.org")[`clang.llvm.org`] (the front end), #link("https://llvm.org")[`llvm.org`] (the whole)], [`Clang`, `LLVM`],
  [MSVC], [#link("https://visualstudio.microsoft.com")[`visualstudio.microsoft.com`] (install), #link("https://learn.microsoft.com/cpp")[`learn.microsoft.com/cpp`] (docs)], [`Microsoft Visual C++`],
  [Pelles C], [#link("https://pellesc.se")[`pellesc.se`] (home), #link("https://forum.pellesc.de")[`forum.pellesc.de`] (forum)], [No article of its own — a section of `LCC (compiler)`],
  [tcc], [#link("https://bellard.org/tcc")[`bellard.org/tcc`]], [`Tiny C Compiler`],
)

Three things to add. *First, the exact meaning of an option is only in that
compiler's manual* — including what the warnings turned on in chapter 17 catch.
*Second, the release notes tell you which standard features arrived in which
version* — the quickest way to check the state of C23 support. *Third, Wikipedia
is good for history and lineage; for the basis of exact behaviour, use the
official documentation.*

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
    [the embedded family], [chapter 95],
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
whole toolset, so it gets its own chapter at the end of the book (chapter 95).

Part III is over. We read the first program (chapter 15), watched the relay that
turns it into an executable (chapter 16), equipped the tools (chapter 17) and
spread out the world's map of compilers (chapter 18). From the next part the
real study of the language begins — not a list of new syntax, but the goal of
Part IV: reading one piece of chapter 15's hello world completely.
