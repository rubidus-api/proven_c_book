#!/usr/bin/env python3
"""원고가 인용한 표준 조항 번호가 *실재하는 조항*인지 본다 (사실검사 2026-08-29).

두 번의 검사에서 이 축이 오류를 잡았다.

  · 40장이 포인터 산술의 범위를 「§6.5.6」이라 적었다 --- 그것은 *곱셈 연산자*다.
  · 40장이 앨리어싱과 유효 타입을 「§6.5」로만 인용했다 --- C23 §6.5 에는 문단이 없다.
  · 5장·21장이 C17 시절 번호(§6.5.3.4·§6.4.4.4)를 그대로 옮겨 적었다.

사람의 눈으로는 「그럴듯한 번호」를 걸러 내지 못한다. 그래서 기계가 목차와 맞댄다.

★ 표준 초안은 저작권 때문에 이 저장소에 넣지 않는다(부록 D 참고). 그래서 이
  검사는 *목차 파일이 있을 때만* 돈다 --- 없으면 건너뛰고 0 으로 끝난다.
  자리는 `PROVEN_C_BOOK_REFS` 로 지정하거나, 기본값인 저장소 옆
  `proven_c_book_private/refs/toc.json` 을 쓴다.

사용법: python3 scripts/check-clauses.py
종료 상태: 실재하지 않는 조항을 인용했으면 1
"""
import json
import os
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SKIP = {"lib.typ", "registry.typ", "style.typ", "main.typ"}

# 표준이 아닌 §인용 --- 다른 문헌의 절 번호는 이 검사의 대상이 아니다.
NOT_THE_C_STANDARD = {
    "4.1",      # 2장: 커누스 TAOCP 2권의 절
}


def refs_dir():
    env = os.environ.get("PROVEN_C_BOOK_REFS")
    # 환경변수를 지정하면 *그 자리만* 본다 --- 없으면 건너뛴다는 뜻이 되게.
    cands = [pathlib.Path(env)] if env else [
        ROOT.parent / "proven_c_book_private" / "refs"]
    for c in cands:
        if (c / "toc.json").exists():
            return c
    return None


def known_clauses(refs):
    """목차에 실린 조항 + 본문에 홀로 선 조항 번호.

    ★ 목차는 *절 제목이 있는* 조항만 싣는다. 3절(용어와 정의)의 항목들은
      제목 대신 용어가 오므로 목차에 없다 --- §3.24 「non-value representation」이
      그렇다. 그래서 본문에서도 거둔다.
    """
    known = set(json.loads((refs / "toc.json").read_text(encoding="utf-8")))
    body = refs / "n3220.txt"
    if body.exists():
        known |= set(re.findall(r"^([0-9]+(?:\.[0-9]+)+)$",
                                body.read_text(encoding="utf-8", errors="replace"),
                                re.M))
    return known


def paragraph_counts(refs):
    """조항마다 *문단 번호가 어디까지 있는지*를 센다.

    ★ 존재 검사만으로는 약하다 --- 「§6.5.6」은 실재하는 조항이라(곱셈 연산자)
      포인터 산술을 그렇게 인용해도 걸리지 않는다. 그러나 *문단*까지 적은
      인용은 다르다: C23 §6.5 에는 문단이 하나도 없어서 「§6.5 p6」은 그 자리에서
      드러난다. 원고는 문단을 적는 쪽이 규율이므로 이 검사가 실질적으로 문다.
    """
    body = refs / "n3220.txt"
    if not body.exists():
        return None
    heading = re.compile(r"^([0-9]+(?:\.[0-9]+)+)$")
    para = re.compile(r"^([0-9]{1,3})$")
    top = {}
    cur = None
    for line in body.read_text(encoding="utf-8", errors="replace").split("\n"):
        h = heading.match(line)
        if h:
            cur = h.group(1)
            top.setdefault(cur, 0)
            continue
        if cur:
            m = para.match(line)
            if m:
                n = int(m.group(1))
                # 문단 번호는 조항 안에서 올라가기만 한다. 쪽 번호 같은 잡음은
                # 그 규칙으로 걸러진다.
                if n == top[cur] + 1:
                    top[cur] = n
    return top


def main():
    refs = refs_dir()
    if refs is None:
        print("check-clauses: 표준 목차가 없어 건너뛴다 "
              "(PROVEN_C_BOOK_REFS 로 자리를 알려 주면 돈다)")
        return 0
    toc = known_clauses(refs)
    paras = paragraph_counts(refs)

    bad = 0
    seen = 0
    for ed in ("book", "book-en"):
        for path in sorted((ROOT / ed).rglob("*.typ")):
            if path.name in SKIP:
                continue
            text = path.read_text(encoding="utf-8")
            for m in re.finditer(
                    r"§([0-9][0-9.]*[0-9])\s*(?:p\s*([0-9]{1,3})|"
                    r"문단\s*([0-9]{1,3})|para(?:graph)?\s*([0-9]{1,3}))?", text):
                clause = m.group(1)
                pnum = next((g for g in m.groups()[1:] if g), None)
                if clause in NOT_THE_C_STANDARD:
                    continue
                # ★ 옛 판의 번호를 *일부러* 적는 자리가 있다 --- 61장이 「C17
                #   에서는 §6.10.3.5 였는데」로 재번호를 설명한다. 가까이에서
                #   C23 아닌 판을 부르고 있으면 이 검사의 대상이 아니다.
                near = text[max(0, m.start() - 60):m.end() + 60]
                if re.search(r"C(89|90|95|99|11|17)\b", near):
                    continue
                seen += 1
                line = text[:m.start()].count("\n") + 1
                if clause not in toc:
                    bad += 1
                    print(f"  ⚠️  없는 조항 §{clause}  "
                          f"{path.relative_to(ROOT)}:{line}")
                elif pnum and paras is not None:
                    have = paras.get(clause, 0)
                    # ★ have == 0 은 「문단이 없는 상위 조항」이라는 뜻이고,
                    #   그런 자리에 문단을 붙여 인용한 것이 바로 잡을 결함이다
                    #   (40장이 유효 타입을 「§6.5 p6」으로 적고 있었다).
                    if int(pnum) > have:
                        bad += 1
                        print(f"  ⚠️  §{clause} 에는 문단이 {have} 까지인데 "
                              f"p{pnum} 을 인용했다  "
                              f"{path.relative_to(ROOT)}:{line}")
    if bad:
        print(f"check-clauses: 실재하지 않는 조항 인용 {bad}건 (검사 {seen}건)")
        return 1
    print(f"check-clauses: 인용한 조항 {seen}건이 모두 C23 목차에 있다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
