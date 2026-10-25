/* main.c --- a user. It can hold a pointer and call the functions; that is all. */
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
    pcb_stack_free(s);                       /* no destructor runs by itself */
    /* sizeof(pcb_stack) here would not compile: the type is incomplete in this file. */
    return 0;
}
