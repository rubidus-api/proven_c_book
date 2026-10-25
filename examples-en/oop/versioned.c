/* A struct that carries its own size, in the style of the Windows API (cbSize).
   Version 2 of the library added a member at the end. A program compiled against
   version 1 still works: the library reads only as many bytes as the caller says. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* what the version-1 header said */
struct win_opts_v1 { uint32_t size; int32_t width, height; };
/* what the version-2 header says: one member more, at the end, nothing else moved */
struct win_opts    { uint32_t size; int32_t width, height; int32_t dpi; };

static_assert(offsetof(struct win_opts, height) == offsetof(struct win_opts_v1, height),
              "old members must stay where they were");

/* the library (version 2) */
static bool win_open(const void *opts)
{
    uint32_t size;
    memcpy(&size, opts, sizeof size);        /* the first member is always the size */
    struct win_opts o = { .dpi = 96 };       /* defaults for whatever the caller lacks */
    if (size == sizeof(struct win_opts_v1) || size == sizeof(struct win_opts))
        memcpy(&o, opts, size);              /* read no more than the caller has */
    else {
        printf("  size %u: not a version this library knows --- refused\n",
               (unsigned)size);
        return false;
    }
    printf("  size %2u: %d x %d at %d dpi\n", (unsigned)size, o.width, o.height, o.dpi);
    return true;
}

int main(void)
{
    puts("an old program (built with the version-1 header):");
    struct win_opts_v1 old = { .size = sizeof old, .width = 640, .height = 480 };
    win_open(&old);

    puts("a new program:");
    struct win_opts now = { .size = sizeof now, .width = 1920, .height = 1080,
                            .dpi = 144 };
    win_open(&now);

    puts("a program that forgot to fill in the size:");
    struct win_opts careless = { .width = 800, .height = 600 };   /* size stays 0 */
    win_open(&careless);
    return 0;
}
