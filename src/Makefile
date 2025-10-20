CC = gcc
CFLAGS = -O2 -std=c11 -Wall

SRC = search_engine.c
BIN = worker

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) -lrt

clean:
	rm -f $(BIN)

