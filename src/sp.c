
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
#include "mqueue.h"
#include "queue.h"
#include "socket.h"
#include "message.h"
#include "sp.h"

static int mt;
static int mo;
static cmpp_sp_t cmpp;
//static lamb_cache_t rdb;
static lamb_caches_t cache;
static lamb_list_t *storage;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_heartbeat_t heartbeat;
static lamb_confirmed_t confirmed;
static unsigned long long total;
static lamb_status_t status;

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

    /* Cache Cluster Initialization */
    memset(&cache, 0, sizeof(cache));
    lamb_nodes_connect(&cache, LAMB_MAX_CACHE, config.nodes, 7, 4);
    if (cache.len != 7) {
        lamb_log(LOG_ERR, "connect to cache cluster server failed");
        return;
    }

    lamb_debug("connect to cache cluster server successfull\n");

    /* Connect to MT Server */
    mt = lamb_nn_reqrep(config.mt, config.id, config.timeout);
    if (mt < 0) {
        lamb_log(LOG_ERR, "can't connect to mt %s", config.mt);
        return;
    }

    lamb_debug("connect to mt server successfull\n");

    /* Connect to MO Server */
    mo = lamb_nn_pair(config.mo, config.id, config.timeout);
    if (mo < 0) {
        lamb_log(LOG_ERR, "can't connect to mo %s", config.mo);
        return;
    }

    lamb_debug("connect to mo server successfull\n");

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

        rc = nn_send(mt, req, len, NN_DONTWAIT);

        if (rc != len) {
            lamb_sleep(1000);
            continue;
        }

        rc = nn_recv(mt, &buf, NN_MSG, 0);

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

        message = lamb_submit_unpack(NULL, rc - HEAD, (uint8_t *)(buf + HEAD));
        nn_freemsg(buf);

        if (!message) {
            lamb_log(LOG_ERR, "can't unpack for submit message packets");
            continue;
        }

        /* Caching message information */
        confirmed.id = message->id;
        strcpy(confirmed.spcode, message->spcode);
        confirmed.account = message->account;
        confirmed.company = message->company;

        /* Spcode Processing */
        if (config.extended) {
            sprintf(spcode, "%s%s", config.spcode, message->spcode);
        } else {
            strcpy(spcode, config.spcode);
        }

        /* Message Encode Convert */
        memset(content, 0, sizeof(content));
        err = lamb_encoded_convert((char *)message->content.data, message->length, content,
                                   sizeof(content), "UTF-8", tocode, &length);

        if (err || (length == 0)) {
            continue;
        }

        sequenceId = confirmed.sequenceId = cmpp_sequence();
        err = cmpp_submit(&cmpp.sock, sequenceId, config.spid, spcode, message->phone,
                          content, length, msgFmt, true);
        total++;
        status.sub++;
            
        if (err) {
            status.err++;
            lamb_sleep(config.interval * 1000);
            continue;
        }

        err = lamb_wait_confirmation(&cond, &mutex, config.acknowledge_timeout);

        if (err == ETIMEDOUT) {
            status.timeo++;
            lamb_sleep(config.interval * 1000);
        }

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
            /* Cmpp Submit Resp */
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
                memset(stat, 0, sizeof(stat));
                report = (lamb_report_t *)calloc(1, sizeof(lamb_report_t));

                if (!report) {
                    result = 9;
                    goto response1;
                }

                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content_msg_id, &report->id, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, report->phone, 24, 20);

                /* Stat */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_stat, stat, 8, 7);

                /* Submit_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_submit_time, report->submittime, 16, 10);

                /* Done_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_done_time, report->donetime, 16, 10);

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
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, deliver->phone, 24, 21);

                /* Dest_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_dest_id, deliver->spcode, 24, 21);

                /* Service_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_service_id, deliver->serviceid, 16, 10);

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
                           deliver->id, deliver->phone, deliver->spcode, deliver->msgfmt, deliver->length);

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
    char spcode[24];
    lamb_report_t *r;
    lamb_deliver_t *d;
    lamb_node_t *node;
    unsigned long long msgId;

    int slen = strlen(config.spcode);
    Report report = LAMB_REPORT_INIT;
    Deliver deliver = LAMB_DELIVER_INIT;

    while (true) {
        node = lamb_list_lpop(storage);

        if (!node) {
            lamb_sleep(10);
            continue;
        }

        message = node->val;

        if (CHECK_TYPE(message) == LAMB_REPORT) {
            r = (lamb_report_t *)message;
            msgId = r->id;
            memset(spcode, 0, sizeof(spcode));
            lamb_get_cache(&cache, &r->id, &r->account, &r->company, spcode, sizeof(spcode));

            if (r->id > 0 && r->account > 0 && r->company > 0) {
                lamb_del_cache(&cache, msgId);
            } else {
                goto done;
            }

            report.id = r->id;
            report.account = r->account;
            report.company = r->company;
            report.spcode = spcode;
            report.phone = r->phone;
            report.status = r->status;
            report.submittime = r->submittime;
            report.donetime = r->donetime;

            len = lamb_report_get_packed_size(&report);
            pk = malloc(len);

            if (!pk) {
                goto done;
            }

            lamb_report_pack(&report, pk);
            len = lamb_pack_assembly(&buf, LAMB_REPORT, pk, len);

            if (len > 0) {
                if (nn_send(mo, buf, len, 0) != len) {
                    lamb_save_logfile(config.backfile, message);
                }
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

            len = lamb_deliver_get_packed_size(&deliver);
            pk = malloc(len);

            if (!pk) {
                goto done;
            }

            lamb_deliver_pack(&deliver, pk);
            len = lamb_pack_assembly(&buf, LAMB_DELIVER, pk, len);

            if (len > 0) {
                if (nn_send(mo, buf, len, 0) != len) {
                    lamb_save_logfile(config.backfile, message);
                }
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
    lamb_log(LOG_ERR, "Reconnecting to gateway %s ...", config->host);
    while (lamb_cmpp_init(cmpp, config) != 0) {
        lamb_sleep(config->interval * 1000);
    }
    lamb_log(LOG_ERR, "Connect to gateway %s successfull", config->host);
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
        lamb_log(LOG_ERR, "Can't connect to gateway %s server", config->host);
        return 1;
    }

    /* login to cmpp gateway */
    sequenceId = cmpp_sequence();
    err = cmpp_connect(&cmpp->sock, sequenceId, config->user, config->password);
    if (err) {
        lamb_log(LOG_ERR, "Sending connection request to %s failed", config->host);
        return 2;
    }

    err = cmpp_recv_timeout(&cmpp->sock, &pack, sizeof(pack), config->recv_timeout);
    if (err) {
        lamb_log(LOG_ERR, "Receive gateway response packet from %s failed", config->host);
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
            lamb_log(LOG_ERR, "Incorrect protocol packets");
            break;
        case 2:
            lamb_log(LOG_ERR, "Illegal source address");
            break;
        case 3:
            lamb_log(LOG_ERR, "Authenticator failed");
            break;
        case 4:
            lamb_log(LOG_ERR, "Protocol version is too high");
            break;
        default:
            lamb_log(LOG_ERR, "Cmpp unknown error code: %d", status);
            break;
        }

        return 4;
    } else {
        lamb_log(LOG_ERR, "Incorrect response packet from %s gateway", config->host);
        return 5;
    }
    
success:
    return 0;
}

int lamb_save_logfile(char *file, void *data) {
    FILE *fp;
    
    fp = fopen(file, "a");
    if (!fp) {
        return -1;
    }

    lamb_report_t *report;
    lamb_deliver_t *deliver;

    switch (CHECK_TYPE(data)) {
    case LAMB_REPORT:
        report = (lamb_report_t *)data;
        fprintf(fp, "%d,%llu,%s,%d,%s,%s\n", LAMB_REPORT, report->id, report->phone, report->status,
                report->submittime, report->donetime);
        break;
    case LAMB_DELIVER:
        deliver = (lamb_deliver_t *)data;
        fprintf(fp, "%d,%llu,%s,%s,%s,%d,%d,%s\n", LAMB_DELIVER, deliver->id, deliver->phone,
                deliver->spcode, deliver->serviceid, deliver->msgfmt, deliver->length,
                deliver->content);
        break;
    }

    fclose(fp);

    return 0;
}

void *lamb_stat_loop(void *data) {
    //unsigned long long last, error;

    //last = 0;

    while (true) {
        /* 
           error = status.err + status.timeo;
           reply = redisCommand(rdb.handle, "HMSET gateway.%d pid %u status %d speed %llu "
           "error %llu", config.id, getpid(), cmpp.ok ? 1 : 0,
           (unsigned long long)((total - last) / 5), error);
           if (reply != NULL) {
           freeReplyObject(reply);
           } else {
           lamb_log(LOG_ERR, "lamb exec redis command error");
           }
        */
        lamb_debug("queue: %u, sub: %llu, ack: %llu, rep: %llu, delv: %llu, timeo: %llu"
                   ", err: %llu\n", storage->len, status.sub, status.ack, status.rep, status.delv,
                   status.timeo, status.err);

        total = 0;
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

    if (reply == NULL) {
        return -1;
    }

    freeReplyObject(reply);
    return 0;
}

int lamb_get_cache(lamb_caches_t *caches, unsigned long long *id, int *account, int *company, char *spcode, size_t size) {
    int i;
    redisReply *reply = NULL;

    i = (*id % caches->len);

    pthread_mutex_lock(&caches->nodes[i]->lock);
    reply = redisCommand(caches->nodes[i]->handle, "HMGET %llu id account company spcode", *id);
    pthread_mutex_unlock(&caches->nodes[i]->lock);
    
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            if (reply->elements == 4) {
                *id = (reply->element[0]->len > 0) ? strtoull(reply->element[0]->str, NULL, 10) : 0;
                *account = (reply->element[1]->len > 0) ? atoi(reply->element[1]->str) : 0;
                *company = (reply->element[2]->len > 0) ? atoi(reply->element[2]->str) : 0;
                if (reply->element[3]->len >= size) {
                    memcpy(spcode, reply->element[3]->str, size - 1);
                } else {
                    memcpy(spcode, reply->element[3]->str, reply->element[3]->len);
                }
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
    
    if (reply == NULL) {
        return -1;
    }

    freeReplyObject(reply);
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

    if (lamb_get_string(&cfg, "Mt", conf->mt, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid Mt server address\n");
        goto error;
    }

    if (lamb_get_string(&cfg, "Mo", conf->mo, 128) != 0) {
        fprintf(stderr, "ERROR: Invalid Mo server address\n");
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
