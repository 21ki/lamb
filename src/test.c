#include <stdio.h>
#include <string.h>
#include "group.h"

int main(void) {
    int err;
    lamb_db_t db;
    lamb_db_init(&db);
    err = lamb_db_connect(&db, "127.0.0.1", 5432, "postgres", "postgres", "lamb");
    if (err) {
        printf("Can't connect to postgresql database\n");
        return 0;
    }

    lamb_group_t *groups[1024];
    memset(groups, 0, sizeof(groups));

    err = lamb_group_get_all(&db, groups, 1024);
    
    for (int i = 0; (i < 1024) && (groups[i] != NULL); i++) {
        printf("id: %d, len: %d\n", groups[i]->id, groups[i]->len);
    }

    return 0;
}
