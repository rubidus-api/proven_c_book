#ifndef PROVEN_H
#define PROVEN_H

/**
 * @file proven.h
 * @brief Unified entry point for the proven library: the base library, proven_base - and the
 *        network library, proven_net, when its headers (net/include in the source tree) are on
 *        the include path (RFC-0043).
 */

#include "proven/types.h"
#include "proven/config.h"
#include "proven/version.h"
#include "proven/error.h"
#include "proven/memory.h"
#include "proven/align.h"
#include "proven/allocator.h"
#include "proven/heap.h"
#include "proven/alloc_check.h"
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
#include "proven/hash_legacy.h"
#include "proven/hmac.h"
#include "proven/deflate.h"
#include "proven/zstd.h"
#ifndef PROVEN_NO_BROTLI
#include "proven/brotli.h"
#endif
#include "proven/cert.h"
#include "proven/der.h"
#include "proven/sign.h"
#include "proven/aes.h"
#include "proven/aead.h"
#include "proven/kex.h"
#include "proven/url.h"
#include "proven/encode.h"
#include "proven/utf.h"
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

#if defined(__has_include)
#if __has_include("proven_net.h")
#include "proven_net.h"
#endif
#endif

#endif /* PROVEN_H */
