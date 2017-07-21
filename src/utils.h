
/* 
 * Lamb Gateway Platform
 * By typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_UTILS_H
#define _LAMB_UTILS_H

void lamb_signal(void);
void lamb_daemon(void);
void lamb_sleep(unsigned long long milliseconds);
void lamb_errlog(const char *logfile, const char *fmt, ...);
int lamb_str2list(char *string, char *list[], int len);

#endif
