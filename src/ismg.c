
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
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cmpp.h>
#include "ismg.h"
#include "utils.h"
#include "cache.h"
#include "queue.h"
#include "config.h"

static lamb_db_t db;
static cmpp_ismg_t cmpp;
static lamb_cache_t cache;
static lamb_config_t config;
static unsigned int sequence;
static pthread_cond_t cond;
static pthread_mutex_t mutex;

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
        lamb_errlog(config.logfile, "can't connect to redis server", 0);
        return -1;
    }

    /* Postgresql Database Initialization */
    err = lamb_db_init(&db);
    if (err) {
        lamb_errlog(config.logfile, "Postgresql database handle initialization failed", 0);
    }

    err = lamb_db_connect(&db, "127.0.0.1", 5432, "postgres", "postgres", "lamb");
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to postgresql database", 0);
        return -1;
    }
    
    /* Cmpp ISMG Gateway Initialization */
    err = cmpp_init_ismg(&cmpp, config.listen, config.port);
    if (err) {
        lamb_errlog(config.logfile, "Cmpp server initialization failed", 0);
        return -1;
    }

    fprintf(stderr, "Cmpp server initialization successfull\n");

    /* Setting Cmpp Socket Parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.timeout);

    if (config.daemon) {
        lamb_errlog(config.logfile, "lamb server listen %s port %d", config.listen, config.port);
    } else {
        fprintf(stderr, "lamb server listen %s port %d\n", config.listen, config.port);
    }

    /* Start Main Event Thread */
    lamb_set_process("lamb-ismgd");
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
                        lamb_errlog(config.logfile, "Client closed the connection from %s", inet_ntoa(clientaddr.sin_addr));
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);
                        continue;
                    }

                    lamb_errlog(config.logfile, "Incorrect packet format from client %s", inet_ntoa(clientaddr.sin_addr));
                    continue;
                }

                if (cmpp_check_method(&pack, sizeof(pack), CMPP_CONNECT)) {
                    unsigned char version;
                    unsigned int sequenceId;

                    cmpp_pack_get_integer(&pack, cmpp_sequence_id, &sequenceId, 4);
                    cmpp_pack_get_integer(&pack, cmpp_connect_version, &version, 1);
                    sequenceId = ntohl(sequenceId);

                    /* Check Cmpp Version */
                    if (version != CMPP_VERSION) {
                        cmpp_connect_resp(&sock, sequenceId, 4);
                        lamb_errlog(config.logfile, "Version not supported from client %s", inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }
                    
                    /* Check SourceAddr */
                    char key[32];
                    char user[8];
                    char password[64];

                    memset(user, 0, sizeof(user));
                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, user, sizeof(user), 6);
                    sprintf(key, "account.%s", user);
                    
                    if (!lamb_cache_has(&cache, key)) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        lamb_errlog(config.logfile, "Incorrect source address from client %s", inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }

                    memset(password, 0, sizeof(password));
                    lamb_cache_hget(&cache, key, "password", password, sizeof(password));

                    /* Check AuthenticatorSource */
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), user, password)) {
                        /* Check Duplicate Logon */
                        sprintf(key, "online.%s", user);
                        if (lamb_cache_has(&cache, key)) {
                            cmpp_connect_resp(&sock, sequenceId, 9);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                            close(sockfd);
                            lamb_errlog(config.logfile, "Duplicate login from client %s", inet_ntoa(clientaddr.sin_addr));
                            continue;
                        }

                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);

                        /* Login Successfull */

                        lamb_errlog(config.logfile, "Login successfull from client %s", inet_ntoa(clientaddr.sin_addr));

                        lamb_account_t account;
                        memset(&account, 0, sizeof(account));
                        err = lamb_account_get(&db, user, &account);
                        if (err) {
                            cmpp_connect_resp(&sock, sequenceId, 10);
                            lamb_errlog(config.logfile, "Can't to obtain account information", 0);
                            continue;
                        }

                        /* Create Work Process */
                        pid_t pid = fork();
                        if (pid < 0) {
                            lamb_errlog(config.logfile, "Unable to fork child process", 0);
                        } else if (pid == 0) {
                            close(epfd);
                            cmpp_ismg_close(cmpp);
                            lamb_cache_close(&cache);
                            
                            lamb_client_t client;
                            client.sock = &sock;
                            client.account = &account;
                            client.addr = lamb_strdup(inet_ntoa(clientaddr.sin_addr));

                            cmpp_connect_resp(&sock, sequenceId, 0);
                            lamb_work_loop(&client);
                            return;
                        }

                        cmpp_sock_close(&sock);
                    } else {
                        cmpp_connect_resp(&sock, sequenceId, 3);
                        lamb_errlog(config.logfile, "Login failed form client %s", inet_ntoa(clientaddr.sin_addr));
                    }
                } else {
                    lamb_errlog(config.logfile, "Unable to resolve packets from client %s", inet_ntoa(clientaddr.sin_addr));
                }
            }
        }
    }

    return;
}

void lamb_work_loop(lamb_client_t *client) {
    int err, gid;
    cmpp_pack_t pack;
    lamb_submit_t *submit;
    lamb_message_t message;

    gid = getpid();
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    lamb_set_process("lamb-client");
    
    /* Redis Cache */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to redis server", 0);
        return;
    }
    
    /* Epoll Events */
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = client->sock->fd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, client->sock->fd, &ev);

    /* Mqueue Message Queue */
    char name[64];
    lamb_queue_t queue;
    lamb_queue_opt opt;

    opt.flag = O_WRONLY | O_NONBLOCK;
    sprintf(name, "/cli.%d.message", client->account->id);
    opt.attr = NULL;

    err = lamb_queue_open(&queue, name, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Open %s message queue failed", name);
        goto exit;
    }

    /* Start Client Status Update Thread */
    lamb_start_thread(lamb_client_online, client, 1);

    /* Client Message Deliver */
    lamb_start_thread(lamb_deliver_loop, (void *)client, 1);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            /* Waiting for receive request */
            err = cmpp_recv(client->sock, &pack, sizeof(pack));
            if (err) {
                if (err == -1) {
                    lamb_errlog(config.logfile, "Connection is closed by the client %s\n", client->addr);
                    break;
                }
                continue;
            }

            /* Analytic data packet header */
            unsigned char result;
            cmpp_head_t *chp = (cmpp_head_t *)&pack;
            unsigned int commandId = ntohl(chp->commandId);
            unsigned int sequenceId = ntohl(chp->sequenceId);

            /* Check protocol command */
            switch (commandId) {
            case CMPP_ACTIVE_TEST:
                cmpp_active_test_resp(client->sock, sequenceId);
                break;
            case CMPP_SUBMIT:;
                result = 0;
                unsigned long long msgId;

                /* Generate Message ID */
                msgId = lamb_gen_msgid(gid, lamb_sequence());
                cmpp_pack_set_integer(&pack, cmpp_submit_msg_id, msgId, 8);

                /* Message Write Queue */
                memset(&message, 0, sizeof(message));
                message.type = LAMB_SUBMIT;
                submit = (lamb_submit_t *)&message.data;
                submit->id = msgId;
                strcpy(submit->spid, client->account->user);
                cmpp_pack_get_string(&pack, cmpp_submit_dest_terminal_id, submit->phone, 24, 21);
                cmpp_pack_get_string(&pack, cmpp_submit_src_id, submit->spcode, 24, 21);
                cmpp_pack_get_integer(&pack, cmpp_submit_msg_length, &submit->length, 1);
                cmpp_pack_get_string(&pack, cmpp_submit_msg_content, submit->content, 160, submit->length);

                err = lamb_queue_send(&queue, (const char *)&message, sizeof(message), 0);
                if (err) {
                    result = 11;
                }

                /* Submit Response */
                cmpp_submit_resp(client->sock, sequenceId, msgId, result);
                break;
            case CMPP_DELIVER_RESP:;
                result = 0;
                cmpp_pack_get_integer(&pack, cmpp_deliver_resp_result, &result, 1);
                if ((result == 0) && (sequenceId == sequence)) {
                    pthread_mutex_lock(&mutex);
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                }
                break;
            case CMPP_TERMINATE:
                cmpp_terminate_resp(client->sock, sequenceId);
                goto exit;
            }
        }
    }

exit:
    close(epfd);
    cmpp_sock_close(client->sock);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
    
    return;
}

void *lamb_deliver_loop(void *data) {
    int err;
    ssize_t ret;
    lamb_message_t message;
    lamb_client_t *client;
    lamb_deliver_t *deliver;
    lamb_report_t *report;
    unsigned int sequenceId;

    client = (lamb_client_t *)data;

    char name[64];
    lamb_queue_t queue;
    lamb_queue_opt opt;

    opt.flag = O_RDWR;
    opt.attr = NULL;

    sprintf(name, "/cli.%d.deliver", client->account->id);
    err = lamb_queue_open(&queue, name, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Open %s message queue failed", name);
        goto exit;
    }
    
    while (true) {
        memset(&message, 0 , sizeof(message));
        ret = lamb_queue_receive(&queue, (char *)&message, sizeof(message), 0);
        if (ret > 0) {
            switch (message.type) {
            case LAMB_DELIVER:;
                sequenceId = sequence = cmpp_sequence();
                deliver = (lamb_deliver_t *)&message.data;
            deliver:
                err = cmpp_deliver(client->sock, sequenceId, deliver->id, deliver->spcode, deliver->phone, deliver->content, 8);
                if (err) {
                    lamb_errlog(config.logfile, "Sending 'cmpp_deliver' packet to client %s failed", client->addr);
                    lamb_sleep(3000);
                    goto deliver;
                }
                break;
            case LAMB_REPORT:;
                sequenceId = sequence = cmpp_sequence();
                report = (lamb_report_t *)&message.data;
            report:
                err = cmpp_report(client->sock, sequenceId, report->id, report->status, report->submitTime, report->doneTime, report->phone, 0);
                if (err) {
                    lamb_errlog(config.logfile, "Sending 'cmpp_report' packet to client %s failed", client->addr);
                    lamb_sleep(3000);
                    goto report;
                }
                break;
            default:
                continue;
            }

            struct timeval now;
            struct timespec timeout;
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + 5;

            /* Waiting for message confirmation */
            pthread_mutex_lock(&mutex);
            err = pthread_cond_timedwait(&cond, &mutex, &timeout);

            if (err == ETIMEDOUT) {
                lamb_errlog(config.logfile, "Deliver message timeout from client %s", client->addr);
                if (message.type == LAMB_DELIVER) {
                    pthread_mutex_unlock(&mutex);
                    goto deliver;
                } else if (message.type == LAMB_REPORT) {
                    goto report;
                    pthread_mutex_unlock(&mutex);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    }

exit:
    pthread_exit(NULL);
    return NULL;
}

void *lamb_client_online(void *data) {
    int err;
    lamb_client_t *client;

    client = (lamb_client_t *)data;

    /* Redis Cache */
    err = lamb_cache_connect(&cache, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return NULL;
    }

    redisReply *reply = NULL;

    while (true) {
        reply = redisCommand(cache.handle, "SET online.%s 1", client->account->user);
        if (reply != NULL) {
            freeReplyObject(reply);
            reply = redisCommand(cache.handle, "EXPIRE online.%s 5", client->account->user);
            if (reply != NULL) {
                freeReplyObject(reply);
            }
        } else {
            lamb_errlog(config.logfile, "Lamb exec redis command error", 0);
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

