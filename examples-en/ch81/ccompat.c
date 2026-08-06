#include <stdio.h>
#include <stdlib.h>

/* Everything below is correct C. Put it into a C++ compiler as it stands,
   though, and a good deal of it will not compile. This example builds as C only. */

struct point { int x, y; };

/* (1) a C++ reserved word is an ordinary name in C */
static int class_of(int n) { return n / 10; }

int main(void)
{
    /* (2) the implicit conversion from void* to another pointer — allowed in C, refused in C++ */
    int *buf = malloc(4 * sizeof *buf);
    if (!buf) return 1;
    for (int i = 0; i < 4; i++) buf[i] = i * i;

    /* (3) the type of a character constant: in C it is int */
    printf("sizeof('a') = %zu  (in C++ it comes out 1)\n", sizeof('a'));

    /* (4) designated initialisers out of order — allowed since C99 */
    struct point p = { .y = 2, .x = 1 };
    printf("point = (%d, %d)\n", p.x, p.y);

    /* (5) a struct tag lives in its own name space, so struct must be written */
    struct point q = p;
    printf("copy  = (%d, %d)\n", q.x, q.y);

    printf("class_of(37) = %d\n", class_of(37));
    printf("buf = %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);

    free(buf);
    return 0;
}
