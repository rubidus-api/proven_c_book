#!/usr/bin/env bash
# 조판 견본*만* 다시 만든다 (RFC-0027 §5, 저자 지시 2026-08-10).
#
# ★ 왜 따로 두는가
#   디자인을 한 번 보자고 책 전체를 다시 찍을 까닭이 없다. 견본은 그 자체로 완결된 두
#   파일(docs/style-specimen.{html,pdf})이므로, 그것만 다시 만들어 눈으로 본다.
#
# ★ 공개는 판을 낼 때 (2026-11-27)
#   웹판 · PDF · 견본은 main 에 커밋하지 않고 gh-pages 에만 둔다. 그 가지는 릴리스 때만
#   커밋 하나로 갈아 끼운다(작업공간 문서 기준 DS-G5 · DS-G5a). 그래서 이 스크립트는 만들기만
#   한다. 릴리스 사이에 견본만 따로 내야 하면 저자에게 묻는다(DS-G5a).
#
#     scripts/publish-specimen.sh
#       → docs/style-specimen.{html,pdf} 갱신 (로컬)
#       → 다음 판: release.sh → stage-pages.sh → pub publish … --release --tag vX
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
bash "$root/scripts/make-specimen.sh"
echo "publish-specimen: 견본을 다시 만들었다 --- 공개는 다음 판과 함께(gh-pages)"
echo "  웹  file://$root/docs/style-specimen.html"
