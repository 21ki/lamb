
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
int lamb_str2list(char *string, char *list[], int len);
unsigned long long lamb_to_long(unsigned long long mon, unsigned long long day,
                                unsigned long long hour, unsigned long long min,
                                unsigned long long sec, unsigned long long gid,
                                unsigned long long seq);
#endif
