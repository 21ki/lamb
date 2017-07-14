
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <libconfig.h>
#include <leveldb/c.h>
#include <cmpp2.h>
#include "sp.h"

lamb_config_t config;
lamb_cache_t cache;
lamb_queue_t queue;

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
    lamb_read_config(&config, file);

    /* daemon mode */
    if (conifg.daemon) {
        signal(SIGCHLD, SIG_IGN);
        daemon(0, 0);
    }

    lamb_queue_t send;
    lamb_queue_t recv;

    lamb_fetch_loop(&send);
    lamb_update_loop(&recv);

    lamb_send_loop(&send);
    lamb_recv_loop(&recv);

    lamb_event_loop();

    return 0;
}

int lamb_read_config(lamb_config_t *config, const char *file) {
    if (!config) {
        return -1;
    }

    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, file)) {
        fprintf(stderr, "ERROR: Can't open %s configuration file\n", file);
        config_destroy(&cfg);
        return -1;
    }

    const char *host;
    if (!config_lookup_string(&cfg, "host", &host)) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->host, host, 16);

    int port;
    if (config_lookup_int(&cfg, "port", &port)) {
        if (port > 0 && port < 65535) {
            config->port = (unsigned short)port;
        } else {
            fprintf(stderr, "ERROR: Invalid host port number\n");
            config_destroy(&cfg);
            return -1;
        }
    }

    const char *user;
    if (!config_lookup_string(&cfg, "user", &user)) {
        fprintf(stderr, "ERROR: Can't read user parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->user, user, 64);

    const char *password;
    if (!config_lookup_string(&cfg, "password", &password)) {
        fprintf(stderr, "ERROR: Can't read password parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->password, password, 128);

    const char *spid;
    if (!config_lookup_string(&cfg, "spid", &spid)) {
        fprintf(stderr, "ERROR: Can't read spid parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->spid, spid, 8);

    const char *spcode;
    if (!config_lookup_string(&cfg, "spcode", &spcode)) {
        fprintf(stderr, "ERROR: Can't read spcode parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->spcode, spcode, 16);

    int queue;
    if (!config_lookup_int(&cfg, "queue", &queue)) {
        fprintf(stderr, "ERROR: Can't read queue parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->queue = queue;

    int confirmed;
    if (!config_lookup_int(&cfg, "confirmed", &confirmed)) {
        fprintf(stderr, "ERROR: Can't read confirmed parameter\n");tu
        config_destroy(&cfg);
        return -1;
    }
    config->confirmed = confirmed;

    long long contimeout;
    if (!config_lookup_int64(&cfg, "contimeout", &contimeout)) {
        fprintf(stderr, "ERROR: Can't read contimeout parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->contimeout = contimeout;

    long long sendtimeout;
    if (!config_lookup_int64(&cfg, "sendtimeout", &sendtimeout)) {
        fprintf(stderr, "ERROR: Can't read sendtimeout parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->sendtimeout = sendtimeout;

    long long recvtimeout;
    if (!config_lookup_int64(&cfg, "recvtimeout", &recvtimeout)) {
        fprintf(stderr, "ERROR: Can't read recvtimeout parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->recvtimeout = recvtimeout;

    const char *logfile;
    if (!config_lookup_string(&cfg, "logfile", &logfile)) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->logfile, logfile, 128);

    const char *cache;
    if (!config_lookup_string(&cfg, "cache", &cache)) {
        fprintf(stderr, "ERROR: Can't read cache parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    strncpy(config->cache, cache, 128);

    int debug;
    if (!config_lookup_bool(&cfg, "debug", &debug)) {
        fprintf(stderr, "ERROR: Can't read debug parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->debug = debug ? true : false;

    int daemon;
    if (!config_lookup_bool(&cfg, "daemon", &daemon)) {
        fprintf(stderr, "ERROR: Can't read daemon parameter\n");
        config_destroy(&cfg);
        return -1;
    }
    config->daemon = daemon ? true : false;

    config_destroy(&cfg);
    return 0;
}
