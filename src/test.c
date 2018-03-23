
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sched.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include "socket.h"
#include "test.h"

static int scheduler;
static lamb_db_t *db;
static lamb_config_t *config;

int main(int argc, char *argv[]) {
    bool background = false;
    char *file = "test.conf";
    
    int opt = 0;
    char *optstring = "a:c:d";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'c':
            file = optarg;
            break;
        case 'd':
            background = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    /* Read lamb configuration file */
    config = (lamb_config_t *)calloc(1, sizeof(lamb_config_t));
    if (lamb_read_config(config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (background) {
        lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    lamb_set_process("lamb-testd");

    err = lamb_component_initialization(config);
    if (err) {
        lamb_debug("component initialization failed\n");
        return;
    }

    int len;
    void *pk;
    char *buf;
    int status;
    int channel;
    lamb_submit_t submit;
    Message message = MESSAGE__INIT;

    /* Master control loop*/
    while (true) {
        memset(&submit, 0, sizeof(lamb_submit_t));
        err = lamb_fetch_message(db, &channel, &submit);

        if (err) {
            lamb_sleep(3000);
            continue;
        }

        message.id = submit.id;
        message.phone = submit.phone;
        message.length = submit.length;
        message.content.data = (uint8_t *)submit.content;
        message.content.len = submit.length;
        message.spcode = submit.spcode;
        message.channel = channel;

        len = message__get_packed_size(&message);
        pk = malloc(len);

        if (!pk) {
            lamb_log(LOG_ERR, "the kernel can't allocate memory");
            continue;
        }

        message__pack(&message, pk);
        len = lamb_pack_assembly(&buf, LAMB_MESSAGE, pk, len);
        if (len > 0) {
            /* Send message to schduler */
            nn_send(scheduler, buf, len, 0);

            /* Check state response */
            status = lamb_check_response(scheduler);

            switch (status) {
            case 1:
                lamb_log(LOG_NOTICE, "message %llu response state successfull", message.id);
                break;
            case 2:
                lamb_log(LOG_NOTICE, "message %llu response state no channel", message.id);
                break;
            default:
                lamb_log(LOG_NOTICE, "message %llu response state unknown error", message.id);
                break;
            }

            /* Update message status */
            lamb_update_message(db, submit.id, status);
            free(buf);
        }
        free(pk);
    }

    return;
}

int lamb_fetch_message(lamb_db_t *db, int *channel, lamb_submit_t *message) {
    PGresult *res = NULL;
    char *sql = "SELECT * FROM message WHERE status = 0 ORDER BY create_time DESC LIMIT 1";

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        return -1;
    }

    message->id = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
    strncpy(message->spcode, PQgetvalue(res, 0, 1), 20);
    strncpy(message->phone, PQgetvalue(res, 0, 2), 20);
    strncpy(message->content, PQgetvalue(res, 0, 3), 159);
    message->length = strlen(message->content);
    *channel = atoi(PQgetvalue(res, 0, 5));

    PQclear(res);

    return 0;
}

int lamb_check_response(int sock) {
    int rc;
    char *buf;
    int stat = -1;

    rc = nn_recv(sock, &buf, NN_MSG, 0);
    if (rc < HEAD) {
        if (rc > 0) {
            nn_freemsg(buf);
        }

        return stat;
    }

    if (CHECK_COMMAND(buf) == LAMB_OK) {
        stat = 1;
    } else if (CHECK_COMMAND(buf) == LAMB_NOROUTE) {
        stat = 2;
    }

    nn_freemsg(buf);

    return stat;
}

int lamb_update_message(lamb_db_t *db, unsigned long long id, int status) {
    char sql[128];
    PGresult *res = NULL;

    snprintf(sql, sizeof(sql), "UPDATE message SET status = %d WHERE id = %llu", status, id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

int lamb_nn_testsched(const char *host, int id, int timeout) {
    int fd;
    int err;
    Response *resp;
    Request req = REQUEST__INIT;

    req.id = id;
    req.type = LAMB_TEST;
    req.addr = "0.0.0.0";

    resp = lamb_nn_request(host, &req, timeout);

    if (!resp) {
        return -1;
    }

    err = lamb_nn_connect(&fd, resp->host, NN_PAIR, timeout);

    response__free_unpacked(resp, NULL);

    if (err) {
        return -1;
    }

    return fd;
}

int lamb_component_initialization(lamb_config_t *cfg) {
    int err;

    /* Postgresql Database  */
    db = (lamb_db_t *)malloc(sizeof(lamb_db_t));
    if (!db) {
        lamb_log(LOG_ERR, "the kernel can't allocate memory");
        return -1;
    }

    err = lamb_db_init(db);
    if (err) {
        lamb_log(LOG_ERR, "postgresql database initialization failed");
        return -1;
    }

    err = lamb_db_connect(db, cfg->db_host, cfg->db_port,
                          cfg->db_user, cfg->db_password, cfg->db_name);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to postgresql database");
        return -1;
    }

    lamb_debug("connect to postgresql %s successfull\n", cfg->db_host);
    
    /* Connect to Scheduler server */
    scheduler = lamb_nn_testsched(cfg->scheduler, cfg->id, cfg->timeout);

    if (scheduler < 0) {
        lamb_log(LOG_ERR, "can't connect to scheduler %s", cfg->scheduler);
        return -1;
    }

    lamb_debug("connect to scheduler %s successfull\n", cfg->scheduler);

    return 0;
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

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Id' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "Scheduler", conf->scheduler, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid scheduler server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid DB port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbName' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}


