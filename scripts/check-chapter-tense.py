#!/usr/bin/env python3
r"""장 참조 곁의 「거리·시제」가 실제 순서와 맞는지 본다 (RFC-0052 의 뒤처리).

`check-chapter-refs` 는 장 *이름*이 맞는지만 본다. 이름 곁의 말은 보지 않는다 ---
앞 장을 「에서 볼」로 가리키거나, 두 장 뒤를 「다음 장」이라 적어도 통과한다.
RFC-0052 의 정독에서 그런 자리를 손으로 다섯 곳 찾았다.

★ 「다음 장」이 문단 어딘가에 있고 참조도 어딘가에 있는 꼴은 보지 않는다. 그렇게
  재면 거짓 양성이 스물넷에 진짜가 하나였다(`check-part-position.py` 의 기록).
  여기서는 말과 참조가 *바로 붙은* 꼴만 본다.

  · 시제 --- `#chref("x")에서 본` 은 x 가 앞 장일 때, `#chref("x")에서 볼` 은 뒷장일 때.
  · 거리 --- `다음 장(#chref("x"))`·`다음 장인 #chref("x")` 는 x 가 바로 다음 장일 때,
    `앞 장(#chref("x"))` 은 바로 앞 장일 때.

부록·앞글·뒷글은 모든 장의 *뒤*로 친다(앞글은 앞으로). 부 소개글은 그 부의 첫 장
바로 앞이다. 사람이 읽고 맞다고 본 줄은 `docs/chapter-tense-allow.tsv` 에 올린다.

사용법: python3 scripts/check-chapter-tense.py [--self-test]
종료 상태: 어긋난 곳이 있으면 1
"""
import pathlib
import re
import sys

from chapters import NO, is_draft

ROOT = pathlib.Path(__file__).resolve().parent.parent
ALLOW = ROOT / "docs" / "chapter-tense-allow.tsv"
SKIP = {"lib.typ", "registry.typ", "style.typ", "main.typ", "style-specimen.typ"}

REF = r'#chref\("([a-z0-9-]+)"[^()]*\)(?:/\*\*/)?'
# 조사·따옴표·강조 기호 몇 글자까지는 「바로 붙었다」고 본다.
PAST_KO = re.compile(REF + r"(?:에서|에)\s*\*?(?:이미\s*)?(본|보았|봤|배운|배웠|다룬|다뤘|다루었|만난|만났|"
                     r"말한|말했|쓴|썼|만든|만들었|익힌|살핀|살폈|읽은|읽었|세운|세웠|미뤄 둔|약속한)(?!다)")
# ★ 「본다」·「다룬다」는 지금 시제다 --- 뒷장을 가리켜도 맞는 말이라 잡지 않는다.
FUTURE_KO = re.compile(REF + r"(?:에서|에)\s*\*?(볼|보게|배울|다룰|다루게|만날|만나게|쓸|쓰게|만들|살필|읽을|세울|갚|돌아온다|이어진다)(?![가-힣])")
NEXT_KO = re.compile(r"(?:바로\s*)?다음 장(?:\(|인\s*|,\s*)" + REF)
PREV_KO = re.compile(r"(?:바로\s*)?(?:앞|지난|직전) 장(?:\(|인\s*|,\s*)" + REF)

# ★ 영어의 `is covered in` 은 뒷장에도 쓰는 말이다. 지난 일임이 분명한 꼴만 잡는다.
PAST_EN = re.compile(r"(?:(?:(?:\b(?:we|you|I)\s+(?:\w+\s+)?(?:saw|met|learned|learnt|covered|built|read|used))"
                     r"|(?:\b(?:as|already|was|were|been|just)\s+(?:seen|met|learned|learnt|covered|built|introduced|described|discussed)))"
                     r"(?:\s+\w+){0,3}?\s+(?:back\s+)?in\s+)" + REF
                     + r"|\bAs\s+" + REF + r"\s+(?:showed|taught|explained)\b", re.I)
FUTURE_EN = re.compile(r"(?:(?:will|shall)\s+(?:see|meet|learn|cover|build|read|use|return to)(?:\s+\w+){0,3}\s+in\s+)" + REF
                       + r"|" + REF + r"\s+(?:will|takes up|returns to)\b"
                       + r"|\bwill be (?:seen|met|covered|taken up) in\s+" + REF, re.I)
NEXT_EN = re.compile(r"\bthe next chapter(?:\s*\(|,\s*)" + REF, re.I)
PREV_EN = re.compile(r"\bthe (?:previous|preceding|last) chapter(?:\s*\(|,\s*)" + REF, re.I)


def position(path, part_first):
    """이 파일이 읽기 순서의 어디에 있는가 (장 번호 단위, 장 사이는 .5)."""
    rel = path.relative_to(ROOT).parts
    stem = path.stem
    if rel[1] == "chapters":
        m = re.fullmatch(r"ch(\d+)", stem)
        return float(m.group(1)) if m else None
    if rel[1] == "parts":
        return part_first.get(stem, 0) - 0.5 if stem in part_first else None
    if rel[1] == "front":
        return 0.0
    if rel[1] in ("appendix", "back"):
        return len(NO) + 1.0
    return None


def part_first_chapters():
    reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(r'intro:\s*"([^"]+)"[\s\S]*?chapters:\s*\(([^)]*)\)', reg):
        ids = re.findall(r'"([^"]+)"', m.group(2))
        if ids:
            out[m.group(1)] = NO[ids[0]]
    return out


def judge(text, here, lang):
    """(줄 번호, 까닭, 원문 조각) 의 목록."""
    out = []
    pats = ((PAST_KO, "past"), (FUTURE_KO, "future"), (NEXT_KO, "next"), (PREV_KO, "prev")) if lang == "ko" else \
           ((PAST_EN, "past"), (FUTURE_EN, "future"), (NEXT_EN, "next"), (PREV_EN, "prev"))
    for pat, kind in pats:
        for m in pat.finditer(text):
            cid = next(g for g in m.groups() if g and g in NO) if any(g in NO for g in m.groups() if g) else None
            if cid is None:
                continue
            there = NO[cid]
            ok = {"past": there <= here, "future": there >= here,
                  "next": there == int(here) + 1,
                  "prev": there == (int(here) - 1 if here == int(here) else int(here))}[kind]
            if ok:
                continue
            why = {"past": f"{there}장은 뒤에 있는데 지난 일로 말한다",
                   "future": f"{there}장은 앞에 있는데 올 일로 말한다",
                   "next": f"{there}장은 바로 다음 장이 아니다",
                   "prev": f"{there}장은 바로 앞 장이 아니다"}[kind]
            line = text.count("\n", 0, m.start()) + 1
            out.append((line, why, " ".join(m.group(0).split())))
    return sorted(out)


def allowed():
    if not ALLOW.exists():
        return set()
    rows = set()
    for raw in ALLOW.read_text(encoding="utf-8").splitlines():
        if raw.startswith("#") or not raw.strip():
            continue
        cols = raw.split("\t")
        if len(cols) >= 2:
            rows.add((cols[0], cols[1]))
    return rows


def self_test():
    """RFC-0052 가 손으로 찾은 꼴 --- 검사기가 없던 때에는 전부 통과했다."""
    a, b, c = sorted(NO, key=NO.get)[:3]          # 1·2·3장
    cases = [
        (f'#chref("{a}")에서 볼 것이다', 2.0, "ko", 1),           # 앞 장을 「볼」
        (f'#chref("{c}")에서 본 대로다', 2.0, "ko", 1),           # 뒷장을 「본」
        (f'바로 다음 장(#chref("{c}"))이다', 1.0, "ko", 1),        # 두 장 뒤를 「다음 장」
        (f'#chref("{a}")에서 본 대로다', 2.0, "ko", 0),
        (f'다음 장(#chref("{b}"))에서', 1.0, "ko", 0),
        (f'as we saw in #chref("{c}")', 2.0, "en", 1),
        (f'the next chapter (#chref("{b}"))', 1.0, "en", 0),
        (f'we will see it in #chref("{a}")', 2.0, "en", 1),
    ]
    bad = 0
    for text, here, lang, want in cases:
        got = len(judge(text, here, lang))
        if got != want:
            bad += 1
            print(f"  self-test: {text!r} --- {got}건, 기대 {want}건")
    print("check-chapter-tense: self-test " + ("FAILED" if bad else "ok"))
    return 1 if bad else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    pf = part_first_chapters()
    allow = allowed()
    used, bad, seen = set(), 0, 0
    for ed, lang in (("book", "ko"), ("book-en", "en")):
        for path in sorted((ROOT / ed).rglob("*.typ")):
            if is_draft(path) or path.name in SKIP:
                continue
            here = position(path, pf)
            if here is None:
                continue
            seen += 1
            rel = str(path.relative_to(ROOT))
            for line, why, frag in judge(path.read_text(encoding="utf-8"), here, lang):
                if (rel, frag) in allow:
                    used.add((rel, frag))
                    continue
                bad += 1
                print(f"  ⚠️  {rel}:{line}: {frag} --- {why}")
    for rel, frag in sorted(allow - used):
        bad += 1
        print(f"  ⚠️  허용 목록의 낡은 줄: {rel}\t{frag}")
    if bad:
        print(f"check-chapter-tense: 장 참조 곁의 말이 순서와 어긋난다 {bad}건")
        return 1
    print(f"check-chapter-tense: 파일 {seen}개 --- 장 참조 곁의 시제·거리가 순서와 맞는다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
