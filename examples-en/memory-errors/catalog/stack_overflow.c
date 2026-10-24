/* Breaking the size, writing (1) --- an 8-byte container on the stack, written at its ninth place. */
[[gnu::noinline]] static void put(char *p, int i) { p[i] = 'x'; }

int main(void)
{
    char buf[8];
    put(buf, 8);            /* buf[8] --- the place just past the end */
    return buf[0];
}
