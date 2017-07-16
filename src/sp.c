
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

cmpp_sp_t cmpp;
lamb_queue_t *send;
lamb_queue_t *recv;
lamb_config_t config;
static int unconfirmed = 0;

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

    /* read lamb configuration file */
    if (!lamb_read_config(&config, file)) {
        return -1;
    }

    /* daemon mode */
    if (conifg.daemon) {
        lamb_daemon();
    }

    lamb_signal();

    lamb_queue_init(send);
    lamb_queue_init(recv);

    lamb_fetch_loop();
    lamb_update_loop();

    lamb_send_loop();
    lamb_recv_loop();

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
            if (cmpp_submit(&cmpp, pack->phone, pack->message, pack->delivery, pack->serviceId, pack->msgFmt, pack->msgSrc)) {
                free(node);
                continue;
            }

            if (!cmpp_check_connnect(&cmpp)) {
                while (!cmpp_reconnect()) {
                    errlog("cmpp server connection failed");
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
        if (cmpp_recv(&cmpp, &pack, sizeof(pack))) {
            head = (CMPP_HEAD_T *)pack;
            if (head->commandId == CMPP_SUBMIT_RESP) {
                resp = (CMPP_SUBMIT_RESP_T *)pack;
                lamb_queue_rpush(recv, (void *)resp->msgId);
            }
            continue;
        }

        if (!cmpp_check_connnect(&cmpp)) {
            while (!cmpp_reconnect()) {
                errlog("cmpp server connection failed");
                lamb_sleep(3000);
            }
        }
    }
}

void lamb_update_loop(void) {
    list_node_t *node;
    lamb_pgsql_t conn;
    lamb_pgsql_connect(conn, "user", "password", "dbname");

    while (true) {
        node = lamb_queue_lpop(recv);
        if (node != NULL) {
            lamb_report_update(conn, node->val);
            free(noede)
        }

        lamb_sleep(10);
    }
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

    if (!lamb_get_int(&cfg, "queue", &config->queue)) {
        fprintf(stderr, "ERROR: Can't read queue parameter\n");
        goto error;
    }

    if (!lamb_get_int(&cfg, "confirmed", &config->confirmed)) {
        fprintf(stderr, "ERROR: Can't read confirmed parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "contimeout", &config->contimeout)) {
        fprintf(stderr, "ERROR: Can't read contimeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "sendtimeout", &config->sendtimeout)) {
        fprintf(stderr, "ERROR: Can't read sendtimeout parameter\n");
        goto error;
    }

    if (!lamb_get_int64(&cfg, "recvtimeout", &config->recvtimeout)) {
        fprintf(stderr, "ERROR: Can't read recvtimeout parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "logfile", config->logfile, 128)) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        goto error;
    }

    if (!lamb_get_string(&cfg, "cache", config->cache, 128)) {
        fprintf(stderr, "ERROR: Can't read cache parameter\n");
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

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
