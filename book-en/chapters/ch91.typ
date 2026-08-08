#import "../../book/lib.typ": *

= The outside world — files, streams, time, random numbers

#prereq(
  ([chapter 62, Streams in reality], [streams]),
  ([chapter 25, Input], [input]),
)

#deepqa[
  Chapter 10's design had a stream be one where "the program does not know what it is
  connected to", and chapter 60 said `fopen` reports failure with null. Then what
  failure is most often missed in a file API?
][
  *The partial write.* A failure to open is conspicuous, but the case of `write`
  returning without writing all that was requested is easy to forget — it really
  happens when the disk fills, when a signal cuts in, or when the other end is a pipe.
  So this library has two editions. `proven_fs_write` returns *the number of bytes
  actually written*, and `proven_fs_write_all` repeats until all is written and then
  reports only success or failure. What most code wants is the latter, and the former
  remains so as not to hide that fact.
]

#organizer[
  From here we touch the operating system. Opening, reading and writing files,
  buffered streams, reading and formatting time, and random numbers — those random
  numbers that become entirely different things according to the purpose. Chapter 10's
  story of streams and chapter 25's story of input are completed here as real APIs.
]

#chapter-questions()

== The life of one file

A file is the first resource in this part that touches *the outside world*. The
discipline of making and giving back is the same as in the earlier chapters, but the
kinds of failure are far more numerous.

#demo("examples-en/ch91/fslife.c")

The life cycle is four steps, and at each step failure comes as a value.

#dtable(
  columns: 3,
  [*step*], [*function*], [*to know*],
  [opening], [`proven_fs_open(scratch, path, mode)`], [*a scratch allocator* is needed (for path conversion)],
  [writing, reading], [`_write`/`_write_all`, `_read`], [amount requested ≠ amount handled],
  [nailing it down], [`proven_fs_sync(file)`], [closing alone does not leave it on the disk],
  [closing], [`proven_fs_close(file)`], [it has a return value — there is something to check],
)

*The mode weaves bit flags.* Instead of a string like the standard `fopen`'s `"w+b"`,
named values are joined with `|`.

#dtable(
  columns: 2,
  [*flag*], [*meaning*],
  [`PROVEN_FS_READ`], [reading],
  [`PROVEN_FS_WRITE`], [writing],
  [`PROVEN_FS_APPEND`], [appending at the end],
  [`PROVEN_FS_CREATE`], [create it if absent],
  [`PROVEN_FS_TRUNC`], [empty it if present],
  [`PROVEN_FS_CREATE_NEW`], [★ *fail if it already exists* — creating anew without a race],
)

Two things are better than string modes. First, *the combination is visible* — there is
no need to memorise what `"a+"` exactly is. Second, things absent from string modes,
such as `CREATE_NEW`, can be expressed. When making a lock file or a temporary file,
"fail if it already exists" is the only road that blocks a race condition (recall
chapter 63's `tmpnam` story).

*Handles travel by value.* That the functions take a `proven_file_t` by value rather
than by pointer is the mark of it, because what is inside is about one integer
descriptor. So the discipline of not using that value again after closing is still a
human's part.

=== The convenience functions that read and write in one go

In half the cases in practice the file is small and may be handled whole. Then opening,
reading and closing need not be woven by hand.

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*caution*],
  [`proven_fs_read_all(alloc, path)`], [the whole file as bytes], [a large file eats memory],
  [`proven_fs_read_all_u8str(alloc, path)`], [the whole file as a string], [the same. it does not check the encoding],
  [`proven_fs_write_file(scratch, path, data)`], [writing it whole], [die in the middle and a half-written file is left],
  [`proven_fs_write_file_atomic(...)`], [write to a temporary and swap], [★ no half-written file is left],
  [`proven_fs_write_file_durable(...)`], [atomic + `sync`], [it survives a power cut. the slowest],
)

The difference between the last three rows matters in practice. Code that overwrites a
configuration file or saved data, *if it dies in the middle, leaves a file that is
neither the original nor the new one*, and the standard practice that prevents it is
"write to a temporary file and rename" (renaming is atomic within the same file system).
`_atomic` does that work for you, and `_durable` hangs a `sync` on it as well so that it
survives a power cut.

#demo("examples-en/ch91/fileio.c")

Several things stand out.

*The path is a view too.* `proven_u8str_view_from_cstr("...")` — wherever the string
came from, it is handled as a pointer and a length (chapter 88).

*Opening needs an allocator.* The signature's first argument is a `scratch` allocator,
because temporary memory may be needed to turn the path into the form the operating
system requires. Chapter 87's rule is honestly kept here too — *if it can allocate, it
takes an allocator*.

*What is read becomes a view.* Bind the buffer and the number of bytes read and from
then on all of chapter 88's tools can be used. That is what the example used to divide
the lines, and copying never happened once.

#antipattern[
  Trusting `size` and assuming that much was read
][
  ```c
  proven_result_size_t sz = proven_fs_size(f);
  proven_byte_t *buf = malloc(sz.value);
  (void)proven_fs_read(f, (proven_mem_mut_t){ buf, sz.value });
  process(buf, sz.value);      /* was that much really read? */
  ```
  A file's size and *the amount this read brought* are different. From pipes,
  terminals and networks it comes a little at a time, and even a file may end in the
  middle. The number `read` returned must be used, and that is why this library returns
  the read result as a bundle. The fact chapter 25 stated — "input is not a keyboard
  but a stream" — becomes a practical rule here.
]

== Streams — reading and writing with a buffer

System calls are expensive. Call `write` one byte at a time and that cost accumulates
as it stands. So standard C's `FILE*` kept a buffer (chapter 10's story of line
buffering), and proven puts a *stream* in the same place — differing in two ways.

- *The caller gives the buffer.* The stream does not allocate in secret.
- *The failure of a flush comes as a value.* It is not quietly swallowed on closing.

These two aim at the same problem. In buffered writing the real failure shows itself
not in `write` but in the *flush*, and missing that failure creates data that "was
believed successful but is not on the disk". It is also the place databases and file
systems are most careful about (the reason `proven_fs_sync` exists separately).

== Time — two different clocks

Two different things are mixed together in time.

- *Calendar time* — a time a human reads, like "5 August 2026, 09:00". It is used for
  showing to users and leaving in records. Time zones, leap seconds and summer time are
  entangled in it.
- *Monotonic time* — a scale that does not go backwards. It is used for measuring
  *elapsed time*.

Mix the two and you get the famous bug. Measure elapsed time with a calendar clock and
the moment the system adjusts the time or summer time changes, *a negative elapsed
time* comes out. Let that value into a timeout calculation and it waits forever or
expires at once.

Date formatting uses the format syntax seen in chapter 60 as it is — named
placeholders with width and fill specified, as in `"{year}-{month:0>2}-{day:0>2}"`.
Unlike `strftime`'s `%Y-%m-%d`, the difference is that *there is no need to memorise
what symbol means what*.

#misconception[
  "Time is just a number, so adding and subtracting is fine"
][
  Not in calendar time. A day is not always 86400 seconds (a summer-time transition
  day is 23 or 25 hours), the lengths of months differ, and time zones change by
  political decision. "A month later" is not arithmetic but a calendar rule. The
  difference of *monotonic* times, on the other hand, may be handled as a plain number
  — that is one more reason to use a monotonic clock for measuring elapsed time.
]

== Random numbers — the purpose settles the thing

Few tools are as much "the same name, different demands" as random numbers. The
library does not hide this but divides it into three.

#demo("examples-en/ch91/rng.c")

*Reproducible random numbers* (`proven_xoshiro256ss_t`) are for simulation, games and
testing. The same seed gives the same sequence — the very reason the example's two
lines are identical, and also the property that lets a failed test be reproduced. They
are fast but *predictable*, so they are never used for secrets.

*Random numbers for secrets* (`proven_random_bytes`) come from the operating system's
cryptographic source of randomness. They are used for values *an attacker must not
guess* — keys, tokens, session identifiers.

The third is the compromise between the two, `proven_chacha_rng_t`, which takes a seed
once from the OS source of randomness and then continues a cryptographically secure
sequence quickly.

#realcase[
  The accidents predictable random numbers made
][
  Accidents breaking this distinction have happened repeatedly. An online card game
  whose hands were predicted because it used the time as a seed, a case where session
  identifiers made with a fast random generator let one into somebody else's account,
  and, representatively, the 2008 incident in which Debian's OpenSSL patch deleted the
  entropy-gathering code so that *the number of generable keys shrank to a few tens of
  thousands*. The last required every key already made to be discarded. The lesson is
  summed up in one line of the library's documentation — *random numbers for secrets
  come only from a cryptographic source of randomness.*
]

#qa[
  What becomes of random numbers for secrets if there is no operating system?
][
  They cannot be obtained, so `proven_random_bytes` *returns a falsehood* — and this is
  an important design decision. Many libraries slip back to the time or an address
  value in this situation, and then you have the worst state of "believed safe but
  predictable". This library does not fall back; it declares failure. If the board has a
  real source of entropy (a hardware random number generator) it can be registered and
  used. *Rather than quietly give something bad, say there is none* — another face of
  the principle met continually in this part.
]

== Memory mapping

There is one more way of reading a file. Hanging the file whole *in the address space*
and accessing it with a pointer — `proven_mmap_*` is that window. It is advantageous
when reading a large file by roaming randomly over it, and it is used when several
processes share the same file.

The price is clear too. If the file is truncated while the mapped region is being
touched, the program can receive a signal and die, and portability is lower than the
file API's. So this tool is not "what is used by default" but "what is chosen when
there is a reason".

#recap[
  The outside world in summary.

  #dtable(
  columns: 3,
    [*what it does*], [*API*], [*to beware of*],
    [open and close], [`proven_fs_open/close`], [needs a scratch allocator],
    [reading], [`proven_fs_read`], [amount requested ≠ amount read],
    [writing], [`proven_fs_write` / `_write_all`], [partial writes],
    [nailing it to the disk], [`proven_fs_sync`], [flush ≠ sync],
    [buffered I/O], [`proven_stream_*`], [the caller gives the buffer],
    [the current time], [`proven_time_now_datetime`], [not used for measuring elapsed time],
    [date formatting], [`proven_time_u8_fmt`], [named placeholders such as `{year}`],
    [reproducible random], [`proven_xoshiro256ss_*`], [not used for secrets],
    [random for secrets], [`proven_random_bytes`], [a falsehood if absent — it does not fall back],
    [memory mapping], [`proven_mmap_*`], [only when there is a reason],
)
]

What remains now is the boundaries — the way of running several things overlapped, and
the place with no operating system at all. The last chapter closes this part.
