CC=gcc
CFLAGS=-O2 -std=c11 -Wall

# Compilar solo worker e interfaz
all: worker interfaz

worker: src/search_engine.c
	$(CC) $(CFLAGS) src/search_engine.c -o worker -lrt

interfaz: src/interfaz.c
	$(CC) $(CFLAGS) src/interfaz.c -o interfaz

clean:
	rm -f worker interfaz
