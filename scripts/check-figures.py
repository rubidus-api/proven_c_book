#!/usr/bin/env python3
"""개념도의 글자가 상자를 넘지 않는지 잰다 (저자 지적 2026-08-09).

그림 26.1 의 「정수 타입」 아래 상자들에서 글자가 좌우로 삐져나온 것을 저자가
발견했다. 원인은 단순하다 --- 상자 폭이 `150` 처럼 *손으로 적힌 수*였고, 라벨이
길어지거나 번역이 길어져도 아무도 알려 주지 않았다. 실제로 두 그림에서 9건이
넘치고 있었다(ko 4 · en 5).

그래서 두 가지를 했다.
  · `make-figures.py` 가 `fit_width()` 로 **글자를 재서** 상자를 만든다.
  · 그리고 이 검사가 결과물을 **다시 재서** 넘친 곳이 없는지 확인한다.

만드는 쪽만 고치면 다음에 손으로 좌표를 적는 그림이 하나 생기는 순간 되돌아간다.
결과물을 재는 쪽이 최종 방어선이다.

측정은 조판에 쓰는 실제 글꼴(Noto Sans CJK KR)로 한다. 글꼴이나 PIL 이 없으면
*검사를 건너뛰지 않고* 어림값으로 재되, 그 사실을 인쇄한다 --- 조용히 통과하는
검사는 없느니만 못하다.

사용법: python3 scripts/check-figures.py [--all]
종료 상태: 넘친 곳이 있으면 1
"""
import functools
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONTS = {
    False: ROOT.parent / "toolchains/fonts/noto-cjk-kr/NotoSansCJKkr-Regular.otf",
    True: ROOT.parent / "toolchains/fonts/noto-cjk-kr/NotoSansCJKkr-Bold.otf",
}
# 0.5 단위 미만은 반올림·힌팅 차이로 보고 넘어간다
TOLERANCE = 0.5

RECT = re.compile(r'<rect x="([\d.-]+)" y="([\d.-]+)" width="([\d.]+)" '
                  r'height="([\d.]+)"[^>]*/>')
TEXT = re.compile(r'<text x="([\d.-]+)" y="([\d.-]+)" font-size="([\d.]+)" '
                  r'font-weight="(\w+)" text-anchor="(\w+)"[^>]*>([^<]*)</text>')

_exact = True


@functools.lru_cache(maxsize=None)
def _font(size, bold):
    global _exact
    try:
        from PIL import ImageFont
        return ImageFont.truetype(str(FONTS[bold]), int(size * 4))
    except Exception:
        _exact = False
        return None


def text_width(s, size, bold=False):
    f = _font(size, bold)
    if f is not None:
        return f.getlength(s) / 4
    wide = sum(1 for c in s if ord(c) > 0x2E80)
    return (wide * 1.0 + (len(s) - wide) * 0.55) * size


def overflows(path):
    s = path.read_text(encoding="utf-8")
    rects = [tuple(map(float, m.groups())) for m in RECT.finditer(s)]
    out = []
    for m in TEXT.finditer(s):
        x, y = float(m.group(1)), float(m.group(2))
        size, weight, anchor, label = (float(m.group(3)), m.group(4),
                                       m.group(5), m.group(6))
        if not label.strip():
            continue
        tw = text_width(label, size, weight == "bold")
        x0 = x - tw / 2 if anchor == "middle" else (x if anchor == "start" else x - tw)
        for rx, ry, rw, rh in rects:
            # 이 글자를 담고 있다고 볼 상자: 세로로 겹치고 가로로 안에 있는 것
            if ry <= y <= ry + rh + 2 and rx - 2 <= x <= rx + rw + 2:
                over = max(rx - x0, (x0 + tw) - (rx + rw))
                if over > TOLERANCE:
                    out.append((label, round(tw, 1), round(rw, 1), round(over, 1)))
                break
    return out


def main() -> int:
    bad, checked = [], 0
    for lang in ("ko", "en"):
        d = ROOT / "book" / "figures" / lang
        if not d.exists():
            continue
        for f in sorted(d.glob("*.svg")):
            checked += 1
            for label, tw, rw, over in overflows(f):
                bad.append((lang, f.name, label, tw, rw, over))

    show = bad if "--all" in sys.argv else bad[:20]
    for lang, name, label, tw, rw, over in show:
        print(f'  ⚠️  [{lang}] {name}  "{label[:44]}"  '
              f'글자 {tw} > 상자 {rw} ({over}px 넘침)')
    if bad:
        print("     make-figures.py 에서 fit_width() 로 상자를 재어 만들 것 "
              "— 폭을 손으로 적지 않는다")
    how = "실제 글꼴" if _exact else "★어림값(글꼴이나 PIL 이 없다)"
    print(f"check-figures: 개념도 {checked}개를 {how}으로 쟀다 — "
          f"{'글자가 상자를 넘는 곳 ' + str(len(bad)) + '건' if bad else '넘치는 곳 없다'}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
