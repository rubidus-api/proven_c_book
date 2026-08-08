#import "../../book/lib.typ": *

= A program's map of memory — operating systems and embedded

#prereq(
  ([chapter 43, Lifetime and storage duration], [storage duration]),
  ([chapter 2, The regions of memory], [the regions of memory]),
)

#deepqa[
  Chapter 43 said "in the C standard there is neither the word stack nor any
  promise about its size", and that it settles only the automatic storage duration
  and leaves the rest to the implementation. Then in a real program, who settles
  that place?
][
  Three layers settle it between them. *The compiler and linker* divide the regions
  inside the executable file (which variable goes to which region), *the operating
  system* reads that file and spreads it into an address space, preparing the
  places for stack and heap, and *the startup code* does the remaining trimming.
  With no operating system the startup code takes on the last two entirely — that is
  the embedded world, and the latter half of this chapter is that story.
]

#organizer[
#idx("stack size")  We redraw at a larger scale the map of memory sketched in
  chapter 43. What is absent from the C standard and present only in the operating
  system — the stack and its limit — is confirmed on Linux and on Windows, and we
#idx("bss")  see what relation `data`, `bss` and the read-only region have to the
  executable file. Then we go down into the world with no operating system and see
  in detail how memory is laid out on Arm Cortex-M, AVR and PIC — and what the
  startup code does.
]

#chapter-questions()

== Regions in the executable file, regions in the address space

Writing out again the order chapter 43's example showed, but this time separating
*what is contained in the executable file* from *what arises when it runs*.

#dtable(
  columns: 4,
  [*region*], [*is it in the file*], [*what lives there*], [*permissions*],
  [`.text`], [yes], [machine instructions], [read, execute],
  [`.rodata`], [yes], [string literals, `const` data], [read],
  [`.data`], [yes (values too)], [globals and `static`s with a nonzero initial value], [read, write],
  [`.bss`], [*only the size*], [globals and `static`s with a zero or no initial value], [read, write],
  [heap], [no], [what `malloc` gave], [read, write],
  [stack], [no], [local variables, return addresses], [read, write],
)

That *`.bss` has only its size in the file* is the heart of this table (chapter 43
showed the origin of the name). Filling with zeros happens at run time — with an
operating system the kernel gives pages already filled with zeros, and without one
the startup code turns the loop itself.

One practical misunderstanding is resolved here. If you declared a big global array
and the executable's size did not grow, that is normal (`.bss`). Conversely, give
that array even one nonzero initial value — `int t[1000000] = {1};`, say — and the
whole array goes to `.data` and the file grows by 4 MB.

== Linux's map

On Linux, one program's address space is laid out roughly like this (on 64-bit).

```text
high address  ┌──────────────────────────┐
              │ kernel region (no access)│
              ├──────────────────────────┤
              │ stack     ↓ (grows down)  │  default limit 8 MiB
              │   ...  (a very wide gap)  │
              │ mmap region ↓             │  shared libraries, big mallocs
              │   ...                     │
              │ heap      ↑ (grows up)    │  enlarged with brk
              ├──────────────────────────┤
              │ .bss / .data / .rodata    │
              │ .text                     │
low address   └──────────────────────────┘  around address 0 access is forbidden (chapter 6)
```

There are four things to take away.

*① The stack limit is 8 MiB by default.* It is seen and changed with `ulimit -s`
(chapter 43). This limit is not a *reservation* but an upper bound, so in reality
pages attach as much as is used.

*② Overflow dies at once — the guard page.* Below the stack there is a page marked
as forbidden. The moment the stack touches that place a signal (SIGSEGV) arises and
the program stops. It is the device that prevents the worst accident, "quietly
overwriting somebody else's place".

*③ Large allocations go not to the heap but through `mmap`.* The allocator cuts
small requests out of the heap and, for large requests (glibc's default threshold
is around 128 KiB), receives a whole new region from the kernel. So large blocks are
sometimes really returned to the operating system on `free` — the exception to
chapter 44's "`free` mostly does not return to the OS".

*④ Addresses change on every run — ASLR.* Run chapter 43's example twice and the
addresses differ. It is a security device (address space layout randomisation) to
keep an attacker from knowing addresses in advance. So code that *records an address
value and uses it on the next run* does not hold.

#platform[
  Each thread has its own stack
][
  Make a strand (a thread) and one more stack arises with it. On Linux the default
  size mostly follows the main thread's limit at 8 MiB, and it is settled with
  `pthread_attr_setstacksize`. This fact leaves two things in practice — a design
  making thousands of strands reserves that much *address space* (bearable on
  64-bit but soon exhausted on 32-bit), and deep recursion inside a strand
  overflows sooner than in the main thread.
]

== Windows' map

The big picture is similar but the numbers and names differ.

#dtable(
  columns: 3,
  [], [*Linux*], [*Windows*],
  [default stack], [8 MiB (a limit)], [*1 MiB* (reserved), the first commit is 4 KiB],
  [specifying the stack size], [`ulimit -s`, `pthread_attr_setstacksize`], [the linker's `/STACK:reserve[,commit]`, a `CreateThread` argument],
  [executable format], [ELF], [PE],
  [large allocations], [`mmap`], [`VirtualAlloc` (called by the heap manager)],
  [heap API], [`malloc` → `brk`/`mmap`], [`malloc` → `HeapAlloc` → `VirtualAlloc`],
  [address randomisation], [ASLR], [ASLR (the same concept)],
)

*Windows' 1 MiB often makes trouble in practice.* There are three typical cases in
which code that was fine on Linux dies of stack overflow on Windows — a large local
array, deep recursion, and calls passing a large struct by value (chapter 46). The
structure separating reservation from commitment is worth knowing too: the 1 MiB is
merely *addresses held aside*, and physical memory attaches as much as is used. So
reserving a large stack does not cost much if the actual usage is small.

#realcase[
  "It works on Linux but dies on Windows"
][
  An accident that comes up repeatedly in porting work. The cause is usually within
  two lines.

  ```c
  void process(void) {
      char buffer[4 * 1024 * 1024];   /* 4 MiB — passes within Linux's 8 MiB */
      ...                             /* instant death in Windows' 1 MiB */
  }
  ```

  There are three roads to mending it. Enlarge the stack with a linker option
  (`/STACK:8388608`), turn the buffer `static` (though re-entrancy and thread safety
  are lost), or move it to `malloc`. Which of the three is right is settled by that
  buffer's character — if it is large and outlives the function, the heap; if it is
  large but finished within the function, the heap or a larger stack; if it is small
  and hot, the stack as it is.
]

== The world with no operating system — freestanding implementations

Now we go down to the side with no kernel, no virtual memory and no ASLR. It is the
world of the *freestanding implementation* seen in chapter 60. Here most of the
earlier picture vanishes and instead *physical addresses appear bare*.

The common skeleton is this. A chip mostly has two kinds of memory — *flash*, which
remains when the power is off (code and constants), and *RAM*, which vanishes when
it is off (variables and the stack). And when the program starts, the *startup code*
(often `crt0` or `Reset_Handler`) does three things.

+ *Copies `.data` from flash to RAM.* The initial values must survive the power
  being off, so they are stored in flash, and the variables must be in RAM, so they
  are moved at startup.
+ *Fills `.bss` with zeros.* The work the kernel did for us on Linux.
+ *Sets up the stack pointer and calls `main`.*

These three lines show that C's promise of "an uninitialised global is 0" is *not
free but work somebody does*. And this code mostly pairs with a linker script — a
file in which a human writes down which region is placed at which address.

=== Arm Cortex-M — the first word of the vector table is the stack

The family most widely used in microcontrollers. The layout is mostly this.

```text
0x0800_0000  flash  ┌──────────────┐
                    │ vector table │  [0] the initial stack pointer value
                    │              │  [1] the address of Reset_Handler
                    │ .text        │
                    │ .rodata      │
                    │ .data source │ → copied to RAM at startup
                    └──────────────┘
0x2000_0000  RAM    ┌──────────────┐
                    │ .data        │
                    │ .bss         │
                    │ heap    ↑    │ (if used)
                    │   ...        │
                    │ stack   ↓    │ ← initial SP = the end of RAM
                    └──────────────┘
```

Three characteristics to take away.

*① The stack's starting address is embedded in the vector table.* On reset, a
Cortex-M reads the first word at address 0 and takes it as the stack pointer, then
reads the second word and jumps there. The stack's place is, in effect, *set up by
the hardware reading it from the file*.

*② There are two stack pointers.* MSP (main) and PSP (process). Used without an
operating system (an RTOS) only MSP is used, but lay an RTOS on top and it divides
them so that the kernel uses MSP and each task uses PSP. Each task has its own
stack, and its size is *written as a number by a human when creating the task* —
there is no such generosity as Linux's 8 MiB; it is usually a few hundred bytes to a
few KiB.

*③ There is a device to report overflow, or there is not.* Higher chips have a stack
limit register (MSPLIM/PSPLIM) or an MPU and can catch overflow as an exception. On
chips without them the stack quietly overwrites `.bss` — so a practice arose:
painting the stack region beforehand with a particular value (`0xAA`, say) and later
counting how much has been erased to measure the *maximum usage* (*stack painting*).
Static analysis tools also calculate the worst-case depth from the call graph.

=== AVR — Harvard architecture, so even constants are copied

The 8-bit family widely known through the Arduino. The *Harvard architecture* seen
in chapter 58 — where the address spaces of code and data are entirely different —
becomes flesh here.

```text
flash (program space)         RAM (data space)
┌───────────────────┐        ┌──────────────────┐
│ vector table      │        │ .data            │ ← copied from flash
│ .text             │        │ .bss             │
│ .data's initial   │──────▶ │ heap ↑ (if used) │
│ values (and       │        │  ...             │
│ constants)        │        │ stack ↓          │ ← starts at RAMEND
└───────────────────┘        └──────────────────┘
```

Two characteristics dominate practice.

*① String constants eat RAM.* The address spaces being divided, flash cannot be read
with an ordinary pointer. So the compiler takes the safe side — it copies even string
literals into RAM. On a chip with only 2 KiB of RAM, this is where the situation
arises of a few lines of log strings eating half the RAM. The solutions are
`PROGMEM` and `pgm_read_byte`, and macros such as `F("...")` — instructions saying
*"leave this constant in flash and read it with a special instruction"*. These are
not standard C but extensions of the AVR tools.

*② The stack and the heap grow facing each other in the same RAM.* The stack grows
down from the end of RAM, the heap grows up from after `.bss`. When the two meet —
they overwrite each other with no warning. There is nothing like a guard page. So
the long-standing practice of the AVR world is *not to use dynamic allocation at
all*.

=== PIC — banks and the "compiled stack"

Microchip's 8-bit family (PIC10/12/16/18) is different again in structure.

*① The data memory is divided into banks.* The window that can be seen at once is
small, so to touch a variable in another bank the bank-select register must be
changed. The compiler inserts this for you, but if the variable layout is bad the
bank-switching instructions multiply and the code grows larger and slower.

*② There is no data stack in the hardware.* PIC10/12/16 have only a *call stack* (a
hardware stack piling up return addresses) and its depth is fixed (eight levels,
say; PIC18 has thirty-one). There is no stack to hold local variables. So the XC8
compiler uses a method called the *compiled stack* — it allocates a place for each
function's local variables *statically*, and functions that cannot be alive at the
same time overlap in that place.

*③ So recursion is forbidden by default.* If the place for local variables is
static, the same function cannot be called overlapping itself. The call depth cannot
exceed the hardware limit either. Since the compiler calculates "can this function
call that one" from the call graph in order to divide the places, scattering
function pointers about (chapter 58) leaves the compiler unable to know the graph
and the layout worsens. PIC24, dsPIC and PIC32 are different — they have a real data
stack and are closer to ordinary C.

#dtable(
  columns: 4,
  [], [*Arm Cortex-M*], [*AVR*], [*PIC (8-bit)*],
  [architecture], [von Neumann (unified addresses)], [Harvard], [Harvard + banks],
  [stack], [down from the end of RAM, two SPs], [down from RAMEND], [only a hardware call stack],
  [local variables], [the stack], [the stack], [*laid out statically* (a compiled stack)],
  [recursion], [possible], [possible (dangerous)], [effectively impossible],
  [constants], [read ordinarily], [`PROGMEM` needed], [special access needed],
  [heap], [usable (not recommended)], [barely used], [barely used],
  [overflow detection], [MPU, SPLIM (if present)], [none], [none],
)

#misconception[
  "Can `malloc` not just be used in embedded work too?"
][
  It can be used, but here chapter 44's reasons weigh far more. First, *there is
  nowhere to go on failure.* On a desktop you can take the null and terminate, but
  for a brake system or a pacemaker "terminate" is not an answer. Second,
  *fragmentation does not recover.* In a device running for months or years without
  a reboot, a fragmented heap leads in the end to allocation failure. Third, *the
  time is not constant.* Allocation time varies with the request and the heap's
  state, so it does not fit real-time control that must finish within a settled
  time.

  So the norm of this world is *static allocation*. Take as much as is needed at
  compile time and reuse it. Next most common are *pools* (making pieces of the same
  size in advance and lending them out) and *arenas* (cutting from one lump and
  throwing the whole away) — both are constant in time and free of fragmentation. It
  is why MISRA and the safety standards forbid or strongly restrict dynamic
  allocation, and the next chapter and Part XII treat those alternatives.
]

#qa[
  How is the stack size settled in embedded work? Can it not be taken generously as
  on Linux?
][
  In a world where the whole RAM is a few KiB there is no "generously". The method
  in the field overlaps three things. *Calculation* — a static analysis tool obtains
  the worst-case stack depth from the call graph (with recursion and function
  pointers the calculation becomes impossible). *Measurement* — measure the real
  maximum usage by the stack painting seen above and add a margin. *Protection* —
  catch overflow as an exception with an MPU or a stack limit register.

  And one design rule follows: *do not use deep recursion or large local arrays.*
  The fact seen in chapter 46 that "passing a struct by value makes the stack jump
  by that much" leads here straight to the product's failure.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [`.bss`], [*only the size* in the file. filling with zeros happens at run time],
    [Linux's stack], [8 MiB by default, `ulimit -s`. caught at once by the guard page],
    [Windows' stack], [*1 MiB* by default (4 KiB committed), the linker's `/STACK`],
    [porting accidents], [large local arrays and deep recursion die on Windows first],
    [large allocations], [go separately through `mmap`/`VirtualAlloc`],
    [freestanding], [the startup code copies `.data` + zeroes `.bss` + sets the SP],
    [Cortex-M], [the vector table's first word is the initial SP. MSP/PSP],
    [AVR], [Harvard — even constants eat RAM (`PROGMEM`). stack↔heap collision],
    [8-bit PIC], [a compiled stack. recursion effectively impossible],
    [embedded allocation], [static > pool > arena. `malloc` last],
  )
]

The map is drawn. The next chapter opens one region of that map — the heap — to see
what an allocator actually does, and what choices there are besides the standard
`malloc`.
