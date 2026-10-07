#!/usr/bin/env python3
r"""PDF 에 박힌 글꼴이 정한 글꼴뿐인가 (LESSONS 2026-08-05 의 손 확인을 기계로).

조판기는 글꼴 목록에 없는 글자를 만나면 *아무 시스템 글꼴*이나 집어 오고, 빌드는
성공한다. 한글이 중국어·일본어 글꼴로 나간 적이 있고(v0.3.0), ✗ 한 글자가
`FreeMono` 로, 수식 안의 한글이 `NotoSansCJKjp` 로 들어온 적이 있다(v0.94.2).

정한 글꼴: Noto Serif/Sans (CJK KR), D2Coding, Noto Sans Mono, 수식용 New Computer Modern Math.

사용법: python3 scripts/check-pdf-fonts.py [PDF ...]   (기본: build/book.pdf build/book-en.pdf)
종료 상태: 정하지 않은 글꼴이 있거나 PDF 가 없으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ALLOWED = re.compile(r"^(NotoSerifCJKkr|NotoSansCJKkr|NotoSerif|NotoSans|NotoSansMono|D2Coding|D2CodingBold|NewCMMath)(-[A-Za-z]+)?$")


def fonts(path):
    data = path.read_bytes()
    names = set()
    for n in re.findall(rb"/BaseFont\s*/([A-Za-z0-9+#.,_-]+)", data):
        name = n.decode().split("+")[-1]
        names.add(re.sub(r"-Identity-H$", "", name))
    return names


def main():
    paths = [pathlib.Path(a) for a in sys.argv[1:]] or [ROOT / "build" / "book.pdf", ROOT / "build" / "book-en.pdf"]
    bad = 0
    for p in paths:
        if not p.exists():
            bad += 1
            print(f"  ⚠️  {p.name}: 없다 --- 재지 못한 것을 통과로 치지 않는다")
            continue
        names = fonts(p)
        if not names:
            bad += 1
            print(f"  ⚠️  {p.name}: 글꼴 이름을 하나도 읽지 못했다")
            continue
        stray = sorted(n for n in names if not ALLOWED.match(n))
        if stray:
            bad += 1
            print(f"  ⚠️  {p.name}: 정하지 않은 글꼴 {', '.join(stray)}")
        else:
            print(f"  {p.name}: 글꼴 {len(names)}개 --- 모두 정한 글꼴이다")
    if bad:
        print("check-pdf-fonts: 정하지 않은 글꼴이 PDF 에 박혔다 --- 글꼴 목록에 없는 글자가 있다")
        return 1
    print("check-pdf-fonts: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
