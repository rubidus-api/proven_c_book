#include <stdio.h>

#define COUNT(a) (sizeof (a) / sizeof (a)[0])   /* 진짜 배열에만 쓴다 */

static void show(const char *label, const int *v, size_t n)
{
    printf("%-22s", label);
    for (size_t i = 0; i < n; i++)
        printf(" %d", v[i]);
    printf("   (%zu elements)\n", n);
}

static int counter[3];          /* 정적 저장 기간 --- 적지 않아도 0 */

int main(void)
{
    int full[5]    = {3, 1, 4, 1, 5};   /* 다섯 칸을 다 적었다 */
    int part[5]    = {3, 1};            /* 나머지 셋은 0 */
    int sized[]    = {2, 7, 1, 8};      /* 크기를 초기값 개수로 정한다 */
    int marked[6]  = {[4] = 9, [1] = 2};/* 지정 초기화(C99) */
    int empty[4]   = {};                /* C23: 빈 중괄호는 전부 0 */

    puts("[1] initializers");
    show("{3, 1, 4, 1, 5}", full, COUNT(full));
    show("{3, 1}", part, COUNT(part));
    show("[] = {2, 7, 1, 8}", sized, COUNT(sized));
    show("{[4] = 9, [1] = 2}", marked, COUNT(marked));
    show("{}", empty, COUNT(empty));
    show("static, no initializer", counter, COUNT(counter));

    puts("\n[2] the array type knows its size");
    printf("sizeof full    = %zu bytes\n", sizeof full);
    printf("sizeof full[0] = %zu bytes\n", sizeof full[0]);
    printf("COUNT(full)    = %zu\n", COUNT(full));

    puts("\n[3] no whole-array assignment: copy element by element");
    int copy[5];
    for (size_t i = 0; i < COUNT(full); i++)
        copy[i] = full[i];                  /* copy = full; 은 오류다 */
    show("copy of full", copy, COUNT(copy));

    puts("\n[4] the first element shares the array's address");
    printf("(void *)full == (void *)&full[0] : %d\n",
           (void *)full == (void *)&full[0]);
    return 0;
}
