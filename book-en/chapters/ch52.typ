#import "../../book/lib.typ": *

= How to read a declaration — two readings and `typedef`

#prereq(
  ([chapter 51, Functions as values], [the notation of a function pointer]),
  ([chapter 36, Arrays], [the array declarator]),
)

#organizer[
#idx("reading declarations")  C's most notorious place — we learn how to read a
  declaration such as `char *(*table[4])(int)`. There are only two principles:
  reading *from the outside in*, and reading *from the innermost name outward*.
  That the two have different uses, and how to unfold this rough declaration in
#idx("typedef")  layers with `typedef`. Finally we introduce a tool that does the
  job for you.
]

#deepqa[
  While learning function pointers in chapter 51 a declaration like
  `int (*(*s)(void))(int);` appeared, and we passed on saying only "it is hard to
  read, so use a `typedef`". But by what rule are such declarations built — why do
  they twist like this?
][
  Because a C declaration is not *writing down a type* but *writing down how that
  name is used*. Dennis Ritchie's design principle was that declaration reflects
  use. `int *p;` reads "*`*p` is an int*", and `int a[3];` reads "`a[3]` is an
  int". This principle is elegant in the simple cases, but once `*` and `[]` and
  `()` overlap, parentheses intrude because of precedence and it quickly turns
  rough. Hence the separate need for *rules for reading*.
]

#chapter-questions()

== The two readings

One rule suffices to begin. The three symbols attaching beside the name in a
declaration have different strengths.

- `[]` (array) and `()` (function) attach *to the right of the name*, and `*`
  (pointer) attaches *to the left*.
- The two on the right are *stronger* than the one on the left. So `int *a[3]` is
  "an array of pointers", not "a pointer to an array".
- To reverse the order, bind with parentheses — `int (*a)[3]`.

On this difference of strength the two readings arise.

*Reading ① — from the innermost name outward (the right-left rule).* Find the
identifier, start there, and go outward alternately, *looking right first and then
left*. On meeting a parenthesis, read all of its inside and then step out. The
charm of this method is that what you say as you read connects in English word
order.

*Reading ② — from the outside in.* Start from the type name (the leftmost thing,
such as `int` or `char`) and wrap your way in asking "this is a what of a what".
In a declaration with no identifier at all — the *abstract declarator* written in
a cast or in `sizeof` — there is no name to start from, so this is the only road.

The two readings arrive at the same conclusion. Use whichever suits the occasion.

== Reading ① in practice — connecting it in English word order

#demo("examples-en/ch52/decl.c")

We read the example's declarations one at a time. What you say is most natural
strung together in English — because C's declaration syntax was designed to follow
English word order.

*`int *pa[3];`*

+ Start from the identifier `pa` — "pa is"
+ Look right: `[3]` — "array 3 of"
+ The right is finished, so look left: `*` — "pointer to"
+ What remains: `int` — "int"

Read through: *"pa is array 3 of pointer to int"* — an array of three pointers.
The example confirmed it with 24 bytes in total and 8 bytes per element.

*`int (*ap)[3];`*

+ Start from `ap` — "ap is"
+ To the right is the end of the parenthesis, so we cannot go. Look left: `*` —
  "pointer to"
+ Step out of the parenthesis. Right again: `[3]` — "array 3 of"
+ What remains: `int`

*"ap is pointer to array 3 of int"* — one pointer (8 bytes), and what it points at
is a 12-byte array. That is the example's second line.

*`char *(*table[4])(int);`* — this chapter's protagonist.

+ Start from `table` — "table is"
+ Right: `[4]` — "array 4 of"
+ Left: `*` — "pointer to"
+ Outside the parenthesis, right: `(int)` — "function (int) returning"
+ Left: `*` — "pointer to"
+ What remains: `char`

*"table is array 4 of pointer to function (int) returning pointer to char"* — a
table of functions taking one integer and giving back a string. The example
actually put two functions in and called them.

#qa[
  I have heard of a "clockwise spiral rule" too — is it the same thing?
][
  It is the same thing drawn to be easy to memorise, and it mostly works. But there
  has long been the objection that the spiral rule, being *a summary of the
  right-left rule*, is drawn confusingly for some forms. Written exactly, the rule
  is this — *start from the identifier, read right as far as you can go, and when
  you can go no further read left. Parentheses are the boundary.* Use the spiral
  picture as a visual nickname for that sentence.
]

== Reading ② — when there is no name

Casts and `sizeof` take a type written without a name. The last line of the
example, `sizeof(char *(*)(int))`, is that. Such a form is called an *abstract
declarator*, and the way to read it is simple — *find the place where the name
ought to be, lay a name there, and read by reading ①.*

```c
char *(*)(int)        /* there is no name */
char *(*x)(int)       /* put x in the empty place and */
                      /* "x is pointer to function (int) returning pointer to char" */
```

The empty place is usually after the star of `(*)`, or before `[]` or `()`. Thanks
to this knack, reading ①'s muscle can be used as it is.

*Then when is reading ② used.* When the shells are several layers deep — as in a
declaration taking a function pointer as an argument — and you want to know first
which is the outermost.

```c
void qsort(void *base, size_t n, size_t size,
           int (*cmp)(const void *, const void *));
```

Reading from the outside in, the skeleton is grasped first: "this is a function
`qsort`, returning `void`, with four arguments of which the last is *a function
pointer*." The details are then confirmed with reading ①. When reading somebody
else's header in the field this order is the comfortable one.

== `typedef` — dividing into layers and naming them

The most practical answer to a rough declaration is *dividing it into layers by
naming them*. The example's ④ is that demonstration.

```c
typedef char       *charptr;        /* pointer to char           */
typedef charptr     handler(int);   /* function(int) returning charptr */
typedef handler    *handler_ptr;    /* pointer to that function  */
typedef handler_ptr table4[4];      /* array[4] of that pointer  */
```

Each of the four lines lays on just one layer. The final `table4` is *exactly the
same type* as the example ③'s `char *(*table[4])(int)`, and the example's
`static_assert` confirms it at compile time. In the field it is more common to
name only one or two layers than to slice this finely.

```c
typedef char *(*handler_fn)(int);   /* when one layer is enough */
handler_fn table[4];
```

#qa[
  `typedef`'s syntax is odd — why is it not `typedef newname = type`?
][
  Because `typedef` is a word that comes in the position of a *storage-class
  specifier*. Grammatically `typedef int count;` is placed where `static int
  count;` or `extern int count;` would be. So read it like this — *"in the place
  where this declaration would create a variable, a type name is created
  instead."*

  This understanding pays in practice. Seeing `typedef int arr[3];` unfolds as
  "had there been no `typedef`, `arr` would have been an array variable of three
  ints → therefore `arr` is the name of that type." The declaration reading is
  reused as it is.
]

#antipattern[
  Hiding a pointer behind a `typedef`
][
  ```c
  typedef struct node *node;      /* that it is a pointer vanishes from the name */
  void f(const node n);           /* what is the const attaching to? */
  ```
  `const node n` is not "what is pointed at is const" but *"the pointer itself is
  const"* (`struct node *const`). It cannot be known from the name alone, so it is
  commonly got wrong.

  Practice splits in two. *Do not hide pointers* (recommended) — leave the type
  name as `struct node` and write `struct node *`. Or if you do hide it, *mark it
  in the name* — as in `node_ptr`. The Linux kernel's coding conventions pinned
  down "do not use pointer `typedef`s" for the same reason.

  There is an exception, though. When making an *opaque type* you hide it on
  purpose — in a handle whose innards the user must not know (the position of
  `FILE *`, say), hiding is the design. Just distinguish hiding from losing by
  accident.
]

#misconception[
  "`typedef` makes a new type"
][
  It does not. `typedef` makes only an *alias*, treated as entirely the same as the
  original type.

  ```c
  typedef int meters;
  typedef int seconds;
  meters  d = 10;
  seconds t = 5;
  d = t;              /* no warning — both are just int */
  ```

  If you hoped the compiler would catch the mistake of "assigning seconds to
  metres", you will be disappointed. To distinguish types for real you must wrap
  them in a struct — `struct meters { int v; };` and then the assignment becomes an
  error (thanks to chapter 41's property that a struct is a value). A device like
  C++'s strong type aliases does not exist in C.
]

== Leaving it to a tool — `cdecl`

A program that does this reading for you has existed for a long time. It is
*`cdecl`*, which appeared in the 1980s and is still maintained (its current
maintainer is Paul J. Lucas, GPLv3). Give it a declaration and it unfolds it into
English; speak English and it builds the declaration.

```text
cdecl> explain char *(*table[4])(int)
cdecl> declare table as array 4 of pointer to function (int) returning pointer to char
```

The first line does what we did by hand above, and the second is the opposite
direction — *it builds a C declaration from what was said in English*. It can be
installed as a package on Linux distributions (`cdecl`), and there is `cdecl.org`
for using it in a browser without installing.

#qa[
  If a tool exists, must one bother learning to read by hand?
][
  It is worth learning for two reasons. First, *reading happens constantly while
  opening a tool is occasional.* You do not open a browser every time you meet an
  `int (*p)[3]` while skimming somebody's code. Second, there is *the writing
  side*. A tool reads for you, but "what declaration should be written in this
  place" is settled in the end by whoever knows the rules — and the usual right
  answer is this chapter's conclusion: *do not write it roughly in one line;
  divide it into layers with `typedef`.*
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [design principle], [a declaration reflects *use* — hence the twisting],
    [difference of strength], [`[]`, `()` (right) are stronger than `*` (left). parentheses reverse it],
    [reading ①], [start from the identifier → right first, then left → in English word order],
    [reading ②], [from the outside in. essential for an abstract declarator with *no name*],
    [abstract declarator], [lay a name in the empty place and read by reading ①],
    [`typedef`], [a word in the storage-class position — a *type name* is created instead of a variable],
    [`typedef`'s limit], [an alias only, not a new type. to distinguish, use a struct],
    [hiding pointers], [not recommended (the meaning of `const` blurs). the exception is an opaque type],
    [tool], [`cdecl` (explain/declare), `cdecl.org`],
  )
]

We have gained the muscle for reading declarations. From the next chapter we enter
the terrain of the standard library — the part where, on top of the language
learned so far, we see what contracts and traps the functions the world has piled
up over half a century carry.
