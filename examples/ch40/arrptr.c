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
    printf("  함수 안 sizeof p        = %zu  (포인터 하나)\n", sizeof p);
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
}

int main(void)
{
    puts("① 무엇을 담는가 — 크기로 드러난다");
    printf("  sizeof arr = %zu   sizeof ptr = %zu\n", sizeof arr, sizeof ptr);
    printf("  원소 수: sizeof arr / sizeof arr[0] = %zu\n",
           sizeof arr / sizeof arr[0]);

    puts("\n② 같은 주소, 다른 타입 — +1 이 건너뛰는 폭이 다르다");
    printf("  arr      = %p\n", (void *)arr);
    printf("  &arr[0]  = %p   (같다)\n", (void *)&arr[0]);
    printf("  &arr     = %p   (같다 — 그러나 타입이 int(*)[5] 다)\n", (void *)&arr);
    printf("  arr  + 1 = %p   (+%td 바이트)\n",
           (void *)(arr + 1), (char *)(arr + 1) - (char *)arr);
    printf("  &arr + 1 = %p   (+%td 바이트 — 배열 통째로 건너뛴다)\n",
           (void *)(&arr + 1), (char *)(&arr + 1) - (char *)arr);

    puts("\n③ 누가 옮겨 다닐 수 있는가");
    ptr = ptr + 2;                 /* 포인터는 옮길 수 있다 */
    printf("  ptr 를 두 칸 옮기니 *ptr = %d\n", *ptr);
    /* arr = ptr;  <- 배열 이름은 수정 가능한 좌변값이 아니다: 컴파일 오류 */
    ptr = arr;

    puts("\n④ 첨자는 포인터 오프셋이다 — 그래서 뒤집어도 된다");
    printf("  arr[3] = %d,  *(arr + 3) = %d,  3[arr] = %d\n",
           arr[3], *(arr + 3), 3[arr]);

    puts("\n⑤ 함수에 넘기면 크기가 사라진다");
    printf("  부르기 전 sizeof arr    = %zu\n", sizeof arr);
    by_param(arr);

    return 0;
}
