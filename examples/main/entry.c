/* main 이 받는 것과 남기는 것 */
#include <stdio.h>
#include <stdlib.h>

/* 프로그램이 끝날 때 불러 달라고 등록해 두는 함수 */
static void farewell(void)
{
    puts("  atexit: the cleanup we registered runs here");
}

int main(int argc, char *argv[])
{
    atexit(farewell);

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("  argv[%d] = \"%s\"\n", i, argv[i]);

    /* 표준이 약속한다: argv[argc] 는 반드시 널 포인터다 */
    printf("is argv[argc] null? %s\n", argv[argc] == nullptr ? "yes" : "no");

    printf("EXIT_SUCCESS = %d, EXIT_FAILURE = %d\n", EXIT_SUCCESS, EXIT_FAILURE);

    /* 성공을 알리는 세 가지 표기는 모두 같은 뜻이다 */
    return EXIT_SUCCESS;        /* == return 0; == (C99 부터) 그냥 끝내기 */
}
