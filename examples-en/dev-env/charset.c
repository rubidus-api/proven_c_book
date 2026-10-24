/* What bytes and values a literal actually becomes, layer by layer.
   The same source gives a different first line under different compile
   options (see the measured table in the text). */
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <uchar.h>

static void dump(const char *tag, const char *p, size_t n)
{
    printf("  %-28s", tag);
    for (size_t i = 0; i < n; i++) printf(" %02X", (unsigned char)p[i]);
    puts("");
}

int main(void)
{
    const char    *plain = "\uAC00";   /* the literal encoding — implementation-defined */
    const char8_t *utf8  = u8"\uAC00"; /* always UTF-8 — the standard nails it down */

    puts("[the bytes of a string literal]");
    dump("\"\\uAC00\"   (literal encoding)", plain, strlen(plain));
    dump("u8\"\\uAC00\" (always UTF-8)", (const char *)utf8, strlen((const char *)utf8));

    puts("\n[the code units of a wide literal]");
    printf("  u\"\\uAC00\"[0] = U+%04X   (char16_t, UTF-16)\n",  (unsigned)u"\uAC00"[0]);
    printf("  U\"\\uAC00\"[0] = U+%04X   (char32_t, UTF-32)\n",  (unsigned)U"\uAC00"[0]);
    printf("  L\"\\uAC00\"[0] = U+%04X   (wchar_t, %zu bytes)\n",
           (unsigned)L"\uAC00"[0], sizeof(wchar_t));

    puts("\n[what the implementation says it guarantees]");
#ifdef __STDC_ISO_10646__
    printf("  __STDC_ISO_10646__ = %ldL  -> wchar_t is Unicode\n",
           (long)__STDC_ISO_10646__);
#else
    puts("  __STDC_ISO_10646__ undefined -> the wchar_t encoding is implementation-defined");
#endif
#ifdef __STDC_UTF_16__
    printf("  __STDC_UTF_16__    = %d      -> char16_t is UTF-16\n", __STDC_UTF_16__);
#endif
#ifdef __STDC_UTF_32__
    printf("  __STDC_UTF_32__    = %d      -> char32_t is UTF-32\n", __STDC_UTF_32__);
#endif

    puts("\n[the basic character set does not move]");
    printf("  'A' = %d, '0' = %d, sizeof \"A\" = %zu\n", 'A', '0', sizeof "A");
    return 0;
}
