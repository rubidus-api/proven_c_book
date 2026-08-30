#!/usr/bin/env bash
# GitHub Pages 를 *실제로* 다시 굽고, 웹 판이 그 판 번호를 내주는지 확인한다.
#
#   scripts/pages-build.sh [--version vX.Y.Z]
#
# ★ 왜 필요한가
#   푸시하면 Pages 가 알아서 다시 구울 것 같지만, 이 저장소에서는 *두 번*
#   그러지 않았다. v0.75.3 과 v0.76.0 을 올린 뒤에도 마지막 빌드가 v0.75.2
#   커밋에 멈춰 있어서, 저장소에는 새 판이 들어 있는데 웹 판은 두 판 뒤를
#   내주고 있었다(2026-08-31 저자 지적으로 발견). v0.76.4 에서도 같았다.
#
#   저장소 안은 모두 맞고 GitHub 쪽 발행만 멈춘 것이라, 어떤 파일 검사로도
#   드러나지 않는다. *살아 있는 페이지를 읽어야* 안다. 그래서 릴리스 절차의
#   마지막 걸음으로 이 스크립트를 둔다 --- 빌드를 청하고, 끝날 때까지 기다리고,
#   웹 판에서 판 번호를 눈으로 확인한다.
#
# 인증서는 저장소 밖에 둔다. 자리는 GH_TOKEN_FILE 로 주고, 기본값은 저장소
# 바깥의 상대 경로다 --- 이 기계의 절대 경로를 공개 저장소에 적지 않는다.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
repo=rubidus-api/proven_c_book
site=https://rubidus-api.github.io/proven_c_book
ver=""

while [ $# -gt 0 ]; do
  case "$1" in
    --version) shift; [ $# -gt 0 ] || { echo "pages-build: --version 뒤에 판 번호" >&2; exit 2; }; ver=$1 ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) printf 'pages-build: 모르는 선택지: %s\n' "$1" >&2; exit 2 ;;
  esac
  shift
done

command -v curl >/dev/null 2>&1 || { echo "pages-build: curl 이 필요하다" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "pages-build: python3 이 필요하다" >&2; exit 1; }

# 판 번호를 주지 않으면 원고에서 읽는다 --- 단일 소스는 book/main.typ 이다.
if [ -z "$ver" ]; then
  ver=$(sed -n 's/^#let book-version = "\(.*\)"$/\1/p' "$root/book/main.typ")
fi
[ -n "$ver" ] || { echo "pages-build: 판 번호를 알 수 없다" >&2; exit 1; }

tokfile=${GH_TOKEN_FILE:-"$root/../github-personal-access-token"}
[ -f "$tokfile" ] || { echo "pages-build: 인증서를 찾지 못했다 (GH_TOKEN_FILE)" >&2; exit 1; }
tok=$(tr -d '\n\r ' < "$tokfile")

api() { curl -sS -H "Authorization: token $tok" -H "Accept: application/vnd.github+json" "$@"; }
field() { python3 -c "import json,sys; d=json.load(sys.stdin); print(d.get('$1') or '')"; }

echo "pages-build: $ver --- 빌드를 청한다"
api -X POST "https://api.github.com/repos/$repo/pages/builds" >/dev/null

# 끝날 때까지 기다린다. Pages 빌드는 대개 1 분 안쪽이다.
i=0
while [ "$i" -lt 60 ]; do
  status=$(api "https://api.github.com/repos/$repo/pages/builds/latest" | field status)
  case "$status" in
    built|errored) break ;;
  esac
  i=$((i + 1))
  sleep 5
done

latest=$(api "https://api.github.com/repos/$repo/pages/builds/latest")
status=$(printf '%s' "$latest" | field status)
commit=$(printf '%s' "$latest" | field commit)
head=$(cd "$root" && git rev-parse HEAD)

if [ "$status" != "built" ]; then
  echo "pages-build: 빌드가 끝나지 않았다 (status=$status)" >&2
  exit 1
fi
printf 'pages-build: 빌드 완료 (commit %s)\n' "$(printf '%s' "$commit" | cut -c1-8)"
if [ "$commit" != "$head" ]; then
  echo "pages-build: ★ 구운 커밋이 HEAD 가 아니다 --- 먼저 푸시했는지 본다" >&2
  exit 1
fi

# ★ 여기가 요점이다. 빌드가 「끝났다」고 해도 *페이지가 그 판을 내주는지*는
#   따로 물어야 한다. 두 판(한국어·영어)을 모두 읽는다.
# 몇 번까지 물어볼지. 실패 경로를 시험할 때 짧게 준다.
tries=${PAGES_POLL:-40}
ok=0
for lang in ko en; do
  j=0
  while [ "$j" -lt "$tries" ]; do
    # ★ 파이프로 바로 grep 에 넘기면 grep -q 가 먼저 끝나 curl 이 「출력을
    #   쓰지 못했다」(23)고 짖는다. 받아 두고 나서 본다.
    page=$(curl -sS "$site/$lang/index.html?cachebust=$$-$j" || true)
    case "$page" in
      *"$ver"*)
        printf '  %s : %s\n' "$lang" "$ver"
        ok=$((ok + 1))
        break ;;
    esac
    j=$((j + 1))
    sleep 5
  done
done

if [ "$ok" -ne 2 ]; then
  echo "pages-build: ★ 웹 판이 아직 $ver 을 내주지 않는다" >&2
  exit 1
fi
echo "pages-build: 웹 판 두 쪽이 $ver 을 내준다"
