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

    printf("reading a 64x64 double matrix 4096 times. Cache line %d bytes,\n", LINE);
    printf("which holds %zu doubles.\n\n", (size_t)LINE / sizeof(double));

    printf("%-14s %14s %18s\n", "traversal", "lines touched", "same line as before");
    printf("%-14s %14zu %18zu\n", "row-major ij", row.lines_touched, row.same_line_hits);
    printf("%-14s %14zu %18zu\n", "column-major ji", col.lines_touched, col.same_line_hits);

    printf("\nboth traversals touch the same number of lines - they read the same data.\n");
    printf("what differs is *locality*. Row-major is served from the previous line %zu times\n",
           row.same_line_hits);
    printf("out of the total, column-major only %zu times. Column-major steps %zu bytes\n",
           col.same_line_hits, (size_t)N * sizeof(double));
    printf("each time, so it jumps to a different line every time.\n");

    free(order);
    return 0;
}
