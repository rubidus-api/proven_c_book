#!/usr/bin/env python3
"""절(2단계 제목) 단위로 책을 분해해 재배치의 근거를 만든다 (저자 지시 2026-08-08).

장 단위 의존은 `make-depgraph.py` 가 이미 그린다. 그러나 「설명 순서를 고치자」는
판단은 장보다 잔 눈금이 필요하다 — 옮겨야 하는 것은 대개 장이 아니라 *절*이기
때문이다. 이 도구는 496개 남짓의 절을 하나씩 재어 다음을 뽑는다.

  · 앞질러 참조   그 절이 *자기보다 뒤 장*을 가리키는 횟수 → 순서가 뒤집힌 자리
  · 되돌아 참조   앞 장을 얼마나 끌어오는가 → 의존의 무게
  · 난이도 지표   회색지대 낱말·표준 조항 인용·실측/어셈블리 등장 (0~3 등급)
  · 성격          설명(explain) / 정리(roundup) / 참조표(reference) / 시연(demo)
  · 분량          줄 수와 장치 수

「모아서 정리하는 절은 설명하는 절보다 뒤로」라는 규율을 기계가 검사할 수 있게,
성격이 *정리*인 절이 같은 장에서 *설명* 절보다 앞에 오면 따로 표시한다.

만드는 것:
  docs/SECTIONS.md            절 지도(장별 표 + 이상 목록)
  docs/section-heat.svg       장×난이도·앞질러 참조 히트 그림
  (표준 출력) 요약과 이상 목록 — 리팩토링 판단의 근거

사용법: python3 scripts/section-map.py [--json out.json]
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO = ROOT / "book" / "chapters"
FONT = "Noto Sans CJK KR, Noto Sans, sans-serif"

# ── 성격 판정 ────────────────────────────────────────────────
# 성격은 낱말만으로 갈리지 않는다 — 「지도」는 장 첫머리면 *예고*, 끝이면 *정리*이고
# 「정리」는 「뒷정리(cleanup)」를 뜻할 때가 있다. 그래서 자리와 함께 본다.
ROUNDUP = ("총정리", "요약", "한눈에", "총람", "닫으며", "돌아보기", "모아 보기",
           "정리하면", "이 부를", "이 장을 닫")
OPENING_MAP = ("지도", "얼개", "전체 그림", "무엇이 있는가", "목록")
REFERENCE = ("표", "목록", "조회", "대조", "참조", "계약", "규칙 모음")
HARD = ("정의되지 않은", "회색지대", "UB", "미지정", "구현 정의", "프로버넌스",
        "정렬", "원자적", "메모리 모델", "시퀀스 포인트", "앨리어싱", "패딩",
        "승격", "부동소수점", "링커", "어셈블리")
EASY = ("첫", "무엇인가", "왜", "시작", "설치", "맛보기", "소개")

SEC_RE = re.compile(r"^==\s+(.+)$", re.M)
CHREF_RE = re.compile(r"(\d+)\s*장")
CLAUSE_RE = re.compile(r"§\d")
MEASURE = ("실측", "재어", "측정", "밀리초", " ms", "어셈블리")


def parse_chapter(path):
    n = int(re.match(r"ch(\d+)\.typ$", path.name).group(1))
    text = path.read_text(encoding="utf-8")
    tm = re.search(r"^=\s+(.+)$", text, re.M)
    ch_title = tm.group(1).strip() if tm else path.stem

    marks = [(m.start(), m.group(1).strip()) for m in SEC_RE.finditer(text)]
    secs = []
    for i, (pos, title) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(text)
        body = text[pos:end]
        refs = sorted({int(x) for x in CHREF_RE.findall(body)} - {n})
        fwd = [r for r in refs if r > n]
        back = [r for r in refs if r < n]

        # 성격은 제목의 *머리*로 판정한다. 「… — 왜 `malloc`이 목록에 없는가」처럼
        # 낱말이 문장 속에 묻혀 있으면 그것은 설명이지 정리가 아니다.
        head = re.split(r"\s+[—–-]{1,3}\s+", title)[0].strip()
        low = title
        kind = "explain"
        if any(k in low for k in ROUNDUP):
            kind = "roundup"
        elif any(k in head for k in OPENING_MAP) and not head.endswith("가"):
            # 첫머리에 있으면 예고 지도(정상), 뒤쪽이면 정리로 본다
            kind = "map" if i <= 1 else "roundup"
        elif any(k in low for k in REFERENCE) and len(body) > 1500:
            kind = "reference"
        if "#demo(" in body and kind == "explain":
            kind = "demo"

        hard = sum(1 for k in HARD if k in body)
        clauses = len(CLAUSE_RE.findall(body))
        measured = any(k in body for k in MEASURE)
        level = 0
        if hard >= 2 or clauses >= 2:
            level = 2
        if hard >= 5 or clauses >= 5:
            level = 3
        if level == 0 and (hard or clauses or measured):
            level = 1
        if any(k in title for k in EASY) and level > 1:
            level -= 1

        secs.append(dict(
            chapter=n, index=i, title=title, lines=body.count("\n"),
            refs=refs, forward=fwd, back=back, kind=kind, level=level,
            subs=len(re.findall(r"^=== ", body, flags=re.M)),
            devices=len(re.findall(r"#(qa|deepqa|misconception|realcase|"
                                   r"antipattern|platform|dtable|demo)\[?\(?", body)),
        ))
    return n, ch_title, secs


def collect():
    chapters, sections = {}, []
    for f in sorted(KO.glob("ch*.typ")):
        if not re.match(r"ch\d+\.typ$", f.name):
            continue
        n, title, secs = parse_chapter(f)
        chapters[n] = title
        sections += secs
    return chapters, sections


def anomalies(chapters, sections):
    """재배치를 검토할 자리를 규칙으로 뽑는다."""
    out = []
    by_ch = {}
    for s in sections:
        by_ch.setdefault(s["chapter"], []).append(s)

    # ① 앞질러 참조가 많은 절 — 뒤에 올 것을 먼저 쓰고 있다
    for s in sections:
        if len(s["forward"]) >= 3:
            out.append(("forward", s,
                        f"뒤 장 {len(s['forward'])}곳을 가리킨다: "
                        f"{', '.join(str(x) for x in s['forward'][:6])}"))

    # ② 정리 절이 설명 절보다 앞에 있다 (저자 규율)
    for n, secs in by_ch.items():
        for i, s in enumerate(secs):
            if s["kind"] != "roundup":
                continue
            later = [t for t in secs[i + 1:] if t["kind"] in ("explain", "demo")]
            if len(later) >= 2:
                out.append(("roundup-early", s,
                            f"뒤에 설명 절이 {len(later)}개 남아 있다: "
                            f"{later[0]['title'][:24]}…"))

    # ③ 난이도 역전 — 장 안에서 어려운 절이 쉬운 절보다 앞
    for n, secs in by_ch.items():
        for i, s in enumerate(secs[:-1]):
            drop = [t for t in secs[i + 1:] if t["level"] <= s["level"] - 2]
            if s["level"] >= 2 and len(drop) >= 2:
                out.append(("hard-first", s,
                            f"난이도 {s['level']} 절 뒤에 난이도 낮은 절 "
                            f"{len(drop)}개가 온다"))

    # ④ 지나치게 큰 절 — 다만 *안이 보이면* 괜찮다.
    #    큰 것 자체가 문제가 아니라 차례에 한 줄로만 나타나는 것이 문제다.
    for s in sections:
        if s["lines"] >= 170 and s["subs"] < 2:
            out.append(("huge", s,
                        f"{s['lines']}줄인데 3단계 절이 {s['subs']}개 — 차례에서 속이 안 보인다"))
    return out


def build_md(chapters, sections, anom):
    by_ch = {}
    for s in sections:
        by_ch.setdefault(s["chapter"], []).append(s)
    kinds = {}
    for s in sections:
        kinds[s["kind"]] = kinds.get(s["kind"], 0) + 1

    md = ["# 절 단위 지도", "",
          "설명 순서를 고치려면 장보다 잔 눈금이 필요하다. 이 문서는 원고의 2단계",
          "제목을 전부 분해해 *앞질러 참조·되돌아 참조·난이도·성격·분량*을 잰 것이고,",
          "`scripts/section-map.py` 가 원고에서 생성한다.", "",
          f"- 장 {len(chapters)}개, 절 **{len(sections)}개**",
          "- 성격: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())),
          f"- 검토가 필요한 자리 **{len(anom)}건** (아래)", "",
          "## 검토가 필요한 자리", "",
          "| 갈래 | 장 | 절 | 무엇이 걸렸나 |", "|---|---|---|---|"]
    label = {"forward": "앞질러 참조", "roundup-early": "정리가 앞에",
             "hard-first": "난이도 역전", "huge": "너무 큼"}
    for kind, s, why in sorted(anom, key=lambda a: (a[0], a[1]["chapter"])):
        md.append(f"| {label[kind]} | {s['chapter']} | {s['title']} | {why} |")

    md += ["", "## 장별 절 목록", ""]
    for n in sorted(by_ch):
        md += [f"### {n}장 — {chapters[n]}", "",
               "| # | 절 | 성격 | 난이도 | 줄 | 되돌아 | 앞질러 |",
               "|---|---|---|---|---|---|---|"]
        for s in by_ch[n]:
            md.append(f"| {s['index']+1} | {s['title']} | {s['kind']} | "
                      f"{s['level']} | {s['lines']} | "
                      f"{', '.join(str(x) for x in s['back']) or '—'} | "
                      f"{', '.join(str(x) for x in s['forward']) or '—'} |")
        md.append("")
    md += ["---", "", "생성: `python3 scripts/section-map.py`", ""]
    return "\n".join(md)


def build_svg(chapters, sections):
    n = max(chapters)
    left, step = 62, 19
    width = left + 60 + step * (n - 1)
    rows_y, row_h = 120, 26
    height = rows_y + row_h * 4 + 120

    by_ch = {}
    for s in sections:
        by_ch.setdefault(s["chapter"], []).append(s)

    o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
         f'width="{width}" height="{height}" font-family="{FONT}">',
         f'<rect width="{width}" height="{height}" fill="#fff"/>',
         f'<text x="{width/2}" y="34" font-size="19" font-weight="bold" '
         f'text-anchor="middle" fill="#111">절 단위 지도 — 무게·난이도·앞질러 참조</text>',
         f'<text x="{width/2}" y="56" font-size="12" text-anchor="middle" fill="#444">'
         f'가로축은 1장부터 {n}장까지. 칸이 진할수록 값이 크다</text>']

    rows = [("절 수", lambda ss: len(ss), 8),
            ("분량(줄)", lambda ss: sum(x["lines"] for x in ss), 700),
            ("평균 난이도", lambda ss: sum(x["level"] for x in ss) / max(len(ss), 1), 3),
            ("앞질러 참조", lambda ss: sum(len(x["forward"]) for x in ss), 6)]
    for r, (name, fn, cap) in enumerate(rows):
        y = rows_y + r * row_h
        o.append(f'<text x="{left-8}" y="{y+17}" font-size="11" text-anchor="end" '
                 f'fill="#333">{name}</text>')
        for c in range(1, n + 1):
            ss = by_ch.get(c, [])
            v = fn(ss) if ss else 0
            t = min(v / cap, 1.0)
            shade = int(255 - t * 190)
            fill = f"rgb({shade},{shade},{min(255, shade+22)})"
            o.append(f'<rect x="{left+(c-1)*step:.0f}" y="{y}" width="{step-1.5}" '
                     f'height="{row_h-4}" fill="{fill}" stroke="#dde3ea" stroke-width="0.5"/>')
    axis = rows_y + row_h * 4 + 6
    for c in range(1, n + 1):
        if c % 5 == 0 or c == 1 or c == n:
            o.append(f'<text x="{left+(c-1)*step+step/2-1:.0f}" y="{axis+12}" '
                     f'font-size="9" text-anchor="middle" fill="#555">{c}</text>')
    o.append(f'<text x="{left}" y="{axis+44}" font-size="11.5" fill="#333">'
             f'앞질러 참조가 진한 칸이 「뒤에 올 것을 먼저 쓰고 있는」 자리다 — 재배치 후보.</text>')
    o.append("</svg>")
    return "".join(o)


def main() -> int:
    chapters, sections = collect()
    anom = anomalies(chapters, sections)
    out = ROOT / "docs"
    out.mkdir(exist_ok=True)
    (out / "SECTIONS.md").write_text(build_md(chapters, sections, anom), encoding="utf-8")
    (out / "section-heat.svg").write_text(build_svg(chapters, sections), encoding="utf-8")

    if "--json" in sys.argv:
        j = sys.argv[sys.argv.index("--json") + 1]
        pathlib.Path(j).write_text(json.dumps(
            {"chapters": chapters, "sections": sections}, ensure_ascii=False, indent=1),
            encoding="utf-8")

    kinds = {}
    for s in sections:
        kinds[s["kind"]] = kinds.get(s["kind"], 0) + 1
    print(f"section-map: 장 {len(chapters)} · 절 {len(sections)} "
          f"({', '.join(f'{k} {v}' for k, v in sorted(kinds.items()))})")
    tally = {}
    for kind, _s, _w in anom:
        tally[kind] = tally.get(kind, 0) + 1
    print("  검토 대상:", ", ".join(f"{k} {v}" for k, v in sorted(tally.items())) or "없음")
    print("  → docs/SECTIONS.md, docs/section-heat.svg")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
