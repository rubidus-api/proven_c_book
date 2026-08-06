/* An error is a value — what those values really are, seen with the eyes. */
#include <proven.h>

static const char *name_of(proven_err_t e)
{
    switch (e) {
    case PROVEN_OK:                   return "PROVEN_OK";
    case PROVEN_ERR_NOMEM:            return "NOMEM (out of memory)";
    case PROVEN_ERR_OUT_OF_BOUNDS:    return "OUT_OF_BOUNDS (outside the vessel)";
    case PROVEN_ERR_INVALID_ENCODING: return "INVALID_ENCODING (broken encoding)";
    case PROVEN_ERR_INVALID_ARG:      return "INVALID_ARG (argument outside the contract)";
    case PROVEN_ERR_IO:               return "IO (a failure of the outside world)";
    case PROVEN_ERR_NOT_FOUND:        return "NOT_FOUND (there is none)";
    case PROVEN_ERR_INVALID_STATE:    return "INVALID_STATE (cannot be done now)";
    case PROVEN_ERR_NEED_MORE:        return "NEED_MORE (more input is needed)";
    case PROVEN_ERR_OVERFLOW:         return "OVERFLOW (the computation overflowed)";
    case PROVEN_ERR_UNSUPPORTED:      return "UNSUPPORTED (not in this environment)";
    case PROVEN_ERR_AGAIN:            return "AGAIN (not now, try again)";
    case PROVEN_ERR_EOF:              return "EOF (the end was reached)";
    case PROVEN_ERR_BUSY:             return "BUSY (someone else is using it)";
    case PROVEN_ERR_PERMISSION:       return "PERMISSION (no permission)";
    case PROVEN_ERR_INVALID_FORMAT:   return "INVALID_FORMAT (the format is wrong)";
    }
    return "(unknown)";
}

int main(void)
{
    proven_println("-- every error code --");
    for (int i = PROVEN_OK; i <= PROVEN_ERR_INVALID_FORMAT; i++)
        proven_println("{:>2}  {}", PROVEN_ARG(i), PROVEN_ARG(name_of((proven_err_t)i)));

    proven_println("");
    proven_println("-- which code actually comes back --");

    /* (1) the vessel is too small -> OUT_OF_BOUNDS */
    proven_byte_t small[8];
    proven_u8str_t s = proven_u8str_borrow(small, sizeof small);
    proven_err_t e1 = proven_u8str_append(&s, PROVEN_LIT("Hello, world"));
    proven_println("append 12 bytes into 8   -> {}", PROVEN_ARG(name_of(e1)));

    /* failure atomicity: after the refusal the original is still untouched */
    proven_println("  after failure, len = {}",
                   PROVEN_ARG(proven_u8str_as_view(&s).size));

    /* (2) an argument breaking the contract -> INVALID_ARG.
          Give it an unusable allocator (a value that is all zeros) and it is
          caught before anything is made. */
    proven_allocator_t nothing = (proven_allocator_t){0};
    proven_result_u8str_t bad = proven_u8str_create(nothing, 16);
    proven_println("create with a null allocator -> {}", PROVEN_ARG(name_of(bad.err)));

    /* (3) slicing past the end -> OUT_OF_BOUNDS (it does not hand over what there is) */
    proven_u8str_view_t v = PROVEN_LIT("abcdefgh");
    proven_result_mem_view_t sl =
        proven_mem_view_slice_checked(proven_mem_view_from_u8(v), 6, 4);
    proven_println("slice 4 bytes from 6/8   -> {}", PROVEN_ARG(name_of(sl.err)));

    /* (4) success is always 0, and the check is always the same one line */
    proven_err_t ok = proven_u8str_append(&s, PROVEN_LIT("hi"));
    proven_println("append 2 bytes into 8    -> {} (is_ok={})",
                   PROVEN_ARG(name_of(ok)), PROVEN_ARG(proven_is_ok(ok)));
    return 0;
}
