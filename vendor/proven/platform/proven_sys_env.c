#if !defined(_WIN32) && !defined(_WIN64)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "proven_sys_env.h"

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#else
#include <stdlib.h>
#include <string.h>
#endif

proven_err_t proven_sys_env_get(const char *name, char *out_buf, size_t buf_cap, size_t *out_len) {
    if (!name || name[0] == '\0') return PROVEN_ERR_INVALID_ARG;

#if defined(_WIN32) || defined(_WIN64)
    int wname_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
    if (wname_len <= 0) return PROVEN_ERR_INVALID_ARG;

    size_t wname_count = (size_t)wname_len;
    if (wname_count > PROVEN_SIZE_MAX / sizeof(wchar_t)) return PROVEN_ERR_OVERFLOW;

    wchar_t *wname = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, wname_count * sizeof(wchar_t));
    if (!wname) return PROVEN_ERR_NOMEM;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wname, wname_len) == 0) {
        HeapFree(GetProcessHeap(), 0, wname);
        return PROVEN_ERR_INVALID_ARG;
    }

    /*
     * Two calls: one to size the value, one to read it. A zero return from the second is
     * ambiguous by the API's own definition - it is both "failed" and "stored 0 characters" -
     * so a variable set to the empty string used to read as PROVEN_ERR_IO (RFC-0009 D-002).
     * GetLastError separates them. If the value grew between the calls, size it again.
     */
    wchar_t *wbuf = NULL;
    DWORD actual_res = 0;
    for (int attempt = 0; ; ++attempt) {
        DWORD res = GetEnvironmentVariableW(wname, NULL, 0);
        if (res == 0) {
            DWORD err = GetLastError();
            HeapFree(GetProcessHeap(), 0, wname);
            if (err == ERROR_ENVVAR_NOT_FOUND) return PROVEN_ERR_NOT_FOUND;
            return PROVEN_ERR_IO;
        }
        /* res counts the terminator. */
#if SIZE_MAX < UINT32_MAX
        if (res > (DWORD)(SIZE_MAX / sizeof(wchar_t))) {
            HeapFree(GetProcessHeap(), 0, wname);
            return PROVEN_ERR_OVERFLOW;
        }
#endif
        wbuf = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, (size_t)res * sizeof(wchar_t));
        if (!wbuf) {
            HeapFree(GetProcessHeap(), 0, wname);
            return PROVEN_ERR_NOMEM;
        }
        SetLastError(ERROR_SUCCESS);
        actual_res = GetEnvironmentVariableW(wname, wbuf, res);
        if (actual_res == 0) {
            DWORD err = GetLastError();
            if (err == ERROR_SUCCESS) { wbuf[0] = L'\0'; break; }   /* set, and empty */
            HeapFree(GetProcessHeap(), 0, wbuf);
            HeapFree(GetProcessHeap(), 0, wname);
            if (err == ERROR_ENVVAR_NOT_FOUND) return PROVEN_ERR_NOT_FOUND;   /* removed in between */
            return PROVEN_ERR_IO;
        }
        if (actual_res < res) break;
        /* It grew in between: the return is the size now needed. */
        HeapFree(GetProcessHeap(), 0, wbuf);
        wbuf = NULL;
        if (attempt == 2) {
            HeapFree(GetProcessHeap(), 0, wname);
            return PROVEN_ERR_AGAIN;
        }
    }
    HeapFree(GetProcessHeap(), 0, wname);

    /* WC_ERR_INVALID_CHARS: a lone surrogate is an error, never a silent U+FFFD (RFC-0009
     * D-003, the strict-text rule of B-039). */
    int utf8_bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wbuf, -1, NULL, 0, NULL, NULL);
    if (utf8_bytes <= 0) {
        DWORD err = GetLastError();
        HeapFree(GetProcessHeap(), 0, wbuf);
        return err == ERROR_NO_UNICODE_TRANSLATION ? PROVEN_ERR_INVALID_ENCODING : PROVEN_ERR_IO;
    }

    size_t req_len = (size_t)(utf8_bytes - 1);
    if (out_len) *out_len = req_len;

    if (!out_buf || buf_cap <= req_len) {
        HeapFree(GetProcessHeap(), 0, wbuf);
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }

    int out_cap = buf_cap > (size_t)INT_MAX ? INT_MAX : (int)buf_cap;
    int bytes_written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wbuf, -1, out_buf, out_cap, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wbuf);
    
    if (bytes_written == 0) return PROVEN_ERR_IO;
    return PROVEN_OK;
#else
    char *val = getenv(name);
    if (!val) return PROVEN_ERR_NOT_FOUND;

    size_t len = strlen(val);
    if (out_len) *out_len = len;

    if (!out_buf || buf_cap <= len) {
        return PROVEN_ERR_OUT_OF_BOUNDS;
    }

    memcpy(out_buf, val, len + 1);
    return PROVEN_OK;
#endif
}
