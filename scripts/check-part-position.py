#!/usr/bin/env python3
r"""「이 부의 마지막 장이다」가 정말 마지막인지 본다 (저자 지적 2026-08-31).

83장이 「이 부의 마지막 장이다」라고 적고 있었다. 한때는 맞았다 --- 제11부가
83장에서 끝나던 시절의 문장이고, 그 뒤로 여섯 장(84\~89)이 붙었다. 같은 결함이
18·34·86·99장에도 있었다.

장을 끼워 넣으면 *번호*는 도구가 고쳐 주지만, 「마지막」·「닫는다」 같은 *자리에
대한 말*은 아무도 고쳐 주지 않는다. 그래서 기계에 맡긴다.

  · 「이 부의 마지막 장이다」 꼴은 그 장이 정말 부의 끝일 때만 쓴다.
  · 「제N부를 닫으며」 같은 절 제목도 마찬가지다.

★ 뒷장을 가리키는 말(「다음 장은 이 부의 마지막이다」·「이 부를 닫는 장이 하나
  남았다」)은 대상이 아니다 --- 앞에 「다음」·「남은」 같은 말이 붙는다.

★ 「다음 장은 …이다」가 정말 다음 장인지도 재어 보았다. 이번 검토에서 그 부류로
  다섯 곳(19·29·33·77·80·99장)을 찾았으므로 값어치는 분명하다. 그러나 기계로는
  거짓 양성이 스물넷에 진짜가 하나였다 --- 「다음 장」 곁의 참조가 대개 *다른 것*을
  가리키기 때문이다(「§에서 미뤄 온 빚이 다음 장에서 청산된다」). 무시하게 되는
  검사는 없느니만 못하므로 싣지 않았다. 그 부류는 사람이 훑는다.

사용법: python3 scripts/check-part-position.py
종료 상태: 어긋난 곳이 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# 「자기가 마지막이다」라고 말하는 꼴만 잡는다.
CLAIM_KO = re.compile(r"(?<![다은는])\s*(이 부의 마지막 장이다|제?\d+부의 마지막 장이다"
                      r"|== 제?\d+부를 닫으며|== 이 부를 닫으며)")
CLAIM_EN = re.compile(r"(The last chapter of (?:this part|[Pp]art [IVX0-9]+)\."
                      r"|^== Closing Part [IVX]+)", re.M)


def parts():
    reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
    out, n = [], 1
    for m in re.finditer(r'ko:\s*"([^"]+)"[\s\S]*?chapters:\s*\(([^)]*)\)', reg):
        ids = re.findall(r'"([^"]+)"', m.group(2))
        out.append((m.group(1).split("—")[0].strip(), n, n + len(ids) - 1))
        n += len(ids)
    return out



def main():
    ranges = parts()
    bad = 0

    for ed, pat in (("book", CLAIM_KO), ("book-en", CLAIM_EN)):
        for i in range(1, 104):
            path = ROOT / ed / "chapters" / f"ch{i:02d}.typ"
            if not path.exists():
                continue
            name, first, last = next(p for p in ranges if p[1] <= i <= p[2])
            text = path.read_text(encoding="utf-8")
            for m in pat.finditer(text):
                # ★ 「…」 안에 든 것은 *예시*이지 주장이 아니다. 102장이 이 검사가
                #   무엇을 보는지 설명하며 그 문장을 그대로 인용한다.
                if m.start() > 0 and text[m.start(1) - 1] in "「\"":
                    continue
                if i != last:
                    bad += 1
                    print(f"  \u26a0\ufe0f  {path.relative_to(ROOT)}: "
                          f"「{m.group(0).strip()}」 --- {name} 의 마지막은 {last}장이다")
    if bad:
        print(f"check-part-position: 자리에 대한 서술이 낡았다 {bad}건")
        return 1
    print("check-part-position: 「부의 마지막」이라 적은 장이 모두 실제로 마지막이다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
