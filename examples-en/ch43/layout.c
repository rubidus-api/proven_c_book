/* The layout of a struct - padding, reordering, forced alignment */
#include <stddef.h>
#include <stdio.h>

struct loose  { char  a; int b; char c; };   /* something big between small ones */
struct tight  { int   b; char a; char c; };  /* the big one laid down first      */

#pragma pack(push, 1)                         /* no padding from here on */
struct packed { char  a; int b; char c; };
#pragma pack(pop)                             /* back to the ordinary rules */

struct cacheline { alignas(64) int counter; };  /* forced to a wide alignment */

struct nested { struct loose inner; int tag; };

static void report(const char *name, size_t size, size_t align,
                   size_t oa, size_t ob, size_t oc)
{
    printf("%-8s size %2zu  align %2zu   offsets a=%zu b=%zu c=%zu\n",
           name, size, align, oa, ob, oc);
}

int main(void)
{
    report("loose",  sizeof(struct loose),  alignof(struct loose),
           offsetof(struct loose, a), offsetof(struct loose, b), offsetof(struct loose, c));
    report("tight",  sizeof(struct tight),  alignof(struct tight),
           offsetof(struct tight, a), offsetof(struct tight, b), offsetof(struct tight, c));
    report("packed", sizeof(struct packed), alignof(struct packed),
           offsetof(struct packed, a), offsetof(struct packed, b), offsetof(struct packed, c));

    printf("\ncacheline size %zu  align %zu\n",
           sizeof(struct cacheline), alignof(struct cacheline));
    printf("nested    size %zu  inner offset %zu  tag offset %zu\n",
           sizeof(struct nested), offsetof(struct nested, inner),
           offsetof(struct nested, tag));

    /* the difference between the sum of the three members and the struct = the padding bytes */
    size_t members = sizeof(char) + sizeof(int) + sizeof(char);
    printf("\nloose: members sum %zu, actual %zu -> padding %zu bytes\n",
           members, sizeof(struct loose), sizeof(struct loose) - members);
    return 0;
}
