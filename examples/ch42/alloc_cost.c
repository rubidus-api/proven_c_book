/* malloc 이 돌려주는 주소의 정렬, 그리고 빌리고 돌려주는 값 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 300000

static double seconds_since(struct timespec t0)
{
    struct timespec t1;
    timespec_get(&t1, TIME_UTC);
    return (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
}

static void *volatile sink;      /* 최적화가 할당을 지우지 못하게 붙잡는다 */

int main(void)
{
    /* ── ① 정렬: 무엇을 담을지 모르므로 가장 엄격한 정렬로 준다 ── */
    printf("max_align_t 의 정렬  = %zu 바이트\n", alignof(max_align_t));

    size_t misaligned = 0;
    void *p[8];
    for (int i = 0; i < 8; i++) {
        p[i] = malloc(1);                    /* 1바이트만 요청해도 */
        if ((uintptr_t)p[i] % alignof(max_align_t) != 0) misaligned++;
    }
    printf("1바이트 요청 8회 중 기본 정렬을 어긴 것: %zu 개\n", misaligned);
    printf("이웃한 두 블록의 주소 간격: %td 바이트 (1을 요청했는데도)\n",
           (char *)p[1] - (char *)p[0]);
    for (int i = 0; i < 8; i++) free(p[i]);

    /* ── ② 값: 같은 일을 세 방식으로 ── */
    struct timespec t0;
    unsigned long long acc = 0;

    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* 매번 빌리고 돌려준다 */
        char *b = malloc(64);
        if (!b) return 1;
        b[0] = (char)i;
        acc += (unsigned char)b[0];
        sink = b;
        free(b);
    }
    double t_alloc = seconds_since(t0);

    char *reused = malloc(64);
    if (!reused) return 1;
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* 한 번 빌려 계속 쓴다 */
        reused[0] = (char)i;
        acc += (unsigned char)reused[0];
        sink = reused;
    }
    double t_reuse = seconds_since(t0);
    free(reused);

    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < N; i++) {           /* 아예 스택에 둔다 */
        char b[64];
        b[0] = (char)i;
        acc += (unsigned char)b[0];
        sink = b;
    }
    double t_stack = seconds_since(t0);

    printf("\n%d 회 반복\n", N);
    printf("  매번 malloc + free : %7.4f 초  (회당 %5.1f ns)\n", t_alloc, t_alloc / N * 1e9);
    printf("  한 번 빌려 재사용   : %7.4f 초  (회당 %5.1f ns)\n", t_reuse, t_reuse / N * 1e9);
    printf("  스택 배열           : %7.4f 초  (회당 %5.1f ns)\n", t_stack, t_stack / N * 1e9);
    printf("\n(합계 %llu — 계산이 지워지지 않게 쓰는 값)\n", acc);
    return 0;
}
