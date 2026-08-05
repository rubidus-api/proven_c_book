// Proven C Book — English edition. Build: scripts/build-book-en.sh
#import "../book/lib.typ": *

#let book-version = "v0.7.0"
#let book-updated = "2026-08-06"
#let book-status = "draft"
#let book-repo = "https://github.com/rubidus-api/proven_c_book"

#set document(title: "Proven C Book " + book-version, author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
// 영어판 본문은 라틴 Noto 로 조판한다. CJK 판(…CJK KR)의 라틴 자형은
// 본디 다른 글꼴이라 영문 조판에는 맞지 않는다 — CJK 는 뒤에 두어
// 이따금 인용되는 한글만 받는다.
#set text(font: ("Noto Serif", "Noto Serif CJK KR"), size: 10.5pt, lang: "en")
#set par(justify: true, leading: 0.78em, first-line-indent: (amount: 1em, all: true))
#show heading: set text(font: ("Noto Sans", "Noto Sans CJK KR"))
// 절 제목(1.2 꼴)은 위아래로 숨을 준다 — 기본값은 본문에 너무 붙는다
#show heading.where(level: 2): set block(above: 1.9em, below: 1.05em)
#show heading.where(level: 3): set block(above: 1.5em, below: 0.85em)
// 코드는 영어권 독자에게 익숙한 라틴 고정폭 Noto Sans Mono 로 통일한다.
// D2Coding 은 쓰지 않는다. 유니코드·인코딩 설명처럼 코드 안에 한글이
// 꼭 있어야 하는 자리만 Noto Sans CJK KR 이 뒤에서 받는다.
#show raw: set text(font: ("Noto Sans Mono", "Noto Sans CJK KR"), size: 0.96em, ligatures: false)
// 인쇄를 위해 구문 강조 색을 쓰지 않는다 (잉크·토너 절약)
// 회색조에서도 구분되는 절제된 구문 강조 (RFC-0006 §7.2)
#set raw(theme: "/book/theme-print.tmTheme")
#set heading(numbering: none)

#page(numbering: none)[
  #v(3.2cm)
  #align(center)[
    #text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[An Introduction to Modern C with the Proven C Library]
    #v(0.9cm)
    #box(inset: (x: 10pt, y: 5pt), stroke: 1pt + black)[
      #text(size: 10pt, weight: "bold")[#book-status]
    ]
    #v(0.5cm)
    #text(size: 10.5pt)[#book-version · last updated #book-updated]
    #v(1.0cm)
    #text(size: 11pt)[rubidus]
    #v(0.25cm)
    #text(size: 10pt)[
      #link("mailto:rubidus@gmail.com")[rubidus\@gmail.com] #h(0.8em) #link("https://github.com/rubidus-api/proven_c_book")[github.com/rubidus-api/proven_c_book]
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

// ── 판권 — 한국어판과 같은 내용을 영어로 (book/main.typ 의 대응면) ──────
#page(numbering: none)[
  #v(1fr)
  #set text(size: 10pt)
  #set par(justify: false, first-line-indent: 0em, leading: 0.85em, spacing: 0.95em)
  #show link: it => text(fill: black, it)

  #block(width: 100%)[
    #text(size: 11pt, weight: "bold", font: ("Noto Sans", "Noto Sans CJK KR"))[
      Proven C Book
    ]
    #linebreak()
    #text(size: 10pt)[An Introduction to Modern C with the Proven C Library]
  ]

  #v(0.45cm)

  #grid(
    columns: (5.6em, 1fr),
    row-gutter: 0.62em,
    column-gutter: 0.8em,
    text(fill: rgb("#555555"))[author], [rubidus],
    text(fill: rgb("#555555"))[contact], link("mailto:rubidus@gmail.com")[rubidus\@gmail.com],
    text(fill: rgb("#555555"))[repository], link("https://github.com/rubidus-api/proven_c_book")[github.com/rubidus-api/proven_c_book],
    text(fill: rgb("#555555"))[edition], [#book-version — #book-status],
    text(fill: rgb("#555555"))[last updated], [#book-updated],
  )

  #v(0.5cm)
  #line(length: 100%, stroke: 0.5pt + rgb("#999999"))
  #v(0.45cm)

  #block(width: 100%)[
    *The text* — Creative Commons Attribution-NonCommercial-ShareAlike 4.0
    International (CC BY-NC-SA 4.0). You may share and adapt it freely so long as
    you credit the source; commercial use is not permitted, and adaptations must
    carry the same licence.
    #linebreak()
    #text(size: 10pt, fill: rgb("#555555"))[https://creativecommons.org/licenses/by-nc-sa/4.0/]
  ]

  #v(0.35cm)

  #block(width: 100%)[
    *The example code* — the MIT licence, so that what you learn here can go into
    your own programs without constraint. The proven library used in the examples
    is under its own licence.
  ]

  #v(0.35cm)

  #block(width: 100%)[
    Every demonstration in this book prints output really obtained by compiling and
    running the code. The typesetting is done with Typst.
  ]

  #v(0.5cm)
  #line(length: 100%, stroke: 0.5pt + rgb("#999999"))
  #v(0.45cm)

  #block(width: 100%)[
    *This book keeps being corrected.* What you are reading is the edition numbered
    above; corrections and additions follow it. *The newest edition and the record of
    what changed are on the author's GitHub* — if you hold an old copy, look there
    first. Reports of errors and suggestions are received in the same place.
  ]
]

#let parts = (
  ("Part I — Ground", none, (1, 2, 3)),
  ("Part II — How computing works", "parts/part02.typ", (4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)),
  ("Part III — The first program", none, (15, 16, 17, 18)),
  ("Part IV — A minimal toolbox", none, (19, 20, 21, 22)),
  ("Part V — Declarations: how names are made", none, (23, 24, 25)),
  ("Part VI — Values and flow", none, (26, 27, 28, 29, 30, 31, 32)),
  ("Part VII — Memory", none, (33, 34, 35, 36, 37, 38, 39, 40)),
  ("Part VIII — The shape of data", none, (41, 42, 43)),
  ("Part IX — Deep corners", none, (44, 45, 46)),
  ("Part X — Structure", none, (47, 48, 49, 50, 51, 52, 53)),
  ("Part XI — Reading the standard library", "parts/part11s.typ", (54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68)),
  ("Part XII — proven — fundamentals, verified", "parts/part12.typ", (69, 70, 71, 72, 73, 74, 75, 76, 77, 78)),
  ("Part XIII — Closing", none, (79, 80, 81)),
)

// 목차: 장만 나열하면 길어지므로 *부 단위로 묶어* 낸다.
// 장 제목과 쪽 번호는 실제 heading 에서 가져온다(수작업 목록 금지).
#context {
  let heads = query(heading.where(level: 1)).filter(h => h.numbering != none)
  let by-num = (:)
  for h in heads {
    let n = counter(heading).at(h.location()).first()
    by-num.insert(str(n), h)
  }
  // 번호 없는 1단계 제목(머리말·번역 노트·부록·찾아보기)도 차례에 싣는다.
  let plain = query(heading.where(level: 1)).filter(h => h.numbering == none)
  let ch-pages = heads.map(h => counter(page).at(h.location()).first())
  let first-ch = calc.min(..ch-pages)
  let last-ch = calc.max(..ch-pages)
  let page-of = h => counter(page).at(h.location()).first()
  let front-extra = plain.filter(h => page-of(h) < first-ch)
  let back-extra = plain.filter(h => page-of(h) > last-ch)
  let row = (label, h) => block(width: 100%, inset: (left: 1.2em), below: 0.5em,
    grid(columns: (1fr, auto), column-gutter: 0.6em, align: (left, right),
      link(h.location())[#label],
      link(h.location())[#page-of(h)]))

  block(width: 100%)[
    #set par(leading: 0.9em)
    #text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 15pt, weight: "bold")[Contents]
    #v(0.9em)
    #for h in front-extra { row(h.body, h) }
    #for (part-title, intro, chs) in parts {
      block(above: 1.5em, below: 0.7em, sticky: true)[
        #text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 10.5pt, weight: "bold", part-title)
      ]
      for i in chs {
        let h = by-num.at(str(i), default: none)
        if h != none { row([#i. #h.body], h) }
      }
    }
    #if back-extra.len() > 0 {
      block(above: 1.5em, below: 0.7em, sticky: true)[
        #text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 10.5pt, weight: "bold")[Appendices and index]
      ]
      for h in back-extra { row(h.body, h) }
    }
  ]
}

#pagebreak()

#set heading(numbering: none)
#include "front/preface.typ"

= A note on this translation

The Korean edition is the original. This English edition is translated from it
chapter by chapter, and *chapter numbers are kept identical to the original*,
so a cross-reference to "chapter 54" means the same chapter in both editions.
All 13 parts, 81 chapters, appendices A–F and the index are now translated.

The two editions are kept in step mechanically: `scripts/sync-status.py`
records the hash of the Korean source each translated file was made from, and
every build reports any chapter whose original has changed since, so a change
on one side surfaces as a stale entry on the other. `scripts/check-xrefs.py`
compares the chapter cross-references of the two editions and reports any that
have drifted apart. Per-chapter status is in `TRANSLATION.md` in the
repository.

The Korean edition is at:

- Web — #link("https://rubidus-api.github.io/proven_c_book/ko/")[rubidus-api.github.io/proven_c_book/ko/]
- PDF — the `ko` asset of the current release

#pagebreak(weak: true)
#set heading(numbering: "1.1")
#counter(heading).update(0)

// ── Body ────────────────────────────────────────────
// Same skeleton as the Korean edition. Only translated chapters are included;
// the heading counter is set per chapter so numbering matches the original.
#let translated = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81)


#for (part-title, intro, chs) in parts {
  let have = chs.filter(c => c in translated)
  pagebreak(weak: true)
  align(center + horizon)[
    #text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 20pt, weight: "bold", part-title)
    #if intro != none and have.len() > 0 {
      v(1.2cm)
      block(width: 80%, align(left, include intro))
    }
    #if have.len() < chs.len() {
      v(1.0cm)
      block(width: 70%)[
        #set text(size: 10pt)
        #set par(justify: false, first-line-indent: 0em)
        #align(center)[
          Not yet translated:
          #chs.filter(c => c not in translated).map(str).join(", ")
          #linebreak()
          (read these in the Korean edition)
        ]
      ]
    }
  ]
  pagebreak(weak: true)
  for i in have {
    counter(heading).update(i - 1)
    let n = if i < 10 { "0" + str(i) } else { str(i) }
    include "chapters/ch" + n + ".typ"
  }
}

// ── Appendices ──────────────────────────────────────
#pagebreak(weak: true)
#align(center + horizon, text(font: ("Noto Sans", "Noto Sans CJK KR"), size: 20pt, weight: "bold", "Appendices"))
#pagebreak(weak: true)
#set heading(numbering: none)
#include "appendix/a1-operators.typ"
#include "appendix/a2-formats.typ"
#include "appendix/a3-conversions.typ"
#include "appendix/a4-reading.typ"
#include "appendix/a5-bibliography.typ"
#include "appendix/a6-grammar.typ"

// ── Index ───────────────────────────────────────────
#pagebreak(weak: true)
#include "back/index.typ"
