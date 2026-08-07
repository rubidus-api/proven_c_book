/* struct lconv — everything a locale knows about numbers and money. */
#include <limits.h>
#include <locale.h>
#include <stdio.h>

/* String members: show emptiness visibly */
static const char *str(const char *s) { return (s && *s) ? s : "(empty)"; }

/* char members: CHAR_MAX means "this locale does not say" */
static void show_char(const char *name, char v)
{
    if (v == CHAR_MAX) printf("  %-20s CHAR_MAX (not specified)\n", name);
    else               printf("  %-20s %d\n", name, v);
}

/* How the grouping string is encoded:
     each byte is the group size at that position. CHAR_MAX means "no more
     grouping", 0 means "repeat the previous value forever". "\3" is groups of
     three without end; en_IN uses "\3\2". */
static void show_grouping(const char *name, const char *g)
{
    printf("%-20s ", name);
    if (!*g) { puts("(empty — no grouping)"); return; }
    for (const char *p = g; *p; p++) {
        if (*p == CHAR_MAX) printf("CHAR_MAX ");
        else                printf("%d ", *p);
    }
    puts("");
}

static void dump_all(void)
{
    struct lconv *L = localeconv();

    puts("[plain numbers]");
    printf("  %-20s \"%s\"\n", "decimal_point", str(L->decimal_point));
    printf("  %-20s \"%s\"\n", "thousands_sep", str(L->thousands_sep));
    show_grouping("  grouping", L->grouping);

    puts("[monetary numbers]");
    printf("  %-20s \"%s\"\n", "mon_decimal_point", str(L->mon_decimal_point));
    printf("  %-20s \"%s\"\n", "mon_thousands_sep", str(L->mon_thousands_sep));
    show_grouping("  mon_grouping", L->mon_grouping);
    printf("  %-20s \"%s\"\n", "positive_sign", str(L->positive_sign));
    printf("  %-20s \"%s\"\n", "negative_sign", str(L->negative_sign));
    printf("  %-20s \"%s\"\n", "currency_symbol", str(L->currency_symbol));
    printf("  %-20s \"%s\"\n", "int_curr_symbol", str(L->int_curr_symbol));
    show_char("frac_digits", L->frac_digits);
    show_char("int_frac_digits", L->int_frac_digits);

    puts("[how the monetary form is assembled]");
    show_char("p_cs_precedes", L->p_cs_precedes);
    show_char("p_sep_by_space", L->p_sep_by_space);
    show_char("p_sign_posn", L->p_sign_posn);
    show_char("n_cs_precedes", L->n_cs_precedes);
    show_char("n_sep_by_space", L->n_sep_by_space);
    show_char("n_sign_posn", L->n_sign_posn);
}

/* 여러 로케일을 한 줄씩 견주어 본다 */
static void compare_row(const char *name)
{
    if (!setlocale(LC_ALL, name)) {
        printf("  %-14s (not on this machine)\n", name);
        return;
    }
    struct lconv *L = localeconv();
    printf("  %-14s point \"%s\"  thousands \"%s\"  symbol \"%s\"  ISO \"%s\"\n",
           name, str(L->decimal_point), str(L->thousands_sep),
           str(L->currency_symbol), str(L->int_curr_symbol));
}

int main(void)
{
    /* A program starts in "C". The standard fixes these values (§7.11). */
    puts("=== the \"C\" locale — the values the standard fixes ===");
    dump_all();

    puts("\n=== compared across locales ===");
    static const char *names[] = {
        "C", "en_US.UTF-8", "ko_KR.UTF-8", "de_DE.UTF-8",
        "fr_FR.UTF-8", "ja_JP.UTF-8", "en_IN.UTF-8",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        compare_row(names[i]);

    /* Grouping is not always three digits — the Indian system */
    puts("\n=== grouping is not always three digits ===");
    static const char *grp[] = { "C", "en_US.UTF-8", "en_IN.UTF-8" };
    for (size_t i = 0; i < sizeof grp / sizeof *grp; i++) {
        if (!setlocale(LC_ALL, grp[i])) {
            printf("  %-14s (not on this machine)\n", grp[i]);
            continue;
        }
        printf("  %-14s ", grp[i]);
        show_grouping("grouping", localeconv()->grouping);
    }
    puts("  -> en_IN's 3,2 means \"three from the right, then two at a time\" —");
    puts("    12345678 is grouped as 1,23,45,678 (the lakh/crore system).");

    /* Change the decimal point and printf and strtod change together */
    puts("\n=== LC_NUMERIC changes printf ===");
    static const char *num_locales[] = { "C", "de_DE.UTF-8", "fr_FR.UTF-8" };
    for (size_t i = 0; i < sizeof num_locales / sizeof *num_locales; i++) {
        if (!setlocale(LC_NUMERIC, num_locales[i])) {
            printf("  %-14s (not on this machine)\n", num_locales[i]);
            continue;
        }
        printf("  %-14s printf(\"%%.2f\", 1234.5) -> %.2f\n",
               num_locales[i], 1234.5);
    }
    return 0;
}
