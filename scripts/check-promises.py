#!/usr/bin/env python3
"""장 사이의 「약속」이 실제로 지켜졌는지 본다 (RFC-0020, 저자 지시 2026-08-08).

`check-chapter-refs.py` 는 두 판이 *같은 장*을 가리키는지만 본다. 그러나 진짜 물어야 할 것은
다른 것이다 --- **「43장에서 다룬다」고 했으면 43장에 정말 그 이야기가 있는가.**

이 도구는 본문에서 「N장…키워드」 꼴의 약속을 뽑아, 그 키워드가 N장에 실제로 있는지
대조한다. 없으면 *깨진 약속*이다 --- 장을 옮기거나 내용을 고치는 동안 생기는 가장
흔한 결함이고, 사람이 읽어서는 좀처럼 못 찾는다.

판정은 보수적으로 한다(거짓 양성을 줄이려고).
  · 키워드는 약속 문장에서 뽑은 *명사구*만 쓴다.
  · 대상 장에 그 명사구가 없더라도, *핵심 낱말 하나*라도 있으면 통과시킨다.
  · 조사·용언으로 끝나는 조각은 애초에 키워드로 쓰지 않는다.

사용법: python3 scripts/check-promises.py [--all]
종료 상태: 깨진 약속이 있으면 1
"""
import pathlib

import chapters as chreg
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO = ROOT / "book" / "chapters"

# 「… N장에서 다룬다 / N장의 X / N장에서 본다」 꼴을 잡는다
PROMISE = re.compile(
    r"([^.。\n]{0,50}?)(\d+)\s*장(?:의|에서|에|은|이|과|와|,)?\s*([^.。\n]{0,40})")
STOP = set("그 이 저 것 수 때 더 또 및 첫 한 그것 여기 거기 지금 이제 다시 정식 "
           "이야기 자리 대로 그대로 부분 내용 경우 때문 위해 통해 대한 관한 "
           "본다 다룬다 있다 없다 한다 된다 같다 보자 보면 이다 이고 하는 "
           "장에서 장의 장을 장이 장과 장도".split())
NOUN = re.compile(r"[가-힣A-Za-z_][가-힣A-Za-z0-9_]*")
BAD_TAIL = ("고", "서", "며", "면", "지", "게", "야", "라", "다", "은", "는", "이",
            "가", "을", "를", "에", "로", "와", "과", "도", "만", "의", "요")


def chapters():
    out = {}
    for f in sorted(KO.glob("ch*.typ")):
        m = re.match(r"ch(\d+)\.typ$", f.name)
        if m:
            out[int(m.group(1))] = chreg.expand(f.read_text(encoding="utf-8"),
                                                   chreg.lang_of(f))
    return out


def concept_vocabulary():
    """대조에 쓸 낱말은 *이 책이 개념어로 인정한 것*만 쓴다.

    아무 낱말이나 대조하면 용언 조각까지 물어 거짓 양성이 쏟아진다(첫 판이 그랬다).
    용어 사전(docs/terms.tsv)과 색인 표제어를 어휘로 삼으면 정밀도가 올라간다.
    """
    vocab = set()
    tsv = ROOT / "docs" / "terms.tsv"
    if tsv.exists():
        for line in tsv.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.lstrip().startswith("#"):
                k = line.split("\t")[0].strip()
                if k and not k.startswith("!"):
                    vocab.add(k)
    for f in KO.glob("ch*.typ"):
        vocab |= set(re.findall(r'#idx\("([^"]+)"\)', f.read_text(encoding="utf-8")))
    # 코드 서체로 쓰이는 이름(함수·헤더)도 대조에 쓸 만하다
    for f in KO.glob("ch*.typ"):
        vocab |= {w for w in re.findall(r"`([A-Za-z_][A-Za-z0-9_]{2,24})`",
                                        f.read_text(encoding="utf-8"))}
    return {v for v in vocab if len(v) >= 2}


VOCAB = concept_vocabulary()


def keywords(text):
    """약속 문장에서 *개념어로 인정된 것*만 남긴다."""
    found = [v for v in VOCAB if v in text]
    # 짧은 낱말이 긴 낱말에 포함되는 경우 긴 쪽만 남긴다
    found.sort(key=len, reverse=True)
    out = []
    for v in found:
        if not any(v in o for o in out):
            out.append(v)
    return out


def main() -> int:
    ch = chapters()
    broken, checked = [], 0
    for n in sorted(ch):
        body = ch[n]
        for m in PROMISE.finditer(body):
            before, target, after = m.group(1), int(m.group(2)), m.group(3)
            if target == n or target not in ch:
                continue
            kws = keywords(after) or keywords(before)
            kws = kws[:2]
            if not kws:
                continue          # 개념어가 없는 약속은 대조할 것이 없다
            checked += 1
            hay = ch[target]
            # 하나라도 대상 장에 있으면 지켜진 약속으로 본다(보수적)
            if any(k in hay for k in kws):
                continue
            broken.append((n, target, kws, m.group(0).strip()[:72]))

    show_all = "--all" in sys.argv
    for n, t, kws, quote in (broken if show_all else broken[:40]):
        print(f"  ⚠️  {n:>2}장 → {t:>2}장  [{'/'.join(kws)}]  …{quote}…")
    print(f"check-promises: 약속 {checked}건 검사 · 깨진 약속 {len(broken)}건")
    return 1 if broken else 0


if __name__ == "__main__":
    raise SystemExit(main())
