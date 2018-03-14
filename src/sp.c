
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <cmpp.h>
#include "config.h"
#include "queue.h"
#include "socket.h"
#include "message.h"
#include "sp.h"

static int scheduler;
static int delivery;
static lamb_db_t *db;
static cmpp_sp_t cmpp;
static lamb_cache_t *rdb;
static lamb_caches_t cache;
static lamb_list_t *storage;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_heartbeat_t heartbeat;
static lamb_confirmed_t confirmed;
static unsigned long long total;
static lamb_status_t status;
static lamb_statistical_t *statistical;

int main(int argc, char *argv[]) {
    char *file = "sp.conf";
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
    memset(&config, 0, sizeof(config));
    if (lamb_read_config(&config, file) != 0) {
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

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Start main event processing */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    total = 0;
    heartbeat.count = 0;
    lamb_set_process("lamb-gateway");
    memset(&status, 0, sizeof(status));
    
    /* Storage Initialization */
    storage = lamb_list_new();
    if (!storage) {
        lamb_log(LOG_ERR, "storage queue initialization failed");
        return;
    }

    lamb_debug("storage initialization successfull\n");

    statistical = (lamb_statistical_t *)calloc(1, sizeof(lamb_statistical_t));
    if (!statistical) {
        lamb_log(LOG_ERR, "the kernel can't allocate memory");
        return;
    }

    /* Redis initialization */
    rdb = (lamb_cache_t *)malloc(sizeof(lamb_cache_t));
    if (!rdb) {
        lamb_log(LOG_ERR, "redis database initialize failed");
        return;
    }

    err = lamb_cache_connect(rdb, config.redis_host, config.redis_port, NULL,
                             config.redis_db);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to redis %s", config.redis_host);
        return;
    }

    /* Database initialization */
    db = (lamb_db_t *)malloc(sizeof(lamb_db_t));
    if (!db) {
        lamb_log(LOG_ERR, "the kernel can't allocate memory");
        return;
    }

    err = lamb_db_init(db);
    if (err) {
        lamb_log(LOG_ERR, "database handle initialize failed");
        return;
    }

    /* Connect to database */
    err = lamb_db_connect(db, config.db_host, config.db_port, config.db_user,
                          config.db_password, config.db_name);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to database %s", config.db_host);
        return;
    }

    lamb_debug("connect to database successfull\n");

    /* Cache cluster initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7, 4);
    if (cache.len != 7) {
        lamb_log(LOG_ERR, "connect to cache cluster failed");
        return;
    }

    lamb_debug("connect to cache cluster successfull\n");

    /* Connect to scheduler server */
    scheduler = lamb_nn_reqrep(config.scheduler, config.id, config.timeout);
    if (scheduler < 0) {
        lamb_log(LOG_ERR, "can't connect to scheduler %s", config.scheduler);
        return;
    }

    lamb_debug("connect to scheduler server successfull\n");

    /* Connect to delivery server */
    delivery = lamb_nn_pair(config.delivery, config.id, config.timeout);
    if (delivery < 0) {
        lamb_log(LOG_ERR, "can't connect to delivery %s", config.delivery);
        return;
    }

    lamb_debug("connect to delivery server successfull\n");

    /* Cmpp client initialization */
    err = lamb_cmpp_init(&cmpp, &config);
    if (err) {
        return;
    }

    lamb_debug("connect cmpp gateway successfull\n");

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    
    /* Start Sender Thread */
    lamb_start_thread(lamb_sender_loop, NULL, 1);
    lamb_debug("start sender thread successfull\n");

    /* Start Work Thread */
    lamb_start_thread(lamb_work_loop, NULL, 1);
    lamb_debug("start work thread successfull\n");

    /* Start Deliver Thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);
    lamb_debug("start deliver thread successfull\n");

    /* Start Keepalive Thread */
    lamb_start_thread(lamb_cmpp_keepalive, NULL, 1);
    lamb_debug("start keepalive thread successfull\n");

    /* Start Status Update Thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);
    lamb_debug("start stat thread successfull\n");

    while (true) {
        lamb_sleep(3000);
    }
}

void *lamb_sender_loop(void *data) {
    int err;
    char spcode[21];
    long long delayed;
    unsigned int sequenceId;
    
    int msgFmt;
    char *tocode;

    /* Convert the coded name to the number */
    if (strcasecmp(config.encoding, "ASCII") == 0) {
        msgFmt = 0;
        tocode = "ASCII";
    } else if (strcasecmp(config.encoding, "UCS-2") == 0) {
        msgFmt = 8;
        tocode = "UCS-2BE";
    } else if (strcasecmp(config.encoding, "UTF-8") == 0) {
        msgFmt = 11;
        tocode = "UTF-8";
    } else {
        msgFmt = 15;
        tocode = "GBK";
    }

    int rc, len;
    char *req, *buf;
    Submit *message;
    int length;
    char content[256];

    memset(spcode, 0, sizeof(spcode));
    len = lamb_pack_assembly(&req, LAMB_REQ, NULL, 0);
    
    while (true) {
        if (!cmpp.ok) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_send(scheduler, req, len, NN_DONTWAIT);

        if (rc != len) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(scheduler, &buf, NN_MSG, 0);

        if (rc < HEAD) {
            if (rc > 0) {
                nn_freemsg(buf);
            }
            lamb_sleep(1000);
            continue;
        }
        
        if (CHECK_COMMAND(buf) == LAMB_EMPTY) {
            nn_freemsg(buf);
            lamb_sleep(100);
            continue;
        }

        if (CHECK_COMMAND(buf) != LAMB_SUBMIT) {
            nn_freemsg(buf);
            lamb_log(LOG_ERR, "only submit packets are allowed");
            continue;
        }

        message = submit__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!message) {
            lamb_log(LOG_ERR, "can't unpack for submit message packets");
            continue;
        }

        /* Caching message information */
        confirmed.id = message->id;
        strncpy(confirmed.spcode, message->spcode, 20);
        confirmed.account = message->account;
        confirmed.company = message->company;

        /* Spcode processing */
        if (config.extended) {
            snprintf(spcode, sizeof(spcode), "%s%s", config.spcode, message->spcode);
        } else {
            strcpy(spcode, config.spcode);
        }

        /* Message encode convert */
        memset(content, 0, sizeof(content));
        err = lamb_encoded_convert((char *)message->content.data, message->length, content,
                                   sizeof(content), "UTF-8", tocode, &length);

        if (err || (length == 0)) {
            continue;
        }

        /* Send message to gateway */
        sequenceId = confirmed.sequenceId = cmpp_sequence();
        err = cmpp_submit(&cmpp.sock, sequenceId, config.spid, spcode, message->phone,
                          content, length, msgFmt, true);
        submit__free_unpacked(message, NULL);

        /* Submit count statistical  */
        total++;
        status.sub++;
        statistical->submit++;

        if (err) {
            status.err++;
            lamb_sleep(config.interval * 1000);
            continue;
        }

        /* Wait for ACK confirmation */
        err = lamb_wait_confirmation(&cond, &mutex, config.acknowledge_timeout);

        if (err == ETIMEDOUT) {
            status.timeo++;
            lamb_sleep(config.interval * 1000);
        }

        /* Flow control */
        if (config.concurrent > 1000) {
            delayed = 1000;
        } else {
            delayed = 1000 / config.concurrent;
        }

        lamb_msleep(delayed);
    }

    pthread_exit(NULL);

}

void *lamb_deliver_loop(void *data) {
    int err;
    int result;
    char stat[8];
    cmpp_pack_t pack;
    cmpp_head_t *chp;
    unsigned int commandId;
    unsigned int sequenceId;
    unsigned long long msgId;
    char registered_delivery;
    lamb_report_t *report;
    lamb_deliver_t *deliver;

    while (true) {
        if (!cmpp.ok) {
            lamb_sleep(10);
            continue;
        }

        err = cmpp_recv_timeout(&cmpp.sock, &pack, sizeof(pack), config.recv_timeout);
        
        if (err) {
            lamb_sleep(10);
            continue;
        }

        result = 0;
        chp = (cmpp_head_t *)&pack;
        commandId = ntohl(chp->commandId);
        sequenceId = ntohl(chp->sequenceId);
        
        switch (commandId) {
        case CMPP_SUBMIT_RESP:;
            /* cmpp submit resp */
            status.ack++;
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_msg_id, &msgId, 8);
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_result, &result, 1);

            //lamb_debug("message response id: %llu, msgId: %llu, result: %u\n", id, msgId, result);
            
            if ((confirmed.sequenceId != sequenceId) || (result != 0)) {
                status.err++;
                lamb_debug("receive sending message error, result: %u\n", result);
                break;
            }

            pthread_cond_signal(&cond);
            lamb_set_cache(&cache, msgId, confirmed.id, confirmed.account, confirmed.company, confirmed.spcode);
            //lamb_debug("receive msgId: %llu message confirmation, result: %d\n", msgId, result);

            break;
        case CMPP_DELIVER:;
            /* Cmpp Deliver */
            cmpp_pack_get_integer(&pack, cmpp_deliver_registered_delivery, &registered_delivery, 1);

            if (registered_delivery == 1) {
                status.rep++;
                report = (lamb_report_t *)calloc(1, sizeof(lamb_report_t));

                if (!report) {
                    result = 9;
                    goto response1;
                }

                memset(stat, 0, sizeof(stat));

                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content_msg_id, &report->id, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, report->phone, 21, 20);

                /* Stat */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_stat, stat, 8, 7);

                /* Submit_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_submit_time, report->submittime, 11, 10);

                /* Done_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_done_time, report->donetime, 11, 10);

                /* Stat */
                if (strncasecmp(stat, "DELIVRD", 7) == 0) {
                    report->status = 1;
                    statistical->delivrd++;
                } else if (strncasecmp(stat, "EXPIRED", 7) == 0) {
                    report->status = 2;
                    statistical->expired++;
                } else if (strncasecmp(stat, "DELETED", 7) == 0) {
                    report->status = 3;
                    statistical->deleted++;
                } else if (strncasecmp(stat, "UNDELIV", 7) == 0) {
                    report->status = 4;
                    statistical->undeliv++;
                } else if (strncasecmp(stat, "ACCEPTD", 7) == 0) {
                    report->status = 5;
                    statistical->acceptd++;
                } else if (strncasecmp(stat, "UNKNOWN", 7) == 0) {
                    report->status = 6;
                    statistical->unknown++;
                } else if (strncasecmp(stat, "REJECTD", 7) == 0) {
                    report->status = 7;
                    statistical->rejectd++;
                } else {
                    report->status = 6;
                    statistical->unknown++;
                }

                report->type = LAMB_REPORT;
                lamb_list_rpush(storage, lamb_node_new(report));

                lamb_debug("receive msgId: %llu, phone: %s, stat: %s, submitTime: %s, doneTime: %s\n",
                           report->id, report->phone, stat, report->submittime, report->donetime);

            response1:
                cmpp_deliver_resp(&cmpp.sock, sequenceId, report->id, result);
                lamb_node_t *node = lamb_list_lpop(storage);
                if (node) {
                    free(node->val);
                    free(node);
                }
            } else {
                status.delv++;
                deliver = (lamb_deliver_t *)calloc(1, sizeof(lamb_deliver_t));

                if (!deliver) {
                    result = 9;
                    goto response2;
                }

                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_id, &deliver->id, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, deliver->phone, 21, 20);

                /* Dest_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_dest_id, deliver->spcode, 21, 20);

                /* Service_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_service_id, deliver->serviceid, 11, 10);

                /* Msg_Fmt */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_fmt, &deliver->msgfmt, 1);

                /* Msg_Length */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_length, &deliver->length, 1);

                /* Msg_Content */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content, deliver->content, 160,
                                     deliver->length);

                deliver->type = LAMB_DELIVER;
                lamb_list_rpush(storage, lamb_node_new(deliver));

                
                lamb_debug("receive msgId: %llu, phone: %s, spcode: %s, msgFmt: %d, length: %d\n",
                           deliver->id, deliver->phone, deliver->spcode, deliver->msgfmt,
                           deliver->length);

            response2:
                cmpp_deliver_resp(&cmpp.sock, sequenceId, deliver->id, result);
            }
            break;
        case CMPP_ACTIVE_TEST_RESP:
            if (heartbeat.sequenceId == sequenceId) {
                heartbeat.count = 0;
            }
            break;
        }
    }

    pthread_exit(NULL);
}

void *lamb_cmpp_keepalive(void *data) {
    int err;
    unsigned int sequenceId;

    while (true) {
        sequenceId = heartbeat.sequenceId = cmpp_sequence();
        err = cmpp_active_test(&cmpp.sock, sequenceId);
        if (err) {
            lamb_log(LOG_ERR, "sending keepalive packet to %s gateway failed", config.host);
        }

        heartbeat.count++;
        
        if (heartbeat.count >= config.retry) {
            cmpp_sp_close(&cmpp);
            lamb_cmpp_reconnect(&cmpp, &config);
            heartbeat.count = 0;
        }

        lamb_sleep(30000);
    }

    pthread_exit(NULL);
}

void *lamb_work_loop(void *data) {
    int len;
    void *pk;
    char *buf;
    void *message;
    lamb_report_t *r;
    lamb_deliver_t *d;
    lamb_node_t *node;

    unsigned long long msgId;
    char spcode[21];
    int account;
    int company;

    int slen = strlen(config.spcode);
    Report report = REPORT__INIT;
    Deliver deliver = DELIVER__INIT;

    while (true) {
        node = lamb_list_lpop(storage);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = node->val;

        if (CHECK_TYPE(message) == LAMB_REPORT) {
            r = (lamb_report_t *)message;
            msgId = account = company = 0;
            memset(spcode, 0, sizeof(spcode));
            lamb_get_cache(&cache, r->id, &msgId, &account, &company, spcode, sizeof(spcode));

            if (msgId > 0 && account > 0 && company > 0) {
                lamb_del_cache(&cache, msgId);
            } else {
                goto done;
            }

            report.id = msgId;
            report.account = account;
            report.company = company;
            report.spcode = spcode;
            report.phone = r->phone;
            report.status = r->status;
            report.submittime = r->submittime;
            report.donetime = r->donetime;

            len = report__get_packed_size(&report);
            pk = malloc(len);

            if (!pk) {
                goto done;
            }

            report__pack(&report, pk);
            len = lamb_pack_assembly(&buf, LAMB_REPORT, pk, len);

            if (len > 0) {
                nn_send(delivery, buf, len, 0);
                free(buf);
            }
            free(pk);
        } else if (CHECK_TYPE(message) == LAMB_DELIVER) {
            d = (lamb_deliver_t *)message;

            deliver.id = d->id;
            deliver.account = 0;
            deliver.company = 0;
            deliver.phone = d->phone;
            deliver.spcode = d->spcode;

            if (strlen(d->spcode) > slen) {
                deliver.serviceid = d->spcode + slen;
            } else {
                deliver.serviceid = "";
            }

            deliver.msgfmt = d->msgfmt;
            deliver.length = d->length;
            deliver.content.len = d->length;
            deliver.content.data = (void *)d->content;

            len = deliver__get_packed_size(&deliver);
            pk = malloc(len);

            if (!pk) {
                goto done;
            }

            deliver__pack(&deliver, pk);
            len = lamb_pack_assembly(&buf, LAMB_DELIVER, pk, len);

            if (len > 0) {
                nn_send(delivery, buf, len, 0);
                free(buf);
            }

            free(pk);
        }

    done:
        free(message);
        free(node);
    }

    pthread_exit(NULL);
}

void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config) {
    lamb_log(LOG_ERR, "reconnecting to gateway %s ...", config->host);
    while (lamb_cmpp_init(cmpp, config) != 0) {
        lamb_sleep(config->interval * 1000);
    }
    lamb_log(LOG_ERR, "connect to gateway %s successfull", config->host);
    return;
}

int lamb_cmpp_init(cmpp_sp_t *cmpp, lamb_config_t *config) {
    int err;
    cmpp_head_t *chp;
    cmpp_pack_t pack;
    unsigned int sequenceId;
    unsigned int responseId;

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_CONTIMEOUT, config->timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_SENDTIMEOUT, config->send_timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_RECVTIMEOUT, config->recv_timeout);

    /* Initialization cmpp connection */
    err = cmpp_init_sp(cmpp, config->host, config->port);
    if (err) {
        lamb_log(LOG_ERR, "can't connect to gateway %s server", config->host);
        return 1;
    }

    /* login to cmpp gateway */
    sequenceId = cmpp_sequence();
    err = cmpp_connect(&cmpp->sock, sequenceId, config->user, config->password);
    if (err) {
        lamb_log(LOG_ERR, "sending connection request to %s failed", config->host);
        return 2;
    }

    err = cmpp_recv_timeout(&cmpp->sock, &pack, sizeof(pack), config->recv_timeout);
    if (err) {
        lamb_log(LOG_ERR, "receive gateway response packet from %s failed", config->host);
        return 3;
    }

    chp = (cmpp_head_t *)&pack;
    responseId = ntohl(chp->sequenceId);

    if (cmpp_check_method(&pack, sizeof(pack), CMPP_CONNECT_RESP) && (sequenceId == responseId)) {
        unsigned char status;
        cmpp_pack_get_integer(&pack, cmpp_connect_resp_status, &status, 1);
        switch (status) {
        case 0:
            cmpp->ok = true;
            goto success;
        case 1:
            lamb_log(LOG_ERR, "incorrect protocol packets");
            break;
        case 2:
            lamb_log(LOG_ERR, "illegal source address");
            break;
        case 3:
            lamb_log(LOG_ERR, "authenticator failed");
            break;
        case 4:
            lamb_log(LOG_ERR, "protocol version is too high");
            break;
        default:
            lamb_log(LOG_ERR, "cmpp unknown error, code: %d", status);
            break;
        }

        return 4;
    } else {
        lamb_log(LOG_ERR, "incorrect response packet from %s gateway", config->host);
        return 5;
    }
    
success:
    return 0;
}

void *lamb_stat_loop(void *data) {
    int err;
    redisReply *reply = NULL;
    lamb_statistical_t last;
    lamb_statistical_t result;
    lamb_statistical_t current;
    unsigned long long error;

    memset(&last, 0, sizeof(lamb_statistical_t));
    last.gid = result.gid = current.gid = config.id;

    while (true) {
        error = status.err + status.timeo;
        reply = redisCommand(rdb->handle, "HMSET gateway.%d pid %u "
                             "status %d speed %llu error %llu",
                             config.id, getpid(), cmpp.ok ? 1 : 0,
                             (unsigned long long)(total / 5),
                             error);
        total = 0;

        if (reply != NULL) {
            freeReplyObject(reply);
        } else {
            lamb_log(LOG_ERR, "redis command executes errors");
        }

        current.submit = statistical->submit;
        current.delivrd = statistical->delivrd;
        current.expired = statistical->expired;
        current.deleted = statistical->deleted;
        current.undeliv = statistical->undeliv;
        current.acceptd = statistical->acceptd;
        current.unknown = statistical->unknown;
        current.rejectd = statistical->rejectd;

        result.submit = current.submit - last.submit;
        result.delivrd = current.delivrd - last.delivrd;
        result.expired = current.expired - last.expired;
        result.deleted = current.deleted - last.deleted;
        result.undeliv = current.undeliv - last.undeliv;
        result.acceptd = current.acceptd - last.acceptd;
        result.unknown = current.unknown - last.unknown;
        result.rejectd = current.rejectd - last.rejectd;

        /* Write data statistical */
        if (result.submit > 0 || result.delivrd > 0 || result.expired > 0 ||
            result.deleted > 0 || result.undeliv > 0 || result.acceptd > 0 ||
            result.unknown || result.rejectd > 0) {
            err = lamb_write_statistical(db, &last);
            
            if (err) {
                lamb_log(LOG_ERR, "can't write data statistical to database");
            }
        }

        last.submit = current.submit;
        last.delivrd = current.delivrd;
        last.expired = current.expired;
        last.deleted = current.deleted;
        last.undeliv = current.undeliv;
        last.acceptd = current.acceptd;
        last.unknown = current.unknown;
        last.rejectd = current.rejectd;

        lamb_debug("queue: %u, sub: %llu, ack: %llu, rep: %llu, delv: %llu, timeo: %llu"
                   ", err: %llu\n", storage->len, status.sub, status.ack, status.rep, status.delv,
                   status.timeo, status.err);

        sleep(5);
    }

    pthread_exit(NULL);
}

int lamb_set_cache(lamb_caches_t *caches, unsigned long long msgId, unsigned long long id,
                   int account, int company, char *spcode) {
    int i;
    redisReply *reply = NULL;
    
    i = (msgId % caches->len);

    pthread_mutex_lock(&caches->nodes[i]->lock);
    reply = redisCommand(caches->nodes[i]->handle, "HMSET %llu id %llu account %d company %d spcode %s",
                         msgId, id, account, company, spcode);
    pthread_mutex_unlock(&caches->nodes[i]->lock);

    if (reply != NULL) {
        freeReplyObject(reply);
        return 0;
    }

    return -1;
}

int lamb_get_cache(lamb_caches_t *caches, unsigned long long id, unsigned long long *msgId,
                   int *account, int *company, char *spcode, size_t size) {
    int i;
    redisReply *reply = NULL;

    i = (id % caches->len);

    pthread_mutex_lock(&caches->nodes[i]->lock);
    reply = redisCommand(caches->nodes[i]->handle, "HMGET %llu id account company spcode", id);
    pthread_mutex_unlock(&caches->nodes[i]->lock);

    if (!reply) {
        return -1;
    }

    if (reply->type == REDIS_REPLY_ARRAY) {
        if (reply->elements == 4) {
            *msgId = (reply->element[0]->len > 0) ? strtoull(reply->element[0]->str, NULL, 10) : 0;
            *account = (reply->element[1]->len > 0) ? atoi(reply->element[1]->str) : 0;
            *company = (reply->element[2]->len > 0) ? atoi(reply->element[2]->str) : 0;
            if (reply->element[3]->len >= size) {
                memcpy(spcode, reply->element[3]->str, size - 1);
            } else {
                memcpy(spcode, reply->element[3]->str, reply->element[3]->len);
            }
        }
    }

    freeReplyObject(reply);

    return 0;
}

int lamb_del_cache(lamb_caches_t *caches, unsigned long long msgId) {
    int i;
    redisReply *reply = NULL;

    i = (msgId % caches->len);

    pthread_mutex_lock(&caches->nodes[i]->lock);
    reply = redisCommand(caches->nodes[i]->handle, "DEL %llu", msgId);
    pthread_mutex_unlock(&caches->nodes[i]->lock);
    
    if (reply != NULL) {
        freeReplyObject(reply);
        return 0;
    }

    return -1;
}

int lamb_write_statistical(lamb_db_t *db, lamb_statistical_t *stat) {
    char sql[512];
    PGresult *res = NULL;

    if (!db || !stat) {
        return -1;
    }

    snprintf(sql, sizeof(sql), "INSERT INTO statistical "
             "VALUES(%d, %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld, current_date)",
             stat->gid, stat->submit, stat->delivrd, stat->expired, stat->deleted,
             stat->undeliv, stat->acceptd, stat->unknown, stat->rejectd);

    res = PQexec(db->conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);

    return 0;
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

    if (lamb_get_int(&cfg, "Timeout", (int *)&conf->timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Timeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Retry", &conf->retry) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Retry' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Interval", &conf->interval) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Interval' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "SendTimeout", (int *)&conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SendTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RecvTimeout", (int *)&conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RecvTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "AcknowledgeTimeout", (int *)&conf->acknowledge_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'AcknowledgeTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPort' parameter\n");
        goto error;
    }

    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "ERROR: Invalid redis port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "RedisPassword", conf->redis_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisPassword' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RedisDb' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Host", conf->host, 16) != 0) {
        fprintf(stderr, "ERROR: Invalid host IP address\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Port", &conf->port) != 0) {
        fprintf(stderr, "ERROR: Can't read Port parameter\n");
        goto error;
    }

    if (conf->port < 1 || conf->port > 65535) {
        fprintf(stderr, "ERROR: Invalid host Port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "User", conf->user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'User' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Password", conf->password, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Password' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Spid", conf->spid, 8) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Spid' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "SpCode", conf->spcode, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SpCode' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Encoding", conf->encoding, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Encoding' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "Concurrent", &conf->concurrent) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Concurrent' parameter\n");
    }

    if (lamb_get_bool(&cfg, "Extended", &conf->extended) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Extended' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "BackFile", conf->backfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BackFile' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'LogFile' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Ac' parameter\n");
    }

    if (lamb_get_string(&cfg, "Scheduler", conf->scheduler, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid Scheduler server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Delivery", conf->delivery, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid Delivery server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbHost' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPort' parameter\n");
        goto error;
    }

    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "ERROR: Invalid DB port number\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbUser' parameter\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbPassword' parameter\n");
        goto error;
    }
    
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "ERROR: Can't read 'DbName' parameter\n");
        goto error;
    }

    char node[32];
    memset(node, 0, sizeof(node));

    if (lamb_get_string(&cfg, "node1", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[0] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node2", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node2' parameter\n");
        goto error;
    }
    conf->nodes[1] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node3", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node3' parameter\n");
        goto error;
    }
    conf->nodes[2] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node4", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node4' parameter\n");
        goto error;
    }
    conf->nodes[3] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node5", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node5' parameter\n");
        goto error;
    }
    conf->nodes[4] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node6", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node1' parameter\n");
        goto error;
    }
    conf->nodes[5] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node7", node, 32) != 0) {
        fprintf(stderr, "ERROR: Can't read 'node7' parameter\n");
        goto error;
    }
    conf->nodes[6] = lamb_strdup(node);

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
