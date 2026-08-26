// 프로세스는 실행할 때마다 새로 태어난다 --- 지난번의 기억은 남지 않는다.
#include <stdio.h>
#include <stdlib.h>

// 프로그램이 통째로 가지는 값. 「지난 실행」의 값이 남아 있을 것 같지만,
// 프로그램이 끝나면 이 값이 놓였던 기억은 운영체제가 거두어 간다.
static int run_count = 0;

static void at_the_end(void)
{
    puts("  the process is ending --- everything it owned goes back");
}

int main(int argc, char **argv)
{
    atexit(at_the_end);          // 프로세스가 끝날 때 부를 함수를 등록한다

    run_count += 1;
    // argv[0] 은 이 프로그램이 불린 이름이다. 자리마다 값이 다르므로 여기서는
    // 「있다/없다」만 본다.
    printf("the program received %d argument(s), and argv[0] is %s\n",
           argc, (argc > 0 && argv[0] != NULL) ? "present" : "absent");
    printf("  run_count in this process: %d\n", run_count);
    puts("  run it again and it prints 1 again --- a new process starts fresh");
    return 0;
}
