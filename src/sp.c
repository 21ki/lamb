
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <cmpp2.h>
#include "sp.h"
#include "config.h"
#include "common.h"
#include "queue.h"
#include "db.h"

#define MAX_LIST 16

cmpp_sp_t cmpp;
lamb_queue_t *send;
lamb_queue_t *recv;
lamb_config_t config;
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
    if (!lamb_read_config(&config, file)) {
        return -1;
    }

    /* Daemon mode */
    if (conifg.daemon) {
        //lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal();

    /* Initialization message queue */
    lamb_queue_init(send);
    lamb_queue_init(recv);

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
    len = lamb_str2list(config.queue, MAX_LIST);

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
        memset(buff, 0, sizeof(buff));
        if (send->list->len < config.send) {
            void *buff = malloc(512);
            err = lamb_amqp_pull_message(&amqp, buff, 512, 0);
            if (err) {
                free(buff);
                lamb_errlog(config.logfile, "pull amqp message error");
                continue;
            }
            
            if (lamb_queue_rpush(send, buff) == NULL) {
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

    while (true) {
        node = lamb_queue_lpop(send);
        if (node != NULL) {
            printf("%s\n", (char *)node->val);
            free(node);
        }

        lamb_sleep(50);
    }

    /* 
       int err;
       list_node_t *node;
       CMPP_SUBMIT_T *pack;

       cmpp_init_sp(&cmpp, "ip", "port");
       cmpp_connect(&cmpp, "901234", "123456");

       while (true) {
       node = lamb_queue_lpop(send);
       if (node != NULL) {
       pack = (CMPP_SUBMIT_T *)node->val;
       if (cmpp_submit(&cmpp, pack->phone, pack->message, pack->delivery,
       pack->serviceId, pack->msgFmt, pack->msgSrc)) {
       free(node);
       continue;
       }

       if (!cmpp_check_connnect(&cmpp)) {
       while (!cmpp_reconnect()) {
       lamb_errlog(config.logfile, "cmpp server connection failed");
       lamb_sleep(3000);
       }
       }

       free(node);
       }

       lamb_sleep(10);
       }
    */
}

void lamb_recv_loop(void) {
    /* 
       CMPP_PACK_T pack;
       CMPP_HEAD_T *head;
       CMPP_SUBMIT_RESP_T *resp;

       while (true) {
       if (!cmpp_recv(&cmpp, &pack, sizeof(pack))) {
       if (cmpp_check_connnect(&cmpp)) {
       continue;
       }

       while (!cmpp_reconnect()) {
       lamb_errlog(config.logfile, "cmpp server connection failed");
       lamb_sleep(3000);
       }
       }

       head = (CMPP_HEAD_T *)pack;
       if (head->commandId == CMPP_SUBMIT_RESP) {
       resp = (CMPP_SUBMIT_RESP_T *)pack;
       lamb_queue_rpush(recv, (void *)resp->msgId);
       }
       }
    
       return;
    */
}

void lamb_update_loop(void) {
    /* 
       int err;
       list_node_t *node;
       while (true) {
       node = lamb_queue_lpop(recv);
       if (!node) {
       lamb_sleep(10);
       continue;
       }
        
       err = lamb_report_update(&db, node->val);
       if (err) {
       lamb_errlog(config.logfile, "update message report to database error");
       lamb_sleep(100);
       }

       free(node);
       }
    */
}

int lamb_cmpp_init(void) {
    int err;

    err = cmpp_init_sp(&cmpp, config.host, config.port);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to cmpp %s server", config.host);
        return -1;
    }

    err = cmpp_connect(&cmpp, config.user, config.password);
    if (err) {
        lamb_errlog(config.logfile, "login cmpp %s gateway failed", config.host);
        return 0;
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
    if (!config) {
        return -1;
    }

    config_t cfg;
    if (!lamb_read_config(&cfg, file)) {
        fprintf(stderr, "ERROR: Can't open the %s configuration file\n", file);
        goto error;
    }

    if (!lamb_get_bool(&cfg, "debug", &conf->debug)) {
        fprintf(stderr, "ERROR: Can't read debug parameter\n");
        goto error;
    }

    if (!lamb_get_bool(&cfg, "daemon", &conf->daemon)) {
        fprintf(stderr, "ERROR: Can't read daemon parameter\n");
        goto error;
    }
        
    if (!lamb_get_string(&cfg, "host", conf->host, 16)) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "port", &conf->port)) {
        fprintf(stderr, "ERROR: Can't read port parameter\n");
        goto error;
    }

    if (config->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid host port number\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "user", &conf->user, 64)) {
        fprintf(stderr, "ERROR: Can't read user parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "password", &conf->password, 128)) {
        fprintf(stderr, "ERROR: Can't read password parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "spid", &conf->spid, 8)) {
        fprintf(stderr, "ERROR: Can't read spid parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "spcode", &conf->spcode, 16)) {
        fprintf(stderr, "ERROR: Can't read spcode parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "send_queue", conf->send)) {
        fprintf(stderr, "ERROR: Can't read send_queue parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "recv_queue", conf->recv)) {
        fprintf(stderr, "ERROR: Can't read recv_queue parameter\n");
        goto error;
    }
    
    if (!lamb_get_int(&cfg, "confirmed", conf->unconfirmed)) {
        fprintf(stderr, "ERROR: Can't read confirmed parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "connect_timeout", conf->timeout)) {
        fprintf(stderr, "ERROR: Can't read connect_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "send_timeout", conf->send_timeout)) {
        fprintf(stderr, "ERROR: Can't read send_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "recv_timeout", conf->recv_timeout)) {
        fprintf(stderr, "ERROR: Can't read recv_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "logfile", &conf->logfile, 128)) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "db_host", &conf->db_host, 16)) {
        fprintf(stderr, "ERROR: Can't read db_host parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "db_port", conf->db_port)) {
        fprintf(stderr, "ERROR: Can't read db_port parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "db_user", &conf->db_user, 64)) {
        fprintf(stderr, "ERROR: Can't read db_user parameter\n");
        goto error;
    }

    if (config->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid db_port number parameter\n");
        goto error;
    }
        
    if (!lamb_get_string(&cfg, "db_password", &conf->db_password, 128)) {
        fprintf(stderr, "ERROR: Can't read db_password parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "queue", &conf->queue, 128)) {
        fprintf(stderr, "ERROR: Can't read queue parameter\n");
        goto error;
    }
        
    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
