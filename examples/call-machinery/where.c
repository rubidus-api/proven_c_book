// 인자는 어디에 놓여 있는가 --- 레지스터로 온 것과 스택으로 온 것.
//
// C 에는 "이 인자는 레지스터로 왔다"를 묻는 문법이 없다. 그러나 최적화를
// 끄면 컴파일러는 레지스터로 받은 인자를 자기 프레임에 나란히 내려놓고,
// 스택으로 실려 온 인자는 호출한 쪽이 놓아 둔 자리를 그대로 쓴다. 두
// 무리는 서로 멀리 떨어져 있으므로, *이웃한 인자의 주소 차이*를 재면
// 경계가 어디인지 드러난다.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

static void eight(int a, int b, int c, int d, int e, int f, int g, int h)
{
    const int *arg[8] = { &a, &b, &c, &d, &e, &f, &g, &h };
    ptrdiff_t widest = 0;
    int at = 0;

    for (int i = 0; i + 1 < 8; i++) {
        uintptr_t p = (uintptr_t)arg[i], q = (uintptr_t)arg[i + 1];
        ptrdiff_t gap = p > q ? (ptrdiff_t)(p - q) : (ptrdiff_t)(q - p);
        printf("  argument %d to %d: %td bytes apart\n", i + 1, i + 2, gap);
        if (gap > widest) { widest = gap; at = i + 1; }
    }
    printf("the widest jump is between argument %d and %d\n", at, at + 1);
}

int main(void)
{
    puts("a call with eight arguments:");
    eight(1, 2, 3, 4, 5, 6, 7, 8);
    puts("that jump is the seam between the register group and the stack group.");
    return 0;
}
