
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-08-24
 */

#include "socket.h"
#include "amqp.h"

lamb_amqp_t *lamb_amqp_new(void) {
    lamb_amqp_t *self;

    self = (lamb_amqp_t *)calloc(1, sizeof(lamb_amqp_t));

    if (self) {
        self->fd = lamb_sock_create();
        if (self->fd > 0) {
            self->ack = false;
            pthread_mutex_init(&self->lock, NULL);
        } else {
            free(self);
            self = NULL;
        }
    }

    return self;
}

void lamb_amqp_bind(lamb_amqp_t *amqp, const char *addr, unsigned short port, int backlog) {
    return lamb_sock_bind(amqp->fd, addr, port, backlog);
}

void lamb_amqp_connect(lamb_amqp_t *amqp, const char *addr, unsigned short port, unsigned long long timeout) {
    return lamb_sock_connect(amqp->fd, addr, port, timeout);
}

int lamb_amqp_push(lamb_amqp_t *amqp, const char *val, size_t len, unsigned long long timeout) {
    int ret;
    lamb_amqp_message message;

    message.len = len;
    message.type = LAMB_AMQP_REQ;
    memcpy(message.val, val, len > LAMB_AMQP_MAX_LENGTH ? LAMB_AMQP_MAX_LENGTH : len);

    pthread_mutex_lock(&amqp->lock);

    ret = lamb_sock_send(amqp->fd, (char *)&message, 4 + len, timeout);
    if (ret != len) {
        pthread_mutex_unlock(&amqp->lock);
        return -1;
    }

    if (amqp->ack) {
        ret = lamb_sock_recv(amqp->fd, (char *)&message, 4, timeout);
        if (ret != 4 || message.type != LAMB_AMQP_ACK) {
            pthread_mutex_unlock(&amqp->lock);
            return -1;
        }
    }
    
    pthread_mutex_unlock(&amqp->lock);

    return 0;
}

int lamb_amqp_pull(lamb_amqp_t *amqp, char *val, size_t len, unsigned long long timeout) {
    int ret, err, length;
    lamb_amqp_message message;

    memset(&message, 0, sizeof(message));
    
    message.len = 0;
    message.type = LAMB_AMQP_REQ;
    
    pthread_mutex_lock(&amqp->lock);

    ret = lamb_sock_send(amqp->fd, (char *)&message, 4, timeout);

    if (ret != 4) {
        pthread_mutex_unlock(&amqp->lock);
        return -1;
    }

    ret = lamb_sock_recv(amqp->fd, (char *)&message, 4, timeout);

    if (ret != 4 || message.type != LAMB_AMQP_RESP) {
        pthread_mutex_unlock(&amqp->lock);
        return -1;
    }

    length = message.len > len ? len : message.len;
    ret = lamb_sock_recv(amqp->fd, val, length);

    if (ret != length) {
        pthread_mutex_unlock(&amqp->lock);
        return -1;
    }

    pthread_mutex_unlock(&amqp->lock);

    return 0;
}

void lamb_amqp_ack(lamb_amqp_t *amqp) {
    unsigned char bit = 0xff;
    lamb_sock_send(fd, (char *)&bit, 1, 3000);
    return;
}

void lamb_amqp_reject(lamb_amqp_t *amqp) {
    char bit = 0x0;

    lamb_sock_send(amqp->fd, (char *)&bit, 1, 3000);

    return;
}

void lamb_amqp_destroy(lamb_amqp_t *amqp) {
    close(amqp->fd);
    pthread_mutex_destroy(&amqp->lock);
    free(amqp);
    return;
}
