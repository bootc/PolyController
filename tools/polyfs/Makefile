CC = gcc
CFLAGS = -W -Wall -O2 -g -std=gnu99
CPPFLAGS = -I../../include
LDLIBS = -lz -llzo2
PROGS = mkpolyfs polyfsck

all: $(PROGS)

distclean clean:
	rm -f $(PROGS)

.PHONY: all clean
