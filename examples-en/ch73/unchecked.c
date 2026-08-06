#include <stdio.h>
#include <stdlib.h>

/* Reading a port number out of a configuration line — the version that does not check for failure */
static int read_port_careless(const char *line) {
    int port = 8080;              /* the default */
    sscanf(line, "port=%d", &port);   /* the return value is not looked at */
    return port;
}

/* the same work — the version that checks for failure */
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
    const char *typo = "prot=9000";     /* a typo */

    printf("careless good: %d\n", read_port_careless(good));
    printf("careless typo: %d\n", read_port_careless(typo));
    printf("checked  typo: %d\n", read_port_checked(typo, 8080));

    /* strtol is the same: it reports failure as a value, but nobody forces you to look */
    long n = strtol("abc", nullptr, 10);
    printf("strtol(\"abc\") = %ld\n", n);
    return 0;
}
