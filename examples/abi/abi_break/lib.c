/* 새로 빌드된 라이브러리 쪽 번역 단위 --- 1.1 의 헤더를 본다. */
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
