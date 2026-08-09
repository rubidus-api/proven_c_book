/* 2차원 배열을 함수에 넘기는 세 가지 배치 — 그리고 왜 int** 가 아닌가. */
#include <stdio.h>
#include <stdlib.h>

/* ① 폭이 고정된 2차원: 안쪽 차원은 타입에 남는다.
      셋은 컴파일러에게 완전히 같은 선언이다. */
static int sum_fixed(int m[3][4])      { int s = 0; for (int i=0;i<3;i++) for (int j=0;j<4;j++) s += m[i][j]; return s; }
/* 다음 둘은 위와 *완전히 같은* 선언이다(본문 참조):
       static int sum_fixed(int m[][4]);
       static int sum_fixed(int (*m)[4]);   */

/* ② VLA 매개변수(C99): 폭을 실행 중에 받는다 — 수치 코드의 정공법 */
static int sum_vla(size_t rows, size_t cols, const int a[rows][cols])
{
    int s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) s += a[i][j];
    return s;
}

/* ③ 행 포인터의 배열: 배치가 아예 다르다 — 행마다 따로 살 수 있다 */
static int sum_rows(size_t rows, size_t cols, int *const rowp[rows])
{
    int s = 0;
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++) s += rowp[i][j];
    return s;
}

int main(void)
{
    int a[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };

    printf("① fixed width    sum_fixed(a)      = %d\n", sum_fixed(a));
    printf("② VLA parameter  sum_vla(3,4,a)    = %d\n", sum_vla(3, 4, a));

    /* 행 포인터 배열을 만들어 같은 자료를 가리키게 한다 */
    int *rowp[3] = { a[0], a[1], a[2] };
    printf("③ row pointers   sum_rows(3,4,rowp) = %d\n\n", sum_rows(3, 4, rowp));

    /* 배치의 차이를 눈으로 */
    printf("the layouts differ:\n");
    printf("  a      : int[3][4]  - one block of %zu bytes, 0 indirections\n", sizeof a);
    printf("  rowp   : int*[3]    - %zu bytes of pointers plus the rows, 1 indirection\n", sizeof rowp);

    /* 행 교환의 비용이 갈린다 */
    int *tmp = rowp[0]; rowp[0] = rowp[2]; rowp[2] = tmp;  /* 포인터 두 개만 바뀐다 */
    printf("\nswapping row pointers leaves the data alone and changes the order: ");
    for (size_t j = 0; j < 4; j++) printf("%d ", rowp[0][j]);
    printf("\n  (doing the same to a 2-D array would really move 16 bytes)\n");

    /* 들쭉날쭉한 행 — 2차원 배열로는 못 하는 배치 */
    int r0[] = { 1 }, r1[] = { 2, 3, 4 };
    int *jag[2] = { r0, r1 };
    size_t len[2] = { 1, 3 };
    printf("\njagged rows: ");
    for (size_t i = 0; i < 2; i++)
        for (size_t j = 0; j < len[i]; j++) printf("%d ", jag[i][j]);
    printf("\n");
    return 0;
}
