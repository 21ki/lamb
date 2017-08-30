
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
OBJS = src/account.o src/cache.o src/channel.o src/company.o src/config.o src/db.o src/gateway.o src/group.o src/queue.o src/utils.o src/security.o
LIBS = -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lpq -lhiredis -lrt -lpcre

all: sp ismg server

sp: src/sp.c src/sp.h $(OBJS)
	$(CC) $(CFLAGS) src/sp.c $(OBJS) $(LIBS) -o sp

ismg: src/ismg.c src/ismg.h $(OBJS)
	$(CC) $(CFLAGS) src/ismg.c $(OBJS) $(LIBS) -o ismg

server: src/server.c src/server.h $(OBJS)
	$(CC) $(CFLAGS) src/server.c $(OBJS) $(LIBS) -o server

src/account.o: src/account.c src/account.h
	$(CC) $(CFLAGS) -c src/account.c -o src/account.o

src/cache.o: src/cache.c src/cache.h
	$(CC) $(CFLAGS) -c src/cache.c -o src/cache.o

src/channel.o: src/channel.c src/channel.h
	$(CC) $(CFLAGS) -c src/channel.c -o src/channel.o

src/company.o: src/company.c src/company.h
	$(CC) $(CFLAGS) -c src/company.c -o src/company.o

src/config.o: src/config.c src/config.h
	$(CC) $(CFLAGS) -c src/config.c -o src/config.o

src/db.o: src/db.c src/db.h
	$(CC) $(CFLAGS) -c src/db.c -o src/db.o

src/gateway.o: src/gateway.c src/gateway.h
	$(CC) $(CFLAGS) -c src/gateway.c -o src/gateway.o

src/group.o: src/group.c src/group.h
	$(CC) $(CFLAGS) -c src/group.c -o src/group.o

src/queue.o: src/queue.c src/queue.h
	$(CC) $(CFLAGS) -c src/queue.c -o src/queue.o

src/utils.o: src/utils.c src/utils.h
	$(CC) $(CFLAGS) -c src/utils.c -o src/utils.o

src/security.o: src/security.c src/security.h
	$(CC) $(CFLAGS) -D_GNU_SOURCE -c src/security.c -o src/security.o

.PHONY: install clean

install:
	echo complete

clean:
	rm -f src/*.o
	rm -f client
	rm -f server

