// 서술 장치 (RFC-0001 §3). 모든 본문 장치는 여기의 함수만 사용한다.


// ── 장치 라벨의 지역화 ────────────────────────────────
// 라벨은 여기 한 곳에만 둔다. 영어판 빌드는 `--input lang=en` 으로 고른다.
// 장치를 고치면 한국어판과 영어판이 함께 바뀐다 (사본을 만들지 않는다).
#let _lang = sys.inputs.at("lang", default: "ko")
#let _html = sys.inputs.at("mode", default: "paged") == "html"
#let _L = (
  ko: (q: "문", a: "답", back: "돌아보기", recap: "복습 정리",
       stdin: "표준 입력으로 준 것", output: "실행 결과",
       platform: "플랫폼 노트", organizer: "이 장이 끝나면",
       prereq: "이 장이 기대는 것", questions: "이 장에서 답할 질문",
       misc: "흔한 오해", tbl: "표", fig: "그림"),
  en: (q: "Q", a: "A", back: "Looking back", recap: "Recap",
       stdin: "Given on standard input", output: "Output",
       platform: "Platform note", organizer: "By the end of this chapter",
       prereq: "What this chapter builds on", questions: "The questions this chapter answers",
       misc: "A common misconception", tbl: "Table", fig: "Figure"),
).at(_lang)

// ── 표·그림 번호와 캡션 (저자 지시 2026-08-06) ─────────
// 번호는 장마다 1부터 다시 센다(main.typ 의 show 규칙이 되돌린다).
// 캡션은 언제나 대상 *아래*, 가운데 정렬로 붙는다. 캡션 글이 없으면
// 번호만 인쇄한다 — 번호는 사람이 손으로 적지 않는다.
#let _tbl-no = counter("proven-table")
#let _fig-no = counter("proven-figure")
#let reset-float-counters() = {
  _tbl-no.update(0)
  _fig-no.update(0)
}
#let _float-caption(kind, ctr, cap) = {
  // step 과 get 은 같은 context 안에서 보면 안 된다 — 단계를 올린 뒤
  // *새* context 에서 읽어야 갱신된 값이 나온다(안 그러면 0 이 찍힌다).
  ctr.step()
  context {
    let chap = counter(heading.where(level: 1)).get()
    let chap = if chap.len() > 0 { str(chap.first()) } else { "0" }
    let label = kind + " " + chap + "." + str(ctr.get().first())
    if _html {
      html.elem("p", attrs: (class: "float-caption"),
        if cap == none { label } else { [#label — #cap] })
    } else {
      block(width: 100%, above: 6pt, below: 1.1em)[
        #align(center, text(font: ("Noto Sans CJK KR", "Noto Sans"), size: 0.96em)[
          #strong[#label]#if cap != none [ — #cap]])
      ]
    }
  }
}

// 인쇄를 위해 채움(fill)을 쓰지 않는다 — 선의 굵기와 모양으로만 구분한다.
#let _device(title, body, rule, icon) = block(
  width: 100%,
  inset: (x: 10pt, y: 8pt),
  above: 1.05em, below: 1.05em,
  stroke: rule,
  breakable: true,
)[
  #set par(first-line-indent: 0em)
  #if title != none [
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold",
          size: 0.98em, fill: black)[#icon #title]
    #v(4pt)
  ]
  #body
]

// ── 장 서두 4종 (RFC-0008 §2) ────────────────────────
// 넷은 하나의 시각 단위다: 굵은 왼쪽 세로선이 서두 전체를 묶고, 라벨은
// 회색 대문자풍 굵은 글씨로 통일한다. 아이콘은 쓰지 않는다(대체 글꼴 사고
// 방지). 안에서는 얇은 규칙선으로 칸을 가른다.
#let _open-rule = 3pt + rgb("#111111")
// 서두 라벨은 본문(명조)과 확실히 갈라 보이도록 고딕·크게·굵게 쓴다.
// 밑선은 두지 않는다 — 고딕 굵은 글씨만으로 충분히 구별된다
// (저자 지시 2026-08-06).
#let _open-label(t) = block(below: 7pt, width: 100%)[
  #text(
    font: ("Noto Sans CJK KR", "Noto Sans"),
    weight: "bold", size: 1.15em, fill: black, tracking: 0.01em, t,
  )
]
#let _open-block(label, body, first: false) = block(
  width: 100%,
  inset: (left: 12pt, right: 2pt, top: if first { 7pt } else { 8pt }, bottom: 8pt),
  stroke: (left: _open-rule, top: if first { none } else { 0.4pt + rgb("#c8c8c8") }),
  above: if first { 1.1em } else { 0pt },
  below: 0pt,
  breakable: false,
)[
  #set par(first-line-indent: 0em, leading: 0.85em, spacing: 0.55em)
  #set block(spacing: 0.55em)
  #_open-label(label)
  #body
]

// 3.1 문답 (즉문즉답) — 기본 리듬
// 3.1 문답 — 문과 답은 *하나의 덩어리*다. 왼쪽 세로선 하나가 둘을 잇고,
// 질문 줄은 굵은 고딕과 옅은 바탕으로 도드라지게 한다 (저자 지시 2026-08-06).
#let _qa_rail = 2.5pt + rgb("#111111")
#let _qa(label_q, label_a, q, a) = block(
  width: 100%, above: 1.25em, below: 1.25em, breakable: true,
  stroke: (left: _qa_rail), inset: (left: 0pt),
)[
  #set par(first-line-indent: 0em)
  // 문과 답은 하나의 덩어리다 — 사이에 빈칸을 두지 않고, 왼쪽 굵은 선
  // 하나가 둘을 통째로 잇는다 (저자 지시 2026-08-06).
  #block(width: 100%, inset: (x: 9pt, top: 6pt, bottom: 6pt),
         above: 0pt, below: 0pt,
         stroke: (bottom: 0.5pt + rgb("#999999")))[
    #metadata(q)<qa-q>
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold", size: 1em)[
      #label_q. #h(2pt) #q]
  ]
  #block(width: 100%, inset: (x: 9pt, top: 7pt, bottom: 2pt), above: 0pt)[
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold",
          size: 0.98em, fill: rgb("#444444"))[#label_a.]
    #h(3pt) #a
  ]
]
#let qa(q, a) = _qa(_L.q, _L.a, q, a)

// 3.2 심화 문답 (장 서두 회고 전용)
// ② 인출 문답 — 선행 개념을 표시하는 데 그치지 않고 실제로 꺼내 보게 한다
#let deepqa(q, a) = _open-block(_L.back)[
  #block(width: 100%)[#q]
  #block(width: 100%, inset: (left: 9pt),
    stroke: (left: 0.6pt + black))[
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold",
          size: 0.98em, fill: rgb("#3a3a3a"))[#_L.a.] #h(3pt) #a
  ]
]

// 3.3 오개념 블록: 그럴듯한 생각 → 왜 그럴듯한가 → 실제로는 → 확인
// 3.3 오개념 — "그럴듯한 생각"이 한눈에 들어와야 교정이 일어난다.
// 제목(오개념 문장)을 인용부호와 함께 크게·굵게 세우고, 굵은 테두리로 감싼다.
#let misconception(title, body) = block(
  width: 100%, above: 1.25em, below: 1.25em, breakable: true,
  stroke: 0.5pt + rgb("#111111"),
  inset: 0pt,
)[
  #set par(first-line-indent: 0em)
  #block(width: 100%, inset: (x: 11pt, y: 8pt), below: 0pt,
         stroke: (bottom: 0.5pt + rgb("#111111")))[
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold",
          size: 1em, fill: black)[#_L.misc.]
    #h(4pt)
    #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold", size: 1.0em)[#title]
  ]
  #block(width: 100%, inset: (x: 11pt, y: 9pt), above: 0pt)[#body]
]

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
      #text(font: ("Noto Sans CJK KR", "Noto Sans"), size: 0.96em, weight: "bold")[#_L.stdin]
      #raw(read("/" + path.replace(".c", ".in")), block: true)
    ]
  ]
  #if show-output [
    #block(width: 100%, inset: 8pt,
      stroke: (left: 2pt + black, rest: 0.5pt + black))[
      #text(font: ("Noto Sans CJK KR", "Noto Sans"), size: 0.96em, weight: "bold")[#_L.output]
      #raw(read(_out-dir(path) + _rel(path) + ".out"), block: true)
    ]
  ]
]

// 메모리 사물함 도해: 주소 라벨 + 내용 셀 (+ 강조 칸 인덱스)
//
// ★ HTML 로도 반드시 나가야 한다. grid·stack·box 는 조판 전용이라
//   HTML 내보내기에서 아무것도 그리지 않는다 — 5장의 그림들이 캡션만 남고
//   사라졌던 원인이다(저자 지적 2026-08-06). HTML 에서는 표로 낸다.
#let memrow(start, cells, highlight: (), caption: none) = {
  if _html {
    html.elem("div", attrs: (class: "memrow"), {
      html.elem("table", attrs: (class: "mem"), {
        html.elem("tr", {
          for (i, c) in cells.enumerate() {
            html.elem("td",
              attrs: (class: if i in highlight { "cell hi" } else { "cell" }),
              raw(c))
          }
        })
        html.elem("tr", {
          for (i, _) in cells.enumerate() {
            html.elem("td", attrs: (class: "addr"), raw(str(start + i)))
          }
        })
      })
    })
  } else {
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
              align(center, text(size: 0.96em, raw(str(start + i))))),
          )
        })
      )
    ])
  }
  _float-caption(_L.fig, _fig-no, caption)
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
  #text(font: ("Noto Sans CJK KR", "Noto Sans"), weight: "bold",
        size: 0.98em, fill: black)[⊞ #_L.platform — #title]
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
    block(width: 100%)[
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
        block(width: 100%, inset: (left: 14pt))[
          #place(left, dx: -14pt, text(fill: rgb("#777777"), weight: "bold")[#(i + 1)])
          #m.value
        ]
      }
    ]
    _open-close
  }
}

// 3.9 개념도 — 공간 관계가 본질인 자리에만 쓴다(생성기: scripts/make-figures.py).
// 그림 파일은 판마다 따로다: book/figures/{ko,en}/<이름>.svg
#let figure-svg(name, caption: none, width: 100%) = {
  if _html {
    // Typst 의 HTML 내보내기는 이미지를 아직 내지 않는다 — 직접 <figure> 를 낸다.
    // 그림 파일은 wrap-html.py 가 docs/<판>/figures/ 로 복사한다.
    html.elem("figure", attrs: (class: "fig"), {
      html.elem("img", attrs: (src: "figures/" + name + ".svg", alt: name, loading: "lazy"))
      _float-caption(_L.fig, _fig-no, caption)
    })
  } else {
    block(width: 100%, above: 1.3em, below: 1.3em, breakable: false)[
      #align(center, image("/book/figures/" + _lang + "/" + name + ".svg", width: width))
      #_float-caption(_L.fig, _fig-no, caption)
    ]
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
#let dtable(columns: 2, caption: none, keycol: auto, ..cells) = {
  let items = cells.pos()
  if sys.inputs.at("mode", default: "paged") == "html" {
    let rows = ()
    let i = 0
    while i < items.len() {
      rows.push(items.slice(i, calc.min(i + columns, items.len())))
      i = i + columns
    }
    html.elem("figure", attrs: (class: "tbl"), {
      html.elem("table", {
        let first = true
        for r in rows {
          let tag = if first { "th" } else { "td" }
          html.elem("tr", { for c in r { html.elem(tag, c) } })
          first = false
        }
      })
      _float-caption(_L.tbl, _tbl-no, caption)
    })
  } else {
    // 굵은 테두리는 표를 감싸는 블록이 아니라 *표 자신의 바깥 선*이어야
    // 한다. 블록으로 감싸면 테두리만 단 너비를 차지하고 표는 제 너비만
    // 차지해 둘이 따로 논다 (저자 지시 2026-08-06).
    //
    // 머리행(1행)은 산세리프 굵은 글씨 + 옅은 음영으로 포인트를 준다.
    // 열이 셋 이상이면 1열도 키 노릇을 하므로 같은 처리를 한다
    // (`keycol: true|false` 로 강제할 수 있다).
    let rows = calc.ceil(items.len() / columns)
    let thick = 1.2pt + rgb("#111111")
    let thin = 0.4pt + rgb("#999999")
    let key = if keycol == auto { columns >= 3 } else { keycol }
    let sans = ("Noto Sans CJK KR", "Noto Sans")
    align(center, table(
      columns: columns,
      inset: 5pt,
      fill: (col, row) => if row == 0 { rgb("#ececec") }
        else if key and col == 0 { rgb("#f5f5f5") } else { none },
      stroke: (x, y) => (
        left: if x == 0 { thick } else { thin },
        right: if x == columns - 1 { thick } else { none },
        top: if y == 0 { thick } else if y == 1 { 0.9pt + rgb("#111111") } else { thin },
        bottom: if y == rows - 1 { thick } else { none },
      ),
      ..items.enumerate().map(((i, c)) => if i < columns {
        text(font: sans, weight: "bold", c)
      } else if key and calc.rem(i, columns) == 0 {
        text(font: sans, weight: "bold", size: 0.98em, c)
      } else { c }),
    ))
    _float-caption(_L.tbl, _tbl-no, caption)
  }
}
