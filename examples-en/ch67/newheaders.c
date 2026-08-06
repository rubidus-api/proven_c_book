#define __STDC_WANT_LIB_EXT1__ 1
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdckdint.h>
#include <stdalign.h>
#include <stddef.h>

int main(void)
{
    /* (1) is annex K really there — the implementation tells us */
#ifdef __STDC_LIB_EXT1__
    printf("annex K: present (__STDC_LIB_EXT1__)\n");
#else
    printf("annex K: not in this implementation\n");
#endif

    /* (2) C23's checked arithmetic — overflow reported as a value */
    int a = 2000000000, b = 2000000000, sum;
    if (ckd_add(&sum, a, b))
        printf("ckd_add: it overflowed (an int cannot hold it)\n");
    else
        printf("ckd_add: %d\n", sum);

    size_t count = (size_t)-1 / 2, elem = 8, bytes;
    if (ckd_mul(&bytes, count, elem))
        printf("ckd_mul: it overflowed — no allocation is attempted\n");

    /* (3) fixed-width integers and alignment */
    printf("int32_t=%zu bytes, alignof(max_align_t)=%zu\n",
           sizeof(int32_t), alignof(max_align_t));

    /* (4) C23's bool is a keyword — used without <stdbool.h> */
    bool ready = true;
    printf("bool is a keyword: %s\n", ready ? "true" : "false");

    /* (5) the edition confirmed through the macro the standard settles */
    printf("__STDC_VERSION__ = %ldL\n", (long)__STDC_VERSION__);
    return 0;
}
