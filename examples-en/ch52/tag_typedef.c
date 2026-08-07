/* Where tag and typedef part ways, and the collision that arises because
   enumeration constants are ordinary identifiers. */
#include <stdio.h>

/* The tag name space and the ordinary-identifier name space differ, so the
   same spelling can name both a tag and a typedef */
typedef struct node {
    int          value;
    struct node *next;      /* the tag is what lets it point at itself */
} node;

/* Enumeration constants are ordinary identifiers, not tags.
   So red below lives in the same yard as an int variable red — they clash. */
enum color { red, green, blue };

/* Written out, this is a compile error (shown only as a comment):
       int red;        // error: 'red' redeclared as different kind of symbol
   Which is why practice puts a prefix on enumeration constants. */
enum status { STATUS_OK, STATUS_BUSY, STATUS_FAIL };

/* Conversely a tag never clashes with an ordinary identifier.
   But struct/union/enum *share one tag yard between them* — so
       struct status { int code; };
   clashes with the enum status above ("defined as wrong kind of tag").
   There is one tag name space, not one per keyword. */
struct handle { int code; };     /* use a different spelling */

/* The ordinary-identifier yard, though, is wholly separate from the tag one */
static int status = 42;          /* coexists with the enum status tag */

static const char *name_of(enum color c)
{
    switch (c) {
    case red:   return "red";
    case green: return "green";
    case blue:  return "blue";
    }
    return "?";
}

int main(void)
{
    node b = { .value = 2, .next = nullptr };
    node a = { .value = 1, .next = &b };

    puts("[tag and typedef name are different name spaces, so one spelling serves]");
    for (node *p = &a; p; p = p->next)
        printf("  node %d\n", p->value);

    puts("\n[enumeration constants are ordinary identifiers — the variables' yard]");
    printf("  enum color: %s %s %s\n", name_of(red), name_of(green), name_of(blue));
    puts("  so int red; is a compile error — hence the STATUS_OK prefix habit");

    puts("\n[struct, union and enum share the tag yard — there is only one]");
    struct handle h = { .code = 7 };
    enum   status e = STATUS_BUSY;
    printf("  struct handle.code = %d, enum status = %d\n", h.code, (int)e);
    puts("  struct status is impossible — enum status already took that tag");

    puts("\n[but ordinary identifiers sit in a different yard from tags]");
    printf("  the variable status = %d lives beside the tag status\n", status);
    return 0;
}
