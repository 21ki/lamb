
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_UTILS_H
#define _LAMB_UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */

#define lamb_log(...) lamb_vlog(__VA_ARGS__)

#ifdef _DEBUG

#define lamb_debug(...) fprintf(stderr, "%s:%d ", __FILE__, __LINE__);fprintf(stderr, __VA_ARGS__)
#else
#define lamb_debug(...)
#endif

#define LAMB_MT        (1 << 1)
#define LAMB_MO        (1 << 2)
#define LAMB_ISMG      (1 << 3)
#define LAMB_SERVER    (1 << 4)
#define LAMB_SCHEDULER (1 << 5)
#define LAMB_DELIVERY  (1 << 6)
#define LAMB_GATEWAY   (1 << 7)

#define LAMB_SUBMIT  1
#define LAMB_DELIVER 2
#define LAMB_REPORT  3
#define LAMB_UPDATE  4

#define CHECK_TYPE(val) *((int *)(val))

#define LAMB_CMCC    1
#define LAMB_CTCC    (1 << 1)
#define LAMB_CUCC    (1 << 2)
#define LAMB_MVNO    (1 << 3)

#define LAMB_MAX_OPERATOR 4

#pragma pack(1)

typedef struct {
    int type;
    int len;
    void *val;
} lamb_embed_t;

typedef struct {
    int len;
    int ops[LAMB_MAX_OPERATOR];
} lamb_operator_t;

typedef struct {
    int type;
    unsigned long long id;
    int account;
    int company;
    char spid[8];
    char spcode[21];
    char phone[21];
    int msgfmt;
    int length;
    char content[160];
} lamb_submit_t;

typedef struct {
    int type;
    int account;
    int company;
    unsigned long long id;
    char spcode[21];
    char phone[21];
    int status;
    char submittime[11];
    char donetime[11];
} lamb_report_t;

typedef struct {
    int type;
    int account;
    int company;
    unsigned long long id;
    char phone[21];
    char spcode[21];
    char serviceid[11];
    int msgfmt;
    int length;
    char content[160];
} lamb_deliver_t;

#pragma pack()

int lamb_signal(int sig, void (*handler)(int));
void lamb_signal_processing(void);
void lamb_daemon(void);
void lamb_sleep(unsigned long long milliseconds);
void lamb_msleep(unsigned long long microsecond);
unsigned long long lamb_now_microsecond(void);
unsigned short lamb_sequence(void);
char *lamb_strdup(const char *str);
void lamb_start_thread(void *(*func)(void *), void *arg, int count);
unsigned long long lamb_gen_msgid(int gid, unsigned short sequenceId);
void lamb_set_process(char *name);
bool lamb_pcre_regular(char *pattern, char *message, int len);
int lamb_mqd_writable(int fd, long long millisecond);
int lamb_encoded_convert(const char *src, size_t slen, char *dst, size_t dlen, const char* fromcode, const char* tocode, int *length);
size_t lamb_ucs2_strlen(const char *str);
size_t lamb_gbk_strlen(const char *str);
bool lamb_check_format(int coded, int list[], size_t len);
void lamb_rlimit_processing(void);
long lamb_get_cpu(void);
int lamb_cpu_affinity(pthread_t thread);
int lamb_hp_parse(char *str, char *host, int *port);
int lamb_wait_confirmation(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, long millisecond);
void lamb_vlog(int level, const char *fmt, ...);

#endif
