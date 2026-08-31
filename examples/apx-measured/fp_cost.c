/* 수 자체의 값 --- 같은 연산인데 값이 다른 경우들.
   ★ 함정: 배열이 크면 결과가 *기억 대역폭*에 묶여 연산 차이가 안 보인다.
     (첫 판에서 곱셈·나눗셈·제곱근이 전부 1.24 나노초로 같게 나왔다 --- 16 MiB 를
      흘려보내느라 계산이 기다리고 있었던 것이다.)
     그래서 여기서는 작업 집합을 L1 안에 넣고 여러 번 되풀이한다. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <time.h>

static double ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp_d(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double med(double *s, int n) { qsort(s, (size_t)n, sizeof *s, cmp_d); return s[n / 2]; }

#define N     2048           /* 2048 × 8바이트 × 2배열 = 32 KiB --- L1 안 */
#define REP   2000           /* 되풀이 */
#define R     7

/* ★ 부동소수점 합계는 *순서를 바꾸면 결과가 달라진다*(덧셈이 결합적이지 않다).
   그래서 컴파일러는 허락 없이 벡터로 묶지 못한다. 아래 두 함수에만 그 허락을 준다. */
__attribute__((optimize("O3", "fast-math")))
static double vec_double(const double *a, const double *b, int reps)
{
    double s = 0;
    for (int r = 0; r < reps; r++) for (size_t i = 0; i < N; i++) s += a[i] * b[i];
    return s;
}
__attribute__((optimize("O3", "fast-math")))
static float vec_float(const float *a, const float *b, int reps)
{
    float s = 0;
    for (int r = 0; r < reps; r++) for (size_t i = 0; i < N; i++) s += a[i] * b[i];
    return s;
}

static volatile double dsink;
static volatile float  fsink;

int main(void)
{
    static double a[N], b[N];
    static float  fa[N], fb[N];
    double s[R];

    printf("== how this is measured ==\n");
    printf("  two arrays, %zu KiB together --- they fit in L1. Repeated %d times.\n",
           (sizeof a + sizeof b) / 1024, REP);
    printf("  four accumulators keep it from waiting on the previous result (breaking the dependency chain).\n\n");

    printf("== 1. denormals --- numbers very close to zero ==\n");
    printf("  the smallest normal double: %g\n", DBL_MIN);
    double normal_v = 0, denorm_v = 0;
    for (int mode = 0; mode < 2; mode++) {
        for (size_t i = 0; i < N; i++) {
            a[i] = mode ? DBL_MIN / 8.0 : 1.0;      /* 비정규수 : 정상 수 */
            b[i] = 1.0;                             /* 곱해도 크기가 그대로 --- 결과도 비정규 */
        }
        for (int r = 0; r < R; r++) {
            double a0 = 0, a1 = 0, a2 = 0, a3 = 0;
            double t0 = ns();
            for (int rep = 0; rep < REP; rep++)
                for (size_t i = 0; i < N; i += 4) {
                    a0 += a[i] * b[i];  a1 += a[i + 1] * b[i + 1];
                    a2 += a[i + 2] * b[i + 2]; a3 += a[i + 3] * b[i + 3];
                }
            double t1 = ns(); dsink = a0 + a1 + a2 + a3;
            s[r] = (t1 - t0) / ((double)N * REP);
        }
        double v = med(s, R);
        if (mode) denorm_v = v; else normal_v = v;
        printf("  %-28s %8.3f ns per element\n",
               mode ? "array filled with denormals" : "array filled with normals (1.0)", v);
    }
    printf("  -> the denormal side is %.1f x %s\n", denorm_v / normal_v,
           denorm_v / normal_v > 1.2 ? "slower" : "--- no great difference on this machine");
    printf("  * here the magnitude of a value alone changes the time. On some machines it is\n");
    printf("    tens of times; on recent chips it is almost nothing --- so measure before speaking.\n");
    printf("    (this is the accident where audio processing slows suddenly as a sound fades.)\n");

    printf("\n== 2. float and double ==\n");
    for (size_t i = 0; i < N; i++) {
        a[i] = 1.0 + (double)i * 1e-6; b[i] = 1.000001;
        fa[i] = (float)a[i]; fb[i] = 1.000001f;
    }
    for (int r = 0; r < R; r++) {
        double a0 = 0, a1 = 0, a2 = 0, a3 = 0, t0 = ns();
        for (int rep = 0; rep < REP; rep++)
            for (size_t i = 0; i < N; i += 4) {
                a0 += a[i] * b[i]; a1 += a[i + 1] * b[i + 1];
                a2 += a[i + 2] * b[i + 2]; a3 += a[i + 3] * b[i + 3];
            }
        double t1 = ns(); dsink = a0 + a1 + a2 + a3; s[r] = (t1 - t0) / ((double)N * REP);
    }
    double dv = med(s, R);
    for (int r = 0; r < R; r++) {
        float a0 = 0, a1 = 0, a2 = 0, a3 = 0; double t0 = ns();
        for (int rep = 0; rep < REP; rep++)
            for (size_t i = 0; i < N; i += 4) {
                a0 += fa[i] * fb[i]; a1 += fa[i + 1] * fb[i + 1];
                a2 += fa[i + 2] * fb[i + 2]; a3 += fa[i + 3] * fb[i + 3];
            }
        double t1 = ns(); fsink = a0 + a1 + a2 + a3; s[r] = (t1 - t0) / ((double)N * REP);
    }
    double fv = med(s, R);
    printf("  %-28s %8.3f ns per element\n", "double (8 bytes)", dv);
    printf("  %-28s %8.3f ns per element\n", "float  (4 bytes)", fv);
    printf("  -> the float side is %.2f x faster %s\n", dv / fv,
           dv / fv < 1.1 ? "--- odd: no difference" : "");

    /* 왜 차이가 없나 --- 벡터 명령을 쓰지 않아서다. 허락을 준 판으로 다시 잰다. */
    for (int r = 0; r < R; r++) {
        double t0 = ns(); dsink = vec_double(a, b, REP); double t1 = ns();
        s[r] = (t1 - t0) / ((double)N * REP);
    }
    double dvv = med(s, R);
    for (int r = 0; r < R; r++) {
        double t0 = ns(); fsink = vec_float(fa, fb, REP); double t1 = ns();
        s[r] = (t1 - t0) / ((double)N * REP);
    }
    double fvv = med(s, R);
    printf("\n  measuring the same computation again, this time allowing vector instructions:\n");
    printf("  %-28s %8.3f ns per element\n", "double (vectors allowed)", dvv);
    printf("  %-28s %8.3f ns per element\n", "float  (vectors allowed)", fvv);
    printf("  -> this time the float side is %.2f x faster\n", dvv / fvv);
    printf("  * the benefit of narrower width appears only with vector instructions. But\n");
    printf("    floating-point addition is not associative (reordering changes the result),\n");
    printf("    so the compiler may not regroup freely. \"float is twice as fast\" has conditions.\n");
    printf("  * do not compare across the two pairs: the scalar version above already overlapped\n");
    printf("    execution with four accumulators. Read the ratio within each pair.\n");

    printf("\n== 3. the operations cost differently ==\n");
    printf("  %-24s %-14s %s\n", "operation", "per element", "vs multiply");
    printf("#DATA-BEGIN\n");
    double base_op = 0;
    for (int k = 0; k < 5; k++) {
        for (int r = 0; r < R; r++) {
            double a0 = 0, a1 = 0, a2 = 0, a3 = 0, t0 = ns();
            for (int rep = 0; rep < REP; rep++)
                for (size_t i = 0; i < N; i += 4) {
                    switch (k) {
                    case 0: a0 += a[i] * b[i]; a1 += a[i+1] * b[i+1];
                            a2 += a[i+2] * b[i+2]; a3 += a[i+3] * b[i+3]; break;
                    case 1: a0 += a[i] + b[i]; a1 += a[i+1] + b[i+1];
                            a2 += a[i+2] + b[i+2]; a3 += a[i+3] + b[i+3]; break;
                    case 2: a0 += a[i] / b[i]; a1 += a[i+1] / b[i+1];
                            a2 += a[i+2] / b[i+2]; a3 += a[i+3] / b[i+3]; break;
                    case 3: a0 += sqrt(a[i]); a1 += sqrt(a[i+1]);
                            a2 += sqrt(a[i+2]); a3 += sqrt(a[i+3]); break;
                    default: a0 += sin(a[i]); a1 += sin(a[i+1]);
                             a2 += sin(a[i+2]); a3 += sin(a[i+3]); break;
                    }
                }
            double t1 = ns(); dsink = a0 + a1 + a2 + a3; s[r] = (t1 - t0) / ((double)N * REP);
        }
        double v = med(s, R);
        if (k == 0) base_op = v;
        static const char *names[] = { "multiply  a*b", "add       a+b", "divide    a/b",
                                       "sqrt      sqrt(a)", "sin(a)" };
        printf("  %-24s %8.3f ns %8.1fx\n", names[k], v, v / base_op);
        printf("#DATA %d %.4f\n", k, v);
    }
    printf("#DATA-END\n");
    printf("\n  * multiply and add cost about the same. Divide is several times, trigonometry tens.\n");
    printf("    Hence the common trick of one divide for a reciprocal and many multiplies.\n");
    printf("  * and the earlier failure is the greater lesson --- with a large array all of\n");
    printf("    this difference is buried in memory bandwidth. To measure arithmetic, keep the data in cache.\n");
    return 0;
}
