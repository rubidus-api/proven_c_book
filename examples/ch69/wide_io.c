/* 스트림에는 '방향'이 있다 — 바이트냐 와이드냐, 한 번 정해지면 못 바꾼다. */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

/* fwide(stream, 0) 은 방향을 묻기만 한다.
   음수=바이트 방향, 0=아직 없음, 양수=와이드 방향. */
static const char *orientation(FILE *f)
{
    int w = fwide(f, 0);
    return w < 0 ? "바이트" : w > 0 ? "와이드" : "아직 없음";
}

int main(void)
{
    setlocale(LC_ALL, "");

    /* 파일 하나를 열어 방향이 정해지는 과정을 본다 */
    FILE *f = tmpfile();
    if (!f) { perror("tmpfile"); return 1; }

    printf("갓 연 스트림의 방향:       %s\n", orientation(f));

    /* fwide 로 *미리* 정할 수도 있다 — 아직 방향이 없을 때만 먹힌다 */
    fwide(f, 1);
    printf("fwide(f, 1) 뒤:            %s\n", orientation(f));

    fputws(L"와이드로 쓴 줄\n", f);
    printf("fputws 한 뒤:              %s\n", orientation(f));

    /* 이미 와이드로 정해진 스트림에 바이트 계열을 쓰면 정의되지 않은 동작이다.
       그래서 여기서는 시도하지 않고, 방향을 되돌리려 해도 안 된다는 것만 본다. */
    fwide(f, -1);
    printf("fwide(f, -1) 로 되돌리기: %s (바뀌지 않는다)\n", orientation(f));

    rewind(f);
    wchar_t line[64];
    if (fgetws(line, 64, f)) printf("다시 읽은 줄:              %ls", line);
    fclose(f);

    /* 두 번째 스트림은 바이트 쪽으로 정해 본다 */
    FILE *g = tmpfile();
    if (!g) { perror("tmpfile"); return 1; }
    printf("\n두 번째 스트림:            %s\n", orientation(g));
    fputs("바이트로 쓴 줄\n", g);
    printf("fputs 한 뒤:               %s\n", orientation(g));
    fclose(g);

    /* 표준 스트림도 마찬가지다. 이 프로그램은 지금까지 printf 만 썼으므로… */
    printf("\nstdout 의 방향:            %s\n", orientation(stdout));
    puts("→ 이 프로그램이 printf 로 시작했기 때문이다.");
    puts("   여기서 wprintf 를 섞으면 정의되지 않은 동작이다.");
    return 0;
}
