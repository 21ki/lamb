
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_KEYWORD_H
#define _LAMB_KEYWORD_H

#include "db.h"

#define LAMB_MAX_KEYWORD 1024

typedef struct {
    int len;
    char *list[LAMB_MAX_KEYWORD];
} lamb_keyword_t;

int lamb_keyword_get_all(lamb_db_t *db, lamb_keyword_t *keys, int size);
bool lamb_keyword_check(lamb_keyword_t *keys, char *content);

#endif
