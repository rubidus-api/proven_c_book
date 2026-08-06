#include <stdio.h>
#include <string.h>

/* The common way: a path assembled into a fixed buffer.
   Inside this function the compiler cannot know the length of dir or name. */
static int build_path(char *out, size_t cap, const char *dir, const char *name) {
    return snprintf(out, cap, "%s/%s", dir, name);   /* if it does not fit, it cuts quietly */
}

int main(void) {
    char path[24];

    build_path(path, sizeof path, "/var/log", "app.log");
    printf("fits      : %s\n", path);

    build_path(path, sizeof path, "/var/log/service/http", "access.log");
    printf("truncated : %s\n", path);
    printf("            length=%zu, buffer=%zu\n", strlen(path), sizeof path);

    /* to find out whether it was truncated, the return value must be looked at */
    int need = build_path(path, sizeof path, "/var/log/service/http", "access.log");
    if (need >= (int)sizeof path)
        printf("detected  : needed %d bytes, had %zu\n", need + 1, sizeof path);
    return 0;
}
