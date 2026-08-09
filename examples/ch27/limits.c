#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <stddef.h>

int main(void)
{
    printf("--- sizes of the basic integer types (on this machine) ---\n");
    printf("char=%zu short=%zu int=%zu long=%zu long long=%zu\n",
           sizeof(char), sizeof(short), sizeof(int), sizeof(long), sizeof(long long));
    printf("size_t=%zu ptrdiff_t=%zu void*=%zu\n",
           sizeof(size_t), sizeof(ptrdiff_t), sizeof(void *));

    printf("\n--- actual ranges (<limits.h> macros) ---\n");
    printf("CHAR_BIT  = %d\n", CHAR_BIT);
    printf("SCHAR_MIN = %d, SCHAR_MAX = %d, UCHAR_MAX = %u\n",
           SCHAR_MIN, SCHAR_MAX, (unsigned)UCHAR_MAX);
    printf("SHRT_MIN  = %d, SHRT_MAX  = %d, USHRT_MAX = %u\n",
           SHRT_MIN, SHRT_MAX, (unsigned)USHRT_MAX);
    printf("INT_MIN   = %d, INT_MAX   = %d, UINT_MAX  = %u\n",
           INT_MIN, INT_MAX, UINT_MAX);
    printf("LONG_MIN  = %ld, LONG_MAX = %ld\n", LONG_MIN, LONG_MAX);
    printf("LLONG_MAX = %lld, ULLONG_MAX = %llu\n", LLONG_MAX, ULLONG_MAX);

    printf("\n--- fixed-width types (<stdint.h>) ---\n");
    printf("INT8_MAX=%d INT16_MAX=%d INT32_MAX=%d\n",
           (int)INT8_MAX, (int)INT16_MAX, (int)INT32_MAX);
    printf("INT64_MAX=%lld UINT64_MAX=%llu\n",
           (long long)INT64_MAX, (unsigned long long)UINT64_MAX);
    printf("SIZE_MAX=%zu\n", SIZE_MAX);

    printf("\n--- floating types (<float.h>) ---\n");
    printf("float : %zu bytes, %d digits, max %g\n", sizeof(float), FLT_DIG, (double)FLT_MAX);
    printf("double: %zu bytes, %d digits, max %g\n", sizeof(double), DBL_DIG, DBL_MAX);
    printf("DBL_EPSILON = %g (smallest difference distinguishable from 1.0)\n", DBL_EPSILON);
    return 0;
}
