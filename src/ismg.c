
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cmpp.h>
#include "ismg.h"
#include "utils.h"
#include "config.h"
#include "list.h"
#include "amqp.h"

lamb_config_t config;

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
    memset(&config, 0, sizeof(config));
    if (lamb_read_config(&config, file) != 0) {
        return -1;
    }

    /* Daemon mode */
    if (config.daemon) {
        //lamb_daemon();
    }

    /* Signal event processing */
    lamb_signal();

    /* Start main event thread */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;
    
    cmpp_ismg_t cmpp;
    err = cmpp_init_ismg(&cmpp, config.listen, config.port);

    if (err) {
        if (config.daemon) {
            lamb_errlog(config.logfile, "%s", cmpp_get_error(err));
        } else {
            fprintf(stderr, "ERROR: %s\n", cmpp_get_error(err));
        }

        return;
    }

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.timeout);

    if (config.daemon) {
        lamb_errlog(config.logfile, "lamb server listen %s port %d", config.listen, config.port);
    } else {
        fprintf(stderr, "lamb server listen %s port %d\n", config.listen, config.port);
    }

    lamb_accept_loop(&cmpp);

    return;
}

void lamb_accept_loop(cmpp_ismg_t *cmpp) {
    pid_t pid, ppid;
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    struct epoll_event ev, events[20];
    int i, err, epfd, nfds, confd, sockfd;
    
    ev.data.fd = cmpp->sock.fd;
    ev.events = EPOLLIN | EPOLLET;

    epoll_ctl(epfd, EPOLL_CTL_ADD, cmpp->sock.fd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 20, 500);

        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == cmpp->sock.fd) {
                /* new client connection */
                confd = accept(cmpp->sock.fd, (struct sockaddr *)&clientaddr, &clilen);
                if (confd < 0) {
                    printf("lamb server accept client socket error\n");
                    continue;
                }

                ev.data.fd = confd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);
                printf("new client connection form %s\n", inet_ntoa(clientaddr.sin_addr));
            } else if (events[i].events & EPOLLIN) {
                /* receive from client data */
                if ((sockfd = events[i].data.fd) < 0) {
                    continue;
                }

                cmpp_sock_t sock;
                cmpp_pack_t pack;
                sock.fd = sockfd;
                sock.recvTimeout = config.timeout;
                err = cmpp_recv(&sock, &pack, sizeof(cmpp_pack_t));
                if (err) {
                    printf("cmpp packet error form client %d\n", sockfd);
                    close(sockfd);
                    events[i].data.fd = -1;
                    continue;
                }

                unsigned int sequenceId;
                char *user = "901234";
                char *password = "123456";

                if (is_cmpp_command(&pack, sizeof(pack), CMPP_CONNECT)) {
                    unsigned char version;
                    cmpp_pack_get_integer(&pack, cmpp_connect_version, &version, 1);
                    if (version != CMPP_VERSION) {
                        cmpp_connect_resp(&sock, sequenceId, 4);
                        continue;
                    }

                    cmpp_pack_get_integer(&pack, cmpp_sequence_id, &sequenceId, 4);
                    if (cmpp_check_authentication((cmpp_connect_t *)&pack, sizeof(cmpp_pack_t), user, password)) {
                        printf("login successfull form client %d\n", sockfd);
                    } else {
                        printf("login failed form client %d\n", sockfd);
                    }
                }
            }
        }
    }
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

    if (lamb_get_int(&cfg, "id", &conf->id) != 0) {
        fprintf(stderr, "ERROR: Can't read debug parameter\n");
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
        
    if (lamb_get_string(&cfg, "listen", conf->listen, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid listen IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read port parameter\n");
        goto error;
    }

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid listen port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "connections", &conf->connections) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "recv_timeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "send_timeout", &conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }
        
    if (lamb_get_string(&cfg, "logfile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read logfile parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
