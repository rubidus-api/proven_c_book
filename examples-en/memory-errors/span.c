/* A buffer that carries its length --- one shape that prevents overflow *by structure*.
 * Pass a single pointer and the receiver does not know the size of the container (strcpy's problem).
 * Pass pointer and length as one bundle, and every write is checked in the same place. */
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char  *data;
    size_t len;     /* bytes written */
    size_t cap;     /* size of the container */
} buf;

/* Grow the container if there is no room. On failure, false --- the old contents stay as they were. */
static bool reserve(buf *b, size_t extra)
{
    size_t need;
    if (ckd_add(&need, b->len, extra)) return false;    /* refuse if the length computation overflows */
    if (need <= b->cap) return true;

    size_t cap = b->cap ? b->cap : 8;
    while (cap < need)
        if (ckd_mul(&cap, cap, 2)) return false;

    char *p = realloc(b->data, cap);    /* receive the result in another variable first */
    if (p == NULL) return false;        /* on failure b->data is still alive (C23 7.24.3.7p3) */
    b->data = p;
    b->cap  = cap;
    return true;
}

static bool append(buf *b, const char *s)
{
    size_t n = strlen(s);
    if (!reserve(b, n)) return false;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return true;
}

/* Size computation when borrowing an array of n --- if n * size overflows, a small block comes back. */
static void *alloc_array(size_t n, size_t size)
{
    size_t bytes;
    if (ckd_mul(&bytes, n, size)) return NULL;
    return malloc(bytes);
}

int main(void)
{
    buf b = {0};
    const char *words[] = {"length ", "travels ", "with ", "the ", "pointer"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++)
        if (!append(&b, words[i])) { free(b.data); return 1; }
    printf("%.*s\n", (int)b.len, b.data);
    printf("len %zu, cap %zu\n", b.len, b.cap);
    free(b.data);

    void *huge = alloc_array(SIZE_MAX / 2, 4);  /* the product overflows size_t */
    printf("alloc_array(SIZE_MAX / 2, 4): %s\n", huge ? "got a block" : "refused (size overflow)");
    free(huge);
    return 0;
}
