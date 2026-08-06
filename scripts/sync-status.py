#!/usr/bin/env python3
"""한국어 원본과 영어판의 동기화 상태를 관리한다.

원본(book/…)의 각 파일 해시를 book-en/sync.json 에 기록해 두고,
- 번역이 없는 파일          → pending
- 원본이 바뀐 파일          → stale   (번역을 갱신해야 한다)
- 원본 해시가 기록과 같으면 → synced

사용법:
  sync-status.py            상태 표 출력 (stale 이 있으면 종료 코드 1)
  sync-status.py --mark F   F(원본 경로)를 "지금 번역했다"고 기록
  sync-status.py --write    TRANSLATION.md 를 갱신
"""
import hashlib, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO, EN = ROOT / "book", ROOT / "book-en"
DB = EN / "sync.json"

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()[:16]

def sources():
    """번역 대상 = 한국어판의 장·부·앞부속 파일"""
    out = []
    for sub in ("chapters", "parts", "front", "appendix", "back"):
        d = KO / sub
        if d.is_dir():
            out += sorted(d.glob("*.typ"))
    out.append(KO / "main.typ")
    return out

def load():
    return json.loads(DB.read_text()) if DB.exists() else {}

def save(db):
    DB.write_text(json.dumps(db, indent=2, ensure_ascii=False, sort_keys=True) + "\n")

def rel(p):
    return str(p.relative_to(ROOT))

def status():
    db, rows = load(), []
    for src in sources():
        key = rel(src)
        cur = digest(src)
        rec = db.get(key)
        en = EN / src.relative_to(KO)
        if rec is None or not en.exists():
            state = "pending"
        elif rec.get("ko") == cur:
            state = "synced"
        else:
            state = "stale"
        rows.append((key, state, cur, (rec or {}).get("ko")))
    return rows

def main():
    args = sys.argv[1:]
    if args and args[0] == "--mark":
        db = load()
        for f in args[1:]:
            p = (ROOT / f).resolve()
            db[rel(p)] = {"ko": digest(p)}
        save(db)
        print(f"기록: {len(args) - 1}개 파일")
        return 0
    rows = status()
    counts = {"synced": 0, "stale": 0, "pending": 0}
    for _, st, _, _ in rows:
        counts[st] += 1
    if "--write" in args:
        lines = ["# 번역 진행 상황 / Translation status", "",
                 "한국어판이 원본이고, 영어판은 그것을 옮긴다. 이 표는",
                 "`scripts/sync-status.py --write` 가 자동으로 갱신한다.",
                 "The Korean edition is the source; this table is generated.", "",
                 f"- **synced** {counts['synced']} · **stale** {counts['stale']} · **pending** {counts['pending']}",
                 "", "| 원본 / source | 상태 / state |", "|---|---|"]
        for key, st, _, _ in rows:
            mark = {"synced": "✅ synced", "stale": "⚠️ stale (원본이 바뀜)",
                    "pending": "⏳ pending"}[st]
            lines.append(f"| `{key}` | {mark} |")
        lines += ["", "- ✅ 번역이 현재 원본과 일치한다.",
                  "- ⚠️ 원본이 바뀌었다 — 영어판을 갱신해야 한다.",
                  "- ⏳ 아직 번역하지 않았다.", ""]
        (ROOT / "TRANSLATION.md").write_text("\n".join(lines))
        print("TRANSLATION.md 갱신")
    print(f"sync-status: synced {counts['synced']}, stale {counts['stale']}, pending {counts['pending']}")
    for key, st, cur, old in rows:
        if st == "stale":
            print(f"  ⚠️  {key}  ({old} → {cur})")
    return 1 if counts["stale"] else 0

if __name__ == "__main__":
    sys.exit(main())
