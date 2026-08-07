#import "../../book/lib.typ": *

= Bibliography and sources

Gathered here are the works this book leans on and the sources of the incidents given
as cases in the body. How to obtain the standard document is in appendix D.

== Standard documents

- ISO/IEC 9899:2024 (C23) — this book's criterion standard. The widely referenced free
  document is N3220, but that is a working draft issued just after C23 (see
  appendix D).
- ISO/IEC 9899:2018 (C17), 9899:2011 (C11), 9899:1999 (C99),
  9899:1990 (C89/C90) — for checking back through the editions. Drafts N2176, N1570,
  N1256.
- IEEE 754 — the standard for floating-point arithmetic (the ground of chapters 8 and
  48).
- The Unicode Standard — character sets, encodings and normalisation (chapter 9).
- POSIX (IEEE Std 1003.1) — the system interfaces outside standard C (the background of
  chapter 90).

== The language's history

- Dennis M. Ritchie, "The Development of the C Language" (1993) — the founder's account
  of the CPL→BCPL→B→C lineage and the early design decisions (chapters 4 and 12).
- Brian W. Kernighan and Dennis M. Ritchie, #box[The C Programming Language]
  (1978, 1988) — the so-called K&R. The reference point dividing before C89 from after.
- The WG14 document repository (#link("https://open-std.org")[`open-std.org`]) — proposals and defect reports. The
  primary source for "why did this clause end up like this".

== The incidents this book cites

- The Ariane 5 flight 501 accident report (1996) — the failure a conversion error
  called down (chapter 29).
- Apple's "goto fail" TLS validation defect (CVE-2014-1266) — a branch without braces
  (chapter 31).
- The Morris internet worm (1988) — a buffer overflow through `gets` (chapters 40 and
  62, and the incident mentioned in appendix D's van der Linden entry).
- The mass discovery of format string vulnerabilities (1999–2000) — wu-ftpd and others
  (chapters 59 and 62).
- Denial of service using hash collisions (many web frameworks, 2011) and the
  algorithmic complexity attack paper (2003) — cases of a data structure's worst case
  becoming a security problem (chapters 82 and 89).
- The Debian OpenSSL random number defect (CVE-2008-0166) — predictable keys
  (chapter 90).
- The Linux kernel's adoption of `-fno-strict-aliasing` and removal of VLAs (2018) —
  compromises of reality (chapters 13, 17 and 38).

== Tools and libraries

- The official documentation of GCC and Clang/LLVM — warning options, sanitizers,
  extensions (chapter 17).
- The GDB and LLDB documentation — debuggers (chapter 17).
- The proven C library — the subject of this book's Part XII. This book describes the
  v26.07.23d snapshot and verified its examples with that edition.
- cppreference.com — the practical reference for the standard library.

== This book's verification environment

Every execution result printed in the body was really obtained in the following
environment.

- Compilation: `-std=c23 -Wall -Wextra -Werror`
- Reference compiler: GCC 14 (x86-64 Linux)
- Cross-checking: Clang 22
- Debugger session captures: GDB 17.2
- Typesetting: Typst 0.15.1
