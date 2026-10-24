/* LC_COLLATE — strcmp 는 사전순이 아니다. strcoll 과 정렬 키(strxfrm). */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *words[] = {
    "Zebra", "apfel", "Apfel", "Äpfel", "banane", "Öl", "Zoo", "zoo",
};
#define N (sizeof words / sizeof *words)

static int by_strcmp(const void *a, const void *b)
{ return strcmp(*(const char *const *)a, *(const char *const *)b); }

static int by_strcoll(const void *a, const void *b)
{ return strcoll(*(const char *const *)a, *(const char *const *)b); }

static void sort_and_show(const char *label, int (*cmp)(const void *, const void *))
{
    const char *v[N];
    memcpy(v, words, sizeof v);
    qsort(v, N, sizeof *v, cmp);
    printf("  %-10s", label);
    for (size_t i = 0; i < N; i++) printf(" %s", v[i]);
    putchar('\n');
}

int main(void)
{
    static const char *locales[] = { "C", "en_US.UTF-8", "de_DE.UTF-8", "ko_KR.UTF-8" };

    puts("sorting the same words two ways.");
    printf("  input     ");
    for (size_t i = 0; i < N; i++) printf(" %s", words[i]);
    puts("\n");

    for (size_t i = 0; i < sizeof locales / sizeof *locales; i++) {
        if (!setlocale(LC_ALL, locales[i])) {
            printf("[%s] not available on this machine\n\n", locales[i]);
            continue;
        }
        printf("[%s]\n", locales[i]);
        sort_and_show("strcmp", by_strcmp);    /* 바이트 값 순서 — 로케일 무관 */
        sort_and_show("strcoll", by_strcoll);  /* 로케일이 정한 사전 순서 */
        putchar('\n');
    }

    /* strxfrm — 비교를 여러 번 할 때는 '정렬 키'를 한 번 만들어 두는 편이 낫다.
       변환한 키끼리는 strcmp 로 견주어도 strcoll 과 같은 순서가 나온다. */
    puts("strxfrm freezes a locale comparison into a 'key':");
    for (size_t i = 0; i < sizeof locales / sizeof *locales; i++) {
        if (!setlocale(LC_ALL, locales[i])) continue;

        char key[64];
        size_t need = strxfrm(key, "Äpfel", sizeof key);
        printf("  %-12s \"Äpfel\" -> key of %zu bytes: ", locales[i], need);
        if (need >= sizeof key) { puts("(buffer too small)"); continue; }
        for (size_t k = 0; k < need && k < 12; k++)
            printf("%02X ", (unsigned char)key[k]);
        puts(need > 12 ? "..." : "");
    }

    puts("\nchecking that strcmp on the keys agrees with strcoll:");
    for (size_t i = 0; i < sizeof locales / sizeof *locales; i++) {
        if (!setlocale(LC_ALL, locales[i])) continue;
        char ka[64], kb[64];
        const char *a = "Äpfel", *b = "banane";
        if (strxfrm(ka, a, sizeof ka) >= sizeof ka) continue;
        if (strxfrm(kb, b, sizeof kb) >= sizeof kb) continue;
        int c1 = strcoll(a, b), c2 = strcmp(ka, kb);
        printf("  %-12s strcoll %s 0, key strcmp %s 0 - %s\n", locales[i],
               c1 < 0 ? "<" : c1 > 0 ? ">" : "=",
               c2 < 0 ? "<" : c2 > 0 ? ">" : "=",
               ((c1 < 0) == (c2 < 0) && (c1 > 0) == (c2 > 0)) ? "agree" : "disagree");
    }
    return 0;
}
