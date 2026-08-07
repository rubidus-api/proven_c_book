// Proven C Book — 조판 진입점. 빌드: scripts/build-book.sh
#import "lib.typ": *

// 이 책의 판 번호. 갱신할 때마다 여기만 고친다 (VERSION.md 와 함께).
#let book-version = "v0.25.0"
#let book-date = "2026년 8월"
#let book-updated = "2026-08-07"          // 최종 수정일
#let book-status = "초안(draft)"           // 판의 성격
#let html-mode = sys.inputs.at("mode", default: "paged") == "html"
#let book-repo = "https://github.com/rubidus-api"

#set document(title: "Proven C Book " + book-version, author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "ko")
#set par(justify: true, leading: 0.78em, first-line-indent: (amount: 1em, all: true))

// 바깥 주소(URL)로 가는 링크는 눈에 보이게 — 클릭할 수 있다는 표시다
// (저자 지시 2026-08-07). 안쪽 링크(차례·색인·상호 참조)는 그대로 둔다.
#show link: it => if type(it.dest) == str {
  text(fill: rgb("#1A4F8A"), it)
} else { it }
#show heading: set text(font: ("Noto Sans CJK KR",))
// 코드 글꼴 = D2Coding (비리가처판, 리가처도 명시적으로 끔)
#show raw: set text(font: "D2Coding", size: 0.96em, ligatures: false)
// 인쇄를 위해 구문 강조 색을 쓰지 않는다 (잉크·토너 절약)
// 회색조에서도 구분되는 절제된 구문 강조 (RFC-0006 §7.2)
#set raw(theme: "/book/theme-print.tmTheme")
// 표·그림 번호는 장마다 1부터 다시 센다 (lib.typ 의 float 카운터)
#show heading.where(level: 1): it => { reset-float-counters(); it }

#set heading(numbering: "1.1")
#show heading.where(level: 1): it => pagebreak(weak: true) + it
// 절 제목(1.2 꼴)은 위아래로 숨을 준다 — 기본값은 본문에 너무 붙는다
#show heading.where(level: 1): set block(below: 1.35em)
#show heading.where(level: 2): set block(above: 1.9em, below: 1.05em)
#show heading.where(level: 3): set block(above: 1.5em, below: 0.85em)

// ── 표제 ──────────────────────────────────────────────
#page(numbering: none)[
  #v(3.2cm)
  #align(center)[
    #text(font: ("Noto Sans CJK KR",), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[프로븐 C 라이브러리와 함께하는 현대적 C 입문]
    #v(0.9cm)
    #box(inset: (x: 10pt, y: 5pt), stroke: 1pt + black)[
      #text(size: 10pt, weight: "bold")[#book-status]
    ]
    #v(0.5cm)
    #text(size: 10.5pt)[#book-version · 최종 수정 #book-updated]
    #v(1.0cm)
    #text(size: 11pt)[rubidus]
    #v(0.25cm)
    #text(size: 10pt)[
      #link("mailto:rubidus@gmail.com")[rubidus\@gmail.com] #h(0.8em) #link("https://github.com/rubidus-api/proven_c_book")[github.com/rubidus-api/proven_c_book]
    ]
  ]

  #v(1.4cm)
  #align(center, block(width: 76%)[
    #set text(size: 10.5pt)
    #set par(justify: false, first-line-indent: 0em, leading: 0.85em)
    #align(center)[
      이 책은 C 언어 입문서와 proven C 라이브러리 소개를 겸하고 있습니다.

      #v(0.35cm)
      대상 독자는 C 언어에 막 입문하려는 초보자부터, \
      입문서를 막 뗀 중급자까지입니다.
    ]
  ])
]

// ── 본문 구성 ────────────────────────────────────────
// (제목, 부 도입부 파일 또는 none, 장 번호들)
#let parts = (
  ("제1부 — 바탕", none, (1, 2, 3)),
  ("제2부 — 전산의 기본과 배경지식", "parts/part02.typ", (4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14)),
  ("제3부 — 첫 프로그램", none, (15, 16, 17, 18)),
  ("제4부 — 최소한의 도구 상자", none, (19, 20, 21, 22)),
  ("제5부 — 선언: 이름을 만드는 법", none, (23, 24, 25)),
  ("제6부 — 값과 흐름", none, (26, 27, 28, 29, 30, 31, 32, 33)),
  ("제7부 — 기억", none, (34, 35, 36, 37, 38, 39, 40, 41, 42)),
  ("제8부 — 자료의 모양", none, (43, 44, 45)),
  ("제9부 — 깊은 구석들", none, (46, 47, 48, 49)),
  ("제10부 — 구성", none, (50, 51, 52, 53, 54, 55, 56, 57, 58)),
  ("제11부 — 표준 라이브러리 정독", "parts/part11s.typ", (59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80)),
  ("제12부 — proven — 검증된 기본기", "parts/part12.typ", (81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91)),
  ("제13부 — 닫으며", none, (92, 93, 94)),
)

#context {
  let heads = query(heading.where(level: 1)).filter(h => h.numbering != none)
  let by-num = (:)
  for h in heads {
    let n = counter(heading).at(h.location()).first()
    by-num.insert(str(n), h)
  }
  // 번호 없는 1단계 제목(머리말·부록·찾아보기)도 차례에 싣는다.
  // 본문 장의 쪽 범위와 견주어 앞부속과 뒷부속을 가른다.
  let plain = query(heading.where(level: 1)).filter(h => h.numbering == none)
  let ch-pages = heads.map(h => counter(page).at(h.location()).first())
  let first-ch = calc.min(..ch-pages)
  let last-ch = calc.max(..ch-pages)
  let page-of = h => counter(page).at(h.location()).first()
  let front-extra = plain.filter(h => page-of(h) < first-ch)
  let back-extra = plain.filter(h => page-of(h) > last-ch)
  // 차례 한 줄 — 제목과 쪽 번호를 양끝에 두고 눌러서 본문으로 간다
  // 제목과 쪽 번호는 가운데 점선으로 잇는다 (저자 지시 2026-08-06)
  let row = (label, h) => block(width: 100%, inset: (left: 1.2em), below: 0.72em)[
    #link(h.location())[#label]
    #box(width: 1fr, inset: (x: 0.5em),
      text(fill: rgb("#888888"), tracking: 0.35em, repeat[.]))
    #link(h.location())[#page-of(h)]
  ]

  block(width: 100%)[
    #set par(leading: 1.0em)
    #text(font: ("Noto Sans CJK KR",), size: 15pt, weight: "bold")[차례]
    #v(0.9em)
    #for h in front-extra { row(h.body, h) }
    #for (part-title, intro, chs) in parts {
      block(above: 1.5em, below: 0.7em, sticky: true)[
        #text(font: ("Noto Sans CJK KR",), size: 10.5pt, weight: "bold", part-title)
      ]
      for i in chs {
        let h = by-num.at(str(i), default: none)
        if h != none { row([#i. #h.body], h) }
      }
    }
    #if back-extra.len() > 0 {
      block(above: 1.5em, below: 0.7em, sticky: true)[
        #text(font: ("Noto Sans CJK KR",), size: 10.5pt, weight: "bold")[부록과 찾아보기]
      ]
      for h in back-extra { row(h.body, h) }
    }
  ]
}

#pagebreak()

#set heading(numbering: none)
#include "front/preface.typ"

// 저작권·연락은 머리말과 한 자리에 둔다 (저자 지시 2026-08-06).
// 판 번호는 여기서만 쓰므로 book-version 을 그대로 인용한다.
#[
  #set par(justify: false, first-line-indent: 0em, leading: 0.85em, spacing: 0.95em)
  #show link: it => text(fill: black, it)

  == 저작권과 연락처

  #metalist(
    ([지은이], [rubidus]),
    ([연락], link("mailto:rubidus@gmail.com")[rubidus\@gmail.com]),
    ([저장소], link("https://github.com/rubidus-api/proven_c_book")[github.com/rubidus-api/proven_c_book]),
    ([판], [#book-version — #book-status]),
    ([최종 수정], [#book-updated]),
  )

  #v(0.45cm)

  *본문* — 크리에이티브 커먼즈 저작자표시-비영리-동일조건변경허락 4.0 국제
  라이선스(CC BY-NC-SA 4.0). 출처를 밝히면 자유롭게 공유하고 고칠 수 있으나,
  영리 목적 이용은 허용되지 않으며, 고친 결과물에는 같은 라이선스를 적용해야
  합니다.
  #linebreak()
  #text(size: 10pt, fill: rgb("#555555"))[https://creativecommons.org/licenses/by-nc-sa/4.0/]

  *예제 코드* — MIT 라이선스. 자유롭게 가져다 쓰실 수 있습니다. 예제에서
  사용한 proven 라이브러리는 그 자체의 라이선스(현재는 MIT 라이선스)를
  따릅니다.

  이 책의 모든 코드 시연은 실제로 컴파일·실행해 얻은 출력을 그대로 인쇄한
  것입니다. 조판은 Typst로 했습니다.

  이 책은 계속 고쳐집니다. 지금 읽고 계신 것은 위 번호의 판이고, 그 뒤로도
  오류 수정과 내용 보강이 이어집니다. 가장 새로운 판과 그동안의 변경 내역은
  저자의 GitHub에 있습니다. 오래된 사본을 들고 계시다면 그쪽을 먼저 확인하시는
  편이 좋습니다. 오류 신고와 수정 제안도 같은 자리에서 받습니다.
]
#set heading(numbering: "1.1")
#counter(heading).update(0)
#pagebreak(weak: true)

// ── 본문 (13부 94장) ─────────────────────
// (제목, 부 도입부 파일 또는 none, 장 번호들)
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

// ── 부록 ─────────────────────────────────────────────
#pagebreak(weak: true)
#align(center + horizon, text(font: ("Noto Sans CJK KR",), size: 20pt, weight: "bold", "부록"))
#pagebreak(weak: true)
#set heading(numbering: none)
#include "appendix/a1-operators.typ"
#include "appendix/a2-formats.typ"
#include "appendix/a3-conversions.typ"
#include "appendix/a4-reading.typ"
#include "appendix/a6-grammar.typ"

// ── 찾아보기 ─────────────────────────────────────────
#pagebreak(weak: true)
#include "back/bibliography.typ"

#pagebreak(weak: true)
#include "back/index.typ"
