#include <stdio.h>

struct point {
    int x;
    int y;
};

struct point moved(struct point p, int dx, int dy)
{
    return (struct point){ .x = p.x + dx, .y = p.y + dy };  /* 복합 리터럴 */
}

int main(void)
{
    struct point a = { .x = 3, .y = 4 };     /* 지정 초기화 */
    struct point b = moved(a, 10, -1);

    printf("a = (%d, %d)\n", a.x, a.y);       /* 값으로 접근: . */
    printf("b = (%d, %d)\n", b.x, b.y);

    struct point *p = &b;
    printf("p->x = %d\n", p->x);              /* 포인터로 접근: -> */

    printf("sizeof(struct point) = %zu\n", sizeof(struct point));
    return 0;
}
