/* 파일 하나의 한살이 — 열기 모드, 부분 쓰기, 위치, 그리고 안전한 저장.
   바깥 세계와 닿는 자리에서 실패가 어떻게 값으로 오는지 본다. */
#include <proven.h>

/* 읽은 바이트를 글자로 보기 위한 도우미 (매크로 인자에 쉼표를 넣지 않으려고) */
static proven_u8str_view_t as_text(const proven_byte_t *p, proven_size_t n)
{
    return (proven_u8str_view_t){ .ptr = p, .size = n };
}

static const char *codename(proven_err_t e)
{
    switch (e) {
    case PROVEN_OK:               return "OK";
    case PROVEN_ERR_NOT_FOUND:    return "NOT_FOUND";
    case PROVEN_ERR_PERMISSION:   return "PERMISSION";
    case PROVEN_ERR_IO:           return "IO";
    case PROVEN_ERR_EOF:          return "EOF";
    case PROVEN_ERR_INVALID_ARG:  return "INVALID_ARG";
    default:                      return "(그 밖)";
    }
}

int main(void)
{
    proven_allocator_t scratch = proven_heap_allocator();
    proven_u8str_view_t path = PROVEN_LIT("build/ch77-demo.txt");

    /* ── ① 열기 — 모드는 비트 깃발을 엮는다 ──────────────────── */
    proven_result_file_t opened = proven_fs_open(
        scratch, path,
        PROVEN_FS_WRITE | PROVEN_FS_CREATE | PROVEN_FS_TRUNC);
    if (!proven_is_ok(opened.err)) {
        proven_println("열기 실패: {}", PROVEN_ARG(codename(opened.err)));
        return 1;
    }
    proven_file_t f = opened.value;
    proven_println("열기            -> OK (쓰기·생성·비우기)");

    /* ── ② 쓰기의 두 판 ──────────────────────────────────────── */
    proven_u8str_view_t line = PROVEN_LIT("first line\nsecond line\n");

    /* _write 는 "실제로 쓴 양"을 돌려준다 — 부분 쓰기가 있을 수 있다 */
    proven_result_size_t w = proven_fs_write(f, proven_mem_view_from_u8(line));
    proven_println("write           -> err={} 쓴 바이트={} (요청 {})",
                   PROVEN_ARG(codename(w.err)), PROVEN_ARG(w.value),
                   PROVEN_ARG(line.size));

    /* _write_all 은 다 쓸 때까지 되풀이한 뒤 성공/실패만 알려 준다 */
    proven_err_t e = proven_fs_write_all(f, proven_mem_view_from_u8(PROVEN_LIT("third\n")));
    proven_println("write_all       -> {} (대부분의 코드가 원하는 쪽)",
                   PROVEN_ARG(codename(e)));

    /* ── ③ 디스크에 못박기 — flush 와 sync 는 다르다 ──────────── */
    e = proven_fs_sync(f);
    proven_println("sync            -> {} (여기까지 해야 정전에도 남는다)",
                   PROVEN_ARG(codename(e)));

    proven_result_size_t sz = proven_fs_size(f);
    proven_result_u64_t at = proven_fs_tell(f);
    proven_println("size={} tell={}", PROVEN_ARG(sz.value), PROVEN_ARG(at.val));
    (void)proven_fs_close(f);

    /* ── ④ 읽기 — 요청량과 읽은 양은 다르다 ──────────────────── */
    proven_result_file_t ro = proven_fs_open(scratch, path, PROVEN_FS_READ);
    if (proven_is_ok(ro.err)) {
        proven_file_t r = ro.value;
        proven_byte_t buf[16];
        proven_result_size_t got = proven_fs_read(r, (proven_mem_mut_t){ buf, sizeof buf });
        proven_println("read(16)        -> 읽은 바이트={} \"{}\"",
                       PROVEN_ARG(got.value),
                       PROVEN_ARG(as_text(buf, got.value)));

        /* 위치를 되돌리고 다시 읽는다 */
        (void)proven_fs_seek(r, 0, PROVEN_FS_SEEK_SET);
        proven_result_size_t again = proven_fs_read(r, (proven_mem_mut_t){ buf, 5 });
        proven_println("seek(0)+read(5) -> {} 바이트", PROVEN_ARG(again.value));
        (void)proven_fs_close(r);
    }

    /* ── ⑤ 한 번에 읽기 — 파일 크기를 모를 때 ────────────────── */
    proven_result_u8str_t all = proven_fs_read_all_u8str(scratch, path);
    if (proven_is_ok(all.err)) {
        proven_u8str_t s = all.value;
        proven_println("read_all        -> {} 바이트 (파일 전체를 한 번에)",
                       PROVEN_ARG(proven_u8str_as_view(&s).size));
        proven_u8str_destroy(scratch, &s);
    }

    /* ── ⑥ 없는 파일 — 실패는 값으로 온다 ────────────────────── */
    proven_result_file_t missing =
        proven_fs_open(scratch, PROVEN_LIT("build/없는파일.txt"), PROVEN_FS_READ);
    proven_println("없는 파일 열기  -> {} (플랫폼 층이 어떤 코드로 옮기는지 확인할 것)",
                   PROVEN_ARG(codename(missing.err)));

    /* ── ⑦ 원자적 저장 — 임시 파일에 쓰고 바꿔치기 ───────────── */
    e = proven_fs_write_file_atomic(scratch, PROVEN_LIT("build/ch77-atomic.txt"),
                                    proven_mem_view_from_u8(PROVEN_LIT("all or nothing\n")));
    proven_println("write_file_atomic -> {} (반쯤 쓰인 파일이 남지 않는다)",
                   PROVEN_ARG(codename(e)));

    (void)proven_fs_remove(scratch, path);
    (void)proven_fs_remove(scratch, PROVEN_LIT("build/ch77-atomic.txt"));
    return 0;
}
