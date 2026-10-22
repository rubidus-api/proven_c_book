/* The cost of the numbers themselves --- the same operation with different costs.
   ★ Trap: with a large array the result is bound by *memory bandwidth* and hides the difference.
     (The first version gave 1.24 ns for multiply, divide and square root alike --- the work
      was waiting while 16 MiB streamed past.)
     So here the working set fits in L1 and is repeated many times. */
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

#define N     2048           /* 2048 × 8 bytes × 2 arrays = 32 KiB --- inside L1 */
#define REP   2000           /* repetitions */
#define R     7

/* ★ A floating-point sum *changes when reordered* (addition is not associative).
   So the compiler may not vectorize it without permission. Only the two functions below get it. */
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
            a[i] = mode ? DBL_MIN / 8.0 : 1.0;      /* subnormal : normal */
            b[i] = 1.0;                             /* multiplying keeps the size --- the result is subnormal too */
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

    /* Why no difference --- no vector instructions were used. Measure again with permission given. */
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
