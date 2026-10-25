#!/usr/bin/env python3
"""원고 줄바꿈이 조사·어미 앞에 떨어지지 않게 한다 (review-0053, 2026-10-24).

Typst 는 한글과 한글 사이의 원고 줄바꿈을 공백으로 조판한다(중국어·일본어와 달리
지우지 않는다). 그래서 원고를 접다가 줄바꿈이 낱말과 조사 사이에 떨어지면

    리틀 엔디안            →  「리틀 엔디안 에서는」
    에서는 그것이 ...

처럼 조사가 띄어 찍힌다. 정오표가 1~15장에서 이런 자리를 짚었고, 전권을 훑으니
140곳이 넘게 있었다. 손으로 접다 보면 다시 생기는 종류의 사고라 기계가 지킨다.

  · 다음 줄(앞의 들여쓰기와 `#idx(...)` 를 건너뛴 자리)이 조사·어미로 시작하고
    앞 줄이 낱말로 끝나면 오류.
  · 홀로 선 「이」는 보지 않는다 --- 지시어 「이 장」「이 책」과 가를 수 없다.
  · ``` 코드 블록 안은 보지 않는다.

사용법:  python3 scripts/check-linebreak-particles.py [--check]
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from chapters import is_draft  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent

PARTICLE = (r"(에서도|에서는|에서|으로는|으로|에는|에게|까지|부터|이다|이고|이며|이라|라는|"
            r"이란|만으로|만큼|처럼|보다|은|는|을|를|가|의|도|와|과|로|에|만)")
START = re.compile(PARTICLE + r"""([\s,.:;!?)」"]|$)""")
LEAD = re.compile(r'^\s*(#idx\("[^"]*"\))*\s*')
END = re.compile(r"[가-힣A-Za-z0-9`*)\]]$")

# 띄어 쓰는 것이 맞는 자리: (파일, 앞 줄 끝, 다음 줄 첫 낱말)
ALLOW = {
    ("book/chapters/ch086.typ", "계산하다", "만"),   # 「계산하다 만 값들」 --- 하다가 그만둔
}


def scan():
    out = []
    for f in sorted((ROOT / "book").rglob("*.typ")):
        if is_draft(f):
            continue
        rel = str(f.relative_to(ROOT))
        lines = f.read_text(encoding="utf-8").split("\n")
        raw = False
        for i in range(len(lines) - 1):
            if lines[i].lstrip().startswith("```"):
                raw = not raw
            if raw or lines[i + 1].lstrip().startswith("```"):
                continue
            a = lines[i]
            b = LEAD.sub("", lines[i + 1])
            m = START.match(b)
            if not (END.search(a) and m):
                continue
            last = re.search(r"(\S+)$", a).group(1)
            if (rel, last, m.group(1)) in ALLOW:
                continue
            out.append((rel, i + 1, a[-20:], b[:20]))
    return out


def main():
    found = scan()
    if "--check" not in sys.argv:
        for rel, n, a, b in found:
            print(f"  ⚠️  {rel}:{n}: …{a}⏎{b}")
    print(f"check-linebreak-particles: 줄바꿈 뒤 조사 {len(found)}곳")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
