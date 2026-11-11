/* The print backend's reply table. The status register (0x1003_0010) always answers
   "transfer done, ACK received" (0), and the receive byte (0x1003_000C) gives 0x19 and
   0x80 --- 25.5 degrees. */
static const struct { uintptr_t addr; unsigned n; uint32_t values[2]; } replies[] = {
    { 0x10030010u, 1, { 0x00u } },
    { 0x1003000Cu, 2, { 0x19u, 0x80u } },
};
