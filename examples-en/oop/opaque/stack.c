/* stack.c --- the only file that knows what a pcb_stack is. Members can change here
   without recompiling a single user, because no user ever saw them. */
#include <stdint.h>
#include <stdlib.h>
#include "stack.h"

struct pcb_stack {
    size_t len, cap;
    int items[];                             /* flexible array member: one allocation */
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
