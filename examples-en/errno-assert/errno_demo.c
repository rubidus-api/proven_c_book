#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    /* (1) even a successful call may touch errno — so it is set to 0 just before */
    errno = 0;
    FILE *ok = fopen("errno_demo_tmp.txt", "w");
    printf("errno after success = %d\n", errno);
    if (ok) { fputs("x", ok); fclose(ok); }

    /* (2) on failure the reason is left in errno */
    errno = 0;
    FILE *f = fopen("/definitely/not/here.txt", "r");
    if (!f) {
        printf("failure: errno=%d, strerror=[%s]\n", errno, strerror(errno));
        perror("perror output");   /* context attached in front, sent to stderr */
    }

    /* (3) let another call in between and errno is overwritten */
    errno = 0;
    f = fopen("/definitely/not/here.txt", "r");
    int saved = errno;                    /* saved at once */
    printf("cleaning up...\n");           /* printf may change errno */
    printf("the saved errno=%d (%s)\n", saved, strerror(saved));

    /* (4) strtol reports going out of range through errno */
    errno = 0;
    long v = strtol("999999999999999999999", NULL, 10);
    printf("strtol out of range: errno==ERANGE? %s (value=%ld)\n",
           errno == ERANGE ? "yes" : "no", v);

    remove("errno_demo_tmp.txt");
    return 0;
}
