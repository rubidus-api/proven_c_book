#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int count = 5;
    int *scores = malloc(count * sizeof *scores);   /* borrowed from the warehouse */

    if (scores == nullptr) {                        /* borrowing can fail */
        printf("allocation failed\n");
        return 1;
    }

    int sum = 0;
    for (int i = 0; i < count; i += 1) {
        scores[i] = (i + 1) * 10;
        sum += scores[i];
    }
    printf("total: %d\n", sum);

    free(scores);                                   /* what is borrowed is returned */
    return 0;
}
