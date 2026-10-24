/* 자주 쓰는 변환 지정과, 폭·정밀도로 줄을 맞추는 법.
   제4부는 아직 변수를 배우기 전이라, 재료는 전부 상수와 수식이다. */
#include <stdio.h>

int main(void)
{
    puts("[each kind of value has its own conversion]");
    printf("  integer   %%d  -> %d\n", 42);
    printf("  float     %%f  -> %f\n", 3.14);
    printf("  char      %%c  -> %c\n", 'A');
    printf("  string    %%s  -> %s\n", "hello");
    printf("  hex       %%x  -> %x\n", 255);
    printf("  percent   %%%%  -> %%\n");

    puts("\n[width - reserve columns so things line up]");
    printf("  |%d|%d|\n", 7, 1234);
    printf("  |%6d|%6d|   <- six columns, right-aligned\n", 7, 1234);
    printf("  |%-6d|%-6d|   <- a minus sign left-aligns\n", 7, 1234);
    printf("  |%06d|          <- a leading 0 pads with zeros\n", 42);

    puts("\n[precision - how many digits after the point]");
    printf("  %%f    -> %f      <- six digits by default\n", 2.0 / 3.0);
    printf("  %%.2f  -> %.2f          <- rounded to two digits\n", 2.0 / 3.0);
    printf("  %%8.2f -> |%8.2f|      <- width and precision together\n", 2.0 / 3.0);
    printf("  %%.3s  -> %.3s        <- on a string it means 'the first N bytes'\n", "abcdef");

    puts("\n[this is how you lay out a table]");
    printf("  %-8s %6s\n", "item", "count");
    printf("  %-8s %6d\n", "apple", 3);
    printf("  %-8s %6d\n", "banana", 12);
    printf("  %-8s %6.1f\n", "weight", 4.25);

    puts("\n[but width counts BYTES - non-ASCII text drifts]");
    printf("  %-8s %6d\n", "사과", 3);
    printf("  %-8s %6d\n", "바나나", 12);
    puts("  a Hangul syllable is 3 bytes in UTF-8, so eight columns are not eight letters.");

    puts("\n[other ways out]");
    puts("  puts prints one string and adds the newline for you");
    putchar(' '); putchar(' '); putchar('p'); putchar('c'); putchar('\n');
    fprintf(stdout, "  fprintf(stdout, ...) is the same as printf\n");
    fprintf(stderr, "  fprintf(stderr, ...) goes out on the error stream\n");
    return 0;
}
