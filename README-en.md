# Proven C Book — Modern C, built on the proven library

*[한국어판 README](README.md)*

A Korean-language book on C for people starting from zero. It begins with how a
computer is actually built rather than with a list of syntax, takes today's
standard (C23) as its default, and ends as a manual for the
[proven](https://github.com/rubidus-api) C library.

- **Current edition**: v0.3.0 — **draft**
- Korean: [PDF](dist/proven_c_book-v0.3.0-ko.pdf) · [Web](https://rubidus-api.github.io/proven_c_book/ko/)
- English: [Web](https://rubidus-api.github.io/proven_c_book/en/) · [PDF](dist/proven_c_book-v0.3.0-en.pdf) — *complete*
- 13 parts, 81 chapters, appendices A–F and an index; about 350 pages (repository head).

## What makes it different

- **Built to be read, not drilled.** Instead of breaking the flow with exercises
  or "try it yourself", it carries you along through question-and-answer in the
  running text, with review as deeper questions at the start of the next
  chapter. If drills are what you want, the established C primers do that
  better — this book takes the seat next to them and explains *why things are
  shaped the way they are*.
- **Every printed output is real.** All 94 listings are compiled and run on
  every build and their output is pasted into the page (GCC 14, cross-checked
  with Clang).
- **Today's C.** C23 is the default; older habits appear only as history.
- **Written with AI as an assisting tool.** Structure, principles and what to
  include were decided and reviewed by the author; examples are machine-verified.

## Translation status

The Korean edition is complete (13 parts, 81 chapters, about 350 pages) and is
the source. The English edition is translated from it chapter by chapter, with
**chapter numbers identical to the original**, so a reference to "chapter 49"
means the same chapter in both editions.

The English edition is now **complete**: all 13 parts, 81 chapters, appendices
A–F and the index. Per-chapter status: [TRANSLATION.md](TRANSLATION.md).

The two editions are kept in step mechanically. `scripts/sync-status.py` records
the hash of the Korean source each translated file was made from and classifies
every file as synced / stale / pending; both book builds run it, so a change to
a Korean chapter shows up immediately as a stale English chapter. Narrative
device labels live in a single localized place, so editing a device changes both
editions at once. `scripts/check-xrefs.py` additionally compares the chapter
cross-references of the two editions and reports any that have drifted apart.

Contributions of translated chapters are not accepted (see below) — the
translation is done by the author.

## Running the examples

Every listing in the book lives here and can be built and run in one go.

```sh
scripts/verify-examples.sh          # build + run every example (C23), capture output
CC=clang scripts/verify-examples.sh # cross-check with another compiler
```

A C23 compiler is required (GCC 14+ or Clang 16+). Examples that
`#include <proven...>` link against the `vendor/proven` snapshot automatically.

> The manuscript sources (Typst) and typesetting scripts are not published.
> What this repository carries is the finished book (PDF, HTML) and the
> example code.

## Layout

```
dist/       released PDFs (ko, en) and zip archives
docs/       HTML editions served by GitHub Pages (ko/, en/)
examples/   every listing in the book (63 files), all verified
scripts/    example verification
vendor/     snapshot of the proven library (for linking the examples)
```

## License

- **Text** (`book/`, `book-en/`, generated PDFs): **CC BY-NC-SA 4.0** — see
  [LICENSE](LICENSE). Attribution required, no commercial use, share alike.
- **Example code** (`examples/`, `scripts/`): **MIT** — see [LICENSE-CODE](LICENSE-CODE).
- `vendor/proven/`: under its own license.

Details in [LICENSE-NOTICE.md](LICENSE-NOTICE.md).

## Contributing — error reports only

Wrong statements, listings that misbehave, typos and stale information are
welcome as issues. New chapters, rewrites and restructuring proposals are not
accepted; see [CONTRIBUTING.md](CONTRIBUTING.md).

Contact: rubidus@gmail.com
