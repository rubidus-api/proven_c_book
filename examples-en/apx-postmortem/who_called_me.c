/* Two layers that turn addresses into names --- the symbol table and debug information.
   Here a live program unwinds its own stack, but a debugger reading a dump does
   exactly the same: collect addresses, then turn them into names and line numbers. */
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

static void level3(void)                 /* static --- not in the dynamic symbol table */
{
    void *frames[16];
    int n = backtrace(frames, 16);       /* unwind and collect addresses */
    char **names = backtrace_symbols(frames, n);   /* try to put names on them */

    printf("frames captured: %d\n\n", n);
    for (int i = 0; i < n && i < 5; i++)
        printf("  [%d] %s\n", i, names[i]);
    free(names);

    puts("\nnotice: main is named, the static functions are not.");
    puts("a name here comes from the *symbol table*, and static functions are local");
    puts("symbols -- they never reach the dynamic one. the debug information (DWARF)");
    puts("still knows them, which is what addr2line reads.");
}
static void level2(void) { level3(); }
static void level1(void) { level2(); }

int main(void) { level1(); return 0; }
