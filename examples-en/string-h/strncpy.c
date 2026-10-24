#include <stdio.h>
#include <string.h>

static void dump(const char *label, const char *buf, size_t n)
{
    printf("%-14s", label);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0)       printf(" \\0");
        else if (c >= 32) printf("  %c", c);
        else              printf(" %02x", c);
    }
    printf("\n");
}

/* Put it behind a function boundary so the compiler cannot see the source's
   length. (Otherwise gcc catches it with -Wstringop-truncation.) */
static void copy_into(char *dst, size_t cap, const char *src)
{
    strncpy(dst, src, cap);
}

int main(void)
{
    /* (1) it fills every spare place with 0 — expensive on a large buffer */
    char pad[10];
    memset(pad, 'X', sizeof pad);
    copy_into(pad, sizeof pad, "abc");
    dump("short source:", pad, sizeof pad);

    /* (2) if it fits exactly or overflows, no NUL is attached — it stops being a string */
    char tight[4];
    memset(tight, 'X', sizeof tight);
    copy_into(tight, sizeof tight, "abcd");
    dump("exact fit:", tight, sizeof tight);
    printf("               there is no NUL — printing this with %%s is outside the contract\n");

    /* (3) to use it safely, close the last place by hand */
    char safe[4];
    copy_into(safe, sizeof safe - 1, "abcd");
    safe[sizeof safe - 1] = '\0';
    printf("closed by hand : [%s]\n", safe);

    /* (4) to know whether it was truncated, the length must be measured separately after all */
    const char *src = "abcd";
    printf("truncated?     : %s\n", strlen(src) >= sizeof safe ? "truncated" : "whole");
    return 0;
}
