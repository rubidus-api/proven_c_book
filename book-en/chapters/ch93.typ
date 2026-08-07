#import "../../book/lib.typ": *

= The embedded toolbox — compilers and the tools beside them

#prereq(
  ([chapter 18, The compiler landscape], [the terrain of compilers]),
  ([chapter 79, A program's map of memory], [memory in embedded systems]),
)

#deepqa[
  Chapter 18 said that numerically the largest branch of the world's C compilers
  is embedded. Why is that — cannot gcc alone target them all?
][
  Three reasons. First, *there are chips gcc does not know* — when a company
  invents its own instruction set, the compiler that knows that chip is made by
  that company. Second, *certification*. The safety standards of automotive,
  aerospace and medical devices demand evidence that "this compiler has been
  qualified against the standard", and selling that qualification together with
  the responsibility is the business of specialist firms. Third, *code size*. On
  a chip with tens of KiB of memory, a few per cent of size decides whether a
  product exists.
]

#organizer[
#idx("embedded")  What people who write C that runs on chips carry with them.
  The compilers that differ by vendor (IAR, Arm, TI, Microchip, Renesas, SDCC
  and others), and the tools that sit beside make and git — build tools, debug
  probes and on-chip debugging, how to see a log on a machine with no screen,
  simulators, static analysis and unit testing, and the tools that measure size.
  There is no need to install any of it now. The point is *to know the names and
  what they are for.*
]

#chapter-questions()

== The embedded compilers

Only those currently in service. Since the point is recognising names, a table
will do.

#dtable(
  columns: 3,
  [*compiler*], [*chips targeted*], [*character*],
  [IAR Embedded Workbench], [Arm, RISC-V, 8051, MSP430, AVR, RX, RL78, RH850 and many more], [commercial. known for certification (automotive, medical) and code size],
  [Arm Compiler for Embedded (`armclang`)], [the Arm Cortex family], [commercial (included in Keil MDK). Clang-based],
  [TI Arm Clang (`tiarmclang`)], [TI's Arm cores], [free. Clang-based, with TI extensions],
  [MPLAB XC8 / XC-DSC / XC32], [Microchip PIC, AVR, SAM], [usable free of charge. XC32 covers Arm and MIPS],
  [Renesas CC-RX / CC-RL], [Renesas RX, RL78], [commercial. Renesas chips only],
  [Green Hills Optimising Compilers], [Arm, PowerPC, RISC-V and others], [commercial. a long-standing power in the safety market],
  [Wind River Diab], [Arm, PowerPC and others], [commercial. aerospace and automotive certification],
  [SDCC], [8051, STM8, Z80, 6502, PDK and other 8-bit], [free software. the standard for small chips],
  [`arm-none-eabi-gcc` and other GCC ports], [Arm, RISC-V, AVR, MSP430 …], [free software. the default inside vendor SDKs],
  [Clang/LLVM embedded builds], [Arm, RISC-V], [the common base of the vendor compilers],
)

Two currents show. One is the same as in chapter 18 — *nearly every new compiler
is LLVM-based* (Arm's `armclang`, TI's `tiarmclang`, Microchip's new XC32). The
other is *becoming free*. Compilers that required a paid key to enable
optimisation used to be common; Microchip removed that restriction in recent
editions.

#platform[
  Look at the chip vendor's SDK first
][
  In practice the starting point is often not choosing a compiler but getting
  *the development bundle the chip company provides* (its SDK) — ST's
  STM32Cube, NXP's MCUXpresso, Espressif's ESP-IDF, Nordic's nRF Connect SDK and
  the like. Inside them a compiler (usually GCC or Clang), a linker script,
  startup code, peripheral libraries and debugging configuration already come
  paired. Choosing tools one by one from scratch is something you do after
  learning why that bundle looks the way it does.
]

== The tools beside make and git

Now for the tools. On an embedded developer's desk there are several beyond make
and git. Grouped by what they do:

*① Assembling the build.* `make` is still the base, but today's practice puts a
*generator* on top. `CMake` is the de facto standard (it produces build files
and is commonly paired with `Ninja`, a fast executor, behind it), and `Meson` is
used too. Higher-level tools that manage libraries, boards and toolchains
wholesale include `PlatformIO` (many boards with one command), the Zephyr
project's `west`, and Espressif's `idf.py`. Projects with many configuration
options, like a kernel, switch features on and off with `Kconfig` and describe
the hardware layout in a *device tree*.

*② Burning it onto the chip and stopping it.* To flash a program onto a chip and
halt it mid-execution to look inside, you need a small piece of hardware called
a *debug probe*. SEGGER's `J-Link`, ST's `ST-Link` and the standard
`CMSIS-DAP` family are common. The software joining probe to computer is
`OpenOCD`, `pyOCD` or the vendor's own server, and on top of that chapter 17's
`gdb` works just as before — except that this is remote debugging, with the
program running *on another machine*. Chip and probe are connected by a few
wires called JTAG or SWD.

*③ Seeing a log on a machine with no screen.* Embedded boards usually have no
screen, so where `printf` output goes becomes a question. The traditional answer
is to send it out of a *serial port* (UART) and receive it on a computer with a
terminal program (`picocom`, `minicom`, `screen`, `PuTTY`). The Arm family has
two faster routes — `ITM`/`SWO`, which floats characters out over the debug
line, and SEGGER's `RTT`, in which the probe peeks at a memory buffer. There is
also *semihosting*, where the debugger performs host I/O on the target's behalf,
but it is slow and used only for testing.

#antipattern[
  Using `printf` anywhere in embedded code
][
  The `printf` debugging habit formed on a host (chapter 17) brings three
  problems in embedded work. *Size* — the whole format interpreter is linked in
  and eats tens of KiB. *Speed* — pushing one character at a time over a UART
  costs milliseconds. *Timing* — that delay changes the real-time behaviour, so
  the bug disappears the moment you print: the classic situation.

  So practice uses lighter alternatives: a reduced `printf` handling integers
  only, a scheme that sends *only a number* instead of putting the format string
  on the chip, or a nearly free channel such as the RTT above. It is also why
  chapter 17's "learn the debugger" is especially valuable in embedded work.
]

*④ Running it without the machine.* When the hardware does not exist yet, or
many boards must be tested automatically, you use a *simulator*. `QEMU` imitates
various boards, and `Renode` can imitate several devices and even a network
together, which makes it common in test automation (CI). Vendor IDEs often ship
an instruction-level simulator too.

*⑤ Having the code read for you.* Embedded software is hard to fix once shipped
(the product leaves your hands), so *static analysis* has great value. Free
tools include `cppcheck` and `clang-tidy` (and the compiler's own
`-Wall -Wextra`); commercial ones commonly used are `PC-lint Plus`,
`PVS-Studio`, `Polyspace` and `Coverity`. In the automotive industry the ability
to check conformance to a set of coding rules called `MISRA C` is especially
important — an approach of "using only the subset of C that has been decided as
allowed."

*⑥ Testing.* Code that runs on a chip is unit-tested too. For C, `Unity` (with
its build tool `Ceedling`) and `CppUTest` are widely used, and it is the practice
to compile logic that does not touch hardware *on the host* and test it there —
where you can turn on checkers like `-fsanitize=address,undefined`
(chapter 17), receiving in place of the chip the checks it cannot give you.

*⑦ Measuring size and layout.* Memory is small, so "how much went in" is
measured constantly. Binary tools such as `size`, `nm`, `objdump` and `readelf`
are the basics, and you read the *map file* the linker leaves to see what
occupies the space. Tools such as `bloaty` and `puncover` present that analysis
nicely. To convert into a format for the chip you use `objcopy`
(ELF → bin/hex) or `srec_cat`.

*⑧ Seeing the signals with your eyes.* A moment comes when software tools alone
will not do. When you must see what signal is leaving a pin, out come the
*logic analyser* (the free software `sigrok`/PulseView supports inexpensive
hardware) and the oscilloscope. That half of the "the code is right but it does
not work" problems are wiring and power is an old proverb of this world.

#realcase[
  Compiler Explorer — a museum of compilers opened in a browser
][
  One tool that needs no installation. The website #link("https://godbolt.org")[`godbolt.org`] (Compiler
  Explorer) lets you write C on the left and shows, immediately on the right,
  *what machine code that code becomes*. You can compare across compilers and
  versions (from gcc 4 to the newest, clang, MSVC, embedded arm targets —
  hundreds of them) and optimisation options.

  It is the easiest way to *see with your own eyes* what chapter 13 (compiler
  optimisation) called "the editor rewriting the code" — `x * 2` becoming a
  shift, the outputs of `-O0` and `-O2` being utterly different, a loop
  disappearing entirely, all visible in seconds. In embedded work it is also
  used to check "how many bytes of instruction does this code become."
]

#qa[
  Must all of this be learned before starting embedded work?
][
  No. The minimum bundle to start is four things — *the chip company's SDK*, *a
  compiler* (usually inside the SDK), *one debug probe*, and *a serial
  terminal*. The rest grows one at a time as the need arises. The purpose of
  this chapter is not to make you memorise a list but to let you recognise, when
  you later meet those names, *which slot the tool sits in*.
]

#recap[
  #dtable(
    columns: 2,
    [*what it does*], [*tools*],
    [assembling the build], [CMake, Ninja, Meson; PlatformIO, west, idf.py; Kconfig],
    [burning and halting], [J-Link, ST-Link, CMSIS-DAP + OpenOCD, pyOCD + gdb],
    [seeing logs], [UART + terminal, ITM/SWO, RTT, (slow) semihosting],
    [running without hardware], [QEMU, Renode, vendor simulators],
    [having code read], [cppcheck, clang-tidy; PC-lint Plus, PVS-Studio; MISRA],
    [testing], [Unity, Ceedling, CppUTest; sanitizers on the host],
    [measuring size], [size, nm, objdump, readelf; map files; bloaty, puncover],
    [seeing signals], [logic analyser (sigrok), oscilloscope],
    [inspecting machine code], [Compiler Explorer (godbolt.org)],
  )
]

We have looked round the embedded desk. After chapter 18's map of compilers and
chapter 92's toolbox, the tools of the place where C is rooted most deeply have
their names too.

Now the last chapter. It retraces the road this book has travelled, gathers the
practices of modern C into a single page of guidance, and closes.
