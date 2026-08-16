/* 포인터가 실제로 무엇을 가리키는지 — 주소를 찍어 눈으로 확인한다.
   주소값 자체는 실행마다 다르지만(ASLR), *관계*는 언제나 같다:
   간격, 어느 것이 어디를 가리키는가, 한 겹인가 두 겹인가. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ① 포인터의 배열 대 2차원 배열 — 같은 자료, 다른 배치 */
static void two_shapes(void)
{
    /* 포인터 배열: 칸은 주소 셋, 글자는 흩어져 있다 */
    const char *names[] = { "ada", "grace", "linus" };
    /* 2차원 배열: 한 덩어리, 모든 줄이 같은 폭 */
    char table[3][8] = { "ada", "grace", "linus" };

    printf("  names: sizeof = %zu (%zu cells x %zu)\n",
           sizeof names, sizeof names / sizeof names[0], sizeof names[0]);
    for (size_t i = 0; i < 3; i++)
        printf("    names[%zu] -> %p  \"%s\" (%zu bytes)\n",
               i, (const void *)names[i], names[i], strlen(names[i]) + 1);

    printf("  table: sizeof = %zu (%zu rows x %zu)\n",
           sizeof table, sizeof table / sizeof table[0], sizeof table[0]);
    for (size_t i = 0; i < 3; i++)
        printf("    &table[%zu] = %p  \"%s\"  (step %td)\n",
               i, (const void *)table[i], table[i],
               i == 0 ? (ptrdiff_t)0 : table[i] - table[i - 1]);
}

/* ② 포인터의 포인터 — 남의 포인터를 고치는 자리 */
static int take_buffer(char **out, size_t n)
{
    char *buf = malloc(n);
    if (buf == NULL)
        return -1;                 /* 실패하면 *out 을 건드리지 않는다 */
    memset(buf, 'x', n - 1);
    buf[n - 1] = '\0';
    *out = buf;                    /* ← 부르는 쪽의 변수에 쓴다 */
    return 0;
}

/* 같은 일을 한 겹으로 적으면 --- 사본만 바뀌고 원본은 그대로다.
   (`out` 을 쓰고 나서 읽지 않는다는 경고를 피하려고 한 번 들여다본다 ---
    바로 그 「쓴 값이 밖으로 나가지 않는다」가 이 함수의 요점이다.) */
static void does_nothing(char *out, size_t n)
{
    char *buf = malloc(n);
    if (buf != NULL) {
        memset(buf, 'y', n - 1);
        buf[n - 1] = '\0';
        out = buf;                 /* 이 대입은 이 함수 안에서 끝난다 */
        printf("    (inside: the local copy now points at %p)\n", (void *)out);
        free(buf);
    }
}

static void out_parameter(void)
{
    char *p = NULL;

    printf("  before the call: p = %p, and p lives at &p = %p\n",
           (void *)p, (void *)&p);
    does_nothing(p, 8);
    printf("  after passing one level:  p = %p (unchanged)\n", (void *)p);

    if (take_buffer(&p, 8) == 0) {
        printf("  after passing two levels: p = %p \"%s\"\n", (void *)p, p);
        free(p);
    }
}

/* ③ 포인터 배열을 정렬하면 — 자료는 그대로, 순서만 바뀐다 */
static int by_text(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void sort_without_moving(void)
{
    const char *v[] = { "linus", "ada", "grace" };
    const void *before[3];

    for (size_t i = 0; i < 3; i++)
        before[i] = (const void *)v[i];
    qsort(v, 3, sizeof v[0], by_text);

    for (size_t i = 0; i < 3; i++) {
        int moved = 1;
        for (size_t j = 0; j < 3; j++)
            if (before[j] == (const void *)v[i])
                moved = 0;
        printf("    v[%zu] -> %p \"%s\"%s\n", i, (const void *)v[i], v[i],
               moved ? "  (a new address?!)" : "");
    }
    puts("    the three addresses are the original ones - not one byte of text moved.");
}

int main(void)
{
    printf("[an array of pointers vs a 2-D array]\n");
    two_shapes();

    printf("\n[a pointer to a pointer - changing the caller's variable]\n");
    out_parameter();

    printf("\n[sorting an array of pointers moves pointers, not data]\n");
    sort_without_moving();
    return 0;
}
