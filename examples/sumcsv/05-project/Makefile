CC     ?= gcc
CFLAGS  = -std=c23 -Wall -Wextra -Werror -MMD -MP
OBJS    = main.o parse.o sum.o
DEPS    = $(OBJS:.o=.d)

all: sumcsv

sumcsv: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	sh ./run.sh

clean:
	rm -f sumcsv sumcsv-buggy $(OBJS) $(DEPS)

-include $(DEPS)
