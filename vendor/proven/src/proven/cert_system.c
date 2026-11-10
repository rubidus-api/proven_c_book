#include "proven/cert.h"
#include "proven/fs.h"
#include "../../platform/proven_sys_trust.h"
#include "../../platform/proven_sys_env.h"

/* The operating system's trusted roots, into a store. Hosted only. */

/* A bundle of every root a system ships is a few hundred kilobytes; this is far above it. */
#define CERT_BUNDLE_MAX ((proven_size_t)16 * 1024 * 1024)

typedef struct {
    proven_cert_store_t *store;
    proven_size_t added;
    proven_err_t err;
} proven_cert_visit_t;

static bool cert_visit(void *ctx, const uint8_t *der, size_t len) {
    proven_cert_visit_t *v = (proven_cert_visit_t *)ctx;
    proven_err_t e = proven_cert_store_add_der(v->store, (proven_mem_view_t){ .ptr = der, .size = len });
    if (e == PROVEN_OK) v->added++;
    else if (e == PROVEN_ERR_NOMEM) { v->err = e; return false; }
    return true;                           /* one this library cannot read is skipped */
}

static proven_err_t cert_add_file(proven_cert_store_t *store, proven_allocator_t alloc, const char *path, proven_size_t *added) {
    proven_size_t n = 0;
    while (path[n]) ++n;
    proven_result_mem_mut_t file = proven_fs_read_all_bounded(alloc, (proven_u8str_view_t){ .ptr = (const proven_byte_t *)path, .size = n }, CERT_BUNDLE_MAX);
    if (file.err != PROVEN_OK) return file.err == PROVEN_ERR_NOMEM ? PROVEN_ERR_NOMEM : PROVEN_ERR_NOT_FOUND;
    proven_err_t e = proven_cert_store_add_pem(store, (proven_mem_view_t){ .ptr = file.value.ptr, .size = file.value.size }, added);
    alloc.free_fn(alloc.ctx, file.value.ptr);
    if (e == PROVEN_ERR_INVALID_ENCODING && *added > 0) e = PROVEN_OK;     /* a damaged tail does not cost the entries before it */
    return e == PROVEN_OK || e == PROVEN_ERR_NOMEM ? e : PROVEN_ERR_NOT_FOUND;
}

/* The store's allocator, without making the struct public. */
proven_allocator_t proven_cert_store_allocator_(const proven_cert_store_t *store);

proven_err_t proven_cert_store_add_system(proven_cert_store_t *store, proven_size_t *added) {
    if (added) *added = 0;
    if (!store) return PROVEN_ERR_INVALID_ARG;
    proven_cert_visit_t v = { .store = store, .added = 0, .err = PROVEN_OK };
    int rc = proven_sys_trust_enumerate(cert_visit, &v);
    if (rc >= 0) {
        if (added) *added = v.added;
        if (v.err != PROVEN_OK) return v.err;
        return rc == 1 && v.added > 0 ? PROVEN_OK : PROVEN_ERR_NOT_FOUND;
    }
    proven_allocator_t alloc = proven_cert_store_allocator_(store);
    proven_size_t count = 0;
    char env_path[1024];
    size_t env_len = 0;
    /* SSL_CERT_FILE is the convention other TLS libraries read; honouring it lets one setting
     * serve every program on the machine. */
    if (proven_sys_env_get("SSL_CERT_FILE", env_path, sizeof env_path, &env_len) == PROVEN_OK && env_len > 0 && env_len < sizeof env_path) {
        env_path[env_len] = 0;
        proven_err_t e = cert_add_file(store, alloc, env_path, &count);
        if (e == PROVEN_OK || e == PROVEN_ERR_NOMEM) { if (added) *added = count; return e; }
    }
    for (proven_size_t i = 0;; ++i) {
        const char *path = proven_sys_trust_bundle_path(i);
        if (!path) break;
        proven_err_t e = cert_add_file(store, alloc, path, &count);
        if (e == PROVEN_OK || e == PROVEN_ERR_NOMEM) { if (added) *added = count; return e; }
    }
    return PROVEN_ERR_NOT_FOUND;
}
