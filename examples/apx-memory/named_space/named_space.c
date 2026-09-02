/* 이름 붙은 주소 공간을 눈으로 --- AVR 의 `__flash`.

   이 파일은 이 기계(x86-64)에서 도는 프로그램이 아니다. *컴파일되는 것 자체가
   실험*이라, 옆의 `run.sh` 가 AVR 교차 컴파일러로 이 파일을 세 번 짓고 그때
   나온 것을 보인다.

   보이려는 것 셋.
     ① `const __flash` 는 선언에 붙는 *타입 한정자*다 --- `const` 와 같은 자리.
     ② 그런데 표준 C 의 낱말이 아니다. `-std=c23`(엄격 ISO)에서는 이 파일이
        아예 컴파일되지 않는다. `-std=gnu23` 이라야 선다.
     ③ 같은 첨자 `x[i]` 인데 어느 공간의 것이냐에 따라 *다른 기계 명령*이 나가고,
        자료도 다른 구역에 놓인다.                                             */

const char         ram[] = "hi";      /* RAM(.rodata) 에 놓인다              */
const __flash char rom[] = "hi";      /* 플래시(.progmem.data) 에 놓인다      */

char from_ram(int i) { return ram[i]; }
char from_rom(int i) { return rom[i]; }

/* ★ 그리고 이것 --- 공간이 다른 두 포인터를 섞는다.
      한쪽은 RAM 을, 다른 쪽은 플래시를 가리키는데 그냥 대입된다.            */
const char         *p;
const __flash char *q;
void mix(void) { p = q; }
