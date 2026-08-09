#!/usr/bin/env python3
"""비유 낱말의 규율을 지킨다 (RFC-0022, 저자 지시 2026-08-09).

「지도」는 이 책에서 한 낱말이 네 가지 일을 하도록 남용되어 있었다 --- 공간 배치,
분류, 판세 조망, 길잡이. 한국어 화자에게는 그중 어느 것도 「지도」로 들리지 않고,
영어판도 `map` 을 100회 넘게 써서 같은 병을 앓았다. 그래서 갈래마다 낱말을 갈랐다.

    ★ 「지도」는 *지리*를 말할 때만 쓴다. C 에는 지리가 없다.

      공간에 놓인 것      → 배치            (layout)
      갈래로 나뉜 것      → 갈래            (families)
      판세를 훑는 것      → 지형            (landscape)
      먼저 그려 두는 큰 틀 → 밑그림·길잡이   (sketch, guide)

이 검사가 없으면 다음 집필에서 잊힌다. 그래서 게이트로 둔다.

★ 정규식에 두 가지 주의가 들어 있다(둘 다 실제로 밟은 함정이다).
  1. 한국어 「-지도」는 조사다 --- 「읽히지도」·「나머지도」·「되는지도」. 전체
     118건 중 65건이 이것이었다. 앞 글자가 한글이면 조사로 보고 거른다.
  2. Typst 마크업이 끼어든다 --- `*다른 지도*를` 처럼 강조 기호가 낱말과 조사
     사이에 들어가면 소박한 정규식이 놓친다. 마크업을 허용해야 한다.

사용법: python3 scripts/check-metaphor.py [--list]
종료 상태: 허용 목록에 없는 자리가 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ALLOW = ROOT / "docs" / "metaphor-allow.tsv"

MARKUP = r"[*_`\]」』]*"          # 낱말과 조사 사이에 낄 수 있는 것
JOSA = "를은는이가다에로와의처럼란"

# 한국어: 명사 「지도」만. 앞 글자가 한글이면 조사 「-지도」이므로 뺀다.
KO = re.compile(rf"(?<![가-힣])지도(?={MARKUP}[{JOSA}]|{MARKUP}(?:$|\s|\)|\]|」|·))")

# 영어: 비유로 쓰인 map 만. 아래는 *진짜* map 이라 세지 않는다.
EN = re.compile(r"\bmaps?\b", re.I)
EN_OK = re.compile(
    r"hash ?map|\bmmap\b|memory[- ]map|map file|\.map\(|`map|proven_map"
    r"|map lookup|map operation|inside the map|the map copies|outlive the map"
    r"|map grows|map owns|map's internals|running a map|map, integrity"
    r"|maps? (?:a|the|them|the whole)\b|mapped",
    re.I,
)

HINT = ("지도 → 배치(공간) · 갈래(분류) · 지형(판세) · 밑그림/길잡이(큰 틀).  "
        "RFC-0022 §2.1")


def allow_list():
    """허용 목록: (파일이름, 낱말) 짝."""
    out = set()
    if not ALLOW.exists():
        return out
    for line in ALLOW.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) >= 2:
            out.add((parts[0].strip(), parts[1].strip()))
    return out


def scan(edition, pattern, skip=None):
    hits = []
    base = ROOT / edition
    if not base.exists():
        return hits
    for f in sorted(base.rglob("*")):
        if f.suffix not in (".typ", ".svg"):
            continue
        for ln, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
            for m in pattern.finditer(line):
                a = max(0, m.start() - 44)
                ctx = re.sub(r"\s+", " ", line[a:m.end() + 38]).strip()
                if skip and skip.search(ctx):
                    continue
                hits.append((f.name, ln, m.group(0), ctx))
    return hits


def main() -> int:
    allowed = allow_list()
    hits = scan("book", KO) + scan("book-en", EN, EN_OK)
    bad = [h for h in hits if (h[0], h[2]) not in allowed]

    for name, ln, word, ctx in bad[:30]:
        print(f"  ⚠️  {name}:{ln}  [{word}]  …{ctx}…")
    if bad:
        print(f"     {HINT}")
        print("     그 자리에서 옳다고 판단했다면 docs/metaphor-allow.tsv 에 올린다.")
    allowed_n = len(hits) - len(bad)
    tail = f" (허용 목록 {allowed_n}건)" if allowed_n else ""
    print(f"check-metaphor: 비유 낱말 규율 --- "
          f"{'어긋난 곳 ' + str(len(bad)) + '건' if bad else '어긋난 곳 없다'}{tail}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
