#include <stdio.h>
#include <stdlib.h>

/* 설정 줄에서 포트 번호를 읽는다 — 실패를 확인하지 않는 판 */
static int read_port_careless(const char *line) {
    int port = 8080;              /* 기본값 */
    sscanf(line, "port=%d", &port);   /* 반환값을 보지 않는다 */
    return port;
}

/* 같은 일 — 실패를 확인하는 판 */
static int read_port_checked(const char *line, int fallback) {
    int port;
    if (sscanf(line, "port=%d", &port) != 1) {
        printf("  (parse failed, keeping %d)\n", fallback);
        return fallback;
    }
    return port;
}

int main(void) {
    const char *good = "port=9000";
    const char *typo = "prot=9000";     /* 오타 */

    printf("careless good: %d\n", read_port_careless(good));
    printf("careless typo: %d\n", read_port_careless(typo));
    printf("checked  typo: %d\n", read_port_checked(typo, 8080));

    /* strtol 도 마찬가지다: 실패를 값으로 알리지만 아무도 강제하지 않는다 */
    long n = strtol("abc", nullptr, 10);
    printf("strtol(\"abc\") = %ld\n", n);
    return 0;
}
