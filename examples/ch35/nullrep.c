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
    printf("   모든 비트 0 인가: %s\n", all_zero ? "그렇다" : "아니다");
}

struct node {
    int          id;
    struct node *next;      /* 포인터 멤버 */
    double       weight;    /* 부동소수 멤버 */
};

int main(void)
{
    printf("이 구현의 포인터 크기: %zu바이트\n\n", sizeof(int *));

    /* ① 세 가지 표기는 모두 '널 포인터 상수'다 — 문법의 층 */
    puts("[같은 널을 세 가지로 적어 담아 본다]");
    int *a = 0;          dump("int *a = 0;",       &a, sizeof a);
    int *b = NULL;       dump("int *b = NULL;",    &b, sizeof b);
    int *c = nullptr;    dump("int *c = nullptr;", &c, sizeof c);
    printf("  셋이 서로 같은가: %s\n",
           (a == b && b == c) ? "그렇다 — 표준이 약속한다" : "아니다");

    /* 0L 과 (void *)0 도 널 포인터 상수다. 반면 '값이 0인 변수'는 아니다:
         const int zero = 0;
         int *p = zero;        ← 컴파일 오류. 상수식이 아니라 변수이기 때문. */
    int *d = 0L;         dump("int *d = 0L;",        &d, sizeof d);
    int *e = (void *)0;  dump("int *e = (void *)0;", &e, sizeof e);

    /* ② 비교와 대입은 언제나 옳다 — 기계 속 표현이 무엇이든 */
    puts("\n[비교는 표현과 무관하게 성립한다]");
    printf("  a == 0      : %s\n", a == 0       ? "참" : "거짓");
    printf("  a == NULL   : %s\n", a == NULL    ? "참" : "거짓");
    printf("  a == nullptr: %s\n", a == nullptr ? "참" : "거짓");
    printf("  !a          : %s\n", !a           ? "참" : "거짓");

    /* ③ 구조체를 0 으로 채우는 두 가지 방법 — 뜻이 다르다 */
    puts("\n[구조체를 '비우는' 두 방법]");
    struct node x = { 0 };              /* 값의 층: 널 포인터와 0.0 을 약속 */
    struct node y;
    memset(&y, 0, sizeof y);            /* 표현의 층: 모든 비트를 0 으로 */

    dump("{ 0 } 의 next", &x.next, sizeof x.next);
    dump("memset 의 next", &y.next, sizeof y.next);
    printf("  x.next == nullptr : %s   ← 표준이 약속한다\n",
           x.next == nullptr ? "참" : "거짓");
    printf("  x.weight == 0.0   : %s   ← 표준이 약속한다\n",
           x.weight == 0.0 ? "참" : "거짓");
    printf("  y.next == nullptr : %s   ← 이 구현에서 그럴 뿐이다\n",
           y.next == nullptr ? "참" : "거짓");

    /* ④ calloc 도 '모든 비트 0' 쪽이다 */
    puts("\n[calloc 이 주는 것도 비트 0 이다]");
    int **arr = calloc(4, sizeof *arr);
    if (!arr) { perror("calloc"); return 1; }
    dump("calloc 의 arr[0]", &arr[0], sizeof arr[0]);
    printf("  arr[0] == nullptr : %s   ← 이 구현에서 그럴 뿐이다\n",
           arr[0] == nullptr ? "참" : "거짓");
    free(arr);

    puts("\n정리: 소스에 적는 0 은 *표기*이고, 기억에 담기는 비트는 *표현*이다.");
    puts("      비교·대입은 표기의 층이라 어디서나 옳고,");
    puts("      memset·calloc 은 표현의 층이라 널을 약속하지 않는다.");
    return 0;
}
