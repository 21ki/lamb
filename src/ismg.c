
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

    lamb_event_loop(&cmpp);

    return;
}

void lamb_event_loop(cmpp_ismg_t *cmpp) {
    //pid_t pid, ppid;
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    struct epoll_event ev, events[20];
    int i, err, epfd, nfds, confd, sockfd;

    epfd = epoll_create1(0);
    ev.data.fd = cmpp->sock.fd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, cmpp->sock.fd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 20, 500);

        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == cmpp->sock.fd) {
                /* new client connection */
                confd = accept(cmpp->sock.fd, (struct sockaddr *)&clientaddr, &clilen);
                if (confd < 0) {
                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Lamb server accept client connect error\n", 0);
                    } else {
                        fprintf(stderr, "Lamb server accept client connect error\n");
                    }

                    continue;
                }

                cmpp_sock_t sock;
                sock.fd = confd;

                /* TCP NONBLOCK */
                cmpp_sock_nonblock(&sock, true);
                /* TCP NODELAY */
                cmpp_sock_tcpnodelay(&sock, true);
                /* TCP KEEPAVLIE */
                cmpp_sock_keepavlie(&sock, 30, 5, 3);

                ev.data.fd = confd;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);
                getpeername(confd, (struct sockaddr *)&clientaddr, &clilen);
                if (config.daemon) {
                    lamb_errlog(config.logfile, "New client connection form %s\n", inet_ntoa(clientaddr.sin_addr));
                } else {
                    fprintf(stderr, "New client connection form %s\n", inet_ntoa(clientaddr.sin_addr));
                }

            } else if (events[i].events & EPOLLIN) {
                /* receive from client data */
                if ((sockfd = events[i].data.fd) < 0) {
                    continue;
                }

                cmpp_sock_t sock;
                cmpp_pack_t pack;
                sock.fd = sockfd;
                cmpp_sock_init(&sock);
                sock.recvTimeout = config.timeout;

                getpeername(sockfd, (struct sockaddr *)&clientaddr, &clilen);

                err = cmpp_recv(&sock, &pack, sizeof(cmpp_pack_t));
                if (err) {
                    if (err == -1) {
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                        
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);
                        continue;
                    }

                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Receive packet error from client %s\n", inet_ntoa(clientaddr.sin_addr));
                    } else {
                        fprintf(stderr, "Receive packet error from client %s\n", inet_ntoa(clientaddr.sin_addr));
                    }
                    
                    continue;
                }

                unsigned int sequenceId;

                if (cmpp_check_method(&pack, sizeof(pack), CMPP_CONNECT)) {
                    unsigned char version;
                    cmpp_pack_get_integer(&pack, cmpp_connect_version, &version, 1);
                    if (version != CMPP_VERSION) {
                        cmpp_connect_resp(&sock, sequenceId, 4);
                        continue;
                    }

                    unsigned char SourceAddr[8];
                    char *user = "901234";
                    char *password = "123456";

                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, SourceAddr, sizeof(SourceAddr), 6);
                    if (memcmp(SourceAddr, user, strlen(user)) != 0) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "Incorrect source address from client %s\n", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "Incorrect source address from client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                        
                        continue;
                    }

                    cmpp_pack_get_integer(&pack, cmpp_sequence_id, &sequenceId, 4);
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), user, password)) {
                        cmpp_connect_resp(&sock, sequenceId, 0);
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "login successfull form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stdout, "login successfull form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }

                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        pid_t pid = fork();
                        if (pid < 0) {
                            if (config.daemon) {
                                lamb_errlog(config.logfile, "Unable to fork child process", 0);
                            } else {
                                fprintf(stderr, "Unable to fork child process");
                            }
                        } else if (pid == 0) {
                            /* some code ... */
                            epoll_ctl(epfd, EPOLL_CTL_DEL, cmpp->sock.fd, NULL);
                            ev.data.fd = sockfd;
                            ev.events = EPOLLIN;
                            epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
                            lamb_work_loop();
                        }

                        cmpp_sock_close(&sock);
                    } else {
                        cmpp_connect_resp(&sock, sequenceId, 3);
                        
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "login failed form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "login failed form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                    }
                } else {
                    cmpp_connect_resp(&sock, sequenceId, 1);
                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
                    } else {
                        fprintf(stderr, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
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

    if (lamb_get_int64(&cfg, "timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "recv_timeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read max connections parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "send_timeout", &conf->send_timeout) != 0) {
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
