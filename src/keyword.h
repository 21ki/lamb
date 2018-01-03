
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_KEYWORD_H
#define _LAMB_KEYWORD_H

#include "db.h"
#include "list.h"

typedef struct {
    int id;
    char *val;
} lamb_keyword_t;

int lamb_keyword_get_all(lamb_db_t *db, lamb_list_t *keys);

#endif
