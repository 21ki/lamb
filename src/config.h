/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#ifndef _LAMB_CONFIG_H
#define _LAMB_CONFIG_H

#include <stdbool.h>
#include <libconfig.h>

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
    bool debug;
    bool daemon;
} lamb_config_t;

int lamb_read_config(config_t *cfg, const char *file);
int lamb_get_string(const config_t *cfg, const char *key, char *val, size_t len);
int lamb_get_int(const config_t *cfg, const char *key, int *val);
int lamb_get_int64(const config_t *cfg, const char *key, long long *val);
int lamb_get_bool(const config_t *cfg, const char *key, bool *val);
int lamb_config_destroy(config_t *cfg);

#endif
