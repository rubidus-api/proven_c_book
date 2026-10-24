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
    printf("alignment of max_align_t = %zu bytes\n", alignof(max_align_t));

    size_t misaligned = 0;
    void *p[8];
    for (int i = 0; i < 8; i++) {
        p[i] = malloc(1);                    /* 1바이트만 요청해도 */
        if ((uintptr_t)p[i] % alignof(max_align_t) != 0) misaligned++;
    }
    printf("of 8 one-byte requests, ones breaking the default alignment: %zu\n", misaligned);
    printf("address gap between two neighbouring blocks: %td bytes (though 1 was asked for)\n",
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

    printf("\n%d iterations\n", N);
    printf("  malloc + free every time : %7.4f s  (%5.1f ns each)\n", t_alloc, t_alloc / N * 1e9);
    printf("  borrow once and reuse    : %7.4f s  (%5.1f ns each)\n", t_reuse, t_reuse / N * 1e9);
    printf("  a stack array            : %7.4f s  (%5.1f ns each)\n", t_stack, t_stack / N * 1e9);
    printf("\n(total %llu - a value we use so the work is not optimized away)\n", acc);
    return 0;
}
