#include <stdio.h>

/* 하나의 목록에서 열거형·이름표·검사 코드를 함께 만들어 낸다.
   목록을 고칠 때 한 곳만 고치면 나머지가 따라온다. */
#define ERROR_LIST(X)                       \
    X(OK,        0, "no error")             \
    X(NOT_FOUND, 2, "resource not found")   \
    X(DENIED,    5, "permission denied")    \
    X(TIMEOUT,   9, "operation timed out")

/* ① 열거형 만들기 — 이름과 값만 쓴다 */
#define AS_ENUM(name, code, text) ERR_##name = code,
enum error_code { ERROR_LIST(AS_ENUM) };
#undef AS_ENUM

/* ② 이름 문자열 표 만들기 — 이름을 문자열로 바꾼다(#) */
#define AS_NAME(name, code, text) [code] = #name,
static const char *const error_name[] = { ERROR_LIST(AS_NAME) };
#undef AS_NAME

/* ③ 설명 문자열을 돌려주는 함수 만들기 — switch 를 통째로 생성한다 */
#define AS_CASE(name, code, text) case ERR_##name: return text;
static const char *error_text(enum error_code e)
{
    switch (e) {
        ERROR_LIST(AS_CASE)
        default: return "unknown error";
    }
}
#undef AS_CASE

/* ④ 개수 세기 — 목록의 길이도 자동으로 얻는다 */
#define AS_COUNT(name, code, text) + 1
enum { ERROR_COUNT = 0 ERROR_LIST(AS_COUNT) };
#undef AS_COUNT

int main(void)
{
    printf("%d error codes defined\n", ERROR_COUNT);

#define AS_ROW(name, code, text) \
    printf("  %-10s code=%d name=%-9s text=%s\n", #name, (int)ERR_##name, \
           error_name[code], error_text(ERR_##name));
    ERROR_LIST(AS_ROW)
#undef AS_ROW

    return 0;
}
