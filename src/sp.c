
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cmpp.h>
#include "sp.h"
#include "config.h"
#include "common.h"
#include "queue.h"
#include "amqp.h"

#define MAX_LIST 16

cmpp_sp_t cmpp;
lamb_queue_t send;
lamb_queue_t recv;
lamb_config_t config;
lamb_db_t cache;
int unconfirmed = 0;
pthread_t fetch_pool[MAX_LIST];


int main(int argc, char *argv[]) {
    char *file = "lamb.conf";

    int opt = 0;
    char *optstring = "c:";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'c':
            file = optarg;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    /* Read lamb configuration file */
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (config.daemon) {
        //lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal();

    /* Initialization working */
    lamb_db_init(&cache, "cache");
    lamb_queue_init(&send);
    lamb_queue_init(&recv);

    lamb_fetch_loop();
    lamb_send_loop();
    
    /* Start worker thread */

    /* 
       if(lamb_cmpp_init()) {
       lamb_fetch_loop();
       lamb_update_loop();
       lamb_send_loop();
       lamb_recv_loop();
       }
    */
    
    /* Master worker thread */
    // lamb_event_loop();

    return 0;
}

void lamb_fetch_loop(void) {
    int i, len;
    char *list[MAX_LIST] = {NULL};
    len = lamb_str2list(config.queue, list, MAX_LIST);

    for (i = 0; i < MAX_LIST; i++) {
        if (list[i] != NULL) {
            int ret;
            ret = pthread_create(&fetch_pool[i], NULL, lamb_fetch_work, (void *)list[i]);
            if (ret == -1) {
                lamb_errlog(config.logfile, "start %s queue work thread failed", list[i]);
            }
            continue;
        }
        break;
    }

    if (len < 1) {
        lamb_errlog(config.logfile, "no queues available at the moment");
    }

    return;
}

void *lamb_fetch_work(void *data) {
    int err;
    lamb_amqp_t amqp;
    err = lamb_amqp_connect(&amqp, config.db_host, config.db_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connection to amqp server");
    }

    err = lamb_amqp_login(&amqp, config.db_user, config.db_password);
    if (err) {
        lamb_errlog(config.logfile, "login amqp server failed");
    }

    err = lamb_amqp_basic_consume(&amqp, (char const *)data);
    if (err) {
        lamb_errlog(config.logfile, "set consume mode failed");
    }

    while (true) {
        if (send.list->len < config.send) {
            lamb_message_t *message = malloc(sizeof(lamb_message_t));
            err = lamb_amqp_pull_message(&amqp, message, 0);
            if (err) {
                lamb_free_message(message);
                lamb_errlog(config.logfile, "pull amqp message error");
                continue;
            }

            if (lamb_queue_rpush(&send, (void *)message) == NULL) {
                lamb_errlog(config.logfile, "push queue message error");
            }
            continue;
        }
        
        lamb_sleep(10);
    }
    
    lamb_amqp_destroy_connection(&amqp);
    pthread_exit(NULL);
    
    return;
}

void lamb_send_loop(void) {
    list_node_t *node;
    lamb_message_t *message;

    while (true) {
        if (unconfirmed >= config.unconfirmed) {
            lamb_sleep(10);
            continue;
        }
        
        node = lamb_queue_lpop(&send);
        if (node != NULL) {
            message = (lamb_message_t *)node->val;

            /* code */
            long seqid;
            char *phone = message.phone;
            char *content = message.content;

            seqid = cmpp_submit(&cmpp, phone, content, true, config.spcode, "UCS-2", config.spid);
            if (seqid > 0) {
                unconfirmed++;
                lamb_db_put(&cache, message.seqid, strlen(message.seqid), message.id, strlen(message.id));
            }
            
            free(node);
            lamb_free_message(message);
            continue;
        }

        lamb_sleep(10);
    }

    return;
}

void lamb_recv_loop(void) {
    int err;
    cmpp_pack_t *pack;

    while (true) {
        if (!cmpp.ok) {
            lamb_errlog(config.logfile, "cmpp connect status error");
            lamb_sleep(3000);
            continue;
        }
        
        pack = malloc(sizeof(cmpp_pack_t));
        err = cmpp_recv(&cmpp, pack, sizeof(pack));
        if (err) {
            cmpp_free_pack(pack);
            lamb_errlog(config.logfile, "error: %s", cmpp_get_error(err));
            lamb_sleep(10);
            continue;
        }

        switch (pack->commandId) {
        case CMPP_SUBMIT_RESP:
            size_t len;
            char seqid[64];
            char *id = NULL;
            lamb_update_t *update;
            lamb_message_t *message;
            cmpp_submit_resp_t *csrp;

            unconfirmed--;
            csrp = (cmpp_submit_resp_t *)pack;
            if (csrp->result != 0) {
                cmpp_free_pack(pack);
                lamb_errlog(config.logfile, "receive cmpp_submit_resp packet error result = %d", csrp->result);
                continue;
            }
            
            update = malloc(sizeof(lamb_update_t));
            message = malloc(sizeof(lamb_message_t));

            update.type = LAMB_UPDATE;
            update.msgId = csrp->msgId;
            sprintf(seqid, "%ld", csrp->sequenceId);
            id = lamb_db_get(&cache, seqid, strlen(seqid), &len);
            if (id > 0) {
                update.id = atoll(id);
                message.len = sizeof(lamb_update_t);
                message.data = update;
                lamb_queue_rpush(&recv, message);
            } else {
                free(update);
                free(message);
            }
            break;

        case CMPP_DELIVER:
            /* other */
            break;
        }
    }

    return;
}

void *lamb_update_loop(void *data) {
    int err;
    lamb_amqp_t amqp;

    err = lamb_amqp_connect(&amqp, config.db_host, config.db_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connection to amqp server");
    }

    err = lamb_amqp_login(&amqp, config.db_user, config.db_password);
    if (err) {
        lamb_errlog(config.logfile, "login amqp server failed");
    }

    err = lamb_amqp_producer(&amqp, "lamb.inbox", "direct", "inbox");
    
    if (err) {
        lamb_errlog(config.logfile, "amqp set producer mode failed");
    }

    while (true) {
        list_node_t *node;
        lamb_message_t *message;

        node = lamb_queue_lpop(&recv);
        if (node != NULL) {
            message = (lamb_message_t *)node->val;

            /* code */
            err = lamb_amqp_push_message(&amqp, message->data, message->len);
            if (err) {
                lamb_errlog(config.logfile, "amqp push message failed");
            }

            lamb_free_message(message);
            free(node);
            continue;
        }

        lamb_sleep(10);
    }

    lamb_amqp_destroy_connection(&amqp);
    pthread_exit(NULL);

    return NULL;
}

int lamb_cmpp_init(void) {
    int err;

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_CONTIMEOUT, config.timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_RECVTIMEOUT, config.recv_timeout);

    /* Initialization cmpp connection */
    err = cmpp_init_sp(&cmpp, config.host, config.port);
    if (err) {
        if (config.daemon) {
            fprintf(stderr, "can't connect to cmpp %s server", config.host);
        } else {
            lamb_errlog(config.logfile, "can't connect to cmpp %s server", config.host);
        }
        return -1;
    }

    /* login to cmpp gateway */
    err = cmpp_connect(&cmpp, config.user, config.password);
    if (err) {
        if (config.daemon) {
            fprintf(stderr, "login cmpp %s gateway failed", config.host);
        } else {
            lamb_errlog(config.logfile, "login cmpp %s gateway failed", config.host);
        }
        return -1;
    }

    return 0;
}

void lamb_event_loop(void) {
    while (true) {
        lamb_sleep(5000);
    }

    return;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "ERROR: Can't open the %s configuration file\n", file);
        goto error;
    }

    if (lamb_get_bool(&cfg, "debug", &conf->debug) != 0) {
        fprintf(stderr, "ERROR: Can't read debug parameter\n");
        goto error;
    }

    if (lamb_get_bool(&cfg, "daemon", &conf->daemon) != 0) {
        fprintf(stderr, "ERROR: Can't read daemon parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "host", conf->host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read port parameter\n");
        goto error;
    }

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid host port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "user", conf->user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read user parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "password", conf->password, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read password parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "spid", conf->spid, 8) != 0) {
        fprintf(stderr, "ERROR: Can't read spid parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "spcode", conf->spcode, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read spcode parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "send", &conf->send) != 0) {
        fprintf(stderr, "ERROR: Can't read send parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "recv", &conf->recv) != 0) {
        fprintf(stderr, "ERROR: Can't read recv parameter\n");
        goto error;
    }
    
    if (lamb_get_int(&cfg, "unconfirmed", &conf->unconfirmed) != 0) {
        fprintf(stderr, "ERROR: Can't read unconfirmed parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read timeout parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "send_timeout", &conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read send_timeout parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "recv_timeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read recv_timeout parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "logfile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "db_host", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read db_host parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "db_port", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read db_port parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "db_user", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read db_user parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid db_port number parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "db_password", conf->db_password, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read db_password parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "queue", conf->queue, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read queue parameter\n");
        goto error;
    }
        
    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
