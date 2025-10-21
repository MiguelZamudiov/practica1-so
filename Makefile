CC=gcc
CFLAGS=-O2 -std=c11 -Wall
all: all: worker indexer_books indexer_reviews interfaz

worker: src/search_engine.c
$(CC) $(CFLAGS) src/search_engine.c -o worker -lrt

indexer_books: src/hash.c
$(CC) $(CFLAGS) src/hash.c -o indexer_books

indexer_reviews: src/hashreviews.c
$(CC) $(CFLAGS) src/hashreviews.c -o indexer_reviews

interfaz: src/interfaz.c
	$(CC) $(CFLAGS) src/interfaz.c -o interfaz

clean:
rm -f worker indexer_books indexer_reviews
