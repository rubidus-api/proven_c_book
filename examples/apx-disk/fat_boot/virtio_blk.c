/* virtio 블록 드라이버. 리눅스의 virtio_blk 와 펌웨어의 작은 드라이버가 하는 일 가운데
   「섹터 하나 읽기」만 남겼다. 레지스터에 닿는 것은 hw_read32 · hw_write32 뿐이고,
   요청과 대답은 *기억 장치에 둔 고리(virtqueue)* 로 주고받는다 --- 장치가 DMA 로
   직접 읽고 쓴다. */
#include <stdatomic.h>
#include "virtio_blk.h"
#include "hw.h"

// [excerpt regs]
enum {                           /* virtio-mmio 레지스터(바이트 자리) */
    MAGIC = 0x000, VERSION = 0x004, DEVICE_ID = 0x008,
    DEV_FEAT = 0x010, DEV_FEAT_SEL = 0x014, DRV_FEAT = 0x020, DRV_FEAT_SEL = 0x024,
    Q_SEL = 0x030, Q_MAX = 0x034, Q_NUM = 0x038, Q_READY = 0x044, Q_NOTIFY = 0x050,
    INT_STATUS = 0x060, INT_ACK = 0x064, STATUS = 0x070,
    Q_DESC = 0x080, Q_AVAIL = 0x090, Q_USED = 0x0A0,     /* 각각 아래 · 위 32비트 */
    CONFIG = 0x100,                                      /* 블록 장치: 용량(섹터) */
};
enum { ACK = 1, DRIVER = 2, DRIVER_OK = 4, FEATURES_OK = 8, FAILED = 128 };
// [/excerpt]

// [excerpt ring]
/* 고리 셋: 서술자 표(무엇을 어디에), 내놓은 고리(드라이버 → 장치),
   쓴 고리(장치 → 드라이버). */
#define QN 4
struct desc  { uint64_t addr; uint32_t len; uint16_t flags, next; };
struct avail { uint16_t flags, idx, ring[QN], unused; };
struct used  { uint16_t flags, idx; struct { uint32_t id, len; } ring[QN];
               uint16_t unused; };
enum { F_NEXT = 1, F_WRITE = 2 };            /* F_WRITE: 장치가 *쓰는* 칸 */

static _Alignas(16) struct desc  q_desc[QN];
static _Alignas(2)  struct avail q_avail;
static _Alignas(4)  struct used  q_used;

struct req { uint32_t type, reserved; uint64_t sector; };   /* type 0 = 읽기 */
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
        return -1;                            /* "virt", 판 2, 블록 장치가 아니다 */
    wr(d, STATUS, 0);                         /* 초기화 */
    wr(d, STATUS, ACK);                       /* 「장치를 알아봤다」 */
    wr(d, STATUS, ACK | DRIVER);              /* 「다룰 줄 안다」 */

    wr(d, DEV_FEAT_SEL, 1);                   /* 기능 비트 32~63 */
    if (!(rd(d, DEV_FEAT) & 1)) return -1;    /* VERSION_1(비트 32) 없음: 옛 장치 */
    wr(d, DRV_FEAT_SEL, 0); wr(d, DRV_FEAT, 0);   /* 다른 기능은 받지 않고 */
    wr(d, DRV_FEAT_SEL, 1); wr(d, DRV_FEAT, 1);   /* VERSION_1 만 받는다 */
    wr(d, STATUS, ACK | DRIVER | FEATURES_OK);
    if (!(rd(d, STATUS) & FEATURES_OK)) { wr(d, STATUS, FAILED); return -1; }

    /* 고리는 한 벌을 장치마다 새로 쓴다(한 번에 장치 하나만 다룬다). 장치는 0 에서
       세기 시작하므로 앞 장치가 올려 둔 idx 를 남기면 「고리가 넘쳤다」고 거절한다. */
    q_avail = (struct avail){ 0 };
    q_used  = (struct used){ 0 };
    wr(d, Q_SEL, 0);                          /* 0번 고리 하나만 쓴다 */
    if (rd(d, Q_MAX) < QN) { wr(d, STATUS, FAILED); return -1; }
    wr(d, Q_NUM, QN);
    wr64(d, Q_DESC,  (uintptr_t)q_desc);      /* 장치가 읽을 *실제 주소*를 알린다 */
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
    /* 머리(무엇을 읽나) → 자료(512바이트) → 결과(1바이트), 셋이 사슬로 이어진다 */
    q_desc[0] = (struct desc){ (uintptr_t)&req, sizeof req, F_NEXT, 1 };
    q_desc[1] = (struct desc){ (uintptr_t)buf512, 512, F_WRITE | F_NEXT, 2 };
    q_desc[2] = (struct desc){ (uintptr_t)&req_status, 1, F_WRITE, 0 };

    uint16_t seen = q_used.idx;
    q_avail.ring[q_avail.idx % QN] = 0;       /* 0번 서술자부터 이어진 요청 하나 */
    atomic_thread_fence(memory_order_seq_cst);   /* 고리를 다 쓴 *뒤에* idx 를 올린다 */
    q_avail.idx++;
    atomic_thread_fence(memory_order_seq_cst);
    wr(d, Q_NOTIFY, 0);                       /* 「0번 고리를 보라」 */

    /* 장치는 레지스터가 아니라 기억 장치(쓴 고리)에 대답한다. */
    for (long i = 0; i < 10000000; i++) {
        if (*(volatile uint16_t *)&q_used.idx != seen) {
            atomic_thread_fence(memory_order_seq_cst);
            wr(d, INT_ACK, rd(d, INT_STATUS));
            return req_status == 0 ? 0 : -1;
        }
    }
    return -2;                                /* 대답이 없다 */
}
// [/excerpt]
