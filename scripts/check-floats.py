#!/usr/bin/env python3
"""표·그림 번호와 캡션의 기계 점검 (저자 지시 2026-08-06).

번호는 사람이 적지 않는다 — `lib.typ` 의 `dtable`·`figure-svg`·`memrow` 가
장별 카운터로 붙인다. 이 검사가 지키는 것은 셋이다.

  1. 원고에 날것의 `#table(`·`#image(`·`#figure(` 가 없어야 한다.
     (그 길로 새면 번호가 붙지 않는다)
  2. 본문에 "표 5.1"·"그림 5.1" 같은 번호를 손으로 적은 자리가 없어야 한다.
  3. 한국어판과 영어판의 장별 표·그림 개수가 같아야 한다.
     (다르면 두 판의 번호가 어긋나 상호참조가 깨진다)
  4. 참조 이름(`id:`)은 영문 소문자·숫자·하이픈만 쓴다 (저자 지시 2026-08-16).
     한글 id 는 두 판이 같은 이름을 쓸 수 없게 만들고, 파일 이름·검사·도구
     어디서나 다루기 나쁘다.
  5. 같은 참조 이름이 두 판 모두에 있어야 한다.
     (한쪽에만 있으면 그 판에서 `#tblref` 가 대상을 못 찾는다)

사용법:  python3 scripts/check-floats.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

RAW = re.compile(r"(?<!\w)#(table|image|figure)\(")
HAND_KO = re.compile(r"(?<!\w)(표|그림)\s*\d+\.\d+")
HAND_EN = re.compile(r"(?<!\w)(Table|Figure)\s+\d+\.\d+")
CALL = re.compile(r"#(dtable|figure-svg|memrow)\(")
IDDEF = re.compile(r'\bid:\s*"([^"]*)"')
IDOK = re.compile(r"[a-z0-9][a-z0-9-]*\Z")


def files(edition):
    base = ROOT / edition
    for sub in ("chapters", "appendix", "back", "front", "parts"):
        d = base / sub
        if d.exists():
            yield from sorted(d.glob("*.typ"))


def main() -> int:
    problems = []
    counts = {"book": {}, "book-en": {}}
    names = {"book": set(), "book-en": set()}

    for ed in ("book", "book-en"):
        for path in files(ed):
            text = path.read_text(encoding="utf-8")
            rel = path.relative_to(ROOT)
            for m in RAW.finditer(text):
                line = text[:m.start()].count("\n") + 1
                problems.append(f"{rel}:{line}: 날것의 #{m.group(1)}( — "
                                f"dtable/figure-svg/memrow 를 쓸 것")
            hand = HAND_KO if ed == "book" else HAND_EN
            for m in hand.finditer(text):
                line = text[:m.start()].count("\n") + 1
                problems.append(f"{rel}:{line}: 번호를 손으로 적었다 "
                                f"({m.group(0)}) — 캡션 인자로 넘길 것")
            counts[ed][path.name] = len(CALL.findall(text))
            for m in IDDEF.finditer(text):
                name = m.group(1)
                names[ed].add(name)
                if not IDOK.match(name):
                    line = text[:m.start()].count("\n") + 1
                    problems.append(f"{rel}:{line}: 참조 이름에 영문 소문자·숫자·"
                                    f'하이픈만 쓸 것 (id: "{name}")')

    for name, ko in sorted(counts["book"].items()):
        en = counts["book-en"].get(name)
        if en is None:
            continue
        if en != ko:
            problems.append(f"{name}: 표·그림 개수가 판마다 다르다 "
                            f"(한국어 {ko}, 영어 {en}) — 번호가 어긋난다")

    for name in sorted(names["book"] - names["book-en"]):
        problems.append(f'참조 이름 "{name}" 이 한국어판에만 있다 '
                        f"— 영어판 같은 표·그림에도 같은 id 를 붙일 것")
    for name in sorted(names["book-en"] - names["book"]):
        problems.append(f'참조 이름 "{name}" 이 영어판에만 있다 '
                        f"— 한국어판 같은 표·그림에도 같은 id 를 붙일 것")

    total = sum(counts["book"].values())
    if problems:
        for p in problems:
            print("⚠️ ", p)
        print(f"check-floats: {len(problems)} 건 — 표·그림 번호 규칙 위반")
        return 1
    print(f"check-floats: 표·그림 {total}개가 모두 장치를 거쳐 번호를 받고, "
          f"참조 이름 {len(names['book'])}개가 두 판에서 같은 영문 이름이다")
    return 0


if __name__ == "__main__":
    sys.exit(main())
