#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int count = 5;
    int *scores = malloc(count * sizeof *scores);   /* 창고에서 빌린다 */

    if (scores == nullptr) {                        /* 빌리기는 실패할 수 있다 */
        printf("allocation failed\n");
        return 1;
    }

    int sum = 0;
    for (int i = 0; i < count; i += 1) {
        scores[i] = (i + 1) * 10;
        sum += scores[i];
    }
    printf("total: %d\n", sum);

    free(scores);                                   /* 빌린 것은 돌려준다 */
    return 0;
}
