#include "proven/time.h"
#include "proven/u8str.h"
#include "proven/fmt.h"
#include "proven/memory.h"
#include "../../platform/proven_sys_time.h"

static const proven_u8str_view_t time_month_names[12] = {
    PROVEN_LIT_INIT("January"), PROVEN_LIT_INIT("February"), PROVEN_LIT_INIT("March"), PROVEN_LIT_INIT("April"), PROVEN_LIT_INIT("May"), PROVEN_LIT_INIT("June"),
    PROVEN_LIT_INIT("July"), PROVEN_LIT_INIT("August"), PROVEN_LIT_INIT("September"), PROVEN_LIT_INIT("October"), PROVEN_LIT_INIT("November"), PROVEN_LIT_INIT("December")
};

static const proven_u8str_view_t time_month_short_names[12] = {
    PROVEN_LIT_INIT("Jan"), PROVEN_LIT_INIT("Feb"), PROVEN_LIT_INIT("Mar"), PROVEN_LIT_INIT("Apr"), PROVEN_LIT_INIT("May"), PROVEN_LIT_INIT("Jun"),
    PROVEN_LIT_INIT("Jul"), PROVEN_LIT_INIT("Aug"), PROVEN_LIT_INIT("Sep"), PROVEN_LIT_INIT("Oct"), PROVEN_LIT_INIT("Nov"), PROVEN_LIT_INIT("Dec")
};

static const proven_u8str_view_t time_weekday_names[7] = {
    PROVEN_LIT_INIT("Sunday"), PROVEN_LIT_INIT("Monday"), PROVEN_LIT_INIT("Tuesday"), PROVEN_LIT_INIT("Wednesday"), PROVEN_LIT_INIT("Thursday"), PROVEN_LIT_INIT("Friday"), PROVEN_LIT_INIT("Saturday")
};

static const proven_u8str_view_t time_weekday_short_names[7] = {
    PROVEN_LIT_INIT("Sun"), PROVEN_LIT_INIT("Mon"), PROVEN_LIT_INIT("Tue"), PROVEN_LIT_INIT("Wed"), PROVEN_LIT_INIT("Thu"), PROVEN_LIT_INIT("Fri"), PROVEN_LIT_INIT("Sat")
};

const proven_time_locale_t proven_time_locale_en = {
    .month_names = time_month_names,
    .month_short_names = time_month_short_names,
    .weekday_names = time_weekday_names,
    .weekday_short_names = time_weekday_short_names
};

proven_err_t proven_time_u8_fmt(proven_allocator_t alloc, proven_u8str_t *str, proven_datetime_t dt, const proven_time_locale_t *locale, const char *fmt) {
    if (!str || !fmt) return PROVEN_ERR_INVALID_ARG;
    if (!locale) locale = &proven_time_locale_en;
    
    const char *p = fmt;
    while (*p) {
        if (*p == '{') {
            p++;
            if (*p == '{') {
                proven_err_t err = proven_u8str_append_byte(alloc, str, '{');
                if (!PROVEN_IS_OK(err)) return err;
                p++;
                continue;
            }
            
            const char *key_start = p;
            while (*p && *p != ':' && *p != '}') p++;
            const char *key_end = p;
            
            const char *spec_start = NULL;
            const char *spec_end = NULL;
            if (*p == ':') {
                spec_start = p;
                while (*p && *p != '}') p++;
                spec_end = p;
            } else {
                spec_start = p;
                spec_end = p;
            }
            
            if (*p == '}') p++;
            
            char fmt_buf[64];
            int fmt_len = 0;
            fmt_buf[fmt_len++] = '{';
            if (spec_start && spec_end > spec_start) {
                for (const char *s = spec_start; s < spec_end && fmt_len < 60; s++) {
                    fmt_buf[fmt_len++] = *s;
                }
            }
            fmt_buf[fmt_len++] = '}';
            fmt_buf[fmt_len] = '\0';
            
            proven_size_t key_len = (proven_size_t)(key_end - key_start);
            #define KEY_EQ(str_lit) (key_len == (sizeof(str_lit)-1) && proven_memcmp(key_start, str_lit, key_len) == 0)
            
            proven_err_t err = PROVEN_OK;
            if (KEY_EQ("year")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.year)).err;
            } else if (KEY_EQ("month")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.month)).err;
            } else if (KEY_EQ("day")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.day)).err;
            } else if (KEY_EQ("hour")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.hour)).err;
            } else if (KEY_EQ("min")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.min)).err;
            } else if (KEY_EQ("sec")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.sec)).err;
            } else if (KEY_EQ("ms")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.ms)).err;
            } else if (KEY_EQ("wday_num")) {
                err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(dt.weekday)).err;
            } else if (KEY_EQ("Month") && locale->month_names) {
                if (dt.month >= 1 && dt.month <= 12) {
                    err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(locale->month_names[dt.month - 1])).err;
                }
            } else if (KEY_EQ("mon") && locale->month_short_names) {
                if (dt.month >= 1 && dt.month <= 12) {
                    err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(locale->month_short_names[dt.month - 1])).err;
                }
            } else if (KEY_EQ("Weekday") && locale->weekday_names) {
                if (dt.weekday <= 6) {
                    err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(locale->weekday_names[dt.weekday])).err;
                }
            } else if (KEY_EQ("wday") && locale->weekday_short_names) {
                if (dt.weekday <= 6) {
                    err = proven_u8str_append_fmt_grow(alloc, str, fmt_buf, PROVEN_ARG(locale->weekday_short_names[dt.weekday])).err;
                }
            } else {
                err = proven_u8str_append_byte(alloc, str, '{');
                if (PROVEN_IS_OK(err)) {
                    for (proven_size_t i = 0; i < key_len; i++) {
                        err = proven_u8str_append_byte(alloc, str, (proven_u8)key_start[i]);
                        if (!PROVEN_IS_OK(err)) return err;
                    }
                    if (spec_end && spec_end > spec_start) {
                        for (const char *s = spec_start; s < spec_end; s++) {
                            err = proven_u8str_append_byte(alloc, str, (proven_u8)*s);
                            if (!PROVEN_IS_OK(err)) return err;
                        }
                    }
                    err = proven_u8str_append_byte(alloc, str, '}');
                    if (!PROVEN_IS_OK(err)) return err;
                }
            }
            if (!PROVEN_IS_OK(err)) return err;
            #undef KEY_EQ
            
        } else if (*p == '}') {
            p++;
            if (*p == '}') {
                proven_err_t err = proven_u8str_append_byte(alloc, str, '}');
                if (!PROVEN_IS_OK(err)) return err;
                p++;
            } else {
                proven_err_t err = proven_u8str_append_byte(alloc, str, '}');
                if (!PROVEN_IS_OK(err)) return err;
            }
        } else {
            proven_err_t err = proven_u8str_append_byte(alloc, str, (proven_u8)*p);
            if (!PROVEN_IS_OK(err)) return err;
            p++;
        }
    }
    return PROVEN_OK;
}

#ifndef PROVEN_NO_U16STR

static proven_err_t append_u8_view_to_u16str(proven_allocator_t alloc, proven_u16str_t *str, proven_u8str_view_t view) {
    for (proven_size_t i = 0; i < view.size; i++) {
        proven_u16 ch = (proven_u16)view.ptr[i];
        proven_u16str_view_t v = { &ch, 1 };
        proven_err_t err = proven_u16str_append_grow(alloc, str, v);
        if (!PROVEN_IS_OK(err)) return err;
    }
    return PROVEN_OK;
}

proven_err_t proven_time_u16_fmt(proven_allocator_t alloc, proven_u16str_t *str, proven_datetime_t dt, const proven_time_locale_t *locale, const char *fmt) {
    if (!str || !fmt) return PROVEN_ERR_INVALID_ARG;

    /* One formatter, two encodings. Render through the u8 path - which delegates every field
     * to the fmt.h spec engine, so the whole {} grammar (fill, align, width) is honoured for
     * numeric and named fields alike - then widen the result to u16 code units. All time
     * output is single-byte (decimal digits, the ASCII locale names, and single-byte fill
     * characters), so the widening is exact and the two encodings can never again disagree on
     * a spec. The former hand-rolled u16 parser recognised only ":0>N" and silently dropped
     * every other fill/align/width spec - the quiet wrong answer this delegation removes. */
    proven_result_u8str_t tmp = proven_u8str_create(alloc, 64);
    if (!PROVEN_IS_OK(tmp.err)) return tmp.err;

    proven_err_t err = proven_time_u8_fmt(alloc, &tmp.value, dt, locale, fmt);
    if (PROVEN_IS_OK(err)) {
        err = append_u8_view_to_u16str(alloc, str, proven_u8str_as_view(&tmp.value));
    }

    proven_u8str_destroy(alloc, &tmp.value);
    return err;
}

#endif /* PROVEN_NO_U16STR */

static proven_u8 calc_weekday(proven_i32 year, proven_u8 month, proven_u8 day) {
    if (month < 1 || month > 12) return 0;
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year - (month < 3);
    int w = (y + y/4 - y/100 + y/400 + t[month-1] + day) % 7;
    // Adding 7 maps negative remainders into [0, 6].
    if (w < 0) w += 7;
    return (proven_u8)w;
}

proven_time_t proven_time_now(void) {
    return (proven_time_t)proven_sys_time_now_ns();
}

proven_datetime_t proven_time_breakdown(proven_time_t time_ns) {
    proven_i64 seconds = time_ns / (proven_i64)1000000000;
    proven_i64 nsec = time_ns % (proven_i64)1000000000;
    
    if (nsec < 0) {
        nsec += 1000000000;
        seconds -= 1;
    }
    
    proven_u32 ms = (proven_u32)(nsec / 1000000);

    // Simple Civil Date algorithm (Howard Hinnant) 
    proven_i64 days = seconds / 86400;
    proven_i64 rem = seconds % 86400;
    
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }

    proven_datetime_t dt;
    dt.hour = (proven_u8)(rem / 3600);
    dt.min = (proven_u8)((rem % 3600) / 60);
    dt.sec = (proven_u8)(rem % 60);
    dt.ms = ms;

    // Days since UNIX epoch (1970-01-01)
    // 719468 is days from 0000-03-01 to 1970-01-01
    proven_i64 z = days + 719468;
    proven_i64 era = (z >= 0 ? z : z - 146096) / 146097;
    proven_i64 doe = (z - era * 146097);
    proven_i64 yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    proven_i64 y = yoe + era * 400;
    proven_i64 doy = doe - (365*yoe + yoe/4 - yoe/100);
    proven_i64 mp = (5*doy + 2)/153;
    proven_i64 d = doy - (153*mp + 2)/5 + 1;
    proven_i64 m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);

    dt.year = (proven_i32)y;
    dt.month = (proven_u8)m;
    dt.day = (proven_u8)d;
    
    proven_i64 w = (days + 4) % 7;
    if (w < 0) w += 7;
    dt.weekday = (proven_u8)w;
    
    return dt;
}

proven_datetime_t proven_time_now_datetime(void) {
    proven_sys_datetime_t sdt;
    proven_sys_time_now_local(&sdt);
    
    proven_datetime_t dt;
    dt.year = (proven_i32)sdt.year;
    dt.month = (proven_u8)sdt.month;
    dt.day = (proven_u8)sdt.day;
    dt.hour = (proven_u8)sdt.hour;
    dt.min = (proven_u8)sdt.min;
    dt.sec = (proven_u8)sdt.sec;
    dt.ms = (proven_u32)sdt.ms;
    dt.weekday = calc_weekday(dt.year, dt.month, dt.day);
    return dt;
}

void proven_time_sleep(proven_u32 ms) {
    proven_sys_time_sleep_ms((unsigned int)ms);
}
