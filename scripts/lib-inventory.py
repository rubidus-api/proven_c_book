#!/usr/bin/env python3
"""C 표준 부속서 B(Library summary)에서 헤더별 인벤토리를 뽑는다 (RFC-0025 L0).

부록 B「표준 라이브러리 요람」의 *뼈대*는 사람이 손으로 적지 않는다. 이름 하나를
빠뜨리면 「모든 함수에 대해서」라는 요구가 깨지는데, 1,000개가 넘는 이름을 눈으로
맞출 수는 없기 때문이다. 그래서 표준 문서에서 기계로 뽑는다.

  ★ 표준 문서 자체는 저장소에 담지 않는다 --- 경로를 인자로 받는다.
  ★ 뽑는 것은 *이름과 선언*뿐이다. 표준의 산문은 옮기지 않는다(RFC-0021 §0).

사용법:
    python3 scripts/lib-inventory.py <n3220.txt> [-o docs/library-inventory.json]

만들어 내는 것: {헤더: {functions: [{name, decl, variants}], macros: [...],
types: [...]}}
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DEFAULT = ROOT / "docs" / "library-inventory.json"

# 변형 접미사 --- sin/sinf/sinl 을 한 행으로 묶는다
SUFFIX = re.compile(r"(f|l|d32|d64|d128)$")

# 표준이 정의하는 헤더만 받는다(예제 속 <file.h> 따위를 거른다)
KNOWN = {
    "assert.h", "complex.h", "ctype.h", "errno.h", "fenv.h", "float.h",
    "inttypes.h", "iso646.h", "limits.h", "locale.h", "math.h", "setjmp.h",
    "signal.h", "stdalign.h", "stdarg.h", "stdatomic.h", "stdbit.h",
    "stdbool.h", "stdckdint.h", "stddef.h", "stdint.h", "stdio.h",
    "stdlib.h", "stdnoreturn.h", "string.h", "tgmath.h", "threads.h",
    "time.h", "uchar.h", "wchar.h", "wctype.h",
}


def annex_b(text):
    """본문 쪽 부속서 B 를 잘라 낸다(목차의 같은 이름을 피한다)."""
    start = None
    for m in re.finditer(r"Annex B", text):
        if "Library summary" in text[m.start():m.start() + 120] and m.start() > 100_000:
            start = m.start()
            break
    if start is None:
        raise SystemExit("부속서 B 를 찾지 못했다 --- 올바른 표준 문서인가?")
    end = len(text)
    for m in re.finditer(r"Annex C", text):
        if m.start() > start:
            end = m.start()
            break
    body = text[start:end]
    # 쪽 머리말·꼬리말을 걷어 낸다
    body = re.sub(r"©[^\n]*|ISO/IEC 9899:2024[^\n]*|Library summary — \d+", " ", body)
    body = re.sub(r"\s+", " ", body)
    # ★ PDF 추출이 밑줄 앞에 공백을 넣는다(354곳): "rsize _t" → "rsize_t".
    #   고치지 않으면 이름이 "_s" 처럼 잘려 인벤토리가 통째로 어긋난다.
    for _ in range(3):
        body = re.sub(r"([A-Za-z0-9]) _([A-Za-z])", r"\1_\2", body)
    return body


def split_headers(body):
    parts = re.split(r"(B\.\d+\s+[A-Za-z0-9 ,/\-]*?<([a-z]+\.h)>)", body)
    out = []
    for i in range(1, len(parts) - 1, 3):
        hdr, chunk = parts[i + 1], parts[i + 2]
        if hdr in KNOWN:
            out.append((hdr, chunk))
    return out


# 반환 타입에 올 수 있는 낱말 --- 이보다 앞은 앞 항목의 꼬리다
_TYPEISH = re.compile(r"^(\\*+|const|struct|union|enum|unsigned|signed|long|short|"
                      r"void|char|int|float|double|_Bool|bool|_Complex|_Atomic|"
                      r"restrict|volatile|QVoid|[A-Za-z_][A-Za-z0-9_]*_t|"
                      r"[A-Za-z_][A-Za-z0-9_]*)\**$")


def tidy_decl(decl, name):
    """선언에서 *이 함수의 것*만 남긴다.

    부속서 B 는 한 문단에 매크로·타입·선언이 잇달아 나온다. 정규식이 앞의 꼬리까지
    물고 오므로(예: "size_t __STDC_VERSION_STRING_H__ NULL void *memcpy(...)"),
    이름 앞 토큰을 뒤에서부터 훑어 *반환 타입으로 볼 수 있는 데까지*만 남긴다.
    """
    i = decl.rfind(name + "(")
    if i < 0:
        i = decl.rfind(name)
    head, tail = decl[:i].split(), decl[i:]
    keep = []
    for tok in reversed(head):
        if re.fullmatch(r"[A-Z][A-Z0-9_]{2,}", tok):   # 매크로 이름이면 거기서 끊는다
            break
        if not _TYPEISH.match(tok) or len(keep) >= 4:
            break
        keep.append(tok)
    return " ".join(reversed(keep) if keep else []) + (" " if keep else "") + tail


# ★ 타입 낱말은 함수 이름이 아니다. 아래 정규식은 「`(` 앞의 이름」을 함수로
#   보는데, 함수 포인터를 돌려주는 선언에서는 그 자리에 타입이 온다 ---
#   `void (*signal(int, void (*)(int)))(int);` 에서 `void (` 가 걸려
#   `signal.h` 의 함수 목록에 **void** 가 들어와 있었다(2026-08-31 발견).
NOT_A_NAME = {
    "void", "int", "char", "long", "short", "float", "double", "signed",
    "unsigned", "const", "volatile", "restrict", "struct", "union", "enum",
    "typedef", "static", "extern", "inline", "return", "sizeof", "if",
    "while", "for", "switch", "case", "do", "else", "goto", "bool",
}


def parse_header(chunk):
    """한 헤더의 덩어리에서 함수·매크로·타입을 가른다."""
    decls = {}
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_ \*\[\]\(\),\.]{0,120}?"
                         r"\b([a-z_][a-z0-9_]*)\s*\([^;]{0,400}?\)\s*;)", chunk):
        decl = " ".join(m.group(1).split())
        name = m.group(2)
        if name in NOT_A_NAME:
            continue
        decls.setdefault(name, tidy_decl(decl, name))

    names = set(decls)
    groups = {}
    for n in sorted(names):
        base = n
        s = SUFFIX.search(n)
        if s and len(n) - len(s.group(1)) >= 3 and n[:s.start()] in names:
            base = n[:s.start()]
        groups.setdefault(base, []).append(n)

    functions = [{"name": b, "variants": sorted(v), "decl": decls[b if b in decls else v[0]]}
                 for b, v in sorted(groups.items())]

    macros = sorted({m for m in re.findall(r"\b([A-Z][A-Z0-9_]{2,})\b", chunk)
                     if not m.startswith("ISO")})
    types = sorted(set(re.findall(r"\b([a-z_][a-z0-9_]*_t)\b", chunk)))
    return {"functions": functions, "macros": macros, "types": types}


def main() -> int:
    args = sys.argv[1:]
    out = OUT_DEFAULT
    if "-o" in args:
        k = args.index("-o")
        out = pathlib.Path(args[k + 1])
        del args[k:k + 2]
    if not args:
        print("사용법: python3 scripts/lib-inventory.py <표준문서.txt> [-o 나갈파일]")
        return 2

    text = pathlib.Path(args[0]).read_text(encoding="utf-8", errors="replace")
    inv = {}
    for hdr, chunk in split_headers(annex_b(text)):
        inv[hdr] = parse_header(chunk)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(inv, ensure_ascii=False, indent=1), encoding="utf-8")

    f = sum(len(v["functions"]) for v in inv.values())
    m = sum(len(v["macros"]) for v in inv.values())
    t = sum(len(v["types"]) for v in inv.values())
    print(f"lib-inventory: 헤더 {len(inv)}개 · 함수 {f}행 · 매크로 {m} · 타입 {t} "
          f"→ {out.relative_to(ROOT) if out.is_relative_to(ROOT) else out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
