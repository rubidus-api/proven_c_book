/* 할당은 약속이다 --- 8 GiB 를 빌려도, 만지기 전에는 기억이 오지 않는다.

   VmSize 는 *배치도에 그은 넓이*이고 VmRSS 는 *실제로 붙은 쪽*이다. 이 둘이
   갈라지는 것이 번역이 있는 기계의 성격이다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

static long field_kb(const char *key)
{
    FILE *f = fopen("/proc/self/status", "r");
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

static void show(const char *key, const char *when)
{
    long vsz = field_kb("VmSize:") / 1024, rss = field_kb("VmRSS:") / 1024;
    printf("  %-24s  map %6ld MB   real %6ld MB\n", when, vsz, rss);
    printf("#DATA %s %ld %ld\n", key, vsz, rss);
}

int main(void)
{
    const size_t GIB = 1024u * 1024u * 1024u;
    const size_t want = 8 * GIB;

    printf("== a promise of %zu MB ==\n\n", want / 1024 / 1024);
    printf("  %-24s  %-13s %s\n", "", "address space", "memory that arrived");
    printf("#DATA-BEGIN\n");
    show("before", "before the request");

    char *p = malloc(want);
    if (!p) {
        printf("#DATA-END\n");
        puts("\n  * this machine refused the promise; the point below still holds.");
        return 0;
    }
    show("malloc", "right after malloc");

    /* 쪽마다 한 바이트씩 --- 쪽 하나를 붙이는 데 접촉 한 번이면 된다. */
    for (size_t i = 0; i < want; i += 4096)
        p[i] = 1;
    show("touched", "after touching it all");

    free(p);
    show("freed", "after free");
    printf("#DATA-END\n");

    puts("\n  * malloc drew a map; it did not fetch memory. The memory arrived one page");
    puts("    at a time, as the program touched it.");
    puts("  * a block this large gets a mapping of its own, so free hands the whole map");
    puts("    back. A small block does not: it returns to the heap, and the address");
    puts("    space stays as wide as it was.");
    return 0;
}
