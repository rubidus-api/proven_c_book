/* Return a struct with a flexible array member *by value*: what happens to the
   receiver's array? Nothing stops you --- but only sizeof bytes travel. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct s2 { uint16_t a; uint32_t b; uint16_t c; char data[]; };

/* returning it by value --- is that even valid? */
static struct s2 make(void)
{
    struct s2 v;                 /* a declared object: 12 bytes */
    memset(&v, 0, sizeof v);
    v.a = 1; v.b = 2; v.c = 3;
    /* sizeof(12) >= offsetof(10) + 1, so data[0] and data[1] fit inside it */
    v.data[0] = 'A'; v.data[1] = 'B';
    return v;
}

int main(void)
{
    printf("sizeof=%zu, offsetof(data)=%zu -> a declared object has room for %zu element(s)\n",
           sizeof(struct s2), offsetof(struct s2, data),
           sizeof(struct s2) - offsetof(struct s2, data));

    /* (1) receive into a declared variable */
    struct s2 v;
    memset(&v, 0, sizeof v);
    v.data[0] = 'x'; v.data[1] = 'y';
    v = make();
    printf("(1) into a declared variable: a=%u b=%u c=%u data[0]=%c data[1]=%c"
           "   (they were x y)\n",
           v.a, v.b, v.c, v.data[0], v.data[1]);

    /* (2) receive by assigning into an allocation that already holds three */
    size_t n = 3, need = offsetof(struct s2, data) + n;
    struct s2 *p = malloc(need);
    p->a = p->b = p->c = 9; memcpy(p->data, "PQR", n);
    *p = make();
    printf("(2) into a 13-byte allocation: a=%u b=%u c=%u data=%.3s   (it was PQR)\n",
           p->a, p->b, p->c, p->data);
    free(p);

    puts("\n  what actually happened: sizeof bytes were copied.");
    puts("  data[0] and data[1] live inside those bytes, so they were overwritten;");
    puts("  data[2] lives past them, so it kept its old value.");
    puts("  the standard promises neither: elements inside the first sizeof bytes");
    puts("  are left with an indeterminate representation (C23 6.7.3.2 p28),");
    puts("  and gcc itself notes that this ABI changed in GCC 4.4.");
    return 0;
}
