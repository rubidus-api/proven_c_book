/* virtio block driver. Of what Linux's virtio_blk and firmware's small drivers do, only
   "read one sector" is kept. Registers are touched only through hw_read32 and
   hw_write32; requests and answers travel through *rings in memory (the virtqueue)* ---
   the device reads and writes them itself by DMA. */
#include <stdatomic.h>
#include "virtio_blk.h"
#include "hw.h"

// [excerpt regs]
enum {                           /* virtio-mmio registers (byte offsets) */
    MAGIC = 0x000, VERSION = 0x004, DEVICE_ID = 0x008,
    DEV_FEAT = 0x010, DEV_FEAT_SEL = 0x014, DRV_FEAT = 0x020, DRV_FEAT_SEL = 0x024,
    Q_SEL = 0x030, Q_MAX = 0x034, Q_NUM = 0x038, Q_READY = 0x044, Q_NOTIFY = 0x050,
    INT_STATUS = 0x060, INT_ACK = 0x064, STATUS = 0x070,
    Q_DESC = 0x080, Q_AVAIL = 0x090, Q_USED = 0x0A0,     /* each low and high 32 bits */
    CONFIG = 0x100,  /* block device: capacity (sectors) */
};
enum { ACK = 1, DRIVER = 2, DRIVER_OK = 4, FEATURES_OK = 8, FAILED = 128 };
// [/excerpt]

// [excerpt ring]
/* Three rings: the descriptor table (what, where), the available ring (driver ->
   device), the used ring (device -> driver). */
#define QN 4
struct desc  { uint64_t addr; uint32_t len; uint16_t flags, next; };
struct avail { uint16_t flags, idx, ring[QN], unused; };
struct used  { uint16_t flags, idx; struct { uint32_t id, len; } ring[QN];
               uint16_t unused; };
enum { F_NEXT = 1, F_WRITE = 2 };            /* F_WRITE: a slot the device *writes* */

static _Alignas(16) struct desc  q_desc[QN];
static _Alignas(2)  struct avail q_avail;
static _Alignas(4)  struct used  q_used;

struct req { uint32_t type, reserved; uint64_t sector; };   /* type 0 = read */
static struct req req;
static volatile uint8_t req_status;
// [/excerpt]

static uint32_t rd(const struct vblk *d, unsigned r)             { return hw_read32(d->base + r); }
static void     wr(const struct vblk *d, unsigned r, uint32_t v) { hw_write32(d->base + r, v); }
static void wr64(const struct vblk *d, unsigned r, uint64_t v)
{ wr(d, r, (uint32_t)v); wr(d, r + 4, (uint32_t)(v >> 32)); }

// [excerpt init]
int vblk_init(struct vblk *d)
{
    if (rd(d, MAGIC) != 0x74726976u || rd(d, VERSION) != 2 || rd(d, DEVICE_ID) != 2)
        return -1;  /* "virt", version 2, a block device: otherwise not ours */
    wr(d, STATUS, 0);                         /* reset */
    wr(d, STATUS, ACK);                       /* "I have noticed the device" */
    wr(d, STATUS, ACK | DRIVER);              /* "I know how to drive it" */

    wr(d, DEV_FEAT_SEL, 1);                   /* feature bits 32-63 */
    if (!(rd(d, DEV_FEAT) & 1)) return -1;    /* no VERSION_1 (bit 32): legacy */
    wr(d, DRV_FEAT_SEL, 0); wr(d, DRV_FEAT, 0);   /* accept no other feature */
    wr(d, DRV_FEAT_SEL, 1); wr(d, DRV_FEAT, 1);   /* accept VERSION_1 only */
    wr(d, STATUS, ACK | DRIVER | FEATURES_OK);
    if (!(rd(d, STATUS) & FEATURES_OK)) { wr(d, STATUS, FAILED); return -1; }

    /* One set of rings is reused for each device (one device at a time). The device
       starts counting at 0, so an idx left over from the previous device makes it
       refuse with "ring overflow". */
    q_avail = (struct avail){ 0 };
    q_used  = (struct used){ 0 };
    wr(d, Q_SEL, 0);                          /* use queue 0 only */
    if (rd(d, Q_MAX) < QN) { wr(d, STATUS, FAILED); return -1; }
    wr(d, Q_NUM, QN);
    /* tell the device the *real addresses* it will read */
    wr64(d, Q_DESC,  (uintptr_t)q_desc);
    wr64(d, Q_AVAIL, (uintptr_t)&q_avail);
    wr64(d, Q_USED,  (uintptr_t)&q_used);
    wr(d, Q_READY, 1);
    wr(d, STATUS, ACK | DRIVER | FEATURES_OK | DRIVER_OK);

    d->sectors = rd(d, CONFIG) | (uint64_t)rd(d, CONFIG + 4) << 32;
    return 0;
}
// [/excerpt]

// [excerpt read]
int vblk_read(struct vblk *d, uint64_t lba, void *buf512)
{
    req = (struct req){ .type = 0, .sector = lba };
    req_status = 0xFF;
    /* header (what to read) -> data (512 bytes) -> status (1 byte), chained */
    q_desc[0] = (struct desc){ (uintptr_t)&req, sizeof req, F_NEXT, 1 };
    q_desc[1] = (struct desc){ (uintptr_t)buf512, 512, F_WRITE | F_NEXT, 2 };
    q_desc[2] = (struct desc){ (uintptr_t)&req_status, 1, F_WRITE, 0 };

    uint16_t seen = q_used.idx;
    q_avail.ring[q_avail.idx % QN] = 0;  /* one request, chained from descriptor 0 */
    /* raise idx only *after* the ring is written */
    atomic_thread_fence(memory_order_seq_cst);
    q_avail.idx++;
    atomic_thread_fence(memory_order_seq_cst);
    wr(d, Q_NOTIFY, 0);                       /* "look at queue 0" */

    /* The device answers in memory (the used ring), not in a register. */
    for (long i = 0; i < 10000000; i++) {
        if (*(volatile uint16_t *)&q_used.idx != seen) {
            atomic_thread_fence(memory_order_seq_cst);
            wr(d, INT_ACK, rd(d, INT_STATUS));
            return req_status == 0 ? 0 : -1;
        }
    }
    return -2;                                /* no answer */
}
// [/excerpt]
