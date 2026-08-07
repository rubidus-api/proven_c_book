/* 태그와 typedef 가 갈리는 자리, 그리고 열거 상수가 '보통 식별자'라서
   생기는 충돌. */
#include <stdio.h>

/* 태그 이름 공간과 보통 식별자 이름 공간이 달라서
   "같은 철자"로 태그와 typedef 이름을 둘 다 만들 수 있다 */
typedef struct node {
    int          value;
    struct node *next;      /* 태그가 있어야 자기 자신을 가리킬 수 있다 */
} node;

/* 열거 상수는 태그가 아니라 *보통 식별자* 다.
   그래서 아래 red 는 int 변수 red 와 같은 마당에 산다 — 충돌한다. */
enum color { red, green, blue };

/* 이렇게 쓰면 컴파일 오류다(주석으로만 보인다):
       int red;        // error: 'red' redeclared as different kind of symbol
   그래서 실무는 열거 상수에 접두어를 붙인다. */
enum status { STATUS_OK, STATUS_BUSY, STATUS_FAIL };

/* 반대로 태그는 보통 식별자와 절대 충돌하지 않는다.
   단, struct/union/enum 은 태그 마당을 *셋이서 함께 쓴다* — 그래서
       struct status { int code; };
   는 위의 enum status 와 충돌한다("defined as wrong kind of tag").
   태그 마당은 넷 중 하나이지, 키워드마다 하나가 아니다. */
struct handle { int code; };     /* 다른 철자를 쓴다 */

/* 반면 보통 식별자 마당은 태그와 완전히 별개다 — 같은 철자를 써도 된다 */
static int status = 42;          /* enum status 태그와 공존한다 */

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

    puts("[태그와 typedef 이름은 다른 이름 공간이라 같은 철자를 쓸 수 있다]");
    for (node *p = &a; p; p = p->next)
        printf("  node %d\n", p->value);

    puts("\n[열거 상수는 보통 식별자다 — 변수와 같은 마당]");
    printf("  enum color: %s %s %s\n", name_of(red), name_of(green), name_of(blue));
    puts("  그래서 int red; 는 컴파일 오류다 — 접두어(STATUS_OK)가 관행인 이유");

    puts("\n[태그 마당은 struct·union·enum 이 함께 쓴다 — 하나뿐이다]");
    struct handle h = { .code = 7 };
    enum   status e = STATUS_BUSY;
    printf("  struct handle.code = %d, enum status = %d\n", h.code, (int)e);
    puts("  struct status 로는 못 짓는다 — enum status 가 이미 그 태그를 차지했다");

    puts("\n[그러나 보통 식별자는 태그와 다른 마당이다]");
    printf("  변수 status = %d 가 태그 status 와 나란히 산다\n", status);
    return 0;
}
