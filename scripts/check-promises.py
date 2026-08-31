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

★ 2026-08-31 --- 이 도구는 만든 뒤로 한 번도 *판정된 적이 없었다*. project-check 에
걸려 있지 않아 아무도 결과를 읽지 않았고, 그 사이 두 가지가 겹쳐 있었다.
  ① 실행마다 답이 달랐다(VOCAB 이 집합이라 낱말 순서가 뒤집혔다 --- 아래 keywords).
  ② 인용 뒤 40자를 통째로 집어, 바로 뒤에 오는 *다른 장* 이야기의 낱말로 판정했다.
둘을 고치자 105건이 21건으로 줄었고, 그중 여섯 자리가 진짜 결함이었다 ---
`#chref` 가 가리키는 장이 실제로 그 이야기를 하지 않는 자리다. 하나는 아예
책에 없는 문장을 인용하고 있었다("goto는 절제해서", 31장에는 goto 가 없다).

남은 14건은 사람이 하나씩 읽어 *약속은 옳고 낱말만 안 맞는 것*으로 판정했다
(2026-08-31). 그래서 이 도구는 아직 project-check 에 걸지 않는다 --- 0 을 만들 수
없는 검사를 게이트로 걸면 아무도 안 보게 되고, 그것이 애초에 이 도구가 잠들어
있던 이유다. 정밀도를 더 올려 0 이 되는 날 게이트로 승격한다.

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
# 구절의 경계 --- 여기서 끊으면 낱말이 인용을 넘어가지 않는다
CLAUSE = re.compile(r"[,，、;)\]\[(·]|\d+\s*장|---|—")
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
    # 짧은 낱말이 긴 낱말에 포함되는 경우 긴 쪽만 남긴다.
    # ★ 2026-08-31 --- 여기서 길이만으로 정렬했더니 *실행마다 답이 달라졌다*.
    #   VOCAB 이 집합이라 반복 순서가 프로세스마다 뒤집히고, 길이가 같은 낱말끼리
    #   앞뒤가 바뀌면 아래 `kws[:2]` 가 다른 두 개를 골라 깨진 약속 수가 101~105 를
    #   오갔다. 흔들리는 수는 기준선이 못 되고, 진짜 회귀를 묻는다.
    #   그래서 같은 길이일 때는 낱말 자체로 순서를 못박는다.
    found.sort(key=lambda v: (-len(v), v))
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
            # ★ 낱말은 *그 인용에 딸린 것*이라야 한다. 40자를 그냥 집으면 바로
            #   뒤에 오는 다른 장의 구절까지 삼켜, 「43장」의 약속을 74장 이야기에
            #   나온 `strcoll` 로 판정하는 일이 생겼다. 그래서 쉼표·괄호·다음
            #   장 인용에서 끊는다 --- 앞쪽은 마지막 토막, 뒤쪽은 첫 토막이다.
            before = CLAUSE.split(before)[-1]
            after = CLAUSE.split(after)[0]
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
