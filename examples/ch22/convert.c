/* 자주 쓰는 변환 지정과, 폭·정밀도로 줄을 맞추는 법.
   제4부는 아직 변수를 배우기 전이라, 재료는 전부 상수와 수식이다. */
#include <stdio.h>

int main(void)
{
    puts("[값의 종류마다 빈칸이 다르다]");
    printf("  정수      %%d  -> %d\n", 42);
    printf("  실수      %%f  -> %f\n", 3.14);
    printf("  글자      %%c  -> %c\n", 'A');
    printf("  문자열    %%s  -> %s\n", "hello");
    printf("  16진수    %%x  -> %x\n", 255);
    printf("  퍼센트    %%%%  -> %%\n");

    puts("\n[폭 — 자리를 미리 잡아 줄을 맞춘다]");
    printf("  |%d|%d|\n", 7, 1234);
    printf("  |%6d|%6d|   ← 여섯 칸을 잡고 오른쪽 맞춤\n", 7, 1234);
    printf("  |%-6d|%-6d|   ← 빼기를 붙이면 왼쪽 맞춤\n", 7, 1234);
    printf("  |%06d|          ← 0 을 붙이면 빈자리를 0 으로\n", 42);

    puts("\n[정밀도 — 소수점 아래 몇 자리까지]");
    printf("  %%f    -> %f      ← 기본은 여섯 자리\n", 2.0 / 3.0);
    printf("  %%.2f  -> %.2f          ← 두 자리에서 반올림\n", 2.0 / 3.0);
    printf("  %%8.2f -> |%8.2f|      ← 폭과 정밀도를 함께\n", 2.0 / 3.0);
    printf("  %%.3s  -> %.3s        ← 문자열에 쓰면 '앞에서 몇 글자'\n", "abcdef");

    puts("\n[표를 만들면 이렇게 쓴다]");
    printf("  %-8s %6s\n", "item", "count");
    printf("  %-8s %6d\n", "apple", 3);
    printf("  %-8s %6d\n", "banana", 12);
    printf("  %-8s %6.1f\n", "weight", 4.25);

    puts("\n[다만 폭은 '바이트' 로 센다 — 한글은 어긋난다]");
    printf("  %-8s %6d\n", "사과", 3);
    printf("  %-8s %6d\n", "바나나", 12);
    puts("  한글 한 글자가 UTF-8 로 3바이트라, 여덟 칸이 여덟 글자가 아니다.");

    puts("\n[출력의 다른 창구]");
    puts("  puts 는 문자열 하나를 찍고 줄바꿈까지 해 준다");
    putchar(' '); putchar(' '); putchar('p'); putchar('c'); putchar('\n');
    fprintf(stdout, "  fprintf(stdout, ...) 는 printf 와 같다\n");
    fprintf(stderr, "  fprintf(stderr, ...) 는 오류용 띠로 나간다\n");
    return 0;
}
