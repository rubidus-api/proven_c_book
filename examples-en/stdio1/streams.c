#include <stdio.h>
#include <string.h>

/* Three realities of a stream — buffering, return values, knowing the end */
int main(void)
{
    const char *path = "stream_demo.txt";

    /* (1) writing: fopen returns null on failure */
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return 1; }

    /* fprintf can fail too — the return value is the number of characters printed */
    int written = fprintf(f, "one\ntwo\nthree\n");
    printf("fprintf wrote %d chars\n", written);

    /* (2) closing can fail too: a failure while flushing the buffer surfaces here */
    if (fclose(f) != 0) { perror("fclose"); return 1; }

    /* (3) reading: a line at a time */
    f = fopen(path, "r");
    if (!f) { perror("fopen"); return 1; }

    char line[64];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = '\0';   /* the idiom for stripping the newline */
        printf("line %d: [%s]\n", ++n, line);
    }

    /* (4) why did it stop — end of file or an error? */
    if (ferror(f))      printf("stopped by an error\n");
    else if (feof(f))   printf("stopped at end of file\n");

    /* (5) a line longer than the buffer is cut and arrives in pieces */
    rewind(f);
    char tiny[4];
    printf("with a 4-byte buffer:\n");
    for (int i = 0; i < 3 && fgets(tiny, sizeof tiny, f); i++)
        printf("  chunk %d: [%s] (has newline: %s)\n",
               i + 1, tiny, strchr(tiny, '\n') ? "yes" : "no");

    fclose(f);
    remove(path);
    return 0;
}
