#include <stdio.h>

/* Drive a loop with feof and the last item is processed once more —
   the classic wrong answer of the C introductory books. */
int main(void)
{
    const char *path = "feof_demo.txt";
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fputs("10\n20\n30\n", f);
    fclose(f);

    printf("the wrong version — while (!feof(f)):\n");
    f = fopen(path, "r");
    if (!f) return 1;
    while (!feof(f)) {
        int v;
        fscanf(f, "%d", &v);          /* it goes round once more even after the last read fails */
        printf("  read %d\n", v);      /* 30 is printed twice */
    }
    fclose(f);

    printf("the right version — driven by the read's return value:\n");
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
