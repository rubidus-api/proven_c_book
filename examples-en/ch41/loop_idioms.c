/* A few loop idioms — and the places that bite quietly. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int a[] = { 10, 20, 30, 40, 50 };
    size_t n = sizeof a / sizeof a[0];

    puts("[walking backwards — with size_t you cannot write i >= 0]");
    printf("  the right shape (i-- > 0): ");
    for (size_t i = n; i-- > 0; ) printf("%d ", a[i]);
    puts("");
    puts("  the wrong shape (size_t i = n-1; i >= 0; i--) loops forever —");
    puts("  after 0 an unsigned value wraps to SIZE_MAX, so the test never fails.");
    printf("  with a signed index that shape works too: ");
    for (int i = (int)n - 1; i >= 0; i--) printf("%d ", a[i]);
    puts("");

    puts("\n[the comma operator — closing in from both ends]");
    printf("  reversed: ");
    int b[] = { 1, 2, 3, 4, 5, 6 };
    size_t m = sizeof b / sizeof b[0];
    for (size_t i = 0, j = m - 1; i < j; i++, j--) {
        int t = b[i]; b[i] = b[j]; b[j] = t;
    }
    for (size_t i = 0; i < m; i++) printf("%d ", b[i]);
    puts("");

    puts("\n[a sentinel loop — read and test in one breath]");
    const char *text = "hi!\n";
    const char *p = text;
    int c, count = 0;
    while ((c = *p++) != '\0') {          /* the parentheses are required */
        if (c == '\n') printf("\\n ");
        else printf("%c ", c);
        count++;
    }
    printf("\n  read %d characters\n", count);

    puts("\n[do not call strlen in the loop condition]");
    const char *s = "measure once";
    size_t len = strlen(s);               /* measured once */
    size_t vowels = 0;
    for (size_t i = 0; i < len; i++)
        if (strchr("aeiou", s[i]) && s[i]) vowels++;
    printf("  \"%s\" has %zu vowels (length %zu, measured once)\n", s, vowels, len);

    puts("\n[write the intent when a body is empty]");
    size_t skip = 0;
    const char *q = "   value";
    while (q[skip] == ' ')
        skip++;                            /* written with a body */
    printf("  skipped %zu leading spaces\n", skip);
    puts("  while (q[skip++] == ' ') ;  is shorter, but that lone");
    puts("  semicolon goes unnoticed, and that is where accidents come from.");
    return 0;
}
