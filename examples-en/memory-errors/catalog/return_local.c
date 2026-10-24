/* Breaking the lifetime, reading --- receive the address of an automatic object that has ended, and read it. */
[[gnu::noinline]] static int *make(void)
{
    int v = 7;
    int *volatile p = &v;   /* hide the address from the compiler's warning --- hidden, it is still outside the contract */
    return p;
}

int main(void)
{
    int *p = make();
    return *p;
}
