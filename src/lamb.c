
/* 
 * Lamb Gateway Platform
 * Copyright (C) 2017 typefo <typefo@qq.com>
 * Update: 2017-07-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <cmpp.h>
#include "lamb.h"

#define LAMB_MAX_COMMAND 1024

char *command[LAMB_MAX_COMMAND] = {
    "show",
    "show client",
    "show account",
    "show gateway",
    "kill",
    "kill account",
    "kill gateway",
    "start",
    "start account",
    "start gateway"
};
    
int main(int argc, char *argv[]) {
    int err;
    char *host = "127.0.0.1";
    int port = 1276;
    char *prompt = "lamb";

    /* Signal event processing */
    lamb_signal_processing();
    
    /* Start main event thread */
    int sock;
    err = lamb_sock_connect(&sock, host, port);
    if (err) {
        fprintf(stderr, "Can't connect to %s server\n", host);
        return -1;
    }

    char *line;
    char *prgname = argv[0];

    linenoiseSetCompletionCallback(completion);
    linenoiseSetHintsCallback(hints);
    linenoiseHistoryLoad(".lamb_history");
    linenoiseHistorySetMaxLen(1000);
    
    while((line = linenoise("lamb> ")) != NULL) {
        /* Do something with the string. */
        if (line[0] != '\0') {
            printf("echo: %s\n", line);
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(".lamb_history");
        } else if (line[0] != '\0' && !strncmp(line, "show client", 11)) {
            int id = atoi(line + 11);
            lamb_show_client(sock, id);
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(".lamb_history");
        } else if (line[0] != '\0') {
            printf("Unknown Command: '%s'\n", line);
        }
        free(line);
    }

    return 0;
}

int lamb_sock_connect(int *sock, char *host, int port) {
    int fd;
    int err;
    char url[128];
    
    fd = nn_socket(AF_SP, NN_REQ);
    if (fd < 0) {
        fprintf(stderr, "socket: %s\n", nn_strerror(nn_errno()));
        return -1;
    }

    sprintf(url, "tcp://%s:%d", host, port);
    err = nn_connect(fd, url);
    if (err < 0) {
        fprintf(stderr, "socket: %s\n", nn_strerror(nn_errno()));
        return -1;
    }

    *sock = fd;
    return 0;
}

int lamb_signal(int sig, void (*handler)(int)) {
    struct sigaction sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(sig, &sa, NULL) == -1) {
        return -1;
    }

    return 0;
}

void lamb_signal_processing(void) {
    int err;
    
    err = lamb_signal(SIGCHLD, SIG_IGN);
    if (err) {
        fprintf(stderr, "setting SIGCHLD signal processor failed\n");
    }
    
    err = lamb_signal(SIGPIPE, SIG_IGN);
    if (err) {
        fprintf(stderr, "setting SIGPIPE signal processor failed\n");
    }
    
    return;
}

void completion(const char *buf, linenoiseCompletions *lc) {
    if (!strcasecmp(buf, "show") || !strcasecmp(buf, "show ")) {
        linenoiseAddCompletion(lc, "show client");
        linenoiseAddCompletion(lc, "show account");
        linenoiseAddCompletion(lc, "show company");
        linenoiseAddCompletion(lc, "show gateway");
    }
    
    if (!strcasecmp(buf, "kill") || !strcasecmp(buf, "kill ")) {
        linenoiseAddCompletion(lc, "kill account");
        linenoiseAddCompletion(lc, "kill gateway");
    }

    if (!strcasecmp(buf, "start") || !strcasecmp(buf, "start ")) {
        linenoiseAddCompletion(lc, "start account");
        linenoiseAddCompletion(lc, "start gateway");
    }

}

char *hints(const char *buf, int *color, int *bold) {
    if (!strcasecmp(buf, "sh")) {
        *color = 32;
        *bold = 0;
        return "ow";
    }

    if (!strcasecmp(buf, "ki")) {
        *color = 32;
        *bold = 0;
        return "ll";
    }

    if (!strcasecmp(buf, "st")) {
        *color = 32;
        *bold = 0;
        return "art";
    }

    return NULL;
}

void lamb_show_client(int sock, int id) {
    /* 
       printf("id | account | company | queue | concurrent\n");
       printf("--------------------------------------------\n");
       printf("%d    a10001    2         25461   1000/s\n", id++);
       printf("%d    a10002    2         2461    2000/s\n", id++);
       printf("%d    a10003    2         5461    5000/s\n", id++);
       printf("%d    a10004    2         15461   1000/s\n", id++);
       printf("%d    a10005    2         461     500/s\n", id++);
    */

    int rc;
    char *cmd = "show client 12";
    char res[4096];

    rc = nn_send(sock, cmd, strlen(cmd), 0);
    if (rc < 0) {
        fprintf(stderr, "nn_send: %s (ignoring)\n", nn_strerror(nn_errno()));
        return;
    }

    memset(res, 0, sizeof(res));
    rc = nn_recv (sock, res, sizeof(res), NN_DONTWAIT);
    if (rc < 0) {
        fprintf(stderr, "nn_recv: %s\n", nn_strerror(nn_errno()));
        return;
    }

    printf("\%s", res);

    return;
}
