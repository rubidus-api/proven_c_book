/* See a named address space: AVR `__flash`.

   This file does not run on this machine (x86-64). Compiling it is the experiment:
   run.sh builds it three times with an AVR cross compiler and shows the results.

   Three things to see:
     1. `const __flash` is a type qualifier in the same position as `const`.
     2. It is not standard C. Strict `-std=c23` rejects it; `-std=gnu23` accepts it.
     3. The same subscript `x[i]` emits different instructions for different spaces,
        and the data lands in different regions. */

const char         ram[] = "hi";      /* placed in RAM (.rodata)              */
const __flash char rom[] = "hi";      /* placed in flash (.progmem.data)      */

char from_ram(int i) { return ram[i]; }
char from_rom(int i) { return rom[i]; }

/* Mix pointers from different spaces: one points to RAM, the other to flash,
   yet the assignment silently drops the qualifier. */
const char         *p;
const __flash char *q;
void mix(void) { p = q; }
