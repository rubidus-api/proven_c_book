#!/usr/bin/env python3
"""빌려 온 문헌과 본문 사이의 *축자 겹침*을 잰다 (저자 요청 2026-08-10).

저자의 물음은 이랬다 --- 「문단 또는 문장 단위 표절은 있을 수 있지 않을까.」
그 물음에 짐작으로 답하지 않으려면 재는 수밖에 없다. 표절 판정의 표준적 방법은
정규화한 낱말열을 n-그램으로 쪼개 교집합을 보는 것이고, 이 검사가 그것을 한다.

  ★ 원문 텍스트는 *저장소에 담지 않는다.* 저작물이기 때문이다.
    비교 대상은 저장소 밖 경로로 넘긴다.

사용법:
    python3 scripts/check-borrowed.py <원문파일.txt> [...]
    python3 scripts/check-borrowed.py --min 12 /path/to/source.txt

무엇을 걸러 내는가
  · 코드 블록과 인라인 코드 --- 관용구가 겹치는 것은 표절이 아니다.
  · 8낱말 미만의 짧은 일치 --- 기술 영어의 상투구다
    (예: "at an address that is a multiple of").

무엇을 사람이 봐야 하는가
  이 검사는 *일치를 찾아 줄 뿐 판정하지 않는다.* 긴 일치가 나오면 그 자리가
  ① 출처를 밝힌 직접 인용인가 ② 표준 문서처럼 양쪽이 같은 원전을 인용한 것인가
  ③ 둘 다 아닌가 를 사람이 확인한다. ③이면 고쳐야 한다.

2026-08-10 의 실측(Expert C Programming 대조): 8낱말 이상 최대 연속 일치 13건.
그중 실질적인 것은 60장의 두 곳뿐이었고 *둘 다 저자명·따옴표·각주가 붙은 직접
인용*이었다. 나머지는 표준 조문의 공통 인용이거나 기술 영어의 상투구였다.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MIN_DEFAULT = 8


def words(text):
    t = text.lower()
    t = re.sub(r"[^a-z0-9\s]", " ", t)
    return t.split()


def book_words():
    """영어판 본문에서 코드를 걷어 낸 낱말열을 장별로 돌려준다."""
    out = {}
    base = ROOT / "book-en" / "chapters"
    if not base.exists():
        return out
    for f in sorted(base.glob("ch*.typ")):
        t = f.read_text(encoding="utf-8")
        t = re.sub(r"```.*?```", " ", t, flags=re.S)   # 코드 블록
        t = re.sub(r"`[^`]*`", " ", t)                  # 인라인 코드
        t = re.sub(r"#[a-z-]+\([^)]*\)", " ", t)        # 장치 호출
        t = re.sub(r"[#*_\[\]]", " ", t)
        out[f.stem] = words(t)
    return out


def maximal_runs(ours, src, n):
    """겹치는 n-그램을 이어 붙여 *최대 연속 일치 구간*만 남긴다."""
    index = {}
    for i in range(len(src) - n + 1):
        index.setdefault(tuple(src[i:i + n]), i)
    runs = []
    for ch, ws in ours.items():
        i = 0
        while i <= len(ws) - n:
            g = tuple(ws[i:i + n])
            k = index.get(g)
            if k is None:
                i += 1
                continue
            j = i
            while j + n < len(ws) and k + n < len(src) and ws[j + n] == src[k + n]:
                j += 1
                k += 1
            runs.append((j + n - i, ch, " ".join(ws[i:j + n])))
            i = j + n
    runs.sort(reverse=True)
    return runs


def main() -> int:
    args = [a for a in sys.argv[1:]]
    n = MIN_DEFAULT
    if "--min" in args:
        k = args.index("--min")
        n = int(args[k + 1])
        del args[k:k + 2]
    if not args:
        print(__doc__.strip().splitlines()[0])
        print("사용법: python3 scripts/check-borrowed.py <원문파일.txt> [...]")
        return 2

    ours = book_words()
    if not ours:
        print("check-borrowed: 영어판 원고가 없다 --- 건너뛴다")
        return 0
    total = sum(len(v) for v in ours.values())

    worst = 0
    for path in args:
        p = pathlib.Path(path)
        if not p.exists():
            print(f"  ⚠️  없는 파일: {p}")
            return 2
        src = words(p.read_text(encoding="utf-8", errors="replace"))
        runs = maximal_runs(ours, src, n)
        worst = max(worst, runs[0][0] if runs else 0)
        print(f"[{p.name}] 원문 {len(src):,}낱말 · 본문 {total:,}낱말 "
              f"· {n}낱말 이상 최대 연속 일치 {len(runs)}건")
        for length, ch, g in runs[:25]:
            print(f"   {length:3}낱말 [{ch}] {g[:110]}")
        if len(runs) > 25:
            print(f"   … 그리고 {len(runs) - 25}건 더")

    print("check-borrowed: 일치를 찾았을 뿐 판정하지 않았다 --- "
          "긴 일치는 출처 표시가 있는지 사람이 확인할 것")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
