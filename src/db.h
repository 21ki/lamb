
/* 
 * China Mobile CMPP 2.0 protocol library
 * By typefo <typefo@qq.com>
 * Update: 2017-07-10
 */

#ifndef _LAMB_DB_H
#define _LAMB_DB_H

#include <libpq-fe.h>

typedef struct {
    PGconn *conn;
} lamb_db_t;

#endif
