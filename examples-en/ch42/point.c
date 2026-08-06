#include <stdio.h>

struct point {
    int x;
    int y;
};

struct point moved(struct point p, int dx, int dy)
{
    return (struct point){ .x = p.x + dx, .y = p.y + dy };  /* a compound literal */
}

int main(void)
{
    struct point a = { .x = 3, .y = 4 };     /* designated initialisation */
    struct point b = moved(a, 10, -1);

    printf("a = (%d, %d)\n", a.x, a.y);       /* access through a value: . */
    printf("b = (%d, %d)\n", b.x, b.y);

    struct point *p = &b;
    printf("p->x = %d\n", p->x);              /* access through a pointer: -> */

    printf("sizeof(struct point) = %zu\n", sizeof(struct point));
    return 0;
}
