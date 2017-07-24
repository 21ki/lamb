
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_AMQP_H
#define _LAMB_AMQP_H

#include <pthread.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

typedef struct {
    amqp_socket_t *socket;
    amqp_connection_state_t conn;
    char *exchange;
    char *key;
    amqp_bytes_t queue;
    pthread_mutex_t lock;
} lamb_amqp_t;

typedef struct {
  size_t len;
  void *data;
} lamb_pack_t;

int lamb_amqp_connect(lamb_amqp_t *amqp, const char *host, int port);
int lamb_amqp_login(lamb_amqp_t *amqp, const char *user, const char *password);
int lamb_amqp_setting(lamb_amqp_t *amqp, char *exchange, char *key);
int lamb_amqp_producer(lamb_amqp_t *amqp, char const *exchange, const char *type, char const *key);
int lamb_amqp_consume(lamb_amqp_t *amqp, char const *queue);
int lamb_amqp_push_message(lamb_amqp_t *amqp, void *data, size_t len);
int lamb_amqp_pull_message(lamb_amqp_t *amqp, lamb_pack_t *pack, long long milliseconds);
int lamb_amqp_destroy_connection(lamb_amqp_t *amqp);
int lamb_free_pack(lamb_pack_t *pack);

#endif
