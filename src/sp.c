
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
#include "utils.h"
#include "config.h"
#include "queue.h"
#include "cache.h"

static cmpp_sp_t cmpp;
static lamb_config_t config;
static pthread_cond_t cond;
static pthread_mutex_t mutex;
static lamb_seqtable_t table;
static lamb_queue_t send_queue;
static lamb_queue_t recv_queue;
static lamb_heartbeat_t heartbeat;

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
    lamb_signal_processing();

    /* Start main event processing */
    lamb_set_process("lamb-gateway");

    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    /* Message queue initialization */
    char name[64];
    lamb_queue_opt sopt, ropt;
    struct mq_attr sattr, rattr;

    sopt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    sattr.mq_maxmsg = config.send_queue;
    sattr.mq_msgsize = sizeof(lamb_message_t);
    sopt.attr = &sattr;
    
    sprintf(name, "/gw.%d.message", config.id);
    err = lamb_queue_open(&send_queue, name, &sopt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s queue", name);
        return;
    }

    ropt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    rattr.mq_maxmsg = config.recv_queue;
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
        lamb_errlog(config.logfile, "Lamb sp client initialization failed", 0);
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

    while (true) {
        lamb_sleep(3000);
    }
}

void *lamb_sender_loop(void *data) {
    int err;
    int retry;
    int msgFmt;
    ssize_t ret;
    char spcode[21];
    lamb_submit_t *submit;
    lamb_message_t message;
    unsigned int sequenceId;

    retry = 0;
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
            table.msgId = submit->id;
            sprintf(spcode, "%s%s", config.spcode, submit->spcode);
            now_time = lamb_now_microsecond();

            /* Message Encode Convert */
            int length;
            char content[256];
            memset(content, 0, sizeof(content));
            err = lamb_encoded_convert(submit->content, submit->length, content, sizeof(content), "UTF-8", config.encoding, &length);
            if (err || (length == 0)) {
                printf("-> [encoded] Message encoding conversion failed\n");
                continue;
            }
            printf("-> [encoded] Message encoding conversion successfull, length: %d\n", length);
        submit:
            if (retry >= config.retry) {
                retry = 0;
                count = 0;
                start_time = lamb_now_microsecond();
                continue;
            }

            sequenceId = table.sequenceId = cmpp_sequence();
            err = cmpp_submit(&cmpp.sock, sequenceId, config.spid, spcode, submit->phone, content, length, msgFmt, true);
            if (err) {
                lamb_sleep(config.interval * 1000);
                if (!cmpp.ok) {
                    continue;
                }
                goto submit;
            }

            printf("-> [submit] msgId: %llu, phone: %s, msgFmt: %d, length: %d\n", submit->id, submit->phone, msgFmt, length);

            count++;
            
            struct timeval now;
            struct timespec timeout;

            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = now.tv_usec * 1000;
            timeout.tv_sec += config.timeout;
            
            pthread_mutex_lock(&mutex);
            err = pthread_cond_timedwait(&cond, &mutex, &timeout);

            if (err == ETIMEDOUT) {
                retry++;
                pthread_mutex_unlock(&mutex);
                lamb_errlog(config.logfile, "Lamb submit message to %s timeout", config.host);
                lamb_sleep(config.interval * 1000);
                goto submit;
            }
            pthread_mutex_unlock(&mutex);

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
    cmpp_pack_t pack;
    cmpp_head_t *chp;
    lamb_report_t *report;
    lamb_update_t *update;
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

        chp = (cmpp_head_t *)&pack;
        commandId = ntohl(chp->commandId);
        sequenceId = ntohl(chp->sequenceId);
        memset(&message, 0, sizeof(message));
        
        switch (commandId) {
        case CMPP_SUBMIT_RESP:;
            /* Cmpp Submit Resp */
            int result = 0;
            message.type = LAMB_UPDATE;
            update = (lamb_update_t *)&(message.data);
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_msg_id, &msgId, 8);
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_result, &result, 1);

            if ((table.sequenceId != sequenceId) || (result != 0)) {
                break;
            }

            pthread_cond_signal(&cond);

            update->id = table.msgId;
            update->msgId = msgId;

            printf("-> [update] sequenceId: %u, msgId: %llu\n", sequenceId, update->msgId);

            err = lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0);
            if (err) {
                lamb_save_logfile(config.backfile, &message);
            }

            break;
        case CMPP_DELIVER:;
            /* Cmpp Deliver */
            int registered_delivery;
            cmpp_pack_get_integer(&pack, cmpp_deliver_registered_delivery, &registered_delivery, 1);
            if (registered_delivery == 1) {
                message.type = LAMB_REPORT;
                report = (lamb_report_t *)&(message.data);

                /* Msg_Id */
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content_msg_id, &report->id, 8);

                /* Src_Terminal_Id */
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, report->phone, 24, 20);

                /* Stat */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_stat, report->status, 8, 7);

                /* Submit_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_submit_time, report->submitTime, 16, 10);

                /* Done_Time */
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_done_time, report->doneTime, 16, 10);
                
                printf("-> [report] msgId: %llu, phone: %s, status: %s, submitTime: %s, doneTime: %s\n",
                       report->id, report->phone, report->status, report->submitTime, report->doneTime);
                
                err = lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0);
                if (err) {
                    lamb_save_logfile(config.backfile, &message);
                }
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
            }
            break;
        case CMPP_ACTIVE_TEST_RESP:
            if (heartbeat.sequenceId == sequenceId) {
                heartbeat.count = 0;
                printf("-> [active] sequenceId: %u keepalive response\n", sequenceId);
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

    err = cmpp_recv(&cmpp->sock, &pack, sizeof(pack));
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

    lamb_update_t *update;
    lamb_report_t *report;
    lamb_deliver_t *deliver;
    lamb_message_t *message;

    message = (lamb_message_t *)data;

    switch (message->type) {
    case LAMB_UPDATE:
        update = (lamb_update_t *)&message->data;
        fprintf(fp, "%d,%llu,%llu\n", LAMB_UPDATE, update->id, update->msgId);
        break;
    case LAMB_REPORT:
        report = (lamb_report_t *)&message->data;
        fprintf(fp, "%d,%llu,%s,%s,%s,%s\n", LAMB_REPORT, report->id, report->phone, report->status,
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

    if (lamb_get_int(&cfg, "SendQueue", &conf->send_queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'SendQueue' parameter\n");
        goto error;
    }

    if (lamb_get_int(&cfg, "RecvQueue", &conf->recv_queue) != 0) {
        fprintf(stderr, "ERROR: Can't read 'RecvQueue' parameter\n");
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

    if (lamb_get_string(&cfg, "BackFile", conf->backfile, 128) != 0) {
        fprintf(stderr, "ERROR: Can't read 'BackFile' parameter\n");
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
