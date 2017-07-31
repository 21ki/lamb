
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

static lamb_config_t config;

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

    int err;
    cmpp_ismg_t cmpp;
    err = cmpp_init_ismg(&cmpp, config.listen, config.port);

    if (err) {
        if (config.daemon) {
            lamb_errlog(config.logfile, "Cmpp server initialization failed", 0);
        } else {
            fprintf(stderr, "Cmpp server initialization failed\n");
        }

        return -1;
    }

    if (config.daemon) {
        lamb_errlog(config.logfile, "Cmpp server initialization successfull", 0);
    } else {
        fprintf(stderr, "Cmpp server initialization successfull\n");
    }

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.timeout);

    if (config.daemon) {
        lamb_errlog(config.logfile, "lamb server listen %s port %d", config.listen, config.port);
    } else {
        fprintf(stderr, "lamb server listen %s port %d\n", config.listen, config.port);
    }

    /* Start main event thread */
    lamb_event_loop(&cmpp);

    return 0;
}

void lamb_event_loop(cmpp_ismg_t *cmpp) {
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    struct epoll_event ev, events[32];
    int i, err, epfd, nfds, confd, sockfd;

    epfd = epoll_create1(0);
    ev.data.fd = cmpp->sock.fd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, cmpp->sock.fd, &ev);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == cmpp->sock.fd) {
                /* new client connection */
                confd = accept(cmpp->sock.fd, (struct sockaddr *)&clientaddr, &clilen);
                if (confd < 0) {
                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Lamb server accept client connect error", 0);
                    } else {
                        fprintf(stderr, "Lamb server accept client connect error\n");
                    }

                    continue;
                }

                cmpp_sock_t sock;
                sock.fd = confd;

                /* TCP NONBLOCK */
                cmpp_sock_nonblock(&sock, true);

                ev.data.fd = confd;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);
                getpeername(confd, (struct sockaddr *)&clientaddr, &clilen);
                if (config.daemon) {
                    lamb_errlog(config.logfile, "New client connection form %s", inet_ntoa(clientaddr.sin_addr));
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

                cmpp_sock_init(&sock, sockfd);
                cmpp_sock_setting(&sock, CMPP_SOCK_RECVTIMEOUT, config.timeout);
                getpeername(sockfd, (struct sockaddr *)&clientaddr, &clilen);

                err = cmpp_recv(&sock, &pack, sizeof(cmpp_pack_t));
                if (err) {
                    if (err == -1) {
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "Connection is closed by the client %s", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                        
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);
                        continue;
                    }

                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Receive packet error from client %s", inet_ntoa(clientaddr.sin_addr));
                    } else {
                        fprintf(stderr, "Receive packet error from client %s\n", inet_ntoa(clientaddr.sin_addr));
                    }
                    
                    continue;
                }

                if (cmpp_check_method(&pack, sizeof(pack), CMPP_CONNECT)) {
                    unsigned char version;
                    unsigned int sequenceId;

                    cmpp_pack_get_integer(&pack, cmpp_sequence_id, &sequenceId, 4);
                    cmpp_pack_get_integer(&pack, cmpp_connect_version, &version, 1);

                    /* Check Cmpp Version */
                    if (version != CMPP_VERSION) {
                        cmpp_connect_resp(&sock, sequenceId, 4);
                        continue;
                    }
                    
                    /* Check SourceAddr */
                    char *user = "901234";
                    char *password = "123456";
                    unsigned char SourceAddr[8];

                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, SourceAddr, sizeof(SourceAddr), 6);
                    if (memcmp(SourceAddr, user, strlen(user)) != 0) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "Incorrect source address from client %s", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "Incorrect source address from client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                        
                        continue;
                    }

                    /* Check AuthenticatorSource */
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), user, password)) {
                        cmpp_connect_resp(&sock, sequenceId, 0);
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "login successfull form client %s", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stdout, "login successfull form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }

                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);

                        /* Create Work Process */
                        pid_t pid = fork();
                        if (pid < 0) {
                            if (config.daemon) {
                                lamb_errlog(config.logfile, "Unable to fork child process", 0);
                            } else {
                                fprintf(stderr, "Unable to fork child process\n");
                            }
                        } else if (pid == 0) {
                           if (config.daemon) {
                                lamb_errlog(config.logfile, "Start new child work process", 0);
                            } else {
                                fprintf(stderr, "Start new child work process\n");
                            }
                            /* some code ... */
                            close(epfd);
                            cmpp_ismg_close(cmpp);
                            lamb_work_loop(&sock);
                            return;
                        }

                        cmpp_sock_close(&sock);
                    } else {
                        cmpp_connect_resp(&sock, sequenceId, 3);
                        
                        if (config.daemon) {
                            lamb_errlog(config.logfile, "login failed form client %s", inet_ntoa(clientaddr.sin_addr));
                        } else {
                            fprintf(stderr, "login failed form client %s\n", inet_ntoa(clientaddr.sin_addr));
                        }
                    }
                } else {
                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
                    } else {
                        fprintf(stderr, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
                    }
                }
            }
        }
    }

    return;
}

void lamb_work_loop(cmpp_sock_t *sock) {
    cmpp_pack_t pack;
    socklen_t clilen;
    int err, epfd, nfds;
    struct sockaddr_in clientaddr;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = sock->fd;
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ev);
    getpeername(sock->fd, (struct sockaddr *)&clientaddr, &clilen);

    /* msgId */
    unsigned long long msgId = 90140610510510;
    
    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            err = cmpp_recv(sock, &pack, sizeof(pack));
            if (err) {
                if (err == -1) {
                    if (config.daemon) {
                        lamb_errlog(config.logfile, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                    } else {
                        fprintf(stderr, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                    }

                    return;
                }

                if (config.daemon) {
                    lamb_errlog(config.logfile, "Receive packet error from client %s\n", inet_ntoa(clientaddr.sin_addr));
                } else {
                    fprintf(stderr, "Receive packet error from client %s\n", inet_ntoa(clientaddr.sin_addr));
                }
                continue;
            }

            cmpp_head_t *chp = (cmpp_head_t *)&pack;
            unsigned int commandId = ntohl(chp->commandId);
            
            switch (commandId) {
            case CMPP_ACTIVE_TEST:
                fprintf(stdout, "Receive cmpp_active_test packet from client\n");
                cmpp_active_test_resp(sock, ntohl(chp->sequenceId));
                break;
            case CMPP_SUBMIT:
                fprintf(stdout, "Receive cmpp_submit packet from client\n");
                cmpp_submit_resp(sock, ntohl(chp->sequenceId), msgId++, 0);
                break;
            case CMPP_TERMINATE:
                fprintf(stdout, "Receive cmpp_terminate packet from client\n");
                cmpp_terminate_resp(sock, ntohl(chp->sequenceId));
                close(epfd);
                fprintf(stdout, "Close the connection to the client\n");
                cmpp_sock_close(sock);
                return;
            }
        }
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

    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Id' parameter\n");
        goto error;
    }
    
    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Debug' parameter\n");
        goto error;
    }

    if (lamb_get_bool(&cfg, "Daemon", &conf->daemon) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Daemon' parameter\n");
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

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid listen port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Connections", &conf->connections) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Connections' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "RecvTimeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RecvTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "SendTimeout", &conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SendTimeout' parameter\n");
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
