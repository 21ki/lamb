
/* 
 * China Mobile CMPP 2.0 Protocol Library
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#include <leveldb/c.h>
#include "db.h"

int cmpp_db_init(CMPP_DB_T *db, const char *name) {
    db->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(db->options, 1);

    if (cmpp_db_open(db, name) != 0) {
        return -1;
    }
    
    db->roptions = leveldb_readoptions_create();
    db->woptions = leveldb_writeoptions_create();

    return 0;
}

int cmpp_db_open(CMPP_DB_T *db, const char *name) {
    char *err = NULL;

    db->handle = leveldb_open(db->options, name, &err);

    if (err != NULL) {
        return -1;
    }

    leveldb_free(err);
    err = NULL;

    return 0;
}

int cmpp_db_put(CMPP_DB_T *db, const char* key, size_t keylen, const char* val, size_t vallen) {
    char *err = NULL;

    leveldb_put(db->handle, db->woptions, key, keylen, val, vallen, &err);
    if (err != NULL) {
        return -1;
    }

    leveldb_free(err);
    err = NULL;

    return 0;
}

int cmpp_db_get(CMPP_DB_T *db, const char *key, size_t keylen) {
    size_t len;
    char *val;
    char *err = NULL;

    val = leveldb_get(db->handle, db->roptions, "key", keylen, &len, &err);

    if (err != NULL) {
        return NULL;
    }

    leveldb_free(err);
    err = NULL;

    return val;
}

int cmpp_db_delete(CMPP_DB_T *db, const char *key, size_t keylen) {
    char *err = NULL;
    
    leveldb_delete(db->handle, db->woptions, key, keylen, &err);
    
    if (err != NULL) {
      return -1;
    }

    leveldb_free(err);
    err = NULL;

    return 0;
}

int cmpp_db_close(CMPP_DB_T *db) {
    char *err = NULL;
    
    leveldb_close(db->handle);
    leveldb_destroy_db(db->options, db->name, &err);

    if (err != NULL) {
        return -1;
    }

    leveldb_free(err);
    err = NULL;    

    return 0;
}
