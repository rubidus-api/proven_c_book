# Proven C Book — proven 라이브러리에 기반한 모던 C 입문

C를 처음 배우는 사람을 위한 한국어 책이다. 문법 목록이 아니라 *컴퓨터가
어떻게 생겼는지*에서 출발해, 오늘의 표준(C23)을 기본값으로 삼고, 마지막
부에서 [proven](https://github.com/rubidus-api) C 라이브러리의 매뉴얼로
이어진다.

- **현재 판**: v0.1.0 — [dist/proven_c_book-v0.1.0.pdf](dist/proven_c_book-v0.1.0.pdf)
- 12부 58장 + 부록 A~E + 찾아보기, 약 570쪽
- 이 저장소가 최신본이다. 갱신 내역은 [CHANGELOG.md](CHANGELOG.md)에 있다.

## 이 책의 특징

- **읽기만 하면 된다.** 연습문제도, "직접 해 보라"도 없다. 본문 곳곳의
  문답으로 진행한다.
- **인쇄된 실행 결과는 전부 실제 출력이다.** 수록 예제 60여 종을 매 빌드마다
  컴파일·실행해 그 출력을 지면에 싣는다(GCC 14 기준, Clang으로 교차 검증).
- **오늘의 C.** C23을 기본값으로 쓰고, 옛 관행은 역사로만 다룬다.

## 빌드

```sh
# 예제 검증 + PDF 생성
TYPST=/path/to/typst FONT_PATH=/path/to/fonts scripts/build-book.sh
```

- Typst 0.15 이상, 본문 글꼴 Noto Serif/Sans CJK KR, 코드 글꼴 D2Coding.
- `scripts/verify-examples.sh` — 예제 전수 빌드·실행·출력 캡처(C23).

## 구성

```
book/       Typst 원고 (chapters/, appendix/, front/, back/, parts/)
examples/   본문에 실리는 예제 소스 — 전부 검증 대상
scripts/    빌드와 검증
vendor/     proven 라이브러리 스냅샷 (예제 링크용)
dist/       배포 PDF
```

## 라이선스

- **본문**(`book/`, 생성된 PDF): **CC BY-NC-SA 4.0** — 저장소의
  [LICENSE](LICENSE)가 정본이다. 출처를 밝히면 공유·개작할 수 있으나
  영리 이용은 불가하며, 개작물에도 같은 라이선스를 적용해야 한다.
- **예제 코드**(`examples/`, `scripts/`): **MIT** — [LICENSE-CODE](LICENSE-CODE).
  책에서 배운 코드는 제약 없이 가져다 쓸 수 있다.
- `vendor/proven/`: 원저작물의 라이선스를 따른다.

자세한 안내는 [LICENSE-NOTICE.md](LICENSE-NOTICE.md)에 있다.

## 오류 신고와 제안

이 저장소의 이슈로 받는다. 연락은 rubidus@gmail.com.
