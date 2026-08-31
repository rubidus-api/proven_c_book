/* 주소를 이름으로 바꾸는 두 층 --- 심볼 표와 디버그 정보.
   여기서는 살아 있는 프로그램이 제 스택을 되감지만, 덤프를 읽을 때 디버거가 하는
   일이 정확히 같다: 주소를 모아, 그것을 이름과 줄 번호로 바꾼다. */
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

static void level3(void)                 /* static --- 동적 심볼 표에 오르지 않는다 */
{
    void *frames[16];
    int n = backtrace(frames, 16);       /* 되감아 주소를 모은다 */
    char **names = backtrace_symbols(frames, n);   /* 이름을 붙여 본다 */

    printf("frames captured: %d\n\n", n);
    for (int i = 0; i < n && i < 5; i++)
        printf("  [%d] %s\n", i, names[i]);
    free(names);

    puts("\nnotice: main is named, the static functions are not.");
    puts("a name here comes from the *symbol table*, and static functions are local");
    puts("symbols -- they never reach the dynamic one. the debug information (DWARF)");
    puts("still knows them, which is what addr2line reads.");
}
static void level2(void) { level3(); }
static void level1(void) { level2(); }

int main(void) { level1(); return 0; }
