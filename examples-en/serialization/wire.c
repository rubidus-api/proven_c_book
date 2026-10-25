/* Skipping serialisation honestly: when the in-memory layout *is* the format.
   Fixed-width members, widest first, no hidden padding, offsets nailed down by
   static_assert --- so a different compiler or option stops the build instead of
   silently writing different bytes. Byte order is still the machine's: check it too. */
#include <stdbit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct wire_header {            /* shared between two processes on the same machine */
    uint64_t sequence;
    uint32_t length;
    uint16_t kind;
    uint8_t  flags;
    uint8_t  version;
};
static_assert(sizeof(struct wire_header) == 16, "layout changed");
static_assert(offsetof(struct wire_header, length)  == 8);
static_assert(offsetof(struct wire_header, kind)    == 12);
static_assert(offsetof(struct wire_header, flags)   == 14);
static_assert(offsetof(struct wire_header, version) == 15);
static_assert(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__,
              "this format is little endian; a big-endian build must convert");

struct careless {               /* the same members in a careless order */
    uint8_t  flags;
    uint64_t sequence;
    uint16_t kind;
    uint32_t length;
    uint8_t  version;
};

int main(void)
{
    printf("wire_header: %zu bytes, no padding (8 + 4 + 2 + 1 + 1)\n",
           sizeof(struct wire_header));
    printf("careless   : %zu bytes for the same 16 bytes of members\n",
           sizeof(struct careless));
    printf("  offsets: flags %zu, sequence %zu, kind %zu, length %zu, version %zu\n",
           offsetof(struct careless, flags), offsetof(struct careless, sequence),
           offsetof(struct careless, kind), offsetof(struct careless, length),
           offsetof(struct careless, version));
    return 0;
}
