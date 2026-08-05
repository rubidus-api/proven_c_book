#import "../../book/lib.typ": *

= The outside world — files, streams, time, random numbers

#organizer[
  From here we touch the operating system. Opening, reading and writing files,
  buffered streams, reading and formatting time, and random numbers — those random
  numbers that become entirely different things according to the purpose. Chapter 10's
  story of streams and chapter 25's story of input are completed here as real APIs.
]

#deepqa[
  Chapter 10's design had a stream be one where "the program does not know what it is
  connected to", and chapter 53 said `fopen` reports failure with null. Then what
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

== Files — open, read, close

#demo("examples/ch77/fileio.c")

Several things stand out.

*The path is a view too.* `proven_u8str_view_from_cstr("...")` — wherever the string
came from, it is handled as a pointer and a length (chapter 74).

*Opening needs an allocator.* The signature's first argument is a `scratch` allocator,
because temporary memory may be needed to turn the path into the form the operating
system requires. Chapter 73's rule is honestly kept here too — *if it can allocate, it
takes an allocator*.

*What is read becomes a view.* Bind the buffer and the number of bytes read and from
then on all of chapter 74's tools can be used. That is what the example used to divide
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

Date formatting uses the format syntax seen in chapter 53 as it is — named
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

#demo("examples/ch77/rng.c")

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
