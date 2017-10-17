
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_UTILS_H
#define _LAMB_UTILS_H

#include <stdbool.h>
#include <pthread.h>

#define lamb_errlog(f, fmt, ...) lamb_log_error(f, __FILE__, __LINE__, fmt, __VA_ARGS__)

#define LAMB_SUBMIT  1
#define LAMB_DELIVER 2
#define LAMB_REPORT  3
#define LAMB_UPDATE  4

#define LAMB_NN_PULL 1
#define LAMB_NN_PUSH 2

#define LAMB_CMCC    1
#define LAMB_CTCC    2
#define LAMB_CUCC    3

#pragma pack(1)

#define LAMB_MAX_OPERATOR 4

typedef struct {
    int type;
} lamb_nn_type;

typedef struct {
    int id;
    char addr[16];
    int port;
    char token[128];
} lamb_client_t;

typedef struct {
    int type;
    char data[508];
} lamb_message_t;

typedef struct {
    unsigned long long id;
    int account;
    char spid[8];
    char spcode[24];
    char phone[24];
    int msgFmt;
    int length;
    char content[160];
} lamb_submit_t;

typedef struct {
    unsigned long long id;
    char spcode[24];
    char phone[24];
    int status;
    char submitTime[16];
    char doneTime[16];
} lamb_report_t;

typedef struct {
    unsigned long long id;
    char phone[24];
    char spcode[24];
    char serviceId[16];
    int msgFmt;
    int length;
    char content[160];
} lamb_deliver_t;

typedef struct {
    int len;
    int ops[LAMB_MAX_OPERATOR];
} lamb_operator_t;

#pragma pack()

int lamb_signal(int sig, void (*handler)(int));
void lamb_signal_processing(void);
void lamb_daemon(void);
void lamb_sleep(unsigned long long milliseconds);
void lamb_msleep(unsigned long long microsecond);
unsigned long long lamb_now_microsecond(void);
void lamb_flow_limit(unsigned long long *start, unsigned long long *now, unsigned long long *next, int *count, int limit);
void lamb_log_error(const char *logfile, char *file, int line, const char *fmt, ...);
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
bool lamb_check_msgfmt(int coded, int list[], size_t len);
bool lamb_check_operator(lamb_operator_t *sp, const char *phone, size_t len);
void lamb_rlimit_processing(void);
long lamb_get_cpu(void);
int lamb_cpu_affinity(pthread_t thread);
int lamb_hp_parse(char *str, char *host, int *port);
int lamb_wait_confirmation(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, int second);

#endif
