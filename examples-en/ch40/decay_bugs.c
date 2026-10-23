#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof (a) / sizeof (a)[0])

static size_t nonzero(const unsigned char *v, size_t n)
{
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        k += v[i] != 0;
    return k;
}

/* the wrong wipe: key is a parameter, so a pointer; sizeof key is its size */
static void wipe_wrong(unsigned char key[32])
{
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsizeof-pointer-memaccess"
#  pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
    memset(key, 0, sizeof key);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

/* the right wipe: the length comes separately */
static void wipe(unsigned char *key, size_t n)
{
    memset(key, 0, n);
}

/* copies n elements --- n counts elements, not bytes */
static void copy_n(int *dst, const int *src, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[i] = src[i];
}

int main(void)
{
    unsigned char key[32];

    puts("[1] wiping a secret through a decayed pointer");
    memset(key, 'K', sizeof key);
    wipe_wrong(key);
    printf("after wipe_wrong: %zu of %zu bytes still secret\n",
           nonzero(key, sizeof key), sizeof key);
    wipe(key, sizeof key);
    printf("after wipe      : %zu of %zu bytes still secret\n",
           nonzero(key, sizeof key), sizeof key);

    puts("\n[2] bytes are not elements");
    int src[16] = {0}, dst[16];
    printf("sizeof dst = %zu (bytes)   COUNT(dst) = %zu (elements)\n",
           sizeof dst, COUNT(dst));
    copy_n(dst, src, COUNT(dst));       /* copy_n(dst, src, sizeof dst) */
    puts("copy_n(dst, src, COUNT(dst)) : 16 elements, in bounds");
    printf("copy_n(dst, src, sizeof dst) would write %zu elements"
           " into %zu\n", sizeof dst, COUNT(dst));   /* four times too many */
    return 0;
}
