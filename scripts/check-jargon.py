#!/usr/bin/env python3
"""어려운 낱말이 *처음 나올 때* 설명이 붙어 있는지 본다 (RFC-0039).

글쓴이에게 자명한 낱말은 설명 없이 지나가기 쉽다. 그 자리는 기계가 뜻으로
찾지 못하므로, *사람이 한 번 찾은 것을 목록으로 남기고* 검사기가 그 목록을
지킨다.

  목록  : docs/jargon.tsv   (낱말 \t 메모)
  판정  : 첫 등장 자리 둘레에 설명 신호가 있는가
          --- 영어 병기 `낱말(english)`, 「~이란/~라 한다/~라 부른다」,
              `#idx("낱말")`, 또는 낱말 바로 뒤의 괄호·줄표 풀이

사용법:
  check-jargon.py           보고서
  check-jargon.py --check   설명 없는 자리가 있으면 1 을 돌려준다
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LIST = ROOT / "docs" / "jargon.tsv"


def chapter_files():
    reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
    ids = []
    for g in re.findall(r"chapters: \(([^)]*)\)", reg):
        ids += [x.strip().strip('"') for x in g.split(",") if x.strip()]
    for i in range(1, len(ids) + 1):
        f = ROOT / "book" / "chapters" / (f"ch{i:02d}.typ" if i < 10 else f"ch{i}.typ")
        if f.exists():
            yield f"{i}장", f
    # ★ 부록도 원고다 --- 장만 훑는 검사는 새 글이 들어오는 자리에서 눈을 감는다.
    for f in sorted((ROOT / "book" / "appendix").glob("*.typ")):
        yield f"부록 {f.stem}", f


def terms():
    out = []
    for line in LIST.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t")
        out.append((parts[0].strip(), parts[1].strip() if len(parts) > 1 else ""))
    return out


def explained(term, text, pos):
    """첫 등장 자리 둘레에 설명 신호가 있는가.

    ★ 판정은 넉넉하게 한다. 놓치는 것보다 *잘못 잡는 것*이 더 나쁘다 ---
      사람이 확인하러 갔다가 「이미 설명돼 있는데?」를 되풀이하면 검사기를 안 믿게 된다.
    """
    # 표시 문법(*강조*)은 걷어 내고 본다 --- 「*바이어스*(bias)」 같은 자리 때문에
    after = re.sub(r"^[*_\s]+", "", text[pos + len(term): pos + len(term) + 60])
    before = text[max(0, pos - 40): pos]

    if re.match(r"[(（]\s*[A-Za-z]", after):            # 바이어스(bias)
        return True
    if re.match(r"[(（][^)）]*[가-힣]", after):          # 직교(서로 영향을 주지 않는다)
        return True
    if re.search(r"[가-힣][^()（）]{0,20}[(（][*_]*$", before):   # 번역 장치(MMU)
        return True
    if re.match(r"\s*[—–-]{1,3}\s*[가-힣]", after):    # ASLR --- 공격자가 …
        return True

    # 같은 장 안에서 조금 뒤에 풀어 주는 경우도 설명으로 친다(절 제목 뒤 본문 등)
    window = text[max(0, pos - 300): pos + 1500]
    if re.search(r"#idx\(\"[^\"]*" + re.escape(term), window):
        return True
    # 첫 등장 바로 그 자리가 아니라 몇 줄 뒤에서 풀어 주는 경우 --- 그것도 설명이다
    if re.search(re.escape(term) + r"[*_\s]*[(（]\s*[A-Za-z가-힣]", window):
        return True
    for sig in ("이란", "란 무엇", "라 한다", "라고 한다", "라 부른다", "라고 부른다",
                "이라 한다", "이라 부른다", "줄임", "약자", "뜻이다", "말한다", "뜻한다"):
        if sig in window:
            return True
    return False


def main():
    strip = lambda t: re.sub(r"```.*?```", " ", t, flags=re.S)
    seen, missing = {}, []
    for n, f in chapter_files():
        t = strip(f.read_text(encoding="utf-8"))
        for term, memo in terms():
            if term in seen:
                continue
            m = re.search(re.escape(term), t)
            if not m:
                continue
            ok = explained(term, t, m.start())
            ctx = " ".join(t[max(0, m.start() - 60): m.start() + 90].split())
            seen[term] = (n, ok, ctx, memo)
            if not ok:
                missing.append((n, term, ctx, memo))

    for term, memo in terms():
        if term not in seen:
            print(f"  · {term}: 원고에 아직 없다 ({memo})")

    if "--check" in sys.argv:
        for n, term, ctx, memo in sorted(missing):
            print(f"  ⚠️  {n} 「{term}」 첫 등장에 설명이 없다 --- {memo}")
            print(f"        …{ctx}…")
        if missing:
            print(f"check-jargon: {len(missing)} 건 --- 처음 나오는 자리에 한 줄을 붙일 것")
            return 1
        print(f"check-jargon: 목록의 낱말 {len(seen)}개가 모두 첫 자리에서 설명된다")
        return 0

    print(f"낱말 {len(seen)}개 검사 · 설명 없는 자리 {len(missing)}건\n")
    for term, (n, ok, ctx, memo) in sorted(seen.items(), key=lambda kv: kv[1][0]):
        mark = "  " if ok else "★ "
        print(f"{mark}{n:3d}장 {term:<8} {'설명 있음' if ok else '설명 없음'}  …{ctx[:70]}…")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
