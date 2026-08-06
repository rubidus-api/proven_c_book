/* Where a program's memory really sits, confirmed by addresses.
   (The concrete values differ by machine, operating system and run. What to
    look at is the *order of the regions*.) */
#include <stdio.h>
#include <stdlib.h>

const char  ro_text[]  = "read-only";    /* a constant — usually the read-only region */
int         initialized = 7;             /* a global with a value — data */
int         zeroed;                      /* a global without one — bss   */

static void deeper(int depth, char *outer)
{
    char here;                            /* a local variable of this frame */
    if (depth == 0) {
        printf("  local variable one frame deeper : %p\n", (void *)&here);
        printf("  difference from the outer frame : %+ld bytes\n",
               (long)(&here - outer));
        printf("  => the stack grows toward %s addresses\n",
               (&here < outer) ? "lower" : "higher");
        return;
    }
    deeper(depth - 1, outer);
}

int main(void)
{
    static int  static_local;             /* local, but with static storage duration */
    int         automatic = 1;            /* automatic storage duration — the stack */
    void       *heap1 = malloc(64);
    void       *heap2 = malloc(64);

    printf("code (the function main)   : %p\n", (void *)(void (*)(void))main);
    printf("read-only string           : %p\n", (void *)ro_text);
    printf("global (with a value, data): %p\n", (void *)&initialized);
    printf("global (without one, bss)  : %p\n", (void *)&zeroed);
    printf("static inside a function   : %p\n", (void *)&static_local);
    printf("heap (malloc 1)            : %p\n", heap1);
    printf("heap (malloc 2)            : %p\n", heap2);
    printf("stack (a local variable)   : %p\n", (void *)&automatic);
    printf("\ngap between the two heap blocks : %+ld bytes\n", (long)((char *)heap2 - (char *)heap1));
    printf("distance from stack to heap     : about %.1f TiB (a 64-bit address space is this wide)\n\n",
           ((double)((char *)&automatic - (char *)heap1)) / (1024.0 * 1024.0 * 1024.0 * 1024.0));

    char anchor;                          /* the reference point for measuring the stack's direction */
    deeper(1, &anchor);

    free(heap1);
    free(heap2);
    return 0;
}
