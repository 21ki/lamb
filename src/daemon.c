
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
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include "daemon.h"
#include "common.h"
#include "log.h"

static lamb_db_t *db;
static lamb_config_t *config;

int main(int argc, char *argv[]) {
    bool background = false;
    char *file = "daemon.conf";
    
    int opt = 0;
    char *optstring = "c:d";
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

    /* Logger initialization*/
    lamb_log_init("lamb-daemon");

    /* Check lock protection */
    lamb_lock_t lock;

    if (lamb_lock_protection(&lock, "/tmp/daemon.lock")) {
        syslog(LOG_ERR, "Already started, please do not repeat the start!\n");
        return -1;
    }

    /* Save pid to file */
    lamb_pid_file(&lock, getpid());

    /* Signal event processing */
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Setting process information */
    lamb_set_process("lamb-daemon");

    /* Start main event thread */
    lamb_event_loop();

    /* Release lock protection */
    lamb_lock_release(&lock);

    return 0;
}

void lamb_event_loop(void) {
    int err;
    lamb_task_t *task;

    task = (lamb_task_t *)calloc(1, sizeof(lamb_task_t));
    if (!task) {
        syslog(LOG_ERR, "The kernel can't allocate memory");
        return;
    }

    err = lamb_component_initialization(config);
    if (err) {
        lamb_debug("component initialization failed\n");
        return;
    }

    /* Master control loop*/
    while (true) {
        memset(task, 0, sizeof(lamb_task_t));
        err = lamb_fetch_taskqueue(db, task);
        if (!err) {
            lamb_start_program(task);
            lamb_del_taskqueue(db, task->id);
            syslog(LOG_INFO, "Start new id %d in %s service process", task->eid, task->mod);
        }

        lamb_sleep(5000);
    }

    return;
}

int lamb_fetch_taskqueue(lamb_db_t *db, lamb_task_t *task) {
    PGresult *res = NULL;
    char *sql = "SELECT * FROM taskqueue ORDER BY create_time DESC LIMIT 1";

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    if (PQntuples(res) < 1) {
        PQclear(res);
        return -1;
    }

    task->id = atoll(PQgetvalue(res, 0, 0));
    task->eid = atoi(PQgetvalue(res, 0, 1));
    strncpy(task->mod, PQgetvalue(res, 0, 2), 254);
    strncpy(task->config, PQgetvalue(res, 0, 3), 254);
    strncpy(task->argv, PQgetvalue(res, 0, 4), 254);

    PQclear(res);

    return 0;
}

int lamb_del_taskqueue(lamb_db_t *db, long long id) {
    char sql[128];
    PGresult *res = NULL;

    snprintf(sql, sizeof(sql), "DELETE FROM taskqueue WHERE id = %lld", id);
    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

void lamb_start_program(lamb_task_t *task) {
    char cmd[512];
    char prog[128] = {0};
    char cfg[128] = {0};

    snprintf(prog, sizeof(prog), "%s/%s", config->module, task->mod);
    snprintf(cfg, sizeof(cfg), "%s/%s", config->config, task->config);

    if (strlen(task->argv) > 0) {
        snprintf(cmd, sizeof(cmd), "%s -a %d -c %s %s", prog, task->eid, cfg, task->argv);
    } else {
        snprintf(cmd, sizeof(cmd), "%s -a %d -c %s", prog, task->eid, cfg);
    }

    system(cmd);

    return;
}

int lamb_component_initialization(lamb_config_t *cfg) {
    int err;

    /* Postgresql Database  */
    db = (lamb_db_t *)malloc(sizeof(lamb_db_t));
    if (!db) {
        syslog(LOG_ERR, "The kernel can't allocate memory");
        return -1;
    }

    err = lamb_db_init(db);
    if (err) {
        syslog(LOG_ERR, "postgresql database initialization failed");
        return -1;
    }

    err = lamb_db_connect(db, cfg->db_host, cfg->db_port,
                          cfg->db_user, cfg->db_password, cfg->db_name);
    if (err) {
        syslog(LOG_ERR, "can't connect to postgresql database %s", cfg->db_host);
        return -1;
    }

    lamb_debug("connect to postgresql %s successfull\n", cfg->db_host);
    
    return 0;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "Can't open %s configuration file\n", file);
        goto error;
    }

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "Can't read config 'Id' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "Can't read config 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "Can't read config 'LogFile' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "Can't read config 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "Invalid DB port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbPassword' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbName' parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "Module", conf->module, 254) != 0) {
        fprintf(stderr, "Can't read config 'Module' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Config", conf->config, 254) != 0) {
        fprintf(stderr, "ERROR: Can't read config 'Config' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
