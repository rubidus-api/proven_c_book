#ifndef PROVEN_H
#define PROVEN_H

/**
 * @file proven.h
 * @brief Unified entry point for the proven library.
 */

#include "proven/types.h"
#include "proven/config.h"
#include "proven/version.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/align.h"
#include "proven/allocator.h"
#include "proven/heap.h"
#include "proven/arena.h"
#include "proven/pool.h"
#include "proven/buffer.h"
#include "proven/u8str.h"
#include "proven/u16str.h"
#include "proven/array.h"
#include "proven/list.h"
#include "proven/ring.h"
#include "proven/map.h"
#include "proven/algorithm.h"
#include "proven/hash.h"
#include "proven/encode.h"
#include "proven/random.h"
#include "proven/fs.h"
#include "proven/time.h"
#include "proven/fmt.h"
#include "proven/float_parse.h"
#include "proven/float_format.h"
#include "proven/mmap.h"
#ifndef PROVEN_FREESTANDING
#include "proven/stream.h"
#endif
#include "proven/sysio.h"
#include "proven/job.h"
#include "proven/scan.h"
#include "proven/coro.h"

#endif /* PROVEN_H */
