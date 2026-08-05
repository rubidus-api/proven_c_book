#import "../../book/lib.typ": *

= Allocation is a parameter

#organizer[
  The answer to chapter 69's fourth bug — unclear ownership. The *allocator* that
#idx("allocator")  handles the source of memory as a value, the *arena* that returns
#idx("arena")  things of the same lifetime all at once, and the rule that divides
  owning from borrowing by type. The dangers of the heap learned in chapter 40 are
  organised here into design.
]

#deepqa[
  Chapter 40 said memory obtained with `malloc` must be `free`d by somebody exactly
  once, and that settling that "somebody" is the program's design. Then is there a
  way to know from a function alone whether "this function allocates memory"?
][
  In standard C there is not. `malloc` can be called anywhere, so any function can
  allocate in secret, and the signature does not say so. proven's answer is to return
  that information to the signature with one rule — *a function that does not take an
  allocator as an argument does not allocate.* If this rule is kept, reading the
  signature alone tells you "this function may take memory".
]

== An allocator is a value

`proven_allocator_t` is one struct — a context pointer and three function pointers
(allocate, reallocate, free). There is no special global and no registration
procedure. Being simply a value, it can be passed as an argument, held in a struct,
and returned from a function.

So every function that allocates has this shape.

```c
proven_result_u8str_t proven_u8str_create(proven_allocator_t alloc,
                                          proven_size_t limit);
void                  proven_u8str_destroy(proven_allocator_t alloc,
                                           proven_u8str_t *str);
```

*Destroy with the allocator given at creation* — that is the whole of the ownership
rule.

#demo("examples/ch73/swap.c")

Look at the `build` function. This function *does not know* whether the memory it
uses comes from `malloc` or from a static array. The caller settles it. The same code
ran once on the heap and twice on an arena, and the results are the same.

#qa[
  Why is this such an important property? Most of the time the heap will be used
  anyway.
][
  Because three things follow at once. First, *the same code runs in an environment
  with no heap* — such is embedded work (chapter 78), and such is inside a kernel.
  Second, *testing becomes easy* — insert an allocator that fails on purpose and you
  can test "does this code recover properly when memory runs short". Third, *the
  caller can choose the performance* — for data that lives briefly and dies all at
  once, an arena is far faster than the heap. The moment the library calls `malloc`
  directly, all three of these vanish.
]

== The arena — things of the same lifetime, all at once

Chapter 40 taught the heap's three dangers (leak, double free, use after free). All
three come from *individual release*. Then what if individual release were removed —
that is the arena's idea.

An arena takes one large lump of memory and, whenever a request comes, cuts from the
front. There is a release function but it does nothing. Instead there is *reset* — it
returns everything at once.

#demo("examples/ch73/arena.c")

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
  still be alive after the reset must not be put in the arena. Chapter 72's "a view
  cannot outlive its owner" has grown here to arena scale. What an arena removes is
  the *number of times* one releases, not the *thinking* about lifetime.
]

== Owning and borrowing, and state that points at itself

Now we return to chapter 69's fourth bug. The problem of `char *` meaning four things
#idx("owning and borrowing")is solved by dividing the type in two.

- *Owned* — things such as `proven_u8str_t` and `proven_array_t`. Obtained with
  `_create` and let go with `_destroy`.
- *Borrowed* — `proven_u8str_view_t`, `proven_mem_view_t`. Never destroyed. When the
  owner vanishes they become invalid with it.

That "must I release this?" can be answered from the signature alone is the whole of
this distinction and its purpose.

There is one more rule attached. *State that points at itself is not copied.*
Chapter 41 taught that struct assignment is a shallow copy. Copy an object that holds
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
about all through chapter 37.
