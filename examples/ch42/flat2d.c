/* 1차원으로 할당해 2차원으로 쓰기 — 실무에서 가장 흔한 형태.
   첨자 계산을 손으로 하지 않고 매크로 한 줄에 가둔다. */
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>

/* 첨자 매크로: 인자를 모두 괄호로 감싸고(52장), 곱을 size_t 로 올린다.
   i 와 j 를 두 번 평가하지 않으므로 부수효과가 있는 인자도 안전하다. */
#define AT(p, cols, i, j)  ((p)[(size_t)(i) * (size_t)(cols) + (size_t)(j)])

/* 크기 계산은 넘칠 수 있다 — 넘치면 할당을 시도하지 않는다(49·68장). */
static int *alloc_grid(size_t rows, size_t cols)
{
    size_t cells, bytes;
    if (ckd_mul(&cells, rows, cols))          return NULL;
    if (ckd_mul(&bytes, cells, sizeof(int)))  return NULL;
    return malloc(bytes);
}

int main(void)
{
    const size_t rows = 3, cols = 4;
    int *g = alloc_grid(rows, cols);
    if (!g) { puts("할당 실패"); return 1; }

    /* ── ① 채우기와 읽기 — 첨자는 매크로 하나로 ─────────────── */
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            AT(g, cols, i, j) = (int)(i * 10 + j);

    printf("행 우선으로 담은 3x4:\n");
    for (size_t i = 0; i < rows; i++) {
        printf("  ");
        for (size_t j = 0; j < cols; j++)
            printf("%3d", AT(g, cols, i, j));
        printf("\n");
    }

    /* ── ② 기억은 한 줄로 이어져 있다 ───────────────────────── */
    printf("\n같은 기억을 1차원으로 훑으면: ");
    for (size_t k = 0; k < rows * cols; k++)
        printf(k ? " %d" : "%d", g[k]);
    printf("\n(진짜 2차원 배열 int m[3][4] 의 배치와 같다 — 행 우선)\n");

    /* ── ③ 흔한 사고: 행과 열을 바꿔 쓰기 ───────────────────── */
    printf("\nAT(g, cols, 1, 2) = %d   (2행 3번째)\n", AT(g, cols, 1, 2));
    printf("AT(g, cols, 2, 1) = %d   (3행 2번째 — 다른 칸이다)\n", AT(g, cols, 2, 1));

    /* ── ④ 넘치는 크기 요청은 거절된다 ──────────────────────── */
    int *huge = alloc_grid((size_t)-1 / 2, 8);
    printf("\n터무니없는 크기 요청: %s\n", huge ? "할당됨" : "계산이 넘쳐 거절");
    free(huge);

    free(g);
    return 0;
}
