#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    /* ① 성공한 호출도 errno 를 건드릴 수 있다 — 그래서 직전에 0 으로 놓는다 */
    errno = 0;
    FILE *ok = fopen("errno_demo_tmp.txt", "w");
    printf("errno after success = %d\n", errno);
    if (ok) { fputs("x", ok); fclose(ok); }

    /* ② 실패하면 errno 에 이유가 남는다 */
    errno = 0;
    FILE *f = fopen("/definitely/not/here.txt", "r");
    if (!f) {
        printf("failed: errno=%d, strerror=[%s]\n", errno, strerror(errno));
        perror("output of perror");     /* 앞에 문맥을 붙여 stderr 로 */
    }

    /* ③ 중간에 다른 호출이 끼면 errno 가 덮인다 */
    errno = 0;
    f = fopen("/definitely/not/here.txt", "r");
    int saved = errno;                    /* 곧바로 저장해 둔다 */
    printf("cleaning up...\n");           /* printf 가 errno 를 바꿀 수 있다 */
    printf("the errno we saved = %d (%s)\n", saved, strerror(saved));

    /* ④ strtol 은 errno 로 범위 초과를 알린다 */
    errno = 0;
    long v = strtol("999999999999999999999", NULL, 10);
    printf("strtol out of range: errno==ERANGE? %s (value=%ld)\n",
           errno == ERANGE ? "yes" : "no", v);

    remove("errno_demo_tmp.txt");
    return 0;
}
