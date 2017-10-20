
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
#include "test.h"
#include "utils.h"
#include "mqueue.h"

int main(int argc, char *argv[]) {
    int err;
    int id = atoi(argv[1]);
    char *phone = argv[2];
    char *content = argv[3];
    
    /* Signal event processing */
    lamb_signal_processing();

    char name[128];
    lamb_mq_t queue;
    lamb_mq_opt opt;

    opt.flag = O_WRONLY | O_NONBLOCK;
    opt.attr = NULL;

    sprintf(name, "/gw.%d.message", id);
    err = lamb_mq_open(&queue, name, &opt);
    if (err) {
        fprintf(stderr, "Can't open gw.%d.message message queue\n", id);
        return -1;
    }

    lamb_submit_t submit;

    memset(&submit, 0, sizeof(submit));

    submit.id = 0;
    strcpy(submit.spid, "test");
    strcpy(submit.spcode, "1");
    strcpy(submit.phone, phone);
    submit.msgFmt = 11;
    submit.length = strlen(content);
    memcpy(submit.content, content, submit.length);

    err = lamb_mq_send(&queue, (char *)&submit, sizeof(submit), 0);
    if (err) {
        fprintf(stderr, "Can't send message to gateway\n");
        return -1;
    }

    lamb_mq_close(&queue);
    
    return 0;
}

