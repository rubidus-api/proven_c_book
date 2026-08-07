#import "../../book/lib.typ": *

= How to read a declaration — two readings and `typedef`

#prereq(
  ([chapter 56, Functions as values], [the notation of a function pointer]),
  ([chapter 37, Arrays], [the array declarator]),
)

#deepqa[
  While learning function pointers in chapter 56 a declaration like
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

#organizer[
#idx("reading declarations")  C's most notorious place — we learn how to read a
  declaration such as `char *(*table[4])(int)`. There are only two principles:
  reading *from the outside in*, and reading *from the innermost name outward*.
  That the two have different uses, and how to unfold this rough declaration in
#idx("typedef")  layers with `typedef`. Finally we introduce a tool that does the
  job for you.
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

#demo("examples-en/ch57/decl.c")

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
  specifier*. Grammatically `typedef int count;` is placed where `static int count;` or `extern int count;` would be. So read it like this — *"in the place
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
  error (thanks to chapter 43's property that a struct is a value). A device like
  C++'s strong type aliases does not exist in C.
]

== The real thing — declarations that actually shipped

What follows is not exercise material. These are *declarations from libraries
that really shipped*, read by the procedure. The point of the section is to
confirm there is nothing to be afraid of.

#demo("examples-en/ch57/monster.c")

=== ① The monster the standard itself produced — `signal`

This declaration stands in the standard's `<signal.h>` exactly as written
(§7.14.1.1).

```c
void (*signal(int sig, void (*func)(int)))(int);
```

At first sight it is a forest of parentheses, but following the procedure of the
previous section it takes five steps. *Start at the name, look right first, and
go left when the right is exhausted.*

#dtable(
  columns: 3,
  [*Step*], [*What is in view*], [*The sentence so far*],
  [1], [`signal`], [signal is],
  [2], [to the right: `(int sig, void (*func)(int))`], [… a *function* taking an `int` and "a pointer to a function taking `int` and returning `void`"],
  [3], [to the left: `*`], [… returning *a pointer*],
  [4], [outside the parentheses, right: `(int)`], [… that pointer points at *a function* taking `int`],
  [5], [leftmost: `void`], [… returning `void`],
)

In one sentence: *"`signal` takes a signal number and a handler, and returns
**the previous handler**."* That also explains why the return type is so rough —
installing must hand back the previous one so it can be restored later. The first
line of the demonstration actually takes that previous handler and restores it.

One `typedef` makes the declaration ordinary.

```c
typedef void handler_t(int);          /* name the function type */
handler_t *signal(int sig, handler_t *func);
```

The standard does not write it that way for historical reasons — this function
existed long before layering with `typedef` became the habit.

=== ② Rougher in the wild — X11's error handler

From the X Window System's manual:

```c
int (*XSetErrorHandler(int (*handler)(Display *, XErrorEvent *)))();
```

The pattern is *identical* to `signal`: take a handler, return the previous one.
The only difference is that the handler takes two arguments, so a layer of
parentheses looks thicker. By the procedure, the number of steps is the same.

The second block of the demonstration transplants the pattern and runs it — a raw
`set_error_handler` and a `set_error_handler2` layered with `typedef` do the same
work. The latter shows that this rough declaration is really the one line *"a
function taking a handler and returning a handler."*

```c
typedef int error_handler_t(Display *, XErrorEvent *);
error_handler_t *XSetErrorHandler(error_handler_t *handler);
```

#realcase("The same pattern is everywhere")[
  Once recognised, "take a handler and return the previous handler" shows up all
  over: the standard's `signal`, X11's `XSetErrorHandler` and
  `XSetIOErrorHandler`, and most callback-registration functions in GUI and game
  frameworks. The reason is the same in each — *it must be possible to restore*.
  Code that plugs a library in has no way to put things back afterwards unless
  installation hands back what was there.

  For the same reason `qsort` and `bsearch` take a comparison function
  (`int (*compar)(const void *, const void *)`), and POSIX's `pthread_create`
  takes a start routine (`void *(*)(void *)`). Most rough-looking declarations
  come from one idea: *passing behaviour as a value.*
]

=== ③ And the genuinely pointless ones

Declaration quizzes on the internet have their regulars.

```c
char *(*(**foo[][8])())[];      /* an example from cdecl's own documentation */
int (*(*bar[10])(void))(int);
```

The procedure works on these too. The first is *"an array of arrays of 8 of
pointer to pointer to function returning pointer to array of pointer to `char`"*.
More important than having read it is the judgement that follows: *do not put
such a declaration in your code.*

The difference between these and the two above is this chapter's point. The first
two are declarations *worth untangling* — they are really used and the pattern
carries meaning. The last has no meaning; it only shows what the grammar permits.

#qa[
  Then why practise reading such declarations at all?
][
  Three practical reasons.

  *First, you do not get to choose other people's code.* Standard headers, old
  libraries and kernel structures carry these declarations as they are. Unable to
  read one, you cannot use the function.

  *Second, you have to read error messages.* When a function-pointer type does not
  match, the compiler prints types like `int (*)(Display *, XErrorEvent *)`
  verbatim. Knowing the procedure turns that message into a sentence.

  *Third, deciding what to wrap in a `typedef` requires reading it first.* Give a
  name to something you have not understood and the name will lie.

  But the purpose is *reading*. For writing, always divide into layers with
  `typedef` — the tool in the next section helps with that judgement; it does not
  replace it.
]

#misconception[
  "Only geniuses read complicated declarations"
][
  Not so, because there is a procedure. As the table above shows, `signal` is five
  steps and X11's is five steps. The number of steps is set by *how many layers of
  parentheses there are*, not by anyone's talent.

  The real reasons it feels hard are two: *scanning with the eye instead of
  following the procedure*, and *trying to grasp the whole meaning at once*. A
  machine does not read that way — it peels one layer at a time and appends what
  it peeled to a sentence. Do the same and mistakes almost stop happening,
  especially with the steps written down on paper.
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
installed as a package on Linux distributions (`cdecl`), and there is #link("https://cdecl.org")[`cdecl.org`]
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
    [tool], [`cdecl` (explain/declare), #link("https://cdecl.org")[`cdecl.org`]],
  )
]

We have gained the muscle for reading declarations. From the next chapter we enter
the terrain of the standard library — the part where, on top of the language
learned so far, we see what contracts and traps the functions the world has piled
up over half a century carry.
