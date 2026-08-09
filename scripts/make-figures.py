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

# ── 글자 폭 재기 ─────────────────────────────────────────────
# 상자에 글자가 삐져나오는 사고를 막으려면 *재야* 한다. 실제 글꼴로 재고,
# 글꼴이나 PIL 이 없으면 어림값으로 물러선다 --- 그림 생성이 빌드를 막으면 안 된다.
_FONT_FILES = {
    False: ROOT.parent / "toolchains/fonts/noto-cjk-kr/NotoSansCJKkr-Regular.otf",
    True: ROOT.parent / "toolchains/fonts/noto-cjk-kr/NotoSansCJKkr-Bold.otf",
}
_font_cache = {}


def _loaded(size, bold):
    key = (round(size, 2), bold)
    if key not in _font_cache:
        try:
            from PIL import ImageFont
            _font_cache[key] = ImageFont.truetype(str(_FONT_FILES[bold]), int(size * 4))
        except Exception:
            _font_cache[key] = None
    return _font_cache[key]


def text_width(s, size, bold=False):
    """글자의 그려질 폭(SVG 단위). 실제 글꼴로 재는 것이 원칙이다."""
    f = _loaded(size, bold)
    if f is not None:
        return f.getlength(s) / 4
    # 물러선 어림: 한글·한자·가나는 전각, 그 밖은 반각보다 조금 좁게
    wide = sum(1 for c in s if ord(c) > 0x2E80)
    return (wide * 1.0 + (len(s) - wide) * 0.55) * size


def fit_width(labels, size, bold=False, pad=14, minimum=0):
    """이 글자들을 모두 담는 상자 폭. 양판을 함께 넘겨 *넓은 쪽*에 맞춘다."""
    if isinstance(labels, str):
        labels = [labels]
    return max(minimum, max(text_width(t, size, bold) for t in labels) + pad)


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
    w, h = 760, 290
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    left, full = 40, 680

    def row(y, bits, label):
        out.append(text(left, y - 44, label, 12.5, "bold", anchor="start"))
        x = left
        for width_bits, name, hatched in bits:
            bw = full * width_bits / sum(b[0] for b in bits)
            out.append(box(x, y, bw, 46, fill="url(#hatch)" if hatched else "none"))
            # ★ 부호 비트처럼 칸이 1비트뿐이면 이름이 칸보다 넓다. 억지로 넣으면
            #   글자가 옆 칸을 침범하므로, 그럴 때만 이름을 칸 위로 빼고 지시선을 단다.
            if text_width(name, 12.5, True) + 4 <= bw:
                out.append(text(x + bw / 2, y + 21, name, 12.5, "bold"))
            else:
                out.append(text(x + bw / 2, y - 22, name, 11, "bold"))
                out.append(f'<line x1="{x + bw/2}" y1="{y - 18}" x2="{x + bw/2}" '
                           f'y2="{y - 3}" stroke="#111" stroke-width="1"/>')
            out.append(text(x + bw / 2, y + 38, f"{width_bits}", 11, fill="#444"))
            x += bw

    row(62, [(1, L["sign"], False), (8, L["exp"], True), (23, L["frac"], False)],
        f'float — 32 {L["bit"]}')
    row(190, [(1, L["sign"], False), (11, L["exp"], True), (52, L["frac"], False)],
        f'double — 64 {L["bit"]}')
    out.append(text(left, 268, L["note"], 11.5, anchor="start", fill="#444"))
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


# ── F6. 온라인 컴파일러 탐색기의 화면 얼개 (17장) ─────────────
def fig_explorer(L):
    w, h = 760, 250
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 왼쪽: 소스, 오른쪽: 번역 결과
    out.append(box(30, 44, 330, 170, sw=1.8))
    out.append(box(400, 44, 330, 170, sw=1.8))
    out.append(text(195, 64, L["left"], size=12, weight="bold"))
    out.append(text(565, 64, L["right"], size=12, weight="bold"))
    out.append(f'<line x1="30" y1="72" x2="360" y2="72" stroke="#111" stroke-width="1"/>')
    out.append(f'<line x1="400" y1="72" x2="730" y2="72" stroke="#111" stroke-width="1"/>')

    # 짝지어진 줄들 — 한 줄이 여러 줄로 부푸는 모양
    rows = [(90, [90]), (120, [116, 138]), (150, [164]), (180, [186, 202])]
    for i, (ly, rys) in enumerate(rows):
        out.append(f'<line x1="48" y1="{ly}" x2="{200 + (i % 2) * 60}" y2="{ly}" '
                   f'stroke="#111" stroke-width="{3 if i == 1 else 1.4}" opacity="0.85"/>')
        for ry in rys:
            out.append(f'<line x1="418" y1="{ry}" x2="{560 + (ry % 3) * 30}" y2="{ry}" '
                       f'stroke="#111" stroke-width="1.4" opacity="0.6"/>')
        out.append(arrow(305 + (i % 2) * 60, ly, 412, rys[0], sw=1.2 if i != 1 else 2.0))

    out.append(text(w / 2, 238, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)


# ── F7. 기억 시각화 도구가 보여 주는 것 (17장) ────────────────
def fig_viz(L):
    w, h = 760, 240
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 왼쪽 = 스택 프레임, 오른쪽 = 힙
    out.append(box(40, 46, 300, 160, sw=1.8, dash="6 4"))
    out.append(text(190, 66, L["stack"], size=12, weight="bold"))
    out.append(box(430, 46, 290, 160, sw=1.8, dash="6 4"))
    out.append(text(575, 66, L["heap"], size=12, weight="bold"))

    # 스택의 변수 칸 셋
    labels = [L["v1"], L["v2"], L["v3"]]
    vals = [L["v1v"], L["v2v"], L["v3v"]]
    for i, (nm, vv) in enumerate(zip(labels, vals)):
        y = 84 + i * 40
        out.append(box(60, y, 110, 30))
        out.append(box(170, y, 150, 30))
        out.append(text(115, y + 20, nm, size=12))
        out.append(text(245, y + 20, vv, size=12))

    # 힙의 블록
    out.append(box(455, 100, 240, 46))
    for k in range(7):
        out.append(f'<line x1="{455 + 30 * (k + 1)}" y1="100" x2="{455 + 30 * (k + 1)}" '
                   f'y2="146" stroke="#111" stroke-width="1"/>')
    out.append(text(575, 166, L["block"], size=11.5))

    # 포인터 → 힙 화살표
    out.append(arrow(320, 139, 452, 122, sw=2.0))
    out.append(text(386, 108, L["arrow"], size=11.5))

    out.append(text(w / 2, 228, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)



# ── F8. 구조체의 패딩 (43·44장) ───────────────────────────────
def fig_padding(L):
    w, h = 760, 250
    cell = 46
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    def row(y, name, layout, size_note):
        x0 = 120
        out.append(text(x0 - 12, y + 30, name, size=12, weight="bold", anchor="end"))
        for i, kind in enumerate(layout):
            x = x0 + i * cell
            out.append(box(x, y, cell, 44, sw=1.6))
            if kind == ".":
                # 빗금은 패턴 대신 선으로 긋는다 — 어떤 뷰어에서도 그대로 나온다
                for k in range(1, 5):
                    dy = k * 44 / 5
                    out.append(f'<line x1="{x + 3}" y1="{y + dy + 8}" '
                               f'x2="{x + cell - 3}" y2="{y + dy - 8}" '
                               f'stroke="#111" stroke-width="0.9" opacity="0.55"/>')
            else:
                out.append(text(x + cell / 2, y + 28, kind, size=13, weight="bold"))
            out.append(text(x + cell / 2, y - 6, str(i), size=10, fill="#666"))
        out.append(text(x0 + len(layout) * cell + 14, y + 28, size_note,
                        size=12, anchor="start"))

    row(64, L["loose"], ["a", ".", ".", ".", "b", "b", "b", "b", "c", ".", ".", "."],
        L["loose_size"])
    row(160, L["tight"], ["b", "b", "b", "b", "a", "c", ".", "."], L["tight_size"])

    out.append(text(w / 2, 236, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)



# ── F9. 부호 없는 정수는 원 위를 돈다 (7장) ──────────────────
def fig_int_wheel(L):
    import math
    w, h = 760, 380
    cx, cy, r = 380, 208, 104
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))
    out.append(text(w / 2, 46, L["outer"], size=11.5, weight="bold"))
    out.append(text(w / 2, 64, L["inner"], size=11, fill="#666"))
    out.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="#111" '
               f'stroke-width="1.6"/>')

    # 눈금 8개: 0, 32, 64, ... 224 와 255
    marks = [(0, "0"), (32, "32"), (64, "64"), (96, "96"), (128, "128"),
             (160, "160"), (192, "192"), (224, "224")]
    for v, label in marks:
        a = -math.pi / 2 + 2 * math.pi * v / 256
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="#111"/>')
        lx, ly = cx + (r + 20) * math.cos(a), cy + (r + 20) * math.sin(a) + 4
        out.append(text(lx, ly, label, size=11.5))
        # 안쪽에 부호 있는 해석
        sv = v if v < 128 else v - 256
        ix, iy = cx + (r - 22) * math.cos(a), cy + (r - 22) * math.sin(a) + 4
        out.append(text(ix, iy, str(sv), size=10.5, fill="#666"))

    # 255 → 0 으로 넘어가는 자리
    a255 = -math.pi / 2 + 2 * math.pi * 255 / 256
    x, y = cx + r * math.cos(a255), cy + r * math.sin(a255)
    out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="#111"/>')
    out.append(text(x - 28, y - 12, "255", size=11.5))
    out.append(arrow(cx + 120, cy - r + 6, cx + 10, cy - r - 4, sw=2.0))
    out.append(text(cx + 210, cy - r + 10, L["wrap"], size=11.5))

    out.append(text(w / 2, 362, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)


# ── F10. 넓은 그릇으로 옮길 때 앞칸을 무엇으로 채우는가 (7장) ─
def fig_sign_extend(L):
    w, h = 760, 280
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))
    bw = 15

    def row(y, label, fill_bits, value_bits, note):
        out.append(text(30, y + 26, label, size=12, weight="bold", anchor="start"))
        x0 = 214
        for i, b in enumerate(fill_bits + value_bits):
            x = x0 + i * bw
            filled = i < len(fill_bits)
            out.append(box(x, y, bw, 34, sw=1.1))
            out.append(text(x + bw / 2, y + 23, b, size=10,
                            fill="#666" if filled else "#111",
                            weight="normal" if filled else "bold"))
        out.append(f'<line x1="{x0 + len(fill_bits) * bw}" y1="{y - 4}" '
                   f'x2="{x0 + len(fill_bits) * bw}" y2="{y + 38}" '
                   f'stroke="#111" stroke-width="2.2"/>')
        out.append(text(x0 + len(fill_bits) * bw / 2, y - 10, L["filled"], size=10.5, fill="#666"))
        out.append(text(x0 + len(fill_bits) * bw + len(value_bits) * bw / 2, y - 10,
                        L["original"], size=10.5))
        out.append(text(x0 + 32 * bw, y + 52, note, size=11.5, anchor="end",
                        weight="bold"))

    row(74, L["unsigned"], ["0"] * 24, list("11000000"), L["u_note"])
    row(174, L["signed"], ["1"] * 24, list("11000000"), L["s_note"])
    out.append(text(w / 2, 264, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)


# ── F11. 통상 산술 변환의 결정 흐름 (28장) ────────────────────
def fig_conversions(L):
    w, h = 760, 350
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))
    bx, bw_, bh = 90, 380, 46
    steps = [(L["s1"], L["e1"]), (L["s2"], L["e2"]), (L["s3"], L["e3"]), (L["s4"], L["e4"])]
    y = 48
    for i, (s_, e_) in enumerate(steps):
        out.append(box(bx, y, bw_, bh, sw=1.6))
        out.append(text(bx + 14, y + 20, f"{i + 1}. {s_}", size=12, anchor="start",
                        weight="bold"))
        out.append(text(bx + 14, y + 38, e_, size=11, anchor="start", fill="#444"))
        if i < len(steps) - 1:
            out.append(arrow(bx + bw_ / 2, y + bh, bx + bw_ / 2, y + bh + 22, sw=1.6))
        y += bh + 22

    # 오른쪽 예시
    out.append(box(bx + bw_ + 40, 48, 200, y - 70, sw=1.2, dash="5 4"))
    out.append(text(bx + bw_ + 140, 72, L["ex_title"], size=12, weight="bold"))
    for k, line in enumerate(L["ex"]):
        out.append(text(bx + bw_ + 56, 100 + k * 22, line, size=11, anchor="start"))
    out.append(text(w / 2, 336, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)


# ── F12. 배열이 포인터로 무너질 때 잃는 것 (37장) ─────────────
def fig_decay(L):
    w, h = 760, 272
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 왼쪽: 배열
    x0, y0, cw = 60, 78, 52
    out.append(text(x0, y0 - 14, L["array"], size=12, weight="bold", anchor="start"))
    for i in range(5):
        out.append(box(x0 + i * cw, y0, cw, 44))
        out.append(text(x0 + i * cw + cw / 2, y0 + 28, f"a[{i}]", size=11))
    out.append(text(x0, y0 + 108, L["a_type"], size=11, anchor="start"))
    out.append(text(x0, y0 + 128, L["a_size"], size=11, anchor="start"))

    # 오른쪽: 포인터
    px = 470
    out.append(text(px, y0 - 14, L["ptr"], size=12, weight="bold", anchor="start"))
    out.append(box(px, y0, 120, 44))
    out.append(text(px + 60, y0 + 28, L["ptr_val"], size=11))
    out.append(text(px, y0 + 108, L["p_type"], size=11, anchor="start"))
    out.append(text(px, y0 + 128, L["p_size"], size=11, anchor="start"))

    # 같은 주소를 잇는 선 — 상자 아래로 돌아 a[0] 밑을 가리킨다
    ly = y0 + 74
    out.append(f'<line x1="{px + 8}" y1="{y0 + 44}" x2="{px + 8}" y2="{ly}" '
               f'stroke="#111" stroke-width="1.8"/>')
    out.append(f'<line x1="{px + 8}" y1="{ly}" x2="{x0 + cw / 2}" y2="{ly}" '
               f'stroke="#111" stroke-width="1.8"/>')
    out.append(arrow(x0 + cw / 2, ly, x0 + cw / 2, y0 + 48, sw=1.8))
    out.append(text((px + x0) / 2 + 40, ly - 8, L["same"], size=11))
    out.append(text(w / 2, 232, L["lost"], size=12, weight="bold"))
    out.append(text(w / 2, 252, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)


# ── F13. 통째로 쓴 바이트와 필드별로 쓴 바이트 (44장) ─────────
def fig_serialize(L):
    w, h = 760, 290
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))
    cw = 44

    def row(y, label, cells, note):
        out.append(text(46, y + 28, label, size=12, weight="bold", anchor="start"))
        x0 = 210
        for i, c in enumerate(cells):
            x = x0 + i * cw
            out.append(box(x, y, cw, 42, sw=1.5))
            if c == ".":
                for k in range(1, 5):
                    dy = k * 42 / 5
                    out.append(f'<line x1="{x + 3}" y1="{y + dy + 7}" '
                               f'x2="{x + cw - 3}" y2="{y + dy - 7}" stroke="#111" '
                               f'stroke-width="0.9" opacity="0.55"/>')
            else:
                out.append(text(x + cw / 2, y + 27, c, size=11, weight="bold"))
        out.append(text(x0 + len(cells) * cw + 12, y + 27, note, size=11.5,
                        anchor="start"))

    row(64, L["whole"], ["k", ".", ".", ".", "i", "i", "i", "i", "f", "f", ".", "."],
        L["whole_note"])
    row(168, L["fields"], ["k", "i", "i", "i", "i", "f", "f"], L["fields_note"])
    out.append(text(w / 2, 262, L["note"], size=11.5))
    out.append(TAIL)
    return "".join(out)



# ── F14. 타입의 분류 나무 (26장) ──────────────────────────────
def fig_type_tree(L):
    W, H = 760, 430
    o = [HEAD.format(w=W, h=H, f=FONT), DEFS]
    o.append(text(W/2, 22, L["title"], 14, "bold"))

    # 최상단: 타입
    o.append(box(320, 36, 120, 30)); o.append(text(380, 56, L["type"], 13, "bold"))
    # 두 갈래: 객체 타입 / 함수 타입
    o.append(box(150, 92, 160, 30)); o.append(text(230, 112, L["object"], 12))
    o.append(box(470, 92, 160, 30)); o.append(text(550, 112, L["function"], 12))
    o.append(arrow(370, 66, 250, 90)); o.append(arrow(390, 66, 530, 90))

    # 객체 타입 아래 두 줄: 산술 / 그 밖(포인터·배열·구조체·공용체·void)
    o.append(box(60, 152, 175, 28)); o.append(text(147, 171, L["arith"], 12, "bold"))
    o.append(box(255, 152, 260, 28)); o.append(text(385, 171, L["other"], 12))
    o.append(arrow(210, 122, 160, 150)); o.append(arrow(250, 122, 340, 150))

    # 산술 = 정수 + 부동소수점
    o.append(box(30, 208, 120, 26)); o.append(text(90, 226, L["integer"], 11))
    o.append(box(160, 208, 120, 26)); o.append(text(220, 226, L["floating"], 11))
    o.append(arrow(130, 180, 95, 206)); o.append(arrow(165, 180, 215, 206))

    # 정수 = char + 부호 있는/없는 + 열거
    # ★ 상자 폭은 손으로 적지 않는다 --- 글자를 재서 맞춘다. 판마다 그림을 따로
    #   만드므로 폭도 그 판의 라벨에 맞춘다(영어가 길면 영어판 상자가 넓어진다).
    int_keys = ("chars", "signed", "unsigned", "enums")
    iw = fit_width([L[k] for k in int_keys], 10, minimum=150)
    for i, key in enumerate(int_keys):
        o.append(box(20, 258 + i*30, iw, 24))
        o.append(text(20 + iw/2, 275 + i*30, L[key], 10))
    o.append(arrow(90, 234, 90, 256))

    # 겹치는 이름들 — 점선 상자로 '이 묶음도 이름이 있다'
    o.append(box(300, 208, 215, 26, dash="4 3"))
    o.append(text(407, 226, L["real"], 11))
    o.append(box(300, 248, 215, 26, dash="4 3"))
    o.append(text(407, 266, L["scalar"], 11))
    o.append(box(300, 288, 215, 26, dash="4 3"))
    o.append(text(407, 306, L["aggregate"], 11))
    o.append(box(300, 328, 215, 26, dash="4 3"))
    o.append(text(407, 346, L["basic"], 11))

    # 파생 타입 묶음
    o.append(box(545, 152, 195, 28)); o.append(text(642, 171, L["derived"], 12))
    o.append(arrow(560, 122, 620, 150))
    d_keys = ("d1", "d2", "d3")
    dw = fit_width([L[k] for k in d_keys], 10, minimum=175)
    for i, key in enumerate(d_keys):
        o.append(box(W - 30 - dw, 196 + i*30, dw, 24))
        o.append(text(W - 30 - dw/2, 213 + i*30, L[key], 10))
    o.append(arrow(W - 30 - dw/2, 180, W - 30 - dw/2, 194))

    o.append(text(W/2, H-14, L["note"], 10.5))
    o.append(TAIL)
    return "".join(o)


FIGS = {
    "regions": (fig_regions, {
        "ko": dict(title="한 프로그램의 기억 배치", names=["코드", "정적 구역", "힙 (창고)", "스택 (작업대)"],
                   subs=["명령들 · 읽기 전용", "전역 · 프로그램 내내", "빌리고 돌려준다", "함수가 도는 동안"],
                   grow_stack="스택은 이쪽으로 자란다", grow_heap="힙은 이쪽으로 자란다",
                   low="낮은 주소", high="높은 주소"),
        "en": dict(title="the memory layout of one program", names=["code", "static", "heap", "stack"],
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
    "explorer": (fig_explorer, {
        "ko": dict(title="온라인 컴파일러 탐색기가 보여 주는 것",
                   left="내가 적은 C", right="컴파일러가 낸 기계어(어셈블리)",
                   note="한 줄이 몇 줄이 되는지, 어떤 줄이 아예 사라지는지가 보인다 — 13장의 최적화를 눈으로."),
        "en": dict(title="what an online compiler explorer shows",
                   left="the C you wrote", right="the machine code the compiler produced",
                   note="you see how one line becomes several, and which lines vanish — chapter 13's optimization, visible."),
    }),
    "viz": (fig_viz, {
        "ko": dict(title="기억 시각화 도구가 보여 주는 것", stack="스택 (지금 도는 함수)",
                   heap="힙 (malloc 이 준 자리)", v1="int n", v1v="7",
                   v2="int *p", v2v="→", v3="char *buf", v3v="→",
                   block="여덟 칸짜리 한 덩어리", arrow="가리킨다",
                   note="값은 상자로, 포인터는 화살표로 그린다 — 34장의 '가리킨다'가 그림이 된다."),
        "en": dict(title="what a memory visualiser shows", stack="the stack (the running function)",
                   heap="the heap (what malloc gave)", v1="int n", v1v="7",
                   v2="int *p", v2v="→", v3="char *buf", v3v="→",
                   block="one block of eight slots", arrow="points at",
                   note="values are boxes and pointers are arrows — chapter 34's \u201cpoints at\u201d, drawn."),
    }),
    "padding": (fig_padding, {
        "ko": dict(title="같은 멤버, 순서만 바꾸면 크기가 준다",
                   loose="loose", tight="tight",
                   loose_size="12바이트", tight_size="8바이트",
                   note="빗금이 패딩이다. int 는 4의 배수 자리에 놓여야 하고, 끝에도 다음 원소를 위한 자리가 붙는다."),
        "en": dict(title="the same members reordered take less room",
                   loose="loose", tight="tight",
                   loose_size="12 bytes", tight_size="8 bytes",
                   note="the hatched cells are padding: an int must sit at a multiple of four, and the tail keeps the next element aligned."),
    }),
    "int-wheel": (fig_int_wheel, {
        "ko": dict(title="부호 없는 정수는 원 위를 돈다 (8비트)",
                   wrap="255 + 1 = 0 — 감아 돈다", outer="바깥: unsigned char (0~255)",
                   inner="안쪽: 같은 비트를 signed char 로 (−128~127)",
                   note="감아 도는 것은 부호 없는 쪽의 약속이다 — 부호 있는 쪽의 넘침은 정의되지 않은 동작이다."),
        "en": dict(title="unsigned integers ride a wheel (8 bits)",
                   wrap="255 + 1 = 0 — it wraps", outer="outside: unsigned char (0-255)",
                   inner="inside: the same bits as signed char (-128 to 127)",
                   note="wrapping is a promise on the unsigned side; signed overflow is undefined behaviour."),
    }),
    "sign-extend": (fig_sign_extend, {
        "ko": dict(title="좁은 그릇에서 넓은 그릇으로 — 앞칸은 무엇으로 채우는가",
                   unsigned="unsigned char 0xC0 → int", signed="signed char 0xC0 → int",
                   filled="채워지는 24칸", original="원래의 8비트",
                   u_note="= 192 (0 으로 채운다)", s_note="= −64 (부호 비트로 채운다)",
                   note="같은 비트 0xC0 인데 원래 타입의 부호에 따라 채우는 값이 다르고, 그래서 값이 달라진다."),
        "en": dict(title="from a narrow vessel to a wide one — what fills the front",
                   unsigned="unsigned char 0xC0 -> int", signed="signed char 0xC0 -> int",
                   filled="the 24 cells filled in", original="the original 8 bits",
                   u_note="= 192 (filled with 0)", s_note="= -64 (filled with the sign bit)",
                   note="the same bits, 0xC0: the fill depends on the source type's signedness, and so does the value."),
    }),
    "conversions": (fig_conversions, {
        "ko": dict(title="통상 산술 변환 — 두 피연산자를 한 타입으로 맞추는 순서",
                   s1="정수 승격", e1="int 보다 좁은 타입은 먼저 int(또는 unsigned int)로",
                   s2="부호가 같은가?", e2="같으면 순위가 큰 쪽으로 맞춘다",
                   s3="부호 없는 쪽의 순위가 크거나 같은가?", e3="그렇다면 부호 없는 쪽으로",
                   s4="부호 있는 쪽이 상대의 모든 값을 담는가?",
                   e4="담으면 부호 있는 쪽으로, 아니면 그 짝인 부호 없는 타입으로",
                   ex_title="보기", ex=["int + unsigned", "→ 3번에서 멈춘다", "→ unsigned",
                                        "", "−1 이 아주 큰 수가", "되는 사고가 여기서"],
                   note="네 단계를 순서대로 따른다. 대부분의 사고는 3번에서 난다."),
        "en": dict(title="the usual arithmetic conversions — the order of matching two operands",
                   s1="integer promotion", e1="anything narrower than int becomes int (or unsigned int)",
                   s2="same signedness?", e2="if so, take the greater rank",
                   s3="is the unsigned rank greater or equal?", e3="if so, the unsigned type wins",
                   s4="can the signed type hold every value of the other?",
                   e4="if so, the signed type; otherwise its unsigned counterpart",
                   ex_title="example", ex=["int + unsigned", "-> stops at step 3", "-> unsigned",
                                           "", "this is where -1", "becomes a huge number"],
                   note="follow the four in order; most accidents happen at step 3."),
    }),
    "decay": (fig_decay, {
        "ko": dict(title="배열이 포인터로 무너질 때 잃는 것", array="int a[5]",
                   ptr="int *p = a;", ptr_val="a[0] 의 주소",
                   a_type="타입: int[5]", a_size="sizeof a = 20",
                   p_type="타입: int *", p_size="sizeof p = 8",
                   same="같은 주소", lost="잃은 것: 원소가 몇 개인가",
                   note="주소는 같고 타입이 다르다. 그래서 함수 안에서는 sizeof 로 길이를 알 수 없다."),
        "en": dict(title="what an array loses when it decays to a pointer", array="int a[5]",
                   ptr="int *p = a;", ptr_val="the address of a[0]",
                   a_type="type: int[5]", a_size="sizeof a = 20",
                   p_type="type: int *", p_size="sizeof p = 8",
                   same="the same address", lost="what is lost: how many elements there are",
                   note="the address is the same and the type is not, which is why sizeof cannot give a length inside a function."),
    }),
    "serialize": (fig_serialize, {
        "ko": dict(title="통째로 쓴 바이트와 필드별로 쓴 바이트",
                   whole="통째로 쓰기", fields="필드별로 쓰기",
                   whole_note="12바이트 — 빗금이 함께 나간다",
                   fields_note="7바이트 — 내가 정한 순서",
                   note="위쪽은 패딩이 함께 나가고 배치가 컴파일러·플랫폼에 달렸다. 아래쪽은 어느 기계에서 읽어도 같다."),
        "en": dict(title="the bytes written whole, and the bytes written field by field",
                   whole="written whole", fields="written field by field",
                   whole_note="12 bytes — the hatching goes out too",
                   fields_note="7 bytes — the order we chose",
                   note="above, padding goes out and the layout depends on compiler and platform; below reads the same on any machine."),
    }),
    "type-tree": (fig_type_tree, {
        "ko": dict(title="표준이 가른 타입의 갈래 (§6.2.5)", type="타입",
                   object="객체 타입", function="함수 타입",
                   arith="산술 타입", other="포인터·배열·구조체·공용체·void",
                   integer="정수 타입", floating="부동소수점 타입",
                   chars="char / signed char / unsigned char",
                   signed="부호 있는 정수 타입 (short·int·long…)",
                   unsigned="부호 없는 정수 타입 (bool 포함)",
                   enums="열거 타입",
                   real="실수 타입 = 정수 + 실수 부동소수점",
                   scalar="스칼라 = 산술 + 포인터 + nullptr_t",
                   aggregate="집합체 = 배열 + 구조체 (공용체 제외)",
                   basic="기본 타입 = char + 정수 + 부동소수점",
                   derived="파생 타입", d1="배열 · 구조체 · 공용체",
                   d2="함수 · 포인터", d3="원자적(_Atomic)",
                   note="점선 상자는 '겹쳐 부르는 이름'이다 — 나무의 가지가 아니라 여러 가지를 묶은 낱말."),
        "en": dict(title="how the standard divides types (§6.2.5)", type="type",
                   object="object type", function="function type",
                   arith="arithmetic type", other="pointer, array, struct, union, void",
                   integer="integer types", floating="floating types",
                   chars="char / signed char / unsigned char",
                   signed="signed integer types (short, int, long…)",
                   unsigned="unsigned integer types (bool included)",
                   enums="enumerated types",
                   real="real types = integer + real floating",
                   scalar="scalar = arithmetic + pointer + nullptr_t",
                   aggregate="aggregate = array + struct (not union)",
                   basic="basic types = char + integer + floating",
                   derived="derived types", d1="array, struct, union",
                   d2="function, pointer", d3="atomic (_Atomic)",
                   note="a dashed box is a collective name — not a branch of the tree but a word gathering several."),
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
