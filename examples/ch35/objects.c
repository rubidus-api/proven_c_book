#include <stdio.h>

int main(void)
{
    int n = 55;                 /* 객체 하나 --- 이름 n, 값 55 */
    int *p = &n;                /* 또 하나의 객체 --- 값은 n 의 주소 */

    puts("[1] every object has a size");
    printf("sizeof n = %zu bytes\n", sizeof n);
    printf("sizeof p = %zu bytes\n", sizeof p);

    puts("\n[2] p holds n's address; p itself lives somewhere else");
    printf("p  == &n : %d\n", p == &n);
    printf("&p == &n : %d\n", (void *)&p == (void *)&n); /* 다른 두 객체 */

    puts("\n[3] two roads to one object: the name n and *p");
    *p = 77;                    /* p 에 적힌 주소로 찾아가 쓴다 */
    printf("after *p = 77 : n  = %d\n", n);
    n = 99;                     /* 이름으로 쓴다 --- 같은 객체다 */
    printf("after n = 99  : *p = %d\n", *p);

    puts("\n[4] a pointer is a variable: it can be pointed elsewhere");
    int m = 1;
    p = &m;                     /* 이제 p 는 m 을 가리킨다 */
    *p = 2;
    printf("after p = &m; *p = 2 : n = %d, m = %d\n", n, m);

    puts("\n[5] a pointer is an object too, so it has an address");
    int **pp = &p;              /* 「int 포인터」를 가리키는 포인터 */
    **pp = 3;                   /* pp → p → m */
    printf("after **pp = 3 : m = %d\n", m);
    return 0;
}
