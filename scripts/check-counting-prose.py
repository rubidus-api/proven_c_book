#!/usr/bin/env python3
"""장·절·부의 *개수를 세는 서술*을 모아 둔다 (RFC-0028 §3.4).

2026-08-16 의 제1·2부 재편이 이 검사를 만들게 했다. 장 번호를 옮기는 일은
기계가 완벽하게 했지만(149파일·3,789곳·오류 0), 사람이 손으로 고쳐야 했던 곳이
따로 열두 군데 있었다. 전부 *번호가 아니라 개수*를 말하는 문장이었다.

    「이 부의 남은 두 장은 …」      ← 장이 옮겨 가면 둘이 아니게 된다
    「앞선 일곱 장이 …」            ← 앞이 늘면 여덟이 된다
    「독자는 2부를 잡학 열한 장으로」 ← 부 경계가 바뀌면 아홉이 된다

이런 문장은 기계가 고칠 수 없다. 개수의 옳고 그름은 문장의 뜻에 달려 있어서,
「여덟」이 맞는지 「아홉」이 맞는지는 사람이 읽어야 안다. 그래서 이 검사는
*판정하지 않는다* --- 사람이 볼 자리를 좁혀 줄 뿐이다.

쓰는 법
    check-counting-prose.py            목록을 인쇄한다
    check-counting-prose.py --check    기준선과 다르면 1 을 돌려준다 (릴리스 게이트)
    check-counting-prose.py --write    기준선을 지금 상태로 갱신한다

기준선은 `docs/counting-prose.tsv` 다. 장을 옮기거나 부를 다시 가른 뒤에는
이 목록을 한 줄씩 읽고 --- 고칠 것을 고친 다음 --- `--write` 로 갱신한다.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TREES = ("book", "book-en")
BASELINE = ROOT / "docs" / "counting-prose.tsv"

# 한국어: 수 관형사/수사 + 구조 단위. 「32장」처럼 *번호*인 것은 잡지 않는다 ---
# 아라비아 숫자는 빼고 우리말 수사만 본다.
#
# ★「한 장」은 일부러 뺐다. 이 책에서 그 말은 대개 *펀치카드 한 장*이나 종이의
#   이야기이고(9·19장), 장 하나를 가리킬 때는 「장 하나」로 적기 때문이다.
#   반대로 「걸음」과 「묶음」은 부 도입부에서 *부의 구성*을 세는 말이라, 그
#   파일들에서만 본다.
KO_NUM = ("두|세|네|다섯|여섯|일곱|여덟|아홉|열|열한|열두|열세|열네|열다섯|"
          "스무|스물|여러|몇")
KO_UNIT = "장|절|부"
# ★ 단위 뒤에 무엇이 와야 「그 단위」인가. 조사는 붙여 쓰므로(「여덟 장이」) 뒤에
#   한글이 온다고 무조건 버리면 정작 잡아야 할 것을 놓친다 --- 처음 이렇게 짰다가
#   ch11 의 「앞선 여덟 장이」를 통째로 놓쳤다. 그래서 *조사만* 허용한다.
#   반대로 「장치」·「부품」·「부분」·「절대」는 조사가 아니므로 걸러진다.
# ★ 「째」는 넣지 않는다 --- 「장 셋째 사건」처럼 *서수*를 세는 말은 구조의 개수가
#   아니다(기준선을 처음 만들 때 셋을 잘못 물었다).
KO_POST = (r"(?=\s|$|[,.)\]}·—…「」『』:;!?]|"
           r"이|가|은|는|을|를|의|에|과|와|도|만|으로|로|씩|뿐|밖|여)")
KO_PATTERNS = (
    # 두 장 · 여덟 장이 · 열한 장으로 · 세 개의 절
    re.compile(rf"(?<![가-힣])({KO_NUM})\s*(?:개의\s*)?({KO_UNIT}){KO_POST}"),
    # 장 셋 · 절 넷
    re.compile(rf"({KO_UNIT})\s+(둘|셋|넷|다섯|여섯|일곱|여덟|아홉|열){KO_POST}"),
)
# 부 도입부에서만 보는 것 --- 「세 걸음으로 오른다」가 부의 구성을 세는 말이다.
KO_PART_PATTERNS = KO_PATTERNS + (
    re.compile(rf"(?<![가-힣])(한|{KO_NUM})\s*(걸음|묶음){KO_POST}"),
)

# 영어: the two remaining chapters / the preceding eight chapters
# ★ parts 는 뺐다 --- 이 책에서 "three parts" 는 대개 *부품 셋*(CPU·메모리·클록)이다.
EN_NUM = ("one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|"
          "thirteen|fourteen|fifteen|twenty|several")
EN_UNIT = "chapters?|sections?"
EN_PATTERNS = (
    re.compile(rf"\b({EN_NUM})\s+(?:remaining\s+|more\s+|other\s+)?({EN_UNIT})\b", re.I),
    re.compile(rf"\b(the\s+(?:preceding|remaining|last|first|next)\s+({EN_NUM})\s+({EN_UNIT}))\b", re.I),
)
EN_PART_PATTERNS = EN_PATTERNS + (
    re.compile(rf"\b(in\s+({EN_NUM})\s+steps?)\b", re.I),
)

# 코드 블록과 인라인 코드는 산문이 아니다.
FENCE = re.compile(r"```[\s\S]*?```")
INLINE = re.compile(r"`[^`]*`")


def prose_lines(path):
    """코드를 지운 뒤, (줄 번호, 줄) 을 돌려준다."""
    raw = path.read_text(encoding="utf-8")
    # 코드 블록은 줄 수를 유지한 채 지운다 --- 줄 번호가 어긋나면 안 된다.
    def blank(m):
        return "\n" * m.group(0).count("\n")
    text = FENCE.sub(blank, raw)
    for i, line in enumerate(text.splitlines(), 1):
        yield i, INLINE.sub(" ", line)


def find(path, patterns):
    out = []
    for no, line in prose_lines(path):
        for pat in patterns:
            for m in pat.finditer(line):
                phrase = " ".join(m.group(0).split())
                ctx = " ".join(line.strip().split())
                out.append((no, phrase, ctx))
    return out


def collect():
    rows = []
    for tree in TREES:
        base = ROOT / tree
        if not base.exists():
            continue
        for path in sorted(base.rglob("*.typ")):
            rel = str(path.relative_to(ROOT))
            in_part_intro = "/parts/" in rel
            if tree == "book":
                pats = KO_PART_PATTERNS if in_part_intro else KO_PATTERNS
            else:
                pats = EN_PART_PATTERNS if in_part_intro else EN_PATTERNS
            for no, phrase, ctx in find(path, pats):
                rows.append((rel, no, phrase, ctx))
    return rows


def key_of(rows):
    """줄 번호는 문장이 밀리기만 해도 바뀐다 --- 기준선은 (파일, 표현)으로 잡는다."""
    return sorted({(r[0], r[2]) for r in rows})


def read_baseline():
    if not BASELINE.exists():
        return None
    out = []
    for line in BASELINE.read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            f, phrase = line.split("\t")[:2]
            out.append((f, phrase))
    return sorted(out)


def write_baseline(rows):
    keys = key_of(rows)
    body = ["# 장·절·부의 개수를 세는 서술 (RFC-0028 §3.4).",
            "# 재편 뒤에는 한 줄씩 읽고 고친 다음 --- scripts/check-counting-prose.py --write",
            "# 파일\t표현"]
    body += [f"{f}\t{p}" for f, p in keys]
    BASELINE.write_text("\n".join(body) + "\n", encoding="utf-8")
    return len(keys)


def main():
    rows = collect()
    if "--write" in sys.argv:
        n = write_baseline(rows)
        print(f"check-counting-prose: 기준선 갱신 --- {n}건 → {BASELINE.relative_to(ROOT)}")
        return 0

    if "--check" in sys.argv:
        base = read_baseline()
        now = key_of(rows)
        if base is None:
            print("check-counting-prose: 기준선이 없다 --- --write 로 만든다", file=sys.stderr)
            return 1
        added = [x for x in now if x not in base]
        gone = [x for x in base if x not in now]
        if not added and not gone:
            print(f"check-counting-prose: 개수를 세는 서술 {len(now)}건 --- 기준선과 같다")
            return 0
        for f, p in added:
            print(f"  + {f}\t{p}")
        for f, p in gone:
            print(f"  - {f}\t{p}")
        print(f"check-counting-prose: 기준선과 다르다 (새로 {len(added)}건, 사라진 것 {len(gone)}건)",
              file=sys.stderr)
        print("  장을 옮겼다면 이 문장들의 *개수*가 아직 맞는지 읽어 보고,",
              file=sys.stderr)
        print("  맞다면 --write 로 기준선을 갱신한다.", file=sys.stderr)
        return 1

    cur = None
    for f, no, phrase, ctx in rows:
        if f != cur:
            print(f"\n── {f}")
            cur = f
        print(f"  {no:>5}  {phrase:<16} {ctx[:96]}")
    print(f"\ncheck-counting-prose: 개수를 세는 서술 {len(rows)}건 "
          f"({len(key_of(rows))}가지)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
