/* Streams have an orientation — byte or wide, and it cannot be undone. */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>

/* fwide(stream, 0) only asks.
   Negative = byte-oriented, 0 = not yet decided, positive = wide-oriented. */
static const char *orientation(FILE *f)
{
    int w = fwide(f, 0);
    return w < 0 ? "byte" : w > 0 ? "wide" : "none yet";
}

int main(void)
{
    setlocale(LC_ALL, "");

    /* Open a file and watch the orientation settle */
    FILE *f = tmpfile();
    if (!f) { perror("tmpfile"); return 1; }

    printf("a freshly opened stream:   %s\n", orientation(f));

    /* fwide can settle it in advance — but only while there is none */
    fwide(f, 1);
    printf("after fwide(f, 1):         %s\n", orientation(f));

    fputws(L"a line written wide\n", f);
    printf("after fputws:              %s\n", orientation(f));

    /* Writing byte functions to a wide-oriented stream is undefined behaviour,
       so we do not try it — we only show that the orientation will not move. */
    fwide(f, -1);
    printf("fwide(f, -1) to undo it:   %s (it does not move)\n", orientation(f));

    rewind(f);
    wchar_t line[64];
    if (fgetws(line, 64, f)) printf("read back:                 %ls", line);
    fclose(f);

    /* A second stream, settled the other way */
    FILE *g = tmpfile();
    if (!g) { perror("tmpfile"); return 1; }
    printf("\na second stream:           %s\n", orientation(g));
    fputs("a line written byte-wise\n", g);
    printf("after fputs:               %s\n", orientation(g));
    fclose(g);

    /* The standard streams work the same way. This program has used printf… */
    printf("\nthe orientation of stdout: %s\n", orientation(stdout));
    puts("-> because this program started with printf.");
    puts("   Mixing wprintf in from here is undefined behaviour.");
    return 0;
}
