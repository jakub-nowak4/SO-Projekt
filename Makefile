CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
LDFLAGS_PTHREAD = -pthread

PROGS := main dziekan kandydat komisja_a komisja_b

.PHONY: all clean

all: $(PROGS)

# Programy bez watkow
main: main.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o $@ main.c egzamin.c

dziekan: dziekan.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o $@ dziekan.c egzamin.c

kandydat: kandydat.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o $@ kandydat.c egzamin.c

# Programy z watkami - wymagaja -pthread
komisja_a: komisja_a.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) $(LDFLAGS_PTHREAD) -o $@ komisja_a.c egzamin.c

komisja_b: komisja_b.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) $(LDFLAGS_PTHREAD) -o $@ komisja_b.c egzamin.c

clean:
	rm -f $(PROGS)
	rm -r logi
	ipcrm -a
