#include <proven.h>
#include <stdio.h>

static void show(const char *label, proven_u8str_view_t v)
{
    printf("%-12s [%.*s] (%zu bytes)\n", label, (int)v.size, (const char *)v.ptr, v.size);
}

int main(void)
{
    /* (1) a string without allocation: a stack buffer is borrowed.
          A signature with no allocator = it does not allocate */
    proven_byte_t buf[16];
    proven_u8str_t s = proven_u8str_borrow(buf, sizeof buf);

    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("hello"));
    printf("append 'hello' : %s\n", proven_is_ok(e) ? "ok" : "refused");
    show("content", proven_u8str_as_view(&s));

    /* (2) if there is not enough room it refuses — it does not truncate */
    e = proven_u8str_append(&s, proven_u8str_view_from_cstr(" world, and more"));
    printf("append long    : %s (err=%d)\n",
           proven_is_ok(e) ? "ok" : "refused", (int)e);
    show("unchanged", proven_u8str_as_view(&s));   /* failure atomicity */

    /* (3) finding and slicing — all on views, with no copying */
    proven_u8str_view_t csv = proven_u8str_view_from_cstr("name,age,city");
    proven_u8str_view_t comma = proven_u8str_view_from_cstr(",");

    proven_size_t at = proven_u8str_view_find(csv, 0, comma);
    printf("first comma at : %zu\n", at);
    show("field 1", proven_u8str_view_slice(csv, 0, at));

    /* (4) sweeping by a separator: when none is found the sentinel (PROVEN_INDEX_NOT_FOUND) comes back */
    proven_size_t start = 0;
    int n = 0;
    for (;;) {
        proven_size_t hit = proven_u8str_view_find(csv, start, comma);
        proven_size_t end = (hit == PROVEN_INDEX_NOT_FOUND) ? csv.size : hit;
        char label[24];
        snprintf(label, sizeof label, "field %d", ++n % 100);
        show(label, proven_u8str_view_slice(csv, start, end - start));
        if (hit == PROVEN_INDEX_NOT_FOUND) break;
        start = hit + 1;
    }

    /* (5) comparison and a prefix test */
    printf("eq 'name,age,city' : %d\n",
           proven_u8str_view_eq(csv, proven_u8str_view_from_cstr("name,age,city")));
    printf("starts with 'name' : %d\n",
           proven_u8str_view_starts_with(csv, proven_u8str_view_from_cstr("name")));
    return 0;
}
