/* 겹쳐 도는 루프 — 이중과 삼중, 그리고 겹칠 때 새로 생기는 실수들. */
#include <stdio.h>

/* ① 이중 for — 바깥 한 바퀴마다 안쪽이 처음부터 다시 돈다 */
static void times_table(void)
{
    for (int row = 2; row <= 4; row++) {
        printf("  ");
        for (int col = 1; col <= 5; col++)
            printf("%s%2d x %d = %2d", col == 1 ? "" : "   ", row, col, row * col);
        printf("\n");                    /* 안쪽 루프가 끝난 자리 = 한 줄의 끝 */
    }
}

/* ② 안쪽 루프 변수를 바깥에서 만들면 「다시 처음부터」가 사라진다 */
static void forgot_to_reset(void)
{
    int visits = 0;
    int col = 1;                         /* 초기화가 바깥에 있다 */
    for (int row = 2; row <= 4; row++)
        for (; col <= 5; col++)          /* 두 번째 바퀴부터 col 은 이미 6 이다 */
            visits++;
    printf("  counter declared outside : %d cells visited\n", visits);

    visits = 0;
    for (int row = 2; row <= 4; row++)
        for (int c = 1; c <= 5; c++)     /* 안쪽에서 선언하면 매 바퀴 새로 시작한다 */
            visits++;
    printf("  counter declared inside  : %d cells visited\n", visits);
}

/* ③ 안쪽에서 바깥 카운터를 올리는 실수 (실제 사례의 무늬).
      멈추게 하려고 한도를 두었다 — 실제 코드에는 그런 안전장치가 없다. */
static void wrong_counter(void)
{
    int steps = 0;
    for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; j++) {    /* k 가 아니라 j 를 올리고 있다 */
            steps++;
            if (steps >= 100)            /* 이 줄이 없으면 끝나지 않는다 */
                break;
        }
        if (steps >= 100)
            break;
    }
    printf("  wrong counter (k stands still): %d steps and still not done\n", steps);

    steps = 0;
    for (int j = 0; j < 3; j++)
        for (int k = 0; k < 3; k++)
            steps++;
    printf("  right counter                : %d steps, 3 x 3 as intended\n", steps);
}

/* ④ 삼중 for — 세 값을 한꺼번에 훑는 전형. 피타고라스 세 쌍을 찾는다. */
static void triples(int n)
{
    long long tries = 0;
    int found = 0;

    for (int a = 1; a <= n; a++)
        for (int b = a; b <= n; b++)             /* b 를 a 부터 시작해 중복을 없앤다 */
            for (int c = b; c <= n; c++) {
                tries++;
                if (a * a + b * b == c * c) {
                    printf("  %2d^2 + %2d^2 = %2d^2\n", a, b, c);
                    found++;
                }
            }
    printf("  n = %d: %d triple(s) found in %lld checks\n", n, found, tries);
}

int main(void)
{
    printf("[a double loop - the inner one restarts every outer turn]\n");
    times_table();

    printf("\n[where the inner counter is declared decides whether it restarts]\n");
    forgot_to_reset();

    printf("\n[raising the wrong counter in the inner loop]\n");
    wrong_counter();

    printf("\n[a triple loop - Pythagorean triples up to n]\n");
    triples(20);

    printf("\n[how fast the work grows]\n");
    for (int n = 10; n <= 40; n *= 2) {
        long long steps = 0;
        for (int a = 1; a <= n; a++)
            for (int b = 1; b <= n; b++)
                for (int c = 1; c <= n; c++)
                    steps++;
        printf("  n = %2d -> %lld steps (n^3)\n", n, steps);
    }
    return 0;
}
