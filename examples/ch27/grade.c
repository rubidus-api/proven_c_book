#include <stdio.h>

int main(void)
{
    char line[100];
    int score = 0;

    fgets(line, sizeof line, stdin);
    sscanf(line, "%d", &score);

    if (score >= 90) {
        printf("%d점: 수\n", score);
    } else if (score >= 80) {
        printf("%d점: 우\n", score);
    } else if (score >= 70) {
        printf("%d점: 미\n", score);
    } else {
        printf("%d점: 분발\n", score);
    }
    return 0;
}
