#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 줄을 안전하게 읽는 두 가지 방법과 그 차이 */

static const char *PATH = "reading_demo.txt";

static void make_file(void)
{
    FILE *f = fopen(PATH, "w");
    if (!f) return;
    fputs("short\na line that is definitely too long\nend\n", f);
    fclose(f);
}

/* ① 고정 버퍼 + 잘림 확인 */
static void read_fixed(void)
{
    char buf[8];
    FILE *f = fopen(PATH, "r");
    if (!f) return;
    while (fgets(buf, sizeof buf, f)) {
        int complete = strchr(buf, '\n') != NULL;
        if (complete) buf[strcspn(buf, "\n")] = '\0';
        printf("  [%s]%s\n", buf, complete ? "" : "   <- cut (the line continues)");
    }
    fclose(f);
}

/* ② 필요한 만큼 늘려 가며 읽기 — 표준 함수만으로 */
static char *read_line(FILE *f)
{
    size_t cap = 8, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            char *nbuf = realloc(buf, cap * 2);
            if (!nbuf) { free(buf); return NULL; }  /* 실패해도 원본은 살아 있다 */
            buf = nbuf;
            cap *= 2;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void)
{
    make_file();

    printf("a fixed 8-byte buffer:\n");
    read_fixed();

    printf("growing the buffer as we read:\n");
    FILE *f = fopen(PATH, "r");
    if (f) {
        char *line;
        while ((line = read_line(f)) != NULL) {
            printf("  [%s] (%zu bytes)\n", line, strlen(line));
            free(line);
        }
        fclose(f);
    }
    remove(PATH);
    return 0;
}
