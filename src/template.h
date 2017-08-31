
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_TEMPLATE_H
#define _LAMB_TEMPLATE_H

#include "db.h"

#define LAMB_MAX_TEMPLATE 1024

typedef struct {
    int id;
    char name[64];
    char contents[512];
    int account;
} lamb_template_t;

typedef struct {
    int id;
    int len;
    lamb_template_t *list[LAMB_MAX_TEMPLATE];
} lamb_templates_t;

int lamb_template_get(lamb_db_t *db, int id, lamb_template_t *template);
lamb_template_get_all(lamb_db_t *db, int id, lamb_templates_t *templates, int size);
bool lamb_template_check(lamb_templates_t *templates, char *content, int len);
    
#endif
