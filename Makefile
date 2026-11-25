CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

all: server client

server: server.c grpc_frame.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c grpc_frame.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client
	rm -f /tmp/containerd_toy.sock

.PHONY: all clean
