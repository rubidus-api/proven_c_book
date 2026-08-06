#include <proven.h>
#include <stdio.h>

static void show(const char *label, proven_u8str_view_t v)
{
    printf("%-12s [%.*s] (%zu bytes)\n", label, (int)v.size, (const char *)v.ptr, v.size);
}

int main(void)
{
    /* ① 할당 없는 문자열: 스택 버퍼를 빌려 쓴다.
          allocator 가 없는 시그니처 = 할당하지 않는다 */
    proven_byte_t buf[16];
    proven_u8str_t s = proven_u8str_borrow(buf, sizeof buf);

    proven_err_t e = proven_u8str_append(&s, proven_u8str_view_from_cstr("hello"));
    printf("append 'hello' : %s\n", proven_is_ok(e) ? "ok" : "refused");
    show("content", proven_u8str_as_view(&s));

    /* ② 자리가 모자라면 거부한다 — 자르지 않는다 */
    e = proven_u8str_append(&s, proven_u8str_view_from_cstr(" world, and more"));
    printf("append long    : %s (err=%d)\n",
           proven_is_ok(e) ? "ok" : "refused", (int)e);
    show("unchanged", proven_u8str_as_view(&s));   /* 실패 원자성 */

    /* ③ 찾기와 자르기 — 전부 view 위에서, 복사 없이 */
    proven_u8str_view_t csv = proven_u8str_view_from_cstr("name,age,city");
    proven_u8str_view_t comma = proven_u8str_view_from_cstr(",");

    proven_size_t at = proven_u8str_view_find(csv, 0, comma);
    printf("first comma at : %zu\n", at);
    show("field 1", proven_u8str_view_slice(csv, 0, at));

    /* ④ 구분자로 훑기: 못 찾으면 센티널(PROVEN_INDEX_NOT_FOUND)이 온다 */
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

    /* ⑤ 비교와 접두사 검사 */
    printf("eq 'name,age,city' : %d\n",
           proven_u8str_view_eq(csv, proven_u8str_view_from_cstr("name,age,city")));
    printf("starts with 'name' : %d\n",
           proven_u8str_view_starts_with(csv, proven_u8str_view_from_cstr("name")));
    return 0;
}
