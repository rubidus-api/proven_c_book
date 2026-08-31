#!/usr/bin/env python3
"""용어의 영어 병기와 색인 표시를 검사한다 (RFC-0019, 저자 지시 2026-08-08).

규율은 한 줄이다 --- **개념을 정의하는 자리에 병기와 색인 표시가 함께 있어야 한다.**

    정의하는 자리 = `한국어(English)` 병기 + `#idx("한국어")`

「정의하는 자리」는 다음 우선순위로 기계가 정한다(RFC-0019 §1).

    ① 그 용어가 절 제목에 나오는 첫 자리
    ② 없으면 본문에서 굵게(*용어*) 처음 강조된 자리
    ③ 없으면 첫 등장

용어 사전은 손으로 관리하지 않는다. 원고에서 뽑은 것(이미 병기된 쌍, 색인 표제어)에
`docs/terms.tsv`(사람이 더하는 보충 목록)를 합친다.

만드는 것:
  docs/TERMS.md   용어 ↔ 원어 ↔ 정의 장 ↔ 병기·색인 여부 (한↔영 대조표를 겸한다)

사용법: python3 scripts/check-terms.py [--quiet]
종료 상태: 어긋난 곳이 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO = ROOT / "book" / "chapters"
EN = ROOT / "book-en" / "chapters"
TSV = ROOT / "docs" / "terms.tsv"

# 병기하지 않는 것들(RFC-0019 §2) — 색인에는 있을 수 있다
NO_GLOSS = {
    "UTF-8", "UTF-16", "UTF-32", "JSON", "IEEE 754", "ASCII", "EBCDIC", "POSIX",
    "MISRA", "bss", "main", "const", "volatile", "typedef", "bool", "nullptr",
    "assert", "fork", "nodiscard", "localeconv", "Duff's device", "introsort",
    "BCP 47", "ISO 3166", "ISO 4217", "ISO 639", "NUL 문자",
}
# 정규식이 잘못 무는 조각을 걸러 낸다. 병기 표기 `한국어(english)` 앞에는 조사나
# 용언이 붙기 쉬워서, 명사구가 아닌 것을 여기서 떨어낸다.
BAD_TAIL = ("고", "서", "며", "면", "지", "게", "야", "라", "다", "은", "는", "이",
            "가", "을", "를", "에", "로", "와", "과", "도", "만", "의")
BAD_KO = re.compile(r"^(가|는|을|를|의|와|과|이|그|저|것|수|때|더|또|및|첫|한)$|장$|판$")


def _tsv_terms():
    out = set()
    if TSV.exists():
        for line in TSV.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.lstrip().startswith("#"):
                out.add(line.split("\t")[0].strip())
    return out


_KNOWN = _tsv_terms()


def junk(k):
    """개념어가 아닌 조각인가. 사전에 오른 낱말은 어떤 어미든 정상이다."""
    if k in _KNOWN:
        return False
    if BAD_KO.search(k) or len(k) < 2:
        return True
    if k.endswith(BAD_TAIL):          # 용언·조사로 끝나면 명사구가 아니다
        return True
    if len(k.split()) > 2:            # 사전 밖의 3어절 이상은 문장 조각이다
        return True
    return False

GLOSS = re.compile(r"([가-힣][가-힣 ·']{0,14}?)\(([a-z][a-zA-Z0-9 ,'\-/]{2,36})\)")
IDX = re.compile(r'#idx\("([^"]+)"\)')
SEC = re.compile(r"^==+ (.+)$", re.M)
BOLD = re.compile(r"\*([가-힣][가-힣 ·']{1,18})\*")


def chapters(base):
    out = {}
    for f in sorted(base.glob("ch*.typ")):
        m = re.match(r"ch(\d+)\.typ$", f.name)
        if m:
            out[int(m.group(1))] = f.read_text(encoding="utf-8")
    return out


def load_dictionary(ko):
    """용어 사전 = 원고에서 뽑은 것 + docs/terms.tsv 보충."""
    terms = {}          # 한국어 → 원어(없으면 "")
    for n in sorted(ko):
        for m in GLOSS.finditer(ko[n]):
            k, e = m.group(1).strip(), m.group(2).strip()
            if junk(k):
                continue
            terms.setdefault(k, e)
    for n in sorted(ko):
        for m in IDX.finditer(ko[n]):
            k = m.group(1)
            if not junk(k) or k in terms:
                terms.setdefault(k, "")
    noglossed, dropped = set(), set()
    if TSV.exists():
        for line in TSV.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            parts = line.split("\t")
            k = parts[0].strip()
            e = parts[1].strip() if len(parts) > 1 else ""
            if not k:
                continue
            if k.startswith("!"):        # 「!용어」는 사전에서 아예 뺀다
                terms.pop(k[1:], None)
                dropped.add(k[1:])
                continue
            terms[k] = e              # tsv 가 원어의 정본이다
            if not e:
                noglossed.add(k)      # 원어 칸이 비면 「병기하지 않는 것」
    for d in dropped:
        terms.pop(d, None)
    return terms, noglossed


def occurs(term, text):
    """★ 용어가 *낱말로* 나오는가. 그냥 `in` 으로 보면 다른 낱말 속을 짚는다 ---
    「스필」이 「루카스필름」에 걸려 33장을 정의 자리로 잡았다. 앞뒤가 한글이면
    다른 낱말의 조각이다(조사·어미는 한글이 아닌 경계로 치지 않으므로 뒤는
    허용한다 --- 「스필로」·「스필을」은 같은 낱말이다)."""
    i = text.find(term)
    while i != -1:
        before = text[i - 1] if i > 0 else " "
        if not ("가" <= before <= "힣"):
            return True
        i = text.find(term, i + 1)
    return False


def defining_chapter(term, ko):
    """정의하는 자리(장 번호)와 그 근거를 돌려준다."""
    for n in sorted(ko):
        for t in SEC.findall(ko[n]):
            if term in t:
                return n, "절 제목"
    for n in sorted(ko):
        if any(term == b.strip() for b in BOLD.findall(ko[n])):
            return n, "굵은 강조"
    for n in sorted(ko):
        if occurs(term, ko[n]):
            return n, "첫 등장"
    return None, "없음"


def main() -> int:
    ko, en = chapters(KO), chapters(EN)
    terms, noglossed = load_dictionary(ko)

    rows, problems = [], []
    for term in sorted(terms):
        want_gloss = (term not in NO_GLOSS and term not in noglossed
                      and not re.fullmatch(r"[A-Za-z0-9 ._'\-]+", term)
                      and bool(terms[term]))
        n, why = defining_chapter(term, ko)
        if n is None:
            continue
        # 병기: 어디서 처음 병기되었나
        gloss_at = None
        for m in sorted(ko):
            if re.search(re.escape(term) + r"\*?\s*\([a-zA-Z]", ko[m]):
                gloss_at = m
                break
        idx_at = [m for m in sorted(ko) if f'#idx("{term}")' in ko[m]]
        idx_here = n in idx_at

        state = []
        if want_gloss and gloss_at is None:
            state.append("gloss-missing")
        elif want_gloss and gloss_at > n:
            state.append("gloss-late")
        if not idx_at:
            state.append("index-missing")
        elif not idx_here:
            state.append("index-elsewhere")

        rows.append((term, terms[term], n, why, gloss_at, idx_at, state))
        for s in state:
            if s != "index-elsewhere":       # 다른 장에 있는 것은 참고만
                problems.append((s, term, n, why))

    # ── 문서 ────────────────────────────────────────────────
    tally = {}
    for s, *_ in problems:
        tally[s] = tally.get(s, 0) + 1
    md = ["# 용어와 색인", "",
          "개념을 *정의하는 자리*에는 영어 병기와 색인 표시가 함께 있어야 한다",
          "(RFC-0019). 이 표는 `scripts/check-terms.py` 가 원고에서 생성하며,",
          "한국어–영어 용어 대조표를 겸한다.", "",
          f"- 용어 **{len(rows)}개**",
          f"- 어긋난 곳 **{len(problems)}건** " +
          ("(" + ", ".join(f"{k} {v}" for k, v in sorted(tally.items())) + ")" if tally else "— 없다"),
          "", "## 용어 대조표", "",
          "| 용어 | 원어 | 정의 장 | 판정 근거 | 병기 | 색인 |", "|---|---|---|---|---|---|"]
    for term, e, n, why, g, ia, st in rows:
        md.append(f"| {term} | {e or '—'} | {n} | {why} | "
                  f"{g if g else '—'} | {', '.join(str(x) for x in ia) or '—'} |")
    md += ["", "---", "", "생성: `python3 scripts/check-terms.py`", ""]
    (ROOT / "docs" / "TERMS.md").write_text("\n".join(md), encoding="utf-8")

    quiet = "--quiet" in sys.argv
    if problems and not quiet:
        for s, term, n, why in sorted(problems)[:60]:
            print(f"  ⚠️  {s:<15} {term}  ({n}장 — {why})")
    print(f"check-terms: 용어 {len(rows)}개 · 어긋난 곳 {len(problems)}건"
          + (f" ({', '.join(f'{k} {v}' for k, v in sorted(tally.items()))})" if tally else ""))
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
