/* the freshly built library's translation unit --- it sees the 1.1 header. */
#include <stddef.h>
#include <stdio.h>
#include "conf_v2.h"

void lib_report(void)
{
    printf("  library (rebuilt, v1.1) believes:\n");
    printf("    sizeof(struct conf)          = %zu\n", sizeof(struct conf));
    printf("    offsetof(struct conf, width) = %zu\n", offsetof(struct conf, width));
    printf("    offsetof(struct conf, height)= %zu\n", offsetof(struct conf, height));
}
