
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
OBJS = src/amqp.o src/common.o src/config.o src/list.o src/queue.o src/cache.o
LIBS = -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lrabbitmq -lleveldb

all: sp

sp: src/sp.c src/sp.h $(OBJS)
	$(CC) $(CFLAGS) src/sp.c $(OBJS) $(LIBS) -o client

amqp.o: amqp.c amqp.h
	$(CC) $(CFLAGS) -c src/amqp.c -lrabbitmq -o src/amqp.o

common.o: src/common.c src/common.h
	$(CC) $(CFLAGS) -c src/common.c -o src/common.o

config.o: src/config.c src/config.h
	$(CC) $(CFLAGS) -c src/config.c -o src/config.o

list.o: src/list.c src/list.h
	$(CC) $(CFLAGS) -c src/list.c -o src/list.o

queue.o: src/queue.c src/queue.h
	$(CC) $(CFLAGS) -c -pthread src/queue.c -o src/queue.o

cache.o: src/cache.c src/cache.h
	$(CC) $(CFLAGS) -c src/cache.c -lleveldb -o src/cache.o

.PHONY: install clean

install:
	echo complete

clean:
	rm -f src/*.o
	rm -f client
	rm -f server
