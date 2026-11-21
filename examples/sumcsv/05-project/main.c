/* 명령줄 진입점 --- 입출력 경계 검사와 종료 상태만 맡는다. */
#include "sumcsv.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { LINE_CAP = 4096 };

static bool read_single_line(FILE *in, char *buf, size_t cap, const char **out_why)
{
    if (!fgets(buf, (int)cap, in)) {
        *out_why = ferror(in) ? "read error" : "empty input";
        return false;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        int extra = fgetc(in);
        if (ferror(in)) { *out_why = "read error"; return false; }
        if (extra != EOF) { *out_why = "extra line"; return false; }
        return true;
    }
    if (len == cap - 1) {
        int next = fgetc(in);
        if (ferror(in)) { *out_why = "read error"; return false; }
        if (next != EOF) { *out_why = "line too long"; return false; }
    }
    return true;
}

int main(void)
{
    char line[LINE_CAP];
    const char *why = "";
    if (!read_single_line(stdin, line, sizeof line, &why)) {
        fprintf(stderr, "sumcsv: %s\n", why);
        return 1;
    }

    struct sumcsv_res res = sumcsv_sum_line(line);
    if (res.err != SUMCSV_OK) {
        fprintf(stderr, "sumcsv: %s\n", sumcsv_err_str(res.err));
        return 1;
    }

    if (fprintf(stdout, "%" PRId64 "\n", res.sum) < 0 ||
        fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "sumcsv: write error\n");
        return 1;
    }
    return 0;
}
