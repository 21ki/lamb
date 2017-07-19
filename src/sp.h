
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

typedef struct {
    bool debug;
    bool daemon;
    char host[16];
    int port;
    char user[64];
    char password[128];
    char spid[8];
    char spcode[16];
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

int lamb_read_config(lamb_config_t *conf, const char *file);
void lamb_fetch_loop(void);
void *lamb_fetch_work(void *data);
void lamb_send_loop(void);
void lamb_recv_loop(void);
void lamb_update_loop(void);
int lamb_cmpp_init(void);
void lamb_event_loop(void);

#endif
