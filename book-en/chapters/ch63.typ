#import "../../book/lib.typ": *

= Time — `<time.h>`

#prereq(
  ([chapter 42, Structs], [values bundled in a struct]),
  ([chapter 55, The terrain of the standard library], [the contract the standard settles]),
)

#deepqa[
  Bringing forward a story to be treated in Part XII: calendar time and elapsed
  time were said to be different things. Then what is used in standard C to measure
  exactly "whether three seconds have passed"?
][
  With the standard alone there is no way to measure it exactly. `time` is in
  seconds and can go backwards when the system adjusts the clock, and `clock`
  measures not wall-clock time but *the CPU time the process used* (so it does not
  grow while waiting for input and output). C11 brought in `timespec_get` but does
  not require a monotonic clock. Accurate elapsed measurement is the part of
  POSIX's `clock_gettime(CLOCK_MONOTONIC, …)` or Windows'
  `QueryPerformanceCounter` — that is, of *a platform API*.
]

#organizer[
  The header the standard settled unusually little of. The standard says neither
  what `time_t` is nor how time zones are handled. We look at the traps that arise
  in those gaps — the year with 1900 subtracted, the month starting from 0,
  functions that return a static buffer, and the fact that *there is no monotonic
  clock in the standard for measuring elapsed time*.
]

#chapter-questions()

== Three representations of time

#dtable(
  columns: 3,
  [*type or function*], [*what*], [*to know*],
  [`time_t`], [calendar time (usually seconds since 1970)], [★ the standard settles only "an arithmetic type"],
  [`struct tm`], [split into year, month, day, hour, minute, second], [the field rules are a trap],
  [`clock_t`], [the CPU time the process used], [divide by `CLOCKS_PER_SEC`],
  [`struct timespec`], [seconds + nanoseconds (C11)], [`timespec_get`],
)

That the standard did not settle what `time_t` is matters. In most implementations
it is seconds since 1970-01-01 UTC, but that is *a practice*, not a guarantee of
the standard. So portable code does not calculate with `time_t`'s internal value
directly but takes the difference with `difftime`.

#qa[
  Why do so many types exist for time — would one count of seconds not do?
][
  Because three different jobs are involved. `time_t` is an opaque value naming
  *one moment*; `struct tm` is the *calendar notation* people read (year, month,
  day, hour, minute, second); `clock_t` is a scale for measuring *elapsed time*.
  Turning a moment into a calendar is a hard computation full of time zones,
  daylight saving and leap seconds, so the standard keeps the two in separate
  types and lets `localtime` and `mktime` bridge them.

  The accidents in practice happen exactly at that border — subtracting two
  `time_t` values gives seconds, but adding to the fields of a `struct tm`
  directly produces an unnormalised date. Date arithmetic must go through
  `mktime`.
]

== The traps of `struct tm`

#demo("examples-en/ch63/timefns.c")

Two fields are famous traps.

- `tm_year` is *the value with 1900 subtracted*. 2026 is 126.
- `tm_mon` is *from 0*. August is 7.

With `tm_mday` (from 1), `tm_wday` (Sunday is 0) and `tm_yday` (from 0) the rules
are all different, so when filling them by hand it is better to keep a table
beside you.

`mktime` does two things — it turns a `struct tm` into a `time_t`, and it
*normalises the struct*. In the example, adding 30 to `tm_mday` exceeded the range
and yet it was tidied into 4 September thanks to that. Not doing date arithmetic
yourself but using this property is the canonical way.

`tm_isdst` is easy to forget too. Putting in −1 means "I do not know, judge for
yourself", and putting 0 or 1 in wrongly puts you an hour out.

#antipattern[
  Carrying around the result of `localtime`
][
  ```c
  struct tm *a = localtime(&t1);
  struct tm *b = localtime(&t2);   /* what a pointed at has been overwritten */
  printf("%d %d\n", a->tm_hour, b->tm_hour);   /* both are t2's time */
  ```
  `localtime`, `gmtime`, `ctime` and `asctime` return *an internal static buffer*
  (those functions chapter 55 brushed past by name only). The next call overwrites
  the previous result, and in a program running along several strands they wreck
  each other's results.

  There are two prescriptions. *Copy it* immediately on receipt, or use the edition
  in which the caller gives the buffer (`localtime_r` and `gmtime_r` are POSIX,
  `localtime_s` is annex K and MSVC). To keep portability with the standard alone,
  copying is the right answer.
]

== Printing to a string — `strftime`

Unlike `printf`, `strftime` takes the buffer size and *returns 0 if it does not
fit*. The example's small buffer is that case. If the return value is 0 the
buffer's content is undetermined, so it must not be used.

`asctime` and `ctime` are better not used. Besides returning a static buffer, the
form is fixed (`"Wed Aug  5 13:45:30 2026\n"`) and it can overflow when the year
exceeds four digits, so C23 marked them for retirement.

#dtable(
  columns: 3,
  [*specifier*], [*meaning*], [*note*],
  [`%Y`, `%m`, `%d`], [year, month, day], [the ISO date is `%Y-%m-%d`],
  [`%H`, `%M`, `%S`], [hour, minute, second], [24-hour],
  [`%F`, `%T`], [`%Y-%m-%d`, `%H:%M:%S`], [C99],
  [`%z`, `%Z`], [time-zone offset and name], [locale- and platform-dependent],
  [`%s`], [epoch seconds], [★ not standard (a POSIX extension)],
  [`%c`, `%x`, `%X`], [locale notation], [for humans. not used for machines],
)

== Time zones and summer time — what the standard does not handle

The time zones standard C knows are only two, "local" and "UTC", and there is not
even a standard way to *change* the local time zone (POSIX's `TZ` environment
variable is the practice). To handle an arbitrary time zone a library is needed.

Summer time is trickier still. On a transition day there arise times that do not
exist (the hour skipped in spring) and times that exist twice (the hour repeated in
autumn). What `mktime` returns when given such input is settled by the
implementation.

#realcase[
  Real accidents time has called down
][
  Time is a regular in quiet accidents. In 2012 and 2015, when *leap seconds* were
  inserted, several server programs burned CPU at 100% or froze — because kernels
  and applications had not assumed a situation in which "one second comes twice".

  The limit of a *32-bit `time_t`* overflows on 19 January 2038 (the day
  chapter 26's overflow appears on a worldwide scale). In embedded and old systems
  it is an ongoing task even now, and so the transition to a 64-bit `time_t` has
  long been under way.

  *Code that measures elapsed time in local time* loses or gains an hour on every
  summer-time transition day. A log's timestamps go backwards, a timeout becomes an
  hour long, a scheduler runs the same job twice. The prescription is always the
  same — *a monotonic clock for elapsed time, UTC for records, local time only when
  showing it to a human*.
]

#recap[
  Time in summary.

  #dtable(
    columns: 3,
    [*what you want to do*], [*what to use*], [*what to beware of*],
    [the current time], [`time(NULL)`], [in seconds. it can go backwards],
    [splitting it up], [`localtime`/`gmtime` + *copy at once*], [the static buffer],
    [date arithmetic], [add to the fields and `mktime`], [do not calculate seconds by hand],
    [to a string], [`strftime`], [a return of 0 = failure],
    [difference], [`difftime`], [`t2 - t1` is not portable],
    [measuring elapsed time], [the platform's monotonic clock], [`time` and `localtime` forbidden],
    [storing and transmitting], [UTC + ISO 8601], [do not store local time],
    [not to be used], [`asctime`, `ctime`], [static buffer, fixed form, to be retired in C23],
  )
]

We have passed time. The next chapter is the tools used when a program *has gone
wrong* — error numbers, assertions, signals, and non-local jumps.
