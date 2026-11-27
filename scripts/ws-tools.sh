# ws-tools.sh — 작업공간 공용 도구의 자리를 찾는다(다른 스크립트가 `.` 으로 읽는다).
#
# 프로젝트는 작업공간 바로 밑에 있을 수도, 한 단계 더 밑(proj/)에 있을 수도 있다.
# 그래서 자리를 "$root/.." 하나로 박지 않고 두 곳을 차례로 본다. 절대 경로는 적지
# 않는다. 환경 변수(TYPST · FONT_PATH · FONTS · LOCPATH)가 있으면 그것이 먼저다.
#
#   ws_find <상대 경로>...   처음으로 있는 것의 경로를 찍고 0, 없으면 1
#
# 부르는 쪽은 `root` 나 `ROOT` 에 프로젝트 뿌리를 담아 둔다.

ws_find() {
    _ws_root=${root:-${ROOT:-.}}
    for _ws_rel in "$@"; do
        for _ws_dir in "$_ws_root/.." "$_ws_root/../.."; do
            if [ -e "$_ws_dir/$_ws_rel" ]; then
                printf '%s\n' "$_ws_dir/$_ws_rel"
                return 0
            fi
        done
    done
    return 1
}

# 글꼴 디렉터리와 typst 의 기본 자리(옛 자리 usr/toolchains 도 본다)
ws_fonts() {
    ws_find toolchain/fonts usr/toolchains/fonts toolchains/fonts || printf '%s\n' "${root:-${ROOT:-.}}/../toolchain/fonts"
}
ws_typst() {
    ws_find toolchain/typst/typst usr/toolchains/typst/typst || printf '%s\n' typst
}

# 공용 문서 도구의 비공개 사본 --- ops/vendor/ 아래에서 SYNC.txt 와 web/doc.css 가 있는 폴더.
# 공개 파일에는 그 도구의 이름을 적지 않는다(작업공간의 프로젝트 이름은 비공개다).
# 사본은 비공개 쪽(ops/)에만 있으므로 공개 저장소만 받은 곳에서는 1 을 돌려준다.
doc_kit() {
    for _dk in "${root:-${ROOT:-.}}"/ops/vendor/*/; do
        if [ -f "$_dk/SYNC.txt" ] && [ -f "$_dk/web/doc.css" ]; then
            printf '%s\n' "${_dk%/}"
            return 0
        fi
    done
    return 1
}
