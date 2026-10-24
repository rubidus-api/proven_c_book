/* 스트림에는 '방향'이 있다 — 바이트냐 와이드냐, 한 번 정해지면 못 바꾼다. */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

/* fwide(stream, 0) 은 방향을 묻기만 한다.
   음수=바이트 방향, 0=아직 없음, 양수=와이드 방향. */
static const char *orientation(FILE *f)
{
    int w = fwide(f, 0);
    return w < 0 ? "byte" : w > 0 ? "wide" : "not set yet";
}

int main(void)
{
    setlocale(LC_ALL, "");

    /* 파일 하나를 열어 방향이 정해지는 과정을 본다 */
    FILE *f = tmpfile();
    if (!f) { perror("tmpfile"); return 1; }

    printf("direction of a freshly opened stream: %s\n", orientation(f));

    /* fwide 로 *미리* 정할 수도 있다 — 아직 방향이 없을 때만 먹힌다 */
    fwide(f, 1);
    printf("after fwide(f, 1):                    %s\n", orientation(f));

    fputws(L"a line written wide\n", f);
    printf("after fputws:                         %s\n", orientation(f));

    /* 이미 와이드로 정해진 스트림에 바이트 계열을 쓰면 정의되지 않은 동작이다.
       그래서 여기서는 시도하지 않고, 방향을 되돌리려 해도 안 된다는 것만 본다. */
    fwide(f, -1);
    printf("trying fwide(f, -1) to go back:       %s (it does not change)\n", orientation(f));

    rewind(f);
    wchar_t line[64];
    if (fgetws(line, 64, f)) printf("the line read back:                   %ls", line);
    fclose(f);

    /* 두 번째 스트림은 바이트 쪽으로 정해 본다 */
    FILE *g = tmpfile();
    if (!g) { perror("tmpfile"); return 1; }
    printf("\na second stream:                      %s\n", orientation(g));
    fputs("a line written as bytes\n", g);
    printf("after fputs:                          %s\n", orientation(g));
    fclose(g);

    /* 표준 스트림도 마찬가지다. 이 프로그램은 지금까지 printf 만 썼으므로… */
    printf("\ndirection of stdout:                  %s\n", orientation(stdout));
    puts("-> because this program started with printf.");
    puts("   mixing wprintf in here would be undefined behaviour.");
    return 0;
}
