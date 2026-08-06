/* What main receives and what it leaves behind */
#include <stdio.h>
#include <stdlib.h>

/* a function registered to be called when the program ends */
static void farewell(void)
{
    puts("  atexit: the finishing touch registered earlier runs here");
}

int main(int argc, char *argv[])
{
    atexit(farewell);

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
        printf("  argv[%d] = \"%s\"\n", i, argv[i]);

    /* the standard promises it: argv[argc] is always a null pointer */
    printf("is argv[argc] null? %s\n", argv[argc] == nullptr ? "yes" : "no");

    printf("EXIT_SUCCESS = %d, EXIT_FAILURE = %d\n", EXIT_SUCCESS, EXIT_FAILURE);

    /* the three ways of reporting success all mean the same thing */
    return EXIT_SUCCESS;        /* == return 0; == (since C99) just running off the end */
}
