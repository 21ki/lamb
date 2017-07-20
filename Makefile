
CC=gcc
CFLAG=-std=c99 -Wall

dep=src/amqp.c src/amqp.h src/common.c src/common.h src/config.c src/config.h src/list.c src/list.h src/queue.c src/queue.h
obj=src/amqp.c src/common.c src/config.c src/list.c src/queue.c

all: sp

sp: sp.o

sp.o: src/amqp.o src/common.o src/config.o src/list.o src/queue.o src/cache.o

amqp.o: amqp.c amqp.h
	$(CC) $(CFLAG) -c src/amqp.c -o src/amqp.o

common.o: src/common.c src/common.h
	$(CC) $(CFLAG) -c src/common.c -o src/common.o

config.o: src/config.c src/config.h
	$(CC) $(CFLAG) -c src/config.c -o src/config.o

list.o: src/list.c src/list.h
	$(CC) $(CFLAG) -c src/list.c -o src/list.o

queue.o: src/queue.c src/queue.h
	$(CC) $(CFLAG) -c src/queue.c -o src/queue.o

cache.o: src/cache.c src/cache.h
	$(CC) $(CFLAG) -c src/cache.c -o src/cache.o

sp: src/sp.c src/sp.h $(dep)
	gcc -g src/sp.c $(obj) -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lrabbitmq -lleveldb -o client
