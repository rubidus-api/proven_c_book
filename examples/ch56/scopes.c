/* 이름이 보이는 네 가지 범위 --- 표준이 정한 그대로. */
#include <stdio.h>

/* ① 파일 스코프: 여기서부터 이 파일 끝까지 보인다 */
static int file_level = 1;

/* ② 함수 원형 스코프: 매개변수 이름 rows·cols 는 이 괄호가 닫히면 사라진다.
      이름이 사라져도 쓸모는 있다 --- 뒤 매개변수의 크기를 이 이름으로 적는다. */
void print_grid(int rows, int cols, int grid[rows][cols]);

/* ③ 함수 스코프: 레이블은 함수 전체에서 보인다. 블록 안에 적어도 그렇다. */
static int find_first_negative(const int *a, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] < 0) {
            goto found;          /* 아래 블록 안의 레이블로 뛴다 */
        }
    }
    return -1;
    {
    found:                       /* 블록 안에 있어도 함수 어디서나 보인다 */
        return 0;
    }
}

void print_grid(int rows, int cols, int grid[rows][cols])
{
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) printf(" %d", grid[r][c]);
        putchar('\n');
    }
}

int main(void)
{
    /* ④ 블록 스코프: 이 중괄호 안에서만 */
    int block_level = 2;
    {
        int inner = 3;
        printf("inner block sees: file=%d block=%d inner=%d\n",
               file_level, block_level, inner);
    }
    /* 여기서 inner 는 이미 이름이 아니다 */

    int grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
    puts("\nthe grid, sized by names from the prototype scope:");
    print_grid(2, 3, grid);

    int values[] = {5, 7, -2, 9};
    printf("\nfirst negative found: %s\n",
           find_first_negative(values, 4) == 0 ? "yes" : "no");
    return 0;
}
