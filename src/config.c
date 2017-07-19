
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#include <string.h>
#include <stdbool.h>
#include <libconfig.h>

int lamb_read_file(config_t *cfg, const char *file) {
    if (!cfg || !file) {
        return -1;
    }

    config_init(cfg);

    if (!config_read_file(cfg, file)) {
        config_destroy(cfg);
        return -1;
    }

    return 0;
}

int lamb_get_string(const config_t *cfg, const char *key, char *val, size_t len) {
    const char *buff;

    if (!config_lookup_string(cfg, key, &buff)) {
        return -1;
    }

    strncpy(val, buff, len);

    return 0;
}

int lamb_get_int(const config_t *cfg, const char *key, int *val) {
    if (!config_lookup_int(cfg, key, val)) {
        return -1;
    }

    return 0;
}

int lamb_get_int64(const config_t *cfg, const char *key, long long *val) {
    if (!config_lookup_int64(cfg, key, val)) {
        return -1;
    }

    return 0;
}

int lamb_get_bool(const config_t *cfg, const char *key, bool *val) {
    int buff;
    if (!config_lookup_bool(cfg, key, &buff)) {
        return -1;
    }

    *val = buff ? true : false;

    return 0;
}

int lamb_config_destroy(config_t *cfg) {
    if (cfg) {
        config_destroy(cfg);
        return 0;
    }

    return -1;
}
