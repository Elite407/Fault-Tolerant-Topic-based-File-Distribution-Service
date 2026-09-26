CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -pthread

all: server client

protocol.o: protocol.c protocol.h
	$(CC) $(CFLAGS) -c protocol.c

server: server.c protocol.o
	$(CC) $(CFLAGS) server.c protocol.o -o server

client: client.c protocol.o
	$(CC) $(CFLAGS) client.c protocol.o -o client

clean:
	rm -f server client protocol.o
