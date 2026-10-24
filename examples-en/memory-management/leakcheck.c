/* A small checking allocator --- every borrowed block is written in a ledger.
 * A returned block is not freed at once but kept in *quarantine* (painted with poison 0xDD). So when a
 * second free arrives, the address still points at a living object and can be compared with the ledger.
 * Had it really been freed, using that pointer value at all would be outside the contract (C23 6.2.4p2). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_BLOCKS = 16 };

typedef struct {
    unsigned char *p;
    size_t size;
    const char *where;
    bool released;          /* returned, but in quarantine */
} entry;
static entry book[MAX_BLOCKS];

static void *chk_malloc(size_t size, const char *where)
{
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        if (book[i].p == NULL) {
            unsigned char *p = malloc(size);
            if (p == NULL) return NULL;
            book[i] = (entry){ p, size, where, false };
            return p;
        }
    }
    return NULL;                        /* the ledger is full */
}

static void chk_free(void *ptr, const char *where)
{
    if (ptr == NULL) return;
    unsigned char *p = ptr;
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        entry *e = &book[i];
        if (e->p == NULL) continue;
        if (p == e->p) {
            if (e->released) {
                printf("  %s: double free of the block from %s\n", where, e->where);
            } else {
                memset(e->p, 0xDD, e->size);    /* paint with poison */
                e->released = true;             /* quarantine: not freed yet */
            }
            return;
        }
        /* "Is it inside this block?" is not asked with < on pointers --- comparing the size of pointers
           to different objects is outside the contract (6.5.9). Ask with integer addresses (implementation-defined). */
        uintptr_t a = (uintptr_t)p, lo = (uintptr_t)e->p;
        if (a > lo && a - lo < e->size) {
            printf("  %s: free of an address inside the block from %s (offset %ju)\n",
                   where, e->where, (uintmax_t)(a - lo));
            return;
        }
    }
    printf("  %s: free of a pointer this allocator never gave out\n", where);
}

/* At the end: report what was never returned as a leak, and really free everything, quarantined blocks too. */
static void chk_finish(void)
{
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        if (book[i].p == NULL) continue;
        if (!book[i].released)
            printf("  leak: %zu bytes from %s\n", book[i].size, book[i].where);
        free(book[i].p);
        book[i] = (entry){0};
    }
}

int main(void)
{
    char *name = chk_malloc(16, "line A");
    char *tmp  = chk_malloc(64, "line B");
    int  *nums = chk_malloc(4 * sizeof *nums, "line C");
    if (!name || !tmp || !nums) return 1;

    strcpy(name, "ok");
    chk_free(tmp, "line D");
    chk_free(tmp, "line E");            /* second free */
    chk_free(name + 1, "line F");       /* middle of a block */
    chk_free(name, "line G");
    printf("  first byte of the released block from line B: 0x%02X\n",
           (unsigned)(unsigned char)tmp[0]);    /* in quarantine, so reading is inside the contract */

    puts("at exit:");
    chk_finish();                       /* nums was never returned */
    return 0;
}
