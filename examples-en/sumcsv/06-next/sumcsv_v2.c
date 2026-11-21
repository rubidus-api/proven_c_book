/* Version 2 grown by hand in plain C: multiline, column selection, quoted fields, and dynamic buffers. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdckdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dynbuf {
    char  *data;
    size_t len;
    size_t cap;
};

static bool buf_push(struct dynbuf *b, char ch)
{
    if (b->len + 1 >= b->cap) {
        size_t ncap = 0;
        if (b->cap == 0) {
            ncap = 32;
        } else if (ckd_mul(&ncap, b->cap, (size_t)2)) {
            return false;
        }
        char *nb = realloc(b->data, ncap);
        if (!nb) return false;
        b->data = nb;
        b->cap = ncap;
    }
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
    return true;
}

static int read_dyn_line(FILE *in, struct dynbuf *b, const char **why)
{
    b->len = 0;
    if (b->data) b->data[0] = '\0';

    int ch = fgetc(in);
    if (ch == EOF) {
        if (ferror(in)) { *why = "read error"; return -1; }
        return 0;
    }
    do {
        if (ch == '\0') { *why = "embedded NUL byte"; return -1; }
        if (!buf_push(b, (char)ch)) { *why = "out of memory"; return -1; }
        if (ch == '\n') break;
        ch = fgetc(in);
    } while (ch != EOF);

    if (ferror(in)) { *why = "read error"; return -1; }
    return 1;
}

static bool parse_i64_exact(const char *s, int64_t *out, const char **why)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') { *why = "empty cell"; return false; }

    const char *digits = s;
    if (*digits == '+' || *digits == '-') digits++;
    if (!isdigit((unsigned char)*digits)) { *why = "bad integer"; return false; }

    errno = 0;
    char *end = nullptr;
    long long v = strtoll(s, &end, 10);
    if (end == s) { *why = "bad integer"; return false; }
    if (errno == ERANGE) { *why = "integer out of range"; return false; }
#if LLONG_MIN < INT64_MIN || LLONG_MAX > INT64_MAX
    if (v < INT64_MIN || v > INT64_MAX) { *why = "integer out of range"; return false; }
#endif
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') { *why = "trailing garbage"; return false; }
    *out = (int64_t)v;
    return true;
}

static bool eval_row_col(const char *line, size_t target_col, struct dynbuf *cell,
                         int64_t *out_val, const char **why)
{
    const char *p = line;
    size_t col = 0;
    bool found = false;

    for (;;) {
        col++;
        cell->len = 0;
        if (cell->data) cell->data[0] = '\0';

        if (*p == '"') {
            p++;
            for (;;) {
                if (*p == '\0' || *p == '\n' || (*p == '\r' && p[1] == '\n')) {
                    *why = "unclosed quote";
                    return false;
                }
                if (*p == '"') {
                    if (p[1] == '"') {
                        if (!buf_push(cell, '"')) { *why = "out of memory"; return false; }
                        p += 2;
                        continue;
                    }
                    p++;
                    break;
                }
                if (!buf_push(cell, *p)) { *why = "out of memory"; return false; }
                p++;
            }
            while (*p == ' ' || *p == '\t') p++;
        } else {
            while (*p && *p != ',' && *p != '\n' && !(*p == '\r' && p[1] == '\n')) {
                if (*p == '"') { *why = "stray quote"; return false; }
                if (!buf_push(cell, *p)) { *why = "out of memory"; return false; }
                p++;
            }
        }

        if (col == target_col) {
            if (!parse_i64_exact(cell->data ? cell->data : "", out_val, why)) return false;
            found = true;
        }

        if (*p == ',') { p++; continue; }
        if (*p == '\r' && p[1] == '\n') p += 2;
        else if (*p == '\n') p++;
        if (*p != '\0') { *why = "unexpected char after field"; return false; }
        break;
    }

    if (!found) {
        *why = "missing target column";
        return false;
    }
    return true;
}

static bool sum_csv_stream(FILE *in, size_t target_col, int64_t *out_sum, const char **out_why)
{
    struct dynbuf line = {0};
    struct dynbuf cell = {0};
    int64_t acc = 0;
    size_t rows = 0;
    bool ok = false;

    for (;;) {
        int rc = read_dyn_line(in, &line, out_why);
        if (rc < 0) goto cleanup;
        if (rc == 0) break;

        rows++;
        int64_t v = 0;
        if (!eval_row_col(line.data, target_col, &cell, &v, out_why)) goto cleanup;
        if (ckd_add(&acc, acc, v)) { *out_why = "sum overflow"; goto cleanup; }
    }

    if (rows == 0) {
        *out_why = "empty input";
        goto cleanup;
    }
    *out_sum = acc;
    ok = true;

cleanup:
    free(line.data);
    free(cell.data);
    return ok;
}

static void run_case(const char *label, size_t col, const char *csv)
{
    FILE *fp = tmpfile();
    if (!fp) {
        printf("  %-18s -> err: tmpfile failed\n", label);
        return;
    }
    size_t n = strlen(csv);
    if (n > 0 && fwrite(csv, 1, n, fp) != n) {
        fclose(fp);
        printf("  %-18s -> err: write failed\n", label);
        return;
    }
    rewind(fp);

    int64_t sum = -1;
    const char *why = "";
    if (sum_csv_stream(fp, col, &sum, &why)) {
        printf("  %-18s col=%zu -> ok: %" PRId64 "\n", label, col, sum);
    } else {
        printf("  %-18s col=%zu -> err: %s (sum untouched: %" PRId64 ")\n",
               label, col, why, sum);
    }
    fclose(fp);
}

int main(void)
{
    run_case("quoted multiline", 2,
             "\"widget, small\",100\n\"bolt \"\"hex\"\"\",250\nplain,-30\r\n");
    run_case("unclosed quote",   2, "\"widget,100\n");
    run_case("missing col",      3, "a,10\n");
    run_case("bad target cell",  2, "\"ok,name\",abc\n");
    return 0;
}
