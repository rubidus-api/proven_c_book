# Proven C Book — Modern C, built on the proven library

*[한국어판 README](README.md)*

A Korean-language book on C for people starting from zero. It begins with how a
computer is actually built rather than with a list of syntax, takes today's
standard (C23) as its default, and ends as a manual for the
[proven](https://github.com/rubidus-api) C library.

- **Current edition**: v0.1.0 — **draft**
- Korean: [PDF](dist/proven_c_book-v0.1.0-ko.pdf) · [Web](https://rubidus-api.github.io/proven_c_book/ko/)
- English: [PDF](dist/proven_c_book-v0.1.0-en.pdf) · [Web](https://rubidus-api.github.io/proven_c_book/en/) — *translation in progress*
- 12 parts, 58 chapters, appendices A–E and an index; about 570 pages.

## What makes it different

- **You only read.** No exercises, no "try it yourself". The text moves through
  question-and-answer, and review happens as deeper questions at the start of
  the next chapter.
- **Every printed output is real.** All 60+ listings are compiled and run on
  every build and their output is pasted into the page (GCC 14, cross-checked
  with Clang).
- **Today's C.** C23 is the default; older habits appear only as history.
- **Written with AI as an assisting tool.** Structure, principles and what to
  include were decided and reviewed by the author; examples are machine-verified.

## Translation status

The Korean edition is complete. The English edition currently carries the front
matter; chapters are added as they are translated. Contributions of translated
chapters are not accepted (see below) — the translation is done by the author.

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
