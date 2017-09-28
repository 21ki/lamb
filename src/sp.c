
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
#include <cmpp.h>
#include "sp.h"
#include "config.h"
#include "list.h"
#include "queue.h"
#include "cache.h"
#include "leveldb.h"

static cmpp_sp_t cmpp;
static lamb_cache_t rdb;
static lamb_leveldb_t cache;
static lamb_config_t config;
static lamb_list_t *recovery;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_queue_t send_queue;
static lamb_queue_t recv_queue;
static lamb_heartbeat_t heartbeat;
static lamb_confirmed_t confirmed;

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
    lamb_signal_processing();

    /* Resource limit processing */
    lamb_rlimit_processing();

    /* Start main event processing */
    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    lamb_set_process("lamb-gateway");

    /* Storage Queue Initialization */
    recovery = lamb_list_new();
    if (recovery == NULL) {
        lamb_errlog(config.logfile, "Lamb recovery queue initialization failed ", 0);
        return;
    }
    
    /* Cache Initialization */
    err = lamb_level_init(&cache, config.cache);
    if (err) {
        lamb_errlog(config.logfile, "Can't open leveldb cache database", 0);
        return;
    }

    /* Redis Initialization */
    err = lamb_cache_connect(&rdb, config.redis_host, config.redis_port, NULL, config.redis_db);
    if (err) {
        lamb_errlog(config.logfile, "Can't connect to redis server", 0);
        return;
    }

    /* Message queue initialization */
    char name[64];
    lamb_queue_opt sopt, ropt;
    struct mq_attr sattr, rattr;

    sopt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    sattr.mq_maxmsg = 8;
    sattr.mq_msgsize = sizeof(lamb_message_t);
    sopt.attr = &sattr;
    
    sprintf(name, "/gw.%d.message", config.id);
    err = lamb_queue_open(&send_queue, name, &sopt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s queue", name);
        return;
    }

    ropt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    rattr.mq_maxmsg = 65535;
    rattr.mq_msgsize = sizeof(lamb_message_t);
    ropt.attr = &rattr;
    
    sprintf(name, "/gw.%d.deliver", config.id);
    err = lamb_queue_open(&recv_queue, name, &ropt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s queue", name);
        return;
    }
    
    /* Cmpp client initialization */
    err = lamb_cmpp_init(&cmpp, &config);
    if (err) {
        return;
    }

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    
    /* Start Sender Thread */
    lamb_start_thread(lamb_sender_loop, NULL, 1);

    /* Start Deliver Thread */
    lamb_start_thread(lamb_deliver_loop, NULL, 1);
    
    /* Start Keepalive Thread */
    heartbeat.count = 0;
    lamb_start_thread(lamb_cmpp_keepalive, NULL, 1);

    /* Start Status Update Thread */
    lamb_start_thread(lamb_online_update, NULL, 1);

    /* Start Status Update Thread */
    lamb_start_thread(lamb_recovery_loop, NULL, 1);

    while (true) {
        lamb_sleep(3000);
    }
}

void *lamb_sender_loop(void *data) {
    int err;
    int msgFmt;
    ssize_t ret;
    char spcode[21];
    lamb_submit_t *submit;
    lamb_message_t message;
    unsigned int sequenceId;

    memset(spcode, 0, sizeof(spcode));
    
    if (strcasecmp(config.encoding, "ASCII") == 0) {
        msgFmt = 0;
    } else if (strcasecmp(config.encoding, "UCS-2") == 0) {
        msgFmt = 8;
    } else if (strcasecmp(config.encoding, "UTF-8") == 0) {
        msgFmt = 11;
    } else if (strcasecmp(config.encoding, "GBK") == 0) {
        msgFmt = 15;
    } else {
        msgFmt = 0;
    }

    int count = 0;
    unsigned long long start_time;
    unsigned long long next_summary_time;

    start_time = lamb_now_microsecond();
    next_summary_time = start_time + 1000000;
    
    while (true) {
        if (!cmpp.ok) {
            count = 0;
            lamb_sleep(1000);
            start_time = lamb_now_microsecond();
            continue;
        }

        ret = lamb_queue_receive(&send_queue, (char *)&message, sizeof(message), 0);
        if (ret > 0) {
            unsigned long long now_time;
            submit = (lamb_submit_t *)&(message.data);

            confirmed.id = submit->id;

            if (config.extended) {
                sprintf(spcode, "%s%s", config.spcode, submit->spcode);
            } else {
                strcpy(spcode, config.spcode);
            }
            
            now_time = lamb_now_microsecond();

            /* Message Encode Convert */
            int length;
            char content[256];
            memset(content, 0, sizeof(content));
            err = lamb_encoded_convert(submit->content, submit->length, content, sizeof(content), "UTF-8", config.encoding, &length);
            if (err || (length == 0)) {
                printf("-> [encoded] Message %llu encoding conversion failed\n", submit->id);
                continue;
            }
            printf("-> [encoded] Message %llu encoding conversion OK\n", submit->id);

            sequenceId = confirmed.sequenceId = cmpp_sequence();
            err = cmpp_submit(&cmpp.sock, sequenceId, config.spid, spcode, submit->phone, content, length, msgFmt, true);

            if (err) {
                printf("-> [sending] Message %llu phone: %s, msgFmt: %d, length: %d to %s failed\n", submit->id, submit->phone, msgFmt, length, config.host);
                lamb_sleep(config.interval * 1000);
                continue;
            }

            printf("-> [sending] Message %llu phone: %s, msgFmt: %d, length: %d to %s OK\n", submit->id, submit->phone, msgFmt, length, config.host);

            count++;

            err = lamb_wait_confirmation(&cond, &mutex, config.timeout);
            
            if (err == ETIMEDOUT) {
                printf("-> [error] wait message %llu confirmation timeout", submit->id);
                lamb_errlog(config.logfile, "Wait submit message confirmation timeout", 0);
                lamb_sleep(config.interval * 1000);
            }

            /* Flow Monitoring And Control */
            lamb_flow_limit(&start_time, &now_time, &next_summary_time, &count, config.concurrent);
        } else {
            lamb_sleep(10);
        }
    }

    pthread_exit(NULL);

}

void *lamb_deliver_loop(void *data) {
    int err;
    char *tmp;
    size_t len;
    char status[8];
    cmpp_pack_t pack;
    cmpp_head_t *chp;
    lamb_list_node_t *node;
    char key[64], val[64];
    lamb_report_t *report;
    lamb_deliver_t *deliver;
    lamb_message_t message;
    unsigned int commandId;
    unsigned int sequenceId;
    unsigned long long msgId;

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

        memset(key, 0, sizeof(key));
        memset(val, 0, sizeof(val));
        chp = (cmpp_head_t *)&pack;
        commandId = ntohl(chp->commandId);
        sequenceId = ntohl(chp->sequenceId);
        memset(&message, 0, sizeof(message));
        
        switch (commandId) {
        case CMPP_SUBMIT_RESP:;
            /* Cmpp Submit Resp */
            int result = 0;
            unsigned long long id;

            id = confirmed.id;
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_msg_id, &msgId, 8);
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_result, &result, 1);

            if ((confirmed.sequenceId != sequenceId) || (result != 0)) {
                break;
            }

            pthread_cond_signal(&cond);
            sprintf(key, "%llu", msgId);
            sprintf(val, "%llu", id);
            lamb_level_put(&cache, key, strlen(key), val, strlen(val));

            printf("-> [received] message response id: %llu, msgId: %llu\n", id, msgId);

            break;
        case CMPP_DELIVER:;
            /* Cmpp Deliver */
            unsigned char registered_delivery;
            cmpp_pack_get_integer(&pack, cmpp_deliver_registered_delivery, &registered_delivery, 1);
            if (registered_delivery == 1) {
                message.type = LAMB_REPORT;
                report = (lamb_report_t *)&(message.data);
                memset(status, 0, sizeof(status));

                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content_msg_id, &msgId, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, report->phone, 24, 20);

                /* Stat */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_stat, status, 8, 7);

                /* Submit_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_submit_time, report->submitTime, 16, 10);

                /* Done_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_done_time, report->doneTime, 16, 10);

                sprintf(key, "%llu", msgId);
                tmp = lamb_level_get(&cache, key, strlen(key), &len);

                printf("-> tmp : %s\n", tmp ? tmp : "null");
                
                if (tmp != NULL) {
                    report->id = strtoull(tmp, NULL, 10);
                    free(tmp);
                    tmp = NULL;
                    
                    if (strncasecmp(status, "DELIVRD", 7) == 0) {
                        report->status = 1;
                    } else if (strncasecmp(status, "EXPIRED", 7) == 0) {
                        report->status = 2;
                    } else if (strncasecmp(status, "DELETED", 7) == 0) {
                        report->status = 3;
                    } else if (strncasecmp(status, "UNDELIV", 7) == 0) {
                        report->status = 4;
                    } else if (strncasecmp(status, "ACCEPTD", 7) == 0) {
                        report->status = 5;
                    } else if (strncasecmp(status, "UNKNOWN", 7) == 0) {
                        report->status = 6;
                    } else if (strncasecmp(status, "REJECTD", 7) == 0) {
                        report->status = 7;
                    } else {
                        report->status = 6;
                    }

                    err = lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0);
                    if (err) {
                        lamb_save_logfile(config.backfile, &message);
                    }

                    tmp = lamb_strdup(key);
                    pthread_mutex_lock(&recovery->lock);
                    node = lamb_list_rpush(recovery, lamb_list_node_new(tmp));
                    pthread_mutex_unlock(&recovery->lock);

                    if (!node) {
                        lamb_errlog(config.logfile, "Can't memory allocated for recovery queue", 0);
                        lamb_sleep(3000);
                    }
                } else {
                    printf("-> [leveldb] No %s message record found\n", key);
                }

                printf("-> [report] id:%llu, msgId: %llu, phone: %s, status: %s, submitTime: %s, doneTime: %s\n",
                       msgId, report->id, report->phone, status, report->submitTime, report->doneTime);

                cmpp_deliver_resp(&cmpp.sock, sequenceId, msgId, 0);
            } else {
                message.type = LAMB_DELIVER;
                deliver = (lamb_deliver_t *)&(message.data);
                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_id, &deliver->id, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, deliver->phone, 24, 21);

                /* Dest_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_dest_id, deliver->spcode, 24, 21);

                /* Service_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_service_id, deliver->serviceId, 16, 10);

                /* Msg_Fmt */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_fmt, &deliver->msgFmt, 1);

                /* Msg_Length */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_length, &deliver->length, 1);

                /* Msg_Content */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content, deliver->content, 160, deliver->length);

                printf("-> [deliver] msgId: %llu, phone: %s, spcode: %s, serviceId: %s, msgFmt: %d, length: %d\n",
                       deliver->id, deliver->phone, deliver->spcode, deliver->serviceId, deliver->msgFmt, deliver->length);

                err = lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0);
                if (err) {
                    lamb_save_logfile(config.backfile, &message);
                }

                cmpp_deliver_resp(&cmpp.sock, sequenceId, deliver->id, 0);
            }
            break;
        case CMPP_ACTIVE_TEST_RESP:
            if (heartbeat.sequenceId == sequenceId) {
                heartbeat.count = 0;
                printf("-> [active] Receive sequenceId: %u keepalive response\n", sequenceId);
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
        printf("-> [active] sending sequenceId: %u keepalive request\n", sequenceId);
        
        err = cmpp_active_test(&cmpp.sock, sequenceId);
        if (err) {
            lamb_errlog(config.logfile, "sending 'cmpp_active_test' to %s gateway failed", config.host);
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

void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config) {
    lamb_errlog(config->logfile, "Reconnecting to gateway %s ...", config->host);
    while (lamb_cmpp_init(cmpp, config) != 0) {
        lamb_sleep(config->interval * 1000);
    }
    lamb_errlog(config->logfile, "Connect to gateway %s successfull", config->host);
    return;
}

int lamb_cmpp_init(cmpp_sp_t *cmpp, lamb_config_t *config) {
    int err;
    cmpp_head_t *chp;
    cmpp_pack_t pack;
    unsigned int sequenceId;
    unsigned int responseId;

    /* setting cmpp socket parameter */
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_CONTIMEOUT, config->timeout * 1000);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_SENDTIMEOUT, config->send_timeout);
    cmpp_sock_setting(&cmpp->sock, CMPP_SOCK_RECVTIMEOUT, config->recv_timeout);

    /* Initialization cmpp connection */
    err = cmpp_init_sp(cmpp, config->host, config->port);
    if (err) {
        lamb_errlog(config->logfile, "Can't connect to gateway %s server", config->host);
        return 1;
    }

    /* login to cmpp gateway */
    sequenceId = cmpp_sequence();
    err = cmpp_connect(&cmpp->sock, sequenceId, config->user, config->password);
    if (err) {
        lamb_errlog(config->logfile, "Sending connection request to %s failed", config->host);
        return 2;
    }

    err = cmpp_recv_timeout(&cmpp->sock, &pack, sizeof(pack), config->recv_timeout);
    if (err) {
        lamb_errlog(config->logfile, "Receive gateway response packet from %s failed", config->host);
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
            lamb_errlog(config->logfile, "Incorrect protocol packets", 0);
            break;
        case 2:
            lamb_errlog(config->logfile, "Illegal source address", 0);
            break;
        case 3:
            lamb_errlog(config->logfile, "Authenticator failed", 0);
            break;
        case 4:
            lamb_errlog(config->logfile, "Protocol version is too high", 0);
            break;
        default:
            lamb_errlog(config->logfile, "Cmpp unknown error code: %d", status);
            break;
        }

        return 4;
    } else {
        lamb_errlog(config->logfile, "Incorrect response packet from %s gateway", config->host);
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
    lamb_message_t *message;

    message = (lamb_message_t *)data;

    switch (message->type) {
    case LAMB_REPORT:
        report = (lamb_report_t *)&message->data;
        fprintf(fp, "%d,%llu,%s,%d,%s,%s\n", LAMB_REPORT, report->id, report->phone, report->status,
                report->submitTime, report->doneTime);
        break;
    case LAMB_DELIVER:
        deliver = (lamb_deliver_t *)&message->data;
        fprintf(fp, "%d,%llu,%s,%s,%s,%d,%d,%s\n", LAMB_DELIVER, deliver->id, deliver->phone,
                deliver->spcode, deliver->serviceId, deliver->msgFmt, deliver->length, deliver->content);
        break;
    }

    fclose(fp);

    return 0;
}

void *lamb_online_update(void *data) {
    int err;
    redisReply *reply = NULL;

    err = lamb_cpu_affinity(pthread_self());
    if (err) {
        lamb_errlog(config.logfile, "Can't set thread cpu affinity", 0);
    }

    while (true) {
        pthread_mutex_lock(&(rdb.lock));
        reply = redisCommand(rdb.handle, "HMSET gateway.%d pid %u status %d", config.id, getpid(), cmpp.ok ? 1 : 0);
        pthread_mutex_unlock(&(rdb.lock));
        if (reply != NULL) {
            freeReplyObject(reply);
        }
        sleep(3);
    }

    pthread_exit(NULL);
}

void *lamb_recovery_loop(void *data) {
    int err;
    char *key = NULL;
    lamb_list_node_t *node;

    while (true) {
        pthread_mutex_lock(&recovery->lock);
        node = lamb_list_lpop(recovery);
        pthread_mutex_unlock(&recovery->lock);

        if (!node) {
            lamb_sleep(50);
            continue;
        }

        key = (char *)node->val;
        err = lamb_level_delete(&cache, key, strlen(key));
        if (err) {
            lamb_errlog(config.logfile, "Can't delete message resources from leveldb", 0);
            lamb_sleep(1000);
        }

        free(key);
        free(node);
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

    if (lamb_get_int64(&cfg, "Timeout", &conf->timeout) != 0) {
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

    if (lamb_get_int64(&cfg, "SendTimeout", &conf->send_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SendTimeout' parameter\n");
        goto error;
    }

    if (lamb_get_int64(&cfg, "RecvTimeout", &conf->recv_timeout) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RecvTimeout' parameter\n");
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

    if (lamb_get_string(&cfg, "Cache", conf->cache, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'Cache' parameter\n");
        goto error;
    }

    lamb_config_destroy(&cfg);
    return 0;
error:
    lamb_config_destroy(&cfg);
    return -1;
}
