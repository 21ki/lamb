
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
#include <arpa/inet.h>
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
pthread_t fpool[MAX_LIST];


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

    send_queue = lamb_list_new();
    recv_queue = lamb_list_new();
    lamb_db_init(&cache, "cache");

    if(lamb_cmpp_init(&cmpp) == 0) {
        /* Start worker thread */
        lamb_event_loop();
    }

    return 0;
}

void lamb_fetch_loop(void) {
    int i, len, err;
    pthread_attr_t attr;
    char *list[MAX_LIST] = {NULL};
    len = lamb_str2list(config.queue, list, MAX_LIST);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    
    for (i = 0; i < MAX_LIST; i++) {
        if (list[i] != NULL) {
            err = pthread_create(&fpool[i], &attr, lamb_fetch_work, (void *)list[i]);
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
        pthread_exit(NULL);
    }

    err = lamb_amqp_login(&amqp, config.db_user, config.db_password);
    if (err) {
        lamb_errlog(config.logfile, "login amqp server failed", 0);
        lamb_amqp_destroy_connection(&amqp);
        pthread_exit(NULL);
    }

    err = lamb_amqp_consume(&amqp, (char const *)queue);
    if (err) {
        lamb_errlog(config.logfile, "set consume mode failed", 0);
        lamb_amqp_destroy_connection(&amqp);
        pthread_exit(NULL);
    }

    while (true) {
        if (send_queue->len < config.send) {
            lamb_list_node_t *node;
            lamb_pack_t *pack = malloc(sizeof(lamb_pack_t));
            err = lamb_amqp_pull_message(&amqp, pack, 0);
            if (err) {
                lamb_free_pack(pack);
                lamb_errlog(config.logfile, "pull amqp message error", 0);                    
                continue;
            }

            pthread_mutex_lock(&send_queue->lock);
            node = lamb_list_rpush(send_queue, lamb_list_node_new(pack));
            pthread_mutex_unlock(&send_queue->lock);
            
            if (node == NULL) {
                lamb_free_pack(pack);
                lamb_errlog(config.logfile, "push queue message error", 0);
            }
            
            continue;
        }
        
        lamb_sleep(10);
    }

    return NULL;
}

void *lamb_send_loop(void *data) {
    lamb_list_node_t *node;

    while (true) {
        if (unconfirmed >= config.unconfirmed) {
            lamb_sleep(10);
            continue;
        }

        pthread_mutex_lock(&send_queue->lock);
        node = lamb_list_lpop(send_queue);
        pthread_mutex_unlock(&send_queue->lock);
        if (node != NULL) {
            lamb_pack_t *pack = (lamb_pack_t *)node->val;
            lamb_submit_t *message = (lamb_submit_t *)pack->data;

            /* code */
            char seq[64];
            char msgId[64];
            unsigned long seqid;
            char *phone = (char *)message->destTerminalId;
            char *content = (char *)message->msgContent;

            err = cmpp_submit(&cmpp.sock, phone, content, true, config.spcode, config.encoding, config.spid);
            if (err) {
                if (err == -1) {
                    if (!cmpp_check_connect(&cmpp.sock)) {
                        cmpp.ok = false;
                        cmpp_sp_close(&cmpp);
                        lamb_cmpp_reconnect(&cmpp, &config);
                    }
                }

                lamb_free_pack(pack);
                free(node);
                lamb_errlog(config.logfile, "cmpp submit message failed", 0);
                continue;
            }

            unconfirmed++;
            sprintf(seq, "%ld", seqid);
            sprintf(msgId, "%lld", message->msgId);
            lamb_db_put(&cache, seq, strlen(seq), msgId, strlen(msgId));
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
        err = cmpp_recv(&cmpp.sock, pack, sizeof(cmpp_pack_t));
        if (err) {
            cmpp_free_pack(pack);
            lamb_sleep(1000);
            continue;
        }

        if (recv_queue->len >= config.recv) {
            cmpp_free_pack(pack);
            lamb_errlog(config.logfile, "the receive queue is full", 0);
            lamb_sleep(1000);
            continue;
        }

        switch (ntohl(pack->commandId)) {
        case CMPP_SUBMIT_RESP:;
            unconfirmed--;
            unsigned char result;

            cmpp_pack_get_integer(pack, cmpp_submit_resp_result, &result, 1);
            if (result != 0) {
                cmpp_free_pack(pack);
                lamb_errlog(config.logfile, "receive cmpp_submit_resp packet error result = %d", result);
                continue;
            }
	    
            lamb_pack_t *lpack;
            lamb_update_t *update;
            update = malloc(sizeof(lamb_update_t));
            lpack = malloc(sizeof(lamb_pack_t));
	    
            update->type = LAMB_UPDATE;
            cmpp_pack_get_integer(pack, cmpp_submit_resp_msg_id, &update->msgId, 8);
	    
            char seqid[64];
            unsigned long sequenceId;
            cmpp_pack_get_integer(pack, cmpp_sequence_id , &sequenceId, 4);
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
            cmpp_pack_get_integer(pack, cmpp_deliver_registered_delivery, &registered_delivery, 1);
            memset(delpack, 0, sizeof(lamb_pack_t));
            switch (registered_delivery) {
            case 0:; /* message delivery */
                lamb_deliver_t *deliver = malloc(sizeof(lamb_deliver_t));
                deliver->type = LAMB_DELIVER;
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_id, &deliver->msgId, 8);
                cmpp_pack_get_string(pack, cmpp_deliver_dest_id, deliver->destId, 24, 21);
                cmpp_pack_get_string(pack, cmpp_deliver_service_id, deliver->serviceId, 16, 10);
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_fmt, &deliver->msgFmt, 1);
                cmpp_pack_get_string(pack, cmpp_deliver_src_terminal_id, deliver->srcTerminalId, 24, 21);
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_length, &deliver->msgLength, 1);
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
                cmpp_pack_get_integer(pack, cmpp_deliver_msg_content , &report->msgId, 8);
                cmpp_pack_get_string(pack, cmpp_deliver_msg_content + 8, report->stat, 8, 7);
                cmpp_pack_get_string(pack, cmpp_deliver_msg_content + 35, report->destTerminalId, 24, 21);
                delpack->len = sizeof(lamb_report_t);
                delpack->data = report;
                pthread_mutex_lock(&recv_queue->lock);
                lamb_list_rpush(recv_queue, lamb_list_node_new(delpack));
                pthread_mutex_unlock(&recv_queue->lock);
                break;
            default:
                free(delpack);
                break;
            }
            break;
	    case CMPP_ACTIVE_TEST:;
            unsigned int seq_id;
            cmpp_pack_get_integer(pack, cmpp_sequence_id, &seq_id, 4);
            cmpp_active_test_resp(&cmpp.sock, seq_id);
            break;
	    default:
            break;
        }

        if (pack != NULL) {
            cmpp_free_pack(pack);
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
            if (err != LAMB_AMQP_STATUS_OK) {
                lamb_errlog(config.logfile, "amqp push message failed", 0);
                lamb_sleep(1000);
            }

            lamb_free_pack(pack);
            free(node);
            continue;
        }

        lamb_sleep(10);
    }

    return NULL;
}

void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config) {
    int err;
    pthread_mutex_lock(&cmpp->lock);

    /* Initialization cmpp connection */
    while ((err = cmpp_init_sp(cmpp, config->host, config->port)) != 0) {
        lamb_errlog(config->logfile, "can't connect to cmpp %s server", config->host);
        lamb_sleep(cmpp->sock.conTimeout);
    }

    /* login to cmpp gateway */
    while ((err = cmpp_connect(&cmpp->sock, config->user, config->password)) != 0) {
        lamb_errlog(config->logfile, "login cmpp %s gateway failed", config->host);
        lamb_sleep(cmpp->sock.conTimeout);
    }

    pthread_mutex_unlock(&cmpp->lock);
    return;
}

int lamb_cmpp_init(cmpp_sp_t *cmpp) {
    int err;
    cmpp_pack_t pack;

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_CONTIMEOUT, config.timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_RECVTIMEOUT, config.recv_timeout);

    /* Initialization cmpp connection */
    err = cmpp_init_sp(cmpp, config.host, config.port);
    if (err) {
        if (config.daemon) {
            lamb_errlog(config.logfile, "can't connect to cmpp %s server", config.host);
        } else {
            fprintf(stderr, "can't connect to cmpp %s server\n", config.host);
        }
        return -1;
    }

    /* login to cmpp gateway */
    err = cmpp_connect(&cmpp->sock, config.user, config.password);
    if (err) {
        if (config.daemon) {
            lamb_errlog(config.logfile, "send cmpp_connect() failed", 0);
        } else {
            fprintf(stderr, "send cmpp_connect() failed\n");
        }
        return -1;
    }

    err = cmpp_recv(&cmpp->sock, &pack, sizeof(pack));
    if (err) {
        fprintf(stderr, "cmpp cmpp_recv() failed\n");
        return -1;
    }

    if (cmpp_check_method(&pack, sizeof(pack), CMPP_CONNECT_RESP)) {
        unsigned char status;
        cmpp_pack_get_integer(&pack, cmpp_connect_resp_status, &status, 1);
        switch (status) {
        case 0:
            cmpp->ok = true;
            return 0;
        case 1:
            lamb_errlog(config.logfile, "Incorrect protocol packets", 0);
            break;
        case 2:
            lamb_errlog(config.logfile, "Illegal source address", 0);
            break;
        case 3:
            lamb_errlog(config.logfile, "Authenticator failed", 0);
            break;
        case 4:
            lamb_errlog(config.logfile, "The protocol version is too high", 0);
            break;
        default:
            lamb_errlog(config.logfile, "cmpp login failed unknown error", 0);
            break;
        }

        return -1;
    }

    lamb_errlog(config.logfile, "Cannot resolve response packet", 0);
    return -1;
}

void lamb_event_loop(void) {
    int err;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    
    err = pthread_create(&tid, &attr, lamb_update_loop, NULL);
    if (err) {
        lamb_errlog(config.logfile, "start lamb_update_loop thread failed", 0);
    }
    
    err = pthread_create(&tid, &attr, lamb_recv_loop, NULL);
    if (err) {
        lamb_errlog(config.logfile, "start lamb_recv_loop thread failed", 0);
    }

    err = pthread_create(&tid, &attr, lamb_send_loop, NULL);
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

    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "ERROR: Can't read Debug parameter\n");
        goto error;
    }

    if (lamb_get_bool(&cfg, "Daemon", &conf->daemon) != 0) {
        fprintf(stderr, "ERROR: Can't read Daemon parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "Host", conf->host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read Port parameter\n");
        goto error;
    }

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid host Port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "User", conf->user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read User parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Password", conf->password, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read Password parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Spid", conf->spid, 8) != 0) {
        fprintf(stderr, "ERROR: Can't read Spid parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "SpCode", conf->spcode, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read SpCode parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Encoding", conf->encoding, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read Encoding parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "SendQueue", &conf->send) != 0) {
        fprintf(stderr, "ERROR: Can't read SendQueue parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RecvQueue", &conf->recv) != 0) {
        fprintf(stderr, "ERROR: Can't read RecvQueue parameter\n");
        goto error;
    }
    
    if (lamb_get_int(&cfg, "Unconfirmed", &conf->unconfirmed) != 0) {
        fprintf(stderr, "ERROR: Can't read Unconfirmed parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "ConnectTimeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read ConnectTimeout parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "SendTimeout", &conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read SendTimeout parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "RecvTimeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read RecvTimeout parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read LogFile parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "AmqpHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read AmqpHost parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "AmqpPort", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read AmqpPort parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "AmqpUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read AmqpUser parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid AmqpPort number parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "AmqpPassword", conf->db_password, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read AmqpPassword parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Queues", conf->queues, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read Queues parameter\n");
        goto error;
    }
        
    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
