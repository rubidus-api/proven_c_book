/* See copy-on-write: fork does not copy 512 MiB.
   Count Private_Dirty rather than RSS, because RSS includes shared pages. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* smaps_rollup exists from kernel 4.14. If it cannot be read, add up the per-region smaps.
   If neither can be read, return -1 --- dividing that -1 by 1024 used to print "0 MB". */
static long roll_kb(const char *key)
{
    char line[256], pat[64];
    long v, sum = -1;
    snprintf(pat, sizeof pat, "%s %%ld", key);
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, pat, &v) == 1) { sum = v; break; }
        fclose(f);
        if (sum >= 0)
            return sum;
    }
    f = fopen("/proc/self/smaps", "r");
    if (!f)
        return -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, pat, &v) == 1)
            sum = (sum < 0 ? 0 : sum) + v;
    fclose(f);
    return sum;
}

static void show(const char *key, const char *who)
{
    long sh_kb = roll_kb("Shared_Dirty:"), pr_kb = roll_kb("Private_Dirty:");
    if (sh_kb < 0 || pr_kb < 0) {
        printf("  %-28s (unavailable: this system does not let the program read /proc/self/smaps)\n", who);
        printf("#DATA %s -1 -1\n", key);
        return;
    }
    long sh = sh_kb / 1024, pr = pr_kb / 1024;
    printf("  %-28s shared %5ld MB   private %5ld MB\n", who, sh, pr);
    printf("#DATA %s %ld %ld\n", key, sh, pr);
}

/* Make the buffer memory that can be seen from outside. Held only by a local pointer, the
   compiler treats it as memory nobody reads and removes the malloc, the fill and the writes
   entirely --- Clang 22 did, and GCC 16 removed the parent's fill. It survived only by chance
   under this machine's GCC 14 (a phone run exposed it). Stored once in a volatile global,
   outside calls (fopen, fork) must be assumed able to see that memory. */
static char *volatile held;

int main(void)
{
    const size_t N = 512u * 1024 * 1024;
    char *p = malloc(N);
    held = p;
    long sum = 0;

    if (!p) { puts("not enough memory for the experiment."); return 0; }
    memset(p, 7, N);

    printf("== what fork copies ==\n\n");
    printf("#DATA-BEGIN\n");
    show("parent0", "parent, 512 MB filled");
    fflush(stdout);                     /* keep the child from inheriting buffered parent output */

    pid_t kid = fork();
    if (kid == 0) {
        show("child0", "  child, just forked");
        for (size_t i = 0; i < N; i += 4096) sum += p[i];
        show("child_read", "  child, read every page");
        for (size_t i = 0; i < N; i += 4096) p[i] = 9;
        show("child_wrote", "  child, wrote every page");
        (void)sum;
        fflush(stdout);
        _exit(0);
    }
    wait(NULL);
    show("parent1", "parent, after the child");
    printf("#DATA-END\n");

    puts("\n  * forking cost nothing, and reading cost nothing. Writing cost everything.");
    puts("  * the pages were shared until the moment one side changed them: that is the");
    puts("    whole of copy-on-write, and it needs a machine that can translate.");
    free(p);
    return 0;
}
