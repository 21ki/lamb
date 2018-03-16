/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_CONFIG_H
#define _LAMB_CONFIG_H

#include <stdbool.h>
#include <libconfig.h>

int lamb_read_file(config_t *cfg, const char *file);
int lamb_get_string(const config_t *cfg, const char *key, char *val, size_t len);
int lamb_get_int(const config_t *cfg, const char *key, int *val);
int lamb_get_int64(const config_t *cfg, const char *key, long long *val);
int lamb_get_bool(const config_t *cfg, const char *key, bool *val);
int lamb_config_destroy(config_t *cfg);

#endif
