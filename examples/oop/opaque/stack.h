/* stack.h --- 라이브러리를 쓰는 쪽이 보는 것. 구조체는 선언만 하고 정의하지 않는다:
   stack.c 밖에서는 아무도 크기와 멤버를 모르므로, 아무도 그것에 기댈 수 없다. */
#ifndef PCB_STACK_H
#define PCB_STACK_H
#include <stddef.h>

typedef struct pcb_stack pcb_stack;          /* 불완전 타입: 이름뿐이다 */

pcb_stack *pcb_stack_new(size_t capacity);   /* 팩토리: 객체를 얻는 유일한 길 */
void       pcb_stack_free(pcb_stack *s);     /* 그 짝: 쓰는 쪽이 불러야 한다 */

bool   pcb_stack_push(pcb_stack *s, int v);  /* 객체는 언제나 첫 인자 */
bool   pcb_stack_pop(pcb_stack *s, int *out);
size_t pcb_stack_size(const pcb_stack *s);   /* const: 읽기만 한다 */
#endif
