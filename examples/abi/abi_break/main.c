/* 헤더와 라이브러리가 어긋나면 --- 링커는 아무 말도 하지 않는다.

   두 번역 단위가 같은 이름의 구조체를 서로 다른 모양으로 알고 있다.
   호출자는 옛 헤더로 빌드된 채 남아 있고, 라이브러리만 새로 빌드된 상황이다.
   여기서는 구조체를 실제로 주고받지 않는다 --- 각자 무엇을 믿는지만 찍는다. */
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
