#!/usr/bin/env python3
"""원고의 *숫자 장 참조*를 이름 참조로 옮긴다 (RFC-0028 §3.3).

    32장            →  #chref("loops")
    33·34장         →  #chrefs("function-semantics", "assignment")
    6–9장           →  #chrange("integers-repr", "streams-origin")
    chapter 32      →  #chref("loops")
    Chapter 32      →  #chref("loops", cap: true)
    chapters 6–9    →  #chrange("integers-repr", "streams-origin")

번호는 `book/registry.typ` 에서 읽는다 --- 이 스크립트는 등록부를 *진실*로 삼고,
원고에 박힌 숫자를 그 이름으로 되돌린다.

건드리지 않는 것
  · 코드 블록(```)과 인라인 코드(`…`)
  · 큰따옴표 안 --- `#realcase("…")` 처럼 문자열 인자에 든 글
  · 장 번호가 될 수 없는 수(1~98 밖)

옮길 수 없는 것은 *그대로 두고 목록으로 알린다*. 사람이 본다.

쓰는 법
    migrate-chapter-refs.py --dry-run [파일…]   무엇이 바뀌는지만 센다
    migrate-chapter-refs.py [파일…]             실제로 옮긴다
    (파일을 주지 않으면 book/ 과 book-en/ 전체)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "book" / "registry.typ"


def load_ids():
    """등록부에서 (번호 → id) 를 읽는다."""
    text = REGISTRY.read_text(encoding="utf-8")
    ids = []
    for m in re.finditer(r"chapters:\s*\(([^)]*)\)", text):
        ids += re.findall(r'"([^"]+)"', m.group(1))
    return {i + 1: cid for i, cid in enumerate(ids)}


NO = load_ids()
MAXNO = max(NO)


def cid(n):
    return NO.get(int(n))


# 코드와 문자열을 가려 두는 자리표시자
MASKS = (
    re.compile(r"```[\s\S]*?```"),   # 코드 블록
    re.compile(r"`[^`\n]*`"),        # 인라인 코드
    re.compile(r'"[^"\n]*"'),        # 문자열 인자
)


def mask(text):
    store = []

    def put(m):
        store.append(m.group(0))
        return f"\x00{len(store) - 1}\x00"

    for pat in MASKS:
        text = pat.sub(put, text)
    return text, store


def unmask(text, store):
    """★ 자리표시자는 겹쳐 들어간다.

    `"… before `double`"` 은 인라인 코드가 먼저 가려지고 그 결과가 통째로
    문자열로 다시 가려진다. 그래서 한 번만 펴면 안쪽 자리표시자가 그대로 남아
    원고에 제어문자가 박힌다 --- 시범 이전에서 실제로 그렇게 됐다. 남지 않을
    때까지 편다.
    """
    pat = re.compile(r"\x00(\d+)\x00")
    for _ in range(10):
        new = pat.sub(lambda m: store[int(m.group(1))], text)
        if new == text:
            break
        text = new
    if "\x00" in text:
        raise SystemExit("자리표시자를 다 펴지 못했다 --- 이전을 중단한다")
    return text


# ★ 삽입한 호출 바로 뒤에 여는 괄호가 오면 Typst 가 그것을 *함수 호출*로 읽는다
#   (`#chref("x")(정렬)` → "expected function, found string"). 주석 하나를 끼워
#   호출을 끊는다 --- 렌더 결과에는 아무것도 남지 않는다.
# 여는 괄호만이 아니다 --- 세미콜론은 코드 모드의 문장 구분자라 *먹히고*(실측:
# `#chref("x"); 뒤` 에서 `;` 가 사라졌다), `.` 뒤에 글자가 오면 필드 접근으로 읽혀
# 빌드가 멈춘다. 넷 다 같은 방법으로 막는다.
CALLGUARD = re.compile(
    r'(#chref(?:s)?\([^()]*\)|#chrange\([^()]*\))(?=[\(\[\{;]|\.[A-Za-z0-9_가-힣])')


def convert(text, lang):
    """(바뀐 글, 바꾼 수, 못 바꾼 것들)"""
    text, store = mask(text)
    n = 0
    left = []

    def ok(*nums):
        return all(x.isdigit() and 1 <= int(x) <= MAXNO and cid(x) for x in nums)

    if lang == "ko":
        # 6–9장 · 6~9장  (범위가 먼저다 --- 안 그러면 낱개가 먼저 먹는다)
        def rng(m):
            nonlocal n
            if not ok(m.group(1), m.group(2)):
                left.append(m.group(0)); return m.group(0)
            n += 1
            return f'#chrange("{cid(m.group(1))}", "{cid(m.group(2))}")'
        text = re.sub(r"(\d{1,3})\s*[–~-]\s*(\d{1,3})장", rng, text)

        # 33·34장 · 27, 52장
        def many(m):
            nonlocal n
            nums = re.findall(r"\d{1,3}", m.group(1))
            if not ok(*nums):
                left.append(m.group(0)); return m.group(0)
            n += 1
            args = ", ".join(f'"{cid(x)}"' for x in nums)
            return f"#chrefs({args})"
        text = re.sub(r"((?:\d{1,3}\s*[·,]\s*)+\d{1,3})장", many, text)

        # 32장
        def one(m):
            nonlocal n
            if not ok(m.group(1)):
                left.append(m.group(0)); return m.group(0)
            n += 1
            return f'#chref("{cid(m.group(1))}")'
        text = re.sub(r"(?<![\d.])(\d{1,3})장", one, text)

    else:
        def en_rng(m):
            nonlocal n
            if not ok(m.group(2), m.group(3)):
                left.append(m.group(0)); return m.group(0)
            n += 1
            cap = ", cap: true" if m.group(1)[0].isupper() else ""
            return f'#chrange("{cid(m.group(2))}", "{cid(m.group(3))}"{cap})'
        text = re.sub(r"\b([Cc]hapters)\s+(\d{1,3})\s*[–-]\s*(\d{1,3})", en_rng, text)

        def en_many(m):
            nonlocal n
            nums = re.findall(r"\d{1,3}", m.group(2))
            if not ok(*nums):
                left.append(m.group(0)); return m.group(0)
            n += 1
            cap = ", cap: true" if m.group(1)[0].isupper() else ""
            args = ", ".join(f'"{cid(x)}"' for x in nums)
            return f"#chrefs({args}{cap})"
        text = re.sub(r"\b([Cc]hapters)\s+((?:\d{1,3}(?:,\s*|\s+and\s+))+\d{1,3})", en_many, text)

        def en_one(m):
            nonlocal n
            if not ok(m.group(2)):
                left.append(m.group(0)); return m.group(0)
            n += 1
            cap = ", cap: true" if m.group(1)[0].isupper() else ""
            return f'#chref("{cid(m.group(2))}"{cap})'
        text = re.sub(r"\b([Cc]hapter)\s+(\d{1,3})", en_one, text)

    text = CALLGUARD.sub(lambda m: m.group(1) + "/**/", text)
    return unmask(text, store), n, left


def targets(args):
    if args:
        return [pathlib.Path(a) for a in args]
    out = []
    for tree in ("book", "book-en"):
        out += sorted((ROOT / tree).rglob("*.typ"))
    return out


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry = "--dry-run" in sys.argv
    total = files = 0
    leftovers = []
    for path in targets(argv):
        if path.name == "registry.typ" or path.name == "lib.typ":
            continue
        lang = "en" if "book-en" in str(path) else "ko"
        src = path.read_text(encoding="utf-8")
        new, n, left = convert(src, lang)
        leftovers += [(str(path.relative_to(ROOT)), x) for x in left]
        if n:
            total += n
            files += 1
            if not dry:
                path.write_text(new, encoding="utf-8")
    print(f"장 참조 이전: {total}곳 / {files}파일" + ("  (--dry-run)" if dry else ""))
    if leftovers:
        print(f"손대지 않은 것 {len(leftovers)}건 --- 사람이 볼 것:")
        for f, x in leftovers[:20]:
            print(f"  {f}\t{x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
