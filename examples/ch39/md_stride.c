/* 순회 순서가 기억을 어떻게 훑는가 — 시간이 아니라 *접근 간격*을 센다.
   (시간 측정은 빌드마다 달라지므로, 결정적인 수만 인쇄한다.) */
#include <stdio.h>
#include <stdlib.h>

#define LINE 64            /* 캐시 라인 한 줄의 크기(11장) */

/* 훑는 순서대로 만져지는 캐시 라인의 *번호*를 세어, 서로 다른 라인이
   몇 개나 등장하는지와 "직전 접근과 같은 라인인가"를 센다. */
typedef struct { size_t lines_touched; size_t same_line_hits; } stats;

static stats scan(const double *base, const size_t *order, size_t n)
{
    stats s = { 0, 0 };
    long prev_line = -1;
    unsigned char seen[4096] = {0};
    for (size_t k = 0; k < n; k++) {
        size_t byte = order[k] * sizeof(double);
        size_t line = byte / LINE;
        if (line < sizeof seen && !seen[line]) { seen[line] = 1; s.lines_touched++; }
        if ((long)line == prev_line) s.same_line_hits++;
        prev_line = (long)line;
    }
    (void)base;
    return s;
}

int main(void)
{
    enum { N = 64 };                       /* 64x64 double = 32 KiB */
    static double m[N][N];
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++) m[i][j] = (double)(i * N + j);

    size_t *order = malloc((size_t)N * N * sizeof *order);
    if (!order) return 1;

    /* ① 행 우선(row-major 그대로): m[i][j] 를 j 안쪽으로 */
    size_t k = 0;
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++) order[k++] = i * N + j;
    stats row = scan(&m[0][0], order, (size_t)N * N);

    /* ② 열 우선(전치 순회): m[i][j] 를 i 안쪽으로 */
    k = 0;
    for (size_t j = 0; j < N; j++)
        for (size_t i = 0; i < N; i++) order[k++] = i * N + j;
    stats col = scan(&m[0][0], order, (size_t)N * N);

    printf("64x64 double 행렬을 4096번 읽는다. 캐시 라인 %d바이트,\n", LINE);
    printf("한 줄에 double %zu개가 들어간다.\n\n", (size_t)LINE / sizeof(double));

    printf("%-14s %14s %18s\n", "순회", "만진 라인 수", "직전과 같은 라인");
    printf("%-14s %14zu %18zu\n", "행 우선 ij", row.lines_touched, row.same_line_hits);
    printf("%-14s %14zu %18zu\n", "열 우선 ji", col.lines_touched, col.same_line_hits);

    printf("\n두 순회가 만지는 라인의 총수는 같다 — 같은 자료를 다 읽으니까.\n");
    printf("갈리는 것은 *연속성*이다. 행 우선은 %zu번을 직전과 같은 라인에서\n",
           row.same_line_hits);
    printf("해결하고, 열 우선은 %zu번뿐이다. 열 우선은 한 걸음이 %zu바이트라\n",
           col.same_line_hits, (size_t)N * sizeof(double));
    printf("매번 다른 라인으로 건너뛴다.\n");

    free(order);
    return 0;
}
