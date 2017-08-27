
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_TEMPLATE_H
#define _LAMB_TEMPLATE_H

#include "db.h"

typedef struct {
    int id;
    char name[64];
    char contents[512];
    int account;
} lamb_template_t;

int lamb_template_get(lamb_db_t *db, int id, lamb_template_t *template);
int lamb_template_get_all(lamb_db_t *db, lamb_template_t *templates[], size_t size);
int lamb_check_template(char *pattern, char *message, int len);

#endif
