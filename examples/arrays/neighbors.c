#include <stdio.h>
#include <string.h>

/* 진짜 넘침은 계약 밖이라 시연할 수 없다. 그래서 큰 배열 하나를
   「이름 8칸 + 권한 표시 4칸」의 두 변수처럼 나눠 쓰는 모형을 만든다.
   아래의 모든 접근은 frame 안에서 일어나므로 계약 안이다. */
enum { NAME = 8, FLAG = 4 };
static unsigned char frame[NAME + FLAG];

static int flag(void)
{
    int f;
    memcpy(&f, frame + NAME, sizeof f);
    return f;
}

/* 길이를 묻지 않고 받은 만큼 쓰는 복사 --- strcpy 와 같은 꼴 */
static void copy_unchecked(unsigned char *dst, const char *src)
{
    while (*src)
        *dst++ = (unsigned char)*src++;
}

/* 그릇의 크기를 함께 받는 복사 */
static size_t copy_checked(unsigned char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    while (src[n] && n < cap) {
        dst[n] = (unsigned char)src[n];
        n++;
    }
    return n;
}

int main(void)
{
    puts("[1] a short name stays inside its 8 bytes");
    memset(frame, 0, sizeof frame);
    copy_unchecked(frame, "kim");
    printf("flag = %d\n", flag());

    puts("\n[2] a long name runs into the neighbour");
    memset(frame, 0, sizeof frame);
    copy_unchecked(frame, "abcdefgh\x01");
    printf("flag = %d   <- nobody wrote to flag on purpose\n", flag());

    puts("\n[3] the same input, copied with the capacity");
    memset(frame, 0, sizeof frame);
    size_t n = copy_checked(frame, NAME, "abcdefgh\x01");
    printf("copied %zu bytes, flag = %d\n", n, flag());
    return 0;
}
