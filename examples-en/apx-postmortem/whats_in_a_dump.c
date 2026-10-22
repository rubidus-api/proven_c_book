/* What is in a dump --- a process looks into its own memory.
   This is exactly what a core dump does: write this list and these bytes to a file. */
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
    /* read our own memory --- this is what a dump does */
    FILE *m=fopen("/proc/self/mem","rb");
    if (m){ fseek(m,(long)(size_t)&global_init,SEEK_SET); unsigned v=0;
        size_t got=fread(&v,1,4,m);
        printf("\nread through /proc/self/mem at &global_init = 0x%08X (%zu bytes)\n", v, got);
        fclose(m); }
    free(heap); return 0;
}
