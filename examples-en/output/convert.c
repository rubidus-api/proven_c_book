/* The common conversion specifications, and lining things up with width and
   precision. Part IV has not met variables yet, so every argument here is a
   constant or an expression. */
#include <stdio.h>

int main(void)
{
    puts("[a different blank for each kind of value]");
    printf("  integer   %%d  -> %d\n", 42);
    printf("  real      %%f  -> %f\n", 3.14);
    printf("  character %%c  -> %c\n", 'A');
    printf("  string    %%s  -> %s\n", "hello");
    printf("  hex       %%x  -> %x\n", 255);
    printf("  percent   %%%%  -> %%\n");

    puts("\n[width — reserve the columns and things line up]");
    printf("  |%d|%d|\n", 7, 1234);
    printf("  |%6d|%6d|   <- six columns, right aligned\n", 7, 1234);
    printf("  |%-6d|%-6d|   <- a minus means left aligned\n", 7, 1234);
    printf("  |%06d|          <- a 0 fills the blanks with zeros\n", 42);

    puts("\n[precision — how many places after the point]");
    printf("  %%f    -> %f      <- six places by default\n", 2.0 / 3.0);
    printf("  %%.2f  -> %.2f          <- rounded to two\n", 2.0 / 3.0);
    printf("  %%8.2f -> |%8.2f|      <- width and precision together\n", 2.0 / 3.0);
    printf("  %%.3s  -> %.3s        <- on a string: 'how many characters'\n", "abcdef");

    puts("\n[making a table]");
    printf("  %-8s %6s\n", "item", "count");
    printf("  %-8s %6d\n", "apple", 3);
    printf("  %-8s %6d\n", "banana", 12);
    printf("  %-8s %6.1f\n", "weight", 4.25);

    puts("\n[but width counts BYTES — Hangul does not line up]");
    printf("  %-8s %6d\n", "사과", 3);
    printf("  %-8s %6d\n", "바나나", 12);
    puts("  one Hangul syllable is 3 bytes in UTF-8, so eight columns");
    puts("  are not eight characters.");

    puts("\n[the other windows for output]");
    puts("  puts prints one string and the newline for you");
    putchar(' '); putchar(' '); putchar('p'); putchar('c'); putchar('\n');
    fprintf(stdout, "  fprintf(stdout, ...) is the same as printf\n");
    fprintf(stderr, "  fprintf(stderr, ...) goes out on the error band\n");
    return 0;
}
