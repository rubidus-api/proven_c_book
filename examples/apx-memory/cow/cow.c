/* 복사-후-쓰기를 눈으로 본다 --- fork 는 512 MiB 를 복사하지 않는다.

   세는 것은 RSS 가 아니라 *사유 더러운 쪽*(Private_Dirty)이다. RSS 는 공유하는
   쪽까지 세기 때문에, 나누어 쓰는 중인지 갈라선 뒤인지를 구별하지 못한다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static long roll_kb(const char *key)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    char line[256], pat[64];
    long v = -1;
    if (!f)
        return -1;
    snprintf(pat, sizeof pat, "%s %%ld", key);
    while (fgets(line, sizeof line, f))
        if (sscanf(line, pat, &v) == 1)
            break;
    fclose(f);
    return v;
}

static void show(const char *key, const char *who)
{
    long sh = roll_kb("Shared_Dirty:") / 1024, pr = roll_kb("Private_Dirty:") / 1024;
    printf("  %-28s shared %5ld MB   private %5ld MB\n", who, sh, pr);
    printf("#DATA %s %ld %ld\n", key, sh, pr);
}

int main(void)
{
    const size_t N = 512u * 1024 * 1024;
    char *p = malloc(N);
    long sum = 0;

    if (!p) { puts("not enough memory for the experiment."); return 0; }
    memset(p, 7, N);

    printf("== what fork copies ==\n\n");
    printf("#DATA-BEGIN\n");
    show("parent0", "parent, 512 MB filled");
    fflush(stdout);                     /* ★ 자식이 부모의 버퍼를 물려받지 않도록 */

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
