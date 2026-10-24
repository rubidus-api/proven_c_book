/* 배열과 포인터가 어디서 갈라지는지를 한자리에서 갈라 본다.
   같은 자리를 가리키는 두 값이 어떻게 다르게 행동하는지가 요점이다. */
#include <stdio.h>

static int  arr[5] = { 10, 20, 30, 40, 50 };
static int *ptr    = arr;          /* 무너진 주소를 담는다 */

static void by_param(int p[5])     /* [5] 라고 적어도 포인터다 */
{
    /* 오늘의 컴파일러는 바로 이 자리에 경고를 준다(-Wsizeof-array-argument).
       경고가 맞다 — 보여 주기 위해 그 하나만 잠시 끈다. */
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
    printf("  inside the function sizeof p = %zu  (one pointer)\n", sizeof p);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

int main(void)
{
    puts("① what does it hold - the size tells you");
    printf("  sizeof arr = %zu   sizeof ptr = %zu\n", sizeof arr, sizeof ptr);
    printf("  element count: sizeof arr / sizeof arr[0] = %zu\n",
           sizeof arr / sizeof arr[0]);

    puts("\n② same address, different type - +1 skips a different width");
    printf("  arr      = %p\n", (void *)arr);
    printf("  &arr[0]  = %p   (the same)\n", (void *)&arr[0]);
    printf("  &arr     = %p   (the same - but its type is int(*)[5])\n", (void *)&arr);
    printf("  arr  + 1 = %p   (+%td bytes)\n",
           (void *)(arr + 1), (char *)(arr + 1) - (char *)arr);
    printf("  &arr + 1 = %p   (+%td bytes - it skips the whole array)\n",
           (void *)(&arr + 1), (char *)(&arr + 1) - (char *)arr);

    puts("\n③ which one can move");
    ptr = ptr + 2;                 /* 포인터는 옮길 수 있다 */
    printf("  moving ptr two slots gives *ptr = %d\n", *ptr);
    /* arr = ptr;  <- 배열 이름은 수정 가능한 좌변값이 아니다: 컴파일 오류 */
    ptr = arr;

    puts("\n④ a subscript is a pointer offset - which is why it can be reversed");
    printf("  arr[3] = %d,  *(arr + 3) = %d,  3[arr] = %d\n",
           arr[3], *(arr + 3), 3[arr]);

    puts("\n⑤ pass it to a function and the size disappears");
    printf("  sizeof arr before the call = %zu\n", sizeof arr);
    by_param(arr);

    return 0;
}
