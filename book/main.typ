// Proven C Book — 조판 진입점. 빌드: scripts/build-book.sh
#import "lib.typ": *

// 이 책의 판 번호. 갱신할 때마다 여기만 고친다 (VERSION.md 와 함께).
#let book-version = "v0.1.0"
#let book-date = "2026년 8월"
#let book-updated = "2026-08-05"          // 최종 수정일
#let book-status = "초안(draft)"           // 판의 성격
#let html-mode = sys.inputs.at("mode", default: "paged") == "html"
#let book-repo = "https://github.com/rubidus-api"

#set document(title: "Proven C Book " + book-version, author: "rubidus")
#set page(paper: "a4", margin: (x: 2.2cm, y: 2.5cm), numbering: "1")
#set text(font: ("Noto Serif CJK KR",), size: 10.5pt, lang: "ko")
#set par(justify: true, leading: 0.78em, first-line-indent: (amount: 1em, all: true))
#show heading: set text(font: ("Noto Sans CJK KR",))
// 코드 글꼴 = D2Coding (비리가처판, 리가처도 명시적으로 끔)
#show raw: set text(font: "D2Coding", size: 0.92em, ligatures: false)
#set heading(numbering: "1.1")
#show heading.where(level: 1): it => pagebreak(weak: true) + it

// ── 표제 ──────────────────────────────────────────────
#page(numbering: none)[
  #v(3.2cm)
  #align(center)[
    #text(font: ("Noto Sans CJK KR",), size: 26pt, weight: "bold")[Proven C Book]
    #v(0.6cm)
    #text(size: 13pt)[proven 라이브러리에 기반한 모던 C 입문]
    #v(0.9cm)
    #box(inset: (x: 10pt, y: 5pt), radius: 3pt, fill: rgb("#f3e9e6"),
      stroke: 0.7pt + rgb("#b0483c"))[
      #text(size: 10pt, fill: rgb("#8a3226"), weight: "bold")[#book-status]
    ]
    #v(0.5cm)
    #text(size: 10.5pt)[#book-version · 최종 수정 #book-updated]
    #v(1.0cm)
    #text(size: 11pt)[rubidus]
    #v(0.25cm)
    #text(size: 9.5pt, fill: rgb("#555555"))[
      rubidus\@gmail.com #h(0.8em) #book-repo
    ]
  ]

  #v(1.2cm)
  #align(center, block(width: 82%)[
    #set text(size: 9.7pt)
    #set par(justify: false, first-line-indent: 0em, leading: 0.72em)
    #align(left)[
      *이 책이 하려는 것* — 문법의 목록이 아니라 *계약을 읽는 눈*을
      기르는 것. 그래서 C 문법보다 컴퓨터를 먼저 이야기하고, 문법은 그
      이야기가 요구할 때 꺼낸다.

      #v(0.35cm)
      *누구를 위한 책인가* — 프로그래밍을 처음 시작하는 사람, 그리고 문법은
      알지만 "그래서 이걸로 무엇을 어떻게 짜야 하는가"에서 막힌 사람.
      사전 지식은 필요 없고, 컴퓨터 앞에 앉아 있지 않아도 읽힌다.

      #v(0.35cm)
      *어떻게 읽히는가* — 연습문제도 "직접 해 보라"도 없다. 본문 곳곳의
      즉문즉답으로 진행하고, 복습은 다음 장 서두의 심화 문답이 대신한다.

      #v(0.35cm)
      *무엇을 다루는가* — 전산의 기본(비트·기억의 사다리·파이프라인·
      인코딩·부동소수점)에서 시작해, 첫 프로그램과 개발환경, 값과 흐름,
      기억과 포인터, 자료의 모양, 정밀과 계약, 여러 파일과 전처리기를
      지나, C가 반세기째 출하해 온 다섯 가지 버그와 그에 대한 답으로서의
      proven 라이브러리 매뉴얼까지. C23을 기본값으로 삼는다.

      #v(0.35cm)
      *약속* — 실린 모든 코드는 매 빌드마다 실제로 컴파일·실행되고, 인쇄된
      결과는 그 출력을 그대로 옮긴 것이다(GCC·Clang 교차 검증).
    ]
  ])
]

// ── 판권 ──────────────────────────────────────────────
#page(numbering: none)[
  #v(1fr)
  #set text(size: 9.5pt)
  #set par(justify: false, first-line-indent: 0em)

  *Proven C Book — proven 라이브러리에 기반한 모던 C 입문*

  지은이 rubidus \
  rubidus\@gmail.com · #book-repo

  #v(0.5cm)

  이 책의 *본문*은 크리에이티브 커먼즈
  저작자표시-비영리-동일조건변경허락 4.0 국제 라이선스(CC BY-NC-SA 4.0)에
  따라 이용할 수 있다. 출처를 밝히면 자유롭게 공유하고 고칠 수 있으나,
  영리 목적 이용은 허용되지 않으며, 고친 결과물에는 같은 라이선스를
  적용해야 한다. \
  #h(0.8em) https://creativecommons.org/licenses/by-nc-sa/4.0/

  #v(0.3cm)

  이 책에 수록된 *예제 코드*는 MIT 라이선스로 배포한다 — 배운 것을 자기
  프로그램에 제약 없이 가져다 쓸 수 있게 하려는 뜻이다. 예제에서 사용한
  proven 라이브러리는 그 자체의 라이선스를 따른다.

  #v(0.5cm)

  이 책의 모든 코드 시연은 실제로 컴파일·실행해 얻은 출력을 그대로
  인쇄한 것이다. 조판은 Typst로 했다.

  #v(0.5cm)

  *#book-version · #book-date* — #book-status \
  최종 수정 #book-updated

  #v(0.3cm)

  *이 책은 계속 고쳐진다.* 지금 읽고 있는 것은 위 번호의 판이고, 그
  뒤로도 오류 수정과 내용 보강이 이어진다. *가장 새로운 판과 그동안의
  변경 내역은 저자의 GitHub에 있다* — 오래된 사본을 들고 있다면 그쪽을
  먼저 확인하는 편이 좋다. 오류 신고와 수정 제안도 같은 자리에서 받는다.

  #h(0.8em) #book-repo
]

#outline(depth: 2)
#pagebreak()

#set heading(numbering: none)
#include "front/preface.typ"
#set heading(numbering: "1.1")
#counter(heading).update(0)
#pagebreak(weak: true)

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
  ("제10부 — 구성", none, (43, 44, 45, 46)),
  ("제11부 — proven — 검증된 기본기", "parts/part11.typ", (47, 48, 49, 50, 51, 52, 53, 54, 55, 56)),
  ("제12부 — 닫으며", none, (57, 58)),
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

// ── 부록 ─────────────────────────────────────────────
#pagebreak(weak: true)
#align(center + horizon, text(font: ("Noto Sans CJK KR",), size: 20pt, weight: "bold", "부록"))
#pagebreak(weak: true)
#set heading(numbering: none)
#include "appendix/a1-operators.typ"
#include "appendix/a2-formats.typ"
#include "appendix/a3-conversions.typ"
#include "appendix/a4-reading.typ"
#include "appendix/a5-bibliography.typ"

// ── 찾아보기 ─────────────────────────────────────────
#pagebreak(weak: true)
#include "back/index.typ"
