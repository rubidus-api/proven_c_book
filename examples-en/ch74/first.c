/* The first real program — the whole course of making an object, using it and
   giving it back. The skeleton of a proven program is all in this one file. */
#include <proven.h>

/* (1) Where does the memory come from — the caller decides (the allocator parameter).
   (2) Failure arrives as a value — value is not looked at before err is checked.
   (3) What was made is given back through the allocator it was made with. */
static proven_err_t build_line(proven_allocator_t alloc,
                               proven_u8str_view_t who,
                               int count,
                               proven_u8str_t *out)
{
    /* a string with room for 64 bytes, obtained from alloc */
    proven_result_u8str_t made = proven_u8str_create(alloc, 64);
    if (!proven_is_ok(made.err))
        return made.err;

    proven_u8str_t line = made.value;      /* taken out only after the check */

    /* Appended through a format. On failure the original is left untouched.
       Formatting returns, along with err, "bytes written / bytes needed" */
    proven_fmt_result_t r = proven_u8str_append_fmt(&line, "{} has {} message(s)",
                                                    PROVEN_ARG(who), PROVEN_ARG(count));
    if (!proven_is_ok(r.err)) {
        proven_u8str_destroy(alloc, &line); /* returned on the failure path too */
        return r.err;
    }

    *out = line;                            /* ownership passes to the caller */
    return PROVEN_OK;
}

int main(void)
{
    /* the heap allocator — standard malloc wrapped in the library's interface */
    proven_allocator_t alloc = proven_heap_allocator();

    proven_u8str_t line;
    proven_err_t e = build_line(alloc, PROVEN_LIT("alice"), 3, &line);
    if (!proven_is_ok(e)) {
        proven_println("build failed: {}", PROVEN_ARG((int)e));
        return 1;
    }

    /* an owned string -> a borrowed view. A view is valid only while the original lives */
    proven_u8str_view_t v = proven_u8str_as_view(&line);
    proven_println("line   = {}", PROVEN_ARG(v));
    proven_println("length = {} bytes", PROVEN_ARG(v.size));

    /* the meeting point with old APIs that need NUL termination (no copy, no allocation) */
    proven_println("as C string = {}", PROVEN_ARG(proven_u8str_as_cstr(&line)));

    proven_u8str_destroy(alloc, &line);     /* through the very allocator it was made with */

    /* Destroying empties the struct to zero — so it cannot point at the returned
       buffer again. Hence the length after destroying is 0, and the contract is
       that this object is not used any further. */
    proven_println("after destroy, length = {}",
                   PROVEN_ARG(proven_u8str_as_view(&line).size));
    return 0;
}
