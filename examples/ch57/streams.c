#include <stdio.h>
#include <string.h>

/* 스트림의 세 가지 실제 — 버퍼링, 반환값, 끝을 아는 법 */
int main(void)
{
    const char *path = "stream_demo.txt";

    /* ① 쓰기: fopen 은 실패하면 널이다 */
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return 1; }

    /* fprintf 도 실패할 수 있다 — 반환값은 찍은 글자 수 */
    int written = fprintf(f, "one\ntwo\nthree\n");
    printf("fprintf wrote %d chars\n", written);

    /* ② 닫기도 실패할 수 있다: 버퍼를 비우다 실패하면 여기서 드러난다 */
    if (fclose(f) != 0) { perror("fclose"); return 1; }

    /* ③ 읽기: 줄 단위로 */
    f = fopen(path, "r");
    if (!f) { perror("fopen"); return 1; }

    char line[64];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = '\0';   /* 개행 제거의 관용구 */
        printf("line %d: [%s]\n", ++n, line);
    }

    /* ④ 왜 멈췄는가 — 파일 끝인가 오류인가를 구분한다 */
    if (ferror(f))      printf("stopped by an error\n");
    else if (feof(f))   printf("stopped at end of file\n");

    /* ⑤ 버퍼 크기보다 긴 줄은 잘려서 두 번에 나뉘어 온다 */
    rewind(f);
    char tiny[4];
    printf("with a 4-byte buffer:\n");
    for (int i = 0; i < 3 && fgets(tiny, sizeof tiny, f); i++)
        printf("  chunk %d: [%s] (has newline: %s)\n",
               i + 1, tiny, strchr(tiny, '\n') ? "yes" : "no");

    fclose(f);
    remove(path);
    return 0;
}
