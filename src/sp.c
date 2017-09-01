
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
    lamb_signal();

    /* Start main event processing */
    lamb_set_process("lamb-gateway");

    lamb_event_loop();

    return 0;
}

void lamb_event_loop(void) {
    int err;

    /* Message queue initialization */
    char name[64];
    lamb_queue_opt opt;
    struct mq_attr attr;

    opt.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    attr.mq_maxmsg = config.send_queue;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;
    
    sprintf(name, "/gw.%d.message", config.id);
    err = lamb_queue_open(&send_queue, name, &opt);
    if (err) {
        lamb_errlog(config.logfile, "Can't open %s queue", name);
        return;
    }

    opt.flag = O_CREAT | O_WRONLY | O_NONBLOCK;
    attr.mq_maxmsg = config.recv_queue;
    attr.mq_msgsize = sizeof(lamb_message_t);
    opt.attr = &attr;
    
    sprintf(name, "/gw.%d.deliver", config.id);
    err = lamb_queue_open(&recv_queue, name, &opt);
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
    int msgFmt;
    ssize_t ret;
    lamb_submit_t *submit;
    lamb_message_t message;
    unsigned int sequenceId;
    char spcode[21];
        
    if (strcasecmp(config.encoding, "ASCII") == 0) {
        msgFmt = 0;
    } else if (strcasecmp(config.encoding, "UCS-2") == 0) {
        msgFmt = 8;
    } else if (strcasecmp(config.encoding, "GBK") == 0) {
        msgFmt = 15;
    } else {
        msgFmt = 0;
    }

    while (true) {
        if (!cmpp.ok) {
            lamb_sleep(1000);
            continue;
        }

        ret = lamb_queue_receive(&send_queue, (char *)&message, sizeof(message), 0);
        if (ret > 0) {
            submit = (lamb_submit_t *)&(message.data);
            sequenceId = table.sequenceId = cmpp_sequence();
            table.msgId = submit->id;
            sprintf(spcode, "%s%s", config.spcode, submit->spcode);
        submit:
            err = cmpp_submit(&cmpp.sock, sequenceId, config.spid, spcode, submit->phone, submit->content, msgFmt, true);
            if (err) {
                lamb_errlog(config.logfile, "Unable to receive packets from %s gateway", config.host);
                lamb_sleep(config.interval);
                if (!cmpp.ok) {
                    continue;
                }
                goto submit;
            }

            struct timeval now;
            struct timespec timeout;

            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + config.timeout;
            pthread_mutex_lock(&mutex);
            err = pthread_cond_timedwait(&cond, &mutex, &timeout);

            if (err == ETIMEDOUT) {
                lamb_errlog(config.logfile, "Receive submit message response timeout from %s", config.host);
                lamb_sleep(config.interval);
                pthread_mutex_unlock(&mutex);
                goto submit;
            }
            pthread_mutex_unlock(&mutex);
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
        err = cmpp_recv_timeout(&cmpp.sock, &pack, sizeof(pack), 60000);

        if (err) {
            lamb_sleep(10);
            continue;
        }

        chp = (cmpp_head_t *)&pack;
        commandId = ntohl(chp->commandId);
        sequenceId = ntohl(chp->sequenceId);

        switch (commandId) {
        case CMPP_SUBMIT_RESP:;
            /* Cmpp Submit Resp */
            message.type = LAMB_UPDATE;
            update = (lamb_update_t *)&(message.data);
            cmpp_pack_get_integer(&pack, cmpp_submit_resp_msg_id, &msgId, 8);

            if (table.sequenceId == sequenceId) {
                pthread_mutex_lock(&mutex);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
                update->id = table.msgId;
                update->msgId = msgId;

                while (!lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0)) {
                    lamb_sleep(10);
                }
            }
            break;
        case CMPP_DELIVER:;
            /* Cmpp Deliver */
            int registered_delivery;
            cmpp_pack_get_integer(&pack, cmpp_deliver_registered_delivery, &registered_delivery, 1);
            if (registered_delivery == 1) {
                message.type = LAMB_REPORT;
                report = (lamb_report_t *)&(message.data);
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_content_msg_id, &report->id, 8);
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content_stat, report->status, 8, 7);

                while (!lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0)) {
                    lamb_sleep(10);
                }
            } else {
                message.type = LAMB_DELIVER;
                deliver = (lamb_deliver_t *)&(message.data);
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_id, &deliver->id, 8);
                cmpp_pack_get_string(&pack, cmpp_deliver_src_terminal_id, deliver->phone, 24, 21);
                cmpp_pack_get_string(&pack, cmpp_deliver_dest_id, deliver->spcode, 24, 21);
                cmpp_pack_get_string(&pack, cmpp_deliver_service_id, deliver->serviceId, 16, 10);
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_fmt, &deliver->msgFmt, 1);
                cmpp_pack_get_integer(&pack, cmpp_deliver_msg_length, &deliver->length, 1);
                cmpp_pack_get_string(&pack, cmpp_deliver_msg_content, deliver->content, 160, deliver->length);

                while (!lamb_queue_send(&recv_queue, (char *)&message, sizeof(message), 0)) {
                    lamb_sleep(10);
                }
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
        lamb_sleep(config->interval);
    }

    pthread_mutex_unlock(&cmpp->lock);
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
