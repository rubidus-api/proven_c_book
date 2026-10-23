#include <stdio.h>
#include <string.h>

/* A real overflow is outside the contract and cannot be demonstrated.
   So one big array is split into two "variables": an 8-byte name and a
   4-byte permission flag. Every access below stays inside frame. */
enum { NAME = 8, FLAG = 4 };
static unsigned char frame[NAME + FLAG];

static int flag(void)
{
    int f;
    memcpy(&f, frame + NAME, sizeof f);
    return f;
}

/* copies all it is given, never asks the size --- like strcpy */
static void copy_unchecked(unsigned char *dst, const char *src)
{
    while (*src)
        *dst++ = (unsigned char)*src++;
}

/* copies with the capacity of the destination */
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
