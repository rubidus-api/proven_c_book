/* The MBR's boot code. It must fit in 440 bytes (the 4-byte disk signature starts at 0x1b8, the
   table at 0x1be). Its job is one link of the boot chain: find the next thing, load it, check
   it, hand over. */
#include "bios.h"

static void say(const char *s) { while (*s) con_putc(*s++); }

// [excerpt mbr]
void mbr_main(uint8_t drive, const struct part *table)
{
    /* find: the first entry marked 0x80 */
    const struct part *active = nullptr;
    for (int i = 0; i < 4; i++)
        if (table[i].boot == 0x80) { active = &table[i]; break; }
    if (!active) { say("MBR: no active partition\r\n"); stop(1); }

    /* load: that partition's first sector at 0x7c00 */
    uint8_t *vbr = load_area();
    if (disk_read(drive, active->lba, vbr) != 0)
        { say("MBR: read failed\r\n"); stop(1); }

    /* check: the BIOS's check once more */
    if (vbr[510] != 0x55 || vbr[511] != 0xaa) { say("MBR: no 55 aa\r\n"); stop(1); }

    /* hand over: DL = drive, SI = that entry */
    chain(drive, active, vbr);
}
// [/excerpt]
