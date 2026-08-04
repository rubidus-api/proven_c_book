#include <stdio.h>

int main(void)
{
    int *p = nullptr;           /* 아직 아무 데도 가리키지 않는다 */

    if (p == nullptr) {
        printf("p는 지금 비어 있다\n");
    }

    int n = 7;
    p = &n;
    if (p != nullptr) {
        printf("이제 p는 값 %d를 가리킨다\n", *p);
    }
    return 0;
}
