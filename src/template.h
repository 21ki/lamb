
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#ifndef _LAMB_TEMPLATE_H
#define _LAMB_TEMPLATE_H

#include "db.h"
#include "list.h"

typedef struct {
    int id;
    int acc;
    char name[64];
    char content[512];
} lamb_template_t;

int lamb_get_templates(lamb_db_t *db, lamb_list_t *templates);
int lamb_get_template(lamb_db_t *db, int acc, lamb_list_t *templates);

#endif
