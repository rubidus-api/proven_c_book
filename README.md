# Proven C Book — 프로븐 C 라이브러리와 함께하는 현대적 C 입문

*[English README](README-en.md)*

C를 처음 배우는 사람을 위한 한국어 책이다. 문법 목록이 아니라 *컴퓨터가
어떻게 생겼는지*에서 출발해, 오늘의 표준(C23)을 기본값으로 삼고, 마지막
부에서 [proven](https://github.com/rubidus-api) C 라이브러리의 매뉴얼로
이어진다.

- **현재 판**: v0.14.0 — **초안(draft)**
- **PDF 바로 받기** — [한국어 PDF](https://github.com/rubidus-api/proven_c_book/releases/download/v0.14.0/proven_c_book-v0.14.0-ko.pdf) · [English PDF](https://github.com/rubidus-api/proven_c_book/releases/download/v0.14.0/proven_c_book-v0.14.0-en.pdf)
- **웹으로 읽기** — [한국어](https://rubidus-api.github.io/proven_c_book/ko/) · [English](https://rubidus-api.github.io/proven_c_book/en/)
- **묶음(zip)** — [ko](https://github.com/rubidus-api/proven_c_book/releases/download/v0.14.0/proven_c_book-v0.14.0-ko.zip) · [en](https://github.com/rubidus-api/proven_c_book/releases/download/v0.14.0/proven_c_book-v0.14.0-en.zip) · [전체](https://github.com/rubidus-api/proven_c_book/releases/download/v0.14.0/proven_c_book-v0.14.0-all.zip)
- 저장소 안 사본: [ko PDF](dist/proven_c_book-v0.14.0-ko.pdf) · [en PDF](dist/proven_c_book-v0.14.0-en.pdf)
- 13부 81장 + 부록 A~E + 찾아보기 — 한국어판 475쪽, 영어판 505쪽 (저장소 최신본)
- 이 저장소가 최신본이다. 갱신 내역은 [CHANGELOG.md](CHANGELOG.md)에 있다.

## 이 책의 특징

- **실습보다 읽는 흐름을 택했다.** 연습문제나 "직접 해 보라"로 끊지 않고,
  본문 곳곳의 즉문즉답을 따라가며 이어 읽도록 썼다. 손으로 익히는 연습이
  목적이라면 기존의 좋은 C 입문서들이 그 일을 더 잘한다 — 이 책은 그 옆에서
  "왜 그렇게 생겼는지"를 읽어 나가는 쪽을 맡는다.
- **인쇄된 실행 결과는 전부 실제 출력이다.** 수록 예제 94종을 매 빌드마다
  컴파일·실행해 그 출력을 지면에 싣는다(GCC 14 기준, Clang으로 교차 검증).
- **예제도 판마다 갈라 둔다.** 영어판은 주석과 출력 문구가 영어인 별도
  트리(`examples-en/`)를 쓴다. 두 트리 모두 매 빌드마다 전수 검증한다.
- **오늘의 C.** C23을 기본값으로 쓰고, 옛 관행은 역사로만 다룬다.
- **AI를 보조 도구로 삼아 집필했다.** 구성·방침·수록 여부는 저자가 정하고
  검토했으며, 예제는 기계가 매번 실제로 돌려 검증한다.

## 예제 직접 돌려 보기

본문에 실린 예제는 전부 이 저장소에 있고, 한 번에 빌드·실행해 볼 수 있다.

```sh
scripts/verify-examples.sh              # 한국어판 예제 전수 빌드 + 실행 + 출력 캡처
scripts/verify-examples.sh examples-en  # 영어판 예제 트리
CC=clang scripts/verify-examples.sh     # 다른 컴파일러로 교차 검증
```

- C23 컴파일러가 필요하다(GCC 14+ 또는 Clang 16+).
- `#include <proven...>`을 쓰는 예제는 `vendor/proven`이 자동으로 함께
  빌드된다.

> 원고(Typst 소스)와 조판 스크립트(`build-book.sh` 등)는 공개하지 않는다.
> 저장소의 `scripts/`에는 예제 검증과 점검 도구만 둔다. 이 저장소가 담는
> 것은 완성된 책(PDF·HTML)과 예제 코드다.

## 구성

```
dist/       배포물 — PDF(ko·en)와 zip 묶음
docs/       GitHub Pages 가 서비스하는 HTML 판 (ko/, en/)
examples/   본문에 실리는 예제 소스 94종(파일 98개) — 전부 검증된 것
examples-en/ 같은 예제의 영어판 (주석·문자열·출력이 영어)
scripts/    예제 검증 스크립트
vendor/     proven 라이브러리 스냅샷 (예제 링크용)
```

## 라이선스

- **본문**(`book/`, 생성된 PDF): **CC BY-NC-SA 4.0** — 저장소의
  [LICENSE](LICENSE)가 정본이다. 출처를 밝히면 공유·개작할 수 있으나
  영리 이용은 불가하며, 개작물에도 같은 라이선스를 적용해야 한다.
- **예제 코드**(`examples/`, `examples-en/`, `scripts/`): **MIT** — [LICENSE-CODE](LICENSE-CODE).
  책에서 배운 코드는 제약 없이 가져다 쓸 수 있다.
- `vendor/proven/`: 원저작물의 라이선스를 따른다.

자세한 안내는 [LICENSE-NOTICE.md](LICENSE-NOTICE.md)에 있다.

## 기여 — 오류 신고만 받는다

틀린 서술, 동작하지 않는 예제, 오탈자, 낡은 정보를 이슈로 알려 주시면
고맙게 반영한다. 새 장·절 원고나 구성 변경 제안은 받지 않는다. 자세한
내용은 [CONTRIBUTING.md](CONTRIBUTING.md).

연락은 rubidus@gmail.com.
