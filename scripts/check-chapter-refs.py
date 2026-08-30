#!/usr/bin/env python3
"""장 참조가 *이름*으로 되어 있는지, 두 판이 같은 장을 가리키는지 본다 (RFC-0028 §3.4).

옛 `check-xrefs.py` 는 두 판의 *장 번호* 집합을 맞대어 봤다. 번호가 원고에서
사라진 지금은 물을 것이 둘로 바뀐다.

  ① 원고에 *맨 숫자 장 참조*가 남아 있지 않은가
     (`32장`, `chapter 32` --- 이제는 `#chref("loops")` 로 적는다)
  ② 같은 장의 두 판이 *같은 장들*을 가리키는가
     번호가 아니라 id 를 맞대므로, 한쪽만 고친 참조가 바로 드러난다.

②는 옛 검사보다 세다. 예전에는 두 판이 *나란히 틀리면* 통과했지만(실제로
영어판 94장이 「82 (memory layout)」이라고 한 장 어긋나 있었다), id 는 뜻이
있어서 그런 어긋남이 눈에 띈다.

사용법: python3 scripts/check-chapter-refs.py
종료 상태: 어긋난 곳이 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO, EN = ROOT / "book", ROOT / "book-en"
SKIP = {"lib.typ", "registry.typ", "style.typ", "main.typ"}

FENCE = re.compile(r"```[\s\S]*?```")
INLINE = re.compile(r"`[^`\n]*`")
# ★ 예전에는 따옴표 안을 통째로 지웠다(`"[^"\n]*"`). 그런데 한국어 본문은
#   인용에 그 따옴표를 쓴다 --- 「그때의 "선언"과, 23장에서 배운 "변수」의 가운데가
#   문자열로 오인되어 맨 숫자 참조 넷이 여러 판 동안 숨어 있었다. 지금은
#   *함수 호출의 인자*만 지운다. 경로 인자(`#demo("examples/ch57/…")`)가 표적이다.
CALL = re.compile(r'#[A-Za-z][\w-]*\([^()]*\)')
# ★ 예전에는 뒤에 한글이 오면 그냥 넘겼다(`(?![가-힣])`). 그런데 한국어는 조사가
#   바로 붙는다 --- 「51장에서 배운」이 그래서 여러 판 동안 숨었다. 지금은 조사를
#   허용하고, 「장」으로 시작하는 *다른 낱말*(장면·장치…)만 뺀다.
BARE_KO = re.compile(r"(?<![\d.])\d{1,3}\s*장(?![면치벽독][가-힣]|가|기)")
BARE_EN = re.compile(r"\b(?:[Cc]hapters?\s+\d{1,3}|ch\.\s*\d{1,3})\b")
REF = re.compile(r'#(?:chref|chrefs|chrange)\(([^()]*)\)')


def prose(path):
    t = path.read_text(encoding="utf-8")
    return CALL.sub(" ", INLINE.sub(" ", FENCE.sub(" ", t)))


def ids_in(path):
    return set(re.findall(r'"([^"]+)"', " ".join(REF.findall(path.read_text(encoding="utf-8")))))


def main():
    bad = 0

    # ① 맨 숫자 참조가 남았는가
    for base, pat in ((KO, BARE_KO), (EN, BARE_EN)):
        for path in sorted(base.rglob("*.typ")):
            if path.name in SKIP:
                continue
            for m in pat.finditer(prose(path)):
                bad += 1
                print(f"  ⚠️  맨 숫자 참조  {path.relative_to(ROOT)}: {m.group(0).strip()}")

    # ★ *표·그림* 참조 뒤에는 「이/가」·「은/는」·「을/를」을 붙이지 않는다.
    #   그 참조는 숫자로 끝나서(「표 40.7」) 번호가 바뀌면 받침이 바뀌고, 조사가
    #   틀리게 된다 --- 실제로 「표 40.7가」가 인쇄됐다. 「에」·「에서」처럼 변하지
    #   않는 조사를 쓰거나 문장을 바꾼다.
    #   장 참조는 해당 없다. 언제나 「…장」으로 끝나므로 조사가 변하지 않는다.
    VAR_JOSA = re.compile(r'#(?:tblref|figref)\([^()]*\)(?:/\*\*/)?(이|가|은|는|을|를)(?![가-힣])')
    for path in sorted(KO.rglob("*.typ")):
        if path.name in SKIP:
            continue
        for m in VAR_JOSA.finditer(path.read_text(encoding="utf-8")):
            bad += 1
            print(f"  ⚠️  참조 뒤 변이 조사 「{m.group(1)}」  {path.relative_to(ROOT)}: "
                  f"{m.group(0)[-24:]}")

    # ★ 장·부의 *구간*은 en dash 로 적는다 --- `#chrange` 가 「16–26장」으로 펴고,
    #   본문의 「제3–5부」도 같은 기호다. 물결표(`~`)는 *값*의 범위에만 쓴다
    #   (`0x20~0x7E`, `A`~`Z`, 1~4바이트). 두 기호가 섞이면 읽는 사람이 무엇이
    #   장이고 무엇이 값인지 기호로 가를 수 없다. (2026-08-31 저자 지시)
    STRUCT_TILDE = re.compile(r"제\s*\d+\s*\\?~\s*\d+\s*[부장]|[Pp]arts?\s+[IVX]+\s*\\?~")
    for base in (KO, EN):
        for path in sorted(base.rglob("*.typ")):
            if path.name in SKIP:
                continue
            for m in STRUCT_TILDE.finditer(path.read_text(encoding="utf-8")):
                bad += 1
                print(f"  \u26a0\ufe0f  구간에 물결표  {path.relative_to(ROOT)}: "
                      f"{m.group(0).strip()} --- en dash 로 적는다")

    # ★ 목록(`#chrefs`)의 장 번호가 오름차순인가.
    #   내림차순이면 두 가지가 걸린다 --- 읽는 사람이 순서를 근거로 짚지 못하고,
    #   무엇보다 「51·48장」처럼 *범위로 오해*되기 쉽다. 범위를 뜻했다면 도구가
    #   따로 있다: `#chrange(a, b)` 가 「48–51장」으로 편다.
    #   (2026-08-31 저자 지적: 「1·15장」이 1장과 15장으로 읽힌다.)
    import chapters as _chreg
    order = {cid: i for i, cid in enumerate(_chreg.ids(), 1)} if hasattr(_chreg, "ids") else None
    if order is None:
        reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
        cids = []
        for m in re.finditer(r"chapters:\s*\(([^)]*)\)", reg):
            cids += re.findall(r'"([^"]+)"', m.group(1))
        order = {cid: i for i, cid in enumerate(cids, 1)}
    for base in (KO, EN):
        for path in sorted(base.rglob("*.typ")):
            if path.name in SKIP:
                continue
            for m in re.finditer(r"#chrefs\(([^()]*)\)", path.read_text(encoding="utf-8")):
                args = re.findall(r'"([^"]+)"', m.group(1))
                ns = [order.get(a) for a in args]
                if len(ns) < 2 or any(n is None for n in ns):
                    continue
                if ns != sorted(ns):
                    bad += 1
                    print(f"  \u26a0\ufe0f  장 목록이 내림차순  {path.relative_to(ROOT)}: "
                          f"{', '.join(args)} = {ns}")
                # ★ 이어진 두 장은 목록이 아니라 *구간*이다. 「47·48장」은 틀리지
                #   않지만, 점을 「떨어진 장들」에만 남겨 두면 기호 자체가 뜻을
                #   나른다 --- 점이면 흩어져 있고 대시면 이어져 있다.
                #   (2026-08-31 저자 지적: 부록 H 에 점이 여전히 남아 있다.)
                elif len(ns) == 2 and ns[1] - ns[0] == 1:
                    bad += 1
                    print(f"  \u26a0\ufe0f  이어진 두 장은 구간이다  "
                          f"{path.relative_to(ROOT)}: {', '.join(args)} = "
                          f"{ns[0]}·{ns[1]} --- #chrange 를 쓴다")

    # ② 두 판이 같은 장을 가리키는가
    pairs = 0
    for ko in sorted((KO / "chapters").glob("ch*.typ")):
        en = EN / "chapters" / ko.name
        if not en.exists():
            continue
        pairs += 1
        a, b = ids_in(ko), ids_in(en)
        if a != b:
            bad += 1
            only_ko = ", ".join(sorted(a - b)) or "—"
            only_en = ", ".join(sorted(b - a)) or "—"
            print(f"  ⚠️  {ko.name}: 한국어판에만 [{only_ko}] · 영어판에만 [{only_en}]")

    if bad:
        print(f"check-chapter-refs: 어긋난 곳 {bad}건", file=sys.stderr)
        return 1
    print(f"check-chapter-refs: 장 참조가 모두 이름이고, 두 판 {pairs}장이 같은 장을 가리킨다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
