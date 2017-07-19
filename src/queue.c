
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <pthread.h>
#include "queue.h"

void lamb_queue_init(lamb_queue_t *queue) {
    queue->list = list_new();
    pthread_mutex_init(queue->lock, NULL);
    return;
}

list_node_t *lamb_queue_rpush(lamb_queue_t *queue, void *data) {
    list_node_t *node;
    pthread_mutex_lock(&queue->lock);
    node = list_rpush(queue->list, list_node_new(data));
    pthread_mutex_unlock(&queue->lock);
    return node;
}

list_node_t *lamb_queue_lpop(lamb_queue_t *queue) {
    list_node_t *node;
    pthread_mutex_lock(&queue->lock);
    node = list_lpop(queue->list);
    pthread_mutex_unlock(&queue->lock);
    return node;
}

void lamb_queue_remove(lamb_queue_t *queue, list_node_t *node) {
    pthread_mutex_lock(&queue->lock);
    list_remove(queue->list, node);
    pthread_mutex_unlock(&queue->lock);
    return;
}

void lamb_queue_destroy(lamb_queue_t *queue) {
    list_destroy(queue->list);
    return;
}
