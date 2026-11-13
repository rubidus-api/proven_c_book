/* The part that lives in the VBR's first sector. It loads the remaining sectors from 0x7e00 on
   and moves over to them. Big boot loaders, too, load a stage that does not fit in 512 bytes in
   pieces like this. */
#include "bios.h"

extern uint8_t _load_end[];        /* set by the linker script: the end of what to load */
[[noreturn]] void vbr_main(uint8_t drive, const struct part *entry);

[[noreturn]] void vbr_boot(uint8_t drive, const struct part *entry)
{
    uint8_t *at = load_area() + 512;
    for (uint32_t s = 1; at < _load_end; s++, at += 512)
        if (disk_read(drive, entry->lba + s, at) != 0) stop(1);
    vbr_main(drive, entry);
}
