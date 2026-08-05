#include <stdio.h>
#include <string.h>

/* 흔한 방식: 고정 버퍼에 경로를 조립한다.
   컴파일러는 이 함수 안에서 dir/name 의 길이를 알 수 없다. */
static int build_path(char *out, size_t cap, const char *dir, const char *name) {
    return snprintf(out, cap, "%s/%s", dir, name);   /* 넘치면 조용히 자른다 */
}

int main(void) {
    char path[24];

    build_path(path, sizeof path, "/var/log", "app.log");
    printf("fits      : %s\n", path);

    build_path(path, sizeof path, "/var/log/service/http", "access.log");
    printf("truncated : %s\n", path);
    printf("            length=%zu, buffer=%zu\n", strlen(path), sizeof path);

    /* 잘렸는지 알아내려면 반환값을 보아야 한다 */
    int need = build_path(path, sizeof path, "/var/log/service/http", "access.log");
    if (need >= (int)sizeof path)
        printf("detected  : needed %d bytes, had %zu\n", need + 1, sizeof path);
    return 0;
}
