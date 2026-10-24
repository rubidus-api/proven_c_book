/* The ways of writing a constant — how notation settles value and type. */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

int main(void)
{
    puts("[integer constants — four bases and the C23 digit separator]");
    printf("  1234        = %d\n", 1234);          /* decimal */
    printf("  0755        = %d   <- a leading 0 means octal\n", 0755);
    printf("  0xFF        = %d\n", 0xFF);
    printf("  0b1010      = %d   <- C23 binary\n", 0b1010);
    printf("  1'000'000   = %d   <- the C23 digit separator\n", 1'000'000);
    printf("  0b11'10'11'01 = %d\n", 0b11'10'11'01);

    puts("\n[the suffix settles the type]");
    printf("  sizeof 1    = %zu, sizeof 1L  = %zu, sizeof 1LL = %zu\n",
           sizeof 1, sizeof 1L, sizeof 1LL);
    printf("  sizeof 1U   = %zu, sizeof 1wb = %zu  <- wb is the C23 _BitInt\n",
           sizeof 1U, sizeof 1wb);

    puts("\n[character constants — the prefix settles the type]");
    printf("  'a'   size %zu, value %d   <- in C a character constant is an int\n",
           sizeof 'a', 'a');
    printf("  u8'a' size %zu (char8_t)\n",  sizeof u8'a');
    printf("  u'a'  size %zu (char16_t)\n", sizeof u'a');
    printf("  U'a'  size %zu (char32_t)\n", sizeof U'a');
    printf("  L'a'  size %zu (wchar_t)\n",  sizeof L'a');
    /* A multi-character constant such as 'ab' has an implementation-defined
       value, so -Wmultichar warns. The warning is left on here; the measured
       value appears only in the table in the text. */

    puts("\n[escapes — octal and hexadecimal]");
    printf("  '\\101' = %d, '\\x41' = %d   <- both are 'A'\n", '\101', '\x41');
    printf("  \"\\x41\" \"1\" = \"%s\"        <- hex eats the longest run.\n",
           "\x41" "1");
    puts("    So split the string and let it join, rather than \"\\x411\".");

    puts("\n[floating constants]");
    printf("  3.14  1e3=%g  1.=%g  .5=%g\n", 1e3, 1., .5);
    printf("  0x1p-3 = %g            <- a hex float. The p exponent is required\n", 0x1p-3);
    printf("  sizeof 1.0 = %zu, 1.0f = %zu, 1.0L = %zu\n",
           sizeof 1.0, sizeof 1.0f, sizeof 1.0L);
    printf("  0.1 == 0.1f ? %s   <- a different suffix is a different value\n",
           (double)0.1f == 0.1 ? "yes" : "no");

    puts("\n[string literals]");
    printf("  sizeof \"abc\" = %zu        <- one more slot for the NUL\n", sizeof "abc");
    printf("  \"hello, \" \"world\" = \"%s\"  <- adjacent literals join into one\n",
           "hello, " "world");
    printf("  \"a\\0b\": strlen = %zu, sizeof = %zu  <- a NUL inside does not shrink the array\n",
           strlen("a\0b"), sizeof "a\0b");
    printf("  sizeof u8\"\\uAC00\" = %zu, u\"\\uAC00\" = %zu, U\"\\uAC00\" = %zu\n",
           sizeof u8"\uAC00", sizeof u"\uAC00", sizeof U"\uAC00");
    return 0;
}
