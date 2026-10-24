/* 엄격한 앨리어싱 --- int 와 float 의 좌변값이 같은 자리를 가리키면 어떻게 되는가.
 * store_both 의 두 인자는 *서로 다른 타입*이다. 컴파일러는 「int 객체와 float 객체는
 * 겹치지 않는다」고 읽어도 된다(C23 6.5.1p7) --- 그 전제로 *ip 를 다시 읽지 않는다. */
#include <stdio.h>

[[gnu::noinline]] static int store_both(int *ip, float *fp)
{
    *ip = 1;
    *fp = 0.0f;
    return *ip;             /* 컴파일러: 「float 쓰기는 int 를 바꾸지 못한다 → 1」 */
}

int main(void)
{
    int x = 42;
    int r = store_both(&x, (float *)&x);    /* 계약 밖: int 객체를 float 로 쓴다 */
    printf("returned %d, x is now %d\n", r, x);
    return 0;
}
