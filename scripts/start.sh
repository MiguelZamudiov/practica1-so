#!/usr/bin/env bash
set -e
PROJECT="$HOME/projects/practica1"
FIFO_REQ="$PROJECT/fifo_req"
FIFO_RESP="$PROJECT/fifo_resp"
[ -p "$FIFO_REQ" ] || mkfifo "$FIFO_REQ"
[ -p "$FIFO_RESP" ] || mkfifo "$FIFO_RESP"
cd "$PROJECT"
./worker data/index_books.bin data/books_data.csv "$FIFO_REQ" "$FIFO_RESP" 2> worker.log &
echo "Worker lanzado. PID=$! request->$FIFO_REQ response<-$FIFO_RESP"
