#include <proven.h>
#include <stdio.h>

static void dump(const char *label, proven_mem_view_t v)
{
    printf("%-10s size=%zu bytes:", label, v.size);
    for (proven_size_t i = 0; i < v.size; i++) printf(" %02x", v.ptr[i]);
    printf("\n");
}

int main(void)
{
    /* A raw byte is a proven_byte_t — an alias of unsigned char, and so the
       one type that may look into the representation of any object */
    proven_byte_t buf[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};

    /* a view = pointer + length. The length is not carried around separately */
    proven_mem_view_t all = { .ptr = buf, .size = sizeof buf };
    dump("all", all);

    /* slicing: cross the boundary and the failure arrives as a value */
    proven_result_mem_view_t mid = proven_mem_view_slice_checked(all, 2, 3);
    if (proven_is_ok(mid.err)) dump("slice 2+3", mid.value);

    proven_result_mem_view_t over = proven_mem_view_slice_checked(all, 6, 4);
    printf("%-10s err=%d (refused, not clamped)\n", "slice 6+4", (int)over.err);

    /* the size computation is kept from wrapping too: element count x element size */
    proven_size_t n = (proven_size_t)-1 / 2;   /* an absurdly large count */
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, n, (proven_size_t)8))
        printf("%-10s %zu * 8 overflows — allocation refused\n", "size calc", n);
    else
        printf("%-10s %zu bytes\n", "size calc", bytes);

    /* rounding up an alignment: pushed up to the next boundary */
    printf("align_up(13, 8) = %zu\n", proven_mem_align_up(13, 8));
    return 0;
}
