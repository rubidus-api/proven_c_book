#include <stdarg.h>
#include <stdio.h>

/* 개수를 첫 인자로 받는 방식 — 스스로는 개수를 알 길이 없다 */
int sum_n(int count, ...)
{
    va_list ap;
    int total = 0;

    va_start(ap, count);            /* count 다음부터 읽겠다 */
    for (int i = 0; i < count; i += 1) {
        total += va_arg(ap, int);   /* 타입을 '내가' 알려 준다 */
    }
    va_end(ap);                     /* 반드시 닫는다 */
    return total;
}

/* 끝 표지(sentinel)로 개수를 대신하는 방식 */
int sum_until_zero(int first, ...)
{
    va_list ap;
    int total = first;

    va_start(ap, first);
    for (;;) {
        int v = va_arg(ap, int);
        if (v == 0) { break; }
        total += v;
    }
    va_end(ap);
    return total;
}

int main(void)
{
    printf("sum_n(4, 1,2,3,4)        = %d\n", sum_n(4, 1, 2, 3, 4));
    printf("sum_until_zero(1,2,3,0)  = %d\n", sum_until_zero(1, 2, 3, 0));
    return 0;
}
