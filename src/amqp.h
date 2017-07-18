
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_AMQP_H
#define _LAMB_AMQP_H

#include <pthread.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

typedef struct {
    amqp_socket_t *scoket;
    amqp_connection_state_t conn;
    char *exchange;
    char *key;
    amqp_bytes_t queue;
    pthread_mutex_t lock;
} lamb_amqp_t;

#endif
