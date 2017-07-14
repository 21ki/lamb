
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#include <libconfig.h>

int lamb_read_config(lamb_config_t *config, const char *file) {
    if (!config) {
        return -1;
    }

    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, file)) {
        fprintf(stderr, "ERROR: Unable to open the %s configuration file\n", file);
        config_destroy(&cfg);
        return -1;
    }

    
}