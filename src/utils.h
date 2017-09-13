
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#ifndef _LAMB_UTILS_H
#define _LAMB_UTILS_H

#include <stdbool.h>

#define lamb_errlog(f, fmt, ...) lamb_log_error(f, __FILE__, __LINE__, fmt, __VA_ARGS__)

#define LAMB_SUBMIT  1
#define LAMB_DELIVER 2
#define LAMB_REPORT  3
#define LAMB_UPDATE  4

#define LAMB_CMCC    1
#define LAMB_CTCC    2
#define LAMB_CUCC    3

#pragma pack(1)

typedef struct {
    int type;
    char data[508];
} lamb_message_t;

typedef struct {
    unsigned long long id;
    unsigned long long msgId;
} lamb_update_t;

typedef struct {
    unsigned long long id;
    char spid[8];
    char spcode[24];
    char phone[24];
    int msgFmt;
    int length;
    char content[160];
} lamb_submit_t;

typedef struct {
    unsigned long long id;
    char phone[24];
    char status[8];
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
bool lamb_check_operator(int sp, const char *phone, size_t len);
void lamb_rlimit_processing(void);

#endif
