#include <stdio.h>

int main(void)
{
    char line[100];
    int score = 0;

    fgets(line, sizeof line, stdin);
    sscanf(line, "%d", &score);

    if (score >= 90) {
        printf("%d: excellent\n", score);
    } else if (score >= 80) {
        printf("%d: good\n", score);
    } else if (score >= 70) {
        printf("%d: fair\n", score);
    } else {
        printf("%d: needs work\n", score);
    }
    return 0;
}
