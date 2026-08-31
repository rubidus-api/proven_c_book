/* the public contract - all the caller knows */
#ifndef GREET_H
#define GREET_H

/* Longest greeting. If this number changes, the caller must be rebuilt too. */
#define GREET_MAX 16

void greet(char *out);

#endif
