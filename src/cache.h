
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-20
 */

#ifndef _LAMB_CACHE_H
#define _LAMB_CACHE_H

#include <leveldb/c.h>

typedef struct {
    char *name;
    leveldb_t *handle;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} lamb_db_t;

#endif
