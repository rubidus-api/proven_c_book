#include <stdio.h>

/* One list produces the enumeration, the name table and the lookup code
   together. Change the list in one place and the rest follows. */
#define ERROR_LIST(X)                       \
    X(OK,        0, "no error")             \
    X(NOT_FOUND, 2, "resource not found")   \
    X(DENIED,    5, "permission denied")    \
    X(TIMEOUT,   9, "operation timed out")

/* (1) making the enumeration — only the name and the value are used */
#define AS_ENUM(name, code, text) ERR_##name = code,
enum error_code { ERROR_LIST(AS_ENUM) };
#undef AS_ENUM

/* (2) making the table of name strings — the name is turned into a string (#) */
#define AS_NAME(name, code, text) [code] = #name,
static const char *const error_name[] = { ERROR_LIST(AS_NAME) };
#undef AS_NAME

/* (3) making the function that returns the description — a whole switch is generated */
#define AS_CASE(name, code, text) case ERR_##name: return text;
static const char *error_text(enum error_code e)
{
    switch (e) {
        ERROR_LIST(AS_CASE)
        default: return "unknown error";
    }
}
#undef AS_CASE

/* (4) counting — the length of the list comes automatically too */
#define AS_COUNT(name, code, text) + 1
enum { ERROR_COUNT = 0 ERROR_LIST(AS_COUNT) };
#undef AS_COUNT

int main(void)
{
    printf("%d error codes defined\n", ERROR_COUNT);

#define AS_ROW(name, code, text) \
    printf("  %-10s code=%d name=%-9s text=%s\n", #name, (int)ERR_##name, \
           error_name[code], error_text(ERR_##name));
    ERROR_LIST(AS_ROW)
#undef AS_ROW

    return 0;
}
