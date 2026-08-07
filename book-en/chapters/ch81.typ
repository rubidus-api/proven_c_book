#import "../../book/lib.typ": *

= Inside the allocator — the heap, alternative allocators, alternative standard libraries

#prereq(
  ([chapter 43, Dynamic memory], [dynamic allocation]),
  ([chapter 80, A program's map of memory], [a program's map of memory]),
)

#deepqa[
  Chapter 43 said one `malloc` is "a trip to the warehouse office", and
  chapter 80 showed where in the address space that warehouse lies. Then what
  ledgers exactly does that office keep inside?
][
  It writes two things down — *which piece is free* and *how many bytes each piece
  is*. The classic answer manages the former with lists by size (bins) and the
  latter with a header attached to each block. How these two ledgers are designed
  settles an allocator's performance and security entire. This chapter is the map of
  that design.
]

#organizer[
#idx("allocator")  We open the inside of the `malloc` chapter 43 passed over saying
  only "it is expensive". How an allocator manages free pieces (bins, boundary
  tags, coalescing), why there is a cache per strand, what fragmentation is and why
#idx("fragmentation")  it does not recover. Then the alternatives that can be
  swapped in for the standard `malloc` (jemalloc, tcmalloc, mimalloc, snmalloc) and
  the alternative standard libraries (musl, picolibc and others), and the arenas and
  pools that change the very shape of allocation. The end of this chapter is the
  door to Part XII.
]

#chapter-questions()

== Ledger ① — the header attached to each block

`free(p)` does not take a size. And yet it must know how many bytes are being
returned. The answer is simple — *the size is written just before the address that
was returned.*

```text
        ┌────────────┬──────────────────────────┐
        │ header     │ the place given to you   │
        │ size·flags │ ← the address malloc gave│
        └────────────┴──────────────────────────┘
```

This header explains two facts seen in chapter 43. *Even a request for 1 byte
consumes far more* (the header plus alignment padding), and *making countless small
pieces makes the management information as large as the data.*

The classic design (the dlmalloc family) lays one more layer on top of this. It
writes the size *at the end of the block too* — a *boundary tag*. Then from any
block the size of "the block just before" can be known at once, so *coalescing* with
a neighbouring free piece on release finishes in constant time. It is the most basic
device for preventing pieces from scattering finely.

#misconception[
  "A heap block's header is an internal matter, no need to care"
][
  It is worth caring about for two reasons.

  First, *security*. The header being right beside the user data, one buffer
  overflow can overwrite a neighbouring block's header. Then the allocator's ledger
  tells lies, and an attacker can make "the next `malloc` return the address I want".
  The whole family of techniques called heap exploitation grows from this place. So
  modern allocators put defences in — scrambling the free list's pointers with the
  address (safe-linking), inserting marks that notice a double free, or putting the
  header entirely away from the user data (mimalloc and snmalloc below go in that
  direction).

  Second, *memory accounting*. The answer to "why do a million 100-byte pieces eat
  160 MB rather than 100 MB" is this header and the alignment padding.
]

== Ledger ② — lists by size and caches per strand

Keep the free pieces in one long list and every request must scan the whole list. So
the lists are divided by size — these are *bins*. Taking glibc's allocator as an
example, there are roughly these layers.

#dtable(
  columns: 3,
  [*layer*], [*what*], [*why*],
  [tcache], [a small cache kept separately per strand], [to take pieces out at once without locking],
  [fastbin], [singly linked lists of small sizes], [quick reuse without coalescing],
  [smallbin], [lists by exact size], [finding is constant time],
  [largebin], [sorted lists by range], [to choose a fitting size],
  [unsorted bin], [a temporary place for the just-freed], [to reuse as it is for the next request],
  [top chunk], [the lump remaining at the very end of the heap], [when short, it is cut from here],
)

The heart of it is the *cache per strand* (tcache). If several strands touch the
same ledger it becomes slow through locking (chapter 77), so each strand keeps a
small drawer of its own and takes from there first. Only when the drawer is empty
does it go to the shared ledger. It is the common design of today's fast allocators,
differing only in name (tcache, thread cache, mimalloc's local heap) while the idea
is the same.

Two things seen in chapter 80 attach here. When the heap is short it is enlarged
with `brk` (for small requests), and large requests are received separately through
`mmap`. And releasing a large block may return it to the operating system, while
small ones are mostly only marked in the ledger.

== Fragmentation — memory grows though there is no leak

#idx("fragmentation")*Fragmentation* has two faces.

*Internal fragmentation* — the waste arising from giving a piece larger than the
request. Request 24 bytes and be given a 32-byte slot and 8 bytes die. It is the
fate of allocators that use size classes, and speed is gained in exchange.

*External fragmentation* — the state in which the total free space is ample but
there is no *contiguous* large lump, so a large request fails. If small pieces are
lodged among the large ones, those large pieces cannot be joined.

This is the identity of the phenomenon "there is no leak and yet memory keeps
growing". It appears especially in long-running servers, and the cause is usually
*allocating things of different lifetimes mixed together* — one long-lived small
object lodged in the middle of a large empty region ties up that whole region.

#dtable(
  columns: 2,
  [*how to mitigate it*], [*explanation*],
  [gather things of similar lifetime], [an arena does exactly this (below)],
  [things of the same size into a pool], [no pieces arise],
  [take long-lived objects early], [taken at startup, they do not lodge in the middle],
  [change the allocator], [there are ones with different purge and shrink policies (below)],
  [periodic restarts], [an honest last resort — really used],
)

#realcase[
  Why RSS does not go down
][
  There is a conversation that recurs in operations. "There seems to be a memory
  leak — the request has ended and the process memory (RSS) does not go down." And
  yet checking with tools shows no leak.

  The reason is, as seen in chapter 43, that `free` is not a return to the operating
  system, and two more things compound it. First, even to return it *the end side of
  the heap must be empty* to shrink (if even one live block sits below the top chunk
  it cannot shrink). Second, the allocator judges that being sparing with returns is
  advantageous — another request will come soon.

  Hence a rule of observation in the field. *When diagnosing a memory problem do not
  look at RSS alone* — look together at the allocator's statistics (`malloc_info`,
  say) and at the number and size distribution of live allocations. And if you really
  want to give it back there is a road of asking explicitly (glibc's `malloc_trim`,
  the purge settings of jemalloc and mimalloc).
]

== Swapping out the standard `malloc` — alternative allocators

The allocator alone can be changed without mending the program. `malloc` and `free`
are, after all, names, so linking another implementation (statically) or inserting
it at run time (Linux's `LD_PRELOAD`) makes that one be used.

#dtable(
  columns: 3,
  [*allocator*], [*where it came from*], [*character*],
  [glibc malloc (ptmalloc)], [GNU], [the default. balanced. improved with tcache],
  [jemalloc], [FreeBSD → Meta], [size classes and arenas. strong on suppressing fragmentation and on statistics],
  [TCMalloc], [Google], [centred on per-strand caches. multi-strand throughput],
  [mimalloc], [Microsoft], [relatively new (2019–). small and fast, with a safety-minded header layout],
  [snmalloc], [MS Research], [a design that passes cross-strand frees as messages],
  [scudo], [LLVM], [security-hardened. the default on Android],
)

How much difference does changing make — *it depends on the workload* is the honest
answer. On a server with many strands and a flood of small allocations tens of per
cent can change, while in a single-strand computational program there is almost no
difference. So the rule is one: *measure, then change.* And before changing, trying
chapter 43's first prescription — not allocating at all in the hot loop — is usually
the greater gain.

#platform[
  How swapping is actually done
][
  - *At run time* — `LD_PRELOAD=/usr/lib/libjemalloc.so ./program` (Linux). Neither
    the code nor the build is touched.
  - *At link time* — join it as `-ljemalloc` and that `malloc` is used.
  - *Windows* — the above do not work. It is common to use the CRT's heap, or to
    link the allocator as a library and call its API directly from your own code.
  - *The road that works anywhere* — writing the code from the start so that it
    *takes an allocator as an argument*. This chapter's last section and Part XII
    are that way.
]

== Alternative standard libraries

There is also the choice of swapping out one layer larger than the allocator — the
standard library implementation itself. It is the flesh of chapter 59's "the
standard is a list and the implementations are several".

#dtable(
  columns: 3,
  [*implementation*], [*where it is used*], [*character*],
  [glibc], [mainstream Linux distributions], [the most features. large and rich in extensions],
  [musl], [containers, static linking, Alpine], [small and simple. advantageous for static linking],
  [uClibc-ng], [small embedded Linux], [features chosen and cut down],
  [picolibc], [bare metal, RTOS], [a small one split off from newlib and avr-libc],
  [newlib(-nano)], [the default of embedded toolchains], [it comes with `arm-none-eabi-gcc`],
  [Bionic], [Android], [security-minded (the scudo allocator)],
  [UCRT], [Windows], [the runtime MSVC and MinGW use],
)

The criteria for choosing are mostly three — *size* (embedded, containers), *the
convenience of static linking* (musl's strength), and *the breadth of features*
(glibc's strength). And what one runs into when moving is mostly not standard C but
*extensions*: functions only glibc has, locale handling, subtle differences in
threads and signals. Chapter 59's conclusion — "use only what is in the standard and
it ports" — pays here.

== Alternatives that change the shape — arenas, pools, and the allocator as an argument

Until now the story has been *making the same `malloc` better*. There is another
road. It is changing *the very shape* of allocation.

*Arena (region, bump allocator).* Take a large lump once, and when a request comes
merely push a pointer forward. Allocation is one addition and so nearly free, and
release *is not done individually* — when the work is done the pointer is returned to
the beginning and the whole is thrown away. There is no fragmentation, the time is
constant, and release cannot be forgotten. In exchange, individual release is given
up.

This way fits perfectly "things of the same lifetime". One request of a web server,
one function of a compiler, one frame of a game — for each unit of work with a clear
beginning and end, keep one arena and reset it when it ends. It really is widely
used (Apache's memory pools and PostgreSQL's memory contexts are the same idea).

*Pool (slab).* An allocator that handles only pieces of the same size. Slots cut in
advance are managed as a list, so allocation and release are one list operation each,
and there is no internal fragmentation either. It fits data structures with tens of
thousands of nodes, or the fixed-object management of embedded work seen in
chapter 80.

*Taking the allocator as an argument.* For the two above to have force, a library
must *ask the caller* "where shall I get memory from". So modern C libraries make a
bundle of allocation functions into one value and take it as an argument. The table
of function pointers learned in chapter 57 does exactly this work here.

#realcase[
  proven's memory model — a trailer for the next part
][
  The library this book has leaned on uses exactly this design. proven handles the
  allocator as one value (`proven_allocator_t`) — inside it are the `alloc`,
  `realloc` and `free` function pointers and a context (`ctx`), exactly
  chapter 57's vtable. And there are three providers that make that value.

  - `proven_heap_allocator()` — the standard `malloc` wrapped in that interface.
  - `proven_arena_create(backing)` — takes *memory already secured* and makes it a
    bump allocator. It is returned whole with `proven_arena_reset` and fitted into
    the same interface with `proven_arena_as_allocator`.
  - `proven_pool_init(...)` — lays a pool of same-size pieces over a backing
    allocator.

  This structure gives three things. First, *the same data-structure code runs on
  the heap, on an arena and in a pool* — where memory comes from is settled by an
  argument. Second, the embedded norm seen in chapter 80 (no dynamic allocation) can
  be kept — give a static array as the arena's backing and `malloc` is never called
  once. Third, failure comes back as a value (`proven_result_mem_mut_t`) — instead of
  checking for null you check the result.

  We meet these three as real code in Part XII. The heap's circumstances seen in
  this chapter — that it is expensive, fragments, can fail, and is sometimes
  forbidden outright in embedded work — are the whole reason for that design.
]

#qa[
  Then should `malloc` no longer be used?
][
  No. `malloc` is still the right answer as the general-purpose tool for handling
  *things of assorted lifetimes*. The knack is fitting the tool to the purpose.

  - the lifetime equals a unit of work → *an arena*
  - the sizes are all the same → *a pool*
  - lifetimes and sizes are assorted → *`malloc`* (or an allocator wrapping it)
  - real-time or safety-critical → *static allocation*, plus the two above

  And whichever is used, chapter 43's discipline stands — settle the ownership,
  confirm failure, and check with tools (ASan, Valgrind).
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [the header], [the size is written just before the user's place — an overflow breaks the ledger],
    [boundary tags], [writing the size at both ends makes coalescing with a neighbour constant time],
    [bins], [lists by size. the per-strand cache (tcache) avoids locking],
    [internal/external fragmentation], [waste vs. no *contiguous* large lump left],
    [RSS], [it may not go down even after `free` — distinguish it from a leak],
    [alternative allocators], [jemalloc, TCMalloc, mimalloc, snmalloc, scudo. *measure, then change*],
    [alternative libcs], [musl, picolibc, newlib, Bionic — a trade of size, static linking and features],
    [arena], [only pushing, and throwing the whole away. for things of the same lifetime],
    [pool], [the same size only. no fragmentation],
    [the allocator as an argument], [the same code runs on heap, arena or pool alike (Part XII)],
  )
]

We have walked the terrain of the standard library and opened even the warehouse
beneath it. The next part is the attempt to handle all these traps and costs *by
design* — proven, the library this book has leaned on.
