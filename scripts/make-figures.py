#!/usr/bin/env python3
"""개념도(SVG)를 생성한다 — 손으로 좌표를 적지 않는다.

원칙(docs/backlogs/items/2026-08-06-figures.md):
  - 공간 관계가 본질인 곳에만 그린다.
  - 흑백으로 완결한다(색 없이도 뜻이 남게 선 굵기와 해칭으로 구분).
  - 글자가 든 그림은 판마다 따로 만든다 → book/figures/{ko,en}/*.svg
  - PDF·HTML 모두 같은 파일을 쓴다(Typst 의 image()).

사용법: python3 scripts/make-figures.py
"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONT = "Noto Sans CJK KR, Noto Sans, sans-serif"

HEAD = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
        'width="{w}" height="{h}" font-family="{f}">'
        '<rect width="{w}" height="{h}" fill="none"/>')
TAIL = "</svg>"


def text(x, y, s, size=13, weight="normal", anchor="middle", fill="#111"):
    return (f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" '
            f'text-anchor="{anchor}" fill="{fill}">{s}</text>')


def box(x, y, w, h, sw=1.6, dash=None, fill="none"):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" '
            f'stroke="#111" stroke-width="{sw}"{d}/>')


def arrow(x1, y1, x2, y2, sw=1.6):
    return (f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#111" '
            f'stroke-width="{sw}" marker-end="url(#a)"/>')


DEFS = ('<defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" '
        'markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
        '<path d="M0,0 L10,5 L0,10 z" fill="#111"/></marker>'
        '<pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" '
        'patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="6" '
        'stroke="#111" stroke-width="1" opacity="0.35"/></pattern></defs>')


# ── F1. 기억의 구역 지도 (2장) ────────────────────────────────
def fig_regions(L):
    w, h = 760, 210
    y, bh = 70, 62
    xs = [30, 190, 350, 570]
    widths = [150, 150, 210, 160]
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    for x, bw, name, sub in zip(xs, widths, L["names"], L["subs"]):
        out.append(box(x, y, bw, bh))
        out.append(text(x + bw / 2, y + 26, name, 15, "bold"))
        out.append(text(x + bw / 2, y + 46, sub, 11.5, fill="#444"))
    # 스택과 힙이 마주 자란다
    out.append(text(w / 2, 34, L["title"], 14, "bold"))
    out.append(arrow(560, y + bh + 24, 380, y + bh + 24))
    out.append(text(470, y + bh + 44, L["grow_stack"], 11.5))
    out.append(arrow(580, y + bh + 24, 720, y + bh + 24))
    out.append(text(650, y + bh + 44, L["grow_heap"], 11.5))
    out.append(f'<line x1="30" y1="{y + bh + 8}" x2="730" y2="{y + bh + 8}" '
               f'stroke="#111" stroke-width="1" opacity="0.4"/>')
    out.append(text(30, y - 12, L["low"], 11.5, anchor="start", fill="#444"))
    out.append(text(730, y - 12, L["high"], 11.5, anchor="end", fill="#444"))
    out.append(TAIL)
    return "".join(out)


# ── F3. IEEE 754 비트 배치 (8장) ──────────────────────────────
def fig_ieee(L):
    w, h = 760, 250
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    left, full = 40, 680

    def row(y, bits, label):
        out.append(text(left, y - 8, label, 12.5, "bold", anchor="start"))
        x = left
        for width_bits, name, hatched in bits:
            bw = full * width_bits / sum(b[0] for b in bits)
            out.append(box(x, y, bw, 46, fill="url(#hatch)" if hatched else "none"))
            out.append(text(x + bw / 2, y + 21, name, 12.5, "bold"))
            out.append(text(x + bw / 2, y + 38, f"{width_bits}", 11, fill="#444"))
            x += bw

    row(50, [(1, L["sign"], False), (8, L["exp"], True), (23, L["frac"], False)],
        f'float — 32 {L["bit"]}')
    row(150, [(1, L["sign"], False), (11, L["exp"], True), (52, L["frac"], False)],
        f'double — 64 {L["bit"]}')
    out.append(text(left, 228, L["note"], 11.5, anchor="start", fill="#444"))
    out.append(TAIL)
    return "".join(out)


# ── F6. 포인터 산술 (36장) ────────────────────────────────────
def fig_ptrmath(L):
    w, h = 760, 220
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    x0, y, cell, n = 60, 90, 78, 8
    for i in range(n):
        out.append(box(x0 + i * cell, y, cell, 52))
        out.append(text(x0 + i * cell + cell / 2, y + 31, f"a[{i}]", 13))
    out.append(text(x0, y - 40, L["title"], 14, "bold", anchor="start"))
    # p, p+1, p+3 화살표
    for i, lab in ((0, "p"), (1, "p+1"), (3, "p+3")):
        cx = x0 + i * cell + cell / 2
        out.append(arrow(cx, y - 26, cx, y - 4))
        out.append(text(cx, y - 32, lab, 12.5, "bold"))
    # one past the end
    cx = x0 + n * cell
    out.append(f'<line x1="{cx}" y1="{y}" x2="{cx}" y2="{y + 52}" stroke="#111" '
               f'stroke-width="1.6" stroke-dasharray="4 3"/>')
    out.append(arrow(cx, y - 26, cx, y - 4))
    out.append(text(cx, y - 32, L["past"], 11.5, "bold"))
    # 간격 표시
    out.append(f'<line x1="{x0}" y1="{y + 74}" x2="{x0 + cell}" y2="{y + 74}" '
               f'stroke="#111" stroke-width="1.4" marker-start="url(#a)" '
               f'marker-end="url(#a)"/>')
    out.append(text(x0 + cell / 2, y + 94, L["step"], 12))
    out.append(text(x0 + cell * 4.6, y + 94, L["rule"], 12, anchor="start", fill="#444"))
    out.append(TAIL)
    return "".join(out)


# ── F8. 다차원 배열의 두 번의 점프 (38장) ────────────────────
def fig_mdarray(L):
    w, h = 780, 250
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    x0, y, cell = 50, 96, 56
    # 12칸을 한 줄로 — 행 경계는 굵은 선으로 나눈다
    for i in range(12):
        out.append(box(x0 + i * cell, y, cell, 46))
        out.append(text(x0 + i * cell + cell / 2, y + 29,
                        f"{i // 4},{i % 4}", 12))
    for r in (0, 4, 8, 12):
        x = x0 + r * cell
        out.append(f'<line x1="{x}" y1="{y - 6}" x2="{x}" y2="{y + 52}" '
                   f'stroke="#111" stroke-width="2.6"/>')
    out.append(text(x0, y - 46, L["title"], 14, "bold", anchor="start"))
    for r in range(3):
        out.append(text(x0 + (r * 4 + 2) * cell, y - 14, L["row"].format(r), 12,
                        fill="#444"))
    # 점프 1: a + 2  (행 크기)
    x_from, x_to = x0, x0 + 8 * cell
    out.append(f'<path d="M {x_from} {y + 78} C {x_from} {y + 118}, '
               f'{x_to} {y + 118}, {x_to} {y + 78}" fill="none" stroke="#111" '
               f'stroke-width="1.6" marker-end="url(#a)"/>')
    out.append(text((x_from + x_to) / 2, y + 116, L["jump1"], 12.5, "bold"))
    # 점프 2: +1 (원소 크기)
    x2_from, x2_to = x0 + 8 * cell, x0 + 9 * cell
    out.append(f'<line x1="{x2_from}" y1="{y + 66}" x2="{x2_to}" y2="{y + 66}" '
               f'stroke="#111" stroke-width="1.6" marker-end="url(#a)"/>')
    out.append(text(x2_to + 84, y + 70, L["jump2"], 12.5, "bold"))
    out.append(text(x0, h - 14, L["note"], 12, anchor="start", fill="#444"))
    out.append(TAIL)
    return "".join(out)


# ── F7. 포인터 값에 붙은 세 가지 (33장) ──────────────────────
def fig_pointer_parts(L):
    w, h = 700, 250
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    cx, cy = 210, 120
    out.append(box(cx - 110, cy - 34, 220, 68, sw=2.4))
    out.append(text(cx, cy - 6, L["value"], 15, "bold"))
    out.append(text(cx, cy + 18, L["value_sub"], 11.5, fill="#444"))
    items = [(L["p1"], L["p1s"]), (L["p2"], L["p2s"]), (L["p3"], L["p3s"])]
    for i, (name, sub) in enumerate(items):
        y = 46 + i * 66
        out.append(box(430, y, 240, 52, dash="5 3"))
        out.append(text(550, y + 22, name, 13.5, "bold"))
        out.append(text(550, y + 40, sub, 11, fill="#444"))
        out.append(arrow(cx + 112, cy, 428, y + 26))
    out.append(text(cx, 30, L["title"], 14, "bold"))
    out.append(TAIL)
    return "".join(out)


FIGS = {
    "regions": (fig_regions, {
        "ko": dict(title="한 프로그램의 기억 지도", names=["코드", "정적 구역", "힙 (창고)", "스택 (작업대)"],
                   subs=["명령들 · 읽기 전용", "전역 · 프로그램 내내", "빌리고 돌려준다", "함수가 도는 동안"],
                   grow_stack="스택은 이쪽으로 자란다", grow_heap="힙은 이쪽으로 자란다",
                   low="낮은 주소", high="높은 주소"),
        "en": dict(title="the memory map of one program", names=["code", "static", "heap", "stack"],
                   subs=["instructions · read only", "globals · whole program", "borrowed and returned",
                         "while a function runs"],
                   grow_stack="the stack grows this way", grow_heap="the heap grows this way",
                   low="low addresses", high="high addresses"),
    }),
    "ieee754": (fig_ieee, {
        "ko": dict(sign="부호", exp="지수", frac="가수", bit="비트",
                   note="지수는 바이어스를 더해 저장하고, 가수의 맨 앞 1은 저장하지 않는다(숨은 비트)."),
        "en": dict(sign="sign", exp="exponent", frac="fraction", bit="bits",
                   note="The exponent is stored with a bias; the leading 1 of the fraction is not stored (the hidden bit)."),
    }),
    "ptrmath": (fig_ptrmath, {
        "ko": dict(title="포인터의 +1 은 한 칸이 아니라 한 원소다", past="a+8 (끝 다음)",
                   step="+1", rule="간격 = sizeof(원소). 끝 다음 자리는 만들 수 있으나 따라가면 안 된다"),
        "en": dict(title="+1 on a pointer moves one element, not one byte", past="a+8 (one past)",
                   step="+1", rule="the step is sizeof(element); one past the end may be formed, never followed"),
    }),
    "mdarray": (fig_mdarray, {
        "ko": dict(title="a[2][1] 을 찾아가는 두 번의 점프 (int a[3][4])",
                   row="행 {0}", jump1="a + 2 → 행 크기(16바이트)씩 두 번",
                   jump2="+1 → 원소 크기(4바이트)",
                   note="기억은 한 줄이다. 행 경계는 굵은 선일 뿐, 빈틈이 아니다."),
        "en": dict(title="the two jumps that reach a[2][1] (int a[3][4])",
                   row="row {0}", jump1="a + 2 → twice the row size (16 bytes)",
                   jump2="+1 → one element (4 bytes)",
                   note="memory is one run; the thick lines are row boundaries, not gaps."),
    }),
    "pointer-parts": (fig_pointer_parts, {
        "ko": dict(title="포인터 값에 붙어 다니는 것", value="포인터 값", value_sub="복사·비교할 수 있다",
                   p1="주소", p1s="어느 자리를 가리키는가", p2="가리키는 타입",
                   p2s="몇 바이트를 어떤 눈으로 읽는가", p3="출처(프로버넌스)",
                   p3s="어느 객체에서 나왔는가 — 어디까지 갈 수 있는가"),
        "en": dict(title="what travels with a pointer value", value="pointer value",
                   value_sub="can be copied and compared", p1="address", p1s="which place it points at",
                   p2="pointed-at type", p2s="how many bytes, through what eye",
                   p3="provenance", p3s="which object it came from — how far it may go"),
    }),
}


def main() -> int:
    made = 0
    for lang in ("ko", "en"):
        out_dir = ROOT / "book" / "figures" / lang
        out_dir.mkdir(parents=True, exist_ok=True)
        for name, (fn, labels) in FIGS.items():
            (out_dir / f"{name}.svg").write_text(fn(labels[lang]), encoding="utf-8")
            made += 1
    print(f"make-figures: {made}개 SVG 생성 (book/figures/ko, book/figures/en)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
