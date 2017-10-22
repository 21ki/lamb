
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_TEMPLATE_H
#define _LAMB_TEMPLATE_H

#include "db.h"
#include "queue.h"

typedef struct {
    int id;
    int account;
    char name[64];
    char contents[512];
} lamb_template_t;

int lamb_template_get(lamb_db_t *db, int id, lamb_template_t *template);
int lamb_template_get_all(lamb_db_t *db, int id, lamb_queue_t *templates);
bool lamb_template_check(lamb_template_t *template, char *content, int len);
    
#endif
