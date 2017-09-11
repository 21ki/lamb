
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
MACRO = -D_POSIX_C_SOURCE=200112L
OBJS = src/account.o src/cache.o src/channel.o src/company.o src/config.o src/db.o src/gateway.o src/group.o src/queue.o src/utils.o src/security.o src/list.o src/template.o src/keyword.o src/route.o src/message.o
LIBS = -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lpq -lhiredis -lrt -lpcre

all: sp ismg server deliver lamb test

sp: src/sp.c src/sp.h $(OBJS)
	$(CC) $(CFLAGS) $(MACRO) src/sp.c $(OBJS) $(LIBS) -o sp

ismg: src/ismg.c src/ismg.h $(OBJS)
	$(CC) $(CFLAGS) $(MACRO) src/ismg.c $(OBJS) $(LIBS) -o ismg

server: src/server.c src/server.h $(OBJS)
	$(CC) $(CFLAGS) $(MACRO) src/server.c $(OBJS) $(LIBS) -lnanomsg -o server

deliver: src/deliver.c src/deliver.h $(OBJS)
	$(CC) $(CFLAGS) $(MACRO) src/deliver.c $(OBJS) $(LIBS) -o deliver

lamb: src/lamb.c src/lamb.h
	$(CC) $(CFLAGS) $(MACRO) src/lamb.c -o lamb

test: src/test.c src/test.h src/utils.o src/queue.o
	$(CC) $(CFLAGS) $(MACRO) src/test.c src/utils.o src/queue.o $(LIBS) -o test

src/account.o: src/account.c src/account.h
	$(CC) $(CFLAGS) $(MACRO) -c src/account.c -o src/account.o

src/cache.o: src/cache.c src/cache.h
	$(CC) $(CFLAGS) $(MACRO) -c src/cache.c -o src/cache.o

src/channel.o: src/channel.c src/channel.h
	$(CC) $(CFLAGS) $(MACRO) -c src/channel.c -o src/channel.o

src/company.o: src/company.c src/company.h
	$(CC) $(CFLAGS) $(MACRO) -c src/company.c -o src/company.o

src/config.o: src/config.c src/config.h
	$(CC) $(CFLAGS) $(MACRO) -c src/config.c -o src/config.o

src/db.o: src/db.c src/db.h
	$(CC) $(CFLAGS) $(MACRO) -c src/db.c -o src/db.o

src/gateway.o: src/gateway.c src/gateway.h
	$(CC) $(CFLAGS) $(MACRO) -c src/gateway.c -o src/gateway.o

src/group.o: src/group.c src/group.h
	$(CC) $(CFLAGS) $(MACRO) -c src/group.c -o src/group.o

src/queue.o: src/queue.c src/queue.h
	$(CC) $(CFLAGS) $(MACRO) -c src/queue.c -o src/queue.o

src/utils.o: src/utils.c src/utils.h
	$(CC) $(CFLAGS) $(MACRO) -c src/utils.c -o src/utils.o

src/security.o: src/security.c src/security.h
	$(CC) $(CFLAGS) $(MACRO) -c src/security.c -o src/security.o

src/list.o: src/list.c src/list.h
	$(CC) $(CFLAGS) $(MACRO) -c src/list.c -o src/list.o

src/keyword.o: src/keyword.c src/keyword.h
	$(CC) $(CFLAGS) $(MACRO) -c src/keyword.c -o src/keyword.o

src/template.o: src/template.c src/template.h
	$(CC) $(CFLAGS) $(MACRO) -c src/template.c -o src/template.o

src/route.o: src/route.c src/route.h
	$(CC) $(CFLAGS) $(MACRO) -c src/route.c -o src/route.o

src/message.o: src/message.c src/message.h
	$(CC) $(CFLAGS) $(MACRO) -c src/message.c -o src/message.o

.PHONY: install clean

install:
	/usr/bin/mkdir -p /usr/local/lamb/bin
	/usr/bin/install -m 750 ismg /usr/local/lamb/bin
	/usr/bin/install -m 750 server /usr/local/lamb/bin
	/usr/bin/install -m 750 deliver /usr/local/lamb/bin
	/usr/bin/install -m 750 sp /usr/local/lamb/bin
	/usr/bin/install -m 750 test /usr/local/lamb/bin

clean:
	rm -f src/*.o
	rm -f ismg server deliver sp

