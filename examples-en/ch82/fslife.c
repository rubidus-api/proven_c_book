/* The life of one file — the open modes, partial writes, the position, and
   saving safely. How failure arrives as a value where the outside world is
   touched. */
#include <proven.h>

/* a helper for seeing the bytes read as text (to keep a comma out of a macro argument) */
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
    default:                      return "(other)";
    }
}

int main(void)
{
    proven_allocator_t scratch = proven_heap_allocator();
    proven_u8str_view_t path = PROVEN_LIT("build/ch77-demo.txt");

    /* ── (1) opening — the mode weaves bit flags together ────────── */
    proven_result_file_t opened = proven_fs_open(
        scratch, path,
        PROVEN_FS_WRITE | PROVEN_FS_CREATE | PROVEN_FS_TRUNC);
    if (!proven_is_ok(opened.err)) {
        proven_println("open failed: {}", PROVEN_ARG(codename(opened.err)));
        return 1;
    }
    proven_file_t f = opened.value;
    proven_println("open            -> OK (write, create, truncate)");

    /* ── (2) the two versions of writing ─────────────────────────── */
    proven_u8str_view_t line = PROVEN_LIT("first line\nsecond line\n");

    /* _write returns "how much was really written" — a partial write can happen */
    proven_result_size_t w = proven_fs_write(f, proven_mem_view_from_u8(line));
    proven_println("write           -> err={} bytes written={} (requested {})",
                   PROVEN_ARG(codename(w.err)), PROVEN_ARG(w.value),
                   PROVEN_ARG(line.size));

    /* _write_all repeats until everything is written and then reports success or failure only */
    proven_err_t e = proven_fs_write_all(f, proven_mem_view_from_u8(PROVEN_LIT("third\n")));
    proven_println("write_all       -> {} (what most code wants)",
                   PROVEN_ARG(codename(e)));

    /* ── (3) nailing it to the disk — flush and sync differ ──────── */
    e = proven_fs_sync(f);
    proven_println("sync            -> {} (only this far does it survive a power cut)",
                   PROVEN_ARG(codename(e)));

    proven_result_size_t sz = proven_fs_size(f);
    proven_result_u64_t at = proven_fs_tell(f);
    proven_println("size={} tell={}", PROVEN_ARG(sz.value), PROVEN_ARG(at.val));
    (void)proven_fs_close(f);

    /* ── (4) reading — what is asked for and what is read differ ─── */
    proven_result_file_t ro = proven_fs_open(scratch, path, PROVEN_FS_READ);
    if (proven_is_ok(ro.err)) {
        proven_file_t r = ro.value;
        proven_byte_t buf[16];
        proven_result_size_t got = proven_fs_read(r, (proven_mem_mut_t){ buf, sizeof buf });
        proven_println("read(16)        -> bytes read={} \"{}\"",
                       PROVEN_ARG(got.value),
                       PROVEN_ARG(as_text(buf, got.value)));

        /* the position is rewound and it is read again */
        (void)proven_fs_seek(r, 0, PROVEN_FS_SEEK_SET);
        proven_result_size_t again = proven_fs_read(r, (proven_mem_mut_t){ buf, 5 });
        proven_println("seek(0)+read(5) -> {} bytes", PROVEN_ARG(again.value));
        (void)proven_fs_close(r);
    }

    /* ── (5) reading it all at once — when the size is unknown ───── */
    proven_result_u8str_t all = proven_fs_read_all_u8str(scratch, path);
    if (proven_is_ok(all.err)) {
        proven_u8str_t s = all.value;
        proven_println("read_all        -> {} bytes (the whole file at one go)",
                       PROVEN_ARG(proven_u8str_as_view(&s).size));
        proven_u8str_destroy(scratch, &s);
    }

    /* ── (6) a file that is not there — failure arrives as a value ─ */
    proven_result_file_t missing =
        proven_fs_open(scratch, PROVEN_LIT("build/no-such-file.txt"), PROVEN_FS_READ);
    proven_println("opening a missing file -> {} (check which code the platform layer maps it to)",
                   PROVEN_ARG(codename(missing.err)));

    /* ── (7) an atomic save — written to a temporary and swapped in ─ */
    e = proven_fs_write_file_atomic(scratch, PROVEN_LIT("build/ch77-atomic.txt"),
                                    proven_mem_view_from_u8(PROVEN_LIT("all or nothing\n")));
    proven_println("write_file_atomic -> {} (no half-written file is left behind)",
                   PROVEN_ARG(codename(e)));

    (void)proven_fs_remove(scratch, path);
    (void)proven_fs_remove(scratch, PROVEN_LIT("build/ch77-atomic.txt"));
    return 0;
}
