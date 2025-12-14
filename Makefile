CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

PROGS := main dziekan kandydat komisja_a komisja_b

.PHONY: all clean

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)
