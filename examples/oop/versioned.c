/* 자기 크기를 지니고 다니는 구조체 --- Windows API 의 cbSize 방식.
   라이브러리 판 2 가 끝에 멤버를 하나 더했다. 판 1 로 컴파일한 프로그램도 그대로 돈다 ---
   라이브러리는 부른 쪽이 말한 바이트만큼만 읽는다. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 판 1 헤더에 있던 것 */
struct win_opts_v1 { uint32_t size; int32_t width, height; };
/* 판 2 헤더: 끝에 멤버 하나 더, 다른 것은 움직이지 않았다 */
struct win_opts    { uint32_t size; int32_t width, height; int32_t dpi; };

static_assert(offsetof(struct win_opts, height) == offsetof(struct win_opts_v1, height),
              "old members must stay where they were");

/* 라이브러리(판 2) */
static bool win_open(const void *opts)
{
    uint32_t size;
    memcpy(&size, opts, sizeof size);        /* 첫 멤버는 언제나 크기 */
    struct win_opts o = { .dpi = 96 };       /* 부른 쪽에 없는 것은 기본값으로 */
    if (size == sizeof(struct win_opts_v1) || size == sizeof(struct win_opts))
        memcpy(&o, opts, size);              /* 부른 쪽이 가진 것보다 더 읽지 않는다 */
    else {
        printf("  size %u: not a version this library knows --- refused\n",
               (unsigned)size);
        return false;
    }
    printf("  size %2u: %d x %d at %d dpi\n", (unsigned)size, o.width, o.height, o.dpi);
    return true;
}

int main(void)
{
    puts("an old program (built with the version-1 header):");
    struct win_opts_v1 old = { .size = sizeof old, .width = 640, .height = 480 };
    win_open(&old);

    puts("a new program:");
    struct win_opts now = { .size = sizeof now, .width = 1920, .height = 1080,
                            .dpi = 144 };
    win_open(&now);

    puts("a program that forgot to fill in the size:");
    struct win_opts careless = { .width = 800, .height = 600 };   /* 크기는 0 */
    win_open(&careless);
    return 0;
}
