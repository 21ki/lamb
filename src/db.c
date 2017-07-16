
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include "db.h"

int lamb_db_connect(lamb_db_t *db, const char *host, const int port,
                    const char *user, const char *password, const char *dbname) {
    char info[512];
    sprintf(info, "postgresql://%s:%s@%s:%d/%s?connect_timeout=10", user, password, host, port, dbname);
    db->conn = PQconnectdb(info);
    if (PQstatus(db->conn) != CONNECTION_OK) {
        return -1;
    }

    return 0;
}

ConnStatusType lamb_db_status(lamb_db_t *db) {
    return PQstatus(db->conn);
}

int lamb_db_exec(lamb_db_t *db, const char *sql) {
    PGresult *res = NULL;
    res = PQexec(db->conn, sql);
    if (PQresultstatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return -1;
    }

    PQclear(res);
    return 0;
}

void lamb_db_reset(lamb_db_t *db) {
    PQreset(db->conn);
    return;
}