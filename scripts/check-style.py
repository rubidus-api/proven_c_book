#!/usr/bin/env python3
"""문체의 잔재를 센다 (RFC-0038).

세 갈래로 나눈다.

  금지(forbid) --- 이 원고에 있어서는 안 되는 것. 하나라도 나오면 실패한다.
                   존댓말, 「살펴봅시다」류 권유, 흔한 번역투 관형어.
  주의(warn)   --- 있을 수는 있으나 대개 더 나은 우리말이 있는 것.
                   기준선보다 늘면 실패한다.
  기록(note)   --- 세어만 둔다. 열거 스캐폴딩처럼 *정당한 쓰임과 구별해야 하는* 것.

사용법:
  check-style.py            보고서를 낸다
  check-style.py --check    기준선과 견준다(금지 1건 이상, 또는 주의가 늘면 1)
  check-style.py --write    기준선을 갱신한다 (docs/style-baseline.tsv)
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASELINE = ROOT / "docs" / "style-baseline.tsv"

# ★ 세지 않는 자리
#   - 코드(``` 블록과 `인라인`) --- 코드 안의 글자는 문체가 아니다
#   - 인용부호 안의 문장 --- 「파일을 열 수 없습니다」 같은 *예시 메시지*다
#   - 저자가 직접 쓴 서문 --- 손대지 않는다(AUTHORSHIP 규율)
SKIP_FILES = {"book/front/preface.typ", "book-en/front/preface.typ"}


def strip_code(text):
    text = re.sub(r"```.*?```", " ", text, flags=re.S)   # 코드 블록
    text = re.sub(r"`[^`\n]*`", " ", text)               # 인라인 코드
    text = re.sub(r'"[^"\n]*"', " ", text)               # 인용부호 안
    text = re.sub(r"「[^」\n]*」", " ", text)             # 「 」 안의 인용도 마찬가지
    return text

FORBID_KO = ["입니다", "습니다", "살펴봅시다", "알아봅시다", "해 봅시다",
             "이러한", "다양한", "효과적", "수행한다", "라고 볼 수 있다"]
FORBID_EN = ["Let's ", "utilize", "utilise", "it should be noted", "delve into"]

WARN_KO = ["존재한다", "가능하다", "제공한다", "를 통해", "을 통해",
           "에 대해서", "해당 ", "관련된", "라고 하는", "여러분",
           # ★ 「가치」 자리에 「값」을 쓴 것 (저자 지적 2026-08-28).
           #   이 책에서 「값」은 *value* 라는 기술 용어라 뜻이 부딪힌다.
           #   「값을 한다」·「값이 크다」는 거의 언제나 가치의 뜻이므로 잡는다.
           #   ―「반환할 값이 있다」처럼 진짜 값인 자리를 못 가르는 「값이 있다」는
           #     넣지 않는다(RFC-0038 §4: 기계가 못 가르는 표현은 넣지 않는다).
           "값을 한다", "값을 하는", "값이 크다",
           # ★ 번역투 쓸어내기 (저자 지적 2026-08-28). 영어 have 를 그대로 옮긴
           #   「~을 갖는다」와, 굳어진 외래 표현들. 전부 0 으로 만들어 두었다.
           "를 가진", "을 가진", "를 갖는", "을 갖는", "를 가지고 있", "을 가지고 있",
           "케이스", "다수의"]
# ★ 「에 있어서」는 뺐다 --- 「그 안에 있어서」처럼 옳은 자리를 잘못 잡는다.
#   기계가 못 가르는 표현은 검사기에 넣지 않는다(RFC-0038 §4).
WARN_EN = ["in order to", "the fact that", "Moreover", "essentially", "In summary",
           "Let us", "In other words", "Note that", "robust", "vital", "seamless"]

NOTE_KO = ["첫째", "둘째", "셋째", "넷째", "짚는다", "셈이다", "그것이다",
           "핵심이다", "정리하면", "요약하면", "는 것이다", "즉 ", "곧 "]
NOTE_EN = ["First,", "Second,", "Third,", "In fact,", "Therefore,"]


def files():
    for tree, forb, warn, note in (("book", FORBID_KO, WARN_KO, NOTE_KO),
                                   ("book-en", FORBID_EN, WARN_EN, NOTE_EN)):
        base = ROOT / tree
        for sub in ("chapters", "appendix", "front", "back", "parts"):
            d = base / sub
            if not d.is_dir():
                continue
            for f in sorted(d.glob("*.typ")):
                if str(f.relative_to(ROOT)) in SKIP_FILES:
                    continue
                yield f, forb, warn, note


def count(text, pats):
    out = {p: text.count(p) for p in pats if text.count(p)}
    # ★ 「활용」은 「재활용」(이 책이 일부러 쓰는 비유)과 갈라야 해서 따로 센다
    n = len(re.findall(r"(?<!재)활용", text))
    if n:
        out["활용"] = n
    return out


def scan():
    forbid, warn, note = {}, {}, {}
    for f, fp, wp, np_ in files():
        t = strip_code(f.read_text(encoding="utf-8"))
        rel = str(f.relative_to(ROOT))
        for store, pats in ((forbid, fp), (warn, wp), (note, np_)):
            for pat, n in count(t, pats).items():
                store[(rel, pat)] = n
    return forbid, warn, note


def read_baseline():
    if not BASELINE.exists():
        return {}
    out = {}
    for line in BASELINE.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        kind, rel, pat, n = line.split("\t")
        out[(kind, rel, pat)] = int(n)
    return out


def write_baseline(warn, note):
    rows = ["# 문체 기준선 (RFC-0038) --- check-style.py --write 가 만든다",
            "# 갈래\t파일\t표현\t횟수"]
    for kind, store in (("warn", warn), ("note", note)):
        for (rel, pat), n in sorted(store.items()):
            rows.append(f"{kind}\t{rel}\t{pat}\t{n}")
    BASELINE.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return len(rows) - 2


def main():
    args = sys.argv[1:]
    forbid, warn, note = scan()

    if "--write" in args:
        n = write_baseline(warn, note)
        print(f"check-style: 기준선 갱신 --- {n}건 → {BASELINE.relative_to(ROOT)}")
        return 0

    total = lambda d: sum(d.values())
    if "--check" in args:
        base = read_baseline()
        bad = 0
        for (rel, pat), n in sorted(forbid.items()):
            print(f"  ✗ 금지  {rel}: 「{pat}」 {n}회")
            bad += 1
        for (rel, pat), n in sorted(warn.items()):
            was = base.get(("warn", rel, pat), 0)
            if n > was:
                print(f"  ⚠️  늘었다  {rel}: 「{pat}」 {was} → {n}")
                bad += 1
        if bad:
            print(f"check-style: {bad} 건 --- 문체 잔재가 늘었거나 금지 표현이 있다")
            return 1
        print(f"check-style: 금지 0 · 주의 {total(warn)}건(기준선 안) · 기록 {total(note)}건")
        return 0

    print(f"금지 {total(forbid)}건 · 주의 {total(warn)}건 · 기록 {total(note)}건\n")
    for label, store in (("금지", forbid), ("주의", warn)):
        if not store:
            continue
        print(f"[{label}]")
        agg = {}
        for (rel, pat), n in store.items():
            agg.setdefault(pat, []).append((rel, n))
        for pat, hits in sorted(agg.items(), key=lambda kv: -sum(h[1] for h in kv[1])):
            s = sum(h[1] for h in hits)
            where = ", ".join(f"{pathlib.Path(r).name}({n})" for r, n in sorted(hits)[:6])
            print(f"  {pat:<14} {s:4d}  {where}{' …' if len(hits) > 6 else ''}")
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
