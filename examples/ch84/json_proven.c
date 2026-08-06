/* 간이 JSON — proven 판. 같은 문법을 읽고 쓰되, 그릇과 실패를 계약으로 다룬다. */
#include <proven.h>
#include <stdio.h>

typedef enum { J_STR, J_NUM, J_BOOL, J_NULL } jkind;

/* 값은 *빌려 본다* — 원문 버퍼 안을 가리키는 view 라서 복사가 없다.
   길이를 함께 들고 다니므로 NUL 도, 잘림도 없다. */
typedef struct {
    proven_u8str_view_t key;
    jkind               kind;
    proven_u8str_view_t str;   /* J_STR */
    long long           num;   /* J_NUM */
    bool                boolean;
} jpair;

typedef struct {
    jpair       *pair;     /* 아레나에서 얻는다 */
    proven_size_t count;
    proven_size_t cap;
} jdoc;

/* 실패는 값으로 돌려준다. 어디서 멈췄는지도 함께. */
typedef struct {
    proven_err_t  err;
    proven_size_t at;      /* 오류 위치(바이트 오프셋) */
} jresult;

static bool is_ws(proven_byte_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static proven_size_t skip_ws(proven_u8str_view_t t, proven_size_t i)
{
    while (i < t.size && is_ws(t.ptr[i])) i++;
    return i;
}

static bool lit_at(proven_u8str_view_t t, proven_size_t i, proven_u8str_view_t lit)
{
    if (t.size - i < lit.size) return false;
    for (proven_size_t k = 0; k < lit.size; k++)
        if (t.ptr[i + k] != lit.ptr[k]) return false;
    return true;
}

/* 따옴표 문자열을 *잘라 가리킨다* — 담을 그릇이 없으니 넘칠 일도 없다. */
static jresult take_string(proven_u8str_view_t t, proven_size_t *i,
                           proven_u8str_view_t *out)
{
    if (*i >= t.size || t.ptr[*i] != '"')
        return (jresult){ PROVEN_ERR_INVALID_FORMAT, *i };
    proven_size_t start = ++(*i);
    while (*i < t.size && t.ptr[*i] != '"') {
        if (t.ptr[*i] == '\\' && *i + 1 < t.size) (*i)++;
        (*i)++;
    }
    if (*i >= t.size) return (jresult){ PROVEN_ERR_INVALID_FORMAT, *i };
    *out = (proven_u8str_view_t){ .ptr = t.ptr + start, .size = *i - start };
    (*i)++;
    return (jresult){ PROVEN_OK, *i };
}

/* 정수는 넘침을 검사하며 모은다 — 조용히 감기지 않는다. */
static jresult take_number(proven_u8str_view_t t, proven_size_t *i, long long *out)
{
    proven_size_t start = *i;
    bool neg = false;
    if (*i < t.size && (t.ptr[*i] == '-' || t.ptr[*i] == '+')) {
        neg = t.ptr[*i] == '-';
        (*i)++;
    }
    if (*i >= t.size || t.ptr[*i] < '0' || t.ptr[*i] > '9')
        return (jresult){ PROVEN_ERR_INVALID_FORMAT, start };

    long long v = 0;
    while (*i < t.size && t.ptr[*i] >= '0' && t.ptr[*i] <= '9') {
        int d = t.ptr[*i] - '0';
        if (PROVEN_CKD_MUL(&v, v, 10LL) || PROVEN_CKD_ADD(&v, v, (long long)d))
            return (jresult){ PROVEN_ERR_OVERFLOW, start };
        (*i)++;
    }
    *out = neg ? -v : v;
    return (jresult){ PROVEN_OK, *i };
}

static jresult json_parse(proven_u8str_view_t text, proven_arena_t *arena,
                          proven_size_t cap, jdoc *out)
{
    /* 자리 수 × 원소 크기 — 곱셈부터 검사한다 */
    proven_size_t bytes;
    if (PROVEN_CKD_MUL(&bytes, cap, sizeof(jpair)))
        return (jresult){ PROVEN_ERR_OVERFLOW, 0 };
    proven_result_mem_mut_t room = proven_arena_alloc(arena, bytes);
    if (!proven_is_ok(room.err)) return (jresult){ room.err, 0 };

    *out = (jdoc){ .pair = (jpair *)room.value.ptr, .count = 0, .cap = cap };

    proven_size_t i = skip_ws(text, 0);
    if (i >= text.size || text.ptr[i] != '{')
        return (jresult){ PROVEN_ERR_INVALID_FORMAT, i };
    i = skip_ws(text, i + 1);
    if (i < text.size && text.ptr[i] == '}') return (jresult){ PROVEN_OK, i };

    for (;;) {
        if (out->count == out->cap) return (jresult){ PROVEN_ERR_NOMEM, i };
        jpair *e = &out->pair[out->count];

        jresult r = take_string(text, &i, &e->key);
        if (!proven_is_ok(r.err)) return r;
        i = skip_ws(text, i);
        if (i >= text.size || text.ptr[i] != ':')
            return (jresult){ PROVEN_ERR_INVALID_FORMAT, i };
        i = skip_ws(text, i + 1);

        if (i < text.size && text.ptr[i] == '"') {
            e->kind = J_STR;
            r = take_string(text, &i, &e->str);
            if (!proven_is_ok(r.err)) return r;
        } else if (lit_at(text, i, PROVEN_LIT("true"))) {
            e->kind = J_BOOL; e->boolean = true;  i += 4;
        } else if (lit_at(text, i, PROVEN_LIT("false"))) {
            e->kind = J_BOOL; e->boolean = false; i += 5;
        } else if (lit_at(text, i, PROVEN_LIT("null"))) {
            e->kind = J_NULL; i += 4;
        } else {
            e->kind = J_NUM;
            r = take_number(text, &i, &e->num);
            if (!proven_is_ok(r.err)) return r;
        }

        out->count++;
        i = skip_ws(text, i);
        if (i < text.size && text.ptr[i] == ',') { i = skip_ws(text, i + 1); continue; }
        if (i < text.size && text.ptr[i] == '}') return (jresult){ PROVEN_OK, i };
        return (jresult){ PROVEN_ERR_INVALID_FORMAT, i };
    }
}

/* 쓰기는 자라는 문자열에 붙인다 — 잘림이 아니라 실패가 온다. */
static proven_err_t json_write(const jdoc *doc, proven_u8str_t *s)
{
    proven_err_t e = proven_u8str_append(s, PROVEN_LIT("{"));
    for (proven_size_t i = 0; proven_is_ok(e) && i < doc->count; i++) {
        const jpair *p = &doc->pair[i];
        if (i) e = proven_u8str_append(s, PROVEN_LIT(","));
        if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("\""));
        if (proven_is_ok(e)) e = proven_u8str_append(s, p->key);
        if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("\":"));
        if (!proven_is_ok(e)) break;
        switch (p->kind) {
        case J_STR:
            e = proven_u8str_append(s, PROVEN_LIT("\""));
            if (proven_is_ok(e)) e = proven_u8str_append(s, p->str);
            if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("\""));
            break;
        case J_NUM: {
            char tmp[32];
            int n = snprintf(tmp, sizeof tmp, "%lld", p->num);
            e = proven_u8str_append(s, (proven_u8str_view_t){
                    .ptr = (const proven_byte_t *)tmp, .size = (proven_size_t)n });
            break;
        }
        case J_BOOL:
            e = proven_u8str_append(s, p->boolean ? PROVEN_LIT("true")
                                                  : PROVEN_LIT("false"));
            break;
        case J_NULL:
            e = proven_u8str_append(s, PROVEN_LIT("null"));
            break;
        }
    }
    if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("}"));
    return e;
}

static void show(const jdoc *doc)
{
    printf("pairs: %zu\n", (size_t)doc->count);
    for (proven_size_t i = 0; i < doc->count; i++) {
        const jpair *p = &doc->pair[i];
        printf("  %-6.*s = ", (int)p->key.size, (const char *)p->key.ptr);
        switch (p->kind) {
        case J_STR:  printf("\"%.*s\"\n", (int)p->str.size, (const char *)p->str.ptr); break;
        case J_NUM:  printf("%lld\n", p->num);                                         break;
        case J_BOOL: printf("%s\n", p->boolean ? "true" : "false");                    break;
        case J_NULL: printf("null\n");                                                 break;
        }
    }
}

int main(void)
{
    static proven_byte_t backing[4096];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing, .size = sizeof backing });
    proven_allocator_t alloc = proven_arena_as_allocator(&arena);

    proven_u8str_view_t text = PROVEN_LIT(
        "{ \"name\": \"proven\", \"year\": 2026, \"draft\": true, \"note\": null }");

    jdoc doc;
    jresult r = json_parse(text, &arena, 16, &doc);
    if (!proven_is_ok(r.err)) {
        printf("parse failed at byte %zu (err %d)\n", (size_t)r.at, (int)r.err);
        return 1;
    }
    show(&doc);

    proven_result_u8str_t made = proven_u8str_create(alloc, 256);
    if (!proven_is_ok(made.err)) return 1;
    proven_u8str_t out = made.value;
    if (proven_is_ok(json_write(&doc, &out))) {
        proven_u8str_view_t v = proven_u8str_as_view(&out);
        printf("\nwritten: %.*s\n", (int)v.size, (const char *)v.ptr);
    }

    /* 한계는 조용히 넘어가지 않는다 — 값으로 온다 */
    jdoc small;
    proven_u8str_view_t two = PROVEN_LIT("{\"a\":1,\"b\":2,\"c\":3}");
    jresult tight = json_parse(two, &arena, 2, &small);
    printf("cap 2 for 3 pairs -> err %d at byte %zu (refused, not cut)\n",
           (int)tight.err, (size_t)tight.at);

    proven_u8str_view_t huge = PROVEN_LIT("{\"n\":999999999999999999999}");
    jresult over = json_parse(huge, &arena, 4, &small);
    printf("overflowing number -> err %d at byte %zu (refused, not wrapped)\n",
           (int)over.err, (size_t)over.at);
    return 0;
}
