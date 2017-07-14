
/* 
 * China Mobile CMPP 2.0 protocol library
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#ifndef _CMPP_DB_H
#define _CMPP_DB_H

typedef struct {
    char *name;
    leveldb_t *handle;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} CMPP_DB_T;

int cmpp_db_init(CMPP_DB_T *db, const char *name);
int cmpp_db_open(CMPP_DB_T *db, const char *name);
int cmpp_db_put(CMPP_DB_T *db, const char* key, size_t keylen, const char* val, size_t vallen);
int cmpp_db_get(CMPP_DB_T *db, const char *key, size_t keylen);
int cmpp_db_delete(CMPP_DB_T *db, const char *key, size_t keylen);
int cmpp_db_close(CMPP_DB_T *db);

#endif
