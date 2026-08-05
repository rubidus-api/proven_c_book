// Proven C Book — English edition (in progress). Build: scripts/build-book-en.sh
#import "../book/lib.typ": *

#let book-version = "v0.1.0"
#let book-updated = "2026-08-05"
#let book-status = "draft — translation in progress"
#let book-repo = "https://github.com/rubidus-api/proven_c_book"

#set document(title: "Proven C Book " + book-version, author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "en")
#set par(justify: true, leading: 0.78em, first-line-indent: (amount: 1em, all: true))
#show heading: set text(font: ("Noto Sans CJK KR",))
#show raw: set text(font: "D2Coding", size: 0.92em, ligatures: false)
#set heading(numbering: none)

#page(numbering: none)[
  #v(3.2cm)
  #align(center)[
    #text(font: ("Noto Sans CJK KR",), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[Modern C, built on the proven library]
    #v(0.9cm)
    #box(inset: (x: 10pt, y: 5pt), radius: 3pt, fill: rgb("#f3e9e6"),
      stroke: 0.7pt + rgb("#b0483c"))[
      #text(size: 10pt, fill: rgb("#8a3226"), weight: "bold")[#book-status]
    ]
    #v(0.5cm)
    #text(size: 10.5pt)[#book-version · last updated #book-updated]
    #v(1.0cm)
    #text(size: 11pt)[rubidus]
    #v(0.25cm)
    #text(size: 9.5pt, fill: rgb("#555555"))[
      rubidus\@gmail.com #h(0.8em) #book-repo
    ]
  ]

  #v(1.4cm)
  #align(center, block(width: 74%)[
    #set text(size: 10pt)
    #set par(justify: false, first-line-indent: 0em, leading: 0.75em)
    #align(center)[
      An introduction to C, and an introduction to the proven C library.

      #v(0.3cm)
      Written for readers who are just starting out with C.
    ]
  ])
]

= Translation status

This English edition is being translated from the Korean original, which is
complete (12 parts, 58 chapters, appendices A–E and an index, about 570 pages).
The translation is *in progress*: this volume currently carries the front
matter, and chapters are added as they are translated.

Until then, the complete text is the Korean edition:

- PDF — `dist/proven_c_book-v0.1.0.pdf`
- Web — #link("https://rubidus-api.github.io/proven_c_book/ko/")[rubidus-api.github.io/proven_c_book/ko/]

#v(0.4cm)

Chapter-level progress is tracked in the repository (`TRANSLATION.md`).

= Preface

This book is an introduction to *proven*, a C library I wrote — and, honestly,
a piece of promotion for it.

But to argue that something is worth using, you first have to show why it is
needed. The problems proven addresses live in the subtle corners of C: the
limits of representation, the rules of conversion, the lifetime of memory, the
regions the standard declines to define. To someone who has not met those
corners, proven looks like an unnecessary contraption. So I decided to explain
the problems first. That is why most of this book is plain standard C, and why
proven does not appear in earnest until Part XI. If, after the earlier
chapters, you conclude that you do not need proven — that is a good outcome
too.

There is also, in these pages, some measure of affection for the language. Try
explaining why a fifty-year-old language is still here, and what its design gave
up in order to get what it got, and affection follows on its own. I have not
hidden that part.

Every listing in this book is really compiled and really run. The printed output
was not transcribed by hand: it is what the machine produced when the book was
built, cross-checked with two compilers (gcc and clang).

One more disclosure. *This book was written with AI as an assisting tool.* What
to cover, in what order, under which principles, and what to leave out — all of
that I decided, and the draft was written to those instructions and then
reviewed by me. Technical claims were checked against the standard and primary
sources, and the examples are verified by machine on every build: whoever wrote
it, *code that does not run does not go into this book*.

Errors will remain nonetheless. Those are mine, not the tool's. If you find one,
please tell me.

== Copyright and contact

The text of this book (prose and figures) is licensed under
*CC BY-NC-SA 4.0*. You may share and adapt it with attribution; commercial use
is not permitted, and adaptations must carry the same license.

The example code is licensed under the *MIT license*. Anything you learn here
can go into your own programs without restriction.

Contact is by email. Corrections are best filed through the GitHub repository —
they stay on the record and other readers can see them.

*Only error reports are accepted as contributions.* Wrong statements, listings
that do not behave as printed, typos and stale information are all welcome.
Manuscript contributions — new chapters or sections — are not.

#v(0.2cm)
rubidus\@gmail.com \
#book-repo
