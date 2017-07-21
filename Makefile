
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
OBJS = src/amqp.o src/utils.o src/config.o src/list.o src/queue.o src/cache.o
LIBS = -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lrabbitmq -lleveldb

all: sp

sp: src/sp.c src/sp.h $(OBJS)
	$(CC) $(CFLAGS) src/sp.c $(OBJS) $(LIBS) -o client

src/amqp.o: src/amqp.c src/amqp.h
	$(CC) $(CFLAGS) -c src/amqp.c -o src/amqp.o

src/utils.o: src/utils.c src/utils.h
	$(CC) $(CFLAGS) -c src/common.c -o src/common.o

src/config.o: src/config.c src/config.h
	$(CC) $(CFLAGS) -c src/config.c -o src/config.o

src/list.o: src/list.c src/list.h
	$(CC) $(CFLAGS) -c src/list.c -o src/list.o

src/queue.o: src/queue.c src/queue.h
	$(CC) $(CFLAGS) -c src/queue.c -o src/queue.o

src/cache.o: src/cache.c src/cache.h
	$(CC) $(CFLAGS) -c src/cache.c -o src/cache.o

.PHONY: install clean

install:
	echo complete

clean:
	rm -f src/*.o
	rm -f client
	rm -f server

