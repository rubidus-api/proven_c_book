#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

int main(void)
{
    /* (1) NaN is not even equal to itself */
    double nan_v = nan("");
    printf("NaN == NaN : %s\n", nan_v == nan_v ? "true" : "false");
    printf("isnan(NaN) : %s\n", isnan(nan_v) ? "true" : "false");

    /* (2) infinity and division by zero (for reals this is not UB) */
    double inf_v = 1.0 / 0.0;
    printf("1.0/0.0    : %f (isinf: %d)\n", inf_v, isinf(inf_v));
    printf("0.0/0.0    : %s\n", isnan(0.0 / 0.0) ? "NaN" : "a number");

    /* (3) a call outside the domain reports through errno and NaN */
    errno = 0;
    double r = sqrt(-1.0);
    printf("sqrt(-1)   : %s, errno=%s\n",
           isnan(r) ? "NaN" : "a number", errno == EDOM ? "EDOM" : "0");

    /* (4) beyond the range: ERANGE and infinity */
    errno = 0;
    double big = exp(1000.0);
    printf("exp(1000)  : %s, errno=%s\n",
           isinf(big) ? "inf" : "a number", errno == ERANGE ? "ERANGE" : "0");

    /* (5) the classification function sorts values by kind */
    double vals[] = { 1.0, 0.0, -0.0, inf_v, nan_v, 1e-320 };
    const char *names[] = { "1.0", "0.0", "-0.0", "inf", "NaN", "1e-320" };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        const char *kind = "?";
        switch (fpclassify(vals[i])) {
            case FP_NORMAL:    kind = "normal";    break;
            case FP_SUBNORMAL: kind = "subnormal"; break;
            case FP_ZERO:      kind = "0";         break;
            case FP_INFINITE:  kind = "infinite";  break;
            case FP_NAN:       kind = "NaN";       break;
        }
        printf("  %-7s -> %-9s (sign bit %d)\n", names[i], kind, signbit(vals[i]) != 0);
    }

    /* (6) 0.0 and -0.0 compare equal, but their signs differ */
    printf("0.0 == -0.0 : %s\n", 0.0 == -0.0 ? "true" : "false");
    return 0;
}
