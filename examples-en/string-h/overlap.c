#include <stdio.h>
#include <string.h>

/* memcpy does not allow overlapping regions. memmove does. */
static void show(const char *label, const char *s)
{
    printf("%-22s [%s]\n", label, s);
}

int main(void)
{
    char buf[32];

    /* a copy that does not overlap — the place where memcpy fits */
    strcpy(buf, "abcdefgh");
    char other[32];
    memcpy(other, buf, strlen(buf) + 1);
    show("no overlap (memcpy):", other);

    /* shifting by one — source and destination overlap, so memmove is used */
    strcpy(buf, "abcdefgh");
    memmove(buf + 1, buf, 7);       /* shift one place back */
    buf[8] = '\0';
    show("shift by 1 (memmove):", buf);

    /* pulling forward overlaps as well */
    strcpy(buf, "abcdefgh");
    memmove(buf, buf + 2, 6 + 1);   /* pull two places forward */
    show("pull by 2 (memmove):", buf);

    /* strtok destroys the original — and hides state inside the function */
    char line[] = "name,age,city";
    printf("before strtok : [%s]\n", line);
    for (char *t = strtok(line, ","); t; t = strtok(NULL, ","))
        printf("  token: [%s]\n", t);
    printf("after strtok  : [%s]  <- the original has been cut\n", line);
    return 0;
}
