
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_UTILS_H
#define _LAMB_UTILS_H

#define lamb_errlog(f, fmt, ...) lamb_log_error(f, __FILE__, __LINE__, fmt, __VA_ARGS__)

void lamb_signal(void);
void lamb_daemon(void);
void lamb_sleep(unsigned long long milliseconds);
void lamb_log_error(const char *logfile, char *file, int line, const char *fmt, ...);
unsigned short lamb_sequence(void);
char *lamb_strdup(const char *str);
void lamb_start_thread((void*)(*func)(void*), void *arg, int count);

#endif
