#include <stdio.h>

int main(void)
{
    int *p = nullptr;           /* 아직 아무 데도 가리키지 않는다 */

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
