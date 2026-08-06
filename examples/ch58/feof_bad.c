#include <stdio.h>

/* feof 로 반복을 제어하면 마지막 항목이 한 번 더 처리된다 —
   C 입문서의 고전적 오답이다. */
int main(void)
{
    const char *path = "feof_demo.txt";
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fputs("10\n20\n30\n", f);
    fclose(f);

    printf("틀린 판 — while (!feof(f)):\n");
    f = fopen(path, "r");
    if (!f) return 1;
    while (!feof(f)) {
        int v;
        fscanf(f, "%d", &v);          /* 마지막 읽기 실패 후에도 한 번 더 돈다 */
        printf("  read %d\n", v);      /* 30 이 두 번 찍힌다 */
    }
    fclose(f);

    printf("올바른 판 — 읽기의 반환값으로 제어:\n");
    f = fopen(path, "r");
    if (!f) return 1;
    int v;
    while (fscanf(f, "%d", &v) == 1)
        printf("  read %d\n", v);
    if (ferror(f)) printf("  (read error)\n");
    fclose(f);

    remove(path);
    return 0;
}
