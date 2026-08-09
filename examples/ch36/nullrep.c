/* 소스의 0 과 기억 속의 비트 — 널 포인터의 '표기'와 '표현'은 다른 층이다. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 포인터 하나의 바이트를 그대로 들여다본다 */
static void dump(const char *how, const void *pobj, size_t n)
{
    const unsigned char *b = pobj;
    int all_zero = 1;

    printf("  %-22s", how);
    for (size_t i = 0; i < n; i++) {
        printf(" %02X", b[i]);
        if (b[i] != 0) all_zero = 0;
    }
    printf("   all bits zero: %s\n", all_zero ? "yes" : "no");
}

struct node {
    int          id;
    struct node *next;      /* 포인터 멤버 */
    double       weight;    /* 부동소수 멤버 */
};

int main(void)
{
    printf("pointer size on this implementation: %zu bytes\n\n", sizeof(int *));

    /* ① 세 가지 표기는 모두 '널 포인터 상수'다 — 문법의 층 */
    puts("[the same null written three ways]");
    int *a = 0;          dump("int *a = 0;",       &a, sizeof a);
    int *b = NULL;       dump("int *b = NULL;",    &b, sizeof b);
    int *c = nullptr;    dump("int *c = nullptr;", &c, sizeof c);
    printf("  are all three equal: %s\n",
           (a == b && b == c) ? "yes - the standard promises it" : "no");

    /* 0L 과 (void *)0 도 널 포인터 상수다. 반면 '값이 0인 변수'는 아니다:
         const int zero = 0;
         int *p = zero;        ← 컴파일 오류. 상수식이 아니라 변수이기 때문. */
    int *d = 0L;         dump("int *d = 0L;",        &d, sizeof d);
    int *e = (void *)0;  dump("int *e = (void *)0;", &e, sizeof e);

    /* ② 비교와 대입은 언제나 옳다 — 기계 속 표현이 무엇이든 */
    puts("\n[comparison holds whatever the representation]");
    printf("  a == 0      : %s\n", a == 0       ? "true" : "false");
    printf("  a == NULL   : %s\n", a == NULL    ? "true" : "false");
    printf("  a == nullptr: %s\n", a == nullptr ? "true" : "false");
    printf("  !a          : %s\n", !a           ? "true" : "false");

    /* ③ 구조체를 0 으로 채우는 두 가지 방법 — 뜻이 다르다 */
    puts("\n[two ways to 'empty' a struct]");
    struct node x = { 0 };              /* 값의 층: 널 포인터와 0.0 을 약속 */
    struct node y;
    memset(&y, 0, sizeof y);            /* 표현의 층: 모든 비트를 0 으로 */

    dump("next of { 0 }", &x.next, sizeof x.next);
    dump("next of memset", &y.next, sizeof y.next);
    printf("  x.next == nullptr : %s   <- the standard promises it\n",
           x.next == nullptr ? "true" : "false");
    printf("  x.weight == 0.0   : %s   <- the standard promises it\n",
           x.weight == 0.0 ? "true" : "false");
    printf("  y.next == nullptr : %s   <- true on this implementation, that is all\n",
           y.next == nullptr ? "true" : "false");

    /* ④ calloc 도 '모든 비트 0' 쪽이다 */
    puts("\n[what calloc gives is all-bits-zero too]");
    int **arr = calloc(4, sizeof *arr);
    if (!arr) { perror("calloc"); return 1; }
    dump("arr[0] of calloc", &arr[0], sizeof arr[0]);
    printf("  arr[0] == nullptr : %s   <- true on this implementation, that is all\n",
           arr[0] == nullptr ? "true" : "false");
    free(arr);

    puts("\nin short: the 0 you write in the source is *notation*; what lands in memory is *representation*.");
    puts("        comparison and assignment live at the notation layer, so they are always right,");
    puts("        while memset and calloc live at the representation layer and promise no null.");
    return 0;
}
