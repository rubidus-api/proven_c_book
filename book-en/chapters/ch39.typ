#import "../../book/lib.typ": *

= Multidimensional arrays

#prereq(
  ([chapter 38, Arrays], [decay and the contract of pointer arithmetic]),
  ([chapter 37, The rules of pointers], [alignment and provenance]),
  ([chapter 11, Memory divides], [cache lines and locality]),
)

#deepqa[
  Chapter 38 said `a[i]` is sugar for `*(a + i)`, and that adding an integer to a
  pointer moves it by *the size of the type pointed at*. Then in
  `int a[3][4]`, how many bytes does `a + 1` move?
][
  *Sixteen* — not one `int` (four) but one row of four ints. The key is what the
  elements of `a` are. `int a[3][4]` is "three arrays of four ints", so one
  element of `a` is *a whole row* (`int[4]`). Nothing in chapter 38's rule
  changed — "move by one element" applied exactly as before; it is just that the
  element is itself an array.

  This chapter is everything that grows out of that one sentence.
]

#organizer[
#idx("multidimensional array")  Multidimensional arrays — arrays of arrays. How they lie in memory
  (row-major), how a subscript unfolds, why it is not `int **`, how far you may
  go in viewing one address through different types, and the patterns of practice
  (leading dimension, row pointers, strides) along with what traversal order does
  in matrix work.
]

#chapter-questions()

== An array of arrays — the substance first

There is only one way to read `int a[3][4]`: *three arrays of four ints.* "Three
rows by four columns" is the human reading; what the type says is "an array whose
element is `int[4]`".

#demo("examples-en/ch39/md_layout.c")

The first block of the demonstration is the check. `sizeof a` is 48 (the whole),
`sizeof a[0]` is 16 (one row), `sizeof a[0][0]` is 4 (one element). The elements
of `a` are rows; the elements of a row are ints.

*Memory is one run.* In the second block the offsets from `a[0][0]` to `a[2][3]`
run 0, 4, 8 … 44 with no gaps. The standard pins this layout down — *the last
subscript varies fastest* (row-major). A multidimensional array does not fold
memory into a grid; it is *one line* with the rows laid end to end.

#qa[
  Are there languages that lay them out column-major?
][
  Yes. Fortran is the classic one, along with MATLAB, R, Julia, and OpenGL's
  matrix convention. Neither is correct in the abstract — it is a *convention*,
  and C chose row-major.

  Where the difference bites is clear: calling numeric libraries written in
  Fortran (BLAS, LAPACK) from C. The same memory is read in a different order,
  so passing it straight across is passing *the transpose*. That is why such APIs
  almost always carry an argument saying "read this matrix transposed"
  (`CblasRowMajor` / `CblasColMajor`, or a `trans` flag).
]

== How a subscript unfolds

`a[i][j]` is not magic. It is chapter 38's two rules applied twice.

$ a[i][j] arrow.r.double *(a + i)[j] arrow.r.double *(*(a + i) + j) $

Followed one step at a time, with the types:

#dtable(
  columns: 3,
  [*expression*], [*type*], [*size of one step*],
  [`a`], [`int[3][4]` → decays to `int (*)[4]`], [—],
  [`a + i`], [`int (*)[4]`], [`sizeof(int[4])` = 16 bytes],
  [`*(a + i)`], [`int[4]` → decays to `int *`], [—],
  [`*(a + i) + j`], [`int *`], [`sizeof(int)` = 4 bytes],
  [`*(*(a + i) + j)`], [`int` (an lvalue)], [—],
)

The third block of the demonstration shows that table as actual numbers: `a + 2`
lands at offset +32 (= 2 × 16), `*(a+2) + 1` at +36 (= 32 + 1 × 4). Out comes the
value of `a[2][1]`.

*There is no new rule here.* Chapter 38 said "pointer plus integer moves by the
element size"; this time the element happened to be `int[4]`. Half of what makes
multidimensional arrays feel hard is missing that.

#figure-svg("mdarray", caption: [The two jumps that reach `a[2][1]` — one row-sized, one element-sized.])

#qa[
  Then why are `a[i][j]` and `a[j][i]` different places? Both are just additions.
][
  Additions, but *multiplied by different factors.* Written out in bytes the
  offset is `i × 16 + j × 4`. The factor on `i` (the size of a row) and the factor
  on `j` (the size of an element) differ, so swapping them lands elsewhere —
  `a[1][2]` is +24 and `a[2][1]` is +36.

  In general, for `T a[d_1][d_2]...[d_n]` the offset of `a[i_1]...[i_n]` is
  $ (((i_1 times d_2 + i_2) times d_3 + i_3) dots times d_n + i_n) times "sizeof"(T) $
  Every inner dimension enters as a factor, so *only the outermost dimension is
  never used in the computation* — which is why the first dimension may be
  omitted in a parameter (next section).
]

== Parameters — why it is not `int **`

Chapter 38 said an array parameter decays to a pointer but *only the outermost
dimension is stripped*. Here is what that means in more than one dimension.

```c
void f(int m[3][4]);   /* the three are one and the same declaration */
void f(int m[][4]);
void f(int (*m)[4]);
```

The outer `3` goes and the `4` stays. The offset formula above is the reason —
what the computation needs is the *inner* dimensions; the outer one is unused. So
inner dimensions must be written (`int m[][]` does not compile) and the outer may
be written but is not checked.

#demo("examples-en/ch39/md_param.c")

#misconception[
  "A 2-D array can be received as `int **`"
][
  The most common and most expensive misconception. `int a[3][4]` decays to
  `int (*)[4]`, not to `int **`. The two have *entirely different layouts.*

  `int (*)[4]` points at the first row of a place where twelve ints lie in a row.
  One address computes every slot. `int **` points at a place where *pointers to
  int* lie in a row — finding a slot means following an address *twice*, and that
  array of pointers has to actually exist.

  So passing a real 2-D array to a function taking `int **` is rejected by the
  compiler. Force it through with a cast and you get ints *mistaken for
  addresses* and followed — a collapse with no diagnostic. The third block of the
  demonstration puts the two layouts, with their sizes and indirection counts,
  side by side.
]

*The VLA parameter* earns its keep here (chapter 38). Even when the width is
settled at run time, the `a[i][j]` notation still works.

```c
void sum(size_t rows, size_t cols, const int a[rows][cols]);
```

Taking the sizes first is the rule — the names used as dimensions must already be
declared. This is the most readable form in modern numeric code, and the default
this book recommends.

== One address, different eyes — the contract of flattening

The last block of the demonstration is where this section starts. `a`, `a[0]`,
`&a[0][0]` and `&a` are *all the same address*. But their types differ, so *one
step means a different distance* — 16, 4, 4 and 48 bytes respectively. Holding
the same number does not mean being able to do the same things with it (chapter
35's "an address is not simply an integer" made flesh).

Which raises the natural question.

#qa[
  May I take `int *p = &a[0][0];` and sweep it flat as `p[7]`? It is the same
  memory and the offset works out.
][
  It *works* on essentially every compiler, but by the letter of the standard it
  is outside the contract. The reason has to be separated carefully — two rules
  are entangled here and only one of them bites.

  *Strict aliasing (§6.5) is not the problem.* That rule says not to read an
  object through an lvalue whose type does not match the object's effective type,
  and the thing being read here, `a[i][j]`, has effective type `int` either way.
  Reading it through an `int` lvalue breaks nothing.

  *What bites is the range of the pointer arithmetic* (§6.5.6, and chapter 37's
  provenance). `&a[0][0]` points at the first element of *`a[0]`, an array of
  four*. Where that pointer may go is inside `a[0]` and one past its end. `p + 4`
  is that one-past position — it may be formed but *not dereferenced* — and
  `p + 5` is outside the contract from the moment it is formed. That `a[1][0]`
  happens to sit at that address is beside the point: the rule is about
  *provenance*, not about addresses.
]

The grey zone is old, and the committee knows what practice does. The rule stands
because optimisers lean on the promise — "this pointer only moves inside that
row" has to be believable before a loop can be rewritten (the same logic as
chapter 38's real case).

#realcase("The other direction is sound — allocate flat, view as 2-D")[
  The practical answer is to reverse the direction. Instead of *sweeping a
  declared 2-D array flat*, *view flat memory as two-dimensional*.

  ```c
  double *m = malloc(rows * cols * sizeof *m);
  double (*view)[cols] = (double (*)[cols])m;   /* a 2-D view */
  view[i][j] = ...;
  ```

  This direction is sound for a reason. Memory from `malloc` has *no declared
  type*, and the standard settles the effective type of such an object as "the
  type of the lvalue used to store into it" (§6.5). Writing through a 2-D shape
  makes that shape the effective type. And the range for the arithmetic is *the
  whole allocated block*, so no sweep leaves its provenance.

  Hence the working rule: *if you want to handle it flat, allocate it flat.* If
  you really must pass a declared 2-D array around flat, handle it row by row or
  move it with `memcpy`.
]

== The patterns of practice

There are four ways multidimensional data is handled. Here they are with where
each is actually used.

#dtable(
  columns: 4,
  [*Pattern*], [*Shape*], [*Layout*], [*Where it is seen*],
  [Fixed-width 2-D], [`int a[R][C]`, `int (*)[C]`], [One run], [Frame buffers, game boards, embedded tables],
  [VLA parameter], [`a[rows][cols]`], [One run], [Numeric code; the default since C99],
  [Flat + leading dimension], [`a[i * lda + j]`], [One run], [BLAS, LAPACK, submatrices],
  [Array of row pointers], [`int *rows[R]`, `int **`], [Rows apart], [`argv`, image libraries, jagged rows],
)

=== Flat plus a leading dimension — the lingua franca of numeric libraries

#demo("examples-en/ch39/md_flat.c")

This is the most widely used pattern in numerical computing. Its heart is
*separating the column count from the row stride*.

- The *column count* (`cols`) is how many columns this matrix actually uses.
- The *leading dimension* (`lda` in BLAS) is the distance from one row to the
  next.

The power comes from those two not having to be equal. Keep `lda` as it is and
move only the starting point, and *a submatrix appears with no copying*. In the
demonstration `sub` points at the original's (1,1) with the stride still 5 — so
writing through `sub` changes the original. It is a view.

This design is also why the BLAS and LAPACK APIs have survived nearly half a
century. One matrix-multiply function can take submatrices, transposes and padded
buffers because the shape of the data was reduced to two numbers: *a starting
address and a stride*.

#qa[
  Is the submatrix the only gain from keeping a separate stride?
][
  Three more.

  *Alignment becomes possible.* SIMD instructions want each row to start on a
  16-, 32- or 64-byte boundary. When the column count is awkward, padding is put
  at the end of each row and `lda` is grown to match — the logical shape stays,
  only the physical stride changes.

  *Transposition becomes free.* Only the reading order changes (the last block of
  the demonstration), so no transposed copy need be built. That is exactly what
  BLAS's `trans` flag does.

  *Generalised, it becomes strides.* Keep a stride *per axis* rather than just for
  rows, and transposition, sub-views and reversed views all become stride
  manipulation. NumPy's `strides` and OpenCV's `Mat::step` are that general form,
  and in C's neighbour C++ the same idea has been fixed into a type as
  `std::mdspan`.
]

=== An array of row pointers — binding scattered rows into one

`int *rows[R]` is a wholly different layout. The rows may sit anywhere in memory,
and they may have different lengths.

The latter part of the demonstration shows both properties. *Swapping rows
becomes swapping pointers*, so order can change without moving data (which earns
its keep in sorting and pivoting), and *jagged rows* can be held at all.

Three representative cases:

- `int main(int argc, char *argv[])` — an array of strings of differing lengths.
  Chapter 28's `argv` is exactly this pattern.
- *Image libraries* — libjpeg's `JSAMPARRAY` is an array of row pointers, because
  an API that processes an image one scanline at a time must be able to swap the
  buffer for each line.
- *Ragged data* — text whose sentences differ in length, graphs whose nodes have
  different numbers of children.

The price is plain: one more indirection, broken locality when rows scatter
(chapter 11), and as many allocations and frees as there are rows.

== Traversal order and the cache — why the same computation differs several-fold

The same data, read the same number of times, differs in speed by the order. This
is the most practical piece of knowledge about multidimensional arrays.

#demo("examples-en/ch39/md_stride.c")

The demonstration does not measure time (that differs per machine). It counts
*access strides* instead. Reading a 64×64 `double` matrix 4096 times in two
orders touches the same 512 cache lines. What differs is *continuity* —
row-major settles 3584 accesses inside the line already loaded, and the column
order manages none at all.

The reason is chapter 11's ladder exactly. A cache carries memory in *lines*
(usually 64 bytes), not bytes. For `double` that is eight per line.

- *Row-major (`for i { for j { a[i][j] } }`)* — the inner step is 8 bytes. Eight
  uses come out of one loaded line before moving on. And because the addresses
  rise regularly, the *hardware prefetcher* fetches the next line ahead of time.
- *Column order (`for j { for i { a[i][j] } }`)* — the inner step is the size of a
  row (512 bytes here). Every access touches a different line, and the other
  seven values in each loaded line are evicted unused.

Once the matrix outgrows the cache the gap widens visibly. Figures of *several
times to more than tenfold* are commonly quoted; the exact number depends on
machine and size, so this book pins down no figure. What to remember is the
principle: *make the inner loop sweep memory continuously.*

#realcase("In matrix multiply, changing the loop order alone")[
  The classic case is `C = A × B`. Written as the textbook does, the inner loop is
  `k`, and inside it `B[k][j]` is read *down a column* — the worst possible order.

  ```c
  for (i) for (j) for (k) C[i][j] += A[i][k] * B[k][j];   /* ijk */
  for (i) for (k) for (j) C[i][j] += A[i][k] * B[k][j];   /* ikj */
  ```

  The second (`ikj`) does exactly the same arithmetic, but in its inner loop both
  `B[k][j]` and `C[i][j]` run along rows. That alone makes large matrices several
  times faster.

  Production numeric libraries go a step further — they cut the matrices into
  *tiles* that fit in cache, so that a loaded piece is used as many times as
  possible before it goes. A good part of why BLAS implementations beat a plain
  triple loop by more than tenfold lies in that tiling and in SIMD. Not the
  algorithm but *the order in which memory is handled* decided the performance.
]

#platform("Background — the notion of an iterator")[
  Put in other languages' vocabulary, all of this is the *iterator*. "How shall
  this structure be swept?" separated from the structure itself and made into a
  value. C has no such name, but it has the thing — *a pointer is an iterator*.

  ```c
  for (int *p = a[0]; p != a[0] + 12; ++p) ...   /* start, end, advance */
  ```

  The idea that a sweep needs only a start, an end and a way to advance is the
  same. C++'s `begin()`/`end()`, Python's `__iter__` and Java's `Iterator` are
  those three given names and a specification.

  In more than one dimension the notion is especially useful. The *leading
  dimension* and *strides* above are precisely "the way to advance" written down
  per axis, and NumPy's `nditer` or C++'s `mdspan` make that rule a value to be
  passed around. In C there is no such value, so you carry the two numbers
  `(start address, stride)` by hand — the same pattern, only without the name.
]

#recap[
  #dtable(
    columns: 2,
    [*What to keep*], [*The point*],
    [Substance], [`int a[3][4]` is "three `int[4]`" — the element is a row],
    [Layout], [Row-major. The last subscript varies fastest],
    [Subscript], [`a[i][j]` = `*(*(a+i)+j)`; offset = `i×row + j×element`],
    [Parameters], [Only the outer dimension is stripped — `int (*)[4]`, not `int **`],
    [Flattening], [Sweeping a declared 2-D array flat is a grey zone (provenance)],
    [The other way], [Allocating flat and putting a 2-D view on it is the sound direction],
    [Leading dimension], [Start plus stride — submatrix, transpose and alignment for free],
    [Traversal], [Make the inner loop sweep memory continuously],
  )
]

We have seen what happens when arrays are stacked. The next chapter takes the
array that is used most and hurts most — the array of characters, the string.
