// Proven C Book — English edition (in progress). Build: scripts/build-book-en.sh
#import "../book/lib.typ": *

#let book-version = "v0.2.0"
#let book-updated = "2026-08-05"
#let book-status = "draft — translation in progress"
#let book-repo = "https://github.com/rubidus-api/proven_c_book"

#set document(title: "Proven C Book " + book-version, author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "en")
#set par(justify: true, leading: 0.78em, first-line-indent: (amount: 1em, all: true))
#show heading: set text(font: ("Noto Sans CJK KR",))
#show raw: set text(font: "D2Coding", size: 0.92em, ligatures: false)
// 인쇄를 위해 구문 강조 색을 쓰지 않는다 (잉크·토너 절약)
#set raw(theme: none)
#set heading(numbering: none)

#page(numbering: none)[
  #v(3.2cm)
  #align(center)[
    #text(font: ("Noto Sans CJK KR",), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[Modern C, built on the proven library]
    #v(0.9cm)
    #box(inset: (x: 10pt, y: 5pt), stroke: 1pt + black)[
      #text(size: 10pt, weight: "bold")[#book-status]
    ]
    #v(0.5cm)
    #text(size: 10.5pt)[#book-version · last updated #book-updated]
    #v(1.0cm)
    #text(size: 11pt)[rubidus]
    #v(0.25cm)
    #text(size: 9.5pt)[
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
  block(width: 100%)[
    #text(font: ("Noto Sans CJK KR",), size: 15pt, weight: "bold")[Contents]
    #v(0.5em)
    #for (part-title, intro, chs) in parts {
      block(above: 0.9em, below: 0.35em)[
        #text(font: ("Noto Sans CJK KR",), size: 10.5pt, weight: "bold", part-title)
      ]
      for i in chs {
        let h = by-num.at(str(i), default: none)
        if h != none {
          block(width: 100%, inset: (left: 1.2em), below: 0.2em,
            grid(columns: (1fr, auto), column-gutter: 0.6em, align: (left, right),
              link(h.location())[#i. #h.body],
              link(h.location())[#counter(page).at(h.location()).first()]))
        }
      }
    }
  ]
}

#pagebreak()

#set heading(numbering: none)
#include "front/preface.typ"

= A note on this translation

The Korean edition is the original and is complete — 13 parts, 69 chapters,
appendices A–E and an index, about 287 pages. This English edition is
translated from it chapter by chapter, and *chapter numbers are kept identical
to the original*, so a cross-reference to "chapter 54" means the same chapter
in both editions. Chapters not yet translated are listed where they belong,
and the parts they sit in are marked accordingly.

The two editions are kept in step mechanically: `scripts/sync-status.py`
records the hash of the Korean source each translated file was made from, and
every build reports any chapter whose original has changed since. Per-chapter
status is in `TRANSLATION.md` in the repository.

Until a chapter arrives here, the complete text is the Korean edition:

- Web — #link("https://rubidus-api.github.io/proven_c_book/ko/")[rubidus-api.github.io/proven_c_book/ko/]
- PDF — the `ko` asset of the current release

#pagebreak(weak: true)
#set heading(numbering: "1.1")
#counter(heading).update(0)

// ── Body ────────────────────────────────────────────
// Same skeleton as the Korean edition. Only translated chapters are included;
// the heading counter is set per chapter so numbering matches the original.
#let translated = (1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 80)


#for (part-title, intro, chs) in parts {
  let have = chs.filter(c => c in translated)
  pagebreak(weak: true)
  align(center + horizon)[
    #text(font: ("Noto Sans CJK KR",), size: 20pt, weight: "bold", part-title)
    #if intro != none and have.len() > 0 {
      v(1.2cm)
      block(width: 80%, align(left, include intro))
    }
    #if have.len() < chs.len() {
      v(1.0cm)
      block(width: 70%)[
        #set text(size: 9.5pt)
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
