
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
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
#include "common.h"
#include "socket.h"
#include "config.h"
#include "message.h"

static int mt, mo;
static cmpp_ismg_t cmpp;
static lamb_cache_t *rdb;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_status_t status;
static lamb_confirmed_t confirmed;
static unsigned long long total = 0;

int main(int argc, char *argv[]) {
    bool background = false;
    char *file = "ismg.conf";
    
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

    /* Check lock protection */
    int lock;

    if (lamb_lock_protection(&lock, "/tmp/ismg.lock")) {
        fprintf(stderr, "Already started, please do not repeat the start!\n");
        return -1;
    }

    /* Daemon mode */
    if (background) {
        lamb_daemon();
    }

    if (setenv("logfile", config.logfile, 1) == -1) {
        fprintf(stderr, "setenv error: %s\n", strerror(errno));
        return -1;
    }
    
    /* Signal event processing */
    lamb_signal_processing();

    int err;

    /* Redis database */
    rdb = (lamb_cache_t *)malloc(sizeof(lamb_cache_t));
    if (!rdb) {
        lamb_log(LOG_ERR, "the kernel can't allocate memory");
        return -1;
    }

    err = lamb_cache_connect(rdb, "127.0.0.1", 6379, NULL, 0);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to redis server");
        return -1;
    }

    /* Cmpp ISMG Gateway Initialization */
    err = cmpp_init_ismg(&cmpp, config.listen, config.port);
    if (err) {
        lamb_log(LOG_ERR, "Cmpp server initialization failed");
        return -1;
    }

    lamb_log(LOG_INFO, "ismgd listen on %s port %d",
             config.listen, config.port);
    
    fprintf(stdout, "Cmpp server initialization successfull\n");

    /* Setting Cmpp Socket Parameter */
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_SENDTIMEOUT, config.send_timeout);
    cmpp_sock_setting(&cmpp.sock, CMPP_SOCK_RECVTIMEOUT, config.recv_timeout);

    if (config.daemon) {
        lamb_log(LOG_ERR, "lamb server listen %s port %d",
                 config.listen, config.port);
    } else {
        fprintf(stderr, "lamb server listen %s port %d\n",
                config.listen, config.port);
    }

    /* Setting process information */
    lamb_set_process("lamb-ismgd");

    /* Start Main Event Thread */
    lamb_event_loop(&cmpp);

    /* Release lock protection */
    lamb_lock_release(&lock);

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
                    lamb_log(LOG_ERR, "Lamb server accept client connect error");
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
                lamb_log(LOG_INFO, "New client connection form %s",
                         inet_ntoa(clientaddr.sin_addr));
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
                        lamb_log(LOG_INFO, "Client closed the connection from %s",
                                 inet_ntoa(clientaddr.sin_addr));
                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                        close(sockfd);
                        continue;
                    }

                    lamb_log(LOG_WARNING, "Incorrect packet format from client %s",
                             inet_ntoa(clientaddr.sin_addr));
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
                        lamb_log(LOG_WARNING, "Version not supported from client %s",
                                 inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }
                    
                    /* Check SourceAddr */
                    char key[32];
                    char username[8];
                    char password[64];

                    memset(username, 0, sizeof(username));
                    cmpp_pack_get_string(&pack, cmpp_connect_source_addr, username,
                                         sizeof(username), 6);
                    snprintf(key, sizeof(key), "account.%s", username);
                    
                    if (!lamb_cache_has(rdb, key)) {
                        cmpp_connect_resp(&sock, sequenceId, 2);
                        lamb_log(LOG_WARNING, "Incorrect source address from client %s",
                                 inet_ntoa(clientaddr.sin_addr));
                        continue;
                    }

                    memset(password, 0, sizeof(password));
                    lamb_cache_hget(rdb, key, "password", password, sizeof(password));

                    /* Check AuthenticatorSource */
                    if (cmpp_check_authentication(&pack, sizeof(cmpp_pack_t), username, password)) {
                        lamb_account_t account;
                        memset(&account, 0, sizeof(account));
                        err = lamb_account_get(rdb, username, &account);
                        if (err) {
                            cmpp_connect_resp(&sock, sequenceId, 9);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                            close(sockfd);
                            lamb_log(LOG_ERR, "Can't fetch account information");
                            continue;
                        }

                        /* Check Duplicate Logon */
                        if (lamb_is_login(rdb, account.id)) {
                            cmpp_connect_resp(&sock, sequenceId, 10);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                            close(sockfd);
                            lamb_log(LOG_WARNING, "Duplicate login from client %s",
                                     inet_ntoa(clientaddr.sin_addr));
                            continue;
                        }

                        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);

                        /* Login Successfull */
                        lamb_log(LOG_INFO, "Login successfull from client %s",
                                 inet_ntoa(clientaddr.sin_addr));

                        /* Create Work Process */
                        pid_t pid = fork();
                        if (pid < 0) {
                            lamb_log(LOG_ERR, "Unable to fork child process");
                        } else if (pid == 0) {
                            close(epfd);
                            cmpp_ismg_close(cmpp);
                            lamb_cache_close(rdb);
                            
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
                        lamb_log(LOG_WARNING, "Login failed form client %s",
                                 inet_ntoa(clientaddr.sin_addr));
                    }
                } else {
                    lamb_log(LOG_WARNING, "Unable to resolve packets from client %s",
                             inet_ntoa(clientaddr.sin_addr));
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
    unsigned long long msgId;

    gid = getpid();
    lamb_set_process("lamb-client");
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    memset(&status, 0, sizeof(lamb_status_t));

    /* Redis Cache */
    err = lamb_cache_connect(rdb, "127.0.0.1", 6379, NULL, 0);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to redis %s server", "127.0.0.1");
        return;
    }

    /* Connect to MT server */
    mt = lamb_nn_pair(config.mt, client->account->id, config.timeout);
    if (mt < 0) {
        lamb_log(LOG_ERR, "can't connect to mt %s", config.mt);
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

    /* On line signal state synchronization */
    lamb_start_thread(lamb_online_loop, client, 1);

    int len;
    char *pk, *buf;
    char phone[21] = {0};
    char spcode[21] = {0};
    int msgFmt = 0;
    int length = 0;
    char content[160] = {0};
    Submit message = SUBMIT__INIT;

    while (true) {
        nfds = epoll_wait(epfd, events, 32, -1);

        if (nfds > 0) {
            /* Waiting for receive request */
            err = cmpp_recv(client->sock, &pack, sizeof(pack));
            if (err) {
                if (err == -1) {
                    lamb_log(LOG_INFO, "connection closed by client %s\n", client->addr);
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
                message.id = msgId;
                message.account = client->account->id;
                message.company = client->account->company;
                message.spid = client->account->username;
                cmpp_pack_get_string(&pack, cmpp_submit_dest_terminal_id, phone, 21, 20);
                message.phone = phone;
                cmpp_pack_get_string(&pack, cmpp_submit_src_id, spcode, 21, 20);
                message.spcode = spcode;
                cmpp_pack_get_integer(&pack, cmpp_submit_msg_fmt, &msgFmt, 1);

                /* Check Message Encoded */
                int codeds[] = {0, 8, 11, 15};
                if (!lamb_check_format(msgFmt, codeds, sizeof(codeds) / sizeof(int))) {
                    result = 11;
                    status.fmt++;
                    goto response;
                }

                message.msgfmt = msgFmt;

                cmpp_pack_get_integer(&pack, cmpp_submit_msg_length, &length, 1);
                
                /* Check Message Length */
                if (length > 159 || length < 1) {
                    result = 4;
                    status.len++;
                    goto response;
                }

                message.length = length;

                cmpp_pack_get_string(&pack, cmpp_submit_msg_content, content, 160, length);
                message.content.len = length;
                message.content.data = (uint8_t *)content;

                len = submit__get_packed_size(&message);
                pk = (char *)malloc(len);

                if (!pk) {
                    result = 12;
                    goto response;
                }

                submit__pack(&message, (uint8_t *)pk);

                len = lamb_pack_assembly(&buf, LAMB_SUBMIT, pk, len);

                /* Write Message to MT Server */
                rc = nn_send(mt, buf, len, NN_DONTWAIT);

                if (rc != len) {
                    result = 13;
                    status.err++;
                } else {
                    status.store++;
                }

                free(pk);
                free(buf);

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
    lamb_nn_close(mt);
    cmpp_sock_close(client->sock);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
    
    return;
}

void *lamb_deliver_loop(void *data) {
    char *buf;
    int rc, len, err;
    lamb_client_t *client;
    unsigned int sequenceId;
    char *stat;
    Report *report;
    Deliver *deliver;

    client = (lamb_client_t *)data;

    /* Connect to MO server */
    mo = lamb_nn_reqrep(config.mo, client->account->id, config.timeout);
    if (mo < 0) {
        lamb_log(LOG_ERR, "can't connect to mo %s", config.mo);
        pthread_exit(NULL);
    }

    char *req;
    len = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);

    while (true) {
        rc = nn_send(mo, req, len, NN_DONTWAIT);

        if (rc != len) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(mo, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            lamb_sleep(100);
            continue;
        }

        /* No available messages */
        if (CHECK_COMMAND(buf) == LAMB_EMPTY) {
            nn_freemsg(buf);
            lamb_sleep(200);
            continue;
        }

        /* State report message */
        if (CHECK_COMMAND(buf) == LAMB_REPORT) {
            report = report__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));

            if (!report) {
                nn_freemsg(buf);
                continue;
            }

            sequenceId = confirmed.sequenceId = cmpp_sequence();
            confirmed.msgId = report->id;

            /* State report type */
            switch (report->status) {
            case 1:
                stat = "DELIVRD";
                break;
            case 2:
                stat = "EXPIRED";
                break;
            case 3:
                stat = "DELETED";
                break;
            case 4:
                stat = "UNDELIV";
                break;
            case 5:
                stat = "ACCEPTD";
                break;
            case 6:
                stat = "UNKNOWN";
                break;
            case 7:
                stat = "REJECTD";
                break;
            default:
                stat = "UNKNOWN";
                break;
            }

        report:
            err = cmpp_report(client->sock, sequenceId, report->id, report->spcode, stat,
                              report->submittime, report->donetime, report->phone, 0);
            if (err) {
                status.err++;
                lamb_log(LOG_WARNING, "sending 'cmpp_report' packet to client %s failed", client->addr);
            }
        } else if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
            /* User message delivery */
            deliver = deliver__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));

            if (!deliver) {
                nn_freemsg(buf);
                continue;
            }

            sequenceId = confirmed.sequenceId = cmpp_sequence();
            confirmed.msgId = deliver->id;

        deliver:
            err = cmpp_deliver(client->sock, sequenceId, deliver->id, deliver->spcode, deliver->phone,
                               (char *)deliver->content.data, deliver->content.len, deliver->msgfmt);
            if (err) {
                status.err++;
                lamb_log(LOG_WARNING, "sending 'cmpp_deliver' packet to client %s failed", client->addr);
            }
        }

        /* Waiting for ACK message confirmation */
        err = lamb_wait_confirmation(&cond, &mutex, config.acknowledge_timeout);

        if (err == ETIMEDOUT) {
            status.timeo++;
            if (CHECK_COMMAND(buf) == LAMB_REPORT) {
                goto report;
            } else if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
                goto deliver;
            }
        }

        if (CHECK_COMMAND(buf) == LAMB_REPORT) {
            report__free_unpacked(report, NULL);
        } else if (CHECK_COMMAND(buf) == LAMB_DELIVER) {
            deliver__free_unpacked(deliver, NULL);
        }

        status.rep++;        
        nn_freemsg(buf);
    }

    pthread_exit(NULL);
}

void *lamb_stat_loop(void *data) {
    int signal;
    lamb_client_t *client;
    unsigned long long speed;
    unsigned long long error;
    time_t last_time;
    int interval;

    client = (lamb_client_t *)data;
    redisReply *reply = NULL;
    last_time = time(NULL);

    while (true) {
        interval = time(NULL) - last_time;

        if (interval > 0) {
            speed = total / interval;
        }

        total = 0;
        last_time = time(NULL);

        error = status.timeo + status.fmt + status.len + status.err;

        pthread_mutex_lock(&rdb->lock);
        reply = redisCommand(rdb->handle, "HMSET client.%d pid %u addr %s speed %llu error %llu",
                             client->account->id, getpid(), client->addr, speed, error);
        pthread_mutex_unlock(&rdb->lock);

        if (reply != NULL) {
            freeReplyObject(reply);
            reply = NULL;
        } else {
            lamb_log(LOG_ERR, "lamb exec redis command error");
        }

#ifdef _DEBUG
        printf("-[ %s ]-> recv: %llu, store: %llu, rep: %llu, delv: %llu, ack: %llu, "
               "timeo: %llu, fmt: %llu, len: %llu, err: %llu\n",
               client->account->username, status.recv, status.store, status.rep,
               status.delv, status.ack, status.timeo, status.fmt, status.len, status.err);
#endif

        pthread_mutex_lock(&rdb->lock);
        signal = lamb_check_signal(rdb, client->account->id);
        pthread_mutex_unlock(&rdb->lock);
        printf("-> signal: %d\n", signal);
        if (signal == 9) {
            lamb_nn_close(mt);
            lamb_nn_close(mo);
            lamb_sleep(1000);
            exit(EXIT_SUCCESS);
        }
        
        lamb_sleep(5000);
    }

    pthread_exit(NULL);
}

void *lamb_online_loop(void *arg) {
    lamb_client_t *client;

    client = (lamb_client_t *)arg;

    while (true) {
        pthread_mutex_lock(&rdb->lock);
        lamb_state_renewal(rdb, client->account->id);
        pthread_mutex_unlock(&rdb->lock);
        lamb_sleep(3000);
    }

    pthread_exit(NULL);
}

void lamb_mt_close(int sock) {

}

int lamb_state_renewal(lamb_cache_t *cache, int id) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HSET client.%d online %ld", id, time(NULL));

    if (reply != NULL) {
        freeReplyObject(reply);
        return 0;
    }

    return -1;
}

bool lamb_is_login(lamb_cache_t *cache, int account) {
    int last = 0;
    bool online = false;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGET client.%d online", account);
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING) {
            last = (reply->len > 0) ? atoi(reply->str) : 0;
            if ((time(NULL) - last) < 7) {
                online = true;
            }
        }

        freeReplyObject(reply);
    }

    return online;
}

int lamb_check_signal(lamb_cache_t *cache, int id) {
    int signal = 0;
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HGET client.%d signal", id);

    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_STRING && reply->len > 0) {
            signal = atoi(reply->str);
        }
        freeReplyObject(reply);
    }

    /* Reset signal state */
    lamb_clear_signal(cache, id);

    return signal;
}

void lamb_clear_signal(lamb_cache_t *cache, int id) {
    redisReply *reply = NULL;

    reply = redisCommand(cache->handle, "HSET client.%d signal 0", id);

    if (reply != NULL) {
        freeReplyObject(reply);
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

    if (lamb_get_int(&cfg, "Timeout", (int *)&conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RecvTimeout", (int *)&conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RecvTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "SendTimeout", (int *)&conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SendTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "AcknowledgeTimeout", (int *)&conf->acknowledge_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'AcknowledgeTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Ac' parameter\n");
    }

    if (lamb_get_string(&cfg, "Mt", conf->mt, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid MT server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Mo", conf->mo, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid MO server address\n");
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

