/* 포인터 산술은 바이트가 아니라 "칸" 단위로 움직인다. */
#include <stddef.h>
#include <stdio.h>

struct point { int x, y; };

int main(void)
{
    char          c[4];
    int           a[8] = {0, 10, 20, 30, 40, 50, 60, 70};
    double        d[4];
    struct point  s[4];

    /* ── ① 같은 +1 인데 움직인 폭이 다르다 ─────────────────────── */
    printf("type          sizeof   (p+1) - p bytes\n");
    printf("char           %2zu       %td\n",
           sizeof c[0], (char *)(c + 1) - (char *)c);
    printf("int            %2zu       %td\n",
           sizeof a[0], (char *)(a + 1) - (char *)a);
    printf("double         %2zu       %td\n",
           sizeof d[0], (char *)(d + 1) - (char *)d);
    printf("struct point   %2zu       %td\n",
           sizeof s[0], (char *)(s + 1) - (char *)s);

    /* 바이트 단위로 움직이려면 문자 포인터로 캐스트한다 */
    int *p = a;
    printf("\np + 1        points at %d\n", *(p + 1));
    printf("(char*)p + 1 is the second byte of a[0] (dereference it and you get a piece of the representation, not a value)\n");

    /* ── ② 포인터 뺄셈은 원소 개수를 준다 ──────────────────────── */
    ptrdiff_t gap = &a[4] - &a[1];
    printf("\n&a[4] - &a[1]        = %td  (elements, not bytes)\n", gap);
    printf("counted in bytes     = %td\n",
           (char *)&a[4] - (char *)&a[1]);

    /* ── ③ 첨자는 산술의 당의정이다 ────────────────────────────── */
    printf("\na[3] = %d, *(a + 3) = %d, *(3 + a) = %d, 3[a] = %d\n",
           a[3], *(a + 3), *(3 + a), 3[a]);

    /* ── ④ 마지막 다음 자리는 만들어도 되지만 따라가면 안 된다 ─── */
    int *end = a + 8;                 /* one-past-the-end — 합법 */
    printf("\nelements up to one-past-the-end: %td (never dereferenced)\n", end - a);

    int count = 0;
    for (int *q = a; q != end; q++)   /* 표준 관용구: != end 로 멈춘다 */
        count += (*q != 0);
    printf("nonzero elements: %d\n", count);
    return 0;
}
