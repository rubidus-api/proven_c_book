#include <stdio.h>

int main(void)
{
    char line[100];   /* room for 100 characters — properly explained in chapter 33 */
    int n = 0;

    fgets(line, sizeof line, stdin);   /* step 1: read one whole line */
    sscanf(line, "%d", &n);            /* step 2: interpret an integer out of that line */

    printf("%d squared is %d.\n", n, n * n);
    return 0;
}
