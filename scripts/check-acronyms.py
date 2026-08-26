#!/usr/bin/env python3
"""약어가 *그 자리에서* 풀이되는지 본다 (RFC-0041 §3).

저자 지시는 하나다 --- 「약자로 나오는 것들은 챕터별로 맨 처음에 무엇의 약자인지를
괄호 안의 영어 원문으로」. 그대로 다 지키면 `CPU` 를 백 번 풀어 쓰게 되므로,
약어를 세 갈래로 나누어 `docs/acronyms.tsv` 에 정책을 적고 이 검사기가 그것만 지킨다.

    chapter   장마다 첫 자리에서 풀이가 있어야 한다
    book      책 전체에서 한 번(가장 먼저 나오는 장)만 있으면 된다
    book:N    책에서 한 번이되 *N 장에서* 푼다 --- 스쳐 지나가는 자리가 앞에 있을 때
    skip      보지 않는다

풀이로 인정하는 신호 (첫 등장 언저리 ±400자)
    · 약어 뒤 괄호에 영어 원문   ABI(application binary interface)
    · 한국어 이름 뒤 괄호에 원문과 약어   응용 이진 인터페이스(application binary interface, 줄여서 ABI)
    · 줄표로 푼 것   ABI --- 응용 이진 인터페이스
    · `#idx("…")` 로 색인에 올린 자리에 원문이 함께 있는 경우

쓰는 법
    check-acronyms.py            어긴 곳을 찍는다
    check-acronyms.py --check    조용히, 어긴 곳이 있으면 1
    check-acronyms.py --list     장별 현황을 표로
"""
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TSV = ROOT / "docs" / "acronyms.tsv"
KO = ROOT / "book" / "chapters"
EN = ROOT / "book-en" / "chapters"

WINDOW = 400


def policy():
    out = {}
    for line in TSV.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        p = line.split("\t")
        if len(p) < 2:
            continue
        out[p[0].strip()] = (p[1].strip(),
                             p[2].strip() if len(p) > 2 else "",
                             p[3].strip() if len(p) > 3 else "")
    return out


def chapters(base=KO):
    out = {}
    for f in sorted(base.glob("ch*.typ")):
        m = re.match(r"ch(\d+)\.typ", f.name)
        if m:
            out[int(m.group(1))] = f.read_text(encoding="utf-8")
    return out


def bare(text):
    """코드는 뺀다 --- 코드 안의 대문자는 약어가 아니라 이름이다."""
    text = re.sub(r"```.*?```", " ", text, flags=re.S)
    return re.sub(r"`[^`]*`", " ", text)


def glossed(acr, english, window):
    """이 창 안에서 약어가 풀렸는가."""
    # 줄바꿈이 낱말을 가르는 일이 잦다 --- 공백을 하나로 눌러 놓고 본다.
    window = re.sub(r"\s+", " ", window)
    if re.search(re.escape(acr) + r"[*_]{0,2}\s*\([A-Za-z][^)]{5,}\)", window):
        return True
    if english and re.search(re.escape(english[:12]), window, re.I):
        return True
    # 「(undefined behavior, UB)」처럼 괄호 안에 원문과 약어를 함께 적은 꼴.
    # 철자 차이(behaviour/behavior)에 걸리지 않도록 *괄호의 모양*으로 본다.
    if re.search(r"\([A-Za-z][A-Za-z0-9 .,'/-]{5,}[, ]\s*(?:줄여서\s*|or\s+)?"
                 + re.escape(acr) + r"\)", window):
        return True
    if re.search(r"\([^)]{5,},\s*or\s+" + re.escape(acr) + r"\b", window):
        return True
    if re.search(re.escape(acr) + r"[^\n]{0,20}---[^\n]{3,}", window):
        return True
    return False


def scan(base=KO):
    pol, ko = policy(), chapters(base)
    seen_book = {}
    rows = []
    for n in sorted(ko):
        text = bare(ko[n])
        for acr, (kind, korean, english) in pol.items():
            if kind == "skip":
                continue
            m = re.search(r"(?<![A-Za-z0-9_])" + re.escape(acr) + r"(?![A-Za-z0-9_])", text)
            if not m:
                continue
            first_in_book = acr not in seen_book
            seen_book.setdefault(acr, n)
            if kind.startswith("book:"):
                # 「이 장에서 풀라」고 사람이 정해 둔 자리 --- 그 앞의 스침은 보지 않는다
                if n != int(kind.split(":")[1]):
                    continue
            elif kind == "book" and not first_in_book:
                continue
            # book:N 은 「이 장 안에서 풀라」는 뜻이다 --- 자리는 사람이 고른다.
            if kind.startswith("book:"):
                w = text
            else:
                # 첫 등장 언저리, *또는* 장 서두(제목~첫 절 제목 앞). 제목에 약어가
                # 든 장은 서두에서 풀어 주는 것이 자연스럽다 --- 56장이 그런 자리다.
                head = text[: text.index("\n== ")] if "\n== " in text else text[:2000]
                w = head + " " + text[max(0, m.start() - WINDOW): m.start() + WINDOW]
            rows.append((n, acr, kind, glossed(acr, english, w)))
    return rows


def main():
    rows = ([(n, a, k, ok, "ko") for (n, a, k, ok) in scan(KO)]
            + [(n, a, k, ok, "en") for (n, a, k, ok) in scan(EN)])
    bad = [r for r in rows if not r[3]]
    quiet = "--check" in sys.argv
    if "--list" in sys.argv:
        for n, acr, kind, ok, ed in rows:
            print(f"  [{ed}] {n:>3}장  {acr:<8} {kind:<8} {'풀이 있음' if ok else '풀이 없음'}")
    elif not quiet:
        for n, acr, kind, ok, ed in bad:
            where = ("장마다 풀어야 하는 약어" if kind == "chapter"
                     else "사람이 정해 둔 풀이 자리" if kind.startswith("book:")
                     else "책에서 처음 나오는 자리")
            print(f"  ⚠️  [{ed}] {n:>3}장  {acr:<8} --- {where}인데 풀이가 없다")
    print(f"check-acronyms: 검사 {len(rows)}쌍 · 풀이 없는 곳 {len(bad)}곳")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
