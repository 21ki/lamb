
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
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
#include <syslog.h>
#include <cmpp.h>
#include "config.h"
#include "queue.h"
#include "socket.h"
#include "message.h"
#include "gateway.h"
#include "log.h"
#include "sp.h"

static int gid;
static int scheduler;
static int delivery;
static lamb_db_t *db;
static cmpp_sp_t cmpp;
static lamb_cache_t *rdb;
static lamb_caches_t cache;
static lamb_list_t *storage;
static lamb_config_t config;
static lamb_gateway_t *gateway;
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
    char *optstring = "a:c:d";
    opt = getopt(argc, argv, optstring);

    while (opt != -1) {
        switch (opt) {
        case 'a':
            gid = atoi(optarg);
            break;
        case 'c':
            file = optarg;
            break;
        case 'd':
            background = true;
            break;
        }
        opt = getopt(argc, argv, optstring);
    }

    if (gid < 1) {
        fprintf(stderr, "Invalid gateway id number\n");
        return -1;
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

    /* Logger initialization*/
    lamb_log_init("lamb-gateway");

    /* Check lock protection */
    int lock;
    char lockfile[128];

    snprintf(lockfile, sizeof(lockfile), "/tmp/gtw-%d.lock", gid);

    if (lamb_lock_protection(&lock, lockfile)) {
        syslog(LOG_ERR, "Already started, please do not repeat the start");
        return -1;
    }

    /* Save pid to file */
    lamb_pid_file(lock, getpid());

    /* Signal event processing */
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Setting process information */
    lamb_set_process("lamb-gateway");

    /* Start main event processing */
    lamb_event_loop();

    /* Release lock protection */
    lamb_lock_release(&lock);

    return 0;
}

void lamb_event_loop(void) {
    int err;

    total = 0;
    heartbeat.count = 0;

    memset(&status, 0, sizeof(status));

    /* Storage Initialization */
    storage = lamb_list_new();
    if (!storage) {
        syslog(LOG_ERR, "storage queue initialization failed");
        return;
    }

    statistical = (lamb_statistical_t *)calloc(1, sizeof(lamb_statistical_t));
    if (!statistical) {
        syslog(LOG_ERR, "The kernel can't allocate memory");
        return;
    }

    statistical->gid = gid;
    pthread_mutex_init(&statistical->lock, NULL);

    /* Redis initialization */
    rdb = (lamb_cache_t *)malloc(sizeof(lamb_cache_t));
    if (!rdb) {
        syslog(LOG_ERR, "redis database initialize failed");
        return;
    }

    err = lamb_cache_connect(rdb, config.redis_host, config.redis_port, NULL,
                             config.redis_db);
    if (err) {
        syslog(LOG_ERR, "can't connect to redis %s", config.redis_host);
        return;
    }

    /* Database initialization */
    db = (lamb_db_t *)malloc(sizeof(lamb_db_t));
    if (!db) {
        syslog(LOG_ERR, "The kernel can't allocate memory");
        return;
    }

    err = lamb_db_init(db);
    if (err) {
        syslog(LOG_ERR, "database handle initialize failed");
        return;
    }

    /* Connect to database */
    err = lamb_db_connect(db, config.db_host, config.db_port, config.db_user,
                          config.db_password, config.db_name);
    if (err) {
        syslog(LOG_ERR, "can't connect to database %s", config.db_host);
        return;
    }

    lamb_debug("connect to database successfull\n");

    /* fetch gateway information */
    gateway = (lamb_gateway_t *)calloc(1, sizeof(lamb_gateway_t));
    if (!gateway) {
        syslog(LOG_ERR, "The kernel can't allocate memory");
        return;
    }

    err = lamb_get_gateway(db, gid, gateway);
    if (err) {
        syslog(LOG_ERR, "fetch %d gateway information failure", gid);
        return;
    }
    
    /* Cache cluster initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7, 4);
    if (cache.len != 7) {
        syslog(LOG_ERR, "connect to cache cluster failed");
        return;
    }

    lamb_debug("connect to cache cluster successfull\n");

    /* Connect to scheduler server */
    scheduler = lamb_nn_reqrep(config.scheduler, gid, config.timeout);
    if (scheduler < 0) {
        syslog(LOG_ERR, "can't connect to scheduler %s", config.scheduler);
        return;
    }

    lamb_debug("connect to scheduler server successfull\n");

    /* Connect to delivery server */
    delivery = lamb_nn_pair(config.delivery, gid, config.timeout);
    if (delivery < 0) {
        syslog(LOG_ERR, "can't connect to delivery %s", config.delivery);
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

    /* Start Work Thread */
    lamb_start_thread(lamb_work_loop, NULL, 1);

    /* Start Deliver Thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);

    /* Start Keepalive Thread */
    lamb_start_thread(lamb_cmpp_keepalive, NULL, 1);

    /* Start Status Update Thread */
    lamb_start_thread(lamb_stat_loop, NULL, 1);

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
    msgFmt = gateway->encoding;

    if (msgFmt == 0) {
        tocode = "ASCII";
    } else if (msgFmt == 8) {
        tocode = "UCS-2BE";
    } else if (msgFmt == 11) {
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
            syslog(LOG_ERR, "only submit packets are allowed");
            continue;
        }

        message = submit__unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!message) {
            syslog(LOG_ERR, "can't unpack for submit message packets");
            continue;
        }

        /* Caching message information */
        confirmed.id = message->id;
        strncpy(confirmed.spcode, message->spcode, 20);
        confirmed.account = message->account;
        confirmed.company = message->company;

        /* Spcode processing */
        if (gateway->extended) {
            snprintf(spcode, sizeof(spcode), "%s%s", gateway->spcode, message->spcode);
        } else {
            strcpy(spcode, gateway->spcode);
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
        err = cmpp_submit(&cmpp.sock, sequenceId, gateway->spid, spcode, message->phone,
                          content, length, msgFmt, NULL, true);
        submit__free_unpacked(message, NULL);

        /* Submit count statistical  */
        total++;
        status.sub++;
        pthread_mutex_lock(&statistical->lock);
        statistical->submit++;
        pthread_mutex_unlock(&statistical->lock);

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
        if (gateway->concurrent > 1000) {
            delayed = 1000;
        } else {
            delayed = 1000 / gateway->concurrent;
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

        /* Wait for the message to come */
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
                } else if (strncasecmp(stat, "EXPIRED", 7) == 0) {
                    report->status = 2;
                } else if (strncasecmp(stat, "DELETED", 7) == 0) {
                    report->status = 3;
                } else if (strncasecmp(stat, "UNDELIV", 7) == 0) {
                    report->status = 4;
                } else if (strncasecmp(stat, "ACCEPTD", 7) == 0) {
                    report->status = 5;
                } else if (strncasecmp(stat, "UNKNOWN", 7) == 0) {
                    report->status = 6;
                } else if (strncasecmp(stat, "REJECTD", 7) == 0) {
                    report->status = 7;
                } else {
                    report->status = 6;
                }

                report->type = LAMB_REPORT;
                lamb_list_rpush(storage, lamb_node_new(report));

                lamb_debug("receive msgId: %llu, phone: %s, stat: %s, submitTime: %s, doneTime: %s\n",
                           report->id, report->phone, stat, report->submittime, report->donetime);

            response1:
                cmpp_deliver_resp(&cmpp.sock, sequenceId, report->id, result);
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
            syslog(LOG_ERR, "sending keepalive packet to gateway %s failed", gateway->host);
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

    int slen = strlen(gateway->spcode);
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

            /* Gateway state statistics */
            pthread_mutex_lock(&statistical->lock);
            lamb_check_statistical(r->status, statistical);
            pthread_mutex_unlock(&statistical->lock);

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
    syslog(LOG_ERR, "the connecting to gateway %s ...", gateway->host);
    while (lamb_cmpp_init(cmpp, config) != 0) {
        lamb_sleep(config->interval * 1000);
    }
    syslog(LOG_ERR, "connect to gateway %s successfull", gateway->host);
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
    err = cmpp_init_sp(cmpp, gateway->host, gateway->port);
    if (err) {
        syslog(LOG_ERR, "can't connect to server %s", gateway->host);
        return 1;
    }

    /* login to cmpp gateway */
    sequenceId = cmpp_sequence();
    err = cmpp_connect(&cmpp->sock, sequenceId, gateway->username, gateway->password);
    if (err) {
        syslog(LOG_ERR, "can't connect to gateway %s", gateway->host);
        return 2;
    }

    err = cmpp_recv_timeout(&cmpp->sock, &pack, sizeof(pack), config->recv_timeout);
    if (err) {
        syslog(LOG_ERR, "receive gateway response timeout from %s", gateway->host);
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
            syslog(LOG_ERR, "Incorrect protocol packets");
            break;
        case 2:
            syslog(LOG_ERR, "Illegal source address");
            break;
        case 3:
            syslog(LOG_ERR, "Authenticator failed");
            break;
        case 4:
            syslog(LOG_ERR, "Protocol version is too high");
            break;
        default:
            syslog(LOG_ERR, "Unknown error, code: %d", status);
            break;
        }

        return 4;
    } else {
        syslog(LOG_ERR, "Incorrect response packet from %s", gateway->host);
        return 5;
    }
    
success:
    return 0;
}

void *lamb_stat_loop(void *data) {
    int err;
    lamb_statistical_t curr;
    unsigned long long error;
    redisReply *reply = NULL;

    curr.gid = gid;

    while (true) {
        error = status.err + status.timeo;
        reply = redisCommand(rdb->handle, "HMSET gateway.%d pid %u status %d speed %llu error %llu",
                             gid, getpid(), cmpp.ok ? 1 : 0, (unsigned long long)(total / 5), error);
        total = 0;

        if (reply != NULL) {
            freeReplyObject(reply);
        } else {
            syslog(LOG_ERR, "redis command executes errors");
        }

        pthread_mutex_lock(&statistical->lock);
        memcpy(&curr, statistical, sizeof(lamb_statistical_t));
        lamb_clean_statistical(statistical);
        pthread_mutex_unlock(&statistical->lock);

        /* Write data statistical */
        if (curr.submit > 0 || curr.delivrd > 0 || curr.expired > 0 || curr.deleted > 0 ||
            curr.undeliv > 0 || curr.acceptd > 0 || curr.unknown || curr.rejectd > 0) {
            err = lamb_write_statistical(db, &curr);
            
            if (err) {
                syslog(LOG_ERR, "can't write data statistical to database");
            }
        }

#ifdef _DEBUG
        printf("queue: %u, sub: %llu, ack: %llu, rep: %llu, delv: %llu, timeo: %llu, err: %llu\n",
               storage->len, status.sub, status.ack, status.rep, status.delv, status.timeo, status.err);
#endif

        sleep(5);
    }

    pthread_exit(NULL);
}

void lamb_clean_statistical(lamb_statistical_t *stat) {
    if (stat) {
        stat->submit = 0;
        stat->delivrd = 0;
        stat->expired = 0;
        stat->deleted = 0;
        stat->undeliv = 0;
        stat->acceptd = 0;
        stat->unknown = 0;
        stat->rejectd = 0;
    }

    return;
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

void lamb_check_statistical(int status, lamb_statistical_t *stat) {
    switch (status) {
    case 1:
        stat->delivrd++;
        break;
    case 2:
        stat->expired++;
        break;
    case 3:
        stat->deleted++;
        break;
    case 4:
        stat->undeliv++;
        break;
    case 5:
        stat->acceptd++;
        break;
    case 6:
        stat->unknown++;
        break;
    case 7:
        stat->rejectd++;
        break;
    }

    return;
}

int lamb_read_config(lamb_config_t *conf, const char *file) {
    if (!conf) {
        return -1;
    }

    config_t cfg;
    if (lamb_read_file(&cfg, file) != 0) {
        fprintf(stderr, "Can't open the %s configuration file\n", file);
        goto error;
    }

    /* Id */
    if (lamb_get_int(&cfg, "Id", &conf->id) != 0) {
        fprintf(stderr, "Can't read config 'Id' parameter\n");
        goto error;
    }

    /* Debug */
    if (lamb_get_bool(&cfg, "Debug", &conf->debug) != 0) {
        fprintf(stderr, "Can't read config 'Debug' parameter\n");
        goto error;
    }

    /* Timeout */
    if (lamb_get_int(&cfg, "Timeout", (int *)&conf->timeout) != 0) {
        fprintf(stderr, "Can't read config 'Timeout' parameter\n");
        goto error;
    }

    /* Retry */
    if (lamb_get_int(&cfg, "Retry", &conf->retry) != 0) {
        fprintf(stderr, "Can't read config 'Retry' parameter\n");
        goto error;
    }

    /* Interval */
    if (lamb_get_int(&cfg, "Interval", &conf->interval) != 0) {
        fprintf(stderr, "Can't read config 'Interval' parameter\n");
        goto error;
    }

    /* SendTimeout */
    if (lamb_get_int(&cfg, "SendTimeout", (int *)&conf->send_timeout) != 0) {
        fprintf(stderr, "Can't read config 'SendTimeout' parameter\n");
        goto error;
    }

    /* RecvTimeout */
    if (lamb_get_int(&cfg, "RecvTimeout", (int *)&conf->recv_timeout) != 0) {
        fprintf(stderr, "Can't read config 'RecvTimeout' parameter\n");
        goto error;
    }

    /* AcknowledgeTimeout */
    if (lamb_get_int(&cfg, "AcknowledgeTimeout", (int *)&conf->acknowledge_timeout) != 0) {
        fprintf(stderr, "Can't read config 'AcknowledgeTimeout' parameter\n");
        goto error;
    }

    /* RedisHost */
    if (lamb_get_string(&cfg, "RedisHost", conf->redis_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'RedisHost' parameter\n");
        goto error;
    }

    /* RedisPort */
    if (lamb_get_int(&cfg, "RedisPort", &conf->redis_port) != 0) {
        fprintf(stderr, "Can't read config 'RedisPort' parameter\n");
        goto error;
    }

    /* Check port validity */
    if (conf->redis_port < 1 || conf->redis_port > 65535) {
        fprintf(stderr, "Invalid redis port number\n");
        goto error;
    }

    /* RedisPassword */
    if (lamb_get_string(&cfg, "RedisPassword", conf->redis_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'RedisPassword' parameter\n");
        goto error;
    }

    /* RedisDb */
    if (lamb_get_int(&cfg, "RedisDb", &conf->redis_db) != 0) {
        fprintf(stderr, "Can't read config 'RedisDb' parameter\n");
        goto error;
    }

    /* LogFile */
    if (lamb_get_string(&cfg, "LogFile", conf->logfile, 128) != 0) {
        fprintf(stderr, "Can't read config 'LogFile' parameter\n");
        goto error;
    }

    /* Ac */
    if (lamb_get_string(&cfg, "Ac", conf->ac, 128) != 0) {
        fprintf(stderr, "Can't read config 'Ac' parameter\n");
    }

    /* Scheduler */
    if (lamb_get_string(&cfg, "Scheduler", conf->scheduler, 128) != 0) {
        fprintf(stderr, "Invalid Scheduler server address\n");
        goto error;
    }

    /* Delivery */
    if (lamb_get_string(&cfg, "Delivery", conf->delivery, 128) != 0) {
        fprintf(stderr, "Invalid Delivery server address\n");
        goto error;
    }

    /* DbHost */
    if (lamb_get_string(&cfg, "DbHost", conf->db_host, 16) != 0) {
        fprintf(stderr, "Can't read config 'DbHost' parameter\n");
        goto error;
    }

    /* DbPort */
    if (lamb_get_int(&cfg, "DbPort", &conf->db_port) != 0) {
        fprintf(stderr, "Can't read config 'DbPort' parameter\n");
        goto error;
    }

    /* Check port validity */
    if (conf->db_port < 1 || conf->db_port > 65535) {
        fprintf(stderr, "Invalid database port number\n");
        goto error;
    }

    /* DbUser */
    if (lamb_get_string(&cfg, "DbUser", conf->db_user, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbUser' parameter\n");
        goto error;
    }

    /* DbPassword */
    if (lamb_get_string(&cfg, "DbPassword", conf->db_password, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbPassword' parameter\n");
        goto error;
    }

    /* DbName */
    if (lamb_get_string(&cfg, "DbName", conf->db_name, 64) != 0) {
        fprintf(stderr, "Can't read config 'DbName' parameter\n");
        goto error;
    }

    char node[32];
    memset(node, 0, sizeof(node));

    /* Cache nodes */
    if (lamb_get_string(&cfg, "node1", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node1' parameter\n");
        goto error;
    }
    conf->nodes[0] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node2", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node2' parameter\n");
        goto error;
    }
    conf->nodes[1] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node3", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node3' parameter\n");
        goto error;
    }
    conf->nodes[2] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node4", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node4' parameter\n");
        goto error;
    }
    conf->nodes[3] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node5", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node5' parameter\n");
        goto error;
    }
    conf->nodes[4] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node6", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node1' parameter\n");
        goto error;
    }
    conf->nodes[5] = lamb_strdup(node);

    if (lamb_get_string(&cfg, "node7", node, 32) != 0) {
        fprintf(stderr, "Can't read config 'node7' parameter\n");
        goto error;
    }
    conf->nodes[6] = lamb_strdup(node);

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
