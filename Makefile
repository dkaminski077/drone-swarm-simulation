# Kompilator i flagi
CC = gcc
CFLAGS = -std=c99 -D_GNU_SOURCE -w

# Pliki wykonywalne
TARGETS = operator dron dowodca

# Domyślny cel (wpisz 'make' w konsoli)
all: $(TARGETS)

# Reguły kompilacji (zależą od plików .c i common.h)
operator: operator.c common.h
	$(CC) $(CFLAGS) operator.c -o operator

dron: dron.c common.h
	$(CC) $(CFLAGS) dron.c -o dron

dowodca: dowodca.c common.h
	$(CC) $(CFLAGS) dowodca.c -o dowodca

# Czyszczenie (wpisz 'make clean')
clean:
	rm -f $(TARGETS) logi.txt

# Uruchamianie (wpisz 'make run')
run: operator
	./operator

.PHONY: all clean run