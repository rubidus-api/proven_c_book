/* main.c --- 쓰는 쪽. 포인터를 쥐고 함수를 부를 수 있을 뿐이다. */
#include <stdio.h>
#include "stack.h"

int main(void)
{
    pcb_stack *s = pcb_stack_new(3);
    if (!s) return 1;
    for (int i = 1; i <= 4; i++)
        printf("push %d: %s\n", i, pcb_stack_push(s, i * 10) ? "ok" : "full");
    printf("size %zu\n", pcb_stack_size(s));
    int v;
    while (pcb_stack_pop(s, &v)) printf("pop %d\n", v);
    pcb_stack_free(s);                       /* 소멸자는 저절로 돌지 않는다 */
    /* 여기서 sizeof(pcb_stack) 는 컴파일되지 않는다: 이 파일에서 그 타입은 불완전하다. */
    return 0;
}
