/* 처리기 안에서 왜 '자물쇠 없는' 원자적 연산만 허용되는가. */
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

static _Atomic uint32_t small;
static _Atomic unsigned __int128 big;

int main(void)
{
    printf("ATOMIC_INT_LOCK_FREE   = %d   (2 = always lock-free)\n", ATOMIC_INT_LOCK_FREE);
    printf("ATOMIC_LLONG_LOCK_FREE = %d\n", ATOMIC_LLONG_LOCK_FREE);
    printf("32-bit atomic is lock-free : %s\n",
           atomic_is_lock_free(&small) ? "yes" : "no");
    printf("128-bit atomic is lock-free: %s\n",
           atomic_is_lock_free(&big) ? "yes" : "no");
    puts("");
    puts("a non-lock-free atomic takes a lock behind your back.");
    puts("if the code an interrupt suspended was holding that same lock,");
    puts("the handler waits for a lock only the interrupted code can release.");
    puts("that is why C23 7.14.1.1 allows <stdatomic.h> in a handler only when");
    puts("the atomic arguments are lock-free.");
    return 0;
}
