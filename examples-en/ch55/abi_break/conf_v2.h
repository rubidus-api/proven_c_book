/* library 1.1 --- one member inserted in the middle. Source compatibility is intact. */
#ifndef CONF_V2_H
#define CONF_V2_H
struct conf {
    int width;
    int depth;        /* <- the new member */
    int height;
};
#endif
