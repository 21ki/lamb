
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_AMQP_H
#define _LAMB_AMQP_H

#include <pthread.h>

#define LAMB_AMQP_MAX_LENGTH 4096

#pragma pack(1)

#define LAMB_AMQP_REQ    0x01
#define LAMB_AMQP_PUSH   0x02
#define LAMB_AMQP_RESP   0x03
#define LAMB_AMQP_ACK    0x04
#define LAMB_AMQP_REJECT 0x05
#define LAMB_AMQP_PING   0x06
#define LAMB_AMQP_PONG   0x07

typedef struct {
    int fd;
    char *ip;
    pthread_mutex_t lock;
} lamb_amqp_t;

typedef struct {
    unsigned char id;
    unsigned char type;
    unsigned short len;
    char val[LAMB_AMQP_MAX_LENGTH];
} lamb_amqp_message;

#pragma pack()

void lamb_amqp_req(lamb_amqp_t *amqp);
void lamb_amqp_push();

#endif
