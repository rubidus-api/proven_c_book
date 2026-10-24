#define __STDC_WANT_LIB_EXT1__ 1
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdckdint.h>
#include <stdalign.h>
#include <stddef.h>

int main(void)
{
    /* ① 부속서 K 가 실제로 있는가 — 구현이 알려 준다 */
#ifdef __STDC_LIB_EXT1__
    printf("Annex K: present (__STDC_LIB_EXT1__)\n");
#else
    printf("Annex K: not in this implementation\n");
#endif

    /* ② C23 의 검사 산술 — 넘침을 값으로 알려 준다 */
    int a = 2000000000, b = 2000000000, sum;
    if (ckd_add(&sum, a, b))
        printf("ckd_add: overflowed (it does not fit in an int)\n");
    else
        printf("ckd_add: %d\n", sum);

    size_t count = (size_t)-1 / 2, elem = 8, bytes;
    if (ckd_mul(&bytes, count, elem))
        printf("ckd_mul: overflowed - no allocation is attempted\n");

    /* ③ 고정 폭 정수와 정렬 */
    printf("int32_t=%zu bytes, alignof(max_align_t)=%zu\n",
           sizeof(int32_t), alignof(max_align_t));

    /* ④ C23 의 bool 은 키워드다 — <stdbool.h> 없이 쓴다 */
    bool ready = true;
    printf("bool is a keyword: %s\n", ready ? "true" : "false");

    /* ⑤ 표준이 정한 매크로로 판을 확인한다 */
    printf("__STDC_VERSION__ = %ldL\n", (long)__STDC_VERSION__);
    return 0;
}
