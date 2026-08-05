// 서술 장치 (RFC-0001 §3). 모든 본문 장치는 여기의 함수만 사용한다.

#let _device(title, body, accent, icon) = block(
  width: 100%,
  inset: (x: 10pt, y: 8pt),
  radius: 4pt,
  fill: accent.lighten(92%),
  stroke: (left: 2.5pt + accent),
  breakable: true,
)[
  #set par(first-line-indent: 0em)
  #if title != none [
    #text(fill: accent.darken(25%), weight: "bold", size: 0.92em)[#icon #title]
    #v(2pt)
  ]
  #body
]

// 3.1 문답 (즉문즉답) — 기본 리듬
#let qa(q, a) = block(width: 100%, inset: (y: 2pt), breakable: true)[
  #set par(first-line-indent: 0em)
  #block(inset: (x: 10pt, y: 6pt), radius: 4pt, width: 100%,
    fill: rgb("#f0f4fa"), stroke: (left: 2.5pt + rgb("#3b6ea5")))[
    #text(fill: rgb("#2a5080"), weight: "bold")[문] #h(4pt) #q
  ]
  #block(inset: (x: 10pt, y: 6pt), width: 100%)[
    #text(fill: rgb("#555555"), weight: "bold")[답] #h(4pt) #a
  ]
]

// 3.2 심화 문답 (장 서두 회고 전용)
#let deepqa(q, a) = block(width: 100%, inset: (y: 2pt), breakable: true)[
  #set par(first-line-indent: 0em)
  #block(inset: (x: 10pt, y: 6pt), radius: 4pt, width: 100%,
    fill: rgb("#f3eefa"), stroke: (left: 2.5pt + rgb("#7a4fa5")))[
    #text(fill: rgb("#5c3a80"), weight: "bold")[돌아보기] #h(4pt) #q
  ]
  #block(inset: (x: 10pt, y: 6pt), width: 100%)[
    #text(fill: rgb("#555555"), weight: "bold")[답] #h(4pt) #a
  ]
]

// 3.3 오개념 블록: 그럴듯한 생각 → 왜 그럴듯한가 → 실제로는 → 확인
#let misconception(title, body) = _device(title, body, rgb("#b0483c"), "⚠")

// 3.4 실제 사례 블록
#let realcase(title, body) = _device(title, body, rgb("#3c7a4f"), "◉")

// 3.5 수학 기반 박스 (건너뛰어도 본문이 이어지게 쓴다)
#let mathbox(title, body) = _device(title, body, rgb("#8a6d1a"), "∑")

// (선택) 복습 정리 — 허용되는 유일한 복습 형태 (R17)
#let recap(body) = _device("복습 정리", body, rgb("#666666"), "☰")

// 3.6 코드 시연: 소스와 "실제 실행 결과"를 함께 인쇄한다.
// 출력은 scripts/verify-examples.sh 가 남긴 캡처 파일에서 읽는다 (수작업 전사 금지, R15).
#let demo(path, show-output: true, stdin: false, highlight: none) = block(breakable: true, width: 100%)[
  #block(width: 100%, inset: 8pt, radius: 4pt, fill: rgb("#f6f6f4"),
    stroke: 0.5pt + rgb("#dddddd"))[
    #text(size: 0.8em, fill: rgb("#888888"), raw(path))
    #raw(read("/" + path), lang: "c", block: true)
  ]
  #if stdin [
    #block(width: 100%, inset: 8pt, radius: 4pt, fill: rgb("#f0f4fa"),
      stroke: 0.5pt + rgb("#b9c9de"))[
      #text(size: 0.8em, fill: rgb("#5577aa"))[표준 입력으로 준 것]
      #raw(read("/" + path.replace(".c", ".in")), block: true)
    ]
  ]
  #if show-output [
    #block(width: 100%, inset: 8pt, radius: 4pt, fill: rgb("#1e1e1e"))[
      #text(size: 0.8em, fill: rgb("#999999"))[실행 결과]
      #text(fill: rgb("#e8e8e8"), raw(read("/build/examples-out/" + path.replace("examples/", "") + ".out"), block: true))
    ]
  ]
]

// 메모리 사물함 도해: 주소 라벨 + 내용 셀 (+ 강조 칸 인덱스)
#let memrow(start, cells, highlight: ()) = {
  align(center, block(inset: (y: 6pt))[
    #grid(
      columns: cells.len(),
      column-gutter: 0pt,
      ..cells.enumerate().map(((i, c)) => {
        let bg = if i in highlight { rgb("#fdf0d5") } else { rgb("#fafafa") }
        stack(
          box(width: 3.2em, inset: 4pt, stroke: 0.7pt + rgb("#999999"), fill: bg,
            align(center, raw(c))),
          box(width: 3.2em, inset: (top: 3pt),
            align(center, text(size: 0.72em, fill: rgb("#777777"), raw(str(start + i))))),
        )
      })
    )
  ])
}

// 플랫폼 의존 격리 절: 특정 OS/도구에 묶인 내용은 반드시 이 상자 안에 둔다.
// 본문 일반론은 이 상자를 건너뛰어도 성립해야 한다.
#let platform(title, body) = block(
  width: 100%, inset: (x: 10pt, y: 8pt), radius: 4pt,
  fill: rgb("#eef3f2"), stroke: (left: 2.5pt + rgb("#3d7a78")),
  breakable: true,
)[
  #set par(first-line-indent: 0em)
  #text(fill: rgb("#2a5a58"), weight: "bold", size: 0.92em)[⊞ 플랫폼 노트 — #title]
  #v(2pt)
  #body
]

// 장 서두 선행조직자
#let organizer(body) = block(width: 100%, inset: (x: 10pt, y: 8pt), radius: 4pt,
  fill: rgb("#fafafa"), stroke: 0.5pt + rgb("#cccccc"))[
  #set par(first-line-indent: 0em)
  #text(weight: "bold", size: 0.92em)[이 장이 끝나면] #v(2pt) #body
]
