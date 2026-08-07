#!/usr/bin/env python3
"""Typst 의 실험적 HTML 출력을 장별 페이지로 나누고 읽기 좋게 꾸민다."""
import sys, re, pathlib, html as htmlmod

lang, src, out_dir = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3])
raw = pathlib.Path(src).read_text(encoding="utf-8")
m = re.search(r"<body[^>]*>(.*)</body>", raw, re.S)
body = m.group(1) if m else raw

STR = {
 "ko": dict(title="Proven C Book — 프로븐 C 라이브러리와 함께하는 현대적 C 입문",
   main="Proven C Book", sub="프로븐 C 라이브러리와 함께하는 현대적 C 입문",
   updated="최종 수정", author="rubidus",
   blurb=["이 책은 C 언어 입문서와 proven C 라이브러리 소개를 겸하고 있습니다.",
          "대상 독자는 C 언어에 막 입문하려는 초보자부터,<br>입문서를 막 뗀 중급자까지입니다."],
   colophon="판권 · 저작권 · 연락", lbl_author="지은이", lbl_contact="연락",
   lbl_repo="저장소", lbl_edition="판", lbl_updated="최종 수정",
   short="Proven C Book", other="English", other_href="../en/",
   toc="목차", prev="이전", next="다음", top="목차로", extra="부록과 찾아보기",
   note=('이 판은 <strong>초안(draft)</strong>이다. 쪽 번호가 붙은 찾아보기와 정확한 조판은 '
         '<a href="https://github.com/rubidus-api/proven_c_book/tree/main/dist">PDF</a>를 보라.')),
 "en": dict(title="Proven C Book — An Introduction to Modern C with the Proven C Library",
   main="Proven C Book", sub="An Introduction to Modern C with the Proven C Library",
   updated="last updated", author="rubidus",
   blurb=["An introduction to C, and an introduction to the proven C library.",
          "Written for readers who are just starting out with C."],
   colophon="Colophon — copyright and contact", lbl_author="author", lbl_contact="contact",
   lbl_repo="repository", lbl_edition="edition", lbl_updated="last updated",
   short="Proven C Book", other="한국어", other_href="../ko/",
   toc="Contents", prev="Prev", next="Next", top="Contents", extra="Front and back matter",
   note=('This edition is a <strong>draft</strong>. For the paginated index and exact '
         'typesetting, see the '
         '<a href="https://github.com/rubidus-api/proven_c_book/tree/main/dist">PDF</a>.')),
}[lang]

SRC_MAIN = (pathlib.Path(__file__).resolve().parent.parent
            / ("book" if lang == "ko" else "book-en") / "main.typ")

def book_meta():
    """판 번호·성격·최종 수정일의 단일 출처는 main.typ 이다 — 여기에 베끼지 않는다."""
    txt = SRC_MAIN.read_text(encoding="utf-8")
    def one(name):
        m = re.search(r'#let\s+' + name + r'\s*=\s*"([^"]*)"', txt)
        return m.group(1) if m else ""
    return one("book-version"), one("book-status"), one("book-updated")

body = re.sub(r'<ol style="list-style-type: none">.*?</ol>\s*(?=<h2)', "", body, count=1, flags=re.S)

# 아이콘이 붙는 장치는 언어와 무관하고, 글자 라벨만 판마다 다르다
# (라벨의 단일 출처는 book/lib.typ 의 _L 이다).
DEVICES = [("이 장이 끝나면","organizer"),("BY THE END OF THIS CHAPTER","organizer"),
           ("By the end of this chapter","organizer"),
           ("먼저 알아야 할 것","prereq"),("WHAT THIS CHAPTER BUILDS ON","prereq"),
           ("이 장에서 답할 질문","questions"),("THE QUESTIONS THIS CHAPTER ANSWERS","questions"),
           ("돌아보기","deepqa"),("LOOKING BACK","deepqa"),("Looking back","deepqa"),
           ("흔한 오해","misconception"),("A common misconception","misconception"),
           # 아이콘 대신 낱말 접두어를 쓰는 서식 (2026-08-06). 옛 아이콘도 함께 받는다.
           ("실제 사례","realcase"),("In practice","realcase"),
           ("반례","antipattern"),("Counter-example","antipattern"),
           ("수학","mathbox"),("The mathematics","mathbox"),
           ("복습 정리","recap"),("Recap","recap"),
           ("플랫폼 노트","platform"),("Platform note","platform"),
           ("⚠","misconception"),("◉","realcase"),("✗","antipattern"),
           ("∑","mathbox"),("☰","recap"),("⊞","platform"),
           ("문","qa-q"),("답","qa-a"),("Q","qa-q"),("A","qa-a")]

def _is_label(text, prefix):
    """라벨 판정: 정확히 같거나, 라벨 뒤에 공백이나 마침표가 와야 한다.
    접두 일치만 보면 `문`이 "문자열…"을, `A` 가 "A bundle…"을 물어 버린다.
    아이콘(⚠·◉ 등)은 글자가 아니므로 뒤에 무엇이 오든 라벨로 본다.
    ★ "문."·"답." 처럼 표지에 마침표가 붙는 서식(2026-08-06)을 함께 받는다."""
    if text == prefix:
        return True
    if not text.startswith(prefix):
        return False
    last = prefix[-1]
    if not (last.isalnum() or "가" <= last <= "힣"):
        return True
    return text[len(prefix):len(prefix) + 1] in (" ", "\u00a0", ".")


def _split_label(first_html, prefix):
    """첫 문단이 "답. 본문…" 처럼 표지와 본문을 함께 담은 경우, 표지만
    떼어 `<span class="dev-label">` 으로 감싸고 나머지는 그대로 둔다.
    표지만 있는 문단이면 None 을 돌려준다(기존 경로를 쓴다)."""
    plain = re.sub(r"<[^>]+>", "", first_html).strip()
    if plain == prefix or plain == prefix + ".":
        return None
    rest = first_html.lstrip()
    if not rest.startswith(prefix):
        return None
    rest = rest[len(prefix):]
    mark = prefix
    if rest.startswith("."):
        rest, mark = rest[1:], prefix + "."
    return f'<p><span class="dev-label">{mark}</span> {rest.lstrip()}</p>'


def tag_devices(text):
    """장치 상자에 클래스를 붙인다.

    ★ 상자 안에 <div> 가 중첩될 수 있다(본문 들여쓰기를 위해 블록을 하나 더
      감싼다). 그래서 단순 정규식이 아니라 여는/닫는 <div> 의 짝을 센다 —
      2026-08-06 에 이 중첩 때문에 72장의 실제 사례·플랫폼 노트·복습 정리가
      통째로 서식을 잃었다."""
    out, i = [], 0
    while True:
        k = text.find("<div>", i)
        if k == -1:
            out.append(text[i:]); break
        inner_start = k + 5
        end = _div_end(text, inner_start)          # 짝이 맞는 </div> 뒤
        inner = text[inner_start:end - 6]
        out.append(text[i:k])

        first = re.search(r"<p>(.*?)</p>", inner, re.S)
        # 표제는 *직계* 문단이어야 한다. 안쪽 <div> 속의 문단을 표제로 오인하면
        # 바깥 상자가 통째로 잘못 분류된다(문답이 통째로 질문이 되는 사고).
        nested = inner.find("<div")
        if first and nested != -1 and nested < first.start():
            first = None
        tagged = None
        if first:
            label = re.sub(r"<[^>]+>", "", first.group(1)).strip()
            for prefix, cls in DEVICES:
                if _is_label(label, prefix):
                    merged = _split_label(first.group(1), prefix)
                    if merged is not None:
                        head = inner[:first.start()] + merged + inner[first.end():]
                    else:
                        head = re.sub(r"<p>(.*?)</p>", r'<p class="dev-label">\1</p>',
                                      inner, count=1, flags=re.S)
                    tagged = f'<div class="dev {cls}">{head}</div>'
                    break
        else:
            plain = re.sub(r"<[^>]+>", "", inner).strip()
            for prefix, cls in DEVICES:
                if plain.startswith(prefix):
                    rest = inner.replace(prefix, "", 1).lstrip()
                    tagged = (f'<div class="dev {cls}">'
                              f'<p class="dev-label">{prefix}</p><p>{rest}</p></div>')
                    break

        if tagged is None:
            # 이 <div> 는 장치가 아니다 — 안쪽을 다시 훑는다
            out.append("<div>")
            i = inner_start
        else:
            out.append(tagged)
            i = end
    return "".join(out)


# ── 구문 강조 — 조판 테마(book/theme-print.tmTheme)를 켠 뒤로 Typst 의 HTML
#    내보내기가 <strong>·<em>·인라인 색을 직접 낸다. 여기서는 그 인라인 색만
#    CSS 변수 클래스로 바꿔 다크 모드에서도 읽히게 한다.
_TOK_BY_COLOR = {
    "#3f5b4a": "tok-c",   # 주석
    "#123c87": "tok-k",   # 키워드
    "#0f5b63": "tok-t",   # 타입 이름
    "#7a3b0a": "tok-s",   # 문자열
    "#8a2222": "tok-n",   # 수·상수
    "#6b2d8a": "tok-p",   # 전처리기
    "#555555": "tok-x",   # 구두점·연산자
    "#5f5f5f": "tok-c",   # (옛 테마 잔재)
    "#1f4d7a": "tok-s",
    "#141414": None,
}

def _map_colors(mo):
    block = mo.group(0)
    def one(m):
        cls = _TOK_BY_COLOR.get(m.group(1).lower(), None)
        return f'<span class="{cls}">' if cls else "<span>"
    return re.sub(r'<span style="color: (#[0-9a-fA-F]{6})">', one, block)

body = re.sub(r"<pre>.*?</pre>", _map_colors, body, flags=re.S)

# ── 장 서두 4종은 중첩 <div> 를 담으므로 짝을 세어 잡는다 (RFC-0008 §2.4)
_OPEN_LABELS = {
    "먼저 알아야 할 것": "prereq", "WHAT THIS CHAPTER BUILDS ON": "prereq",
    "돌아보기": "deepqa", "LOOKING BACK": "deepqa",
    "이 장이 끝나면": "organizer", "BY THE END OF THIS CHAPTER": "organizer",
    "이 장에서 답할 질문": "questions", "THE QUESTIONS THIS CHAPTER ANSWERS": "questions",
}

def tag_openings(text):
    out, i = [], 0
    while True:
        m = re.compile(r"<div><p>([^<]{1,60})</p>").search(text, i)
        if not m:
            out.append(text[i:]); break
        cls = _OPEN_LABELS.get(htmlmod.unescape(m.group(1)).strip())
        if cls is None:
            out.append(text[i:m.end()]); i = m.end(); continue
        # 여는 <div> 의 짝을 센다
        depth, j = 1, m.end()
        while depth and j < len(text):
            nxt_open = text.find("<div", j)
            nxt_close = text.find("</div>", j)
            if nxt_close == -1: break
            if nxt_open != -1 and nxt_open < nxt_close:
                depth += 1; j = nxt_open + 4
            else:
                depth -= 1; j = nxt_close + 6
        inner = text[m.end():j - 6]
        out.append(text[i:m.start()])
        out.append(f'<div class="dev {cls}"><p class="dev-label">{m.group(1)}</p>{inner}</div>')
        i = j
    return "".join(out)

# ── 묶기: 서두 4종은 하나의 테두리를 공유하고, 문답은 한 상자 안에서
#    가로선으로 갈린다 (저자 지시 2026-08-06, HTML 전용).
def _div_end(text, start):
    """`start`(여는 <div ...> 다음 위치)에서 짝이 맞는 </div> 뒤 위치."""
    depth, j = 1, start
    while depth and j < len(text):
        o = text.find("<div", j)
        c = text.find("</div>", j)
        if c == -1:
            return len(text)
        if o != -1 and o < c:
            depth += 1; j = o + 4
        else:
            depth -= 1; j = c + 6
    return j


def _wrap_runs(text, classes, wrapper, outer=False, pair_only=False):
    """연속한 장치들을 하나의 <div class=wrapper> 로 감싼다.

    `outer=True` 면 장치를 감싸고 있는 바깥 <div> 단위로 묶는다 — 서두 4종은
    표지 div 와 내용 div 가 바깥 div 하나에 함께 들어 있기 때문이다."""
    body_re = r'<div class="dev (' + "|".join(classes) + r')">'
    open_re = re.compile((r"<div>\s*" if outer else "") + body_re)
    out, i = [], 0
    while True:
        m = open_re.search(text, i)
        if not m:
            out.append(text[i:]); break
        out.append(text[i:m.start()])
        run_start = m.start()
        end = _div_end(text, m.start() + 5 if outer else m.end())
        seen = [m.group(1)]
        while True:
            gap = re.match(r"\s*", text[end:]).end()
            n = open_re.match(text, end + gap)
            if not n:
                break
            end = _div_end(text, n.start() + 5 if outer else n.end())
            seen.append(n.group(1))
        if pair_only and len(seen) < 2:
            out.append(text[run_start:end]); i = end; continue
        out.append(f'<div class="{wrapper}">' + text[run_start:end] + "</div>")
        i = end
    return "".join(out)


_DEMO_SRC = re.compile(r"<div><p><code>examples(?:-en)?/")
_DEMO_OUT_LABELS = ("실행 결과", "Output", "표준 입력으로 준 것",
                    "Given on standard input")


def tag_demos(text):
    """시연 상자(소스·입력·출력)에 클래스를 붙인다 — 선명한 테두리를 주려고."""
    out, i = [], 0
    while True:
        k = text.find("<div>", i)
        if k == -1:
            out.append(text[i:]); break
        body_start = k + 5
        head = text[body_start:body_start + 120]
        cls = None
        if _DEMO_SRC.match(text[k:k + 130]):
            cls = "demo-src"
        else:
            plain = re.sub(r"<[^>]+>", "", head).strip()
            if any(plain.startswith(lbl) for lbl in _DEMO_OUT_LABELS):
                cls = "demo-out"
        out.append(text[i:k])
        out.append(f'<div class="{cls}">' if cls else "<div>")
        i = body_start
    return "".join(out)

body = tag_openings(body)

body = tag_devices(body)
body = tag_demos(body)
# 서두 표지 뒤에 남는 빈 문단 제거 — 헛여백의 원인이다 (빈 문단 제거)
body = re.sub(r"<p>\s*</p>", "", body)
# 표는 제 폭만 쓰고 가운데 정렬한다. 넘칠 때만 감싼 상자가 가로로 스크롤한다.
body = re.sub(r"(<figure class=\"tbl\">)(<table>.*?</table>)",
              r'\1<div class="tblwrap">\2</div>', body, flags=re.S)

parts = re.split(r'(<h2[^>]*>.*?</h2>)', body, flags=re.S)
front = parts[0]
chapters = []
for i in range(1, len(parts), 2):
    head = parts[i]; content = parts[i+1] if i+1 < len(parts) else ""
    # 제목은 순수 텍스트로 보관한다. 내보낸 HTML 에서 뽑은 것이라 이미
    # 이스케이프되어 있으므로 한 번 되돌린다 — 그러지 않으면 목차와 <title>
    # 에서 `<stdio.h>` 가 `&lt;stdio.h>` 로 이중 이스케이프된다.
    chapters.append((htmlmod.unescape(re.sub(r"<[^>]+>", "", head)).strip(), head + content))

CSS = """
:root { color-scheme: light dark; --fg:#141414; --bg:#ffffff; --muted:#5c5c5c;
  --rule:#d6d6d6; --box-bg:#f6f6f6; --box-rule:#9a9a9a; --code-bg:#f2f2f2; --link:#1a4f8a;
  --tok-c:#3f5b4a; --tok-k:#123c87; --tok-t:#0f5b63; --tok-s:#7a3b0a;
  --tok-n:#8a2222; --tok-p:#6b2d8a; --tok-x:#555555; }
@media (prefers-color-scheme: dark) { :root { --fg:#e8e8e8; --bg:#121314; --muted:#a6a6a6;
  --rule:#333; --box-bg:#1c1e20; --box-rule:#6d6d6d; --code-bg:#1b1d1f; --link:#8ab4e8;
  --tok-c:#9fbfa8; --tok-k:#9fc0f0; --tok-t:#7fcbd4; --tok-s:#e0b080;
  --tok-n:#e79a9a; --tok-p:#c79ae0; --tok-x:#a6a6a6; } }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--fg); line-height:1.75;
       font-family:"Noto Serif CJK KR","Noto Serif KR",Georgia,serif; }
.bar { position:sticky; top:0; z-index:20; display:flex; gap:.9rem; align-items:center;
       padding:.55rem clamp(.8rem,3vw,2.5rem); background:var(--bg);
       border-bottom:1px solid var(--rule); font-size:.86rem;
       font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.bar a { color:var(--link); text-decoration:none; } .bar a:hover { text-decoration:underline; }
.bar .sp { flex:1 1 auto; }
main { width:100%; padding:1.6rem clamp(.9rem,3vw,2.5rem) 5rem; }
/* 폭을 가두지 않는다 — 보통의 웹 문서처럼 화면 가로를 그대로 쓴다. */
.wrap { width:100%; max-width:none; margin:0; }
h1,h2,h3,h4 { font-family:"Noto Sans CJK KR",system-ui,sans-serif; line-height:1.35; }
h1 { font-size:clamp(1.5rem,3.4vw,2.1rem); margin:.2rem 0 .9rem; }
h2 { font-size:clamp(1.35rem,3vw,1.8rem); margin:0 0 .7rem; padding-bottom:.35rem;
     border-bottom:2px solid var(--fg); }
h3 { font-size:clamp(1.05rem,2.2vw,1.25rem); margin:1.5rem 0 .5rem; padding-left:.55rem;
     border-left:4px solid var(--fg); }
p { margin:.9rem 0; } a { color:var(--link); }
/* 본문 문단은 첫 줄을 한 글자만큼 들여쓴다 — PDF 와 같은 규칙
   (저자 지시 2026-08-06). 구조적인 자리(표지·차례·표제·캡션·목록)는 뺀다. */
main p { text-indent: 1em; }
.cover p, .note p, .toc p, .float-caption, .metalist p, .memrow p,
.colophon-meta p, li p, td p, th p, figure p, blockquote p,
.demo-src p, .demo-out p, pre, pre p,
.dev-label, p.dev-label { text-indent: 0; }
/* 상자 안에서는 표제 줄에 붙는 첫 문단만 들여쓰지 않는다(PDF 의 all:false) */
.dev p { text-indent: 0; }
.dev p + p { text-indent: 1em; }
/* 표지(문./답.)가 들어간 첫 문단은 절대 들여쓰지 않는다 */
.dev p:has(> span.dev-label:first-child) { text-indent: 0; }
.chapter-head p { text-indent: 0; }
pre,code,kbd { font-family:"D2Coding",ui-monospace,SFMono-Regular,Menlo,monospace; }
code { background:none; padding:0; font-size:.95em; }
pre { background:none; border:1px solid var(--fg); border-radius:0;
      padding:.95rem 1.05rem; overflow-x:auto; line-height:1.6; font-size:.95rem;
      margin:1.2rem 0; }
.tok-c { color:var(--tok-c); font-style:italic; }
.tok-k { color:var(--tok-k); font-weight:700; }
.tok-s { color:var(--tok-s); }
.tok-n { color:var(--tok-n); }
.tok-t { color:var(--tok-t); font-weight:700; }
.tok-x { color:var(--tok-x); }
.tok-p { color:var(--tok-p); }
pre code { background:none; padding:0; }
/* 표: 바깥은 굵게, 안은 가늘게, 머리행은 굵은 글씨에 옅은 음영
   (저자 지시 2026-08-06). */
/* 표는 내용에 필요한 만큼만 폭을 쓰고 가운데에 선다. 100% 폭으로 늘리면
   셀 오른쪽에 빈 칸이 생겨 테두리와 내용이 어긋나 보인다
   (저자 지적 2026-08-06). 넘칠 때는 감싼 .tblwrap 이 가로로 스크롤한다. */
.tblwrap { overflow-x:auto; text-align:center; }
/* inline-table 은 언제나 내용만큼만 넓어진다(shrink-to-fit). flex 로 가운데를
   맞추면 브라우저에 따라 늘어나거나 왼쪽이 잘리는 일이 있어 이 방식을 쓴다. */
.tblwrap > table { display:inline-table; width:auto; max-width:none; margin:1.1rem 0; }
table { border-collapse:collapse; margin:1.1rem auto; font-size:.95rem;
        width:auto; max-width:100%; border:2px solid var(--fg);
        table-layout:auto; }
th, td { white-space:normal; }
th,td { border:1px solid var(--rule); padding:.42rem .6rem; text-align:left; vertical-align:top; }
/* 머리행 — 옅은 음영으로 포인트를 준다(PDF 와 같은 처리). 색이 아니라
   반투명 회색이라 밝은·어두운 테마 양쪽에서 자연스럽다. */
th { background:rgba(128,128,128,.16); font-weight:700;
     border-bottom:2px solid var(--fg);
     font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
table tr:first-child th, table tr:first-child td { border-bottom:1px solid var(--fg); }
ul,ol { padding-left:1.4rem; } li { margin:.3rem 0; }
/* 메모리 사물함 도해 — 칸과 그 아래 주소. 표 서식과 섞이지 않게 따로 둔다. */
.memrow { overflow-x:auto; text-align:center; margin:1.1rem 0 .2rem; }
.memrow table.mem { display:inline-table; border-collapse:collapse;
  border:none; margin:0; width:auto; }
.memrow td.cell { border:1px solid var(--fg); padding:.3rem .5rem;
  font-family:"D2Coding",ui-monospace,monospace; font-size:.9rem;
  text-align:center; background:none; }
.memrow td.cell.hi { border-width:2px; }
.memrow td.addr { border:none; padding:.15rem .5rem 0; text-align:center;
  font-family:"D2Coding",ui-monospace,monospace; font-size:.82rem;
  color:var(--muted); }
/* 시연 — 소스와 실행 결과는 선명한 검은 테두리로 묶는다 */
/* 시연 상자 — 문답과 같은 1×2 모양. 위 칸은 표제(파일 경로는 고정폭,
   "실행 결과"는 산세리프 굵은 글씨), 가로선 아래 칸이 내용이다
   (저자 지시 2026-08-06). */
.demo { border:1px solid var(--fg); margin:1.15rem 0; }
.demo > .demo-head { margin:0; padding:.5rem .9rem; text-indent:0;
  border-bottom:1px solid var(--fg); font-weight:700; font-size:.92rem; }
.demo-src > .demo-head { font-family:"D2Coding",ui-monospace,monospace; }
.demo-in > .demo-head, .demo-out > .demo-head {
  font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.demo > .demo-body { padding:0; }
.demo > .demo-body pre { border:none; border-radius:0; margin:0;
  padding:.6rem .9rem; }

.dev { margin:1.15rem 0; padding:.75rem .95rem; background:none;
       border-left:3px solid var(--box-rule); border-radius:0; }
/* 표·그림 캡션 — 대상 아래 가운데, 산세리프 */
.float-caption { text-align:center; font-family:"Noto Sans CJK KR",system-ui,sans-serif;
  font-size:.92rem; color:var(--muted); margin:.35rem 0 1.2rem; }
figure { margin:1.2rem 0; }
figure.tbl table { margin-bottom:.2rem; }
figure.tbl { text-align:center; }
.dev > p:first-child { margin-top:0; } .dev > p:last-child { margin-bottom:0; }
.dev-label { font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
             font-size:.92rem; color:var(--muted); margin-bottom:.4rem; }
/* 장 서두 — 넷을 각각 하나의 상자로 둔다. 표제 줄과 내용을 가로선으로
   가르는 모양은 문답·복습 정리와 같다 (저자 지시 2026-08-06).
   ★ 좌우 여백은 *상자*가 준다. 자식 요소에 주면, 본문이 요소로 감싸이지 않은
     칸(「이 장이 끝나면」)은 여백이 0 이 되고 문단이 여럿인 칸(질문 목록)은
     여백이 겹쳐 들쭉날쭉해진다. */
.dev.open { border:1px solid var(--fg); border-radius:0; background:none;
  padding:0 .9rem .55rem; margin:.75rem 0; }
.dev.open > .dev-label { margin:0 -.9rem .5rem; padding:.45rem .9rem;
  text-indent:0; border-bottom:1px solid var(--fg);
  font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
  font-size:.98rem; color:var(--fg); text-transform:none; letter-spacing:0; }
/* 서두 본문은 한 겹(.open-body) 안에 있다 — 위·아래 여백을 여기서 한 번만 준다 */
.dev.open > .open-body { margin:.5rem 0 0; padding:0; }
.dev.open > .open-body > * { margin:.4rem 0; }
.dev.open > .open-body > *:first-child { margin-top:0; }
.dev.open > .open-body > *:last-child { margin-bottom:0; }
/* 질문 목록 — 번호를 보이게 하고 항목 간격은 촘촘히 */
.dev.open .open-body > ol.qlist { margin:0; padding-left:1.35rem; }
.dev.open ol.qlist li { margin:.18rem 0; padding-left:.15rem; }
.dev.open ol.qlist li::marker { color:var(--muted); font-weight:700; }
/* 돌아보기 안의 답 — 왼쪽 가는 선으로 구분 */
.dev.open .qa-a { border:none; border-left:2px solid var(--muted);
  padding:.1rem 0 .1rem .7rem; margin:.4rem 0; }
/* 서두 표지: 굵고 선명한 검은 산세리프, 본문보다 약간 크게
   (저자 지시 2026-08-06). 대문자 변형과 자간 늘리기는 쓰지 않는다. */
.prereq .dev-label, .deepqa .dev-label, .organizer .dev-label, .questions .dev-label {
  text-transform:none; letter-spacing:0; font-size:1.12rem; font-weight:700;
  color:var(--fg); margin-bottom:.45rem; }
/* 돌아보기 안의 답 — 왼쪽 세로선을 또렷하게 */
.deepqa .qa-a { border-left:2px solid var(--muted); padding-left:.75rem; }

/* 문답 — 한 상자 안에서 가로선 하나로 갈린다 */
.qa-box { border:1px solid var(--fg); margin:1.15rem 0; }
.qa-box .dev { margin:0; border:none; border-radius:0; padding:.6rem .9rem;
  background:none; }
.qa-box .qa-a { border-top:1px solid var(--fg); }
/* 복습 정리도 문답과 같은 상자 — 표제 줄과 본문을 가로선으로 가른다 */
.dev.recap { border:1px solid var(--fg); border-radius:0; background:none;
  padding:0 .9rem .55rem; margin:1.15rem 0; }
.dev.recap > .dev-label { margin:0 -.9rem .5rem; padding:.5rem .9rem;
  text-indent:0; border-bottom:1px solid var(--fg);
  font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
  font-size:.95rem; color:var(--fg); }
.dev.recap > p, .dev.recap > div, .dev.recap > figure { margin:.3rem 0;
  padding:0; }
.deepqa .qa-a { border-left:2px solid var(--muted); padding-left:.75rem; }
/* 문./답. 은 줄을 바꾸지 않고 질문·답 첫 문단과 같은 줄에 이어 붙는다
   (저자 지시 2026-08-06). 답의 둘째 문단부터는 그대로 문단으로 흐른다. */
.qa-q .dev-label, .qa-a .dev-label, .deepqa .qa-a .dev-label {
  display:inline; margin-bottom:0; }
.qa-q .dev-label + p, .qa-a .dev-label + p { display:inline; }
.qa-q .dev-label, .qa-q .dev-label + p {
  font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
  color:var(--fg); font-size:1rem; }
.qa-a .dev-label { color:var(--muted); }
.qa-a .dev-label + p { font-family:inherit; font-weight:400; }
span.dev-label { display:inline; font-size:1rem; margin:0; }
/* 상자형 장치 공통 — 문답 상자와 같은 모양: 표제 줄과 본문을 가로선으로
   가르고, 좌우 여백은 상자가 준다 (저자 지시 2026-08-06).
   ★ 예전의 2단 구조(.misconception + div) 규칙이 남아 뒤따르는 문답 상자를
     오해 상자의 아랫칸처럼 그리던 문제를 여기서 없앤다. */
.dev.misconception, .dev.realcase, .dev.antipattern, .dev.mathbox,
.dev.platform, .dev.recap {
  border:1px solid var(--fg); border-radius:0; background:none;
  padding:0 .9rem .55rem; margin:1.15rem 0; }
.dev.misconception > .dev-label, .dev.realcase > .dev-label,
.dev.antipattern > .dev-label, .dev.mathbox > .dev-label,
.dev.platform > .dev-label, .dev.recap > .dev-label {
  margin:0 -.9rem .5rem; padding:.45rem .9rem; text-indent:0;
  border-bottom:1px solid var(--fg);
  font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
  font-size:.95rem; color:var(--fg); }
.dev.misconception > p, .dev.realcase > p, .dev.antipattern > p,
.dev.mathbox > p, .dev.platform > p, .dev.recap > p,
.dev.misconception > div, .dev.realcase > div, .dev.antipattern > div,
.dev.mathbox > div, .dev.platform > div, .dev.recap > div,
.dev.recap > figure, .dev.mathbox > figure {
  margin:.35rem 0; padding:0; }
/* 상자 안의 문단 간격은 한 줄 사이 정도로 — 본문 문단 간격(.9rem)을 그대로
   쓰면 상자 안이 헐렁해 보인다 (저자 지시 2026-08-06). */
/* 고정 바: 제목은 왼쪽 끝, 장 이동 화살표·언어·GitHub 는 오른쪽 끝
   (저자 지시 2026-08-06) */
.bar > strong { flex:0 0 auto; }
.bar .bar-nav { display:flex; gap:.55rem; align-items:center; margin-right:.35rem; }
.bar .bar-nav a, .bar .bar-nav .off {
  display:inline-block; min-width:1.35rem; text-align:center;
  font-size:1.15rem; line-height:1.1; text-decoration:none; }
.bar .bar-nav a:hover { text-decoration:none; color:var(--fg); }
.bar .bar-nav .off { color:var(--muted); opacity:.35; }
.dev p, .dev div, .dev ul, .dev ol, .qa-box p, .qa-box div {
  margin-top:.4rem; margin-bottom:.4rem; }
.dev > *:first-child, .qa-box .dev > *:first-child { margin-top:0; }
.dev > *:last-child, .qa-box .dev > *:last-child { margin-bottom:0; }
.dev ul, .dev ol { padding-left:1.3rem; }
.dev li { margin:.15rem 0; }
/* 갈래는 테두리 모양으로 구별한다 */
.dev.antipattern { border-style:dashed; }
.dev.platform { border-style:double; }
.dev.mathbox { font-size:.95rem; }
.note { border:1px solid var(--rule); background:none; padding:.65rem .9rem;
        margin:0 0 1.6rem; font-size:.9rem; }
.cover { text-align:center; margin:1.4rem 0 2rem; padding-bottom:1.8rem;
         border-bottom:1px solid var(--rule); }
.cover h1 { font-size:clamp(1.9rem,5vw,2.9rem); margin:0; letter-spacing:.01em; }
.cover p { margin:.45rem 0; }
.cover-sub { font-size:clamp(1rem,2.2vw,1.2rem); color:var(--fg); }
.badge { display:inline-block; margin-top:.5rem; padding:.16rem .6rem;
         border:1px solid var(--fg); font-size:.8rem; font-weight:700;
         font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.cover-meta { color:var(--muted); font-size:.9rem;
              font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.cover-author { margin-top:1.1rem; font-size:1.05rem; }
.cover-links { font-size:.87rem; }
.cover-links a { margin:0 .6rem; }
.cover-blurb { max-width:44rem; margin:1.7rem auto 0; font-size:.95rem; color:var(--muted); }
.cover-blurb p { margin:.5rem 0; }
.metalist { display:grid; grid-template-columns:max-content 1fr; gap:.35rem 1.2rem;
  margin:.6rem 0 1rem; }
.metalist dt { color:var(--muted); font-family:"Noto Sans CJK KR",system-ui,sans-serif;
  font-size:.92rem; }
.metalist dd { margin:0; }
.colophon-meta { display:grid; grid-template-columns:max-content 1fr; gap:.35rem 1.2rem;
                 margin:1.2rem 0 1.8rem; font-size:.93rem; }
.colophon-meta dt { color:var(--muted);
                    font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-size:.88rem; }
.colophon-meta dd { margin:0; }
.colophon { font-size:.93rem; }
.colophon > div { margin:.9rem 0; padding-left:.9rem; border-left:1px solid var(--rule); }
/* 개념도 — 흑백 선화라 다크 모드에서는 반전해 준다 */
figure.fig { margin:1.6rem 0; text-align:center; }
figure.fig img { max-width:100%; height:auto; }
figure.fig figcaption { margin-top:.5rem; font-size:.92rem; color:var(--muted); }
@media (prefers-color-scheme: dark) { figure.fig img { filter:invert(1) hue-rotate(180deg); } }
:root[data-theme="dark"] figure.fig img { filter:invert(1) hue-rotate(180deg); }
:root[data-theme="light"] figure.fig img { filter:none; }
.toc { columns:24rem; column-gap:2.4rem; }
.toc-part { break-inside:avoid; margin:1.1rem 0 .3rem; font-size:.95rem; letter-spacing:.02em;
            font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.toc-part:first-child { margin-top:0; }
.toc-front { margin-bottom:.4rem; }
.toc-group { break-inside:avoid; padding-left:.85rem; border-left:1px solid var(--rule); }
.toc a { display:block; padding:.28rem 0; text-decoration:none; border-bottom:1px solid var(--rule); }
.toc a:hover { text-decoration:underline; }
.nav { display:flex; gap:1rem; align-items:center; margin-top:3rem; padding-top:1rem;
       border-top:1px solid var(--rule); font-size:.9rem;
       font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.nav .sp { flex:1 1 auto; }
"""

# ── 글꼴 — 판마다 다르다. 한국어판은 Noto CJK KR + D2Coding,
#    영어판은 라틴 Noto + Noto Sans Mono (PDF 와 같은 선택).
FONTS = {
 "ko": ('"Noto Serif CJK KR","Noto Serif KR",Georgia,serif',
        '"Noto Sans CJK KR","Noto Sans KR",system-ui,sans-serif',
        '"D2Coding",ui-monospace,SFMono-Regular,Menlo,monospace'),
 "en": ('"Noto Serif",Georgia,"Noto Serif CJK KR",serif',
        '"Noto Sans",system-ui,"Noto Sans CJK KR",sans-serif',
        '"Noto Sans Mono",ui-monospace,SFMono-Regular,Menlo,monospace'),
}[lang]
CSS += f"""
body {{ font-family:{FONTS[0]}; }}
h1,h2,h3,h4,.bar,.dev-label,.toc-part,.cover-meta,.badge,
.colophon-meta dt {{ font-family:{FONTS[1]}; }}
pre,code,kbd {{ font-family:{FONTS[2]}; }}
"""

def page(title, inner, prev=None, nxt=None, is_index=False, self_name=None):
    # 위 고정 바에도 이전·목차·다음을 둔다 (저자 지시 2026-08-06) — 긴 장에서
    # 바닥까지 내려가지 않고도 옮겨 다닐 수 있어야 한다.
    bar_nav = ""
    if not is_index:
        bar_nav = '<span class="bar-nav">'
        bar_nav += (f'<a href="{prev[0]}" title="{STR["prev"]}"'
                    f' aria-label="{STR["prev"]}">←</a>' if prev
                    else f'<span class="off" aria-hidden="true">←</span>')
        bar_nav += (f'<a href="index.html" title="{STR["top"]}"'
                    f' aria-label="{STR["top"]}">↑</a>')
        bar_nav += (f'<a href="{nxt[0]}" title="{STR["next"]}"'
                    f' aria-label="{STR["next"]}">→</a>' if nxt
                    else f'<span class="off" aria-hidden="true">→</span>')
        bar_nav += "</span>"
    # 언어 전환은 *같은 장*으로 보낸다 (저자 지시 2026-08-07).
    # 장 페이지의 이름(chNN.html)은 두 판에서 같다. 번호 없는 앞·뒷부속은
    # 판마다 쪽수가 달라 이름이 어긋나므로 그쪽 차례로 보낸다.
    other_href = STR["other_href"]
    if self_name and self_name.startswith("ch"):
        other_href = STR["other_href"] + self_name

    nav = ""
    if not is_index:
        left = f'<a href="{prev[0]}">← {STR["prev"]}</a>' if prev else "<span></span>"
        right = f'<a href="{nxt[0]}">{STR["next"]} →</a>' if nxt else "<span></span>"
        nav = (f'<div class="nav">{left}<span class="sp"></span>'
               f'<a href="index.html">{STR["top"]}</a><span class="sp"></span>{right}</div>')
    return f"""<!doctype html>
<html lang="{lang}">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{htmlmod.escape(title)}</title><style>{CSS}</style></head>
<body>
<div class="bar"><strong><a href="index.html">{STR['short']}</a></strong><span class="sp"></span>{bar_nav}<a href="{other_href}">{STR['other']}</a>
<a href="https://github.com/rubidus-api/proven_c_book">GitHub</a></div>
<main><div class="wrap">
{inner}
{nav}
</div></main></body></html>
"""

out_dir.mkdir(parents=True, exist_ok=True)
for f in out_dir.glob("*.html"): f.unlink()
names = []
for i, (t, _) in enumerate(chapters):
    mnum = re.match(r"\s*(\d+)\s", t)
    names.append(f"ch{int(mnum.group(1)):02d}.html" if mnum else f"sec{i+1:02d}.html")
# ── 목차를 *부 단위로* 묶는다 ──────────────────────────────────
# 부 구성의 단일 출처는 main.typ 이다 (여기에 목록을 베끼지 않는다).
def read_parts(lang):
    src = pathlib.Path(__file__).resolve().parent.parent / ("book" if lang == "ko" else "book-en") / "main.typ"
    out = []
    for line in src.read_text(encoding="utf-8").splitlines():
        m = re.match(r'\s*\("([^"]+)",\s*(?:none|"[^"]*"),\s*\(([\d,\s]+)\)\),', line)
        if m:
            nums = [int(x) for x in re.findall(r"\d+", m.group(2))]
            out.append((m.group(1), nums))
    return out

by_num = {}
for i, (t, _) in enumerate(chapters):
    mn = re.match(r"\s*(\d+)\s", t)
    if mn:
        by_num[int(mn.group(1))] = i

# 앞부속(머리말·번역 노트)과 뒷부속(부록·찾아보기)을 가른다.
# 문서 순서에서 첫 번호 장보다 앞에 있으면 앞부속이고, 그것은 부 목록보다
# *앞*에 놓는다 — 뒷부속 묶음("부록과 찾아보기")에 섞이면 안 된다.
first_ch = min(by_num.values()) if by_num else len(chapters)
last_ch = max(by_num.values()) if by_num else -1

toc_parts = []
placed = set()
front_rows = [f'<a href="{names[i]}">{htmlmod.escape(chapters[i][0])}</a>'
              for i in range(len(chapters)) if i < first_ch]
if front_rows:
    toc_parts.append(f'<div class="toc-group toc-front">{"".join(front_rows)}</div>')
for part_title, nums in read_parts(lang):
    rows = []
    for n in nums:
        i = by_num.get(n)
        if i is None:
            continue
        placed.add(i)
        rows.append(f'<a href="{names[i]}">{htmlmod.escape(chapters[i][0])}</a>')
    if rows:
        toc_parts.append(f'<h4 class="toc-part">{htmlmod.escape(part_title)}</h4>'
                         f'<div class="toc-group">{"".join(rows)}</div>')
rest = [f'<a href="{names[i]}">{htmlmod.escape(chapters[i][0])}</a>'
        for i in range(len(chapters))
        if i not in placed and i > last_ch]
if rest:
    toc_parts.append(f'<h4 class="toc-part">{STR["extra"]}</h4>'
                     f'<div class="toc-group">{"".join(rest)}</div>')
toc = "".join(toc_parts)
# ── 표지 — PDF 표제면의 내용을 목차 앞에 그대로 둔다 ─────────────
version, status, updated = book_meta()
repo = "https://github.com/rubidus-api/proven_c_book"
cover = (f'<header class="cover"><h1>{htmlmod.escape(STR["main"])}</h1>'
         f'<p class="cover-sub">{htmlmod.escape(STR["sub"])}</p>'
         f'<p><span class="badge">{htmlmod.escape(status)}</span></p>'
         f'<p class="cover-meta">{htmlmod.escape(version)} · '
         f'{STR["updated"]} {htmlmod.escape(updated)}</p>'
         f'<p class="cover-author">{htmlmod.escape(STR["author"])}</p>'
         f'<p class="cover-links"><a href="mailto:rubidus@gmail.com">rubidus@gmail.com</a>'
         f'<a href="{repo}">github.com/rubidus-api/proven_c_book</a></p>'
         + '<div class="cover-blurb">'
         + "".join(f'<p>{t}</p>' for t in STR["blurb"])
         + '</div></header>')
# ── 머리말은 제 페이지가 따로 있으므로 index 에 옮겨 싣지 않는다
#    (저자 지시 2026-08-07: 같은 글이 두 번 나올 이유가 없다).
#    차례의 앞부속 묶음 첫 줄이 그 페이지로 가는 링크다.
index_inner = (f'{cover}<div class="note">{STR["note"]}</div>'
               f'<h3>{STR["toc"]}</h3><div class="toc">{toc}</div>')
(out_dir/"index.html").write_text(page(STR["title"], index_inner, is_index=True, self_name="index.html"), encoding="utf-8")
for i,(title,content) in enumerate(chapters):
    prev = ("index.html", STR["toc"]) if i==0 else (names[i-1], "")
    nxt = (names[i+1], "") if i+1 < len(chapters) else None
    (out_dir/names[i]).write_text(page(f"{title} — {STR['short']}", content, prev, nxt,
                                      self_name=names[i]), encoding="utf-8")
# 분할로 어긋난 내부 앵커(#loc-N)를 "파일#앵커" 로 고친다
anchor_home = {}
for f in out_dir.glob("*.html"):
    for aid in re.findall(r'id="([^"]+)"', f.read_text(encoding="utf-8")):
        anchor_home[aid] = f.name
for f in out_dir.glob("*.html"):
    txt = f.read_text(encoding="utf-8")
    def fix(mo):
        aid = mo.group(1)
        home = anchor_home.get(aid)
        if home is None:
            return mo.group(0)
        return f'href="#{aid}"' if home == f.name else f'href="{home}#{aid}"'
    new = re.sub(r'href="#([^"]+)"', fix, txt)
    if new != txt:
        f.write_text(new, encoding="utf-8")

total = sum(f.stat().st_size for f in out_dir.glob("*.html"))
print(f"wrap-html: {out_dir}/ — {len(chapters)} pages + index ({total//1024} KB)")
