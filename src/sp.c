
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <cmpp.h>
#include "sp.h"
#include "utils.h"
#include "config.h"
#include "list.h"
#include "amqp.h"
#include "cache.h"

#define MAX_LIST 16

cmpp_sp_t cmpp;
lamb_list_t *send_queue;
lamb_list_t *recv_queue;
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

    send = lamb_list_new();
    recv = lamb_list_new();
    lamb_db_init(&cache, "cache");

    if(lamb_cmpp_init() == 0) {
        /* Start worker thread */
        lamb_event_loop();
    }

    return 0;
}

void lamb_fetch_loop(void) {
    int i, len, err;
    pthread_t tid;
    pthread_attr_t attr;
    char *list[MAX_LIST] = {NULL};
    len = lamb_str2list(config.queue, list, MAX_LIST);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    
    for (i = 0; i < MAX_LIST; i++) {
        if (list[i] != NULL) {
            err = pthread_create(&fetch_pool[i], &attr, lamb_fetch_work, (void *)list[i]);
            if (err == -1) {
                lamb_errlog(config.logfile, "start %s queue work thread failed", list[i]);
            }
            continue;
        }
        break;
    }

    if (len < 1) {
        lamb_errlog(config.logfile, "no queues available at the moment", 0);
    }

    return;
}

void *lamb_fetch_work(void *queue) {
    int err;
    lamb_amqp_t amqp;
    err = lamb_amqp_connect(&amqp, config.db_host, config.db_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connection to amqp server", 0);
    }

    err = lamb_amqp_login(&amqp, config.db_user, config.db_password);
    if (err) {
        lamb_errlog(config.logfile, "login amqp server failed", 0);
    }

    err = lamb_amqp_consume(&amqp, (char const *)queue);
    if (err) {
        lamb_errlog(config.logfile, "set consume mode failed", 0);
    }

    while (true) {
        if (send_queue->len < config.send) {
            lamb_pack_t *pack = malloc(sizeof(lamb_pack_t));
            err = lamb_amqp_pull_message(&amqp, pack, 0);
            if (err) {
                lamb_free_pack(pack);
                lamb_errlog(config.logfile, "pull amqp message error", 0);
                continue;
            }

            pthread_mutex_lock(&send_queue->lock);
            if (lamb_list_rpush(send_queue, lamb_list_node_new(pack)) == NULL) {
                lamb_errlog(config.logfile, "push queue message error", 0);
            }
            pthread_mutex_unlock(&send_queue->lock);
            
            continue;
        }
        
        lamb_sleep(10);
    }
    
    lamb_amqp_destroy_connection(&amqp);
    pthread_exit(NULL);

    return NULL;
}

void *lamb_send_loop(void *data) {
    lamb_list_node_t *node;

    while (true) {
        if (unconfirmed >= config.unconfirmed) {
            lamb_sleep(10);
            continue;
        }
        
        node = lamb_list_lpop(send_queue);
        if (node != NULL) {
            lamb_pack_t *pack = (lamb_pack_t *)node->val;
            lamb_submit_t *message = (lamb_submit_t *)pack->data;

            /* code */
            char seq[64];
            char msgId[64];
            unsigned long seqid;
            char *phone = (char *)message->destTerminalId;
            char *content = (char *)message->msgContent;

            seqid = cmpp_submit(&cmpp, phone, content, true, config.spcode, config.encoding, config.spid);
            if (seqid > 0) {
                unconfirmed++;
                sprintf(seq, "%ld", seqid);
                sprintf(msgId, "%lld", message->msgId);
                lamb_db_put(&cache, seq, strlen(seq), msgId, strlen(msgId));
            }

            lamb_free_pack(pack);
            free(node);
            continue;
        }

        lamb_sleep(10);
    }

    return NULL;
}

void *lamb_recv_loop(void *data) {
    int err;
    cmpp_pack_t *pack;

    while (true) {
        if (!cmpp.ok) {
            lamb_errlog(config.logfile, "cmpp gateway %s connection error", config.host);
            lamb_sleep(3000);
            continue;
        }

        pack = malloc(sizeof(cmpp_pack_t));
        err = cmpp_recv(&cmpp, pack, sizeof(cmpp_pack_t));
        if (err) {
            cmpp_free_pack(pack);
            lamb_errlog(config.logfile, "cmpp %s", cmpp_get_error(err));
            lamb_sleep(10);
            continue;
        }

        switch (pack->commandId) {
        case CMPP_SUBMIT_RESP:;
            unsigned char result;
            lamb_update_t *update;

            unconfirmed--;
            cmpp_pack_get_integer(pack, cmpp_submit_resp_result, (void *)&result, 1);
            if (result != 0) {
                cmpp_free_pack(pack);
                lamb_errlog(config.logfile, "receive cmpp_submit_resp packet error result = %d", result);
                continue;
            }
	    
            lamb_pack_t *lpack;
            update = malloc(sizeof(lamb_update_t));
            lpack = malloc(sizeof(lamb_pack_t));
	    
            update->type = LAMB_UPDATE;
            cmpp_pack_get_integer(pack, cmpp_submit_resp_msg_id, (void *)update->msgId, 8);
	    
            char seqid[64];
            unsigned long sequenceId;
            cmpp_pack_get_integer(pack, cmpp_sequence_id , (void *)&sequenceId, 4);
            sprintf(seqid, "%ld", sequenceId);
	    
            size_t len;
            unsigned long long id;
            id = atoll(lamb_db_get(&cache, seqid, strlen(seqid), &len));
            if (id > 0) {
                update->id =id;
                lpack->len = sizeof(lamb_update_t);
                lpack->data = update;
                pthread_mutex_lock(&recv_queue->lock);
                lamb_list_rpush(recv_queue, lamb_list_node_new(lpack));
                pthread_mutex_unlock(&recv_queue->lock);
            } else {
                free(update);
                free(lpack);
            }
            break;
        case CMPP_DELIVER:;
            unsigned char registered_delivery;
            lamb_pack_t *delpack = malloc(sizeof(lamb_pack_t));
            cmpp_pack_get_integer(&pack, cmpp_deliver_registered_delivery, (void *)&registered_delivery, 1);

            switch (registered_delivery) {
            case 0:; /* message delivery */
                lamb_deliver_t *deliver = malloc(sizeof(lamb_deliver_t));
                deliver->type = LAMB_DELIVER;
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_id, (void *)&deliver->msgId, 8);
                cmpp_pack_get_string(pack, cmpp_deliver_dest_id, deliver->destId, 24, 21);
                cmpp_pack_get_string(pack, cmpp_deliver_service_id, deliver->serviceId, 16, 10);
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_fmt, (void *)&deliver->msgFmt, 1);
                cmpp_pack_get_string(pack, cmpp_deliver_src_terminal_id, deliver->srcTerminalId, 24, 21);
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_length, (void *)&deliver->msgLength, 1);
                cmpp_pack_get_string(pack, cmpp_deliver_msg_content, deliver->msgContent, 160, deliver->msgLength);
                delpack->len = sizeof(lamb_deliver_t);
                delpack->data = deliver;
                pthread_mutex_lock(&recv_queue->lock);
                lamb_list_rpush(recv_queue, lamb_list_node_new(delpack));
                pthread_mutex_unlock(&recv_queue->lock);
                break;
            case 1:;
                /* message status report */ 
                lamb_report_t *report = malloc(sizeof(lamb_report_t));
                report->type = LAMB_REPORT;
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content , (void *)&report->msgId, 8);
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content + 8, report->stat, 8, 7);
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content + 35, report->destTerminalId, 24, 21);
                delpack->len = sizeof(lamb_report_t);
                delpack->data = report;
                pthread_mutex_lock(&recv_queue->lock);
                lamb_list_rpush(recv_queue, lamb_list_node_new(delpack));
                pthread_mutex_unlock(&recv_queue->lock);
                break;
            }
	    case CMPP_ACTIVE_TEST:;
            //cmpp_active_test_resp(&cmpp);
            break;
	    default:;
            break;
        }
    }

    return NULL;
}

void *lamb_update_loop(void *data) {
    int err;
    lamb_amqp_t amqp;

    err = lamb_amqp_connect(&amqp, config.db_host, config.db_port);
    if (err) {
        lamb_errlog(config.logfile, "can't connection to %s amqp server", config.db_host);
    }

    err = lamb_amqp_login(&amqp, config.db_user, config.db_password);
    if (err) {
        lamb_errlog(config.logfile, "login amqp server %s failed", config.db_host);
    }

    lamb_amqp_setting(&amqp, "lamb.inbox", "inbox");

    while (true) {
        lamb_pack_t *pack;
        lamb_list_node_t *node;

        pthread_mutex_lock(&recv_queue->lock);
        node = lamb_list_lpop(recv_queue);
        pthread_mutex_unlock(&recv_queue->lock);
        if (node != NULL) {
            pack = (lamb_pack_t *)node->val;

            /* code */
            err = lamb_amqp_push_message(&amqp, pack->data, pack->len);
            if (err) {
                lamb_errlog(config.logfile, "amqp push message failed", 0);
            }

            lamb_free_pack(pack);
            free(node);
            continue;
        }

        lamb_sleep(10);
    }

    return NULL;
}

int lamb_cmpp_init(void) {
    int err;

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_CONTIMEOUT, config.timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.recv_timeout);

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
    int err;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    
    err = pthread_create(&thread, &attr, lamb_update_loop, NULL);
    if (err) {
        lamb_errlog(config.logfile, "start lamb_update_loop thread failed", 0);
    }
    
    err = pthread_create(&thread, &attr, lamb_recv_loop, NULL);
    if (err) {
        lamb_errlog(config.logfile, "start lamb_recv_loop thread failed", 0);
    }

    err = pthread_create(&thread, &attr, lamb_send_loop, NULL);
    if (err) {
        lamb_errlog(config.logfile, "start lamb_send_loop thread failed", 0);
    }
    
    lamb_fetch_loop();

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

    if (lamb_get_string(&cfg, "encoding", conf->encoding, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read encoding parameter\n");
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
