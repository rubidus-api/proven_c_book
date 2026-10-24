#include <stdio.h>
#include <string.h>

/* memcpy 는 겹치는 영역을 허용하지 않는다. memmove 는 허용한다. */
static void show(const char *label, const char *s)
{
    printf("%-22s [%s]\n", label, s);
}

int main(void)
{
    char buf[32];

    /* 겹치지 않는 복사 — memcpy 가 맞는 자리 */
    strcpy(buf, "abcdefgh");
    char other[32];
    memcpy(other, buf, strlen(buf) + 1);
    show("no overlap (memcpy):", other);

    /* 한 칸 밀기 — 원본과 목적지가 겹친다. memmove 를 쓴다 */
    strcpy(buf, "abcdefgh");
    memmove(buf + 1, buf, 7);       /* 뒤로 한 칸 밀기 */
    buf[8] = '\0';
    show("shifting one slot up (memmove):", buf);

    /* 앞으로 당기기도 겹친다 */
    strcpy(buf, "abcdefgh");
    memmove(buf, buf + 2, 6 + 1);   /* 두 칸 당기기 */
    show("pulling two slots down (memmove):", buf);

    /* strtok 은 원본을 부순다 — 그리고 상태를 함수 안에 숨긴다 */
    char line[] = "name,age,city";
    printf("before strtok : [%s]\n", line);
    for (char *t = strtok(line, ","); t; t = strtok(NULL, ","))
        printf("  token: [%s]\n", t);
    printf("after strtok  : [%s]  <- the original was cut apart\n", line);
    return 0;
}
