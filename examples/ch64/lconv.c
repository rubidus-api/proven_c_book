/* struct lconv — 로케일이 숫자와 통화에 대해 아는 것 전부. */
#include <limits.h>
#include <locale.h>
#include <stdio.h>

/* 문자열 멤버: 비어 있으면 눈에 보이게 표시한다 */
static const char *str(const char *s) { return (s && *s) ? s : "(빈 문자열)"; }

/* char 멤버: CHAR_MAX 는 "이 로케일은 정하지 않았다"는 뜻이다 */
static void show_char(const char *name, char v)
{
    if (v == CHAR_MAX) printf("  %-20s CHAR_MAX (정해지지 않음)\n", name);
    else               printf("  %-20s %d\n", name, v);
}

/* grouping 문자열의 인코딩:
     각 바이트가 그 자리의 그룹 자릿수. CHAR_MAX 면 "더는 묶지 않는다",
     0 이면 "직전 값을 계속 되풀이한다". "\3" 은 3자리씩 무한, en_IN 은 "\3\2". */
static void show_grouping(const char *name, const char *g)
{
    printf("%-20s ", name);
    if (!*g) { puts("(빈 문자열 — 자리 묶음 없음)"); return; }
    for (const char *p = g; *p; p++) {
        if (*p == CHAR_MAX) printf("CHAR_MAX ");
        else                printf("%d ", *p);
    }
    puts("");
}

static void dump_all(void)
{
    struct lconv *L = localeconv();

    puts("[비통화 숫자]");
    printf("  %-20s \"%s\"\n", "decimal_point", str(L->decimal_point));
    printf("  %-20s \"%s\"\n", "thousands_sep", str(L->thousands_sep));
    show_grouping("  grouping", L->grouping);

    puts("[통화 숫자]");
    printf("  %-20s \"%s\"\n", "mon_decimal_point", str(L->mon_decimal_point));
    printf("  %-20s \"%s\"\n", "mon_thousands_sep", str(L->mon_thousands_sep));
    show_grouping("  mon_grouping", L->mon_grouping);
    printf("  %-20s \"%s\"\n", "positive_sign", str(L->positive_sign));
    printf("  %-20s \"%s\"\n", "negative_sign", str(L->negative_sign));
    printf("  %-20s \"%s\"\n", "currency_symbol", str(L->currency_symbol));
    printf("  %-20s \"%s\"\n", "int_curr_symbol", str(L->int_curr_symbol));
    show_char("frac_digits", L->frac_digits);
    show_char("int_frac_digits", L->int_frac_digits);

    puts("[통화 표기의 조립 규칙]");
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
        printf("  %-14s (이 기계에 없음)\n", name);
        return;
    }
    struct lconv *L = localeconv();
    printf("  %-14s 소수점 \"%s\"  천단위 \"%s\"  통화 \"%s\"  ISO \"%s\"\n",
           name, str(L->decimal_point), str(L->thousands_sep),
           str(L->currency_symbol), str(L->int_curr_symbol));
}

int main(void)
{
    /* 시작 로케일은 "C" 다. 표준이 이 값들을 직접 규정한다(§7.11). */
    puts("=== \"C\" 로케일 — 표준이 규정한 값 ===");
    dump_all();

    puts("\n=== 로케일별 비교 ===");
    static const char *names[] = {
        "C", "en_US.UTF-8", "ko_KR.UTF-8", "de_DE.UTF-8",
        "fr_FR.UTF-8", "ja_JP.UTF-8", "en_IN.UTF-8",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        compare_row(names[i]);

    /* 자리 묶음이 3자리 고정이 아닌 로케일이 있다 — 인도식 표기 */
    puts("\n=== 자리 묶음은 3자리 고정이 아니다 ===");
    static const char *grp[] = { "C", "en_US.UTF-8", "en_IN.UTF-8" };
    for (size_t i = 0; i < sizeof grp / sizeof *grp; i++) {
        if (!setlocale(LC_ALL, grp[i])) {
            printf("  %-14s (이 기계에 없음)\n", grp[i]);
            continue;
        }
        printf("  %-14s ", grp[i]);
        show_grouping("grouping", localeconv()->grouping);
    }
    puts("  → en_IN 의 3,2 는 \"오른쪽부터 3자리, 그 뒤로는 2자리씩\" —");
    puts("    12345678 이 1,23,45,678 로 묶인다(라크·크로르 셈법).");

    /* 소수점이 바뀌면 printf 와 strtod 가 함께 바뀐다 */
    puts("\n=== LC_NUMERIC 이 printf 를 바꾼다 ===");
    static const char *num_locales[] = { "C", "de_DE.UTF-8", "fr_FR.UTF-8" };
    for (size_t i = 0; i < sizeof num_locales / sizeof *num_locales; i++) {
        if (!setlocale(LC_NUMERIC, num_locales[i])) {
            printf("  %-14s (이 기계에 없음)\n", num_locales[i]);
            continue;
        }
        printf("  %-14s printf(\"%%.2f\", 1234.5) → %.2f\n",
               num_locales[i], 1234.5);
    }
    return 0;
}
