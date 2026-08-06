/* 실수의 속을 열어 본다 — 부호·지수·가수가 실제로 어떻게 들어 있는가.
   타입 퍼닝은 공용체가 아니라 memcpy 로 한다(36·45장의 규칙). */
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t bits_of(double d)
{
    uint64_t u;
    memcpy(&u, &d, sizeof u);          /* 표현을 그대로 옮긴다 */
    return u;
}

static uint32_t bits_of_f(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

/* double: 부호 1 + 지수 11 + 가수 52 */
static void dump(const char *label, double d)
{
    uint64_t u = bits_of(d);
    unsigned sign = (unsigned)(u >> 63);
    unsigned expo = (unsigned)((u >> 52) & 0x7FFu);
    uint64_t frac = u & 0xFFFFFFFFFFFFFu;

    printf("%-12s %016" PRIx64 "  부호 %u  지수 %4u(=%+5d)  가수 %013" PRIx64,
           label, u, sign, expo,
           expo == 0 ? -1022 : (int)expo - 1023, frac);

    if (expo == 0x7FF)      printf("  <- %s", frac ? "NaN" : "무한대");
    else if (expo == 0)     printf("  <- %s", frac ? "비정규수" : "영");
    printf("\n");
}

int main(void)
{
    puts("── double 의 비트 (부호 1 + 지수 11 + 가수 52) ──");
    dump("1.0", 1.0);
    dump("-1.0", -1.0);
    dump("0.5", 0.5);
    dump("2.0", 2.0);
    dump("0.1", 0.1);
    dump("0.3", 0.3);
    dump("0.1+0.2", 0.1 + 0.2);
    dump("0.0", 0.0);
    dump("-0.0", -0.0);
    dump("inf", INFINITY);
    dump("NaN", NAN);

    puts("\n── 0.1 + 0.2 와 0.3 은 비트가 다르다 ──");
    printf("0.1+0.2 = %.20f\n", 0.1 + 0.2);
    printf("0.3     = %.20f\n", 0.3);
    printf("차이의 비트: %016" PRIx64 " vs %016" PRIx64 "  (마지막 1비트)\n",
           bits_of(0.1 + 0.2), bits_of(0.3));

    puts("\n── 1.0 에서 한 눈금(ULP) 움직이면 가수의 끝자리가 1 오른다 ──");
    double one = 1.0;
    double next = nextafter(1.0, 2.0);
    printf("1.0        %016" PRIx64 "\n", bits_of(one));
    printf("다음 값    %016" PRIx64 "  차이 %.17g\n", bits_of(next), next - one);
    printf("DBL_EPSILON 과 같은가? %s\n",
           (next - one) == 0x1p-52 ? "그렇다" : "아니다");

    puts("\n── float 은 같은 구조를 좁게 쓴다 (부호 1 + 지수 8 + 가수 23) ──");
    float f = 0.1f;
    uint32_t fu = bits_of_f(f);
    printf("0.1f       %08" PRIx32 "  부호 %u  지수 %3u(=%+4d)  가수 %06" PRIx32 "\n",
           fu, fu >> 31, (fu >> 23) & 0xFFu, (int)((fu >> 23) & 0xFFu) - 127,
           fu & 0x7FFFFFu);
    printf("(double)0.1f 를 다시 찍으면 %.17g — float 로 좁힌 흔적이 남는다\n",
           (double)f);

    puts("\n── 정규수의 바닥을 지나면 비정규수가 된다 ──");
    double small = 0x1p-1022;          /* 가장 작은 정규수 */
    dump("2^-1022", small);
    dump("half", small / 2);           /* 비정규수 */
    dump("2^-1074", 0x1p-1074);        /* 가장 작은 비정규수 */
    dump("half again", 0x1p-1074 / 2); /* 0 으로 가라앉는다 */
    return 0;
}
