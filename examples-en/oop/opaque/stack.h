/* stack.h --- what the user of the library sees. The struct is declared, never defined:
   outside stack.c nobody knows its size or members, so nobody can depend on them. */
#ifndef PCB_STACK_H
#define PCB_STACK_H
#include <stddef.h>

typedef struct pcb_stack pcb_stack;          /* incomplete type: a name, nothing more */

pcb_stack *pcb_stack_new(size_t capacity);   /* the factory: the only way to get one */
void       pcb_stack_free(pcb_stack *s);     /* its pair: the user must call it */

bool   pcb_stack_push(pcb_stack *s, int v);  /* the object always comes first */
bool   pcb_stack_pop(pcb_stack *s, int *out);
size_t pcb_stack_size(const pcb_stack *s);   /* const: only reads */
#endif
