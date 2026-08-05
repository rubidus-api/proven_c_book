// 서술 장치 (RFC-0001 §3). 모든 본문 장치는 여기의 함수만 사용한다.


// ── 장치 라벨의 지역화 ────────────────────────────────
// 라벨은 여기 한 곳에만 둔다. 영어판 빌드는 `--input lang=en` 으로 고른다.
// 장치를 고치면 한국어판과 영어판이 함께 바뀐다 (사본을 만들지 않는다).
#let _lang = sys.inputs.at("lang", default: "ko")
#let _L = (
  ko: (q: "문", a: "답", back: "돌아보기", recap: "복습 정리",
       stdin: "표준 입력으로 준 것", output: "실행 결과",
       platform: "플랫폼 노트", organizer: "이 장이 끝나면",
       prereq: "이 장이 기대는 것", questions: "이 장에서 답할 질문"),
  en: (q: "Q", a: "A", back: "Looking back", recap: "Recap",
       stdin: "Given on standard input", output: "Output",
       platform: "Platform note", organizer: "By the end of this chapter",
       prereq: "What this chapter builds on", questions: "The questions this chapter answers"),
).at(_lang)

// 인쇄를 위해 채움(fill)을 쓰지 않는다 — 선의 굵기와 모양으로만 구분한다.
#let _device(title, body, rule, icon) = block(
  width: 100%,
  inset: (x: 11pt, y: 9pt),
  above: 1.15em, below: 1.15em,
  stroke: rule,
  breakable: true,
)[
  #set par(first-line-indent: 0em)
  #if title != none [
    #text(weight: "bold", size: 0.96em)[#icon #title]
    #v(4pt)
  ]
  #body
]

// ── 장 서두 4종 (RFC-0008 §2) ────────────────────────
// 넷은 하나의 시각 단위다: 굵은 왼쪽 세로선이 서두 전체를 묶고, 라벨은
// 회색 대문자풍 굵은 글씨로 통일한다. 아이콘은 쓰지 않는다(대체 글꼴 사고
// 방지). 안에서는 얇은 규칙선으로 칸을 가른다.
#let _open-rule = 3pt + rgb("#111111")
#let _open-label(t) = text(
  weight: "bold", size: 0.9em, fill: rgb("#3a3a3a"), tracking: 0.06em, upper(t),
)
#let _open-block(label, body, first: false) = block(
  width: 100%,
  inset: (left: 12pt, right: 2pt, top: if first { 7pt } else { 8pt }, bottom: 8pt),
  stroke: (left: _open-rule, top: if first { none } else { 0.4pt + rgb("#c8c8c8") }),
  above: if first { 1.1em } else { 0pt },
  below: 0pt,
  breakable: false,
)[
  #set par(first-line-indent: 0em, leading: 0.78em)
  #_open-label(label)
  #v(3pt)
  #body
]

// 3.1 문답 (즉문즉답) — 기본 리듬
#let qa(q, a) = block(width: 100%, above: 1.15em, below: 1.15em, breakable: true)[
  #set par(first-line-indent: 0em)
  #block(inset: (x: 11pt, y: 7pt), width: 100%, stroke: (left: 2pt + black), below: 6pt)[
    #metadata(q)<qa-q>
    #text(weight: "bold")[#_L.q] #h(5pt) #q
  ]
  #block(inset: (x: 11pt, y: 7pt), width: 100%, stroke: (left: 0.5pt + black))[
    #text(weight: "bold")[#_L.a] #h(5pt) #a
  ]
]

// 3.2 심화 문답 (장 서두 회고 전용)
// ② 인출 문답 — 선행 개념을 표시하는 데 그치지 않고 실제로 꺼내 보게 한다
#let deepqa(q, a) = _open-block(_L.back)[
  #block(below: 5pt, width: 100%)[#q]
  #block(width: 100%, inset: (left: 10pt),
    stroke: (left: 2pt + rgb("#999999")))[
    #text(weight: "bold", size: 0.92em, fill: rgb("#3a3a3a"))[#_L.a] #h(5pt) #a
  ]
]

// 3.3 오개념 블록: 그럴듯한 생각 → 왜 그럴듯한가 → 실제로는 → 확인
#let misconception(title, body) = _device(title, body, (left: 3pt + black), "⚠")

// 3.4 실제 사례 블록
#let realcase(title, body) = _device(title, body, (top: 0.5pt + black, bottom: 0.5pt + black), "◉")

// 반례 블록 (RFC-0004 §3): 독자의 생각이 아니라 *코드*가 틀린 경우.
// 오개념 블록(⚠)과 구별한다.
#let antipattern(title, body) = _device(title, body, (left: (thickness: 3pt, paint: black, dash: "dotted")), "✗")

// 3.5 수학 기반 박스 (건너뛰어도 본문이 이어지게 쓴다)
#let mathbox(title, body) = _device(title, body, (left: 1pt + black), "∑")

// (선택) 복습 정리 — 허용되는 유일한 복습 형태 (R17)
#let recap(body) = _device(_L.recap, body, 0.5pt + black, "☰")

// 3.6 코드 시연: 소스와 "실제 실행 결과"를 함께 인쇄한다.
// 출력은 scripts/verify-examples.sh 가 남긴 캡처 파일에서 읽는다 (수작업 전사 금지, R15).
//
// 예제 트리는 판마다 하나다 — 한국어판 `examples/`, 영어판 `examples-en/`
// (주석과 출력 문자열이 판마다 다르다). 캡처 디렉터리도 그에 맞춰 갈린다.
#let _out-dir(path) = if path.starts-with("examples-en/") {
  "/build/examples-out-en/"
} else {
  "/build/examples-out/"
}
#let _rel(path) = path.replace("examples-en/", "").replace("examples/", "")

#let demo(path, show-output: true, stdin: false, highlight: none) = block(breakable: true, width: 100%)[
  #block(width: 100%, inset: 8pt, stroke: 0.5pt + black)[
    #text(size: 0.96em, fill: rgb("#4a4a4a"), raw(path))
    #raw(read("/" + path), lang: "c", block: true)
  ]
  #if stdin [
    #block(width: 100%, inset: 8pt,
      stroke: (left: 2pt + black, rest: 0.5pt + black))[
      #text(size: 0.96em, weight: "bold")[#_L.stdin]
      #raw(read("/" + path.replace(".c", ".in")), block: true)
    ]
  ]
  #if show-output [
    #block(width: 100%, inset: 8pt,
      stroke: (left: 2pt + black, rest: 0.5pt + black))[
      #text(size: 0.96em, weight: "bold")[#_L.output]
      #raw(read(_out-dir(path) + _rel(path) + ".out"), block: true)
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
        let w = if i in highlight { 1.6pt } else { 0.5pt }
        stack(
          box(width: 3.2em, inset: 4pt, stroke: w + black,
            align(center, raw(c))),
          box(width: 3.2em, inset: (top: 3pt),
            align(center, text(size: 0.9em, raw(str(start + i))))),
        )
      })
    )
  ])
}

// 플랫폼 의존 격리 절: 특정 OS/도구에 묶인 내용은 반드시 이 상자 안에 둔다.
// 본문 일반론은 이 상자를 건너뛰어도 성립해야 한다.
#let platform(title, body) = block(
  width: 100%, inset: (x: 10pt, y: 7pt),
  stroke: (left: (thickness: 3pt, paint: black, dash: "densely-dotted"),
           rest: 0.5pt + black),
  breakable: true,
)[
  #set par(first-line-indent: 0em)
  #text(weight: "bold", size: 0.96em)[⊞ #_L.platform — #title]
  #v(2pt)
  #body
]

// ③ 이 장이 끝나면
#let organizer(body) = _open-block(_L.organizer, body)

// 서두를 닫는 굵은 규칙선 — 마지막 칸이 그린다
#let _open-close = block(width: 100%, above: 0pt, below: 1.3em)[
  #line(length: 100%, stroke: 1.2pt + rgb("#111111"))
]

// 3.7 기댄 것 (RFC-0006 §3.1) — 이 장이 어느 장의 무슨 개념 위에 서는가.
// 번호만 쓰지 않고 개념 이름을 함께 적는다. 항목은 (참조, 개념) 쌍이다.
// ① 이 장이 기대는 것 — 항목당 한 줄로 압축한다(재평가 §7.3: 서두가 길다)
#let prereq(..items) = _open-block(_L.prereq, first: true)[
  #for (where, what) in items.pos() {
    block(below: 5pt, width: 100%)[
      #text(weight: "bold")[#where]
      #h(5pt) #text(fill: rgb("#555555"))[·] #h(5pt)
      #text(fill: rgb("#333333"))[#what]
    ]
  }
]

// 3.8 이 장에서 답할 질문 (RFC-0006 §3.3) — 목록을 손으로 적지 않는다.
// 이 자리 뒤부터 다음 1단계 제목 전까지의 문답(qa)에서 질문만 모은다.
// 답은 싣지 않는다 — 독자가 잠시 생각할 자리를 만드는 것이 목적이다.
#let chapter-questions(min: 1) = context {
  let nexts = query(heading.where(level: 1).after(here()))
  let sel = selector(<qa-q>).after(here())
  let sel = if nexts.len() > 0 { sel.before(nexts.first().location()) } else { sel }
  let qs = query(sel)
  if qs.len() < min {
    // 질문이 없으면 서두를 여기서 닫는다
    _open-close
  } else {
    _open-block(_L.questions)[
      #for (i, m) in qs.enumerate() {
        block(below: 4pt, width: 100%, inset: (left: 14pt))[
          #place(left, dx: -14pt, text(fill: rgb("#777777"), weight: "bold")[#(i + 1)])
          #m.value
        ]
      }
    ]
    _open-close
  }
}

// ── 색인 ─────────────────────────────────────────────
// #idx("용어") 를 본문에 두면 그 자리의 쪽 번호가 색인에 실린다.
// 화면에는 아무것도 그리지 않는다(metadata).
#let idx(term) = [#metadata(term)<idx-entry>]

// 책 끝에서 호출한다. 모든 표시를 모아 가나다순으로 정리한다.
#let make-index(pages: true) = context {
  let entries = query(<idx-entry>)
  let terms = ()
  let hits = ()          // 표제어별 (쪽번호, 위치) 목록
  for e in entries {
    let term = e.value
    let loc = e.location()
    let p = if pages { counter(page).at(loc).first() } else { 0 }
    let i = terms.position(x => x == term)
    if i == none {
      terms.push(term)
      hits.push(((page: p, loc: loc),))
    } else if not pages or hits.at(i).filter(h => h.page == p).len() == 0 {
      hits.at(i).push((page: p, loc: loc))
    }
  }
  if not pages {
    // HTML 판: 레이아웃 함수(columns/grid)는 HTML 내보내기에서 버려지므로
    // 단순 목록으로 낸다. 표제어를 누르면 본문의 그 자리로 간다.
    return list(..terms.sorted().map(t => {
      let hs = hits.at(terms.position(x => x == t))
      link(hs.first().loc, t)
    }))
  }
  set par(justify: false, first-line-indent: 0em, leading: 0.62em)
  set text(size: 1.0em)
  // 쪽 번호를 누르면 본문의 그 자리로 간다.
  // (2026-08-05: 태그 PDF 가 링크마다 쪽을 늘린다고 보고 `--no-pdf-tags` 를
  //  썼으나, 재측정 결과 사실이 아니었다 — 태그판과 무태그판의 쪽 수는
  //  417 쪽으로 같다. 상류 이슈 typst/typst#8722 참고. 태그를 다시 켠다.)
  columns(2, gutter: 1.4em, {
    for t in terms.sorted() {
      let hs = hits.at(terms.position(x => x == t))
      block(width: 100%, below: 0.42em, grid(
        columns: (1fr, auto),
        column-gutter: 0.6em,
        align: (left, right),
        t,
        hs.map(h => link(h.loc, str(h.page))).join(", "),
      ))
    }
  })
}

// ── 모드 인지 표 ─────────────────────────────────────
// PDF 에서는 Typst table, HTML 에서는 진짜 <table> 로 나간다.
#let dtable(columns: 2, ..cells) = {
  let items = cells.pos()
  if sys.inputs.at("mode", default: "paged") == "html" {
    let rows = ()
    let i = 0
    while i < items.len() {
      rows.push(items.slice(i, calc.min(i + columns, items.len())))
      i = i + columns
    }
    html.elem("table", {
      let first = true
      for r in rows {
        let tag = if first { "th" } else { "td" }
        html.elem("tr", { for c in r { html.elem(tag, c) } })
        first = false
      }
    })
  } else {
    align(center, table(
      columns: columns,
      stroke: 0.4pt + black,
      inset: 5pt,
      ..items,
    ))
  }
}
