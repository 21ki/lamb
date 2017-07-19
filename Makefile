
dep=src/amqp.c src/amqp.h src/common.c src/common.h src/config.c src/config.h src/list.c src/list.h src/queue.c src/queue.h
obj=src/amqp.c src/common.c src/config.c src/list.c src/queue.c

all: sp

sp: src/sp.c src/sp.h $(dep)
	gcc -g src/sp.c $(obj) -pthread -lssl -lcrypto -liconv -lcmpp2 -lconfig -lrabbitmq -o client
