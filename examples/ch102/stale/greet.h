/* 공개 계약 — 부르는 쪽이 아는 것은 이것뿐이다 */
#ifndef GREET_H
#define GREET_H

/* 인사말의 최대 길이. ★ 이 수가 바뀌면 부르는 쪽도 다시 만들어져야 한다 */
#define GREET_MAX 16

void greet(char *out);

#endif
