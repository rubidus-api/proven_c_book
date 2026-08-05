#import "../../book/lib.typ": *

= The five bugs shipped for fifty years

#prereq(
  ([chapter 45, Errors and contracts], [errors and contracts]),
  ([chapter 37, Strings], [the danger of strings]),
  ([chapter 53, The terrain of the standard library], [the traps of the standard library]),
)

#deepqa[
  Chapter 37 said the functions handling strings "do not know the size of the
  vessel", and chapter 45 said "a failure not confirmed becomes a thing that never
  happened". Then why do such problems still remain — is half a century not more
  than enough time to mend them?
][
  Not because they cannot be mended but because mending them *breaks all the code
  already written*. The moment one more size parameter is put into `strcpy`'s
  signature, every C program in the world stops compiling. The standard is an
  institution that must protect existing code (chapter 53's reason for "thin and
  old" appears here too), so instead of removing dangerous functions it has taken the
  road of *placing better functions beside them*. So the choice comes over to the
  programmer — it is, in effect, a language in which knowing what is dangerous and
  choosing accordingly is itself skill.
]

#organizer[
  This part's statement of the problem. We confirm with actually running code why C
  has kept shipping the same five classes of bug for half a century — that it is not
  the programmer's carelessness but *the shape of the API*. Only after all five have
  been seen does the name proven appear again. Not introducing the tool first is
  this part's principle.
]

#qa[
  Then are these five a list of mistakes beginners make?
][
  No. These are mistakes *the skilled keep making too*, and that point matters. If
  people who know the whole grammar still slip in the same places, the cause is not
  the person but *the shape of the tool*. A function that does not take a size has
  no way of checking a size, and a function whose return value may be thrown away
  with nothing happening will one day be thrown away. This chapter takes that
  "shape" apart one at a time.
]

#chapter-questions()

== One — string functions do not know the size of the vessel

The oldest and most exploited class. Exactly as seen in chapter 37.

```c
char buf[64];
strcpy(buf, name);            /* how long is name? strcpy does not ask */
strcat(buf, ", welcome!");    /* and how much room is left now? */
```

`strcpy`'s signature has no destination size. What is not there cannot be checked,
so this function will happily write to the 200th byte of a 64-byte vessel. What gets
wrecked depends on what the compiler placed after it, and commonly that is the
function's return address (recall chapter 39's picture of the stack).

The modern prescription is to use the editions that take a size — `snprintf` is
representative. And here is a second trap. Functions that take a size *quietly
truncate* when it overflows.

#demo("examples-en/ch69/truncate.c")

Look at the third line. What was to be made was
`/var/log/service/http/access.log`, and what remained in hand is
`/var/log/service/http/a`. The program neither stopped nor warned. If this string is
a file path it opens the wrong file, if a command it becomes a different command, if
a log the record of an accident is cut without a sound. *A truncated path is not a
short path but a wrong path.*

#antipattern[
  Treating truncation as success
][
  ```c
  snprintf(path, sizeof path, "%s/%s", dir, name);
  open_file(path);          /* nobody asked whether it was truncated */
  ```
  `snprintf` in fact gives the answer — it returns *the length that would have been
  needed*. If that value is at least the vessel's size it was truncated (the
  example's last line is that check). The problem is that this check is *optional*.
  Throw the return value away and the compiler says nothing. That leads straight
  into the second bug.
]

#realcase[
  The compiler catches only what it can see
][
  Something that really happened while making this example. At first `snprintf` was
  called directly inside `main` with literal arguments, and gcc caught it.

  ```text
  error: ‘%s’ directive output truncated writing 10 bytes
         into a region of size 2 [-Werror=format-truncation=]
  note: ‘snprintf’ output 33 bytes into a destination of size 24
  ```

  An excellent diagnosis. Yet moving the same call inside a function called
  `build_path` made the warning *vanish*. The moment a function boundary is crossed
  the compiler cannot know the real lengths of `dir` and `name`. In a real program
  strings come from files or from the network, so cases where the compiler can help
  are rather rare. A warning is a free review, not a guarantee (chapter 17).
]

== Two — there is no device that makes you confirm failure

```c
char *p = malloc(n);
p[0] = 'x';                   /* malloc gives null on failure */
```

C's ways of reporting failure are two. Return a *sentinel value* (null, `-1`, `EOF`),
or leave the reason in the global variable `errno`. Neither can compel a check. Code
that throws the return value away is perfectly legal, and `errno` is global state
that must be read at exactly the right moment, before the next call overwrites it
(chapter 53).

#demo("examples-en/ch69/unchecked.c")

That `careless typo` returned 8080 is this section's heart. There was a typo in the
configuration and the program *quietly fell back to the default*. On the surface
nothing happened, and months later only the question "why is the setting not taking
effect?" remains. That `strtol("abc")` gives 0 is the same pattern — failure and "a
real 0" come back as the same value.

#misconception[
  "Failure is exceptional, so it can be handled later"
][
  The premise that failure is rare is wrong to begin with. A file may not exist,
  input carries typos, disks fill, networks break — every place where the program
  touches the outside world is a point of failure. And the real reason "later" is
  dangerous lies elsewhere. Code that ignored a failure *does not stop but keeps
  running*. A wrong value flows into the next calculation, into the function after
  that, and by the time the problem finally shows itself it blows up far from its
  cause. Chapter 45's "fail early" returns here.
]

== Three — `printf` believes exactly what you tell it

Chapter 53 took the grammar of the format string apart. That grammar has one
structural weakness — *the type is written twice*. Once in the format (`%d`) and
once in the argument (the variable's type). If the two go out of step the language
cannot prevent it, because as seen in chapter 50 type information does not ride
along into variadic arguments.

Today's compilers catch this. The real message is like this.

```text
warning: format ‘%d’ expects argument of type ‘int’,
         but argument 2 has type ‘double’ [-Wformat=]
    3 |     printf("%d\n", 3.0);
      |             ~^     ~~~
      |              |     |
      |              int   double
```

But only this far. The moment the format becomes a *variable* — the moment a
multilingual message is taken from a table or a log format is received from a
configuration — the compiler has nothing left to look at.

#antipattern[
  Code that takes the format as a variable
][
  ```c
  const char *fmt = load_message("greeting");   /* a format taken from a table */
  printf(fmt, count);                           /* no warning. no check either */
  ```
  Not a single warning comes from this code. Because a way of knowing whether format
  and arguments match does not exist at compile time. Worst is when the format is
  *user input*, which becomes the format string vulnerability seen in chapter 53.
]

== Four — who frees this

```c
char *s = build_message();    /* must this be freed? the type says nothing */
```

As learned in chapter 40, dynamically taken memory must be released by somebody
exactly once. Yet a `char *` a function returned may be any of four things.

- Just allocated — it must be freed.
- Pointing at a buffer the caller gave — it must not be freed.
- A string literal in a read-only place — freeing it is an accident.
- A static buffer the next call will overwrite — it must neither be freed nor held
  for long (chapter 53's `strtok` was such).

The types of the four cases are *all the same*. The answer is in the documentation,
and documentation goes out of step with code as a matter of course. Here arise
chapter 40's three accidents — a leak (nobody frees), a double free (both free), and
use after free (somebody still points at it after freeing).

#antipattern[
  An API whose type does not state ownership
][
  ```c
  const char *lookup(int code);        /* a literal? an allocation? a static buffer? */
  char       *format_time(time_t t);   /* must this be freed? */
  ```
  It cannot be known from the name and type alone. *Every place* that uses this API
  must remember the documentation, and if even one forgets it becomes one of the
  three accidents above. That the discipline is entrusted to human memory is the
  essence of the problem.
]

== Five — a callback nobody can type-check for you

```c
qsort(a, n, sizeof *a, cmp);   /* cmp takes const void* */
```

`qsort` takes a comparison function through a `void *` interface in order to sort any
type. In a language with no generics this is nearly the only way, but the price is
*the complete abandonment of type checking*. Whatever you cast to inside the
comparator, the compiler believes you.

#demo("examples-en/ch69/cmp_bad.c")

The `first-char` comparator's types match perfectly, it compiles without a single
warning, and it does not die. It is only that `peach` and `pear` are in the wrong
order — seeing only the first letter, the two were judged "equal" and the rest was
left to chance. This class of bug is found last of all, because it gives *a quietly
wrong answer*.

#realcase[
  Attacks aiming at a data structure's worst case
][
  There is a performance trap in the same place. Widely used sorting and hashing
  implementations are fast on average but slow down sharply on particular inputs, and
  the technique of an attacker deliberately making such inputs to paralyse a server
  (an algorithmic complexity attack) was organised in a 2003 paper and used in real
  attacks. Attacks of the same family aiming at hash collisions brought down several
  web frameworks at once in 2011. They were events showing that a data structure's
  *worst case* is itself a security problem, and so today's libraries take as their
  defaults sorting with a guarantee even in the worst case (introsort) and hashes
  using a random seed — we see them in the flesh in chapter 76.
]

== And a sixth — bytes have types

We said five, but one more must be added to be fair. It is the strict aliasing seen
in chapter 13. A hand-written parser that peers into a byte buffer through pointers
of different widths runs perfectly at `-O0` and quietly gives a different answer at
`-O2`. It is the representative of the "bug that appears only in release" seen in
chapter 17, and the clause to which the Linux kernel surrendered with a single flag.

Gathering the six into one table gives this part's map.

#recap[
  #dtable(
  columns: 3,
    [*problem*], [*what C gives*], [*what is needed*],
    [buffer overflow and truncation], [string functions that do not know the size], [strings that carry their length, writes that do not truncate],
    [unconfirmed failure], [sentinel values and `errno`], [errors that come as values, a compile refusal if discarded],
    [format mismatch], [a `printf` that believes the format string], [placeholders that take the type from the argument],
    [unclear ownership], [a `char *` that means four things], [different types for owning and borrowing],
    [unchecked callbacks], [the `void *` interface], [documented contracts and worst-case-guaranteed algorithms],
    [the hidden type of bytes], [UB on breaking the aliasing rule], [a byte type the rule exempts],
)
]

#qa[
  Would it not be better to use another language entirely to avoid such problems?
][
  That too is an answer, and many places really went that road (chapter 1). But the
  places where C must be used still remain — operating systems, firmware, the floor
  layer other languages lean on, and projects where decades of code have already
  piled up. What can be done in such places is *not to change the language but to
  change the shape of the API*. The right-hand column of the table above is not a
  list of items requiring a new language but things that can be made by design within
  C. From the next chapter we see that design.
]

The library that implements that right-hand column as it stands is the proven this
book has leaned on. We first met it in chapter 38 and its name has come up a few
times since, but treating it head on begins now. The next chapter is installation and
a first program — and why this library has the shape of "nothing to install".
