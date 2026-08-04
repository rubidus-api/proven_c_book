// Proven C Book — 조판 진입점. 빌드: scripts/build-book.sh
#import "lib.typ": *

#set document(title: "Proven C Book", author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "ko")
#set par(justify: true, leading: 0.78em, first-line-indent: 1em)
#show heading: set text(font: ("Noto Sans CJK KR",))
// 코드 글꼴 = D2Coding (비리가처판, 리가처도 명시적으로 끔)
#show raw: set text(font: "D2Coding", size: 0.92em, ligatures: false)
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

// ── 본문 (RFC-0002 rev.f 10부 45장) ──────────────────
// (제목, 부 도입부 파일 또는 none, 장 번호들)
#let parts = (
  ("제1부 — 바탕", none, (1,)),
  ("제2부 — 전산의 기본과 배경지식", "parts/part02.typ", (2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)),
  ("제3부 — 첫 프로그램", none, (13, 14, 15)),
  ("제4부 — 최소한의 도구 상자", none, (16, 17, 18, 19)),
  ("제5부 — 선언: 이름을 만드는 법", none, (20, 21, 22)),
  ("제6부 — 값과 흐름", none, (23, 24, 25, 26, 27, 28, 29)),
  ("제7부 — 기억", none, (30, 31, 32, 33, 34, 35, 36, 37)),
  ("제8부 — 자료의 모양", none, (38, 39)),
  ("제9부 — 정밀", none, (40, 41, 42)),
  ("제10부 — 구성", none, (43, 44, 45, 46, 47, 48)),
)
#for (part-title, intro, chs) in parts {
  pagebreak(weak: true)
  align(center + horizon)[
    #text(font: ("Noto Sans CJK KR",), size: 20pt, weight: "bold", part-title)
    #if intro != none {
      v(1.2cm)
      block(width: 80%, align(left, include intro))
    }
  ]
  pagebreak(weak: true)
  for i in chs {
    let n = if i < 10 { "0" + str(i) } else { str(i) }
    include "chapters/ch" + n + ".typ"
  }
}
