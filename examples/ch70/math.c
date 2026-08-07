#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    /* ① NaN 은 자기 자신과도 같지 않다 */
    double nan_v = nan("");
    printf("NaN == NaN : %s\n", nan_v == nan_v ? "참" : "거짓");
    printf("isnan(NaN) : %s\n", isnan(nan_v) ? "참" : "거짓");

    /* ② 무한대와 0 나누기(실수는 UB 가 아니다) */
    double inf_v = 1.0 / 0.0;
    printf("1.0/0.0    : %f (isinf: %d)\n", inf_v, isinf(inf_v));
    printf("0.0/0.0    : %s\n", isnan(0.0 / 0.0) ? "NaN" : "수");

    /* ③ 정의역 밖 호출은 errno 와 NaN 으로 알려 온다 */
    errno = 0;
    double r = sqrt(-1.0);
    printf("sqrt(-1)   : %s, errno=%s\n",
           isnan(r) ? "NaN" : "수", errno == EDOM ? "EDOM" : "0");

    /* ④ 범위를 넘으면 ERANGE 와 무한대 */
    errno = 0;
    double big = exp(1000.0);
    printf("exp(1000)  : %s, errno=%s\n",
           isinf(big) ? "inf" : "수", errno == ERANGE ? "ERANGE" : "0");

    /* ⑤ 분류 함수로 값의 종류를 가른다 */
    double vals[] = { 1.0, 0.0, -0.0, inf_v, nan_v, 1e-320 };
    const char *names[] = { "1.0", "0.0", "-0.0", "inf", "NaN", "1e-320" };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        const char *kind = "?";
        switch (fpclassify(vals[i])) {
            case FP_NORMAL:    kind = "정규";      break;
            case FP_SUBNORMAL: kind = "비정규";    break;
            case FP_ZERO:      kind = "0";         break;
            case FP_INFINITE:  kind = "무한";      break;
            case FP_NAN:       kind = "NaN";       break;
        }
        printf("  %-7s → %-6s (부호비트 %d)\n", names[i], kind, signbit(vals[i]) != 0);
    }

    /* ⑥ 0.0 과 -0.0 은 같다고 비교되지만 부호는 다르다 */
    printf("0.0 == -0.0 : %s\n", 0.0 == -0.0 ? "참" : "거짓");
    return 0;
}
