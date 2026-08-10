#!/usr/bin/env bash
# 조판 견본*만* 웹에 올린다 (RFC-0027 §5, 저자 지시 2026-08-10).
#
# ★ 왜 따로 두는가
#   디자인을 한 번 보이자고 787쪽을 다시 찍고 릴리스를 낼 까닭이 없다.
#   견본은 그 자체로 완결된 두 파일이므로, 그것만 밀어 올리면 된다.
#
#     scripts/publish-specimen.sh
#       → docs/style-specimen.html 갱신 → 커밋 → main 푸시
#       → https://rubidus-api.github.io/proven_c_book/style-specimen.html
#
#   책의 판 번호도, dist 의 묶음도 건드리지 않는다.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
bash "$root/scripts/make-specimen.sh"

cd "$root"
# ★ `git diff` 는 *추적 중인* 파일만 본다 --- 처음 생긴 견본 PDF 를 「달라진 것이
#   없다」고 넘겨 버렸다. status 로 본다(untracked 도 잡힌다).
if [ -z "$(git status --porcelain -- docs/style-specimen.html docs/style-specimen.pdf)" ]; then
  echo "publish-specimen: 달라진 것이 없다 --- 올리지 않는다"
  exit 0
fi
git add docs/style-specimen.html docs/style-specimen.pdf book/style.typ book/style-specimen.typ \
        styles/book-trial.css styles/book.css book/lib.typ 2>/dev/null || true
git -c user.name=rubidus-api -c user.email=rubidus@gmail.com \
    commit -q -m "조판 견본 갱신 (시험 값)"
tok=$(cat <workspace>/github-personal-access-token | tr -d '\n\r ')
git push -q "https://x-access-token:$tok@github.com/rubidus-api/proven_c_book.git" main
echo "publish-specimen: 올렸다"
echo "  웹  https://rubidus-api.github.io/proven_c_book/style-specimen.html"
echo "  PDF https://rubidus-api.github.io/proven_c_book/style-specimen.pdf"
