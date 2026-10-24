/* When the header and the library disagree, the linker says nothing.

   Two translation units know a struct of the same name in two different shapes.
   The caller is still built from the old header; only the library was rebuilt.
   Nothing crosses the boundary here --- each side just prints what it believes. */
#include <stddef.h>
#include <stdio.h>
#include "conf_v1.h"

void lib_report(void);

int main(void)
{
    puts("the same name, two different shapes:");
    printf("  caller (not rebuilt, v1.0) believes:\n");
    printf("    sizeof(struct conf)          = %zu\n", sizeof(struct conf));
    printf("    offsetof(struct conf, width) = %zu\n", offsetof(struct conf, width));
    printf("    offsetof(struct conf, height)= %zu\n", offsetof(struct conf, height));
    lib_report();

    puts("\nnothing in the build complained:");
    puts("  the compiler saw one translation unit at a time,");
    puts("  and the linker matches names, not layouts.");
    printf("\nhad a struct crossed the boundary, the caller would write height at"
           " offset %zu\n", offsetof(struct conf, height));
    puts("  and the library would read it from offset 8 --- a silent wrong answer.");
    return 0;
}
