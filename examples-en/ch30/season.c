#include <stdio.h>

int main(void)
{
    char line[100];
    int month = 0;

    fgets(line, sizeof line, stdin);
    sscanf(line, "%d", &month);

    switch (month) {
    case 12:
    case 1:
    case 2:                     /* three cases share one answer (fall-through) */
        printf("month %d: winter\n", month);
        break;
    case 3:
    case 4:
    case 5:
        printf("month %d: spring\n", month);
        break;
    case 6:
    case 7:
    case 8:
        printf("month %d: summer\n", month);
        break;
    case 9:
    case 10:
    case 11:
        printf("month %d: autumn\n", month);
        break;
    default:
        printf("month %d: no such month\n", month);
        break;
    }
    return 0;
}
