#import "../../book/lib.typ": *

= The traps of reading and writing — `<stdio.h>` ②

#prereq(
  ([chapter 58, Streams in reality], [the state of a stream]),
  ([chapter 40, Safe input, and the appearance of proven], [safe input]),
)

#deepqa[
  Chapter 40 said `gets` was deleted from the standard, and chapter 56 said its
  funeral took decades. Then what exactly was wrong with `gets`, and why did it
  take so long?
][
  The problem is simple — *it does not take the size of the destination buffer.*
  Its signature is only `char *gets(char *s)`, so there is no way at all to know
  how much may be written, and if the input is long it necessarily overflows. It is
  one of the rare functions for which "use it carefully and it is fine" does not
  hold — a safe way to use it does not exist.

  It took long because of the standard's character (chapter 57). Not breaking code
  already written is the standard's duty, so C99 marked it "do not use"
  (deprecated) and only C11 deleted it. For over twenty years in between, every
  compiler spat warnings and yet compiled it.
]

#organizer[
  We begin with the story of the only function ever *deleted* from the standard
  library. Why `gets` died, what stands in its place, and what traps that
  replacement carries in its turn. Then the remaining dangers of formatted input
  and output, and how safe the functions known as safe really are.
]

#chapter-questions()

== The death of `gets` and its successors

The hole the 1988 internet worm bored into was exactly this function (chapter 40).
Today `gets` is not in the standard, and three remain in its place.

#dtable(
  columns: 3,
  [*function*], [*status*], [*assessment*],
  [`gets`], [deleted in C11], [there is no way to use it. mend it wherever it is seen in old code],
  [`fgets`], [standard], [the realistic standard solution. but truncation must be checked by hand],
  [`gets_s`], [C11 annex K (optional)], [implementations barely exist — treated in chapter 68],
)

`fgets` is the right answer, but it is not the end in itself. As seen in
chapter 58, if the buffer is too small it quietly reads only the front piece.

#demo("examples-en/ch59/reading.c")

The difference between the two approaches is clear. The fixed buffer split the
long line into five pieces, while the edition that reads while growing returned
the 34-byte line whole.

The rules for reading code come to three in the end.

+ Turn the loop on `fgets`'s return value (whether it is null).
+ Check whether the line read has a newline and judge *whether it was truncated*.
+ Erase the newline with `buf[strcspn(buf, "\n")] = '\0';` — this idiom is safer
  than hand-written code based on `strlen`.

#antipattern[
  `scanf("%s", buf)` — reading a string without a width
][
  ```c
  char name[32];
  scanf("%s", name);        /* exactly the same danger as gets */
  ```
  Give `%s` no width and it writes without knowing the destination's size. Always
  write *a number one less than the buffer size*, as in `scanf("%31s", name)` (for
  the NUL). And that this number must be written by hand is this API's weakness —
  change the buffer size and the format string must be mended with it, which is
  easy to forget.
]

== The remaining traps of formatted input

Chapter 56 took the grammar apart, so here we gather only the places where
accidents happen.

*First, not checking the return value.* The `scanf` family returns the number of
items filled. Without checking it you end up using the *previous value* of the
variable that failed (we see it in the flesh in chapter 74).

*Second, leftover input.* After `scanf("%d", &n)` a newline remains in the input
buffer. Call `fgets` in that state and it reads an empty line. Not mixing them is
the best policy, and if you must mix, empty the rest of the line and move on.

*Third, integer overflow.* Give `99999999999` to a `%d` and it is outside the
contract. If the range must be checked, read with `strtol` (chapter 61).

*Fourth, `%s` and the locale.* The definition of whitespace may change with the
locale (chapter 62).

#misconception[
  "`snprintf` instead of `sprintf` is safe"
][
  Half right. `snprintf` is safe in that it does not overrun the buffer, but it
  brings in the new danger of *quietly truncating*. And the meaning of its return
  value is peculiar — it is not the number of characters written but *the number of
  characters that would have been needed*.

  ```c
  int need = snprintf(buf, sizeof buf, "%s/%s", dir, name);
  if (need < 0 || (size_t)need >= sizeof buf) {
      /* truncated — this path must not be used as it is */
  }
  ```

  Leave this check out and it becomes "I used the safe function and opened the
  wrong file." We run this pattern for real in chapter 74.
]

== The traps on the output side

*`printf`'s return value* — code that checks it is rare, but it fails in the
situation of a broken pipe (`program | head`, say). For a program that keeps logs
there is a value worth checking.

*The format string vulnerability* — the one treated in chapter 56. The rule is
one. The format string must always be a constant written by the program.

*`%n`* — the conversion that *writes* the number of characters printed to where
the argument points. Being the passage that promotes a format string vulnerability
into an arbitrary memory write, several implementations block it by default today.
There is almost no reason to use it.

*Buffering and order* — mix `printf` (standard output, usually line-buffered) with
`fprintf(stderr, ...)` (mostly unbuffered) and the order in which things appear on the
screen can be reversed. Half of the occasions on which "the output vanished"
during debugging are this, and the other half are cases where the buffer was not
emptied just before a collapse.

#realcase[
  Why output vanishes — buffers and abnormal termination
][
  When a program dies by `abort` or a signal, the output remaining in the buffer
  vanishes with it. That is why the inference "the last printed line came out, so
  execution reached at least there" is dangerous — in reality several more lines
  may have run and died trapped in the buffer.

  The practice of sending debugging output to `stderr` came from here. What the
  standard promises about `stderr` goes only as far as *"it is not fully buffered"*
  (that is, it is unbuffered or line-buffered), and real implementations mostly make it
  unbuffered. So the record right up to the moment of death is *likely* to survive, but
  that is not a guarantee — it can be set again with `setvbuf`, and the manner of
  abnormal termination changes the outcome too. It is also
  the reason chapter 17 discussed debuggers and logs together.
]

== The remaining functions that handle files

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*to beware of*],
  [`remove`], [delete a file], [implementation-defined for an open file],
  [`rename`], [change a name], [if the target exists it fails or overwrites, depending on the implementation],
  [`tmpfile`], [create a temporary file], [deleted automatically on closing. the only portable safe edition],
  [`tmpnam`], [generate a temporary name], [★ a race condition — it can be intercepted between receiving the name and creating the file],
  [`setvbuf`], [specify the buffering mode], [it may be called only right after opening the stream],
  [`freopen`], [reconnect a stream], [used when turning `stdout` to a file],
)

`tmpnam` is in the standard, but not using it is the right answer — because
another program can slip in between the returning of the name and the creating of
a file with that name (the class of race called TOCTOU). Within the standard
`tmpfile` is the answer, and if a platform API is permitted, `mkstemp` (POSIX).

#qa[
  Then can "safe file handling" be written with standard I/O alone?
][
  Mostly it can. But there are clearly places where the standard gives no answer —
  handling directories, file locking, atomic replacement (writing to a temporary
  file and then renaming), permissions, symbolic links. All of these are the
  territory of platform APIs, and so a serious program lays one thin layer over
  standard I/O. Part XII's file layer is exactly that layer.
]

#recap[
  Reading and writing in summary.

  #dtable(
    columns: 3,
    [*what you want to do*], [*what to use*], [*what to check*],
    [read one line], [`fgets`], [whether null + whether there is a newline (truncation)],
    [a line of unknown length], [an `fgetc` loop + reallocation], [preserve the original when `realloc` fails],
    [print into a string], [`snprintf`], [return value ≥ buffer size means truncation],
    [parse user input], [`fgets` + `strtol`/`sscanf`], [the item count and the range],
    [temporary file], [`tmpfile`], [`tmpnam` is a race condition],
    [debugging output], [`fprintf(stderr, …)`], [not fully buffered (mostly unbuffered) — likely to survive],
    [never to be used], [`gets`, `%s` without a width, `%n`], [—],
  )
]

We have crossed the minefield of input and output. The next chapter is another
minefield just as famous — the string functions.
