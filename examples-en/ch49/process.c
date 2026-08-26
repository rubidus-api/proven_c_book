// A process is born anew on every run --- nothing of the last one remains.
#include <stdio.h>
#include <stdlib.h>

// A value the whole program owns. It looks as if the last run's value should
// still be there, but when the program ends the operating system takes back the memory it sat in.
static int run_count = 0;

static void at_the_end(void)
{
    puts("  the process is ending --- everything it owned goes back");
}

int main(int argc, char **argv)
{
    atexit(at_the_end);          // register a function to call when the process ends

    run_count += 1;
    // argv[0] is the name this program was invoked by. It differs from place to
    // place, so here we only look at whether it is there.
    printf("the program received %d argument(s), and argv[0] is %s\n",
           argc, (argc > 0 && argv[0] != NULL) ? "present" : "absent");
    printf("  run_count in this process: %d\n", run_count);
    puts("  run it again and it prints 1 again --- a new process starts fresh");
    return 0;
}
