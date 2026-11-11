/* The print backend's reply table. The line status register (0x10000005) answers "room
   to send" (0x60), adding "something received" (0x61) only the third time. The receive
   byte (0x10000000) returns the last byte written --- an imitation of one loopback
   wire. */
/* where bytes are sent and received: also printed as characters */
#define TRACE_TEXT 0x10000000u
static const struct { uintptr_t addr; unsigned n; uint32_t values[4]; } replies[] = {
    { 0x10000005u, 4, { 0x60u, 0x60u, 0x61u, 0x60u } },
};
