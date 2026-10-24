// 두 파일이 같은 함수를 서로 다르게 알고 있다.
//
// 경고: 이 프로그램은 미정의 동작이다. 컴파일러도 링커도 아무 말을 하지
// 않는다 --- 각자 자기 파일만 보기 때문이다. 아래 출력은 "옳은 결과"가
// 아니라 이 기계의 호출 규약에서 우연히 맞아떨어진 모습일 뿐이다.
#include <stdio.h>

// 정의는 int 둘인데 여기서는 long 둘이라고 선언했다.
void report(long id, long value);

int main(void)
{
    puts("calling a function this file has mis-declared:");
    report(1, 2);
    puts("it linked, it ran, and it even looks right --- that is the danger.");
    return 0;
}
