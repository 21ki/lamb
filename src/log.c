
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 */

#include <stdio.h>
#include <syslog.h>
#include "log.h"

void lamb_log_init(const char *ident) {
    openlog(ident, LOG_CONS | LOG_PID, LOG_USER);
    return;
}

