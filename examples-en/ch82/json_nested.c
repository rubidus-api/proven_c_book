/* A tiny JSON — the extended proven edition. Reads and writes nested objects
   and arrays. No recursion: both parsing and output are loops driven by an
   *explicit stack*. Every part comes from proven — the arena (a lump of
   lifetime), the pool (recycled fixed-size nodes), the intrusive list (linking
   children), the dynamic array (the stack), checked arithmetic (depth, count). */
#include <proven.h>
#include <stdio.h>

typedef enum { J_OBJ, J_ARR, J_STR, J_NUM, J_BOOL, J_NULL } jkind;

/* One node. Children are linked intrusively — no separate child array. */
typedef struct jnode {
    jkind                kind;
    proven_u8str_view_t  key;      /* filled only for a member of an object */
    proven_u8str_view_t  str;      /* J_STR */
    long long            num;      /* J_NUM */
    bool                 boolean;  /* J_BOOL */
    proven_list_t        kids;     /* children of J_OBJ / J_ARR */
    proven_list_node_t   link;     /* the hook onto the parent's kids */
} jnode;

typedef struct { proven_err_t err; proven_size_t at; } jresult;

#define OK(pos)        ((jresult){ PROVEN_OK, (pos) })
#define BAD(pos)       ((jresult){ PROVEN_ERR_INVALID_FORMAT, (pos) })

/* -- lexing --------------------------------------------------------- */

static bool is_ws(proven_byte_t c)
{ return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static proven_size_t skip_ws(proven_u8str_view_t t, proven_size_t i)
{ while (i < t.size && is_ws(t.ptr[i])) i++; return i; }

static bool lit_at(proven_u8str_view_t t, proven_size_t i, proven_u8str_view_t lit)
{
    if (t.size - i < lit.size) return false;
    for (proven_size_t k = 0; k < lit.size; k++)
        if (t.ptr[i + k] != lit.ptr[k]) return false;
    return true;
}

static jresult take_string(proven_u8str_view_t t, proven_size_t *i,
                           proven_u8str_view_t *out)
{
    if (*i >= t.size || t.ptr[*i] != '"') return BAD(*i);
    proven_size_t start = ++(*i);
    while (*i < t.size && t.ptr[*i] != '"') {
        if (t.ptr[*i] == '\\' && *i + 1 < t.size) (*i)++;
        (*i)++;
    }
    if (*i >= t.size) return BAD(*i);
    *out = (proven_u8str_view_t){ .ptr = t.ptr + start, .size = *i - start };
    (*i)++;
    return OK(*i);
}

static jresult take_number(proven_u8str_view_t t, proven_size_t *i, long long *out)
{
    proven_size_t start = *i;
    bool neg = false;
    if (*i < t.size && (t.ptr[*i] == '-' || t.ptr[*i] == '+')) { neg = t.ptr[*i] == '-'; (*i)++; }
    if (*i >= t.size || t.ptr[*i] < '0' || t.ptr[*i] > '9') return BAD(start);
    long long v = 0;
    while (*i < t.size && t.ptr[*i] >= '0' && t.ptr[*i] <= '9') {
        if (PROVEN_CKD_MUL(&v, v, 10LL) ||
            PROVEN_CKD_ADD(&v, v, (long long)(t.ptr[*i] - '0')))
            return (jresult){ PROVEN_ERR_OVERFLOW, start };
        (*i)++;
    }
    *out = neg ? -v : v;
    return OK(*i);
}

/* -- parser: a loop with an explicit stack, no recursion ------------- */

typedef struct { jnode *node; bool first; } frame;

typedef struct {
    proven_allocator_t nodes;   /* pool — recycles fixed-size nodes */
    proven_array_t     stack;   /* the open containers (explicit stack) */
    proven_size_t      max_depth;
    jnode             *root;
    proven_size_t      count;
} jparser;

static jnode *node_new(jparser *p, jkind k)
{
    proven_result_mem_mut_t m =
        p->nodes.alloc_fn(p->nodes.ctx, sizeof(jnode), alignof(jnode));
    if (!proven_is_ok(m.err)) return nullptr;
    jnode *n = (jnode *)m.value.ptr;
    *n = (jnode){ .kind = k };
    proven_list_init(&n->kids);
    p->count++;
    return n;
}

static jresult parse(jparser *p, proven_u8str_view_t t)
{
    proven_size_t i = skip_ws(t, 0);
    p->root = nullptr;

    for (;;) {
        /* read one value */
        proven_u8str_view_t key = { .ptr = nullptr, .size = 0 };
        bool in_obj = false;

        if (p->stack.len > 0) {
            frame *top = (frame *)proven_array_get_mut(&p->stack, p->stack.len - 1);
            in_obj = top->node->kind == J_OBJ;
            if (!top->first) {                       /* a comma from the second on */
                if (i < t.size && t.ptr[i] == ',') i = skip_ws(t, i + 1);
                else if (i < t.size && (t.ptr[i] == '}' || t.ptr[i] == ']')) goto close;
                else return BAD(i);
            } else if (i < t.size && (t.ptr[i] == '}' || t.ptr[i] == ']')) {
                goto close;                          /* an empty container */
            }
            if (in_obj) {                            /* in an object a key comes first */
                jresult r = take_string(t, &i, &key);
                if (!proven_is_ok(r.err)) return r;
                i = skip_ws(t, i);
                if (i >= t.size || t.ptr[i] != ':') return BAD(i);
                i = skip_ws(t, i + 1);
            }
        }

        if (i >= t.size) return BAD(i);
        jnode *n = nullptr;
        proven_byte_t c = t.ptr[i];

        if (c == '{' || c == '[') {
            n = node_new(p, c == '{' ? J_OBJ : J_ARR);
            if (!n) return (jresult){ PROVEN_ERR_NOMEM, i };
            n->key = key;
        } else if (c == '"') {
            n = node_new(p, J_STR);
            if (!n) return (jresult){ PROVEN_ERR_NOMEM, i };
            n->key = key;
            jresult r = take_string(t, &i, &n->str);
            if (!proven_is_ok(r.err)) return r;
        } else if (lit_at(t, i, PROVEN_LIT("true")) || lit_at(t, i, PROVEN_LIT("false"))) {
            n = node_new(p, J_BOOL);
            if (!n) return (jresult){ PROVEN_ERR_NOMEM, i };
            n->key = key;
            n->boolean = c == 't';
            i += (c == 't') ? 4 : 5;
        } else if (lit_at(t, i, PROVEN_LIT("null"))) {
            n = node_new(p, J_NULL);
            if (!n) return (jresult){ PROVEN_ERR_NOMEM, i };
            n->key = key;
            i += 4;
        } else {
            n = node_new(p, J_NUM);
            if (!n) return (jresult){ PROVEN_ERR_NOMEM, i };
            n->key = key;
            jresult r = take_number(t, &i, &n->num);
            if (!proven_is_ok(r.err)) return r;
        }

        /* hook it onto the parent — intrusive, so no child array is needed */
        if (p->stack.len > 0) {
            frame *top = (frame *)proven_array_get_mut(&p->stack, p->stack.len - 1);
            proven_list_push_back(&top->node->kids, &n->link);
            top->first = false;
        } else {
            p->root = n;
        }

        if (n->kind == J_OBJ || n->kind == J_ARR) {
            /* depth is capped *as a value* — we do not wait for a stack overflow */
            if (p->stack.len >= p->max_depth)
                return (jresult){ PROVEN_ERR_OUT_OF_BOUNDS, i };
            frame f = { .node = n, .first = true };
            proven_err_t e = proven_array_push(&p->stack, &f);
            if (!proven_is_ok(e)) return (jresult){ e, i };
            i = skip_ws(t, i + 1);
            continue;
        }

        i = skip_ws(t, i);
        if (p->stack.len == 0) return OK(i);          /* a lone scalar was the whole document */
        continue;

    close:
        {
            frame done;
            proven_err_t e = proven_array_pop(&p->stack, &done);
            if (!proven_is_ok(e)) return (jresult){ e, i };
            proven_byte_t want = done.node->kind == J_OBJ ? '}' : ']';
            if (i >= t.size || t.ptr[i] != want) return BAD(i);
            i = skip_ws(t, i + 1);
            if (p->stack.len == 0) return OK(i);
            continue;
        }
    }
}

/* -- output: again without recursion, on its own stack --------------- */

typedef struct { jnode *node; proven_list_node_t *next; bool opened; } oframe;

static proven_err_t emit_scalar(proven_u8str_t *s, const jnode *n)
{
    switch (n->kind) {
    case J_STR: {
        proven_err_t e = proven_u8str_append(s, PROVEN_LIT("\""));
        if (proven_is_ok(e)) e = proven_u8str_append(s, n->str);
        if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("\""));
        return e;
    }
    case J_NUM: {
        char tmp[32];
        int k = snprintf(tmp, sizeof tmp, "%lld", n->num);
        return proven_u8str_append(s, (proven_u8str_view_t){
                .ptr = (const proven_byte_t *)tmp, .size = (proven_size_t)k });
    }
    case J_BOOL:
        return proven_u8str_append(s, n->boolean ? PROVEN_LIT("true") : PROVEN_LIT("false"));
    case J_NULL:
        return proven_u8str_append(s, PROVEN_LIT("null"));
    default:
        return PROVEN_ERR_INVALID_STATE;
    }
}

static proven_err_t emit_key(proven_u8str_t *s, const jnode *n)
{
    if (n->key.ptr == nullptr) return PROVEN_OK;
    proven_err_t e = proven_u8str_append(s, PROVEN_LIT("\""));
    if (proven_is_ok(e)) e = proven_u8str_append(s, n->key);
    if (proven_is_ok(e)) e = proven_u8str_append(s, PROVEN_LIT("\":"));
    return e;
}

static proven_err_t write_iter(jnode *root, proven_allocator_t alloc,
                               proven_size_t max_depth, proven_u8str_t *s)
{
    proven_result_array_t made = PROVEN_ARRAY_INIT(alloc, oframe, 8);
    if (!proven_is_ok(made.err)) return made.err;
    proven_array_t st = made.value;
    proven_err_t e = PROVEN_OK;

    oframe f0 = { .node = root, .next = nullptr, .opened = false };
    e = proven_array_push(&st, &f0);

    while (proven_is_ok(e) && st.len > 0) {
        oframe *f = (oframe *)proven_array_get_mut(&st, st.len - 1);
        jnode *n = f->node;

        if (!f->opened) {
            e = emit_key(s, n);
            if (!proven_is_ok(e)) break;
            if (n->kind != J_OBJ && n->kind != J_ARR) {
                e = emit_scalar(s, n);
                oframe drop; (void)proven_array_pop(&st, &drop);
                continue;
            }
            e = proven_u8str_append(s, n->kind == J_OBJ ? PROVEN_LIT("{") : PROVEN_LIT("["));
            f->opened = true;
            f->next = n->kids.head.next;
            continue;
        }

        if (f->next == &n->kids.head) {              /* all children emitted */
            e = proven_u8str_append(s, n->kind == J_OBJ ? PROVEN_LIT("}") : PROVEN_LIT("]"));
            oframe drop; (void)proven_array_pop(&st, &drop);
            continue;
        }

        jnode *kid = PROVEN_LIST_ENTRY(f->next, jnode, link);
        bool first = f->next == n->kids.head.next;
        f->next = f->next->next;
        if (!first) e = proven_u8str_append(s, PROVEN_LIT(","));
        if (!proven_is_ok(e)) break;
        if (st.len >= max_depth) { e = PROVEN_ERR_OUT_OF_BOUNDS; break; }
        oframe kf = { .node = kid, .next = nullptr, .opened = false };
        e = proven_array_push(&st, &kf);
    }

    proven_array_destroy(&st);
    return e;
}

/* -- demonstration --------------------------------------------------- */

static void run(const char *label, proven_u8str_view_t text,
                proven_allocator_t backing, proven_size_t max_depth)
{
    proven_pool_t pool;
    if (!proven_is_ok(proven_pool_init(&pool, backing, sizeof(jnode),
                                       alignof(jnode), 64))) return;
    proven_result_array_t st = PROVEN_ARRAY_INIT(backing, frame, 8);
    if (!proven_is_ok(st.err)) { proven_pool_destroy(&pool); return; }

    jparser p = { .nodes = proven_pool_as_allocator(&pool), .stack = st.value,
                  .max_depth = max_depth, .count = 0 };
    jresult r = parse(&p, text);

    printf("%-22s ", label);
    if (!proven_is_ok(r.err)) {
        printf("refused: err %d at byte %zu (depth limit %zu)\n",
               (int)r.err, (size_t)r.at, (size_t)max_depth);
    } else {
        proven_result_u8str_t made = proven_u8str_create(backing, 512);
        if (proven_is_ok(made.err)) {
            proven_u8str_t out = made.value;
            proven_err_t e = write_iter(p.root, backing, max_depth, &out);
            proven_u8str_view_t v = proven_u8str_as_view(&out);
            if (proven_is_ok(e))
                printf("%zu nodes -> %.*s\n", (size_t)p.count,
                       (int)v.size, (const char *)v.ptr);
            else
                printf("write refused: err %d\n", (int)e);
            proven_u8str_destroy(backing, &out);
        }
    }
    proven_array_destroy(&p.stack);
    proven_pool_destroy(&pool);
}

int main(void)
{
    static proven_byte_t backing_mem[64 * 1024];
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = backing_mem, .size = sizeof backing_mem });
    proven_allocator_t alloc = proven_arena_as_allocator(&arena);

    run("flat object", PROVEN_LIT(
        "{\"name\":\"proven\",\"year\":2026}"), alloc, 32);

    run("nested", PROVEN_LIT(
        "{\"book\":{\"title\":\"Proven C\",\"parts\":13},"
        "\"tags\":[\"c23\",\"systems\"],\"draft\":true}"), alloc, 32);

    run("array of objects", PROVEN_LIT(
        "[{\"id\":1,\"ok\":true},{\"id\":2,\"ok\":false},[]]"), alloc, 32);

    /* input 200 deep — where a recursive parser would blow the stack */
    static char deep[512];
    proven_size_t n = 0;
    for (int k = 0; k < 200; k++) deep[n++] = '[';
    for (int k = 0; k < 200; k++) deep[n++] = ']';
    proven_u8str_view_t deep_view = {
        .ptr = (const proven_byte_t *)deep, .size = n };

    run("depth 200, limit 32", deep_view, alloc, 32);
    run("depth 200, limit 256", deep_view, alloc, 256);
    return 0;
}
