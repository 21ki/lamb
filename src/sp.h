
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

#define LAMB_UPDATE 1
#define LAMB_DELIVER 2

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
    unsigned long long id;
    char phone[16];
    char content[256];
} lamb_message_t;

typedef struct {
    int type;
    unsigned long long msgId;
    int status;
} lamb_report_t;

typedef struct {
    int type;
} lamb_deliver_t;

int lamb_read_config(lamb_config_t *conf, const char *file);
void lamb_fetch_loop(void);
void *lamb_fetch_work(void *queue);
void lamb_send_loop(void);
void lamb_recv_loop(void);
void *lamb_update_loop(void *data);
int lamb_cmpp_init(void);
void lamb_event_loop(void);

#endif
