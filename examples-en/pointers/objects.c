#include <stdio.h>

int main(void)
{
    int n = 55;                 /* one object: name n, value 55 */
    int *p = &n;                /* another object: value n's address */

    puts("[1] every object has a size");
    printf("sizeof n = %zu bytes\n", sizeof n);
    printf("sizeof p = %zu bytes\n", sizeof p);

    puts("\n[2] p holds n's address; p itself lives somewhere else");
    printf("p  == &n : %d\n", p == &n);
    printf("&p == &n : %d\n", (void *)&p == (void *)&n); /* two objects */

    puts("\n[3] two roads to one object: the name n and *p");
    *p = 77;                    /* go to the address written in p and write */
    printf("after *p = 77 : n  = %d\n", n);
    n = 99;                     /* write by the name --- the same object */
    printf("after n = 99  : *p = %d\n", *p);

    puts("\n[4] a pointer is a variable: it can be pointed elsewhere");
    int m = 1;
    p = &m;                     /* now p points at m */
    *p = 2;
    printf("after p = &m; *p = 2 : n = %d, m = %d\n", n, m);

    puts("\n[5] a pointer is an object too, so it has an address");
    int **pp = &p;              /* a pointer to "a pointer to int" */
    **pp = 3;                   /* pp → p → m */
    printf("after **pp = 3 : m = %d\n", m);
    return 0;
}
