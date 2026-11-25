#!/usr/bin/env python3
"""공개 저장소에 이 기계의 사정이 새지 않았는가.

규칙과 구현은 작업공간의 공용 도구 하나에 모여 있다(`usr/bin/check-privacy`).
이 파일은 그 도구를 이 저장소의 사정에 맞게 부르는 얇은 껍데기다 --- 추적 파일에
더해 *캡처된 예제 출력*까지 보게 한다. 실행 결과는 아무도 다시 읽지 않은 채 책에
인쇄되기 때문이다(53장 `argv[0]` 이 빌드 디렉터리를 찍어 444쪽에 실렸다).

도구는 작업공간 바로 밑(`../usr/bin`)과 한 단계 더 밑(`../../usr/bin`, 프로젝트가
`proj/` 아래에 있을 때) 두 곳에서 찾는다. 하나만 보던 때는 프로젝트를 옮긴 뒤
도구를 못 찾고 *조용히 건너뛰었다*(2026-11-25 에 드러났다).

저장소 뿌리에 `public.manifest` 가 있으면(비공개 운영 문서 `ops/` 와 한 저장소인
통합 배치) 공개될 파일 --- `[include]` 에서 `[exclude]` 를 뺀 것 --- 만 본다.
비공개 쪽은 공개되지 않으므로 이 검사의 대상이 아니고, 섞어 세면 게이트가 늘
실패해서 결국 아무도 읽지 않게 된다. 거른 파일 수는 따로 말한다.

도구가 없는 곳(저장소만 따로 clone 한 경우)에서는 건너뛴다고 말하고 지나간다.

사용법:  python3 scripts/check-privacy.py
"""
import importlib.machinery
import importlib.util
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CANDIDATES = [d / "usr" / "bin" / "check-privacy" for d in (ROOT.parent, ROOT.parent.parent)]
TOOL = next((p for p in CANDIDATES if os.access(p, os.X_OK)), None)
MANIFEST = ROOT / "public.manifest"


def manifest_rules(path):
    """public.manifest 의 [include] · [exclude] 항목(뿌리 기준 상대 경로)."""
    secs, cur = {"include": [], "exclude": []}, None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            cur = line[1:-1].strip()
            continue
        if cur in secs:
            secs[cur].append(line)
    return secs["include"], secs["exclude"]


def matches(rel, entry):
    if entry.endswith("/"):
        return rel.startswith(entry)
    return rel == entry


def is_public(rel, include, exclude):
    return any(matches(rel, e) for e in include) and not any(matches(rel, e) for e in exclude)


def load_tool(path):
    loader = importlib.machinery.SourceFileLoader("ws_check_privacy", str(path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    return mod


def main() -> int:
    if TOOL is None:
        print("check-privacy: 공용 검사 도구가 없다 --- 건너뛴다 "
              "(작업공간의 usr/bin/check-privacy)")
        return 0
    if not MANIFEST.is_file():
        # 공개 저장소 그 자체(미러 · 따로 clone 한 것): 추적 파일 전부가 공개 대상이다.
        return subprocess.call([str(TOOL)], cwd=ROOT)

    tool = load_tool(TOOL)
    include, exclude = manifest_rules(MANIFEST)
    allow = tool.allowlist(ROOT)
    tracked = tool.tracked(ROOT)
    paths, skipped = [], 0
    for p in tracked:
        rel = p.relative_to(ROOT).as_posix() if p.is_absolute() else p.as_posix()
        if is_public(rel, include, exclude):
            paths.append(p)
        else:
            skipped += 1
    hits = tool.scan(paths, ROOT, allow)
    if hits:
        for h in hits:
            print("⚠️ ", h)
        print(f"check-privacy: {len(hits)} 건 — 이 기계의 사정이 공개될 파일에 있다")
        print("    경로는 저장소 기준의 상대 경로로, 인증서 자리는 환경 변수로 뺄 것")
        print("    그 자리에서 옳다면 저장소 뿌리의 .privacy-allow 에 적는다")
        return 1
    print(f"check-privacy: 깨끗하다 — 공개될 파일 {len(paths)}개를 훑었다 "
          f"(public.manifest 밖 {skipped}개는 공개되지 않아 뺐다)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
