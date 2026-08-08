#!/usr/bin/env python3
"""장 사이의 의존 관계도를 만든다 (저자 지시 2026-08-08).

자료의 출처는 원고 자체다 — 각 장 서두의 `#prereq(([N장 …], [무엇]), …)` 가
「이 장은 N장에 기댄다」는 선언이므로, 그것을 그대로 그래프로 옮긴다.
손으로 목록을 베끼지 않는다는 뜻이다(베끼면 곧 어긋난다).

만드는 것 넷:
  docs/dependency-graph.svg      한국어 관계도(호 다이어그램)
  docs/dependency-graph-en.svg   영어 관계도
  docs/DEPENDENCIES.md           장별 표(기댄 곳 / 이 장에 기대는 곳)
  docs/DEPENDENCIES-en.md        같은 표의 영어판

왜 호 다이어그램인가: 이 책은 선형으로 읽는 책이라 장을 번호순 축에 올리는 것이
가장 읽기 쉽고, 「얼마나 멀리서 끌어오는가」가 호의 길이로 한눈에 보인다. 상자와
화살표로 그린 일반 그래프는 96개 마디에서 실뭉치가 된다.

사용법: python3 scripts/make-depgraph.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONT = "Noto Sans CJK KR, Noto Sans, sans-serif"

PREREQ_KO = re.compile(r"\[\s*(\d+)장\s+([^\]]*?)\]")
PREREQ_EN = re.compile(r"\[\s*chapter\s+(\d+)\s*,\s*([^\]]*?)\]", re.I)


def chapters(lang):
    """{번호: (제목, [기댄 장 번호…])} 를 원고에서 읽는다."""
    base = ROOT / ("book" if lang == "ko" else "book-en") / "chapters"
    out = {}
    for f in sorted(base.glob("ch*.typ")):
        m = re.match(r"ch(\d+)\.typ$", f.name)
        if not m:
            continue
        n = int(m.group(1))
        text = f.read_text(encoding="utf-8")
        tm = re.search(r"^=\s+(.+)$", text, re.M)
        title = tm.group(1).strip() if tm else f"ch{n}"
        deps = []
        pm = re.search(r"#prereq\((.*?)\n\)", text, re.S)
        if pm:
            pat = PREREQ_KO if lang == "ko" else PREREQ_EN
            for d in pat.finditer(pm.group(1)):
                k = int(d.group(1))
                if k != n and k not in deps:
                    deps.append(k)
        out[n] = (title, deps)
    return out


def parts(lang):
    """main.typ 의 부 구성이 단일 출처다. [(제목, [장…])…]"""
    src = (ROOT / ("book" if lang == "ko" else "book-en") / "main.typ").read_text(encoding="utf-8")
    block = src[src.index("#let parts = ("):]
    block = block[:block.index("\n)")]
    out = []
    for line in block.split("\n"):
        m = re.search(r'\("([^"]+)"[^()]*\(((?:\d+,\s*)*\d+)\)\)', line)
        if m:
            out.append((m.group(1), [int(x) for x in m.group(2).split(",")]))
    return out


# ── SVG ──────────────────────────────────────────────────────
def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
             .replace('"', "&quot;"))


def short(title, limit):
    t = re.split(r"\s+[—–-]\s+", title)[0].strip()
    t = t.replace("`", "")
    return t if len(t) <= limit else t[:limit - 1] + "…"


def build_svg(chs, prts, L):
    n = max(chs)
    left, right = 60, 60
    step = 19
    width = left + right + step * (n - 1)
    axis_y = 470          # 축의 y
    top = 96              # 호가 쓸 수 있는 위쪽 여백의 시작
    height = axis_y + 210

    def x(i):
        return left + step * (i - 1)

    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
         f'width="{width}" height="{height}" font-family="{FONT}">',
         f'<rect width="{width}" height="{height}" fill="#ffffff"/>']

    o.append(f'<text x="{width/2}" y="34" font-size="20" font-weight="bold" '
             f'text-anchor="middle" fill="#111">{esc(L["title"])}</text>')
    o.append(f'<text x="{width/2}" y="58" font-size="12.5" text-anchor="middle" '
             f'fill="#444">{esc(L["sub"])}</text>')

    # 부 띠 — 축 아래
    band_y = axis_y + 14
    for k, (ptitle, plist) in enumerate(prts):
        if not plist:
            continue
        x0, x1 = x(min(plist)) - step / 2, x(max(plist)) + step / 2
        fill = "#eef2f6" if k % 2 == 0 else "#e2e8ef"
        o.append(f'<rect x="{x0:.1f}" y="{band_y}" width="{x1-x0:.1f}" height="26" '
                 f'fill="{fill}" stroke="#c3ccd6" stroke-width="0.8"/>')
        head = re.split(r"\s+[—–-]\s+", ptitle)
        num, name = head[0].strip(), (head[1].strip() if len(head) > 1 else "")
        # 한글은 글자 폭이 글꼴 크기와 거의 같고, 라틴 글자는 그 절반쯤이다
        def w(t):
            return sum(10.5 if ord(ch) > 0x2000 else 5.8 for ch in t)
        full = f"{num} {name}"
        label = full if name and w(full) + 8 <= (x1 - x0) else num
        o.append(f'<text x="{(x0+x1)/2:.1f}" y="{band_y+17}" font-size="10.5" '
                 f'text-anchor="middle" fill="#333">{esc(label)}</text>')

    # 호 — 기댄 곳(작은 번호) → 이 장(큰 번호)
    edges = []
    for c, (_t, deps) in sorted(chs.items()):
        for d in deps:
            edges.append((min(c, d), max(c, d), d > c))
    for a, b, forward in sorted(edges, key=lambda e: e[1] - e[0]):
        span = b - a
        xa, xb = x(a), x(b)
        r = (xb - xa) / 2
        h = min(top + r * 0.55, axis_y - top)          # 호의 높이
        cy = axis_y - h
        # 반타원을 3차 베지에로
        d = (f'M {xa:.1f} {axis_y} C {xa:.1f} {cy:.1f}, {xb:.1f} {cy:.1f}, '
             f'{xb:.1f} {axis_y}')
        if forward:                                     # 뒤 장에 기대는 것 — 이례
            o.append(f'<path d="{d}" fill="none" stroke="#b3261e" stroke-width="1.6" '
                     f'stroke-dasharray="4 3" opacity="0.85"/>')
        else:
            w = 0.7 + min(span, 40) / 40 * 1.1
            op = 0.22 + min(span, 40) / 40 * 0.5
            o.append(f'<path d="{d}" fill="none" stroke="#1A4F8A" '
                     f'stroke-width="{w:.2f}" opacity="{op:.2f}"/>')

    # 축과 마디
    o.append(f'<line x1="{x(1)-10}" y1="{axis_y}" x2="{x(n)+10}" y2="{axis_y}" '
             f'stroke="#111" stroke-width="1.2"/>')
    indeg = {c: 0 for c in chs}
    for _a, b, _f in edges:
        indeg[b] = indeg.get(b, 0) + 1
    outdeg = {c: 0 for c in chs}
    for c, (_t, deps) in chs.items():
        for d in deps:
            outdeg[d] = outdeg.get(d, 0) + 1

    for c in sorted(chs):
        cx = x(c)
        rad = 2.4 + min(outdeg.get(c, 0), 8) * 0.45     # 많이 기대어지는 장일수록 크게
        o.append(f'<circle cx="{cx:.1f}" cy="{axis_y}" r="{rad:.1f}" fill="#111"/>')
        if c % 5 == 0 or c == 1 or c == n:
            o.append(f'<text x="{cx:.1f}" y="{axis_y+50}" font-size="9.5" '
                     f'text-anchor="middle" fill="#555">{c}</text>')

    # 가장 많이 기대어지는 장 다섯을 이름으로 적는다
    hubs = sorted(chs, key=lambda c: -outdeg.get(c, 0))[:5]
    o.append(f'<text x="{left}" y="{axis_y+82}" font-size="12" font-weight="bold" '
             f'fill="#111">{esc(L["hubs"])}</text>')
    for i, c in enumerate(hubs):
        o.append(f'<text x="{left}" y="{axis_y+102+i*17}" font-size="11" fill="#333">'
                 f'{c} — {esc(short(chs[c][0], 34))} '
                 f'<tspan fill="#777">({outdeg.get(c,0)})</tspan></text>')

    # 범례
    lx = left + 330
    o.append(f'<text x="{lx}" y="{axis_y+82}" font-size="12" font-weight="bold" '
             f'fill="#111">{esc(L["legend"])}</text>')
    for i, (txt, kind) in enumerate([(L["l1"], "arc"), (L["l2"], "fwd"), (L["l3"], "dot")]):
        y = axis_y + 100 + i * 17
        if kind == "arc":
            o.append(f'<path d="M {lx} {y} C {lx} {y-9}, {lx+30} {y-9}, {lx+30} {y}" '
                     f'fill="none" stroke="#1A4F8A" stroke-width="1.4" opacity="0.6"/>')
        elif kind == "fwd":
            o.append(f'<path d="M {lx} {y} C {lx} {y-9}, {lx+30} {y-9}, {lx+30} {y}" '
                     f'fill="none" stroke="#b3261e" stroke-width="1.6" stroke-dasharray="4 3"/>')
        else:
            o.append(f'<circle cx="{lx+15}" cy="{y-4}" r="4" fill="#111"/>')
        o.append(f'<text x="{lx+40}" y="{y}" font-size="11" fill="#333">{esc(txt)}</text>')

    o.append("</svg>")
    return "".join(o)


# ── Markdown ─────────────────────────────────────────────────
def build_md(chs, prts, L):
    outdeg = {c: [] for c in chs}
    for c, (_t, deps) in sorted(chs.items()):
        for d in deps:
            outdeg.setdefault(d, []).append(c)

    edges = [(c, d) for c, (_t, ds) in chs.items() for d in ds]
    forward = sorted((c, d) for c, d in edges if d > c)
    roots = sorted(c for c, (_t, ds) in chs.items() if not ds)
    leaves = sorted(c for c in chs if not outdeg.get(c))

    L_ = L
    md = [f"# {L_['title']}", "",
          L_["intro"], "",
          f"![{L_['title']}]({L_['svg']})", "",
          f"- {L_['n_ch']}: **{len(chs)}**",
          f"- {L_['n_edge']}: **{len(edges)}**",
          f"- {L_['n_root']}: {', '.join(str(c) for c in roots)}",
          ""]

    md += [f"## {L_['fwd_h']}", ""]
    if not forward:
        md += [L_["fwd_none"], ""]
    else:
        md += [L_["fwd_note"], ""]
        md += [f"- {L_['ch']} {c} → {L_['ch']} {d} — {L_['depends']}" for c, d in forward]
        md += [""]

    md += [f"## {L_['hub_h']}", "",
           f"| {L_['ch']} | {L_['title_col']} | {L_['cnt']} |", "|---|---|---|"]
    for c in sorted(chs, key=lambda c: (-len(outdeg.get(c, [])), c))[:12]:
        md.append(f"| {c} | {chs[c][0]} | {len(outdeg.get(c, []))} |")
    md += ["", f"## {L_['table_h']}", ""]

    for ptitle, plist in prts:
        md += [f"### {ptitle}", "",
               f"| {L_['ch']} | {L_['title_col']} | {L_['leans']} | {L_['leaned']} |",
               "|---|---|---|---|"]
        for c in plist:
            if c not in chs:
                continue
            t, deps = chs[c]
            a = ", ".join(str(d) for d in deps) or "—"
            b = ", ".join(str(d) for d in sorted(outdeg.get(c, []))) or "—"
            md.append(f"| {c} | {t} | {a} | {b} |")
        md.append("")

    if leaves:
        md += [f"## {L_['leaf_h']}", "", L_["leaf_note"], "",
               ", ".join(str(c) for c in leaves), ""]
    md += ["---", "", L_["footer"], ""]
    return "\n".join(md)


KO = dict(
    title="장 사이의 의존 관계",
    sub="각 장 서두의 「먼저 알아야 할 것」이 그대로 화살이 된다 — 왼쪽(기댄 곳)에서 오른쪽(그 장)으로",
    hubs="가장 많이 기대어지는 장",
    legend="읽는 법",
    l1="호 하나가 의존 하나. 길수록 멀리서 끌어온다",
    l2="뒤 장에 기대는 것(점검이 필요한 자리)",
    l3="점의 크기 = 이 장에 기대는 장의 수",
    svg="dependency-graph.svg",
    intro=("이 문서는 손으로 적은 것이 아니라 **원고에서 뽑아낸 것**이다. 각 장 서두의\n"
           "`먼저 알아야 할 것` 항목이 「이 장은 저 장에 기댄다」는 선언이므로,\n"
           "`scripts/make-depgraph.py` 가 그것을 읽어 이 그림과 표를 만든다.\n"
           "원고가 바뀌면 다시 돌려서 갱신한다."),
    n_ch="장 수", n_edge="의존 관계 수", n_root="아무것에도 기대지 않는 장",
    fwd_h="뒤 장에 기대는 자리", fwd_note="선형으로 읽는 책에서는 이례적인 자리다 — 의도한 것인지 점검할 대상.",
    fwd_none="없다. 모든 장이 자기보다 앞선 장에만 기댄다.",
    hub_h="많이 기대어지는 장 — 이 책의 기둥",
    table_h="부별 전체 표", ch="장", title_col="제목", cnt="기대는 장 수",
    leans="기댄 곳", leaned="이 장에 기대는 곳", depends="아직 오지 않은 장을 선행으로 걸고 있다",
    leaf_h="아직 아무도 기대지 않는 장",
    leaf_note="대개 마지막 부의 장들이다. 앞부분에 있다면 「연결이 약한 장」일 수 있다.",
    footer="생성: `python3 scripts/make-depgraph.py`",
)

EN = dict(
    title="Dependencies between chapters",
    sub="each chapter's \"What to know first\" becomes an arc — from the chapter leaned on (left) to the chapter itself (right)",
    hubs="Most leaned-on chapters",
    legend="How to read it",
    l1="one arc, one dependency — the longer, the further it reaches back",
    l2="leaning on a later chapter (worth checking)",
    l3="dot size = how many chapters lean on it",
    svg="dependency-graph-en.svg",
    intro=("This document is not written by hand but **extracted from the manuscript**.\n"
           "Each chapter's `What to know first` entries declare \"this chapter leans on that one\",\n"
           "and `scripts/make-depgraph.py` reads them to build this picture and these tables.\n"
           "Re-run it whenever the manuscript changes."),
    n_ch="Chapters", n_edge="Dependencies", n_root="Chapters leaning on nothing",
    fwd_h="Leaning on a later chapter",
    fwd_note="Unusual in a book read linearly — worth checking whether it is intended.",
    fwd_none="None. Every chapter leans only on chapters before it.",
    hub_h="The most leaned-on chapters — this book's pillars",
    table_h="Full table by part", ch="Ch.", title_col="Title", cnt="Chapters leaning on it",
    leans="Leans on", leaned="Leaned on by", depends="listed as a prerequisite before it is reached",
    leaf_h="Chapters nothing leans on yet",
    leaf_note="Mostly the closing part. One near the front may be weakly connected.",
    footer="Generated by `python3 scripts/make-depgraph.py`",
)


def main() -> int:
    out = ROOT / "docs"
    out.mkdir(exist_ok=True)
    for lang, L in (("ko", KO), ("en", EN)):
        chs = chapters(lang)
        if not chs:
            print(f"make-depgraph: {lang} 원고를 찾지 못했다", file=sys.stderr)
            return 1
        prts = parts(lang)
        (out / L["svg"]).write_text(build_svg(chs, prts, L), encoding="utf-8")
        name = "DEPENDENCIES.md" if lang == "ko" else "DEPENDENCIES-en.md"
        (out / name).write_text(build_md(chs, prts, L), encoding="utf-8")
        edges = sum(len(d) for _t, d in chs.values())
        print(f"make-depgraph: {lang} — 장 {len(chs)}개, 의존 {edges}개 "
              f"→ docs/{L['svg']}, docs/{name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
