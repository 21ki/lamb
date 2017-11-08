
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
#include <signal.h>
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
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <cmpp.h>
#include "ismg.h"
#include "utils.h"
#include "cache.h"
#include "socket.h"
#include "config.h"

static int mt, mo;
static cmpp_ismg_t cmpp;
static lamb_cache_t rdb;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_status_t status;
static lamb_confirmed_t confirmed;
static unsigned long long total = 0;

int main(int argc, char *argv[]) {
    char *file = "ismg.conf";

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
    lamb_signal_processing();

    int err;

    /* Redis Cache */
    err = lamb_cache_connect(&rdb, "127.0.0.1", 6379, NULL, 0);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to redis server", 0);
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
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.recv_timeout);

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
                    char username[8];
                    char password[64];

                    memset(username, 0, sizeof(username));
                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, username, sizeof(username), 6);
                    sprintf(key, "account.%s", username);
                    
                    if (!lamb_cache_has(&rdb, key)) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        lamb_errlog(config.logfile, "Incorrect source address from client %s", inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }

                    memset(password, 0, sizeof(password));
                    lamb_cache_hget(&rdb, key, "password", password, sizeof(password));

                    /* Check AuthenticatorSource */
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), username, password)) {
                        lamb_account_t account;
                        memset(&account, 0, sizeof(account));
                        err = lamb_account_get(&rdb, username, &account);
                        if (err) {
                            cmpp_connect_resp(&sock, sequenceId, 9);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                            close(sockfd);
                            lamb_errlog(config.logfile, "Can't fetch account information", 0);
                            continue;
                        }

                        /* Check Duplicate Logon */
                        if (lamb_is_login(&rdb, account.id)) {
                            cmpp_connect_resp(&sock, sequenceId, 10);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                            close(sockfd);
                            lamb_errlog(config.logfile, "Duplicate login from client %s", inet_ntoa(clientaddr.sin_addr));
                            continue;
                        }

                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);

                        /* Login Successfull */
                        lamb_errlog(config.logfile, "Login successfull from client %s", inet_ntoa(clientaddr.sin_addr));

                        /* Create Work Process */
                        pid_t pid = fork();
                        if (pid < 0) {
                            lamb_errlog(config.logfile, "Unable to fork child process", 0);
                        } else if (pid == 0) {
                            close(epfd);
                            cmpp_ismg_close(cmpp);
                            lamb_cache_close(&rdb);
                            
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
    int rc;
    int err, gid;
    cmpp_pack_t pack;
    lamb_submit_t submit;
    unsigned long long msgId;
    lamb_message_t bye;

    gid = getpid();
    lamb_set_process("lamb-client");
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    memset(&status, 0, sizeof(lamb_status_t));
    bye.type = LAMB_BYE;

    /* Redis Cache */
    err = lamb_cache_connect(&rdb, "127.0.0.1", 6379, NULL, 0);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to redis %s server", "127.0.0.1");
        return;
    }

    /* Connect to MT server */
    lamb_nn_option opt;

    memset(&opt, 0, sizeof(opt));
    opt.id = client->account->id;
    opt.type = LAMB_NN_PUSH;
    memcpy(opt.addr, config.listen, 16);
    
    err = lamb_nn_connect(&mt, &opt, config.mt_host, config.mt_port, NN_PAIR, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to mt %s server", config.mt_host);
        return;
    }

    /* Epoll Events */
    int epfd, nfds;
    struct epoll_event ev, events[32];

    epfd = epoll_create1(0);
    ev.data.fd = client->sock->fd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, client->sock->fd, &ev);

    /* Start Client Status Update Thread */
    lamb_start_thread(lamb_stat_loop, client, 1);

    /* Client Message Deliver */
    lamb_start_thread(lamb_deliver_loop, client, 1);

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            /* Waiting for receive request */
            err = cmpp_recv(client->sock, &pack, sizeof(pack));
            if (err) {
                if (err == -1) {
                    lamb_errlog(config.logfile, "connection closed by client %s\n", client->addr);
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
                total++;
                status.recv++;
                
                /* Generate Message ID */
                msgId = lamb_gen_msgid(gid, lamb_sequence());
                cmpp_pack_set_integer(&pack, cmpp_submit_msg_id, msgId, 8);

                /* Message Resolution */
                memset(&submit, 0, sizeof(lamb_submit_t));
                submit.type = LAMB_SUBMIT;
                submit.id = msgId;
                submit.account = client->account->id;
                strcpy(submit.spid, client->account->username);
                cmpp_pack_get_string(&pack, cmpp_submit_dest_terminal_id, submit.phone, 24, 21);
                cmpp_pack_get_string(&pack, cmpp_submit_src_id, submit.spcode, 24, 21);
                cmpp_pack_get_integer(&pack, cmpp_submit_msg_fmt, &submit.msgFmt, 1);

                /* Check Message Encoded */
                int codeds[] = {0, 8, 11, 15};
                if (!lamb_check_msgfmt(submit.msgFmt, codeds, sizeof(codeds) / sizeof(int))) {
                    result = 11;
                    status.fmt++;
                    goto response;
                }
                
                cmpp_pack_get_integer(&pack, cmpp_submit_msg_length, &submit.length, 1);

                /* Check Message Length */
                if (submit.length > 159 || submit.length < 1) {
                    result = 4;
                    status.len++;
                    goto response;
                }
                
                cmpp_pack_get_string(&pack, cmpp_submit_msg_content, submit.content, 160, submit.length);

                /* Write Message to MT Server */
                rc = nn_send(mt, (char *)&submit, sizeof(submit), 0);
                if (rc != sizeof(submit)) {
                    result = 12;
                    status.err++;
                } else {
                    status.store++;
                 }

                /* Submit Response */
            response:
                cmpp_submit_resp(client->sock, sequenceId, msgId, result);
                break;
            case CMPP_DELIVER_RESP:;
                result = 0;
                status.ack++;

                cmpp_pack_get_integer(&pack, cmpp_deliver_resp_result, &result, 1);
                cmpp_pack_get_integer(&pack, cmpp_deliver_resp_msg_id, &msgId, 8);
                
                if ((confirmed.sequenceId == sequenceId) && (confirmed.msgId == msgId) && (result == 0)) {
                    pthread_cond_signal(&cond);
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
    nn_send(mt, &bye, sizeof(bye), 0);
    nn_close(mt);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
    
    return;
}

void *lamb_deliver_loop(void *data) {
    char *buf;
    int rc, err;
    lamb_message_t req;
    lamb_client_t *client;
    lamb_report_t *report;
    lamb_deliver_t *deliver;
    lamb_message_t *message;
    unsigned int sequenceId;

    client = (lamb_client_t *)data;

    /* Connect to MO server */
    lamb_nn_option opt;

    memset(&opt, 0, sizeof(opt));
    opt.id = client->account->id;
    opt.type = LAMB_NN_PULL;
    memcpy(opt.addr, config.listen, 16);
    
    err = lamb_nn_connect(&mo, &opt, config.mo_host, config.mo_port, NN_REQ, config.timeout);
    if (err) {
        lamb_errlog(config.logfile, "can't connect to MO %s server", config.mo_host);
        pthread_exit(NULL);
    }
    
    int rlen, dlen;

    rlen = sizeof(lamb_report_t);
    dlen = sizeof(lamb_deliver_t);

    req.type = LAMB_REQ;

    while (true) {
        rc = nn_send(mo, &req, sizeof(req), 0);

        if (rc < 0) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(mo, &buf, NN_MSG, 0);
        if (rc < 0) {
            lamb_sleep(1000);
            continue;
        }

        if (rc != rlen && rc != dlen) {
            nn_freemsg(buf);
            lamb_sleep(1000);
            continue;
        }

        message = (lamb_message_t *)buf;

        if (message->type == LAMB_REPORT) {
            report = (lamb_report_t *)message;
            sequenceId = confirmed.sequenceId = cmpp_sequence();
            confirmed.msgId = report->id;
            
        report:
            err = cmpp_report(client->sock, sequenceId, report->id, report->spcode, report->status, report->submitTime, report->doneTime, report->phone, 0);
            if (err) {
                status.err++;
                lamb_errlog(config.logfile, "sending 'cmpp_report' packet to client %s failed", client->addr);
                lamb_sleep(3000);
                goto report;
            }
            status.rep++;
        } else if (message->type == LAMB_DELIVER) {
            deliver = (lamb_deliver_t *)message;
            sequenceId = confirmed.sequenceId = cmpp_sequence();
            confirmed.msgId = deliver->id;

        deliver:
            err = cmpp_deliver(client->sock, sequenceId, deliver->id, deliver->spcode, deliver->phone, deliver->content, deliver->msgFmt, 8);
            if (err) {
                status.err++;
                lamb_errlog(config.logfile, "sending 'cmpp_deliver' packet to client %s failed", client->addr);
                lamb_sleep(3000);
                goto deliver;
            }
            status.rep++;
        }

        /* Waiting for message confirmation */
        err = lamb_wait_confirmation(&cond, &mutex, 3);

        if (err == ETIMEDOUT) {
            status.timeo++;
            if (message->type == LAMB_REPORT) {
                goto report;
            } else if (message->type == LAMB_DELIVER) {
                goto deliver;
            }
        }

        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *data) {
    lamb_client_t *client;
    redisReply *reply = NULL;
    unsigned long long last, error;

    last = 0;
    client = (lamb_client_t *)data;

    while (true) {
        error = status.timeo + status.fmt + status.len + status.err;
        reply = redisCommand(rdb.handle, "HMSET client.%d pid %u speed %llu error %llu", client->account->id, getpid(), (unsigned long long)((total - last) / 5), error);
        if (reply != NULL) {
            freeReplyObject(reply);
            reply = NULL;
        } else {
            lamb_errlog(config.logfile, "Lamb exec redis command error", 0);
        }

        printf("-[ %s ]-> recv: %llu, store: %llu, rep: %llu, delv: %llu, ack: %llu, timeo: %llu, fmt: %llu, len: %llu, err: %llu\n",
               client->account->username, status.recv, status.store, status.rep, status.delv, status.ack, status.timeo, status.fmt, status.len, status.err);

        total = 0;
        sleep(5);
    }

    pthread_exit(NULL);
}

bool lamb_is_login(lamb_cache_t *cache, int account) {
    int pid = 0;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGET client.%d pid", account);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING) {
            pid = (reply->len > 0) ? atoi(reply->str) : 0;
            if (pid > 0) {
                if (kill(pid, 0) == 0) {
                    return true;
                }
            }
        }

        freeReplyObject(reply);
    }

    return false;
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

    if (lamb_get_string(&cfg, "AcHost", conf->ac_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisHost' parameter\n");
    }

    if (lamb_get_int(&cfg, "AcPort", &conf->ac_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'redisPort' parameter\n");
    }

    if (conf->ac_port < 1 || conf->ac_port > 65535) {
        fprintf(stderr, "ERROR: Invalid AC port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MtHost", conf->mt_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MT server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MtPort", &conf->mt_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MtPort' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "MoHost", conf->mo_host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid MO server IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "MoPort", &conf->mo_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'MoPort' parameter\n");
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

