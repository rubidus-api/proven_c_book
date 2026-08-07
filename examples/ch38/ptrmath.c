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
    printf("타입          sizeof   (p+1) - p 바이트\n");
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
    printf("\np + 1        은 %d 을 가리킨다\n", *(p + 1));
    printf("(char*)p + 1 은 a[0] 의 두 번째 바이트다 (역참조하면 값이 아니라 표현의 조각)\n");

    /* ── ② 포인터 뺄셈은 원소 개수를 준다 ──────────────────────── */
    ptrdiff_t gap = &a[4] - &a[1];
    printf("\n&a[4] - &a[1]        = %td  (바이트가 아니라 원소 수)\n", gap);
    printf("바이트로 세면        = %td\n",
           (char *)&a[4] - (char *)&a[1]);

    /* ── ③ 첨자는 산술의 당의정이다 ────────────────────────────── */
    printf("\na[3] = %d, *(a + 3) = %d, *(3 + a) = %d, 3[a] = %d\n",
           a[3], *(a + 3), *(3 + a), 3[a]);

    /* ── ④ 마지막 다음 자리는 만들어도 되지만 따라가면 안 된다 ─── */
    int *end = a + 8;                 /* one-past-the-end — 합법 */
    printf("\n끝 다음 주소까지의 원소 수: %td (역참조는 하지 않는다)\n", end - a);

    int count = 0;
    for (int *q = a; q != end; q++)   /* 표준 관용구: != end 로 멈춘다 */
        count += (*q != 0);
    printf("0 이 아닌 원소: %d 개\n", count);
    return 0;
}
