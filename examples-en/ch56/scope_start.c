/* A scope begins where the declarator ends --- before the equals sign, not after. */
#include <stdio.h>

typedef int meter;          /* a type name at file scope */

int main(void)
{
    /* i's scope has already begun *before* the = is reached, so sizeof(i)
       refers to the i just declared (and does not read its value). */
    int i = sizeof(i);
    printf("int i = sizeof(i);  -> %d\n", i);

    /* a pointer holding its own address works for the same reason */
    void *self = &self;
    printf("void *self = &self; -> self == &self is %s\n",
           self == (void *)&self ? "true" : "false");

    {
        /* Here meter is a type name. After the line below, meter is a
           *variable* in this block --- from where the declarator ended. */
        meter meter = 42;
        printf("meter meter = 42;   -> %d  (the type name is now shadowed)\n", meter);
    }

    /* outside the block, meter is a type name again */
    meter distance = 7;
    printf("outside the block, meter is a type again -> %d\n", distance);

    /* a name declared in for's first slot is gone when the loop ends */
    for (int n = 0; n < 3; n++) { /* n lives only here */ }
    /* out here n is not a name --- reusing it means nothing to the first one */
    for (int n = 10; n > 8; n--) { /* unrelated to the n above */ }
    puts("\ntwo loops used the name n; neither knows the other");
    return 0;
}
