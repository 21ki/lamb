
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
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cmpp.h>
#include "ismg.h"
#include "list.h"
#include "utils.h"
#include "cache.h"
#include "config.h"

static cmpp_ismg_t cmpp;
static lamb_list_t *list;
static lamb_cache_t cache;
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

    /* Redis Cache */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        if (config.daemon) {
            lamb_errlog(config.logfile, "can't connect to redis server", 0);
        } else {
            fprintf(stderr, "can't connect to redis server\n");
        }

        return -1;
    }

    /* Cmpp ISMG Gateway Initialization */
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

    /* Setting Cmpp Socket Parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.timeout);

    if (config.daemon) {
        lamb_errlog(config.logfile, "lamb server listen %s port %d", config.listen, config.port);
    } else {
        fprintf(stderr, "lamb server listen %s port %d\n", config.listen, config.port);
    }

    /* Start Main Event Thread */
    lamb_event_loop(&cmpp);

    return 0;
}

void lamb_event_loop(cmpp_ismg_t *cmpp) {
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    struct epoll_event ev, events[32];
    int i, err, epfd, nfds, confd, sockfd;

    clilen = sizeof(struct sockaddr);

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
                    lamb_errlog(config.logfile, "Lamb server accept client connect error", 0);
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
                lamb_errlog(config.logfile, "New client connection form %s", inet_ntoa(clientaddr.sin_addr));
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
                        lamb_errlog(config.logfile, "Connection is closed by the client %s", inet_ntoa(clientaddr.sin_addr));
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);
                        continue;
                    }

                    lamb_errlog(config.logfile, "Receive packet error from client %s", inet_ntoa(clientaddr.sin_addr));
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
                    char key[32];
                    unsigned char user[8];
                    unsigned char password[64];

                    memset(user, 0, sizeof(user));
                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, user, sizeof(user), 6);
                    sprintf(key, "account.%s", user);
                    
                    if (!lamb_cache_has(&cache, key)) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        lamb_errlog(config.logfile, "Incorrect source address from client %s", inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }

                    memset(password, 0, sizeof(password));
                    lamb_cache_hget(&cache, key, "password", (char *)password, sizeof(password));

                    /* Check AuthenticatorSource */
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), (const char *)user, (char *)password)) {
                        cmpp_connect_resp(&sock, sequenceId, 0);
                        lamb_errlog(config.logfile, "login successfull form client %s", inet_ntoa(clientaddr.sin_addr));
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);

                        /* Create Work Process */
                        pid_t pid = fork();
                        if (pid < 0) {
                            lamb_errlog(config.logfile, "Unable to fork child process", 0);
                        } else if (pid == 0) {
                            close(epfd);
                            cmpp_ismg_close(cmpp);
                            lamb_cache_close(&cache);

                            lamb_account_t account;
                            memset(&account, 0, sizeof(account));
                            strcpy(account.username, (char *)user);
                            err = lamb_account_get(&cache, (char *)user, &account);
                            if (err) {
                                cmpp_connect_resp(&sock, sequenceId, 9);
                                lamb_errlog(config.logfile, "can't to obtain account information", 0);
                                return;
                            }

                            lamb_work_loop(&sock, &account);
                            return;
                        }

                        cmpp_sock_close(&sock);
                    } else {
                        cmpp_connect_resp(&sock, sequenceId, 3);
                        lamb_errlog(config.logfile, "login failed form client %s", inet_ntoa(clientaddr.sin_addr));
                    }
                } else {
                    lamb_errlog(config.logfile, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
                }
            }
        }
    }

    return;
}

void lamb_work_loop(cmpp_sock_t *sock, lamb_account_t *account) {
    int err, gid;
    cmpp_pack_t pack;
    lamb_message_t message;
    
    gid = getpid();
    list = lamb_list_new();
    
    /* Epoll Events */
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = sock->fd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock->fd, &ev);

    /* Client Information */
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    clilen = sizeof(struct sockaddr);
    getpeername(sock->fd, (struct sockaddr *)&clientaddr, &clilen);

    /* Mqueue Message Queue */
    mqd_t queue;
    char name[64];
    struct mq_attr mattr;

    mattr.mq_maxmsg = 1024;
    mattr.mq_msgsize = sizeof(message);

    sprintf(name, "/msg.%s", account->username);
    queue = mq_open(name, O_CREAT | O_WRONLY, 0644, &mattr);
    if (queue < 0) {
        lamb_errlog(config.logfile, "Open %s message queue failed", name);
        goto exit;
    }

    /* Client Online Status Update */
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    err = pthread_create(&tid, &attr, lamb_online, (void *)(account->username));
    if (err) {
        lamb_errlog(config.logfile, "Start client online status thread failed", account->username);
    }

    /* Client Message Deliver */
    err = pthread_create(&tid, &attr, lamb_deliver_loop, (void *)(account->username));
    if (err) {
        lamb_errlog(config.logfile, "Start client message deliver thread failed", account->username);
    }

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            err = cmpp_recv(sock, &pack, sizeof(pack));
            if (err) {
                if (err == -1) {
                    lamb_errlog(config.logfile, "Connection is closed by the client %s\n", inet_ntoa(clientaddr.sin_addr));
                    break;
                }
                continue;
            }

            cmpp_head_t *chp = (cmpp_head_t *)&pack;
            //unsigned int totalLength = ntohl(chp->totalLength);
            unsigned int commandId = ntohl(chp->commandId);
            unsigned int sequenceId = ntohl(chp->sequenceId);

            unsigned char result;
            switch (commandId) {
            case CMPP_ACTIVE_TEST:
                cmpp_active_test_resp(sock, sequenceId);
                break;
            case CMPP_SUBMIT:;
                result = 0;
                /* cmpp submit resp setting */
                time_t rawtime;
                struct tm *t;
                time(&rawtime);
                t = localtime(&rawtime);
                unsigned long long msgId;

                msgId = cmpp_gen_msgid(t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, gid, cmpp_sequence());
                cmpp_pack_set_integer(&pack, msgId, cmpp_submit_msg_id, 8);

                unsigned char len;
                memset(&message, 0, sizeof(message));
                message.type = CMPP_SUBMIT;
                message.id = msgId;
                cmpp_pack_get_string(&pack, cmpp_submit_dest_terminal_id, message.phone, 24, 21);
                cmpp_pack_get_string(&pack, cmpp_submit_src_id, message.spcode, 24, 21);
                cmpp_pack_get_integer(&pack, cmpp_submit_msg_length, (size_t *)&len, 1);
                cmpp_pack_get_string(&pack, cmpp_submit_msg_content, message.content, 160, len);
                printf("len: %d\n", len);
                
                err = mq_send(queue, (const char *)&message, sizeof(message), 0);
                if (err) {
                    result = 10;
                    lamb_errlog(config.logfile, "write message to queue failed", 0);
                }


                cmpp_submit_resp(sock, sequenceId, msgId, result);
                break;
            case CMPP_DELIVER_RESP:;
                result = 0;
                cmpp_pack_get_integer(&pack, cmpp_deliver_resp_result, &result, 1);
                if (result == 0) {
                    char val[64];
                    sprintf(val, "%u", sequenceId);
                    pthread_mutex_lock(&list->lock);
                    lamb_list_rpush(list, lamb_list_node_new(val));
                    pthread_mutex_unlock(&list->lock);
                }

                break;
            case CMPP_TERMINATE:
                cmpp_terminate_resp(sock, sequenceId);
                fprintf(stdout, "Close the connection to the client\n");
                break;
            }
        }
    }

exit:
    close(epfd);
    cmpp_sock_close(sock);
    //pthread_cond_destroy(&cond);
    //pthread_mutex_destroy(&mutex);
    //pthread_attr_destroy(&attr);
    
    return;
}

void *lamb_deliver_loop(void *data) {
    //int err;
    ssize_t ret;
    lamb_message_t message;
    //cmpp_pack_t pack;

    mqd_t queue;
    char name[64];
    struct mq_attr attr;

    attr.mq_maxmsg = 1024;
    attr.mq_msgsize = sizeof(message);

    sprintf(name, "/msg.%s", (char *)data);
    queue = mq_open(name, O_CREAT | O_RDWR, 0644, &attr);
    if (queue < 0) {
        lamb_errlog(config.logfile, "Open %s message queue failed", name);
        goto exit;
    }

    while (true) {
        memset(&message, 0 , sizeof(message));
        ret = mq_receive(queue, (char *)&message, sizeof(message), 0);
        printf(" ret: %zd", ret);
        if (ret > 0) {
            printf("===============================\n");
            printf("msgId: %llu\n", message.id);
            printf("phone: %s\n", message.phone);
            printf("message: %s\n", message.content);
        }
    }

exit:
    pthread_exit(NULL);
    return NULL;
}

void *lamb_online(void *user) {
    int err;
    /* Redis Cache */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to redis server", 0);
        return NULL;
    }

    redisReply *reply = NULL;

    while (true) {
        reply = redisCommand(cache.handle, "SET online.%s 1", (char *)user);
        if (reply != NULL) {
            freeReplyObject(reply);
            reply = redisCommand(cache.handle, "EXPIRE online.%s 5", (char *)user);
            if (reply != NULL) {
                freeReplyObject(reply);
            }
        }

        sleep(3);
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

    if (lamb_get_string(&cfg, "Queue", conf->queue, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Queue' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "redisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisHost' parameter\n");
    }

    if (lamb_get_int(&cfg, "redisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisPort' parameter\n");
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "ERROR: Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "redisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisDb' parameter\n");
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
