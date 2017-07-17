
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

lamb_db_t db;
cmpp_sp_t cmpp;
lamb_queue_t *send;
lamb_queue_t *recv;
lamb_config_t config;
int unconfirmed = 0;

int main(int argc, char *argv[]) {
    char *file = "/etc/lamb.conf";

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
        lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal();

    /* Initialization for databses */
    int err;
    err = lamb_db_connect(db, config.db_host, config.db_port, config.db_user,
                          config.db_password, config.db_name);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to postgresql database");
        return EXIT_FAILURE;
    }

    /* Initialization message queue */
    lamb_queue_init(send);
    lamb_queue_init(recv);

    /* Start worker thread */
    lamb_fetch_loop();
    lamb_update_loop();
    lamb_send_loop();
    lamb_recv_loop();

    /* Master worker thread */
    lamb_event_loop();

    return 0;
}

void lamb_fetch_loop(void) {
    lamb_rabbit_t conn;
    lamb_rabbit_data_t *data;
    lamb_rabbit_connect(conn, "user", "password");

    while (true) {
        if (send->list->len < config.queue) {
            data = lamb_rabbit_get(conn);
            lamb_queue_rpush(send, (void *)data);
            continue;
        }

        lamb_sleep(10);
    }

    return;
}

void lamb_send_loop(void) {
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
}

void lamb_recv_loop(void) {
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
}

void lamb_update_loop(void) {
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
            if (lamb_db_status(&db) != CONNECTION_OK) {
                lamb_errlog(config.logfile, "postgresql database connection error");
                lamb_sleep(1000);
                lamb_db_reset(&db);
            } else {
                lamb_errlog(config.logfile, "update message report to database error");
                lamb_sleep(100);
            }
        }

        free(node);
    }
}

int lamb_report_update(lamb_db_t *db, void *val) {
    char sql[512];
    sprintf(sql, "UPDATE message SET status = %d WHERE id = %lld", val->status, val->id);
    if (lamb_db_exec(&db, sql)) {
        return 0;
    }
    return -1;
}

int lamb_read_config(lamb_config_t *config, const char *file) {
    if (!config) {
        return -1;
    }

    config_t cfg;
    if (!lamb_read_config(&cfg, file)) {
        fprintf(stderr, "ERROR: Can't open the %s configuration file\n", file);
        goto error;
    }

    if (!lamb_get_bool(&cfg, "debug", &config->debug)) {
        fprintf(stderr, "ERROR: Can't read debug parameter\n");
        goto error;
    }

    if (!lamb_get_bool(&cfg, "daemon", &config->daemon)) {
        fprintf(stderr, "ERROR: Can't read daemon parameter\n");
        goto error;
    }
        
    if (!lamb_get_string(&cfg, "host", config->host, 16)) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "port", &config->port)) {
        fprintf(stderr, "ERROR: Can't read port parameter\n");
        goto error;
    }

    if (config->port < 1 || config->port > 65535) {
        fprintf(stderr, "ERROR: Invalid host port number\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "user", config->user, 64)) {
        fprintf(stderr, "ERROR: Can't read user parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "password", config->password, 128)) {
        fprintf(stderr, "ERROR: Can't read password parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "spid", config->spid, 8)) {
        fprintf(stderr, "ERROR: Can't read spid parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "spcode", config->spcode, 16)) {
        fprintf(stderr, "ERROR: Can't read spcode parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "send_queue", &config->send_queue)) {
        fprintf(stderr, "ERROR: Can't read send_queue parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "recv_queue", &config->recv_queue)) {
        fprintf(stderr, "ERROR: Can't read recv_queue parameter\n");
        goto error;
    }
    
    if (!lamb_get_int(&cfg, "confirmed", &config->confirmed)) {
        fprintf(stderr, "ERROR: Can't read confirmed parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "connect_timeout", &config->connect_timeout)) {
        fprintf(stderr, "ERROR: Can't read connect_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "send_timeout", &config->send_timeout)) {
        fprintf(stderr, "ERROR: Can't read send_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "recv_timeout", &config->recv_timeout)) {
        fprintf(stderr, "ERROR: Can't read recv_timeout parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "logfile", config->logfile, 128)) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "db_host", config->db_host, 16)) {
        fprintf(stderr, "ERROR: Can't read db_host parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "db_port", &config->db_port)) {
        fprintf(stderr, "ERROR: Can't read db_port parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "db_user", config->db_user, 64)) {
        fprintf(stderr, "ERROR: Can't read db_user parameter\n");
        goto error;
    }

    if (config->db_port < 1 || config->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid db_port number parameter\n");
        goto error;
    }
        
    if (!lamb_get_string(&cfg, "db_password", config->db_password, 128)) {
        fprintf(stderr, "ERROR: Can't read db_password parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "db_name", config->db_name, 64)) {
        fprintf(stderr, "ERROR: Can't read db_name parameter\n");
        goto error;
    }
    
    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
