#import "../../book/lib.typ": *

= Allocation is a parameter

#prereq(
  ([chapter 41, Dynamic memory], [dynamic memory]),
  ([chapter 70, Inside the allocator], [inside the allocator]),
)

#deepqa[
  Chapter 41 said memory obtained with `malloc` must be `free`d by somebody exactly
  once, and that settling that "somebody" is the program's design. Then is there a
  way to know from a function alone whether "this function allocates memory"?
][
  In standard C there is not. `malloc` can be called anywhere, so any function can
  allocate in secret, and the signature does not say so. proven's answer is to return
  that information to the signature with one rule — *a function that does not take an
  allocator as an argument does not allocate.* If this rule is kept, reading the
  signature alone tells you "this function may take memory".
]

#organizer[
  The answer to chapter 71's fourth bug — unclear ownership. The *allocator* that
#idx("allocator")  handles the source of memory as a value, the *arena* that returns
#idx("arena")  things of the same lifetime all at once, and the rule that divides
  owning from borrowing by type. The dangers of the heap learned in chapter 41 are
  organised here into design.
]

#chapter-questions()

== An allocator is a value

`proven_allocator_t` is one struct — a context pointer and three function pointers
(allocate, reallocate, free). There is no special global and no registration
procedure. Being simply a value, it can be passed as an argument, held in a struct,
and returned from a function. The *virtual function table* seen in chapter 53 is here
as it stands.

```c
typedef struct {
    void *ctx;                        /* this allocator's state (for an arena, the arena) */
    proven_alloc_fn_t   alloc_fn;
    proven_realloc_fn_t realloc_fn;
    proven_free_fn_t    free_fn;
} proven_allocator_t;
```

The three functions' signatures write the contract down as they stand.

```c
proven_result_mem_mut_t (*alloc_fn)(void *ctx, proven_size_t size,
                                    proven_size_t align);
proven_result_mem_mut_t (*realloc_fn)(void *ctx, void *old_ptr,
                                      proven_size_t old_size,
                                      proven_size_t new_size,
                                      proven_size_t align);
void                    (*free_fn)(void *ctx, void *ptr);
```

*Why pass `old_size` and `align` too.* The standard `realloc` does not ask the old size
— because the allocator wrote it into the block's header (chapter 70). But an arena has
no header. Not making a place to write the size is why an arena is fast, so the choice
was for *the caller to tell it* instead. This decision is what lets a "headerless
allocator" fit the same interface.

Four things from the contract must be remembered.

+ *`align` must be a power of two.* And *a block must be reallocated and freed with the
  same alignment it was allocated with.* Because the heap allocator handles requests at
  or below the default alignment differently from stricter ones (chapter 74) — handing
  a block back in a different alignment class is outside the contract.
+ *`size == 0` is `INVALID_ARG`.* A zero-byte allocation is treated as a caller's bug.
  Before this rule the heap said `NOMEM` (a lie — nothing was out of memory) and the
  arena returned a valid pointer, so *the answer differed per allocator*. Generic code
  cannot be written on a rule like that.
+ *A reallocation with `new_size == 0` is a free* — it gives a null pointer and
  `PROVEN_OK`.
+ *Reallocation is failure-atomic.* On failure `old_ptr` is still valid and its contents
  untouched — chapter 60's `realloc` leak counterexample blocked at the level of the
  interface.

And every function that allocates has this shape.

```c
proven_result_u8str_t proven_u8str_create(proven_allocator_t alloc,
                                          proven_size_t limit);
void                  proven_u8str_destroy(proven_allocator_t alloc,
                                           proven_u8str_t *str);
```

*Destroy with the allocator given at creation* — that is the whole of the ownership
rule.

== Swapping the three sources

#demo("examples-en/ch75/three.c")

Look at the `make_list` function. This function *does not know* whether the memory it
uses comes from `malloc`, from a static array, or from a recycling bin. The caller
settles it, and the same code runs identically over all three.

Three things can be read in the output.

*① An arena's usage is visible.* That `arena.offset` is 129 means exactly 129 bytes
(128 of content + 1 NUL) went out for one string. A number that cannot be known on the
heap can be counted in an arena — in embedded work this transparency pays when setting
a memory budget.

*② Reset makes individual release meaningless.* The usage did not fall although
`destroy` was called (an arena's `free` does nothing), and one `reset` took it to 0.
"Instead of giving back individually, give the whole back" is this picture.

*③ When it runs dry it refuses as a value.* Requesting 128 bytes from a 64-byte arena
gave back `NOMEM` (1). It neither collapses nor quietly falls back to the heap.

#qa[
  Besides `heap`, `arena` and `pool`, can I make an allocator myself?
][
  Of course. Chapter 73's example already did — a testing allocator that "fails from the
  nth call" made of three functions and inserted. The only rule is keeping the contract
  above.

  The cases for making one in practice are mostly these. *Instrumentation* — a shell
  counting allocations and peak usage. *Testing* — failing on purpose, painting freed
  memory with 0xDD. *Debugging* — recording the place (file and line) of an allocation.
  *Special resources* — obtaining from shared memory or a DMA-capable region, places
  `malloc` cannot give.

  All four take the shape of *wrapping an existing allocator*. They hold a backing
  allocator inside, do their own work and pass it on. Chapter 73's example is the model.
]

#qa[
  Why is this such an important property? Most of the time the heap will be used
  anyway.
][
  Because three things follow at once. First, *the same code runs in an environment
  with no heap* — such is embedded work (chapter 80), and such is inside a kernel.
  Second, *testing becomes easy* — insert an allocator that fails on purpose and you
  can test "does this code recover properly when memory runs short". Third, *the
  caller can choose the performance* — for data that lives briefly and dies all at
  once, an arena is far faster than the heap. The moment the library calls `malloc`
  directly, all three of these vanish.
]

== The arena — things of the same lifetime, all at once

Chapter 41 taught the heap's three dangers (leak, double free, use after free). All
three come from *individual release*. Then what if individual release were removed —
that is the arena's idea.

An arena takes one large lump of memory and, whenever a request comes, cuts from the
front. There is a release function but it does nothing. Instead there is *reset* — it
returns everything at once.

That the struct has only two slots shows this simplicity as it stands.

```c
typedef struct {
    proven_mem_mut_t backing;   /* the memory that backs it (borrowed) */
    proven_size_t    offset;    /* how far it has been handed out */
} proven_arena_t;
```

*An arena does not own its memory.* That `backing` is a writing window (`mem_mut_t`) is
that declaration — a static array, a stack array, or a lump received once from the heap
is prepared by the caller, and the arena only cuts on top of it. So
`proven_arena_destroy` *does nothing* (there is nothing borrowed to give back). If the
lump came from the heap, returning it is still the caller's part.

The life cycle is four steps.

#dtable(
  columns: 3,
  [*step*], [*function*], [*what happens*],
  [making], [`proven_arena_create(backing)`], [`offset = 0`. no allocation],
  [handing out], [`proven_arena_alloc(&a, n)`], [aligns and pushes `offset`],
  [taking back], [`proven_arena_reset(&a)`], [`offset = 0` — everything becomes invalid],
  [ending], [`proven_arena_destroy(&a)`], [the formal partner. it does nothing],
)

The allocation function has four editions. Choose the one that fits.

#dtable(
  columns: 2,
  [*function*], [*when to use it*],
  [`proven_arena_alloc(&a, size)`], [the default. cuts at the default alignment],
  [`proven_arena_alloc_aligned(&a, size, align)`], [data where alignment matters (SIMD, DMA)],
  [`proven_arena_alloc_or_panic(&a, size)`], [places where failure should stop the program],
  [`proven_arena_realloc_aligned(...)`], [grows in place if it is the last block],
)

The `_or_panic` family exists for the reason given in chapter 73 — where the memory
budget has been calculated in advance (embedded work is representative), "the arena is
short" is not a failure to recover from but *a design error*, so demanding a check every
time is rather noise.

The `realloc` has one interesting property. An arena having no header, ordinary
reallocation is "cut anew and copy", but *if the block being grown happens to be the
last one handed out* it can grow in place simply by pushing `offset`. It is why code
that appends to a string a little at a time is fast on an arena, and why calling
chapter 76's `proven_u8str_reserve` in advance pays especially on an arena (an
intervening allocation breaks the property).

#demo("examples-en/ch75/arena.c")

There really are many places this model fits. The temporary data made while handling
one request, a game's calculation results used for one frame, the syntax tree made
while parsing one file — all data *born at different moments but dying at the same
one*. For such data, individual release is only cost and risk.

#recap[
  A comparison of the three sources of memory.

  #dtable(
  columns: 4,
    [], [*heap*], [*arena*], [*pool*],
    [how it gives], [arbitrary sizes], [cuts from the front], [fixed-size slots],
    [individual release], [yes], [no (reset)], [yes (return the slot)],
    [fragmentation], [can arise], [none], [none],
    [speed], [ordinary], [very fast], [very fast],
    [where it fits], [assorted lifetimes], [a group of the same lifetime], [the same size repeatedly],
)
]

#idx("pool")The pool (`proven_pool_t`) is the third branch. It is used where objects
of the same size are repeatedly made and unmade — a game's bullets, a server's
connection objects, a parser's nodes. The slot size being fixed there is no
fragmentation, and a returned slot is reused at once. A pool too becomes an allocator
through `proven_pool_as_allocator`, so the previous example's `build` function runs
on it as it is.

#antipattern[
  Destroying with a different allocator
][
  ```c
  proven_result_u8str_t r = proven_u8str_create(proven_heap_allocator(), 64);
  ...
  proven_u8str_destroy(proven_arena_as_allocator(&arena), &r.value);  /* wrong */
  ```
  It amounts to giving back to an arena what was obtained from `malloc`. This rule
  cannot be enforced by the library — an allocator is simply a value, and which value
  is passed is settled by the caller. So the practice in the field is *to carry the
  allocator along with the data structure*, or to narrow the scope so that only one is
  used within a module.
]

#misconception[
  "Use an arena and you need not care about releasing"
][
  Individual release disappears but *the lifetime remains as it was.* The moment the
  arena is reset, every piece cut on it becomes invalid at once, so data that must
  still be alive after the reset must not be put in the arena. Chapter 74's "a view
  cannot outlive its owner" has grown here to arena scale. What an arena removes is
  the *number of times* one releases, not the *thinking* about lifetime.

  ```c
  proven_u8str_t name;
  for (int i = 0; i < n; i++) {
      proven_arena_reset(&per_request);        /* taken back per request */
      handle(proven_arena_as_allocator(&per_request), &name);
  }
  use(&name);        /* ← dangerous. what name pointed at has already gone back */
  ```

  The discipline in the field is one — *call the arena's lifetime and the lifetime of
  the data on it by the same name.* "the request arena", "the frame arena", "the file
  arena". Then which data must survive the reset shows itself in the name alone, and
  data that must cross over is copied to *a longer-lived allocator*.
]

== Which to choose

#dtable(
  columns: 4,
  [*situation*], [*what to choose*], [*why*], [*caution*],
  [data of assorted lifetimes], [the heap], [general-purpose. individual release possible], [cost and fragmentation (chapter 70)],
  [data born and dying with a unit of work], [an arena], [allocation is one addition, release one reset], [everything invalid after a reset],
  [the same struct by the thousand], [a pool], [no fragmentation, immediate reuse], [one size only],
  [an environment with no heap at all], [a static array + an arena], [`malloc` is never called once], [the budget must be calculated in advance],
  [testing the out-of-memory path], [a failing shell (chapter 73)], [it runs the path that never otherwise runs], [for testing only],
)

In practice these four are *layered*. The program as a whole uses the heap, while
handling one request uses a request arena, and a data structure making thousands of
nodes inside it lays a pool on top. That all three share the same interface, so that
layering needs no special device, is the value of this design.

== Owning and borrowing, and state that points at itself

Now we return to chapter 71's fourth bug. The problem of `char *` meaning four things
#idx("owning and borrowing")is solved by dividing the type in two.

- *Owned* — things such as `proven_u8str_t` and `proven_array_t`. Obtained with
  `_create` and let go with `_destroy`.
- *Borrowed* — `proven_u8str_view_t`, `proven_mem_view_t`. Never destroyed. When the
  owner vanishes they become invalid with it.

That "must I release this?" can be answered from the signature alone is the whole of
this distinction and its purpose.

There is one more rule attached. *State that points at itself is not copied.*
Chapter 42 taught that struct assignment is a shallow copy. Copy an object that holds
inside a pointer to its own buffer that way, and the copy's pointer still points at
the original, so two objects share the same memory and destroying both is a double
free. So such objects are not copied by value but passed by pointer.

#realcase[
  The spread of the allocator-as-argument design
][
  This way is not proven's invention but is close to the recent consensus of systems
  programming. In Zig, almost every allocating API of the standard library takes an
  allocator as an argument and "no hidden allocation" is the language's motto. In Rust
  too, attaching an allocator to a container is on its way into the standard, and
  C++'s `std::pmr` arose from the same problem-consciousness. The reason is the same
  in every case — in games, embedded work, kernels and high-performance servers,
  *choosing the source of memory* is the heart of both performance and safety.
]

#qa[
  Then what is the price?
][
  One more parameter attaches to the signature. And a new design problem arises of
  *how far the allocator is to be carried* — whether to pass it to every function or
  to hold it in a struct. In a small program this can look like nothing but tiresome
  formality. Its value shows itself after the program grows, or when the code must be
  moved to a place where the heap cannot be used.
]

We have set up the rules for obtaining and letting go of memory. From the next chapter
come the real components that rise on top of it — first, the strings this book warned
about all through chapter 38.
