/* The print backend's reply table. The transmit FIFO (0x1004_0048) always says "room"
   (0), and the receive FIFO (0x1004_004C) gives values in turn: the 0 pushed out by the
   command, ISSI's three identification bytes, four 0s pushed out by the command and
   address, then the flash's first eight bytes. */
static const struct { uintptr_t addr; unsigned n; uint32_t values[16]; } replies[] = {
    { 0x10040048u, 1, { 0 } },
    { 0x1004004Cu, 16, { 0, 0x9d, 0x70, 0x19, 0, 0, 0, 0,
                         'B', 'O', 'O', 'T', ' ', 'O', 'K', '\n' } },
};
