#!/usr/bin/env python3
"""독자가 보는 수치를 *재서* 뿌린다 (검토 2026-08-29 §3).

판 번호는 `sync-version.py` 가 맞춰 주는데, 정작 독자가 먼저 보는 **쪽수 · 예제 수 ·
최종 수정일**은 아무도 맞추지 않았다. 그래서 README 는 917·968쪽이라 적고 실제 배포
PDF 는 1003·1059쪽이었다. 판 번호만 보는 검사는 이것을 못 본다.

★ 「예제」의 뜻을 먼저 정한다 --- 세는 방법이 셋이라 수가 셋이었다.
    · 예제 트리의 C 소스 파일 수      (여러 파일이 한 프로그램인 경우가 있다)
    · 검증기가 돌린 실행 단위 수      (책에 안 실리는 것도 있다)
    · 원고의 `#demo` 호출 수          ← **이것을 쓴다**
  독자에게 「예제」는 *지면에 실린 시연*이다. 그러니 원고가 부르는 횟수로 센다.

쪽수는 빌드된 PDF 에서 잰다. PDF 가 없으면 *고치지 않고 그렇게 말한다* --- 못 잰
것을 옛 수치로 남겨 두는 편이, 0 으로 덮어 새 거짓말을 만드는 것보다 낫다.

사용법:
    python3 scripts/sync-metadata.py           # 재서 맞춘다
    python3 scripts/sync-metadata.py --check   # 어긋나면 1 을 돌려준다
"""
import datetime
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def demo_count() -> int:
    n = 0
    for d in ("chapters", "appendix", "front", "back"):
        for f in (ROOT / "book" / d).glob("*.typ"):
            n += len(re.findall(r"#demo\(", f.read_text(encoding="utf-8")))
    return n


def pages(pdf: pathlib.Path):
    if not pdf.exists():
        return None
    try:
        import pypdf
        return len(pypdf.PdfReader(str(pdf)).pages)
    except Exception:
        return None


def last_change() -> str:
    """원고가 마지막으로 바뀐 날. git 이 없으면 파일 시각에서 잰다."""
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "log", "-1", "--format=%cs"],
                             capture_output=True, text=True, timeout=20)
        if out.returncode == 0 and out.stdout.strip():
            return out.stdout.strip()
    except Exception:
        pass
    newest = 0.0
    for f in (ROOT / "book").rglob("*.typ"):
        newest = max(newest, f.stat().st_mtime)
    return datetime.date.fromtimestamp(newest).isoformat() if newest else ""


def sub(path, pattern, repl, changes, check):
    p = ROOT / path
    txt = p.read_text(encoding="utf-8")
    new, n = re.subn(pattern, repl, txt)
    if n and new != txt:
        changes.append(f"{path}: {n}곳")
        if not check:
            p.write_text(new, encoding="utf-8")
    return n


def main() -> int:
    check = "--check" in sys.argv
    demos = demo_count()
    ko = pages(ROOT / "build" / "book.pdf")
    en = pages(ROOT / "build" / "book-en.pdf")
    day = last_change()
    changes, unknown = [], []

    if ko and en:
        sub("README.md", r"한국어판 [\d,]+쪽, 영어판 [\d,]+쪽",
            f"한국어판 {ko:,}쪽, 영어판 {en:,}쪽", changes, check)
        sub("README-en.md", r"[\d,]+ pages in English, [\d,]+ in Korean",
            f"{en:,} pages in English, {ko:,} in Korean", changes, check)
    else:
        unknown.append("쪽수 --- 빌드된 PDF 가 없어 재지 못했다(고치지 않았다)")

    sub("README.md", r"예제 [\d,]+개", f"예제 {demos}개", changes, check)
    sub("README-en.md", r"All [\d,]+ listings", f"All {demos} listings", changes, check)
    sub("README-en.md", r"The [\d,]+ listings that appear",
        f"The {demos} listings that appear", changes, check)
    if day:
        sub("book/main.typ", r'#let book-updated = "[\d-]+"',
            f'#let book-updated = "{day}"', changes, check)
        sub("book-en/main.typ", r'#let book-updated = "[\d-]+"',
            f'#let book-updated = "{day}"', changes, check)

    for u in unknown:
        print(f"  ⚠️  {u}")
    if check and changes:
        for c in changes:
            print(f"  ⚠️  어긋남 --- {c}")
        print("sync-metadata: 독자가 보는 수치가 실제와 다르다")
        return 1
    for c in changes:
        print(f"  맞춤: {c}")
    print(f"sync-metadata: 예제 {demos}개 · "
          f"{f'{ko:,}·{en:,}쪽' if ko and en else '쪽수 못 잼'} · 최종 수정 {day or '모름'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
