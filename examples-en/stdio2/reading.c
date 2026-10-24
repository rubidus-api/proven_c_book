#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Two safe ways of reading a line, and the difference between them */

static const char *PATH = "reading_demo.txt";

static void make_file(void)
{
    FILE *f = fopen(PATH, "w");
    if (!f) return;
    fputs("short\na line that is definitely too long\nend\n", f);
    fclose(f);
}

/* (1) a fixed buffer plus a truncation check */
static void read_fixed(void)
{
    char buf[8];
    FILE *f = fopen(PATH, "r");
    if (!f) return;
    while (fgets(buf, sizeof buf, f)) {
        int complete = strchr(buf, '\n') != NULL;
        if (complete) buf[strcspn(buf, "\n")] = '\0';
        printf("  [%s]%s\n", buf, complete ? "" : "   <- truncated (the line continues)");
    }
    fclose(f);
}

/* (2) growing as needed while reading — with standard functions alone */
static char *read_line(FILE *f)
{
    size_t cap = 8, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            char *nbuf = realloc(buf, cap * 2);
            if (!nbuf) { free(buf); return NULL; }  /* even on failure the original survives */
            buf = nbuf;
            cap *= 2;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void)
{
    make_file();

    printf("a fixed 8-byte buffer:\n");
    read_fixed();

    printf("growing while reading:\n");
    FILE *f = fopen(PATH, "r");
    if (f) {
        char *line;
        while ((line = read_line(f)) != NULL) {
            printf("  [%s] (%zu bytes)\n", line, strlen(line));
            free(line);
        }
        fclose(f);
    }
    remove(PATH);
    return 0;
}
