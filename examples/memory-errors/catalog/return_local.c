/* 수명을 어긴 읽기 --- 이미 끝난 자동 객체의 주소를 돌려받아 읽는다. */
[[gnu::noinline]] static int *make(void)
{
    int v = 7;
    int *volatile p = &v;   /* 컴파일러 경고를 피해 주소를 숨긴다 --- 숨겨도 계약 밖이다 */
    return p;
}

int main(void)
{
    int *p = make();
    return *p;
}
