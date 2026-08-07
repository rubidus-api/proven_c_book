/* The flexible array member — C99's way to take header and data in one block. */
#include <stdckdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Leave the last member's size empty and it is a flexible array member.
   It is not counted in sizeof — the size is decided when allocating. */
struct msg {
    unsigned kind;
    size_t   len;
    char     data[];      /* <- the flexible array member */
};

/* size = header + data. Skip the overflow check and a big array lands in a small vessel. */
static struct msg *msg_new(unsigned kind, const char *text)
{
    size_t len = strlen(text);
    size_t need;
    /* offsetof(struct msg, data) is more exact than sizeof(struct msg) —
       it does not count the tail padding twice. */
    if (ckd_add(&need, offsetof(struct msg, data), len)) return nullptr;

    struct msg *m = malloc(need);
    if (!m) return nullptr;
    m->kind = kind;
    m->len  = len;
    memcpy(m->data, text, len);
    return m;
}

int main(void)
{
    printf("sizeof(struct msg)          = %zu  <- data is not counted\n",
           sizeof(struct msg));
    printf("offsetof(struct msg, data)  = %zu\n", offsetof(struct msg, data));
    printf("alignof(struct msg)         = %zu\n", alignof(struct msg));

    const char *text = "hello, world!";
    struct msg *m = msg_new(7, text);
    if (!m) { perror("malloc"); return 1; }

    printf("\nallocated = offsetof(data) + %zu = %zu bytes\n",
           m->len, offsetof(struct msg, data) + m->len);
    printf("kind = %u, len = %zu, data = \"%.*s\"\n",
           m->kind, m->len, (int)m->len, m->data);

    /* header and data are one block, so one free */
    free(m);

    puts("\nHow the old practice differed:");
    puts("  char data[1];  <- the 'struct hack'. The size arithmetic was off by one,");
    puts("                   and it accessed past the array — outside the contract.");
    puts("  char data[];   <- what C99 made official. Inside the contract.");
    return 0;
}
