/* 2바이트 조합형 한글 — 한 워드를 초성·중성·종성으로 쪼갠 실제 사례.
   (유니코드 설명이 목적이므로 이 예제에는 한글이 들어간다.) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* 조합형 낱자 이름표. 코드값 0·1 은 채움/미사용이라 비워 둔다. */
static const char *const CHO[32] = {
    [2]="G", [3]="GG", [4]="N", [5]="D", [6]="DD", [7]="R", [8]="M",
    [9]="B", [10]="BB", [11]="S", [12]="SS", [13]="NG", [14]="J",
    [15]="JJ", [16]="C", [17]="K", [18]="T", [19]="P", [20]="H",
};
static const char *const JUNG[32] = {
    [3]="A", [4]="AE", [5]="YA", [6]="YAE", [7]="EO",
    [10]="E", [11]="YEO", [12]="YE",
    [13]="O", [14]="WA", [15]="WAE",
    [18]="OE", [19]="YO", [20]="U", [21]="WEO", [22]="WE", [23]="WI",
    [26]="YU", [27]="EU", [28]="YI", [29]="I",
};
static const char *const JONG[32] = {
    [1]="(none)", [2]="G", [5]="N", [9]="L", [17]="M", [19]="B",
    [21]="S", [23]="NG", [29]="H",
};

/* ① 시프트와 마스크로 뽑는다 — 표준이 정확히 약속하는 길 */
static void split_by_shift(uint16_t w)
{
    unsigned cho  = (w >> 10) & 0x1f;
    unsigned jung = (w >>  5) & 0x1f;
    unsigned jong =  w        & 0x1f;
    printf("  시프트/마스크: 표시비트=%u 초성=%2u(%-2s) 중성=%2u(%-3s) 종성=%2u(%s)\n",
           (unsigned)(w >> 15), cho, CHO[cho] ? CHO[cho] : "?",
           jung, JUNG[jung] ? JUNG[jung] : "?",
           jong, JONG[jong] ? JONG[jong] : "?");
}

/* ② 비트 필드 + 공용체로 본다 — 편하지만 배치는 구현이 정한다 */
union johab {
    uint16_t raw;
    struct {
        uint16_t jong : 5;   /* 이 순서가 표준의 약속이 아니다 */
        uint16_t jung : 5;
        uint16_t cho  : 5;
        uint16_t mark : 1;
    } f;
};

int main(void)
{
    /* "가" 와 "한" 의 조합형 코드 — 실제 표에서 가져온 값이다. */
    const uint16_t GA  = 0x8861;   /* 1 00010 00011 00001 */
    const uint16_t HAN = 0xD065;   /* 1 10100 00011 00101 */

    printf("가 = 0x%04X, 한 = 0x%04X\n\n", GA, HAN);
    puts("가:");   split_by_shift(GA);
    puts("한:");   split_by_shift(HAN);

    union johab u = { .raw = GA };
    printf("\n비트 필드로 본 '가': mark=%u 초성=%u 중성=%u 종성=%u\n",
           u.f.mark, u.f.cho, u.f.jung, u.f.jong);
    puts("  (이 컴파일러에서는 시프트/마스크와 같은 값이 나왔다.");
    puts("   비트를 낮은 자리부터 채우는 구현이라 그렇고, 표준의 약속은 아니다.)");

    /* ③ 둘째 바이트가 ASCII 와 겹친다 — 조합형의 유명한 함정 */
    unsigned char bytes[3] = { (unsigned char)(GA >> 8),
                               (unsigned char)(GA & 0xff), 0 };
    printf("\n'가' 의 두 바이트: %02X %02X\n", bytes[0], bytes[1]);
    printf("  둘째 바이트 0x%02X 는 ASCII '%c' 다 — 바이트 단위로 'a' 를 찾으면\n",
           bytes[1], bytes[1]);
    printf("  글자 속을 잘못 짚는다: strchr 결과 = %s\n",
           strchr((char *)bytes, 'a') ? "찾음(오검출)" : "못 찾음");
    return 0;
}
