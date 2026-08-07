/* Printing a function pointer — three roads for a value %p will not take. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

typedef int (*binop)(int, int);

/* ── Road 1: lift the bytes and print them in hex ───────────────────
   Casting a function pointer to void* is outside the standard. Moving the
   bytes with memcpy is inside the contract everywhere — the size is all. */
static void fmt_funcptr(char *out, size_t cap, binop f)
{
    unsigned char raw[sizeof f];
    memcpy(raw, &f, sizeof raw);

    size_t k = 0;
    k += (size_t)snprintf(out + k, cap - k, "0x");
    for (size_t i = sizeof raw; i-- > 0 && k + 2 < cap; )
        k += (size_t)snprintf(out + k, cap - k, "%02X", raw[i]);
}

/* ── Road 2: carry the name alongside (the practical answer) ─────── */
struct named_op {
    const char *name;
    binop       fn;
};

static const struct named_op ops[] = {
    { "add", add }, { "sub", sub }, { "mul", mul },
};

static const char *name_of(binop f)
{
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++)
        if (ops[i].fn == f) return ops[i].name;      /* comparing function pointers is legal */
    return "(unknown function)";
}

int main(void)
{
    printf("sizeof(function pointer) = %zu, sizeof(void *) = %zu\n\n",
           sizeof(binop), sizeof(void *));

    puts("[Road 1] print the bytes — portable, but a riddle to a reader");
    char buf[2 + 2 * sizeof(binop) + 1];
    fmt_funcptr(buf, sizeof buf, add);
    printf("  length of add's address string: %zu characters (the value differs per run)\n",
           strlen(buf));
    printf("  does it start with \"0x\": %s\n", strncmp(buf, "0x", 2) == 0 ? "yes" : "no");

    puts("\n[Road 2] print the name — this is what a log should keep");
    binop chosen[] = { mul, add, sub };
    for (size_t i = 0; i < sizeof chosen / sizeof *chosen; i++)
        printf("  call %zu: %s(7, 3) = %d\n",
               i, name_of(chosen[i]), chosen[i](7, 3));

    puts("\n[comparing function pointers is inside the contract]");
    binop f = add, g = add, h = sub;
    printf("  equal when they point at the same function: %s\n", f == g ? "yes" : "no");
    printf("  different when the functions differ:      %s\n", f != h ? "yes" : "no");
    puts("  So \"which function is it\" can be answered without printing an address.");

    puts("\n[what not to do]");
    puts("  printf(\"%p\", f);          <- a function pointer to %p: outside the contract");
    puts("  printf(\"%p\", (void *)f);  <- outside ISO C (POSIX allows it). -Wpedantic warns");
    puts("  printf(\"%p\", (void *)&f); <- compiles, but that is the *variable's* address");
    return 0;
}
