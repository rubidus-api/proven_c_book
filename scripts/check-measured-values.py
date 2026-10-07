#!/usr/bin/env python3
r"""재는 예제가 낸 *값*을 본다 (2026-09-15 cow·promise 사고의 뒤처리).

`verify-examples.sh` 는 종료 코드만 본다. 컴파일러가 채우기를 지워 `cow` 가 0 MB 를
찍어도 프로그램은 0 으로 끝난다. 이 검사는 캡처된 출력에서 핵심 값을 뽑아
`docs/measured-values.tsv` 의 기대값과 견준다. 기준 컴파일러와 교차 컴파일러 양쪽의
출력에 같은 표를 대므로, 두 컴파일러의 값을 견주는 일이 된다.

  · 출력 파일이 없으면 실패다 --- 못 잰 것을 통과로 치지 않는다.
  · 교차 검증에서 건너뛴 예제(`docs/example-cross-skip.tsv`)는 건너뛴다고 말한다.
  · x86-64 리눅스가 아니면 전부 건너뛰고, 건너뛰었다고 말한다.

사용법: python3 scripts/check-measured-values.py <캡처 디렉터리> [--self-test]
종료 상태: 값이 어긋나거나 없으면 1
"""
import pathlib
import platform
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TABLE = ROOT / "docs" / "measured-values.tsv"


def rows():
    out = []
    for raw in TABLE.read_text(encoding="utf-8").splitlines():
        if raw.startswith("#") or not raw.strip():
            continue
        c = raw.split("\t")
        out.append((c[0], re.compile(c[1]), float(c[2]), float(c[3]), c[4]))
    return out


def check(text, pat, want, tol):
    """(통과?, 읽은 값 또는 None)."""
    m = pat.search(text)
    if not m:
        return False, None
    got = float(m.group(1))
    return abs(got - want) <= tol, got


def self_test():
    pat = re.compile(r"private\s+(\d+) MB")
    cases = [("private   512 MB", True), ("private     0 MB", False), ("nothing here", False)]
    bad = [t for t, want in cases if check(t, pat, 512, 8)[0] != want]
    print("check-measured-values: self-test " + ("FAILED" if bad else "ok"))
    return 1 if bad else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return 2
    outdir = pathlib.Path(args[0])
    if not (platform.system() == "Linux" and platform.machine() == "x86_64"):
        print(f"check-measured-values: {platform.system()} {platform.machine()} --- "
              "표의 값은 x86-64 리눅스의 것이다. 건너뛴다(통과가 아니다)")
        return 0
    skipped = set(a for a in args[1:])
    bad = seen = 0
    for rel, pat, want, tol, what in rows():
        example = rel[:-len(".out")]
        if example in skipped:
            print(f"  건너뜀: {example} --- 교차 검증에서 건너뛴 예제다")
            continue
        path = outdir / rel
        if not path.exists():
            bad += 1
            print(f"  ⚠️  {rel}: 출력이 없다 --- {what}")
            continue
        seen += 1
        ok, got = check(path.read_text(encoding="utf-8", errors="replace"), pat, want, tol)
        if not ok:
            bad += 1
            shown = "값을 찾지 못했다" if got is None else f"{got:g}"
            print(f"  ⚠️  {rel}: {what} --- 기대 {want:g}±{tol:g}, 읽은 값 {shown}")
    if bad:
        print(f"check-measured-values: 재는 예제의 값이 어긋난다 {bad}건")
        return 1
    print(f"check-measured-values: 핵심 값 {seen}개가 기대 범위 안이다 ({outdir.name})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
