/* 라이브러리 1.1 --- 멤버 하나를 '가운데' 끼워 넣었다. 소스 호환은 그대로다. */
#ifndef CONF_V2_H
#define CONF_V2_H
struct conf {
    int width;
    int depth;        /* ← 새로 생긴 멤버 */
    int height;
};
#endif
