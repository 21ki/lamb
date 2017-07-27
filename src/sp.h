
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

#pragma pack(1)

#define LAMB_UPDATE  1
#define LAMB_REPORT  2
#define LAMB_DELIVER 3

typedef struct {
    bool debug;
    bool daemon;
    char host[16];
    int port;
    char user[64];
    char password[128];
    char spid[8];
    char spcode[16];
    char encoding[32];
    int send;
    int recv;
    int unconfirmed;
    long long timeout;
    long long send_timeout;
    long long recv_timeout;
    char logfile[128];
    char db_host[16];
    int db_port;
    char db_user[64];
    char db_password[128];
    char queue[128];
} lamb_config_t;

typedef struct {
    int type;
    unsigned long long id;
    unsigned long long msgId;
} lamb_update_t;

typedef struct {
    unsigned long long msgId;
    unsigned char serviceId[16];
    unsigned char msgSrc[8];
    unsigned char srcId[24];
    unsigned char destTerminalId[24];
    unsigned int msgLength;
    unsigned char msgContent[160];
} lamb_submit_t;

typedef struct {
    int type;
    unsigned long long msgId;
    unsigned char stat[8];
    unsigned char destTerminalId[24];
} lamb_report_t;

typedef struct {
    int type;
    unsigned long long msgId;
    unsigned char destId[24];
    unsigned char serviceId[16];
    unsigned int msgFmt;
    unsigned char srcTerminalId[24];
    unsigned int msgLength;
    unsigned char msgContent[160];
} lamb_deliver_t;

int lamb_read_config(lamb_config_t *conf, const char *file);
void lamb_fetch_loop(void);
void *lamb_fetch_work(void *queue);
void *lamb_send_loop(void *data);
void *lamb_recv_loop(void *data);
void *lamb_update_loop(void *data);
int lamb_cmpp_init(cmpp_sp_t *cmpp);
void lamb_event_loop(void);
void lamb_cmpp_reconnect(cmpp_sp_t *cmpp, lamb_config_t *config);

#endif
