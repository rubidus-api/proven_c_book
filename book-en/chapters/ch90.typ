#import "../../book/lib.typ": *

= Containers and algorithms

#prereq(
  ([chapter 45, Structs], [structs]),
  ([chapter 44, Dynamic memory], [a growing array]),
  ([chapter 86, The foundation], [views and bounds]),
)

#deepqa[
  Chapter 37 taught that a C array's size is settled at compile time, and chapter 44
  that it can be grown with `realloc`. Then where is it easiest to go wrong when
  making a "growing array" yourself?
][
  In three places. First, *calculating the size to grow to* — the multiplication
  wrap-round seen in chapter 86 happens here. Second, *the state on failure* — when
  `realloc` fails the original pointer is still valid, and the common code that
  assigns the return value straight into the original variable loses that original (a
  leak). Third, *pointers after the growth* — a pointer that pointed at an element
  becomes invalid after reallocation. Rather than getting these three right afresh
  every time you make a container, it is better to use one made properly once.
]

#organizer[
#idx("hash map")  The tools that hold many — the growing array, the intrusive list,
  the ring buffer, the hash map. And as the answer to chapter 83's fifth bug
  (unchecked callbacks) and the performance trap attached to that place, we see
  sorting *guaranteed even in the worst case* and hashing that *withstands attack*.
  Chapter 37's arrays and chapter 45's structs become practical components here.
]

#chapter-questions()

== The life cycle of the four containers

First we see the four on one screen. How making, putting in, traversing and giving back
differ between them is all in this example.

#demo("examples-en/ch90/tour.c")

#dtable(
  columns: 5,
  [*container*], [*making*], [*allocation*], [*traversal*], [*giving back*],
  [`array`], [`PROVEN_ARRAY_INIT(alloc, T, n)`], [grows], [by index], [`PROVEN_ARRAY_DESTROY`],
  [`list`], [`proven_list_init(&l)`], [*none*], [`PROVEN_LIST_FOR_EACH`], [unnecessary],
  [`ring`], [`PROVEN_RING_INIT(alloc, T, n)`], [once, fixed], [by popping], [`proven_ring_destroy`],
  [`map`], [`PROVEN_MAP_INIT_U8_OWNED(alloc, T, n)`], [grows (rehashing)], [by key lookup], [`proven_map_destroy`],
)

That only the list has no allocation stands out — because the node lives inside the
data. So a list is the only container that *cannot fail for lack of memory*, and it is
especially loved in embedded work (chapter 92).

== The growing array

`proven_array_t` is a byte buffer that knows the element size and alignment. Open it up
and why the type is filled in by macros becomes clear.

```c
typedef struct {
    proven_allocator_t alloc;       /* it remembers the allocator given at creation */
    proven_byte_t     *data;        /* the byte buffer */
    proven_size_t      len;         /* the number of elements held now */
    proven_size_t      cap;         /* the number of elements it can hold */
    proven_size_t      elem_size;   /* the size of one element */
    proven_size_t      align;       /* the element's alignment requirement */
} proven_array_t;
```

*That the array remembers the allocator* differs from strings. A string takes an
allocator per operation (chapter 88), while an array takes one at creation and keeps it
inside — making `push` pass an allocator every time it grows would make the code noisy.
So the allocator is not given again at destruction either
(`PROVEN_ARRAY_DESTROY(&arr)`).

C having no generics, the type is filled in by macros.

#demo("examples-en/ch90/arr.c")

`PROVEN_ARRAY_INIT(alloc, int, 4)` means "an array to hold ints, initial capacity 4",
and `PROVEN_ARRAY_PUSH` writes the type again to *check at compile time that it
matches*. At the fifth element, which exceeded the capacity of 4, the array grew by
itself — and that growth happens through the allocator (exactly chapter 87's rule; the
array remembers the allocator it was given at creation).

#antipattern[
  Holding an element pointer and then pushing
][
  ```c
  int *first = PROVEN_ARRAY_GET_MUT(&arr, int, 0);
  (void)PROVEN_ARRAY_PUSH(&arr, int, 42);   /* the buffer may move here */
  *first = 7;                               /* writes at the old address — use after free */
  ```
  When the array grows the contents move to a new buffer and a pointer to the old
  address becomes invalid. It is the form in which the use after free learned in
  chapter 44 appears on a container. The rule is one — *after changing a container,
  obtain it again by index.* The habit of carrying an index rather than a pointer
  becomes the defence here.
]

== The intrusive list — linking without allocation

`proven_list_t` is an *intrusive* linked list — the node does not hold the data;
rather a link field is planted inside the data struct. It amounts to putting one
`proven_list_node_t link;` slot inside a `struct` made in chapter 45.

```c
typedef struct proven_list_node_t {
    struct proven_list_node_t *next;
    struct proven_list_node_t *prev;
} proven_list_node_t;

typedef struct {
    proven_list_node_t head;      /* a sentinel pointing at itself */
} proven_list_t;
```

That `head` is a *sentinel* is this implementation's knack. In an empty list
`head.next` and `head.prev` point at head itself, and so not one "is it null" check
appears in the insertion and deletion code — a pattern the Linux kernel long used.

What is good about it? *There is nothing to allocate separately* in order to put
something in the list — because the node is the data. A list can be used even in an
environment with no heap (chapter 92), and hanging the same object on two lists at
once is a matter of keeping two link fields. The price is that the data struct must
know about the list.

Getting back the other way is the problem, and one macro solves it.

```c
task_t *t = PROVEN_LIST_ENTRY(node, task_t, link);
```

It is the macro that *works the struct's starting address back* from the address of the
`link` member — the `offsetof` seen in chapter 45 used here in the flesh. This one line
returning from node to data is nearly the whole of the intrusive list.

There are two traversal macros.

#dtable(
  columns: 3,
  [*macro*], [*what it does*], [*when*],
  [`PROVEN_LIST_FOR_EACH(it, &l)`], [traverses from the front], [when only reading],
  [`PROVEN_LIST_FOR_EACH_SAFE(it, tmp, &l)`], [holds the next node in advance], [★ when removing while traversing],
)

The star matters. Remove a node during traversal and its `next` becomes meaningless, so
the loop loses its way. The `_SAFE` edition turns with *the next node already in hand*,
so removing the present node is safe. It is why the example used it when detaching item
20.

It is worth knowing too that both macros require the iteration variables to be
*declared in advance* (`proven_list_node_t *node, *tmp;`). The macro does not put a
declaration in the `for`'s initialiser — a kernel practice carried on from the C89 days.

== The ring buffer — a stream of fixed size

`proven_ring_t` is a fixed-size circular buffer. It is used where a producer and a
consumer come and go, for the most recent N of a log, and for streams such as audio
and sensor samples where *what has passed may be thrown away*. The size being fixed,
what to do when it is full is part of the contract — and here too the default is to
report rather than quietly overwrite (the example's `err=2` is that confirmation).

There are only `push` and `pop`, and both *copy the element*. Putting in gives an
address, and taking out gives the address of the place to receive it.

```c
int v = 42;
proven_err_t e = proven_ring_push(&ring, &v);     /* copies the value in */
int out;
e = proven_ring_pop(&ring, &out);                 /* copies the value out */
```

Thanks to this design no pointer into an element inside the ring leaks outward — holding
a pointer in a circular buffer and having that place overwritten is a classic accident,
and it was blocked by the interface.

== The hash map, and data structures under attack

`proven_map_t` is an open-addressing hash map. Keys are integers or byte sequences,
and string keys have two modes — *a borrowed key* (the caller keeps the bytes alive)
and *an owned key* (the map copies and holds it). Chapter 87's owning-borrowing
distinction appears here as it is too.

The kind of key is settled at creation.

#dtable(
  columns: 3,
  [*creation macro*], [*key*], [*caution*],
  [`PROVEN_MAP_INIT_INT(alloc, T, n)`], [an integer (`key.id`)], [the fastest],
  [`PROVEN_MAP_INIT_U8_BORROWED(alloc, T, n)`], [a string (borrowed)], [★ the key bytes must outlive the map],
  [`PROVEN_MAP_INIT_U8_OWNED(alloc, T, n)`], [a string (copied)], [a copy cost on insertion. safe in exchange],
)

The key is passed as one union — `.id` for an integer, `.str` for a string.

```c
proven_map_key_t k = { .str = PROVEN_LIT("beta") };
const int *v = proven_map_get(&map, k);      /* null if absent */
```

*Lookup answers with null.* The reason it returns a pointer rather than a bundle is
that "absent" is not a failure but a normal answer (distinct from chapter 85's error
branches). And that pointer *points inside the map*, so it becomes invalid the next time
the map grows — the same rule as with arrays.

There are three functions for putting in. `proven_map_set` (general),
`proven_map_set_u8_owned` (copying a string key in), and
`proven_map_set_with_scratch` (giving the temporary memory separately). The last is used
when the temporary buffers of internal work such as rehashing are to be obtained from
another allocator — a device for keeping dead memory from piling up when running a map
on an arena.

#demo("examples-en/ch90/wordcount.c")

This one example contains all of this chapter's tools — it counts with a map, gathers
into an array, sorts and prints. Chapter 88's views were used to cut the words, so
string copying happens only once, when the map owns the key.

#realcase[
  HashDoS — when hash maps became a target of attack
][
  In 2011 several web frameworks collapsed at once through the same vulnerability. If
  an attacker chose *keys that crowd into the same bucket* and sent thousands of them
  as parameters in one request, insertion that had been $O(1)$ on average became
  $O(n)$ and the whole degenerated to $O(n^2)$, so a few requests stopped a server. The
  cause was that the hash function was public and collisions *could be calculated*.

  Today's prescription is a *keyed hash* — draw a random secret per process and mix it
  into the hash, and the attacker cannot precompute collisions. It is why proven's
  `proven_map_create` uses SipHash-2-4 and a random seed by default for string keys,
  and conversely why there is a separate `proven_map_create_trusted` using the faster
  FNV-1a for cases where the keys all come from my own code. *The default is the safe
  side, and the fast side states itself in the name* — the principle met repeatedly in
  this part.
]

#qa[
  How much slower is a keyed hash? And where does the random secret come from if
  there is no OS?
][
  SipHash is slower than FNV-1a, but by an amount proportional to the string length,
  and the share hash computation takes in the whole of a map operation is mostly not
  large. The random secret is drawn once from the operating system's source of
  randomness — and in an environment with no OS (chapter 92) there is nowhere to draw
  it from, so it falls back to FNV-1a. The library does not hide this fact but writes
  it in the documentation, and the grounds are clear: *where there is no attacker
  there is no need for an attacker model either.* There exists no outsider choosing
  keys inside your firmware.
]

== Sorting with a worst-case guarantee

The two problems seen in chapter 83 — the unchecked comparator, and the algorithm
that collapses in the worst case — are treated together here.

#idx("introsort")`proven_array_sort` is *introsort*. It begins as a fast quicksort
and, if the recursion becomes too deep, switches to heapsort. So the average is as
fast as quicksort and $O(n log n)$ is *guaranteed* even in the worst case — the
complexity attack seen in chapter 83 does not work. To carry the header's wording
over as it is: "$O(n log n)$ is not an average but a guarantee."

On the comparator side the language cannot help, so the contract is stated in the
documentation and in examples. The example's `by_count_desc` is the model — descending
by count, and *ties broken by the word*. It contrasts exactly with chapter 83's
counterexample (the comparator that sees only the first letter).

#misconception[
  "Ties may be handled however you like"
][
  They may not. A comparator must form a *total order* — 0 if equal, consistently
  greater and less, and the signs of `cmp(a,b)` and `cmp(b,a)` must be opposite. Break
  this and the result is not merely jumbled; depending on the implementation it may
  even *trespass outside the array* (because the partitioning algorithm judges its
  boundaries from the comparison results). A comparator that returns anything at all
  for ties is therefore a bug, not a taste.
]

== Bytes into letters — hashes and encodings

We note the remaining tools in the same box too.

- *Hashes by purpose* — for the map's internals (fast mixing), for integrity checking
  (CRC-32), for cryptographic use (SHA-256), and the keyed hash seen above (SipHash).
  The library distinguishes them because *the same word "hash" means entirely
  different demands* — using SHA-256 where only speed is needed is waste, and using
  FNV-1a where adversarial input comes is dangerous.
- *hex and Base64* — two standards for moving bytes into text. The principle here is
  the same. Wrong input (hex of odd length, wrong padding) is *not guessed at and
  mended* but refused with `PROVEN_ERR_INVALID_ENCODING` (that norm from chapter 88).

#recap[
  Containers in summary.

  #dtable(
  columns: 4,
    [*tool*], [*shape*], [*where it fits*], [*caution*],
    [`array`], [a growing contiguous array], [an ordered list], [pointers invalid after growth],
    [`list`], [an intrusive linked list], [joining without allocation], [the data holds the link],
    [`ring`], [fixed-size circular], [streams, the most recent N], [the contract when full],
    [`map`], [open-addressing hash], [finding by key], [choose whether the key is owned],
    [`array_sort`], [introsort], [sorting anything], [the comparator must be a total order],
    [four hashes], [by purpose], [map, integrity, cryptography, anti-attack], [do not mix the purposes],
)
]

That is as far as the world of pure computation — it all runs even with no operating
system. In the next chapter we go outside. Files, streams, time, random numbers — the
places that touch the OS.
