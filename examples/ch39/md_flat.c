/* 평탄하게 잡고 2차원으로 보기 — BLAS 계열이 쓰는 패턴(선행 차원, 부분행렬). */
#include <stdio.h>
#include <stdlib.h>

/* 선행 차원(leading dimension, lda): 한 행에서 다음 행까지의 *간격*.
   실제 열 수(cols)와 다를 수 있다 — 부분행렬이 그래서 공짜가 된다. */
#define AT(a, lda, i, j) ((a)[(size_t)(i) * (size_t)(lda) + (size_t)(j)])

static void fill(double *a, size_t lda, size_t rows, size_t cols)
{
    for (size_t i = 0; i < rows; i++)
        for (size_t j = 0; j < cols; j++)
            AT(a, lda, i, j) = (double)(10 * (i + 1) + (j + 1));
}

static void show(const char *tag, const double *a, size_t lda,
                 size_t rows, size_t cols)
{
    printf("%s (lda=%zu):\n", tag, lda);
    for (size_t i = 0; i < rows; i++) {
        printf("  ");
        for (size_t j = 0; j < cols; j++) printf("%5.0f", AT(a, lda, i, j));
        printf("\n");
    }
}

int main(void)
{
    const size_t rows = 4, cols = 5;

    /* 한 덩어리로 잡는다 — 곱셈부터 넘침을 본다(86장의 감각) */
    size_t n;
    if (__builtin_mul_overflow(rows, cols, &n)) return 1;
    double *m = malloc(n * sizeof *m);
    if (!m) return 1;

    fill(m, cols, rows, cols);
    show("whole 4x5", m, cols, rows, cols);

    /* ① 2차원 뷰 — malloc 이 준 기억에는 선언된 타입이 없으므로
          여기에 int[..][..] 모양을 씌우는 것이 정공법이다(본문 참조). */
    double (*view)[cols] = (double (*)[cols])m;
    printf("\nthrough the 2-D view view[2][3] = %.0f  (flat, that is m[2*5+3] = %.0f)\n",
           view[2][3], m[2 * 5 + 3]);

    /* ② 부분행렬 — 복사 없이 '가리키기'만 한다.
          행 1..2, 열 1..3 짜리 2x3 블록. 간격(lda)은 원본 그대로 5. */
    double *sub = &AT(m, cols, 1, 1);
    printf("\na submatrix is not a copy - it is a starting point plus a stride:\n");
    show("  sub 2x3", sub, cols, 2, 3);

    /* 부분행렬에 쓰면 원본이 바뀐다 — 뷰이기 때문이다 */
    AT(sub, cols, 0, 0) = -1;
    printf("\nwriting -1 at (0,0) of the submatrix changes (1,1) of the original:\n");
    show("whole 4x5", m, cols, rows, cols);

    /* ③ 전치도 간격을 바꾸는 문제로 바뀐다(복사 없이 읽는 순서만 바꾼다) */
    printf("\nreading it transposed (row stride 1, column stride %zu):\n", cols);
    for (size_t j = 0; j < cols; j++) {
        printf("  ");
        for (size_t i = 0; i < rows; i++) printf("%5.0f", AT(m, cols, i, j));
        printf("\n");
    }

    free(m);
    return 0;
}
