/* What actually happens when a struct with a flexible array member is assigned.
   Nothing stops you --- which is what makes it dangerous. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the array begins where sizeof ends --- offsetof(12) == sizeof(12) */
struct s1 { uint16_t a; uint32_t b; uint16_t c; uint32_t data[]; };
/* the first two elements reach into sizeof --- offsetof(10) < sizeof(12) */
struct s2 { uint16_t a; uint32_t b; uint16_t c; char data[]; };

#define NEED(n) (offsetof(struct s2, data) + (n) * sizeof(char))

static struct s2 *make(size_t n, char fill)
{
    struct s2 *p = calloc(1, NEED(n) > sizeof(struct s2) ? NEED(n) : sizeof(struct s2));
    if (!p) { perror("calloc"); exit(1); }
    memset(p->data, fill, n);
    return p;
}

int main(void)
{
    size_t n = 3;
    puts("how many elements fall inside sizeof? that is what decides everything:");
    printf("  s1: offsetof(data)=%zu sizeof=%zu -> %zu element(s) inside\n",
           offsetof(struct s1, data), sizeof(struct s1),
           (sizeof(struct s1) - offsetof(struct s1, data)) / sizeof(uint32_t));
    printf("  s2: offsetof(data)=%zu sizeof=%zu -> %zu element(s) inside\n",
           offsetof(struct s2, data), sizeof(struct s2),
           sizeof(struct s2) - offsetof(struct s2, data));

    puts("\n(0) assigning an s1: the array begins where sizeof ends");
    struct s1 *x = calloc(1, offsetof(struct s1, data) + n * sizeof(uint32_t));
    struct s1 *y = calloc(1, offsetof(struct s1, data) + n * sizeof(uint32_t));
    if (!x || !y) { perror("calloc"); return 1; }
    x->a = 1; y->a = 9;
    for (size_t i = 0; i < n; i++) { x->data[i] = 100 + (uint32_t)i;
                                     y->data[i] = 900 + (uint32_t)i; }
    *y = *x;
    printf("  y after *y = *x: a=%u data=%u %u %u   <- the array was left alone\n",
           y->a, y->data[0], y->data[1], y->data[2]);
    free(x); free(y);

    printf("\nnow s2, where two elements fall inside sizeof.\n");
    printf("three elements need %zu bytes\n", NEED(n));

    struct s2 *src = make(n, '?'), *dst = make(n, '.');
    src->a = 1; src->b = 2; src->c = 3;
    memcpy(src->data, "ABC", n);

    puts("\n(1) plain struct assignment: *dst = *src");
    *dst = *src;
    printf("  members : a=%u b=%u c=%u   <- copied\n", dst->a, dst->b, dst->c);
    printf("  data    : %.3s              <- source was ABC, destination was ...\n", dst->data);
    puts("  the standard says only the named members are copied, and array elements");
    puts("  inside the first sizeof bytes get an indeterminate representation");
    puts("  (C23 6.7.3.2 p28) -- they may or may not match the source. Never rely on it.");

    puts("\n(2) the same mistake spelled with memcpy: memcpy(dst, src, sizeof *dst)");
    struct s2 *dst2 = make(n, '.');
    memcpy(dst2, src, sizeof *dst2);
    printf("  data    : %.3s              <- the same half copy, for the same reason\n",
           dst2->data);

    puts("\n(3) the correct byte copy: memcpy(dst, src, offsetof(...) + n * sizeof(data[0]))");
    struct s2 *dst3 = make(n, '.');
    memcpy(dst3, src, NEED(n));
    printf("  members : a=%u b=%u c=%u\n", dst3->a, dst3->b, dst3->c);
    printf("  data    : %.3s              <- all of it\n", dst3->data);

    puts("\n(4) and one more trap: an object smaller than its own type");
    printf("  one element needs %zu bytes, but sizeof(struct s2) is %zu\n",
           NEED(1), sizeof(struct s2));
    puts("  a struct assignment there would touch bytes past the end of the allocation");
    puts("  -- undefined behaviour, and ASan reports it as a heap-buffer-overflow.");

    free(src); free(dst); free(dst2); free(dst3);
    return 0;
}
