#include <stdio.h>

int main(void)
{
    int *p = nullptr;           /* it points nowhere yet */

    if (p == nullptr) {
        printf("p is empty for now\n");
    }

    int n = 7;
    p = &n;
    if (p != nullptr) {
        printf("now p points to %d\n", *p);
    }
    return 0;
}
