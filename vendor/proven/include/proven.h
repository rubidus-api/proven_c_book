#ifndef PROVEN_H
#define PROVEN_H

/**
 * @file proven.h
 * @brief Unified entry point for the proven library: the base library, proven_base - and the
 *        cryptography library proven_crypto, the compression library proven_codec (RFC-0045)
 *        and the network library proven_net (RFC-0043), each when its headers (crypto/include,
 *        codec/include, net/include in the source tree) are on the include path.
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
#if __has_include("proven_crypto.h")
#include "proven_crypto.h"
#endif
#if __has_include("proven_codec.h")
#include "proven_codec.h"
#endif
#if __has_include("proven_net.h")
#include "proven_net.h"
#endif
#endif

#endif /* PROVEN_H */
