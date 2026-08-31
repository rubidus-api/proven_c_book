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
    # ★ SVG 는 XML 이다 --- `&p` 같은 글자를 그대로 쓰면 개체 참조로 읽혀 빌드가
    #   멈춘다("malformed entity reference"). 여기 한 곳에서 막는다.
    s = (str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))
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



# ── F15. 포인터의 배열 (40장) ─────────────────────────────────
def fig_ptr_array(L):
    w, h = 760, 300
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 왼쪽 --- 포인터 셋이 나란히 (연속)
    x0, y0, cw, ch = 48, 74, 118, 40
    out.append(text(x0, y0 - 14, L["arr"], size=12, weight="bold", anchor="start"))
    for i in range(3):
        y = y0 + i * ch
        out.append(box(x0, y, cw, ch))
        out.append(text(x0 + cw / 2, y + 25, L["addr"][i], size=11))
        out.append(text(x0 - 8, y + 25, f"[{i}]", size=10.5, anchor="end"))
    out.append(text(x0, y0 + 3 * ch + 20, L["arr_note"], size=10.5, anchor="start"))

    # 오른쪽 --- 길이가 제각각인 문자열들 (흩어져 있다)
    sx = 360
    ys = [y0 - 6, y0 + 58, y0 + 120]
    for i, (s, a) in enumerate(zip(L["str"], L["addr"])):
        cells = len(s) + 1
        for j in range(cells):
            out.append(box(sx + j * 26, ys[i], 26, 30))
            ch_ = s[j] if j < len(s) else "\\0"
            out.append(text(sx + j * 26 + 13, ys[i] + 20, ch_, size=11))
        out.append(text(sx - 10, ys[i] + 20, a, size=10.5, anchor="end"))
        out.append(arrow(x0 + cw + 6, y0 + i * ch + 20, sx - 62, ys[i] + 15))
    out.append(text(sx, ys[2] + 52, L["str_note"], size=10.5, anchor="start"))

    out.append(text(w / 2, h - 16, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


# ── F16. 포인터의 포인터 (40장) ───────────────────────────────
def fig_ptr_ptr(L):
    w, h = 760, 342
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 부르는 쪽
    cx, cy = 60, 86
    bw = fit_width([L["caller_var"], L["before"]], 11, minimum=190)
    out.append(text(cx, cy - 16, L["caller"], size=12, weight="bold", anchor="start"))
    out.append(box(cx, cy, bw, 46))
    out.append(text(cx + bw / 2, cy + 20, L["caller_var"], size=11))
    out.append(text(cx + bw / 2, cy + 38, L["before"], size=10.5))
    out.append(text(cx, cy + 70, L["caller_addr"], size=10.5, anchor="start"))

    # 넘기는 값 --- 그 변수의 주소
    mx = cx + bw + 40
    out.append(arrow(cx + bw + 6, cy + 23, mx + 54, cy + 23))
    out.append(text(mx + 30, cy + 14, L["handoff"], size=10.5, anchor="start"))

    # 부름받는 쪽
    fx = mx + 60
    fw = fit_width([L["param"], L["param_val"]], 11, minimum=200)
    out.append(text(fx, cy - 16, L["callee"], size=12, weight="bold", anchor="start"))
    out.append(box(fx, cy, fw, 46))
    out.append(text(fx + fw / 2, cy + 20, L["param"], size=11))
    out.append(text(fx + fw / 2, cy + 38, L["param_val"], size=10.5))

    # 되돌아가는 화살표 --- *out = buf
    ly = cy + 108
    out.append(f'<line x1="{fx + fw / 2}" y1="{cy + 46}" x2="{fx + fw / 2}" y2="{ly}" '
               f'stroke="#111" stroke-width="1.8"/>')
    out.append(f'<line x1="{fx + fw / 2}" y1="{ly}" x2="{cx + bw / 2}" y2="{ly}" '
               f'stroke="#111" stroke-width="1.8"/>')
    out.append(arrow(cx + bw / 2, ly, cx + bw / 2, cy + 50))
    out.append(text((fx + cx) / 2 + 40, ly + 18, L["write"], size=11, weight="bold"))

    # 새로 잡은 자리
    hx, hy = fx, ly + 30
    out.append(box(hx, hy, fw, 38, dash="5 4"))
    out.append(text(hx + fw / 2, hy + 24, L["heap"], size=11))

    # 요약 두 줄 --- 어떤 상자 안에도 들어가지 않도록 아래에 둔다
    out.append(text(w / 2, h - 42, L["after_line"], size=11.5))
    out.append(text(w / 2, h - 16, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


# ── F17. 구조체의 배열 — 원소 사이에는 빈자리가 없다 (47장) ────
def fig_struct_array(L):
    w, h = 760, 306
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))
    out.append(text(w / 2, 46, L["decl"], size=11.5))

    # 한 원소의 속 --- 멤버 + 꼬리 패딩
    x0, y0, unit = 60, 84, 40           # 1바이트 = 40
    parts = [(L["m1"], 4, False), (L["m2"], 1, False), (L["pad"], 3, True)]
    x = x0
    out.append(text(x0, y0 - 14, L["one"], size=12, weight="bold", anchor="start"))
    for name, n, is_pad in parts:
        out.append(box(x, y0, unit * n, 44, fill="url(#hatch)" if is_pad else "none"))
        out.append(text(x + unit * n / 2, y0 + 28, name, size=11))
        x += unit * n
    out.append(text(x + 12, y0 + 28, L["size"], size=11, anchor="start", weight="bold"))

    # 배열 --- 원소 셋이 딱 붙어 있다
    ay, aunit = 176, 26
    out.append(text(x0, ay - 14, L["arr"], size=12, weight="bold", anchor="start"))
    for e in range(3):
        ex = x0 + e * aunit * 8
        for name, n, is_pad in parts:
            out.append(box(ex, ay, aunit * n, 40, fill="url(#hatch)" if is_pad else "none"))
            ex += aunit * n
        out.append(text(x0 + e * aunit * 8 + aunit * 4, ay - 8, f"[{e}]", size=11, weight="bold"))
        out.append(text(x0 + e * aunit * 8, ay + 58, L["off"][e], size=10.5, anchor="middle"))
    out.append(text(x0 + aunit * 24 + 14, ay + 25, L["stride"], size=11, anchor="start", weight="bold"))

    out.append(text(w / 2, h - 40, L["why"], size=11.5))
    out.append(text(w / 2, h - 18, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


# ── F18. 유연 배열 멤버 — 한 덩어리와 두 덩어리 (46장) ─────────
def fig_flex_array(L):
    w, h = 760, 306
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    # 위 --- 포인터 멤버: 할당 둘
    x0, y0 = 60, 74
    out.append(text(x0, y0 - 14, L["a_head"], size=12, weight="bold", anchor="start"))
    out.append(box(x0, y0, 90, 42))
    out.append(text(x0 + 45, y0 + 26, L["len"], size=11))
    out.append(box(x0 + 90, y0, 120, 42))
    out.append(text(x0 + 150, y0 + 26, L["ptr"], size=11))
    bx = x0 + 330
    out.append(box(bx, y0, 300, 42, dash="5 4"))
    out.append(text(bx + 150, y0 + 26, L["buf"], size=11))
    out.append(arrow(x0 + 210, y0 + 21, bx - 6, y0 + 21))
    out.append(text(x0, y0 + 62, L["a_note"], size=10.5, anchor="start"))

    # 아래 --- 유연 배열 멤버: 할당 하나
    y1 = 182
    out.append(text(x0, y1 - 14, L["b_head"], size=12, weight="bold", anchor="start"))
    out.append(box(x0, y1, 90, 42))
    out.append(text(x0 + 45, y1 + 26, L["len"], size=11))
    out.append(box(x0 + 90, y1, 420, 42))
    out.append(text(x0 + 300, y1 + 26, L["data"], size=11))
    out.append(f'<line x1="{x0}" y1="{y1 + 54}" x2="{x0 + 510}" y2="{y1 + 54}" '
               f'stroke="#111" stroke-width="1.4"/>')
    out.append(text(x0 + 255, y1 + 70, L["one_alloc"], size=11, weight="bold"))
    out.append(text(x0, y1 + 92, L["b_note"], size=10.5, anchor="start"))

    out.append(text(w / 2, h - 14, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


def fig_flex_size(L):
    """앞 멤버가 같은 두 구조체 --- 마지막 멤버의 타입이 자리를 바꾼다."""
    u = 40                      # 한 바이트의 너비
    x0, w, h = 150, 760, 330
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    def bx(b):
        return x0 + b * u

    ytick = 54
    for b in range(0, 15):
        out.append(f'<line x1="{bx(b)}" y1="{ytick}" x2="{bx(b)}" y2="{ytick + 6}" '
                   f'stroke="#666" stroke-width="1"/>')
        if b % 2 == 0:
            out.append(text(bx(b), ytick - 4, str(b), size=9.5, fill="#666"))

    rows = [
        (78, L["s1"], [(0, 2, "a", "none"), (2, 2, L["pad"], "#eee"),
                       (4, 4, "b", "none"), (8, 2, "c", "none"),
                       (10, 2, L["pad"], "#f6e3e3"), (12, 2, L["data4"], "#dbe7f3")],
         12, 12, L["n1"]),
        (188, L["s2"], [(0, 2, "a", "none"), (2, 2, L["pad"], "#eee"),
                        (4, 4, "b", "none"), (8, 2, "c", "none"),
                        (10, 3, L["data1"], "#dbe7f3")],
         10, 12, L["n2"]),
    ]
    for y, head, cells, off, size, note in rows:
        out.append(text(x0 - 12, y + 24, head, size=12, weight="bold", anchor="end"))
        for s, span, label, fill in cells:
            out.append(box(bx(s), y, span * u, 40, fill=fill))
            out.append(text(bx(s) + span * u / 2, y + 25, label, size=10.5))
        # sizeof 자리와 offsetof 자리를 눈금 위에 표시한다
        out.append(f'<line x1="{bx(off)}" y1="{y - 12}" x2="{bx(off)}" y2="{y + 46}" '
                   f'stroke="#c0392b" stroke-width="1.6" stroke-dasharray="4 3"/>')
        out.append(text(bx(off), y - 16, L["off"] + " " + str(off), size=10,
                        weight="bold", fill="#c0392b"))
        out.append(f'<line x1="{bx(size)}" y1="{y + 46}" x2="{bx(size)}" y2="{y + 62}" '
                   f'stroke="#2c6fbb" stroke-width="1.6"/>')
        out.append(text(bx(size) + 6, y + 60, L["size"] + " " + str(size), size=10,
                        weight="bold", fill="#2c6fbb", anchor="start"))
        out.append(text(x0, y + 80, note, size=10.5, anchor="start"))

    out.append(text(w / 2, h - 12, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


def fig_memory_order(L):
    """깃발과 데이터 --- 순서를 안 묶으면 무엇이 어긋나는가."""
    w, h = 760, 350
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 24, L["title"], size=14, weight="bold"))

    def panel(x0, y0, head, note, ok):
        o = []
        o.append(text(x0 + 170, y0, head, size=12, weight="bold"))
        # 두 줄기: 쓰는 쪽 / 읽는 쪽
        o.append(box(x0, y0 + 12, 160, 96, fill="#f7f7f7"))
        o.append(text(x0 + 80, y0 + 32, L["writer"], size=10.5, weight="bold"))
        o.append(text(x0 + 80, y0 + 54, L["w1"], size=10))
        o.append(text(x0 + 80, y0 + 74, L["w2"], size=10))
        o.append(box(x0 + 180, y0 + 12, 160, 96, fill="#f7f7f7"))
        o.append(text(x0 + 260, y0 + 32, L["reader"], size=10.5, weight="bold"))
        o.append(text(x0 + 260, y0 + 54, L["r1"], size=10))
        o.append(text(x0 + 260, y0 + 74, L["r2"], size=10))
        o.append(arrow(x0 + 160, y0 + 60, x0 + 178, y0 + 60))
        o.append(text(x0 + 170, y0 + 128, note, size=10.5,
                      fill="#2c6fbb" if ok else "#c0392b"))
        return o

    out += panel(30, 60, L["bad_head"], L["bad_note"], False)
    out += panel(400, 60, L["good_head"], L["good_note"], True)

    # 아래: 벽 그림
    out.append(box(400, 210, 340, 74, dash="5 4"))
    out.append(text(570, 232, L["wall1"], size=10.5, weight="bold"))
    out.append(text(570, 252, L["wall2"], size=10))
    out.append(text(570, 270, L["wall3"], size=10))

    out.append(box(30, 210, 340, 74, dash="5 4"))
    out.append(text(200, 232, L["free1"], size=10.5, weight="bold"))
    out.append(text(200, 252, L["free2"], size=10))
    out.append(text(200, 270, L["free3"], size=10))

    out.append(text(w / 2, 320, L["note"], size=11.5, weight="bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 가상 주소가 물리 주소가 되기까지 (12장) ────────────────────
def fig_paging(L):
    """★ 이 그림이 나르는 것은 *하나*다 --- 「이웃한 가상 쪽이 물리에서는
    이웃이 아니다」. 표를 타고 내려가는 단계 수보다 그 사실이 먼저다.
    그래서 화살표를 일부러 엇갈리게 그린다."""
    w, h = 760, 300
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 26, L["title"], 14, "bold"))

    bw, bh = 150, 40
    lx, rx = 40, 570
    ys = [64, 112, 160, 208]
    # 왼쪽 --- 가상 쪽 (이어져 있다)
    out.append(text(lx + bw / 2, ys[0] - 16, L["virtual"], 12.5, "bold"))
    for i, y in enumerate(ys):
        out.append(box(lx, y, bw, bh))
        out.append(text(lx + bw / 2, y + 25, L["vpage"].format(n=i), 12.5))
    # 오른쪽 --- 물리 쪽 (흩어져 있다)
    out.append(text(rx + bw / 2, ys[0] - 16, L["physical"], 12.5, "bold"))
    phys = [L["ppage"].format(n=n) for n in (7, 2, 9, 4)]
    for y, lab in zip(ys, phys):
        out.append(box(rx, y, bw, bh))
        out.append(text(rx + bw / 2, y + 25, lab, 12.5))
    # 가운데 --- 쪽 표. ★ 폭은 *재서* 정한다(손으로 적으면 판마다 넘친다 ---
    #   영어 부제가 상자를 25px 넘겼다).
    tw = fit_width([L["table"], L["table_sub"]], 11, pad=18, minimum=160)
    tx = (lx + bw + rx - tw) / 2
    ty, th = ys[0] - 6, ys[-1] + bh + 6 - (ys[0] - 6)
    out.append(box(tx, ty, tw, th, dash="5 4"))
    out.append(text(tx + tw / 2, ty + th / 2 - 8, L["table"], 13, "bold"))
    out.append(text(tx + tw / 2, ty + th / 2 + 14, L["table_sub"], 11, fill="#444"))
    # 엇갈리는 화살표 --- 순서가 지켜지지 않는다는 것이 요점이다
    order = [1, 3, 0, 2]
    for i, y in enumerate(ys):
        out.append(arrow(lx + bw, y + bh / 2, tx, y + bh / 2))
        out.append(arrow(tx + tw, ys[order[i]] + bh / 2, rx, ys[order[i]] + bh / 2))
    out.append(text(w / 2, h - 34, L["note"], 11.5, fill="#444"))
    out.append(text(w / 2, h - 14, L["note2"], 11.5, fill="#444"))
    out.append(TAIL)
    return "".join(out)


# ── F. 실행 파일 형식의 뼈대 (부록 J) ────────────────────────────
def fig_format_skeleton(L):
    """형식은 셋이지만 로더가 묻는 질문은 하나라는 것을 나란히 세워 보인다."""
    w, h = 760, 400
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    for i, (x, key) in enumerate(((40, "com"), (280, "elf"), (520, "pe"))):
        out.append(text(x + 100, 52, L[key + "_t"], 12, "bold"))
        y = 64
        for r in L[key + "_rows"]:
            if not r:
                continue
            out.append(box(x, y, 200, 44, fill="#f5f5f5" if y == 64 else "none"))
            out.append(text(x + 100, y + 27, r, 10))
            y += 52
        for j, line in enumerate(L.get(key + "_memo", [])):
            out.append(text(x + 100, y + 18 + j * 16, line, 10))
    qy = 300
    out.append(box(40, qy, 680, 76, dash="4 3"))
    out.append(text(w / 2, qy + 24, L["q"], 11, "bold"))
    out.append(text(w / 2, qy + 48, L["q1"], 10))
    out.append(text(w / 2, qy + 66, L["q2"], 10))
    out.append(TAIL)
    return "".join(out)


# ── F. 장치를 어디에 두는가 (부록 K) ─────────────────────────────
def fig_addr_space(L):
    w, h = 760, 330
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(text(180, 52, L["mm"], 12, "bold"))
    out.append(box(60, 64, 100, 210))
    out.append(text(110, 80, "CPU", 11, "bold"))
    out.append(text(110, 100, L["mm1"], 10))
    out.append(text(110, 116, L["mm2"], 10))
    out.append(arrow(160, 130, 208, 130))
    out.append(box(210, 64, 90, 210, dash="4 3"))
    out.append(text(255, 80, L["decoder"], 10, "bold"))
    for label, y in zip(L["devices"], (100, 150, 200, 245)):
        out.append(box(310, y - 18, 90, 34))
        out.append(text(355, y + 4, label, 10))
        out.append(arrow(300, 130, 308, y))
    out.append(text(180, 300, L["mm_note"], 11, "bold"))
    out.append(text(600, 52, L["pm"], 12, "bold"))
    # ★ 상자 폭은 재서 정한다 --- 영어의 "dedicated instructions" 가 100px 을 넘었다.
    cpu_w = fit_width(["CPU", L["pm1"], "in / out"], 10, pad=12, minimum=100)
    out.append(box(570 - cpu_w, 64, cpu_w, 210))
    cx = 570 - cpu_w / 2
    out.append(text(cx, 80, "CPU", 11, "bold"))
    out.append(text(cx, 100, L["pm1"], 10))
    out.append(text(cx, 116, "in / out", 10))
    out.append(arrow(570, 110, 618, 110))
    out.append(box(620, 88, 110, 44))
    out.append(text(675, 106, L["io_space"], 10, "bold"))
    out.append(text(675, 122, L["io_sub"], 9))
    out.append(arrow(cx, 274, cx, 300))
    out.append(box(620, 170, 110, 44, dash="4 3"))
    out.append(text(675, 188, L["mem_space"], 10, "bold"))
    out.append(text(675, 204, L["mm1"], 9))
    out.append(arrow(570, 190, 618, 190))
    out.append(text(600, 300, L["pm_note"], 11, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 0 으로 나누면 무슨 일이 일어나는가 (부록 K) ───────────────
def fig_trap_path(L):
    w, h = 760, 250
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    for (label, sub), x in zip(L["steps"], (40, 220, 400, 580)):
        out.append(box(x, 70, 150, 60))
        out.append(text(x + 75, 92, label, 11, "bold"))
        out.append(text(x + 75, 112, sub, 10))
    for x in (190, 370, 550):
        out.append(arrow(x, 100, x + 28, 100))
    out.append(text(w / 2, 160, L["note1"], 11))
    out.append(text(w / 2, 182, L["note2"], 11))
    out.append(arrow(655, 138, 655, 200))
    out.append(text(655, 218, L["escape"], 10, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 인터럽트에서 하드웨어가 저장해 주는 양 (부록 K) ───────────
def fig_irq_save(L):
    w, h = 760, 300
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    for (name, n, items, note), x in zip(L["cols"], (60, 300, 540)):
        out.append(text(x + 80, 52, name, 12, "bold"))
        out.append(box(x, 64, 160, 150, fill="#f7f7f7" if n else "#fdeaea"))
        if items:
            for i, it in enumerate(items):
                out.append(box(x + 20, 78 + i * 26, 120, 22, fill="#dbe7f3"))
                out.append(text(x + 80, 93 + i * 26, it, 10))
        else:
            out.append(text(x + 80, 140, L["none1"], 12, "bold"))
            out.append(text(x + 80, 160, L["none2"], 12, "bold"))
        out.append(text(x + 80, 232, L["count"].format(n=n), 11, "bold"))
        out.append(text(x + 80, 252, note, 9.5))
    out.append(text(w / 2, 286, L["note"], 12, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 부팅의 사슬 (부록 L) ──────────────────────────────────────
def fig_boot_chain(L):
    w, h = 780, 430
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    xs = (40, 230, 420, 610)
    for (name, steps), y in zip(L["lanes"], (70, 190, 310)):
        out.append(text(40, y - 14, name, 12, "bold", anchor="start"))
        for i, (x, s) in enumerate(zip(xs, steps)):
            out.append(box(x, y, 150, 62, fill="#f5f5f5" if i == 0 else "none"))
            lines = s.split("|")
            for j, line in enumerate(lines):
                out.append(text(x + 75, y + (26 if len(lines) == 1 else 20) + j * 16,
                                line, 10))
            if i < 3:
                out.append(arrow(x + 150, y + 31, x + 188, y + 31))
    out.append(box(40, 380, 700, 40, dash="4 3"))
    out.append(text(w / 2, 396, L["note1"], 11, "bold"))
    out.append(text(w / 2, 412, L["note2"], 10))
    out.append(TAIL)
    return "".join(out)


# ── F. 같은 바이트를 보내는 두 길 (부록 M) ───────────────────────
def fig_serial_parallel(L):
    w, h = 760, 380
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(text(40, 56, L["ser"], 12, "bold", anchor="start"))
    out.append(box(40, 66, 90, 40)); out.append(text(85, 91, L["tx"], 10))
    out.append(box(630, 66, 90, 40)); out.append(text(675, 91, L["rx"], 10))
    out.append('<line x1="130" y1="86" x2="630" y2="86" stroke="#111" stroke-width="1.6"/>')
    for i, b in enumerate("10110010"):
        x = 150 + i * 58
        out.append(box(x, 72, 50, 28, fill="#f5f5f5"))
        out.append(text(x + 25, 91, b, 11))
    out.append(text(w / 2, 122, L["ser_note"], 10))
    out.append(text(40, 166, L["par"], 12, "bold", anchor="start"))
    out.append(box(40, 176, 90, 130)); out.append(text(85, 245, L["tx"], 10))
    out.append(box(630, 176, 90, 130)); out.append(text(675, 245, L["rx"], 10))
    for i, b in enumerate("10110010"):
        y = 186 + i * 15
        out.append(f'<line x1="130" y1="{y}" x2="630" y2="{y}" stroke="#111" stroke-width="1.2"/>')
        out.append(text(380, y - 3, b, 9))
    out.append(text(w / 2, 322, L["par_note"], 10))
    out.append(box(40, 336, 680, 36, dash="4 3"))
    out.append(text(w / 2, 352, L["end1"], 10, "bold"))
    out.append(text(w / 2, 366, L["end2"], 10, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 같은 자료를 옮기는 두 길 (부록 M) ─────────────────────────
def fig_dma_path(L):
    w, h = 760, 340
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(text(40, 54, L["by_cpu"], 12, "bold", anchor="start"))
    out.append(box(60, 66, 120, 54)); out.append(text(120, 98, L["dev"], 11))
    out.append(box(320, 66, 120, 54, fill="#f5f5f5"))
    out.append(text(380, 92, "CPU", 11, "bold")); out.append(text(380, 110, L["via_reg"], 9))
    out.append(box(580, 66, 120, 54)); out.append(text(640, 98, L["mem"], 11))
    out.append(arrow(180, 93, 318, 93)); out.append(arrow(440, 93, 578, 93))
    out.append(text(w / 2, 136, L["cpu_note"], 10))
    out.append(text(40, 176, L["by_dma"], 12, "bold", anchor="start"))
    out.append(box(60, 188, 120, 54)); out.append(text(120, 220, L["dev"], 11))
    out.append(box(580, 188, 120, 54)); out.append(text(640, 220, L["mem"], 11))
    out.append(box(320, 188, 120, 54, dash="4 3"))
    out.append(text(380, 214, L["dma_c"], 10, "bold")); out.append(text(380, 232, L["moves"], 9))
    out.append(arrow(180, 215, 318, 215)); out.append(arrow(440, 215, 578, 215))
    out.append(box(320, 262, 120, 40, fill="#f5f5f5"))
    out.append(text(380, 287, "CPU", 11, "bold"))
    out.append('<line x1="380" y1="262" x2="380" y2="244" stroke="#111" '
               'stroke-width="1.2" stroke-dasharray="3 3"/>')
    out.append(text(210, 287, L["starts"], 10, anchor="end"))
    out.append(text(550, 287, L["one_irq"], 10, anchor="start"))
    out.append(box(40, 312, 680, 24, dash="4 3"))
    out.append(text(w / 2, 328, L["end"], 11, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. 디스크 한 장의 지도 (부록 N) ──────────────────────────────
def fig_disk_layers(L):
    w, h = 780, 420
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(text(40, 58, L["l1"], 11, "bold", anchor="start"))
    x = 40
    for i, lab in enumerate(L["sectors"]):
        wid = 150 if i in (2, 3, 5) else 80
        out.append(box(x, 68, wid, 34, fill="#f5f5f5" if i in (0, 1, 6) else "none"))
        out.append(text(x + wid / 2, 89, lab, 10))
        x += wid
    out.append(text(40, 140, L["l2"], 11, "bold", anchor="start"))
    heads = ((40, 80, L["mbr"], L["mbr_s"]), (120, 80, L["gpth"], "LBA 1"),
             (200, 90, L["entries"], "128×128B"))
    for xx, wid, lab, sub in heads:
        out.append(box(xx, 150, wid, 40, fill="#e8e8e8"))
        out.append(text(xx + wid / 2, 168, lab, 9, "bold"))
        out.append(text(xx + wid / 2, 182, sub, 8))
    for (xx, wid), lab in zip(((290, 130), (420, 180), (600, 90)), L["parts"]):
        out.append(box(xx, 150, wid, 40))
        out.append(text(xx + wid / 2, 174, lab, 10))
    out.append(box(690, 90, 40, 40, fill="#e8e8e8", dash="4 3") if False else "")
    out.append(box(690, 150, 90, 40, fill="#e8e8e8", dash="4 3"))
    out.append(text(735, 168, L["alt"], 9, "bold"))
    out.append(text(735, 182, L["alt_s"], 8))
    out.append(text(40, 232, L["l3"], 11, "bold", anchor="start"))
    out.append(arrow(480, 195, 300, 240))
    for (xx, wid), (lab, sub) in zip(((40, 110), (150, 130), (280, 130), (410, 370)),
                                     L["inner"]):
        out.append(box(xx, 250, wid, 46, fill="#f5f5f5" if xx == 40 else "none"))
        out.append(text(xx + wid / 2, 270, lab, 10, "bold"))
        out.append(text(xx + wid / 2, 286, sub, 9))
    out.append(box(40, 320, 740, 78, dash="4 3"))
    out.append(text(w / 2, 344, L["end1"], 12, "bold"))
    out.append(text(w / 2, 364, L["end2"], 10))
    out.append(text(w / 2, 384, L["end3"], 10))
    out.append(TAIL)
    return "".join(out)


# ── F. 확장 파티션의 EBR 사슬 (부록 N) ───────────────────────────
def fig_ebr_chain(L):
    w, h = 780, 330
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(box(30, 60, 110, 50, fill="#e8e8e8"))
    out.append(text(85, 80, "MBR", 10, "bold")); out.append(text(85, 96, "LBA 0", 9))
    out.append(box(150, 60, 600, 50, dash="4 3"))
    out.append(text(450, 78, L["ext"], 10, "bold"))
    out.append(text(450, 96, L["ext_s"], 9))
    for x, (lab, sub) in zip((170, 400), L["ebrs"]):
        out.append(box(x, 140, 90, 44, fill="#f5f5f5"))
        out.append(text(x + 45, 160, lab, 10, "bold"))
        out.append(text(x + 45, 176, sub, 9))
    for x, lab in zip((270, 500), L["logicals"]):
        out.append(box(x, 140, 120, 44))
        out.append(text(x + 60, 166, lab, 10))
    out.append(arrow(260, 162, 268, 162))
    out.append(arrow(490, 162, 498, 162))
    out.append(arrow(215, 184, 445, 184))
    for y, s, bold in ((210, L["n1"], "bold"), (226, L["n1s"], "normal"),
                       (250, L["n2"], "bold"), (266, L["n2s"], "normal")):
        out.append(text(215, y, s, 10, bold, anchor="start"))
    out.append(box(30, 286, 720, 34, dash="4 3"))
    out.append(text(w / 2, 308, L["end"], 11, "bold"))
    out.append(TAIL)
    return "".join(out)


# ── F. SSD 는 쓰는 단위와 지우는 단위가 다르다 (부록 N) ──────────
def fig_ssd_erase(L):
    w, h = 780, 400
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    out.append(text(40, 56, L["s1"], 12, "bold", anchor="start"))
    out.append(box(60, 68, 660, 60, dash="4 3"))
    out.append(text(390, 62, L["erase_unit"], 9, "bold"))
    for i in range(8):
        x = 75 + i * 81
        out.append(box(x, 78, 70, 40, fill="#ececec" if i < 5 else "none"))
        out.append(text(x + 35, 96, L["page"].format(n=i + 1), 9))
        out.append(text(x + 35, 110, L["used"] if i < 5 else L["free"], 8))
    out.append(text(390, 142, L["write_unit"], 10))
    out.append(text(40, 176, L["s2"], 12, "bold", anchor="start"))
    for lab, x in zip(L["steps"], (60, 240, 420, 600)):
        out.append(box(x, 194, 150, 46))
        out.append(text(x + 75, 220, lab, 10))
        if x < 600:
            out.append(arrow(x + 150, 217, x + 178, 217))
    out.append(text(40, 274, L["s3"], 12, "bold", anchor="start"))
    out.append(box(60, 286, 660, 46, fill="#f5f5f5"))
    out.append(text(390, 306, L["amp1"], 11, "bold"))
    out.append(text(390, 324, L["amp2"], 10))
    out.append(box(60, 344, 660, 44, dash="4 3"))
    out.append(text(390, 364, L["trim1"], 11, "bold"))
    out.append(text(390, 380, L["trim2"], 10))
    out.append(TAIL)
    return "".join(out)


# ── 부록 O 의 도해는 *잰 값*을 그린다 ────────────────────────────
# ★ 측정 예제가 `#DATA ...` 줄로 값을 함께 내고, verify-examples.sh 가 그것을
#   `<이름>.c.data` 로 갈라 둔다. 여기서 그 파일을 읽는다 --- 그림이 지난번
#   숫자를 기억하지 않게 하려는 것이다. 값이 없으면 *없다고 그린다*(빈 그림을
#   그럴듯하게 채우지 않는다).
def _measured(name):
    f = ROOT / "build" / "examples-out" / "apx-measured" / name / "run.sh.data"
    rows = []
    if f.exists():
        for line in f.read_text(encoding="utf-8").splitlines():
            if line.startswith("#DATA "):
                rows.append(line.split()[1:])
    return rows


def fig_measure_traps(L):
    w, h = 780, 400
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    for x, panel in zip((30, 275, 520), L["panels"]):
        out.append(box(x, 44, 230, 330))
        out.append(text(x + 115, 68, panel["t"], 13, "bold"))
        y = 86
        for r in panel["rows"]:
            out.append(box(x + 25, y, 180, 34, fill="#f5f5f5"))
            out.append(text(x + 115, y + 22, r, 10))
            y += 46
        if panel.get("gone"):
            out.append(text(x + 115, 232, panel["gone"], 11, "bold"))
        for i, line in enumerate(panel["why"]):
            out.append(text(x + 115, 258 + i * 16, line, 10))
        for i, line in enumerate(panel["fix"]):
            out.append(text(x + 115, 306 + i * 16, line, 10, "bold"))
    out.append(TAIL)
    return "".join(out)


def fig_cache_ladder(L):
    import math
    rows = [(int(r[0]), float(r[1]), float(r[2]), r[3]) for r in _measured("cache_ladder")]
    w, h = 780, 470
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    if not rows:
        out.append(text(w / 2, 200, L["nodata"], 12)); out.append(TAIL)
        return "".join(out)
    x0, y0, pw, ph = 90, 60, 620, 280
    xs = [math.log2(r[0]) for r in rows]
    x_min, x_max = min(xs), max(xs)
    y_max = max(max(r[1] for r in rows), 1.0) * 1.1
    px = lambda k: x0 + (math.log2(k) - x_min) / (x_max - x_min) * pw
    py = lambda v: y0 + ph - v / y_max * ph
    out.append(f'<line x1="{x0}" y1="{y0 + ph}" x2="{x0 + pw}" y2="{y0 + ph}" stroke="#111" stroke-width="1.4"/>')
    out.append(f'<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{y0 + ph}" stroke="#111" stroke-width="1.4"/>')
    for v in range(0, int(y_max) + 1, 20):
        out.append(f'<line x1="{x0 - 4}" y1="{py(v)}" x2="{x0}" y2="{py(v)}" stroke="#111" stroke-width="1"/>')
        out.append(text(x0 - 8, py(v) + 4, str(v), 9, anchor="end"))
    out.append(text(30, y0 + ph / 2, L["ns"], 10, "bold"))
    out.append(text(x0 + pw / 2, y0 + ph + 38, L["xaxis"], 10, "bold"))
    for kib, lab in zip((32, 256, 16384), L["caches"]):
        if x_min <= math.log2(kib) <= x_max:
            X = px(kib)
            out.append(f'<line x1="{X}" y1="{y0}" x2="{X}" y2="{y0 + ph}" stroke="#888" stroke-width="1" stroke-dasharray="4 3"/>')
            out.append(text(X, y0 - 6, lab, 9, "bold"))
    out.append('<polyline points="' + " ".join(f"{px(r[0])},{py(r[1])}" for r in rows)
               + '" fill="none" stroke="#111" stroke-width="2.2"/>')
    for r in rows:
        out.append(f'<circle cx="{px(r[0])}" cy="{py(r[1])}" r="3" fill="#111"/>')
    out.append('<polyline points="' + " ".join(f"{px(r[0])},{py(r[2])}" for r in rows)
               + '" fill="none" stroke="#111" stroke-width="1.4" stroke-dasharray="5 3"/>')
    mid = rows[len(rows) // 2]
    lab_y = py(y_max * 0.30)
    out.append(text(x0 + 24, lab_y - 6, L["dashed"].format(v=f"{mid[2]:.2f}"), 9, anchor="start"))
    out.append(arrow(x0 + 150, lab_y, px(mid[0]), py(mid[2]) - 5))
    for r in rows:
        if r[0] in (4, 32, 256, 1024, 16384, 131072):
            lab = f"{r[0]} KiB" if r[0] < 1024 else f"{r[0] // 1024} MiB"
            out.append(text(px(r[0]), y0 + ph + 18, lab, 9))
    out.append(text(x0 + 20, y0 + 20, L["solid_leg"], 10, anchor="start"))
    out.append(text(x0 + 20, y0 + 38, L["dashed_leg"], 10, anchor="start"))
    first, last = rows[0][1], rows[-1][1]
    out.append(box(90, 402, 620, 48, dash="4 3"))
    out.append(text(w / 2, 422, L["end1"].format(a=f"{first:.2f}", b=f"{last:.1f}",
                                                 n=f"{last / first:.0f}"), 11, "bold"))
    out.append(text(w / 2, 440, L["end2"], 10))
    out.append(TAIL)
    return "".join(out)


def fig_cache_line(L):
    import math
    rows = [(float(r[0]), float(r[1])) for r in _measured("stride")]
    w, h = 780, 400
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    if not rows:
        out.append(text(w / 2, 200, L["nodata"], 12)); out.append(TAIL)
        return "".join(out)
    x0, y0, pw, ph = 90, 70, 620, 230
    xs = [math.log2(r[0]) for r in rows]
    x_min, x_max = min(xs), max(xs)
    y_max = max(r[1] for r in rows) * 1.15
    px = lambda v: x0 + (math.log2(v) - x_min) / (x_max - x_min) * pw
    py = lambda v: y0 + ph - v / y_max * ph
    out.append(f'<line x1="{x0}" y1="{y0 + ph}" x2="{x0 + pw}" y2="{y0 + ph}" stroke="#111" stroke-width="1.4"/>')
    out.append(f'<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{y0 + ph}" stroke="#111" stroke-width="1.4"/>')
    for v in range(0, int(y_max) + 1, 2):
        out.append(f'<line x1="{x0 - 4}" y1="{py(v)}" x2="{x0}" y2="{py(v)}" stroke="#111" stroke-width="1"/>')
        out.append(text(x0 - 8, py(v) + 4, str(v), 9, anchor="end"))
    out.append(text(34, y0 + ph / 2, L["ns"], 10, "bold"))
    out.append(text(x0 + pw / 2, y0 + ph + 36, L["xaxis"], 10, "bold"))
    X = px(64)
    out.append(f'<line x1="{X}" y1="{y0 - 4}" x2="{X}" y2="{y0 + ph}" stroke="#888" stroke-width="1" stroke-dasharray="4 3"/>')
    out.append(text(X, y0 - 10, L["line64"], 9, "bold"))
    out.append('<polyline points="' + " ".join(f"{px(r[0])},{py(r[1])}" for r in rows)
               + '" fill="none" stroke="#111" stroke-width="2.2"/>')
    for r in rows:
        out.append(f'<circle cx="{px(r[0])}" cy="{py(r[1])}" r="3" fill="#111"/>')
        if int(r[0]) in (1, 8, 64, 512, 4096):
            out.append(text(px(r[0]), y0 + ph + 18, str(int(r[0])), 9))
    out.append(text(x0 + 20, y0 + 20, L["left"], 10, anchor="start"))
    out.append(text(X + 14, y0 + 42, L["right"], 10, anchor="start"))
    out.append(box(90, 322, 620, 62, dash="4 3"))
    out.append(text(w / 2, 344, L["end1"], 11, "bold"))
    out.append(text(w / 2, 362, L["end2"], 10))
    out.append(text(w / 2, 378, L["end3"], 10))
    out.append(TAIL)
    return "".join(out)


def fig_branch_pipeline(L):
    w, h = 780, 380
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))
    stages = L["stages"]

    def lane(y, title, n_ok, flush_at, note):
        out.append(text(40, y - 10, title, 12, "bold", anchor="start"))
        for c in range(6):
            for st in range(4):
                x = 150 + c * 100
                yy = y + st * 22
                filled = c < n_ok or (flush_at is None)
                if flush_at is not None and c >= flush_at:
                    filled = False
                out.append(box(x, yy, 92, 18, fill="#ececec" if filled else "none",
                               dash=None if filled else "3 2", sw=1.0))
                out.append(text(x + 46, yy + 13,
                                f"{stages[st]} {c + 1}" if filled else L["dropped"], 8))
        out.append(text(w / 2, y + 104, note, 10))

    lane(60, L["lane1"], 6, None, L["note1"])
    lane(210, L["lane2"], 2, 2, L["note2"])
    out.append(box(40, 330, 700, 40, dash="4 3"))
    out.append(text(w / 2, 348, L["end1"], 11, "bold"))
    out.append(text(w / 2, 364, L["end2"], 10))
    out.append(TAIL)
    return "".join(out)


def fig_false_sharing(L):
    w, h = 780, 380
    out = [HEAD.format(w=w, h=h, f=FONT), DEFS]
    out.append(text(w / 2, 22, L["title"], 14, "bold"))

    def scene(y, title, same_line, note1, note2):
        out.append(text(40, y - 8, title, 12, "bold", anchor="start"))
        out.append(box(60, y + 4, 120, 52))
        out.append(text(120, y + 26, L["coreA"], 11, "bold"))
        out.append(text(120, y + 44, L["incx"], 9))
        out.append(box(600, y + 4, 120, 52))
        out.append(text(660, y + 26, L["coreB"], 11, "bold"))
        out.append(text(660, y + 44, L["incy"], 9))
        if same_line:
            out.append(box(300, y + 4, 180, 52, fill="#ececec"))
            out.append(text(390, y + 24, L["one_line"], 9, "bold"))
            out.append(text(390, y + 42, L["side_by_side"], 9))
            out.append(arrow(180, y + 22, 298, y + 22))
            out.append(arrow(600, y + 40, 482, y + 40))
            out.append(text(390, y + 74, L["fight"], 10, "bold"))
        else:
            out.append(box(250, y + 4, 130, 52, fill="#ececec"))
            out.append(text(315, y + 24, L["line1"], 9, "bold")); out.append(text(315, y + 42, "x", 9))
            out.append(box(410, y + 4, 130, 52, fill="#ececec"))
            out.append(text(475, y + 24, L["line2"], 9, "bold")); out.append(text(475, y + 42, "y", 9))
            out.append(arrow(180, y + 30, 248, y + 30))
            out.append(arrow(600, y + 30, 542, y + 30))
            out.append(text(390, y + 74, L["apart"], 10, "bold"))
        out.append(text(390, y + 92, note1, 10))
        out.append(text(390, y + 108, note2, 10, "bold"))

    scene(60, L["s1"], True, L["s1n1"], L["s1n2"])
    scene(220, L["s2"], False, L["s2n1"], L["s2n2"])
    out.append(TAIL)
    return "".join(out)


FIGS = {
    "memory-order": (fig_memory_order, {
        "ko": dict(title="깃발이 보이면 데이터도 보이는가",
                   writer="쓰는 쪽", reader="읽는 쪽",
                   w1="data = 42", w2="flag = 1",
                   r1="flag 를 본다", r2="data 를 읽는다",
                   bad_head="순서를 안 묶으면", good_head="release / acquire 로 묶으면",
                   bad_note="깃발이 먼저 보일 수 있다 → 옛 data 를 읽는다",
                   good_note="깃발이 보이면 그 앞의 쓰기도 보인다",
                   free1="누가 순서를 바꾸나", free2="컴파일러가 바꾼다 (한 갈래만 보므로 문제없다고 판단)",
                   free3="CPU 도 바꾼다 (쓰기 버퍼·비순차 실행)",
                   wall1="짝이 세우는 벽", wall2="release: 앞의 쓰기가 아래로 못 내려간다",
                   wall3="acquire: 뒤의 읽기가 위로 못 올라간다",
                   note="벽은 한쪽만 세워도 소용없다 --- 쓰는 쪽과 읽는 쪽이 짝을 이뤄야 한다"),
        "en": dict(title="If the flag is visible, is the data visible too?",
                   writer="writer", reader="reader",
                   w1="data = 42", w2="flag = 1",
                   r1="sees flag", r2="reads data",
                   bad_head="with no ordering", good_head="paired release / acquire",
                   bad_note="the flag can arrive first -> stale data is read",
                   good_note="seeing the flag means seeing the writes before it",
                   free1="who reorders?", free2="the compiler does (one thread looks unaffected)",
                   free3="so does the CPU (store buffers, out-of-order execution)",
                   wall1="the wall the pair builds", wall2="release: earlier writes cannot sink below",
                   wall3="acquire: later reads cannot rise above",
                   note="one side alone is useless --- writer and reader must pair up"),
    }),
    "flex-size": (fig_flex_size, {
        "ko": dict(title="앞 멤버가 같아도 마지막 멤버의 타입이 자리를 바꾼다",
                   s1="struct s1", s2="struct s2", pad="채움",
                   data4="data[] (uint32_t)", data1="data[] (char)",
                   off="offsetof(data)", size="sizeof",
                   n1="data 가 uint32_t --- 정렬 4 에 맞추느라 c 뒤에 2바이트가 빈다",
                   n2="data 가 char --- 정렬 1 이라 c 바로 뒤에 붙는다. 앞에 빈자리가 없다",
                   note="두 구조체의 sizeof 는 똑같이 12 다 --- 그런데 data 가 앉는 자리는 다르다"),
        "en": dict(title="Same leading members, and the last member's type moves the boundary",
                   s1="struct s1", s2="struct s2", pad="padding",
                   data4="data[] (uint32_t)", data1="data[] (char)",
                   off="offsetof(data)", size="sizeof",
                   n1="data is uint32_t --- aligning it to 4 leaves two bytes empty after c",
                   n2="data is char --- alignment 1, so it follows c directly, with no gap",
                   note="both structs have sizeof 12 --- yet data begins in a different place"),
    }),
    "ptr-array": (fig_ptr_array, {
        "ko": dict(title="포인터의 배열 --- 길이가 제각각인 것들을 하나로 묶는다",
                   arr="char *names[3]", addr=["0x5f10", "0x5f30", "0x5f48"],
                   arr_note="배열 자체는 연속이다 --- 포인터 셋이 8바이트씩 나란히",
                   str=["ada", "grace", "linus"],
                   str_note="가리키는 곳은 흩어져 있고 길이도 제각각이다",
                   note="배열은 붙어 있고, 내용은 흩어져 있다"),
        "en": dict(title="an array of pointers --- tying together things of different lengths",
                   arr="char *names[3]", addr=["0x5f10", "0x5f30", "0x5f48"],
                   arr_note="the array itself is contiguous --- three pointers, 8 bytes apart",
                   str=["ada", "grace", "linus"],
                   str_note="what they point at is scattered, and each has its own length",
                   note="the array is contiguous; its contents are not"),
    }),
    "ptr-ptr": (fig_ptr_ptr, {
        "ko": dict(title="포인터의 포인터 --- 남의 포인터를 고치는 법",
                   caller="부르는 쪽", caller_var="char *p", before="처음엔 널",
                   caller_addr="p 가 사는 자리: 0x7ffd10",
                   handoff="&p 를 넘긴다 (0x7ffd10)",
                   callee="부름받는 쪽 (char **out)", param="out",
                   param_val="0x7ffd10 --- p 가 사는 자리",
                   write="*out = buf;  ← 남의 변수에 쓴다",
                   heap="새로 잡은 자리 0x5f10",
                   after_line="돌아오면 p 는 0x5f10 --- 부르는 쪽의 변수가 바뀌었다",
                   note="한 겹을 더 두는 이유: 인자는 값 복사라서, 주소를 넘겨야 원본이 바뀐다"),
        "en": dict(title="a pointer to a pointer --- how to change someone else's pointer",
                   caller="caller", caller_var="char *p", before="null at first",
                   caller_addr="where p lives: 0x7ffd10",
                   handoff="pass &p (0x7ffd10)",
                   callee="callee (char **out)", param="out",
                   param_val="0x7ffd10 --- where p lives",
                   write="*out = buf;  <- writes into the caller's variable",
                   heap="the newly allocated block 0x5f10",
                   after_line="on return p is 0x5f10 --- the caller's variable changed",
                   note="the extra level exists because arguments are copies: pass the address to change the original"),
    }),
    "struct-array": (fig_struct_array, {
        "ko": dict(title="구조체의 배열 --- 원소 사이에는 빈자리가 없다",
                   decl="struct rec { int id; char code; };   sizeof = 8, _Alignof = 4",
                   one="원소 하나의 속", m1="id (4)", m2="code", pad="꼬리 패딩 (3)",
                   size="sizeof = 8", arr="struct rec v[3]",
                   off=["+0", "+8", "+16"], stride="간격 = sizeof",
                   why="원소가 딱 붙어 놓이므로, 크기가 정렬의 배수여야 둘째 원소부터도 정렬이 맞는다",
                   note="그래서 꼬리 패딩이 생긴다 --- 마지막 멤버 뒤의 빈자리는 낭비가 아니라 계약이다"),
        "en": dict(title="an array of structs --- no gap between elements",
                   decl="struct rec { int id; char code; };   sizeof = 8, _Alignof = 4",
                   one="inside one element", m1="id (4)", m2="code", pad="tail padding (3)",
                   size="sizeof = 8", arr="struct rec v[3]",
                   off=["+0", "+8", "+16"], stride="stride = sizeof",
                   why="elements sit flush against each other, so the size must be a multiple of the alignment for every element to stay aligned",
                   note="that is where tail padding comes from --- the space after the last member is a contract, not waste"),
    }),
    "flex-array": (fig_flex_array, {
        "ko": dict(title="유연 배열 멤버 --- 머리와 데이터를 한 덩어리로",
                   a_head="① 포인터 멤버 (할당 둘)", len="len", ptr="char *data",
                   buf="따로 잡은 버퍼",
                   a_note="malloc 두 번, free 두 번. 머리와 데이터가 멀리 떨어진다",
                   b_head="② 유연 배열 멤버 char data[] (할당 하나)",
                   data="data[] --- 잡을 때 길이를 정한다",
                   one_alloc="malloc(offsetof(struct msg, data) + len) 한 번",
                   b_note="sizeof 에 data 는 들어가지 않는다. 마지막 멤버여야 하고, 앞에 멤버가 하나 이상 있어야 한다",
                   note="한 덩어리라 캐시에 함께 들어오고, free 도 한 번이다"),
        "en": dict(title="a flexible array member --- header and data in one block",
                   a_head="1. a pointer member (two allocations)", len="len", ptr="char *data",
                   buf="a separately allocated buffer",
                   a_note="two mallocs, two frees, and the header sits far from the data",
                   b_head="2. a flexible array member char data[] (one allocation)",
                   data="data[] --- its length is fixed when it is allocated",
                   one_alloc="one malloc(offsetof(struct msg, data) + len)",
                   b_note="data is not counted in sizeof; it must be the last member, with at least one member before it",
                   note="one block: it arrives in the cache together, and there is one free"),
    }),
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
    "measure-traps": (fig_measure_traps, {
        "ko": dict(title="측정의 세 함정 --- 잰 수가 이상하면 먼저 여기를 본다",
                   panels=[dict(t="① 지워짐", rows=["내가 쓴 코드", "컴파일러", "실행된 코드"],
                                gone="✗ 지워짐",
                                why=["결과를 아무도 안 쓰면", "계산이 통째로 사라진다"],
                                fix=["→ 결과를 volatile 에 넣거나", "   반환값을 쓴다"]),
                           dict(t="② 데우기", rows=["첫 회", "둘째 회", "셋째 회…"],
                                why=["첫 회에는 쪽 부재·캐시 채우기", "값이 섞인다 (이 기계에서 9배)"],
                                fix=["→ 데우기 회차를 돌리고", "   그 회차는 버린다"]),
                           dict(t="③ 잡음", rows=["회차들", "평균", "중앙값"],
                                why=["남의 일이 한 번 끼어들면", "평균이 그쪽으로 끌려간다"],
                                fix=["→ 중앙값을 쓰고", "   최솟값을 함께 적는다"])]),
        "en": dict(title="three traps in measuring --- look here first when a number looks wrong",
                   panels=[dict(t="1. eliminated", rows=["the code I wrote", "the compiler", "the code that ran"],
                                gone="x eliminated",
                                why=["if nobody uses the result,", "the computation disappears entirely"],
                                fix=["-> put the result in a volatile,", "   or use the return value"]),
                           dict(t="2. warming up", rows=["first round", "second round", "third round…"],
                                why=["the first round mixes in page faults", "and cache filling (9x on this machine)"],
                                fix=["-> run warm-up rounds", "   and discard them"]),
                           dict(t="3. noise", rows=["the rounds", "mean", "median"],
                                why=["one interruption from other work", "drags the mean towards it"],
                                fix=["-> use the median, and", "   state the minimum alongside"])]),
    }),
    "cache-ladder": (fig_cache_ladder, {
        "ko": dict(title="기억의 사다리 --- 실제로 잰 계단", nodata="(측정 결과가 아직 없다)",
                   ns="나노초", xaxis="작업 집합 크기 (두 배씩)",
                   caches=["L1 32 KiB", "L2 256 KiB", "L3 16 MiB"],
                   dashed="점선(차례로 훑기)은 바닥에 붙어 있다 --- {v} 나노초",
                   solid_leg="실선 = 무작위 접근(포인터 추적) --- 지연이 보인다",
                   dashed_leg="점선 = 차례로 훑기 --- 미리 가져오기가 지연을 가린다",
                   end1="가장 안쪽 {a} 나노초 → 가장 바깥 {b} 나노초 --- {n}배",
                   end2="같은 코드, 같은 명령 수 --- 달라진 것은 자료가 어디 있느냐뿐이다"),
        "en": dict(title="the memory ladder --- the steps, actually measured", nodata="(no measurement yet)",
                   ns="ns", xaxis="working set size (doubling)",
                   caches=["L1 32 KiB", "L2 256 KiB", "L3 16 MiB"],
                   dashed="the dashed line (sequential) hugs the floor --- {v} ns",
                   solid_leg="solid = random access (pointer chasing) --- the latency shows",
                   dashed_leg="dashed = sequential --- prefetching hides the latency",
                   end1="innermost {a} ns -> outermost {b} ns --- {n} times",
                   end2="same code, same instruction count --- only where the data sits has changed"),
    }),
    "cache-line": (fig_cache_line, {
        "ko": dict(title="걸음 폭이 캐시 줄을 넘어서는 자리", nodata="(측정 결과가 아직 없다)",
                   ns="나노초", xaxis="걸음 폭 (바이트, 두 배씩)", line64="캐시 줄 64바이트",
                   left="왼쪽: 한 줄을 여러 번 나눠 쓴다 --- 값이 낮다",
                   right="오른쪽: 걸음마다 새 줄 --- 값이 올라 평평해진다",
                   end1="줄이 64바이트라는 사실이 시간에 그대로 나타난다",
                   end2="8바이트만 쓰려 해도 기계는 64바이트를 실어 온다",
                   end3="→ 그래서 「무엇을 읽는가」만큼 「어떻게 늘어놓는가」가 값을 정한다"),
        "en": dict(title="where the stride crosses the cache line", nodata="(no measurement yet)",
                   ns="ns", xaxis="stride (bytes, doubling)", line64="cache line, 64 bytes",
                   left="left: one line used several times over --- the cost is low",
                   right="right: a new line every step --- the cost rises, then flattens",
                   end1="that a line is 64 bytes shows up directly in the time",
                   end2="ask for 8 bytes and the machine still fetches 64",
                   end3="-> so how things are laid out settles the cost as much as what is read"),
    }),
    "branch-pipeline": (fig_branch_pipeline, {
        "ko": dict(title="맞혔을 때와 틀렸을 때 --- 파이프라인에서 벌어지는 일",
                   stages=["가져오기", "해독", "실행", "쓰기"], dropped="버려짐",
                   lane1="① 예측이 맞았을 때 --- 빈칸 없이 이어진다",
                   note1="분기 명령이 끝나기 전에 다음 명령들이 이미 들어와 있다 --- 분기가 공짜처럼 보인다",
                   lane2="② 예측이 틀렸을 때 --- 채워 둔 것을 버리고 다시 채운다",
                   note2="잘못된 길로 들어온 명령을 전부 버리고, 옳은 자리에서 다시 채운다",
                   end1="그 「다시 채우는 시간」이 틀린 예측의 값이다",
                   end2="이 기계에서 잰 값: 한 번 틀릴 때 약 6 나노초 --- 덧셈 수십 번에 맞먹는다"),
        "en": dict(title="right and wrong --- what happens in the pipeline",
                   stages=["fetch", "decode", "execute", "write"], dropped="discarded",
                   lane1="1. the prediction was right --- it continues without a gap",
                   note1="the following instructions are already in before the branch finishes --- the branch looks free",
                   lane2="2. the prediction was wrong --- what was filled in is thrown away and refilled",
                   note2="every instruction from the wrong path is discarded and the pipeline refills from the right place",
                   end1="that refilling time is the cost of a wrong prediction",
                   end2="measured on this machine: about 6 ns per miss --- as much as dozens of additions"),
    }),
    "false-sharing": (fig_false_sharing, {
        "ko": dict(title="거짓 공유 --- 남의 변수 때문에 내 코어가 기다린다",
                   coreA="코어 A", coreB="코어 B", incx="x 를 올린다", incy="y 를 올린다",
                   one_line="캐시 줄 하나 (64바이트)", side_by_side="x  y  (같은 줄에 나란히)",
                   fight="줄을 통째로 뺏고 뺏긴다", line1="줄 1", line2="줄 2",
                   apart="서로 건드리지 않는다",
                   s1="① 두 변수가 같은 줄에 있을 때",
                   s1n1="코드는 서로 남의 변수를 만지지 않는다 --- 그런데도",
                   s1n2="이 기계에서 잰 값: 완벽한 나눔보다 1.39배 느리다",
                   s2="② 64바이트 떨어뜨렸을 때",
                   s2n1="같은 코드, 같은 계산, 자리만 옮겼다",
                   s2n2="이 기계에서 잰 값: 1.00배 --- 손해가 사라졌다"),
        "en": dict(title="false sharing --- my core waits because of somebody else's variable",
                   coreA="core A", coreB="core B", incx="increments x", incy="increments y",
                   one_line="one cache line (64 bytes)", side_by_side="x  y  (side by side in one line)",
                   fight="the line is taken back and forth", line1="line 1", line2="line 2",
                   apart="they do not touch each other",
                   s1="1. when the two variables share a line",
                   s1n1="the code never touches the other's variable --- and yet",
                   s1n2="measured here: 1.39 times slower than a perfect split",
                   s2="2. when they are 64 bytes apart",
                   s2n1="same code, same computation, only the place moved",
                   s2n2="measured here: 1.00 times --- the loss is gone"),
    }),
    "disk-layers": (fig_disk_layers, {
        "ko": dict(title="디스크 한 장의 배치 --- 세 층이 겹쳐 있다",
                   l1="① 기계가 보는 것: 번호 붙은 칸(섹터)의 줄",
                   sectors=["LBA 0", "1", "2 ~ 33", "34 …", "…", "끝-33 …", "끝"],
                   l2="② 파티션 표가 말하는 것: 어디부터 어디까지가 한 덩어리인가",
                   mbr="MBR", mbr_s="또는 보호 MBR", gpth="GPT 헤더", entries="항목 배열",
                   parts=["파티션 1 (ESP)", "파티션 2 (root)", "파티션 3"],
                   alt="짝 GPT", alt_s="디스크 끝",
                   l3="③ 파티션 *안*: 파일 시스템이 다시 제 방식으로 나눈다",
                   inner=[("부트 섹터", "BPB·서명"), ("FAT 1벌", "다음 클러스터 표"),
                          ("FAT 2벌", "사본"), ("자료 구역", "클러스터 2, 3, 4 …")],
                   end1="세 층은 서로를 모른다",
                   end2="기계는 섹터만 알고 · 파티션 표는 범위만 알고 · 파일 시스템은 제 파티션 안만 안다",
                   end3="그래서 파티션 표가 날아가도 자료는 그 자리에 있고, 포맷을 해도 다른 파티션은 멀쩡하다"),
        "en": dict(title="one disk, laid out --- three layers on top of each other",
                   l1="1. what the machine sees: a row of numbered cells (sectors)",
                   sectors=["LBA 0", "1", "2 - 33", "34 …", "…", "end-33 …", "end"],
                   l2="2. what the partition table says: from where to where is one piece",
                   mbr="MBR", mbr_s="or a protective MBR", gpth="GPT header", entries="entry array",
                   parts=["partition 1 (ESP)", "partition 2 (root)", "partition 3"],
                   alt="alternate GPT", alt_s="end of disk",
                   l3="3. *inside* a partition: the filesystem divides it again its own way",
                   inner=[("boot sector", "BPB and signature"), ("FAT copy 1", "the next-cluster table"),
                          ("FAT copy 2", "a duplicate"), ("data area", "clusters 2, 3, 4 …")],
                   end1="the three layers know nothing of each other",
                   end2="the machine knows only sectors · the table only ranges · the filesystem only its own partition",
                   end3="so losing the table leaves the data where it was, and formatting one partition leaves the others intact"),
    }),
    "ebr-chain": (fig_ebr_chain, {
        "ko": dict(title="확장 파티션의 EBR 사슬 --- 기준이 둘이라 헷갈린다",
                   ext="확장 파티션 (MBR 의 4번 항목, 종류 0x0F)", ext_s="시작 LBA = E",
                   ebrs=[("EBR 1", "LBA E"), ("EBR 2", "LBA E + d")],
                   logicals=["논리 1", "논리 2"],
                   n1="① 1번 항목 = 바로 뒤의 논리 파티션",
                   n1s="기준 = 이 EBR 자신 (상대 LBA + EBR 의 자리)",
                   n2="② 2번 항목 = 다음 EBR",
                   n2s="기준 = 확장 파티션의 시작 E (상대 LBA + E)",
                   end="같은 표의 두 칸인데 더하는 기준이 다르다 --- 손으로 읽을 때 가장 많이 틀리는 자리"),
        "en": dict(title="the EBR chain of an extended partition --- two different bases confuse it",
                   ext="the extended partition (MBR entry 4, type 0x0F)", ext_s="start LBA = E",
                   ebrs=[("EBR 1", "LBA E"), ("EBR 2", "LBA E + d")],
                   logicals=["logical 1", "logical 2"],
                   n1="1. entry 1 = the logical partition right behind it",
                   n1s="base = this EBR itself (relative LBA + the EBR's place)",
                   n2="2. entry 2 = the next EBR",
                   n2s="base = the start of the extended partition, E (relative LBA + E)",
                   end="two entries of one table with different bases --- the most misread place when doing it by hand"),
    }),
    "ssd-erase": (fig_ssd_erase, {
        "ko": dict(title="SSD 는 쓰는 단위와 지우는 단위가 다르다",
                   s1="① 블록 하나 안에 쪽이 여럿 있다",
                   erase_unit="지우기 단위 = 블록 (수 MiB)", page="쪽 {n}",
                   used="쓰임", free="빈칸",
                   write_unit="쓰기 단위 = 쪽 (수 KiB) · 읽기도 쪽 단위",
                   s2="② 쪽 하나만 고치고 싶다 --- 그런데 제자리 덮어쓰기가 안 된다",
                   steps=["고칠 쪽을 읽고", "새 빈 쪽에 쓰고", "옛 쪽은 「죽음」 표시", "나중에 블록째 지운다"],
                   s3="③ 그래서 「쓰기 증폭」이 생긴다",
                   amp1="4 KiB 를 고치려고 → 블록(수 MiB)을 옮겨 쓰고 지운다",
                   amp2="실제로 쓴 양 ÷ 요청한 양 = 쓰기 증폭 (write amplification)",
                   trim1="TRIM 은 「이 쪽은 이제 안 쓴다」를 알려 주는 신호다",
                   trim2="미리 알면 한가할 때 정리해 둘 수 있어, 쓰기가 느려지는 것을 막는다"),
        "en": dict(title="an SSD writes in one unit and erases in another",
                   s1="1. one block holds many pages",
                   erase_unit="erase unit = a block (several MiB)", page="page {n}",
                   used="used", free="free",
                   write_unit="write unit = a page (several KiB) · reads are by page too",
                   s2="2. you want to change one page --- but it cannot be overwritten in place",
                   steps=["read the page", "write a fresh page", "mark the old one dead", "erase the block later"],
                   s3="3. hence write amplification",
                   amp1="to change 4 KiB -> a block (several MiB) is rewritten and erased",
                   amp2="bytes actually written / bytes asked for = write amplification",
                   trim1="TRIM is the signal that says this page is no longer in use",
                   trim2="knowing in advance lets it tidy up while idle, so writes do not slow down"),
    }),
    "serial-parallel": (fig_serial_parallel, {
        "ko": dict(title="같은 한 바이트를 보내는 두 가지 방법",
                   ser="직렬 --- 선 하나에 비트를 줄지어", par="병렬 --- 선 여덟에 비트를 한꺼번에",
                   tx="보내는 쪽", rx="받는 쪽",
                   ser_note="시간 →   비트가 *차례로* 흐른다 · 선이 적다 · 한 클록에 1비트",
                   par_note="한 클록에 8비트 --- 그런데 여덟 줄이 *똑같은 순간에* 도착해야 한다",
                   end1="빨라질수록 병렬이 진다 --- 줄마다 도착 시각이 어긋나는 폭(스큐)이",
                   end2="한 비트의 시간보다 커지는 순간, 여덟 줄을 맞출 수 없게 된다"),
        "en": dict(title="two ways of sending the same byte",
                   ser="serial --- bits in a queue on one wire", par="parallel --- eight bits at once on eight wires",
                   tx="sender", rx="receiver",
                   ser_note="time →   the bits flow *in turn* · few wires · one bit per clock",
                   par_note="eight bits per clock --- but all eight must arrive at *the same instant*",
                   end1="the faster it goes the more parallel loses --- once the spread in arrival times (skew)",
                   end2="grows larger than one bit's time, the eight lines cannot be lined up"),
    }),
    "dma-path": (fig_dma_path, {
        "ko": dict(title="같은 자료를 옮기는 두 가지 길",
                   by_cpu="① CPU 가 직접 --- 바이트마다 CPU 를 거친다",
                   by_dma="② DMA --- 장치와 기억이 곧장, CPU 는 시작과 끝만",
                   dev="장치", mem="기억", via_reg="레지스터를 거쳐",
                   cpu_note="바이트마다 끼어들기 → CPU 는 그동안 다른 일을 못 한다",
                   dma_c="DMA 제어기", moves="덩어리를 나른다",
                   starts="시작 시킴", one_irq="끝나면 끼어들기 한 번",
                   end="옮기는 일 자체는 같다 --- 달라지는 것은 그동안 CPU 가 자유로운가다"),
        "en": dict(title="two paths for moving the same data",
                   by_cpu="1. the CPU itself --- every byte passes through it",
                   by_dma="2. DMA --- device and memory directly, the CPU only starts and finishes",
                   dev="device", mem="memory", via_reg="through a register",
                   cpu_note="an interrupt per byte → the CPU can do nothing else meanwhile",
                   dma_c="DMA controller", moves="moves it in blocks",
                   starts="starts it", one_irq="one interrupt when done",
                   end="the moving itself is the same --- what differs is whether the CPU is free meanwhile"),
    }),
    "boot-chain": (fig_boot_chain, {
        "ko": dict(title="부팅은 사슬이다 --- 각 칸은 다음 칸을 찾아 검사하고 넘긴다",
                   lanes=[("도스 (BIOS)", ["전원·POST", "부팅 섹터|0x7C00", "IO.SYS|MSDOS.SYS", "COMMAND.COM"]),
                          ("리눅스 (UEFI)", ["전원·펌웨어", "ESP 의 .efi|(PE 형식)", "커널 + initramfs", "PID 1 (init)"]),
                          ("임베디드 RTOS", ["전원·리셋 벡터", "부트 ROM / SPL", "부트로더|(서명 검사)", "펌웨어 이미지|= 앱 + 커널"])],
                   note1="각 칸이 하는 일은 같다 --- 다음 것을 찾고, 읽어 싣고, 맞는지 보고, 제어를 넘긴다",
                   note2="넘길 때 앞 칸은 대개 사라진다 --- 그래서 무엇을 물려주는지가 규약이 된다(명령줄, 메모리 맵, 장치 트리)"),
        "en": dict(title="booting is a chain --- each link finds the next, checks it, and hands over",
                   lanes=[("DOS (BIOS)", ["power, POST", "boot sector|0x7C00", "IO.SYS|MSDOS.SYS", "COMMAND.COM"]),
                          ("Linux (UEFI)", ["power, firmware", ".efi on the ESP|(PE format)", "kernel + initramfs", "PID 1 (init)"]),
                          ("embedded RTOS", ["power, reset vector", "boot ROM / SPL", "bootloader|(signature check)", "firmware image|= app + kernel"])],
                   note1="every link does the same work --- find the next, load it, check it, hand over control",
                   note2="the previous link usually disappears --- so what is handed over becomes the contract (command line, memory map, device tree)"),
    }),
    "addr-space": (fig_addr_space, {
        "ko": dict(title="장치를 어디에 두는가 --- 두 가지 설계",
                   mm="기억 사상 입출력 (memory-mapped I/O)", mm1="적재·저장",
                   mm2="명령 하나로", decoder="해독 회로",
                   devices=["RAM", "UART", "타이머", "..."],
                   mm_note="주소 공간이 하나다 --- 포인터가 그대로 닿는다",
                   pm="포트 입출력 (port-mapped I/O)", pm1="전용 명령",
                   io_space="입출력 공간", io_sub="64 KiB, 따로",
                   mem_space="기억 공간",
                   pm_note="공간이 둘이다 --- C 문법으로는 닿지 못한다"),
        "en": dict(title="where the devices sit --- two designs",
                   mm="memory-mapped I/O", mm1="load and store",
                   mm2="with one instruction", decoder="address decoder",
                   devices=["RAM", "UART", "timer", "..."],
                   mm_note="one address space --- a pointer reaches it directly",
                   pm="port-mapped I/O", pm1="dedicated instructions",
                   io_space="I/O space", io_sub="64 KiB, separate",
                   mem_space="memory space",
                   pm_note="two spaces --- C syntax cannot reach one of them"),
    }),
    "trap-path": (fig_trap_path, {
        "ko": dict(title="0 으로 나누면 무슨 일이 일어나는가 --- 네 걸음",
                   steps=[("① 내 C 코드", "x / zero"), ("② CPU", "예외를 일으킨다"),
                          ("③ 커널", "트랩 처리기가 받는다"), ("④ 내 C 함수", "신호 처리기")],
                   note1="돌아가면? --- 같은 명령을 다시 실행하려 든다. 그래서 무한히 다시 터진다.",
                   note2="표준도 그렇게 적어 두었다: 트랩 처리기에서 그냥 반환하면 미정의 동작이다.",
                   escape="siglongjmp 으로 빠져나온다"),
        "en": dict(title="what happens when you divide by zero --- four steps",
                   steps=[("1. my C code", "x / zero"), ("2. the CPU", "raises an exception"),
                          ("3. the kernel", "the trap handler receives it"),
                          ("4. my C function", "the signal handler")],
                   note1="and on return? --- it retries the same instruction, so it faults again, forever.",
                   note2="the standard says as much: returning from such a handler is undefined behaviour.",
                   escape="leave through siglongjmp"),
    }),
    "irq-save": (fig_irq_save, {
        "ko": dict(title="인터럽트가 걸릴 때 하드웨어가 저장해 주는 양",
                   cols=[("Arm Cortex-M", 8, ["R0–R3", "R12", "LR", "PC", "xPSR"], "보통 C 함수로 충분"),
                         ("AVR", 1, ["PC (2바이트)"], "SREG 저장·reti 를 컴파일러가"),
                         ("RISC-V", 0, [], "쓰는 레지스터 전부를 컴파일러가")],
                   none1="아무것도", none2="저장하지 않는다", count="하드웨어가 {n} 개",
                   note="하드웨어가 해 주는 양이 곧 컴파일러가 해야 할 일의 양이다"),
        "en": dict(title="how much the hardware saves when an interrupt arrives",
                   cols=[("Arm Cortex-M", 8, ["R0–R3", "R12", "LR", "PC", "xPSR"], "an ordinary C function usually suffices"),
                         ("AVR", 1, ["PC (2 bytes)"], "the compiler saves SREG and emits reti"),
                         ("RISC-V", 0, [], "the compiler saves every register used")],
                   none1="it saves", none2="nothing at all", count="{n} saved by hardware",
                   note="what the hardware does is exactly what the compiler need not"),
    }),
    "format-skeleton": (fig_format_skeleton, {
        "ko": dict(title="형식은 달라도 로더가 묻는 질문은 같다",
                   com_t="도스 .COM", com_rows=["(머리가 없다)", "코드 + 자료"],
                   com_memo=["약속: 0x100 에 싣고", "거기서 시작한다"],
                   elf_t="ELF (리눅스)",
                   elf_rows=["ELF 머리", "프로그램 헤더 표",
                             "구역들: .text .data .bss", "심볼·재배치·디버그"],
                   pe_t="PE (윈도우)",
                   pe_rows=["MZ 머리 + 도스 스텁", "PE 머리 + 구역 표",
                            "구역들: .text .data .rdata", "임포트 표(IAT)·재배치"],
                   q="무엇을 어디에 싣는가 · 어디서 시작하는가 · 무엇이 더 필요한가 · 주소를 어떻게 고치는가",
                   q1="매직이 없는 형식(.COM)은 이 질문들의 답을 「약속」으로 대신한다 --- 그래서 하나도 못 바꾼다",
                   q2="매직이 있는 형식은 답을 파일 안에 적는다 --- 그래서 형식이 자란다"),
        "en": dict(title="the formats differ; the loader's questions do not",
                   com_t="DOS .COM", com_rows=["(no header)", "code + data"],
                   com_memo=["by convention: load at 0x100", "and start there"],
                   elf_t="ELF (Linux)",
                   elf_rows=["ELF header", "program header table",
                             "sections: .text .data .bss", "symbols, relocation, debug"],
                   pe_t="PE (Windows)",
                   pe_rows=["MZ header + DOS stub", "PE header + section table",
                            "sections: .text .data .rdata", "import table (IAT), relocation"],
                   q="what goes where · where does it start · what else is needed · how are addresses fixed",
                   q1="a format without a magic number (.COM) answers by convention --- so nothing can change",
                   q2="a format with one writes the answers into the file --- so the format can grow"),
    }),
    "paging": (fig_paging, {
        "ko": dict(title="가상 주소가 물리 주소가 되기까지",
                   virtual="가상 쪽 (프로그램이 보는 것)",
                   physical="물리 쪽 (DRAM 의 자리)",
                   vpage="가상 쪽 {n}", ppage="물리 쪽 {n}",
                   table="쪽 표", table_sub="어느 가상 쪽이 어느 물리 쪽인가",
                   note="이어져 있던 것이 흩어진다 --- 가상에서 이웃이어도 물리에서는 이웃이 아니다.",
                   note2="그래서 「주소가 가깝다」가 「빠르다」를 뜻하지 않는다."),
        "en": dict(title="how a virtual address becomes a physical one",
                   virtual="virtual pages (what the program sees)",
                   physical="physical pages (places in DRAM)",
                   vpage="virtual page {n}", ppage="physical page {n}",
                   table="page table", table_sub="which virtual page is which physical page",
                   note="what was contiguous is scattered --- neighbours in virtual space are not neighbours in physical space.",
                   note2="which is why \"close in address\" does not mean \"fast\"."),
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
