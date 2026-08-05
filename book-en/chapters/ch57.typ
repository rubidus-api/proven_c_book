#import "../../book/lib.typ": *

= Streams in reality — `<stdio.h>` ①

#prereq(
  ([chapter 10, The origin of streams], [the origin of streams]),
  ([chapter 22, Output], [output in reality]),
)

#deepqa[
  Chapter 10 said a stream is "a band whose other end the program does not know",
  and that this is why the same program serves screen, file and other programs
  alike. Then *when* do the bytes a program wrote actually arrive at their
  destination?
][
  Usually not at once. The standard library keeps a *buffer* per stream, gathers
  bytes there and sends them out in one go — because system calls are expensive (we
  meet this again in Part XII). There are three ways of deciding when to empty it.
  *Full buffering* is when the buffer fills, *line buffering* when a newline is
  met, *unbuffered* is immediately. Standard output connected to a terminal is
  usually line-buffered, and when redirected to a file it turns into full
  buffering — meaning *the moment at which the same program's output appears
  changes with what it is connected to*, and that is this chapter's first trap.
]

#organizer[
  We look at the floor beneath the header used most. How a stream is really opened
  and closed, when the buffer is emptied, where failure shows itself — and the
  misuse of `feof`, the place introductory books get wrong over and over. The
  notion of a stream learned in chapter 10 becomes an API here.
]

#chapter-questions()

== Open, write, close — failure can happen three times

#demo("examples-en/ch57/streams.c")

It is worth noticing that the example checks for failure in three places.

*① `fopen`* — on failure it returns null. The reason is left in `errno`, and
`perror` prints it as a sentence a human can read (chapter 64). The file may not
exist, permission may be lacking, or too many files may be open.

*② Writing* — `fprintf` returns the number of characters printed and gives a
negative value on failure. Code that checks it is rare, but if the disk fills or a
pipe breaks it shows itself here.

*③ `fclose`* — here is the real trap. It is the place where what remained in the
buffer is finally sent out, so *it is common for a write failure to show itself
for the first time on closing*. A program that must not lose data therefore always
checks `fclose`'s return value.

#antipattern[
  Ignoring the failure of closing
][
  ```c
  fprintf(f, "%s\n", important);
  fclose(f);                  /* nobody asked whether it failed */
  puts("saved");              /* it may in fact not have been saved */
  ```
  In buffered writing, the moment at which "it succeeded" may be said is *after
  closing has succeeded*. If it must truly be nailed to the disk, flush with
  `fflush` before closing and call the platform's synchronisation call (`fsync` and
  the like) as well — that is what a database does.
]

== How to know the end of a file — the misuse of `feof`

The most widespread wrong answer is here.

#demo("examples-en/ch57/feof_bad.c")

Why is `while (!feof(f))` wrong? `feof` is *not a prophet but a recorder* — the
mark saying "the end of the file was reached" is turned on only *after* a read has
failed. So right after reading the last value it is still off, the loop turns once
more, and the result of the failed read (= the previous value, unchanged) is used
as it stands. That 30 was printed twice in the example is the evidence.

The rule is one. *Control the loop by the reading function's return value.* `feof`
and `ferror` are used after the loop ends, to tell "why did it end".

#dtable(
  columns: 3,
  [*function*], [*success*], [*end or failure*],
  [`fgets`], [the buffer pointer], [null — tell them apart with `feof`/`ferror`],
  [`fscanf`], [the number of items filled], [0 (format mismatch) or `EOF`],
  [`fgetc`], [the character read (an unsigned char as an int)], [`EOF`],
  [`fread`], [the number of *elements* read], [fewer than requested means the end or an error],
)

#misconception[
  "The result of `fgetc` may be put in a `char`"
][
  It may not. `fgetc` returns an `int`, and that value is either *a character in
  0–255* or *`EOF`* (usually −1). The moment it goes into a `char` the two can no
  longer be told apart — on an implementation where `char` is signed, a 0xFF byte
  becomes −1 and is identical to `EOF`, and where it is unsigned, `EOF` becomes 255
  and it never ends. So `int c; while ((c = fgetc(f)) != EOF)` is the canonical
  form. This one line is also the idiom most often miscopied in introductions to C.
]

== Lines longer than the buffer

That is what the last part of the example shows. If the buffer is too small
`fgets` reads *only that far* and stops — it is not an error. So without checking
whether a newline is in there, what you believed to be "one line" may in fact be
the front piece of a line.

Read `one\n` with a 4-byte buffer and it comes split in two: `one` (no newline)
and `\n` (a newline only). When code handling long lines in the field forgets this
fact, one line is quietly processed as two records.

#qa[
  What is done when the line length is unknown?
][
  There are three roads. First, *a big enough buffer plus a newline check* — if
  there is no newline, read the rest away or treat it as an error. Second, *reading
  while growing it yourself* — gather one character at a time with `fgetc` and
  enlarge the buffer when needed (chapter 41's dynamic allocation). Third, *a
  function the platform gives* — POSIX's `getline` enlarges by itself, but it is
  not standard. To write with the standard alone the second is the right answer,
  and using a library so as not to write that code every time is Part XII's story.
]

== Text mode and binary mode

That is the `b` attached to `fopen`'s second argument. On the Unix family there is
no difference, but on Windows there is — text mode turns `\n` into `\r\n` when
writing and turns it back when reading. So opening a binary file in text mode
quietly changes the bytes.

#platform[
  Windows' line-ending conversion
][
  When handling binary data (images, compressed files, serialised structs) always
  open with `"rb"` or `"wb"`. Open in text mode and a 0x0A byte grows into 0x0D
  0x0A, and on reading it shrinks the other way — *the file's size and content
  differ*. It is the place where the CR/LF story seen in chapter 9 is replayed in
  the file API.

  Conversely, opening a text file in binary mode on Windows leaves a `\r` at the
  end of the line, so a line read with `fgets` ends with an invisible `\r` — the
  cause of a failing comparison is often here.
]

#antipattern[
  `fflush(stdin)`
][
  ```c
  scanf("%d", &n);
  fflush(stdin);      /* the intent is to empty the input buffer — it is outside the contract */
  ```
  `fflush` is a function for *output* streams. Using it on an input stream is
  behaviour the standard does not define (some implementations merely support it as
  an extension), and it cannot be used in portable code. To throw away the
  remaining input you must read it away yourself.

  ```c
  int c;
  while ((c = getchar()) != '\n' && c != EOF) { }
  ```
]

== File position and size

`fseek` and `ftell` handle position, but with restrictions. On a text stream the
value `ftell` returns is *not guaranteed to be a byte offset*, and `fseek` is safe
only with that value or with the combination of `SEEK_SET` and 0. On a large file
`long` may be too small, so the non-standard `fseeko` and `ftello` (POSIX) or a
platform API become necessary.

The idiom "to learn a file's size, go to the end and `ftell`" is safe only in
binary mode, and even then it is meaningless if the file is changing.

#recap[
  `<stdio.h>` streams in summary.

  #dtable(
    columns: 3,
    [*place*], [*rule*], [*if got wrong*],
    [`fopen`], [check for null], [null dereference],
    [writing], [check the return value (optional), check `fclose` (compulsory)], [quiet data loss],
    [loop control], [by the reading return value], [misuse of `feof` — the last value duplicated],
    [`fgetc`], [put it in an `int`], [confusing `EOF` with 0xFF],
    [`fgets`], [check for a newline], [a long line processed split],
    [binary files], [`"rb"`/`"wb"`], [byte corruption on Windows],
    [emptying input], [read it away yourself], [`fflush(stdin)` is outside the contract],
    [position], [byte meaning only in binary mode], [misunderstanding in text mode],
  )
]

We have seen the skeleton of streams. The next chapter is the functions that
actually read and write on top of it — and the story of a function *deleted* from
the standard.
