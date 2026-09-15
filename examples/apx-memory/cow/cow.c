/* 복사-후-쓰기를 눈으로 본다 --- fork 는 512 MiB 를 복사하지 않는다.

   세는 것은 RSS 가 아니라 *사유 더러운 쪽*(Private_Dirty)이다. RSS 는 공유하는
   쪽까지 세기 때문에, 나누어 쓰는 중인지 갈라선 뒤인지를 구별하지 못한다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ★ smaps_rollup 은 커널 4.14 부터다. 읽을 수 없으면 영역마다 적힌 smaps 를 더한다.
     둘 다 못 읽으면 -1 --- 예전에는 그 -1 을 1024 로 나눠 「0 MB」로 찍었다(폰에서 실제로). */
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
