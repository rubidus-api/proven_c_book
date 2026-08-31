/* 덤프에 무엇이 들어 있는가 --- 프로세스가 제 기억을 스스로 들여다본다.
   코어 덤프가 하는 일이 정확히 이것이다: 이 목록과 이 바이트들을 파일에 적는 것. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int global_zero; int global_init = 0x41424344;
int main(void){
    char *heap = malloc(4096); strcpy(heap, "heap");
    char stackbuf[64]; strcpy(stackbuf, "stack");
    printf("code=%p  rodata=%p  data=%p  bss=%p  heap=%p  stack=%p\n",
           (void*)main, (void*)"ro", (void*)&global_init, (void*)&global_zero, (void*)heap, (void*)stackbuf);
    FILE *f=fopen("/proc/self/maps","r"); char line[512]; int n=0;
    puts("\n--- /proc/self/maps (first 8 lines) ---");
    while (fgets(line,sizeof line,f) && n++<8) fputs(line,stdout);
    fclose(f);
    /* 제 기억을 스스로 읽어 본다 --- 덤프가 하는 일이 이것이다 */
    FILE *m=fopen("/proc/self/mem","rb");
    if (m){ fseek(m,(long)(size_t)&global_init,SEEK_SET); unsigned v=0;
        size_t got=fread(&v,1,4,m);
        printf("\nread through /proc/self/mem at &global_init = 0x%08X (%zu bytes)\n", v, got);
        fclose(m); }
    free(heap); return 0;
}
