/* stack.c --- pcb_stack 이 무엇인지 아는 유일한 파일. 여기서 멤버를 바꿔도 쓰는 쪽은
   하나도 다시 컴파일하지 않는다 --- 쓰는 쪽은 멤버를 본 적이 없다. */
#include <stdint.h>
#include <stdlib.h>
#include "stack.h"

struct pcb_stack {
    size_t len, cap;
    int items[];                             /* 유연 배열 멤버: 할당 한 번 */
};

pcb_stack *pcb_stack_new(size_t capacity)
{
    if (capacity > (SIZE_MAX - sizeof(pcb_stack)) / sizeof(int)) return nullptr;
    pcb_stack *s = malloc(sizeof *s + capacity * sizeof(int));
    if (s) { s->len = 0; s->cap = capacity; }
    return s;
}

void pcb_stack_free(pcb_stack *s) { free(s); }

bool pcb_stack_push(pcb_stack *s, int v)
{
    if (s->len == s->cap) return false;
    s->items[s->len++] = v;
    return true;
}

bool pcb_stack_pop(pcb_stack *s, int *out)
{
    if (s->len == 0) return false;
    *out = s->items[--s->len];
    return true;
}

size_t pcb_stack_size(const pcb_stack *s) { return s->len; }
