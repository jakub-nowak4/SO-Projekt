CC = gcc
CFLAGS = -Wall -Wextra -pthread -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

TARGETS = main dziekan kandydat komisja_a komisja_b

all: $(TARGETS)

main: main.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o main main.c egzamin.c $(LDFLAGS)

dziekan: dziekan.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o dziekan dziekan.c egzamin.c $(LDFLAGS)

kandydat: kandydat.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o kandydat kandydat.c egzamin.c $(LDFLAGS)

komisja_a: komisja_a.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o komisja_a komisja_a.c egzamin.c $(LDFLAGS)

komisja_b: komisja_b.c egzamin.c egzamin.h
	$(CC) $(CFLAGS) -o komisja_b komisja_b.c egzamin.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)
	rm -rf logi
	ipcrm -a 2>/dev/null || true

run: all
	./main

.PHONY: all clean run