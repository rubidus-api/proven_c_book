/* A locale is process-global state — what it starts as, and what changes it. */
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

/* setlocale returns "the name of the locale now in force".
   Pass NULL as the second argument to ask without changing anything. */
static void show_current(const char *when)
{
    printf("%s LC_ALL=%s\n", when, setlocale(LC_ALL, NULL));
}

int main(void)
{
    /* (1) A program always starts in the "C" locale (standard §7.11.1.1p4) */
    show_current("at startup:         ");

    /* (2) "" means "the locale the environment says".
           It reads LC_ALL, then LC_xxx, then LANG. */
    const char *applied = setlocale(LC_ALL, "");
    if (applied)
        printf("setlocale(LC_ALL,\"\"):  %s\n", applied);
    else
        puts("setlocale(LC_ALL,\"\") failed — the environment has no locale");

    /* (3) How many bytes one character may take depends on the locale.
           MB_CUR_MAX looks like a constant but reads *the current locale*. */
    printf("MB_CUR_MAX:             %zu\n", (size_t)MB_CUR_MAX);

    /* (4) If the requested locale is not on this machine, setlocale returns
           null. So always look at the return value — it fails quietly. */
    static const char *candidates[] = {
        "C", "C.UTF-8", "en_US.UTF-8", "ko_KR.UTF-8", "ko_KR.EUC-KR",
        "de_DE.UTF-8", "tr_TR.UTF-8", "ja_JP.UTF-8", "no_SUCH.locale",
    };

    puts("\nAvailable on this machine?");
    char saved[128];
    snprintf(saved, sizeof saved, "%s", setlocale(LC_ALL, NULL));

    for (size_t i = 0; i < sizeof candidates / sizeof *candidates; i++) {
        const char *got = setlocale(LC_ALL, candidates[i]);
        if (got) printf("  %-15s yes  MB_CUR_MAX=%zu\n",
                        candidates[i], (size_t)MB_CUR_MAX);
        else     printf("  %-15s no\n", candidates[i]);
    }

    /* (5) Put it back — hand the name string in again. But the pointer
           setlocale returned may be overwritten by the next call, so copy it. */
    setlocale(LC_ALL, saved);
    show_current("\nrestored:           ");

    /* (6) Categories can be set separately. This is the idiom in practice —
           what people see follows the environment, numbers a machine reads
           stay in "C". */
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    printf("mixed — LC_CTYPE=%s, LC_NUMERIC=%s\n",
           setlocale(LC_CTYPE, NULL), setlocale(LC_NUMERIC, NULL));
    /* Asking LC_ALL lists every category when they are not all the same */
    printf("asking LC_ALL gives: %.60s...\n", setlocale(LC_ALL, NULL));
    return 0;
}
