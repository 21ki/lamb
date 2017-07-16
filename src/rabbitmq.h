
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_SP_H
#define _LAMB_SP_H

#include <stdbool.h>

typedef struct {
    char host[16];
    unsigned short port;
    char user[64];
    char password[128];
    char spid[8];
    char spcode[16];
    int queue;
    int confirmed;
    long long contimeout;
    long long sendtimeout;
    long long recvtimeout;
    char logfile[128];
    char cache[128];
    bool debug;
    bool daemon;
} lamb_config_t;

int lamb_read_config(lamb_config_t *config, const char *file);

#endif