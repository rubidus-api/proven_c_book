/* 크기를 어긴 쓰기 ① --- 스택의 그릇 8바이트, 아홉째 칸에 쓴다. */
[[gnu::noinline]] static void put(char *p, int i) { p[i] = 'x'; }

int main(void)
{
    char buf[8];
    put(buf, 8);            /* buf[8] --- 끝 바로 다음 칸 */
    return buf[0];
}
