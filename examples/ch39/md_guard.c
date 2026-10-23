#include <stdckdint.h>
#include <stdio.h>

enum { ROWS = 3, COLS = 4 };

/* 오프셋(원소 단위)을 정수로만 계산한다 --- 주소를 만들지 않으므로 계약 안 */
static int offset(int i, int j) { return i * COLS + j; }

/* 두 첨자를 각각 검사하는 접근 함수 */
static int get(const int m[ROWS][COLS], int i, int j, int *out)
{
    if (i < 0 || i >= ROWS || j < 0 || j >= COLS)
        return -1;                    /* 범위 밖: *out 은 건드리지 않는다 */
    *out = m[i][j];
    return 0;
}

int main(void)
{
    int m[ROWS][COLS] = {{0, 1, 2, 3}, {10, 11, 12, 13}, {20, 21, 22, 23}};

    puts("[1] a column past the row end lands on another cell");
    printf("offset(0, 5) = %d\n", offset(0, 5));
    printf("offset(1, 1) = %d   <- the same slot\n", offset(1, 1));

    puts("\n[2] check each subscript on its own");
    int v = -1;
    printf("get(1, 1): %d", get(m, 1, 1, &v));
    printf("  value %d\n", v);
    v = -1;
    printf("get(0, 5): %d", get(m, 0, 5, &v));
    printf("  value untouched: %d\n", v);

    puts("\n[3] the flat index i * cols + j can overflow int");
    int rows = 50000, cols = 50000, cells;
    if (ckd_mul(&cells, rows, cols))
        printf("%d x %d does not fit in int\n", rows, cols);
    size_t big;
    if (!ckd_mul(&big, (size_t)rows, (size_t)cols))
        printf("as size_t: %zu cells\n", big);
    return 0;
}
