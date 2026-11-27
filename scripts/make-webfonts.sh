#!/bin/sh
# Thin wrapper: web fonts are made by the shared document kit's tools/webfonts.py (its private
# copy lives under ops/vendor/; scripts/ws-tools.sh finds it). The kit reads the font folder from
# TYPST_FONT_PATHS or FONT_PATH; when neither is set this book's workspace lookup supplies it.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
. "$root/scripts/ws-tools.sh"
kit=$(doc_kit) || { echo "make-webfonts: no copy of the shared document kit (ops/vendor/)" >&2; exit 1; }
if [ -z "${TYPST_FONT_PATHS:-}" ] && [ -z "${FONT_PATH:-}" ]; then
  FONT_PATH=$(ws_fonts)
  export FONT_PATH
fi
exec python3 "$kit/tools/webfonts.py" --config "$root/ops/doc-src/doc.toml"
