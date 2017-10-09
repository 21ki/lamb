
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include "utils.h"

int main(int argc, char *argv[]) {
    char *file = "mt.conf";
    bool background = false;

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
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (background) {
        lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal_processing();

    int err;

    /* Start Main Event Thread */
    lamb_set_process("lamb-mtserv");
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int fd, rc;
    char url[128];
    pthread_t tids[config.worker_thread];
        
    fd = nn_socket(AF_SP_RAW, NN_REP);
    if (fd < 0) {
        lamb_errlog(config.logfile, "nn_socket create socket failed", 0);
        lamb_debug("nn_socket: %s", nn_errno());
        return;
    }

    sprintf(url, "tcp://%s:%d", config.listen, config.port);

    if (nn_bind(fd, url) < 0) {
        lamb_errlog(config.logfile, "nn_socket create socket failed", 0);
        lamb_debug("nn_bind: %s", nn_errno());
        nn_close(fd);
        return;
    }

    memset(tids, 0, sizeof(tids));

    /* Start up the threads */
    for (int i = 0; i < config.worker_thread; i++) {
        rc = pthread_create(&tids[i], NULL, lamb_work_loop, (void *)(intptr_t)fd);
        if (rc < 0) {
            lamb_debug("pthread_create: %s", strerror(rc));
            nn_close(fd);
            break;
        }
    }

    for (int i = 0; i < config.worker_thread; i++) {
        if (tids[i] != NULL) {
            pthread_join(tids[i], NULL);
        }
    }

    return;
}

void *lamb_work_loop(void *arg) {
    int bytes;
    int fd = (intptr_t)arg;
    char *buf = NULL;

    lamb_debug("start a new %d thread ", pthread_self());
    
    bytes = nn_recv(fd, &buf, NN_MSG, 0);
    if (bytes > 0) {
        lamb_debug("nn_recv receive %s length message", buf);
        bytes = nn_send(fd, buf, NN_MSG, 0);
        nn_freemsg(buf);
    }

    pthread_exit(NULL);
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
        fprintf(stderr, "ERROR: Can't read 'Debug' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Listen", conf->listen, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid Listen IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Port' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "WorkerThread", &conf->worker_thread) != 0) {
        fprintf(stderr, "ERROR: Can't read 'WorkerThread' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}

