/* 넘침을 값이 아니라 "일어났는가"로 받는다 — C23 <stdckdint.h> */
#include <limits.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* 흔한 할당 계산: 원소 n 개 x 크기 sz. 곱셈이 넘치면 그릇이 작아진다. */
static void *alloc_array(size_t n, size_t sz)
{
    size_t bytes;
    if (ckd_mul(&bytes, n, sz)) {       /* 참이면 넘쳤다 */
        printf("  the size arithmetic overflowed - nothing is allocated\n");
        return NULL;
    }
    return malloc(bytes);
}

int main(void)
{
    /* 판정과 결과 읽기를 반드시 갈라 적는다 (한 표현식에 섞으면 순서가 없다) */
    int r;
    bool over = ckd_add(&r, INT_MAX, 1);
    printf("INT_MAX = %d\n", INT_MAX);
    printf("ckd_add(INT_MAX, 1)  overflow? %s   r = %d (the wrapped value)\n", over ? "yes" : "no", r);

    over = ckd_add(&r, 1, 2);
    printf("ckd_add(1, 2)        overflow? %s   r = %d\n", over ? "yes" : "no", r);

    /* 타입이 섞여도 수학적 값으로 판정한다 */
    signed char c;
    over = ckd_add(&c, 200, 100);
    printf("signed char <- 300   overflow? %s   c = %d\n", over ? "yes" : "no", c);

    /* 부호 없는 뺄셈: 감아 도는 것은 정의된 동작이지만, 여기서도 "넘쳤다"고 알린다 */
    unsigned u;
    over = ckd_sub(&u, 3u, 5u);
    printf("unsigned  <- 3 - 5   overflow? %s   u = %u\n", over ? "yes" : "no", u);

    printf("\nsize arithmetic for an allocation\n");
    void *ok = alloc_array(1000, sizeof(int));
    printf("  1000 x %zu -> %s\n", sizeof(int), ok ? "allocated" : "refused");
    free(ok);
    void *bad = alloc_array(SIZE_MAX / 2, sizeof(int));
    printf("  SIZE_MAX/2 x %zu -> %s\n", sizeof(int), bad ? "allocated" : "refused");
    free(bad);
    return 0;
}
