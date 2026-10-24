#include <proven.h>
#include <stdio.h>

int main(void)
{
    proven_allocator_t alloc = proven_heap_allocator();
    proven_u8str_view_t path = proven_u8str_view_from_cstr("proven_demo.txt");
    const char *text = "one\ntwo\nthree\n";

    /* 쓰기: 없으면 만들고, 있으면 비운다 */
    proven_result_file_t opened = proven_fs_open(
        alloc, path, PROVEN_FS_WRITE | PROVEN_FS_CREATE | PROVEN_FS_TRUNC);
    if (!proven_is_ok(opened.err)) {
        printf("open for write failed (err=%d)\n", (int)opened.err);
        return 1;
    }
    proven_file_t f = opened.value;

    proven_mem_view_t src = {
        .ptr = (const proven_byte_t *)text,
        .size = proven_cstr_len(text)
    };
    proven_err_t e = proven_fs_write_all(f, src);   /* 부분 쓰기를 되풀이해 준다 */
    printf("write_all  : %s (%zu bytes)\n", proven_is_ok(e) ? "ok" : "failed", src.size);
    (void)proven_fs_close(f);

    /* 읽기 */
    opened = proven_fs_open(alloc, path, PROVEN_FS_READ);
    if (!proven_is_ok(opened.err)) return 1;
    f = opened.value;

    proven_result_size_t sz = proven_fs_size(f);
    printf("size       : %zu bytes\n", sz.value);

    proven_byte_t buf[64];
    proven_result_size_t got = proven_fs_read(f, (proven_mem_mut_t){ .ptr = buf, .size = sizeof buf });
    printf("read       : %zu bytes\n", got.value);

    /* 읽은 것은 뷰로 다룬다 — 줄 단위로 자른다 */
    proven_u8str_view_t all = { .ptr = buf, .size = got.value };
    proven_u8str_view_t nl = proven_u8str_view_from_cstr("\n");
    proven_size_t start = 0;
    int line = 0;
    while (start < all.size) {
        proven_size_t hit = proven_u8str_view_find(all, start, nl);
        proven_size_t end = (hit == PROVEN_INDEX_NOT_FOUND) ? all.size : hit;
        proven_u8str_view_t v = proven_u8str_view_slice(all, start, end - start);
        printf("  line %d   : %.*s\n", ++line, (int)v.size, (const char *)v.ptr);
        if (hit == PROVEN_INDEX_NOT_FOUND) break;
        start = hit + 1;
    }
    (void)proven_fs_close(f);

    /* 없는 파일을 열면 실패가 값으로 온다 */
    proven_result_file_t missing = proven_fs_open(
        alloc, proven_u8str_view_from_cstr("no_such_file.txt"), PROVEN_FS_READ);
    printf("missing    : %s\n", proven_is_ok(missing.err) ? "opened" : "refused with an error code");

    (void)proven_fs_remove(alloc, path);
    return 0;
}
