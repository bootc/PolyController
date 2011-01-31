CC = gcc
CFLAGS = -W -Wall -O2 -g -std=gnu99
CPPFLAGS = -I../../include
LDLIBS = -lz -llzo2
PROGS = mkpolyfs polyfsck

ifeq ($(shell uname -s),Darwin)
CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
CLEANDIRS += $(addsuffix .dSYM,$(PROGS))
endif

all: $(PROGS)

distclean clean:
	rm -f $(PROGS)
	$(if $(CLEANDIRS),rm -rf $(CLEANDIRS),)

.PHONY: all clean
