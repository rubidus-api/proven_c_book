#include <math.h>
#include <stdio.h>

/* 절대 오차: 0 근처의 비교에 쓴다 */
bool near_abs(double a, double b, double eps)
{
    return fabs(a - b) < eps;
}

/* 상대 오차: 값이 커지면 눈금도 커지므로 크기에 비례한 허용치를 쓴다 */
bool near_rel(double a, double b, double rel)
{
    double scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return fabs(a - b) <= rel * scale;
}

int main(void)
{
    double sum = 0.1 + 0.2;

    printf("0.1 + 0.2 == 0.3 ?      %d\n", sum == 0.3);
    printf("절대 오차로 비교하면?   %d\n", near_abs(sum, 0.3, 1e-9));
    printf("%.20f\n", sum);
    printf("%.20f\n", 0.3);

    double big = 1e16;
    printf("1e16 + 1 == 1e16 ?      %d  (눈금이 1보다 넓다)\n", big + 1 == big);
    printf("상대 오차로 비교하면?   %d\n", near_rel(big + 1, big, 1e-12));
    return 0;
}
