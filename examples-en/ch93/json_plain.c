/* A tiny JSON — the plain C edition. Reads one flat object and writes it back.
   Values are narrowed to four kinds: string, integer, boolean, null. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PAIRS   16
#define MAX_KEY     32
#define MAX_STR    128

typedef enum { J_STR, J_NUM, J_BOOL, J_NULL } jkind;

typedef struct {
    char  key[MAX_KEY];
    jkind kind;
    char  str[MAX_STR];   /* only when J_STR */
    long  num;            /* only when J_NUM */
    bool  boolean;        /* only when J_BOOL */
} jpair;

typedef struct {
    jpair pair[MAX_PAIRS];
    int   count;
} jdoc;

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Copy one quoted string into dst. Returns the next position, or NULL on failure. */
static const char *take_string(const char *p, char *dst, size_t cap)
{
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\') {                 /* escapes: the bare minimum */
            p++;
            if (*p == '\0') return NULL;
        }
        if (i + 1 < cap) dst[i++] = *p;   /* overflow is dropped silently — a trap */
        p++;
    }
    if (*p != '"') return NULL;
    dst[i] = '\0';
    return p + 1;
}

/* Returns 0 on success, -1 on failure with the reason written into err. */
static int json_parse(const char *text, jdoc *out, char *err, size_t errcap)
{
    out->count = 0;
    const char *p = skip_ws(text);
    if (*p != '{') { snprintf(err, errcap, "object expected"); return -1; }
    p = skip_ws(p + 1);
    if (*p == '}') return 0;

    for (;;) {
        if (out->count >= MAX_PAIRS) {
            snprintf(err, errcap, "too many pairs (max %d)", MAX_PAIRS);
            return -1;
        }
        jpair *e = &out->pair[out->count];

        p = take_string(p, e->key, sizeof e->key);
        if (!p) { snprintf(err, errcap, "key expected"); return -1; }
        p = skip_ws(p);
        if (*p != ':') { snprintf(err, errcap, "':' expected"); return -1; }
        p = skip_ws(p + 1);

        if (*p == '"') {
            e->kind = J_STR;
            p = take_string(p, e->str, sizeof e->str);
            if (!p) { snprintf(err, errcap, "string expected"); return -1; }
        } else if (strncmp(p, "true", 4) == 0) {
            e->kind = J_BOOL; e->boolean = true;  p += 4;
        } else if (strncmp(p, "false", 5) == 0) {
            e->kind = J_BOOL; e->boolean = false; p += 5;
        } else if (strncmp(p, "null", 4) == 0) {
            e->kind = J_NULL; p += 4;
        } else {
            char *end;
            long v = strtol(p, &end, 10);   /* overflow only shows in errno — easy to forget */
            if (end == p) { snprintf(err, errcap, "value expected"); return -1; }
            e->kind = J_NUM; e->num = v; p = end;
        }

        out->count++;
        p = skip_ws(p);
        if (*p == ',') { p = skip_ws(p + 1); continue; }
        if (*p == '}') return 0;
        snprintf(err, errcap, "',' or '}' expected");
        return -1;
    }
}

/* Write it back out. If it does not fit, it goes out cut — snprintf's contract. */
static int json_write(const jdoc *doc, char *buf, size_t cap)
{
    size_t used = 0;
    int n = snprintf(buf + used, cap - used, "{");
    if (n < 0) return -1;
    used += (size_t)n;

    for (int i = 0; i < doc->count; i++) {
        const jpair *e = &doc->pair[i];
        n = snprintf(buf + used, cap - used, "%s\"%s\":", i ? "," : "", e->key);
        if (n < 0 || (size_t)n >= cap - used) return -1;
        used += (size_t)n;
        switch (e->kind) {
        case J_STR:  n = snprintf(buf + used, cap - used, "\"%s\"", e->str); break;
        case J_NUM:  n = snprintf(buf + used, cap - used, "%ld", e->num);    break;
        case J_BOOL: n = snprintf(buf + used, cap - used, "%s",
                                  e->boolean ? "true" : "false");            break;
        case J_NULL: n = snprintf(buf + used, cap - used, "null");           break;
        }
        if (n < 0 || (size_t)n >= cap - used) return -1;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "}");
    if (n < 0 || (size_t)n >= cap - used) return -1;
    return 0;
}

int main(void)
{
    const char *text =
        "{ \"name\": \"proven\", \"year\": 2026, \"draft\": true, \"note\": null }";

    jdoc doc;
    char err[64];
    if (json_parse(text, &doc, err, sizeof err) != 0) {
        printf("parse failed: %s\n", err);
        return 1;
    }

    printf("pairs: %d\n", doc.count);
    for (int i = 0; i < doc.count; i++) {
        const jpair *e = &doc.pair[i];
        printf("  %-6s = ", e->key);
        switch (e->kind) {
        case J_STR:  printf("\"%s\"\n", e->str);                       break;
        case J_NUM:  printf("%ld\n", e->num);                          break;
        case J_BOOL: printf("%s\n", e->boolean ? "true" : "false");    break;
        case J_NULL: printf("null\n");                                 break;
        }
    }

    char out[256];
    if (json_write(&doc, out, sizeof out) == 0) printf("\nwritten: %s\n", out);
    else                                        printf("\nwrite failed (buffer too small)\n");

    /* Show the limit — a value longer than the container is cut silently */
    jdoc big;
    const char *long_text =
        "{ \"k\": \"0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789\" }";
    if (json_parse(long_text, &big, err, sizeof err) == 0)
        printf("long value: kept %zu of %zu characters (silently cut)\n",
               strlen(big.pair[0].str), strlen(long_text) - 12);
    return 0;
}
