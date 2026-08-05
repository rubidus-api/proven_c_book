#!/usr/bin/env python3
"""릴리스 zip 생성 (zip 명령이 없는 환경용)."""
import sys, zipfile, pathlib

stage, dist, ver = (pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3])

def make(name, dirs):
    files = [f for d in dirs for f in sorted((stage / d).rglob("*")) if f.is_file()]
    if not files:
        return
    out = dist / f"proven_c_book-{ver}-{name}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in files:
            z.write(f, f.relative_to(stage))
    print(f"  {out.name}  ({out.stat().st_size // 1024} KB, {len(files)} files)")

make("ko", ["ko"])
make("en", ["en"])
make("all", ["ko", "en"])
