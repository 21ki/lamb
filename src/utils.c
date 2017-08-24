
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <cmpp.h>
#include "utils.h"

void lamb_signal(void) {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    return;
}

void lamb_daemon(void) {
    int fd, maxfd;
    switch (fork()) {
    case -1:
        return;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        return;
    }

    switch (fork()) {
    case -1:
        return;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");

    maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd == -1) {
        maxfd = 8192;
        for (fd = 0; fd < maxfd; fd++) {
            close(fd);
        }
    }

    close(STDIN_FILENO);
    fd = open("/dev/null", O_RDWR);
    if (fd != STDIN_FILENO) {
        return;
    }

    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
        return;
    }

    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) {
        return;
    }

    return;
}

void lamb_sleep(unsigned long long milliseconds) {
    struct timeval timeout;

    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds * 1000) % 1000000;
    select(0, NULL, NULL, NULL, &timeout);

    return;
}

void lamb_log_error(const char *logfile, char *file, int line, const char *fmt, ...) {
    char buff[512];
    time_t rawtime;
    struct tm *t;

    time(&rawtime);
    t = localtime(&rawtime);
    sprintf(buff, "[%4d-%02d-%02d %02d:%02d:%02d] %s:%d %s\n", t->tm_year + 1900,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, file, line, fmt);

    FILE *fp = NULL;
    fp = fopen(logfile, "a");
    if (fp != NULL) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(fp, buff, ap);
        va_end(ap);
        fclose(fp);
    }

    return;
}

unsigned short lamb_sequence(void) {
    static unsigned short seq = 1;
    return (seq < 0xffff) ? (seq++) : (seq = 1);
}

char *lamb_strdup(const char *str) {
    size_t len = strlen (str) + 1;
    void *new = malloc (len);

    if (new == NULL) {
        return NULL;
    }

    return (char *) memcpy (new, str, len);
}
