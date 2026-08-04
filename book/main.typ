// Proven C Book — 조판 진입점. 빌드: scripts/build-book.sh
#import "lib.typ": *

#set document(title: "Proven C Book", author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "ko")
#set par(justify: true, leading: 0.78em, first-line-indent: 1em)
#show heading: set text(font: ("Noto Sans CJK KR",))
#show raw: set text(size: 0.88em)
#set heading(numbering: "1.1")
#show heading.where(level: 1): it => pagebreak(weak: true) + it

// ── 표제 ──────────────────────────────────────────────
#page(numbering: none)[
  #v(6cm)
  #align(center)[
    #text(font: ("Noto Sans CJK KR",), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[proven 라이브러리에 기반한 모던 C 입문]
    #v(1.2cm)
    #text(size: 11pt)[rubidus]
  ]
]

#outline(depth: 2)
#pagebreak()

// ── 본문 (RFC-0002 5부 17장) ─────────────────────────
#include "chapters/ch01.typ"
#include "chapters/ch02.typ"
#include "chapters/ch03.typ"
#include "chapters/ch04.typ"
#include "chapters/ch05.typ"
#include "chapters/ch06.typ"
#include "chapters/ch07.typ"
#include "chapters/ch08.typ"
#include "chapters/ch09.typ"
#include "chapters/ch10.typ"
#include "chapters/ch11.typ"
#include "chapters/ch12.typ"
#include "chapters/ch13.typ"
#include "chapters/ch14.typ"
#include "chapters/ch15.typ"
#include "chapters/ch16.typ"
#include "chapters/ch17.typ"
